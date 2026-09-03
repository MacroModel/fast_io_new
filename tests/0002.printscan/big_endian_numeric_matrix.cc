#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <fast_io_freestanding.h>

#if defined(FAST_IO_TEST_REQUIRE_BIG_ENDIAN)
static_assert(::std::endian::native == ::std::endian::big);
#endif

namespace
{

template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] constexpr bool equal_ascii(
	char_type const *first, char_type const *last,
	char8_t const (&expected)[extent]) noexcept
{
	if (last - first != static_cast<::std::ptrdiff_t>(extent - 1u))
	{
		return false;
	}
	for (::std::size_t index{}; index + 1u != extent; ++index)
	{
		if (first[index] != ::fast_io::char_literal<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, typename integer_type,
		  ::std::size_t extent>
[[nodiscard]] bool check_decimal(
	integer_type value, char8_t const (&expected)[extent]) noexcept
{
	char_type storage[96u]{};
	auto const *const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, integer_type>, storage, value)};
	if (!equal_ascii(storage, end, expected))
	{
		return false;
	}
	integer_type parsed{};
	auto scanner{::fast_io::manipulators::dec_get(parsed)};
	auto const tag{::fast_io::io_reserve_type<char_type, decltype(scanner)>};
	auto const direct{
		::fast_io::scan_contiguous_define(tag, storage, end, scanner)};
	if (direct.code != ::fast_io::parse_code::ok || direct.iter != end ||
		parsed != value)
	{
		return false;
	}
	for (::std::size_t index{static_cast<::std::size_t>(end - storage)};
		 index != 96u; ++index)
	{
		storage[index] = ::fast_io::char_literal_v<u8'/', char_type>;
	}
	parsed = {};
	auto const padded{::fast_io::scan_contiguous_padding_define(
		tag, storage, end, 32u, scanner)};
	return padded.code == ::fast_io::parse_code::ok && padded.iter == end &&
		   parsed == value;
}

template <::std::integral char_type>
[[nodiscard]] bool check_hexadecimal() noexcept
{
	using integer_type = ::std::uint64_t;
	constexpr integer_type value{UINT64_C(0xfedcba9876543210)};
	char_type storage[64u]{};
	auto printable{::fast_io::mnp::hex(value)};
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(printable))};
	auto const *const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, decltype(normalized)>, storage,
		normalized)};
	if (!equal_ascii(storage, end, u8"fedcba9876543210"))
	{
		return false;
	}
	integer_type parsed{};
	auto scanner{::fast_io::manipulators::hex_get(parsed)};
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<char_type, decltype(scanner)>, storage, end,
		scanner)};
	return result.code == ::fast_io::parse_code::ok && result.iter == end &&
		   parsed == value;
}

template <::std::integral char_type, ::std::size_t extent,
		  typename scanner_type>
[[nodiscard]] bool scan_ascii(
	char8_t const (&text)[extent], scanner_type scanner,
	::fast_io::parse_code expected_code, ::std::size_t expected_consumed) noexcept
{
	char_type storage[96u]{};
	for (::std::size_t index{}; index + 1u != extent; ++index)
	{
		storage[index] = ::fast_io::char_literal<char_type>(text[index]);
	}
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<char_type, scanner_type>, storage,
		storage + extent - 1u, scanner)};
	return result.code == expected_code &&
		   static_cast<::std::size_t>(result.iter - storage) == expected_consumed;
}

template <::std::integral char_type>
[[nodiscard]] int check_scan_edges() noexcept
{
	::std::uint64_t overflow_value{};
	auto overflow_scanner{::fast_io::manipulators::dec_get(overflow_value)};
	if (!scan_ascii<char_type>(u8"18446744073709551616", overflow_scanner,
							   ::fast_io::parse_code::overflow, 20u))
	{
		return 1;
	}

	double comma_value{};
	auto comma_scanner{
		::fast_io::manipulators::comma_decimal_get(comma_value)};
	if (!scan_ascii<char_type>(u8"1,25e2|", comma_scanner,
							   ::fast_io::parse_code::ok, 6u) ||
		comma_value != 125.0)
	{
		return 2;
	}

	double infinity{};
	auto infinity_scanner{
		::fast_io::manipulators::decimal_get(infinity)};
	if (!scan_ascii<char_type>(u8"infinity|", infinity_scanner,
							   ::fast_io::parse_code::ok, 8u) ||
		infinity != (::std::numeric_limits<double>::infinity)())
	{
		return 3;
	}
	infinity = 0.0;
	return scan_ascii<char_type>(u8"infi|", infinity_scanner,
								 ::fast_io::parse_code::invalid, 4u) &&
				   infinity == 0.0
			   ? 0
			   : 4;
}

template <::std::integral char_type>
[[nodiscard]] int check_character_type() noexcept
{
	if (!check_decimal<char_type>(
			(::std::numeric_limits<::std::uint64_t>::max)(),
			u8"18446744073709551615"))
	{
		return 1;
	}
	if (!check_decimal<char_type>(
			(::std::numeric_limits<::std::int64_t>::min)(),
			u8"-9223372036854775808"))
	{
		return 2;
	}
	if (!check_decimal<char_type>(UINT64_C(10000000000000000000),
								  u8"10000000000000000000"))
	{
		return 3;
	}
	if (!check_hexadecimal<char_type>())
	{
		return 4;
	}
	if (auto const code{check_scan_edges<char_type>()})
	{
		return 4 + code;
	}
	return 0;
}

} // namespace

int main()
{
	if (auto const code{check_character_type<char>()})
	{
		return code;
	}
	if (auto const code{check_character_type<wchar_t>()})
	{
		return 10 + code;
	}
	if (auto const code{check_character_type<char8_t>()})
	{
		return 20 + code;
	}
	if (auto const code{check_character_type<char16_t>()})
	{
		return 30 + code;
	}
	if (auto const code{check_character_type<char32_t>()})
	{
		return 40 + code;
	}
	if (auto const code{check_character_type<signed char>()})
	{
		return 50 + code;
	}
	if (auto const code{check_character_type<unsigned char>()})
	{
		return 60 + code;
	}
	return 0;
}
