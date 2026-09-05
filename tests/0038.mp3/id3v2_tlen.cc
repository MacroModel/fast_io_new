#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <fast_io_core.h>
#include <fast_io_driver/mp3.h>

namespace
{

using byte = unsigned char;

// This is the canonical cross-octet boundary: the third synchsafe digit contributes 1 << 7.
static_assert(::fast_io::mp3::decode_mp3_safe_int(0x00000100u) == 128u);

constexpr ::std::array<byte, 24u> direct_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 14,
											 'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
											 0, '1', '2', '3'}};

constexpr ::std::array<byte, 34u> extended_v3{{'I', 'D', '3', 3, 0, 0x40, 0, 0, 0, 24,
											   0, 0, 0, 6, 0, 0, 0, 0, 0, 0,
											   'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
											   0, '4', '5', '6'}};

constexpr ::std::array<byte, 30u> extended_v4{{'I', 'D', '3', 4, 0, 0x40, 0, 0, 0, 20,
											   0, 0, 0, 6, 1, 0,
											   'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
											   0, '8', '9', 0}};

// ID3v2.3's ten-byte extended payload is selected by the CRC flag; its declared three-byte padding is outside it.
constexpr ::std::array<byte, 41u> extended_crc_padding_v3{{'I', 'D', '3', 3, 0, 0x40, 0, 0, 0, 31,
														   0, 0, 0, 10, 0x80, 0, 0, 0, 0, 3,
														   0x12, 0x34, 0x56, 0x78,
														   'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
														   0, '6', '5', '4', 0, 0, 0}};

// The v2.4 extended-header data follows flag order: update, 35-bit synchsafe CRC, then restrictions.
constexpr ::std::array<byte, 39u> extended_all_flags_v4{{'I', 'D', '3', 4, 0, 0x40, 0, 0, 0, 29,
														 0, 0, 0, 15, 1, 0x70,
														 0, 5, 0x0F, 0x7F, 0x7E, 1, 0, 1, 0xAB,
														 'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
														 0, '9', '8', '7'}};

constexpr auto synchsafe_128_v4{[] {
	::std::array<byte, 148u> value{};
	value[0] = 'I';
	value[1] = 'D';
	value[2] = '3';
	value[3] = 4;
	// The 138-byte body is itself synchsafe-encoded as 00 00 01 0a.
	value[8] = 1;
	value[9] = 10;
	value[10] = 'T';
	value[11] = 'L';
	value[12] = 'E';
	value[13] = 'N';
	// ID3v2.4 frame sizes are synchsafe; 00 00 01 00 therefore denotes 128 bytes.
	value[16] = 1;
	value[21] = '7';
	return value;
}()};

constexpr auto raw_128_v3{[] {
	::std::array<byte, 148u> value{};
	value[0] = 'I';
	value[1] = 'D';
	value[2] = '3';
	value[3] = 3;
	value[8] = 1;
	value[9] = 10;
	value[10] = 'T';
	value[11] = 'L';
	value[12] = 'E';
	value[13] = 'N';
	// ID3v2.3 frame sizes are ordinary big-endian integers, so the low octet may have its high bit set.
	value[17] = 128;
	value[21] = '8';
	return value;
}()};

constexpr ::std::array<byte, 27u> utf16le_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 17,
											  'T', 'L', 'E', 'N', 0, 0, 0, 7, 0, 0,
											  1, 0xFF, 0xFE, '4', 0, '2', 0}};

constexpr ::std::array<byte, 25u> utf16be_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 15,
											  'T', 'L', 'E', 'N', 0, 0, 0, 5, 0, 0,
											  2, 0, '7', 0, '3'}};

constexpr ::std::array<byte, 34u> footer_v4{{'I', 'D', '3', 4, 0, 0x10, 0, 0, 0, 14,
											 'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
											 0, '3', '2', '1',
											 '3', 'D', 'I', 4, 0, 0x10, 0, 0, 0, 14}};

// Although both fields are individually well-formed, v2.4 expressly forbids padding in a tag that carries a footer.
constexpr ::std::array<byte, 36u> footer_with_padding_v4{{'I', 'D', '3', 4, 0, 0x10, 0, 0, 0, 16,
														  'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
														  0, '3', '2', '1', 0, 0,
														  '3', 'D', 'I', 4, 0, 0x10, 0, 0, 0, 16}};

constexpr ::std::array<byte, 25u> no_tlen_with_padding{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 15,
														'T', 'I', 'T', '2', 0, 0, 0, 2, 0, 0,
														0, 'x', 0, 0, 0}};

constexpr auto unsynchronised_header{[] {
	auto value{direct_v4};
	value[5] = 0x80;
	return value;
}()};

constexpr auto invalid_tag_synchsafe_high_bit{[] {
	auto value{direct_v4};
	value[9] = 0x80;
	return value;
}()};

constexpr auto invalid_frame_synchsafe_high_bit{[] {
	auto value{direct_v4};
	value[17] = 0x80;
	return value;
}()};

constexpr auto frame_exceeds_tag{[] {
	auto value{direct_v4};
	value[17] = 5;
	return value;
}()};

constexpr ::std::array<byte, 41u> decimal_overflow{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 31,
													'T', 'L', 'E', 'N', 0, 0, 0, 21, 0, 0,
													0, '1', '8', '4', '4', '6', '7', '4', '4', '0', '7', '3', '7', '0', '9', '5', '5', '1', '6', '1', '6'}};

constexpr ::std::array<byte, 24u> odd_utf16be{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 14,
											   'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
											   2, 0, '1', 0}};

constexpr ::std::array<byte, 14u> invalid_extended_v3{{'I', 'D', '3', 3, 0, 0x40, 0, 0, 0, 4,
													   0, 0, 0, 5}};

constexpr ::std::array<byte, 14u> invalid_extended_v4{{'I', 'D', '3', 4, 0, 0x40, 0, 0, 0, 4,
													   0, 0, 0, 0x80}};

constexpr auto v3_crc_flag_without_crc{[] {
	auto value{extended_v3};
	value[14] = 0x80;
	return value;
}()};

constexpr auto v3_crc_bytes_without_flag{[] {
	auto value{extended_crc_padding_v3};
	value[14] = 0;
	return value;
}()};

constexpr auto v3_reserved_extended_flag{[] {
	auto value{extended_v3};
	value[14] = 0x40;
	return value;
}()};

constexpr auto v3_second_extended_flag_byte{[] {
	auto value{extended_v3};
	value[15] = 1;
	return value;
}()};

constexpr auto v3_underdeclared_padding{[] {
	auto value{extended_crc_padding_v3};
	value[19] = 2;
	return value;
}()};

constexpr auto v3_nonzero_declared_padding{[] {
	auto value{extended_crc_padding_v3};
	value[40] = 1;
	return value;
}()};

constexpr auto v4_wrong_flag_byte_count{[] {
	auto value{extended_v4};
	value[14] = 2;
	return value;
}()};

constexpr auto v4_unknown_extended_flag{[] {
	auto value{extended_v4};
	value[15] = 0x08;
	return value;
}()};

constexpr auto v4_update_without_length{[] {
	auto value{extended_v4};
	value[15] = 0x40;
	return value;
}()};

constexpr auto v4_wrong_crc_length{[] {
	auto value{extended_all_flags_v4};
	value[17] = 4;
	return value;
}()};

constexpr auto v4_crc_exceeds_32_bits{[] {
	auto value{extended_all_flags_v4};
	value[18] = 0x10;
	return value;
}()};

constexpr auto v4_crc_not_synchsafe{[] {
	auto value{extended_all_flags_v4};
	value[19] = 0x80;
	return value;
}()};

// A zero octet after an empty v2.4 flag set is not padding: it lies inside the declared extended header.
constexpr ::std::array<byte, 31u> v4_unowned_extended_data{{'I', 'D', '3', 4, 0, 0x40, 0, 0, 0, 21,
															0, 0, 0, 7, 1, 0, 0,
															'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
															0, '1', '1', '1'}};

constexpr ::std::array<byte, 13u> nonzero_truncated_frame_header{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 3,
																  0, 1, 0}};

constexpr ::std::array<byte, 10u> empty_tag_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0}};

constexpr ::std::array<byte, 14u> padding_only_tag_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 4,
													   0, 0, 0, 0}};

constexpr ::std::array<byte, 38u> duplicate_tlen_v4{{'I', 'D', '3', 4, 0, 0, 0, 0, 0, 28,
													 'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
													 0, '1', 0, 0,
													 'T', 'L', 'E', 'N', 0, 0, 0, 4, 0, 0,
													 0, '2', 0, 0}};

constexpr auto footer_missing{[] {
	auto value{direct_v4};
	value[5] = 0x10;
	return value;
}()};

constexpr auto footer_mismatch{[] {
	auto value{footer_v4};
	value[26] = 'X';
	return value;
}()};

constexpr auto invalid_v3_utf8_selector{[] {
	auto value{direct_v4};
	value[3] = 3;
	value[20] = 3;
	return value;
}()};

constexpr auto zero_sized_frame{[] {
	auto value{direct_v4};
	value[17] = 0;
	return value;
}()};

struct duration_case
{
	::std::span<byte const> input;
	::std::uint_least64_t expected_duration;
	::fast_io::parse_code expected_code;
};

} // namespace

int main()
{
	// Each entry is a complete oracle: both the parse status and duration must match, including non-success paths.
	constexpr duration_case cases[]{
		{direct_v4, 123u, ::fast_io::parse_code::ok},
		{extended_v3, 456u, ::fast_io::parse_code::ok},
		{extended_v4, 89u, ::fast_io::parse_code::ok},
		{extended_crc_padding_v3, 654u, ::fast_io::parse_code::ok},
		{extended_all_flags_v4, 987u, ::fast_io::parse_code::ok},
		{synchsafe_128_v4, 7u, ::fast_io::parse_code::ok},
		{raw_128_v3, 8u, ::fast_io::parse_code::ok},
		{utf16le_v4, 42u, ::fast_io::parse_code::ok},
		{utf16be_v4, 73u, ::fast_io::parse_code::ok},
		{footer_v4, 321u, ::fast_io::parse_code::ok},
		{footer_with_padding_v4, 0u, ::fast_io::parse_code::invalid},
		{no_tlen_with_padding, 0u, ::fast_io::parse_code::ok},
		{unsynchronised_header, 0u, ::fast_io::parse_code::invalid},
		{invalid_tag_synchsafe_high_bit, 0u, ::fast_io::parse_code::invalid},
		{invalid_frame_synchsafe_high_bit, 0u, ::fast_io::parse_code::invalid},
		{::std::span<byte const>{direct_v4.data(), direct_v4.size() - 1u}, 0u,
		 ::fast_io::parse_code::end_of_file},
		{frame_exceeds_tag, 0u, ::fast_io::parse_code::invalid},
		{decimal_overflow, 0u, ::fast_io::parse_code::overflow},
		{odd_utf16be, 0u, ::fast_io::parse_code::invalid},
		{invalid_extended_v3, 0u, ::fast_io::parse_code::invalid},
		{invalid_extended_v4, 0u, ::fast_io::parse_code::invalid},
		{v3_crc_flag_without_crc, 0u, ::fast_io::parse_code::invalid},
		{v3_crc_bytes_without_flag, 0u, ::fast_io::parse_code::invalid},
		{v3_reserved_extended_flag, 0u, ::fast_io::parse_code::invalid},
		{v3_second_extended_flag_byte, 0u, ::fast_io::parse_code::invalid},
		{v3_underdeclared_padding, 0u, ::fast_io::parse_code::invalid},
		{v3_nonzero_declared_padding, 0u, ::fast_io::parse_code::invalid},
		{v4_wrong_flag_byte_count, 0u, ::fast_io::parse_code::invalid},
		{v4_unknown_extended_flag, 0u, ::fast_io::parse_code::invalid},
		{v4_update_without_length, 0u, ::fast_io::parse_code::invalid},
		{v4_wrong_crc_length, 0u, ::fast_io::parse_code::invalid},
		{v4_crc_exceeds_32_bits, 0u, ::fast_io::parse_code::invalid},
		{v4_crc_not_synchsafe, 0u, ::fast_io::parse_code::invalid},
		{v4_unowned_extended_data, 0u, ::fast_io::parse_code::invalid},
		{nonzero_truncated_frame_header, 0u, ::fast_io::parse_code::invalid},
		{empty_tag_v4, 0u, ::fast_io::parse_code::invalid},
		{padding_only_tag_v4, 0u, ::fast_io::parse_code::invalid},
		{duplicate_tlen_v4, 0u, ::fast_io::parse_code::invalid},
		{footer_missing, 0u, ::fast_io::parse_code::end_of_file},
		{footer_mismatch, 0u, ::fast_io::parse_code::invalid},
		{invalid_v3_utf8_selector, 0u, ::fast_io::parse_code::invalid},
		{zero_sized_frame, 0u, ::fast_io::parse_code::invalid}};

	::std::size_t case_number{};
	for (auto const &test : cases)
	{
		auto const *first{test.input.data()};
		auto const result{::fast_io::mp3::compute_mp3_duration(first, first + test.input.size())};
		if (result.code != test.expected_code || result.duration_in_milliseconds != test.expected_duration)
		{
			// A one-based exit code identifies the exact deterministic row without introducing test-framework dependencies.
			return static_cast<int>(case_number + 1u);
		}
		++case_number;
	}

	// Every strict prefix of a valid tag is incomplete. This exhaustively exercises each structural boundary under
	// sanitizers and prevents an extended-header or footer check from reading into absent suffix storage.
	constexpr auto validate_strict_prefixes = []<::std::size_t size>(::std::array<byte, size> const &input) {
		for (::std::size_t extent{}; extent != size; ++extent)
		{
			auto const result{::fast_io::mp3::compute_mp3_duration(input.data(), input.data() + extent)};
			if (result.code == ::fast_io::parse_code::ok)
			{
				return false;
			}
		}
		return true;
	};
	if (!validate_strict_prefixes(direct_v4) ||
		!validate_strict_prefixes(extended_crc_padding_v3) ||
		!validate_strict_prefixes(extended_all_flags_v4) ||
		!validate_strict_prefixes(footer_v4))
	{
		return static_cast<int>(case_number + 1u);
	}
}
