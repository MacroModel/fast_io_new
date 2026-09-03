#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace fast_io_state_machine_cpo
{

inline constexpr ::std::size_t scalar_corpus_size{128u};
inline constexpr ::std::size_t maximum_scalar_text_size{96u};

struct text_record
{
	::std::array<char, maximum_scalar_text_size> bytes{};
	::std::size_t size{};
};

struct scalar_record
{
	text_record decimal{};
	text_record hexadecimal{};
	text_record floating{};
	text_record word{};
	::std::int_least64_t decimal_value{};
	::std::uint_least64_t hexadecimal_value{};
	double floating_value{};
};

using scalar_corpus = ::std::array<scalar_record, scalar_corpus_size>;

[[nodiscard]] inline constexpr ::std::uint_least64_t xorshift64(
	::std::uint_least64_t value) noexcept
{
	value ^= value << 7u;
	value ^= value >> 9u;
	value ^= value << 8u;
	return value;
}

inline constexpr char *append_unsigned(
	char *destination, ::std::uint_least64_t value, unsigned base) noexcept
{
	char reversed[64]{};
	::std::size_t size{};
	do
	{
		auto const digit{static_cast<unsigned>(value % base)};
		reversed[size++] = static_cast<char>(
			digit < 10u ? static_cast<unsigned>('0') + digit
						: static_cast<unsigned>('a') + digit - 10u);
		value /= base;
	} while (value != 0u);
	while (size != 0u)
	{
		*destination++ = reversed[--size];
	}
	return destination;
}

inline constexpr void make_signed_decimal(
	text_record &record, ::std::int_least64_t value) noexcept
{
	auto output{record.bytes.data()};
	auto magnitude{static_cast<::std::uint_least64_t>(value)};
	if (value < 0)
	{
		*output++ = '-';
		/*
		Unsigned subtraction computes |INT_MIN| without evaluating a signed
		negation that is outside the signed domain.
		*/
		magnitude = UINT64_C(0) - magnitude;
	}
	output = append_unsigned(output, magnitude, 10u);
	record.size = static_cast<::std::size_t>(output - record.bytes.data());
}

inline constexpr void make_unsigned_hexadecimal(
	text_record &record, ::std::uint_least64_t value) noexcept
{
	auto const output{append_unsigned(record.bytes.data(), value, 16u)};
	record.size = static_cast<::std::size_t>(output - record.bytes.data());
}

inline constexpr void make_quarter_floating(
	text_record &record, ::std::uint_least64_t integer_part,
	unsigned quarter, bool negative) noexcept
{
	auto output{record.bytes.data()};
	if (negative)
	{
		*output++ = '-';
	}
	output = append_unsigned(output, integer_part, 10u);
	*output++ = '.';
	constexpr char fractions[4][2]{{'0', '0'}, {'2', '5'}, {'5', '0'}, {'7', '5'}};
	*output++ = fractions[quarter][0];
	*output++ = fractions[quarter][1];
	record.size = static_cast<::std::size_t>(output - record.bytes.data());
}

inline void build_scalar_corpus(
	scalar_corpus &corpus, ::std::uint_least64_t seed) noexcept
{
	static constexpr ::std::array<::std::size_t, 10u> word_sizes{
		1u, 2u, 3u, 7u, 8u, 15u, 16u, 31u, 47u, 63u};
	static constexpr char alphabet[]{
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"};
	auto random{seed ^ UINT64_C(0x9e3779b97f4a7c15)};
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto &record{corpus[index]};
		random = xorshift64(random + UINT64_C(0xd1b54a32d192ed03));
		auto decimal_value{static_cast<::std::int_least64_t>(
			random % UINT64_C(1000000000000000))};
		if ((random >> 63u) != 0u)
		{
			decimal_value = -decimal_value;
		}
		if (index == 0u)
		{
			decimal_value = 0;
		}
		else if (index == 1u)
		{
			decimal_value =
				(::std::numeric_limits<::std::int_least64_t>::max)();
		}
		else if (index == 2u)
		{
			decimal_value =
				(::std::numeric_limits<::std::int_least64_t>::min)();
		}
		record.decimal_value = decimal_value;
		make_signed_decimal(record.decimal, decimal_value);

		random = xorshift64(random + UINT64_C(0x94d049bb133111eb));
		record.hexadecimal_value = index == 0u
									   ? UINT64_C(0)
									   : (index == 1u
											  ? (::std::numeric_limits<::std::uint_least64_t>::max)()
											  : random);
		make_unsigned_hexadecimal(
			record.hexadecimal, record.hexadecimal_value);

		random = xorshift64(random + UINT64_C(0xbf58476d1ce4e5b9));
		auto const integer_part{random % UINT64_C(100000000)};
		auto const quarter{static_cast<unsigned>((random >> 17u) & 3u)};
		auto const negative{((random >> 31u) & 1u) != 0u};
		make_quarter_floating(
			record.floating, integer_part, quarter, negative);
		auto floating_value{static_cast<double>(integer_part) +
							static_cast<double>(quarter) * 0.25};
		record.floating_value = negative ? -floating_value : floating_value;

		auto const word_size{word_sizes[(index +
										 static_cast<::std::size_t>(seed)) %
										word_sizes.size()]};
		record.word.size = word_size;
		for (::std::size_t character{}; character != word_size; ++character)
		{
			random = xorshift64(random + static_cast<::std::uint_least64_t>(
											 character + 1u));
			record.word.bytes[character] = alphabet[static_cast<::std::size_t>(random) % (sizeof(alphabet) - 1u)];
		}
	}
}

[[nodiscard]] inline constexpr ::std::string_view view_of(
	text_record const &record) noexcept
{
	return {record.bytes.data(), record.size};
}

inline constexpr ::std::size_t transcode_corpus_size{16u};
inline constexpr ::std::size_t maximum_logical_size{383u};
inline constexpr ::std::size_t maximum_encoded_size{
	maximum_logical_size * 2u};

struct transcode_record
{
	::std::array<char, maximum_logical_size> logical{};
	::std::size_t logical_size{};
	::std::array<char, maximum_encoded_size> encoded{};
	::std::size_t encoded_size{};
};

using transcode_corpus =
	::std::array<transcode_record, transcode_corpus_size>;

inline void build_transcode_corpus(
	transcode_corpus &corpus, ::std::uint_least64_t seed) noexcept
{
	static constexpr ::std::array<::std::size_t, transcode_corpus_size>
		logical_sizes{0u, 1u, 2u, 3u, 7u, 8u, 15u, 16u,
					  31u, 63u, 64u, 127u, 128u, 255u, 256u, 383u};
	static constexpr char alphabet[]{
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 	"};
	auto random{seed ^ UINT64_C(0x243f6a8885a308d3)};
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto &record{corpus[record_index]};
		record.logical_size = logical_sizes[record_index];
		for (::std::size_t index{}; index != record.logical_size; ++index)
		{
			random = xorshift64(random + UINT64_C(0x13198a2e03707344));
			bool const forced_boundary_newline{
				(record_index & 3u) == 1u &&
				(index == 0u || index + 1u == record.logical_size)};
			record.logical[index] = forced_boundary_newline || random % 13u == 0u
										? '\n'
										: alphabet[static_cast<::std::size_t>(random) %
												   (sizeof(alphabet) - 1u)];
		}
		record.encoded_size = 0u;
		for (::std::size_t index{}; index != record.logical_size; ++index)
		{
			auto const character{record.logical[index]};
			if (character == '\n')
			{
				record.encoded[record.encoded_size++] = '\r';
			}
			record.encoded[record.encoded_size++] = character;
		}
	}
}

} // namespace fast_io_state_machine_cpo
