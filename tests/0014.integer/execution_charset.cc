#include <fast_io_core.h>
#include <fast_io_core_impl/integers/optimize_size/impl.h>

#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <type_traits>

namespace
{

template <typename unsigned_type>
	requires(::std::is_unsigned_v<unsigned_type>)
consteval unsigned_type test_byteswap(unsigned_type value) noexcept
{
	unsigned_type result{};
	for (::std::size_t index{}; index != sizeof(unsigned_type); ++index)
	{
		result = static_cast<unsigned_type>(
			static_cast<unsigned_type>(result << CHAR_BIT) |
			static_cast<unsigned_type>(value &
									   static_cast<unsigned_type>(UCHAR_MAX)));
		value = static_cast<unsigned_type>(value >> CHAR_BIT);
	}
	return result;
}

/*
Keep the expected execution characters as character literals, not as one
ordinary or wide string.  GCC packs a one-byte -fwide-exec-charset four bytes
at a time into wchar_t string objects, while the corresponding L'x' character
literal remains one semantic execution character.  The three arrays below are
therefore an independent oracle for char_literal rather than a formatter table
derived from fast_io itself.
*/
#define FAST_IO_TEST_BASIC_EXECUTION_CHARACTERS(X) \
	X(u8'\0', '\0', L'\0')                         \
	X(u8'\a', '\a', L'\a')                         \
	X(u8'\b', '\b', L'\b')                         \
	X(u8'\t', '\t', L'\t')                         \
	X(u8'\n', '\n', L'\n')                         \
	X(u8'\v', '\v', L'\v')                         \
	X(u8'\f', '\f', L'\f')                         \
	X(u8'\r', '\r', L'\r')                         \
	X(u8' ', ' ', L' ')                            \
	X(u8'!', '!', L'!')                            \
	X(u8'"', '"', L'"')                            \
	X(u8'#', '#', L'#')                            \
	X(u8'$', '$', L'$')                            \
	X(u8'%', '%', L'%')                            \
	X(u8'&', '&', L'&')                            \
	X(u8'\'', '\'', L'\'')                         \
	X(u8'(', '(', L'(')                            \
	X(u8')', ')', L')')                            \
	X(u8'*', '*', L'*')                            \
	X(u8'+', '+', L'+')                            \
	X(u8',', ',', L',')                            \
	X(u8'-', '-', L'-')                            \
	X(u8'.', '.', L'.')                            \
	X(u8'/', '/', L'/')                            \
	X(u8'0', '0', L'0')                            \
	X(u8'1', '1', L'1')                            \
	X(u8'2', '2', L'2')                            \
	X(u8'3', '3', L'3')                            \
	X(u8'4', '4', L'4')                            \
	X(u8'5', '5', L'5')                            \
	X(u8'6', '6', L'6')                            \
	X(u8'7', '7', L'7')                            \
	X(u8'8', '8', L'8')                            \
	X(u8'9', '9', L'9')                            \
	X(u8':', ':', L':')                            \
	X(u8';', ';', L';')                            \
	X(u8'<', '<', L'<')                            \
	X(u8'=', '=', L'=')                            \
	X(u8'>', '>', L'>')                            \
	X(u8'?', '?', L'?')                            \
	X(u8'@', '@', L'@')                            \
	X(u8'A', 'A', L'A')                            \
	X(u8'B', 'B', L'B')                            \
	X(u8'C', 'C', L'C')                            \
	X(u8'D', 'D', L'D')                            \
	X(u8'E', 'E', L'E')                            \
	X(u8'F', 'F', L'F')                            \
	X(u8'G', 'G', L'G')                            \
	X(u8'H', 'H', L'H')                            \
	X(u8'I', 'I', L'I')                            \
	X(u8'J', 'J', L'J')                            \
	X(u8'K', 'K', L'K')                            \
	X(u8'L', 'L', L'L')                            \
	X(u8'M', 'M', L'M')                            \
	X(u8'N', 'N', L'N')                            \
	X(u8'O', 'O', L'O')                            \
	X(u8'P', 'P', L'P')                            \
	X(u8'Q', 'Q', L'Q')                            \
	X(u8'R', 'R', L'R')                            \
	X(u8'S', 'S', L'S')                            \
	X(u8'T', 'T', L'T')                            \
	X(u8'U', 'U', L'U')                            \
	X(u8'V', 'V', L'V')                            \
	X(u8'W', 'W', L'W')                            \
	X(u8'X', 'X', L'X')                            \
	X(u8'Y', 'Y', L'Y')                            \
	X(u8'Z', 'Z', L'Z')                            \
	X(u8'[', '[', L'[')                            \
	X(u8'\\', '\\', L'\\')                         \
	X(u8']', ']', L']')                            \
	X(u8'^', '^', L'^')                            \
	X(u8'_', '_', L'_')                            \
	X(u8'`', '`', L'`')                            \
	X(u8'a', 'a', L'a')                            \
	X(u8'b', 'b', L'b')                            \
	X(u8'c', 'c', L'c')                            \
	X(u8'd', 'd', L'd')                            \
	X(u8'e', 'e', L'e')                            \
	X(u8'f', 'f', L'f')                            \
	X(u8'g', 'g', L'g')                            \
	X(u8'h', 'h', L'h')                            \
	X(u8'i', 'i', L'i')                            \
	X(u8'j', 'j', L'j')                            \
	X(u8'k', 'k', L'k')                            \
	X(u8'l', 'l', L'l')                            \
	X(u8'm', 'm', L'm')                            \
	X(u8'n', 'n', L'n')                            \
	X(u8'o', 'o', L'o')                            \
	X(u8'p', 'p', L'p')                            \
	X(u8'q', 'q', L'q')                            \
	X(u8'r', 'r', L'r')                            \
	X(u8's', 's', L's')                            \
	X(u8't', 't', L't')                            \
	X(u8'u', 'u', L'u')                            \
	X(u8'v', 'v', L'v')                            \
	X(u8'w', 'w', L'w')                            \
	X(u8'x', 'x', L'x')                            \
	X(u8'y', 'y', L'y')                            \
	X(u8'z', 'z', L'z')                            \
	X(u8'{', '{', L'{')                            \
	X(u8'|', '|', L'|')                            \
	X(u8'}', '}', L'}')                            \
	X(u8'~', '~', L'~')

#define FAST_IO_TEST_CANONICAL_CHARACTER(u8_character, narrow_character, wide_character) u8_character,
inline constexpr char8_t canonical_characters[]{
	FAST_IO_TEST_BASIC_EXECUTION_CHARACTERS(FAST_IO_TEST_CANONICAL_CHARACTER)};
#undef FAST_IO_TEST_CANONICAL_CHARACTER

#define FAST_IO_TEST_NARROW_CHARACTER(u8_character, narrow_character, wide_character) narrow_character,
inline constexpr char narrow_characters[]{
	FAST_IO_TEST_BASIC_EXECUTION_CHARACTERS(FAST_IO_TEST_NARROW_CHARACTER)};
#undef FAST_IO_TEST_NARROW_CHARACTER

#define FAST_IO_TEST_WIDE_CHARACTER(u8_character, narrow_character, wide_character) wide_character,
inline constexpr wchar_t wide_characters[]{
	FAST_IO_TEST_BASIC_EXECUTION_CHARACTERS(FAST_IO_TEST_WIDE_CHARACTER)};
#undef FAST_IO_TEST_WIDE_CHARACTER
#undef FAST_IO_TEST_BASIC_EXECUTION_CHARACTERS

static_assert(sizeof(canonical_characters) == 95u + 8u);
static_assert(sizeof(canonical_characters) == sizeof(narrow_characters));
static_assert(sizeof(canonical_characters) / sizeof(char8_t) ==
			  sizeof(wide_characters) / sizeof(wchar_t));

template <typename char_type, ::std::size_t size>
consteval bool check_character_literals(char_type const (&expected)[size]) noexcept
{
	static_assert(size == sizeof(canonical_characters) / sizeof(char8_t));
	for (::std::size_t index{}; index != size; ++index)
	{
		if (::fast_io::char_literal<char_type>(canonical_characters[index]) !=
			expected[index])
		{
			return false;
		}
	}
	return true;
}

template <typename char_type>
inline constexpr auto unsigned_character(char_type value) noexcept
{
	return static_cast<::std::make_unsigned_t<char_type>>(value);
}

template <typename char_type, ::std::size_t size>
consteval auto expected_character_value(
	char_type const (&characters)[size], char8_t canonical) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		if (canonical_characters[index] == canonical)
		{
			return unsigned_character(characters[index]);
		}
	}
	return ::std::make_unsigned_t<char_type>{};
}

template <bool swap_wide, typename char_type, ::std::size_t size>
consteval auto normalized_expected_character_value(
	char_type const (&characters)[size], char8_t canonical) noexcept
{
	auto value{expected_character_value(characters, canonical)};
	if constexpr (swap_wide && ::std::same_as<char_type, wchar_t> &&
				  sizeof(wchar_t) != 1u)
	{
		value = test_byteswap(value);
	}
	return value;
}

template <typename char_type, ::std::size_t size>
consteval bool expected_ascii_layout(char_type const (&characters)[size]) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		if (unsigned_character(characters[index]) !=
			static_cast<::std::make_unsigned_t<char_type>>(
				canonical_characters[index]))
		{
			return false;
		}
	}
	return true;
}

template <typename char_type, ::std::size_t size>
consteval bool expected_none_native_execution_endian(
	char_type const (&characters)[size]) noexcept
{
	if constexpr (!::std::same_as<char_type, wchar_t> || sizeof(wchar_t) == 1u)
	{
		return false;
	}
	else
	{
		bool native_values_fit_in_byte{true};
		bool swapped_values_fit_in_byte{true};
		for (auto character : characters)
		{
			auto const value{unsigned_character(character)};
			native_values_fit_in_byte =
				native_values_fit_in_byte && value <= 0xffu;
			swapped_values_fit_in_byte =
				swapped_values_fit_in_byte && test_byteswap(value) <= 0xffu;
		}
		return !native_values_fit_in_byte && swapped_values_fit_in_byte;
	}
}

template <typename char_type, ::std::size_t size>
consteval bool check_arithmetic_character_literals(
	char_type const (&expected)[size]) noexcept
{
	static_assert(size == sizeof(canonical_characters) / sizeof(char8_t));
	constexpr bool may_swap{
		::std::same_as<char_type, wchar_t> && sizeof(wchar_t) != 1u};
	bool const swap{may_swap &&
					expected_none_native_execution_endian(expected)};
	for (::std::size_t index{}; index != size; ++index)
	{
		auto value{unsigned_character(expected[index])};
		if constexpr (may_swap)
		{
			if (swap)
			{
				value = test_byteswap(value);
			}
		}
		if (unsigned_character(::fast_io::arithmetic_char_literal<char_type>(
				canonical_characters[index])) != value)
		{
			return false;
		}
	}
	return true;
}

template <bool swap_wide, typename char_type, ::std::size_t size>
consteval bool expected_ebcdic_invariant_layout(
	char_type const (&characters)[size]) noexcept
{
	using unsigned_type = ::std::make_unsigned_t<char_type>;
	if (normalized_expected_character_value<swap_wide>(characters, u8' ') !=
		static_cast<unsigned_type>(0x40u))
	{
		return false;
	}
	for (char8_t index{}; index != 10u; ++index)
	{
		if (normalized_expected_character_value<swap_wide>(
				characters, static_cast<char8_t>(u8'0' + index)) !=
			static_cast<unsigned_type>(0xf0u + index))
		{
			return false;
		}
	}
	for (char8_t index{}; index != 26u; ++index)
	{
		unsigned expected{index < 9u    ? 0xc1u + index
						  : index < 18u ? 0xd1u + (index - 9u)
										: 0xe2u + (index - 18u)};
		if (normalized_expected_character_value<swap_wide>(
				characters, static_cast<char8_t>(u8'A' + index)) !=
			static_cast<unsigned_type>(expected))
		{
			return false;
		}
	}
	return true;
}

template <bool swap_wide, typename char_type, ::std::size_t size>
consteval bool expected_classic_ebcdic_layout(
	char_type const (&characters)[size]) noexcept
{
	using unsigned_type = ::std::make_unsigned_t<char_type>;
	if (!expected_ebcdic_invariant_layout<swap_wide>(characters))
	{
		return false;
	}
	for (char8_t index{}; index != 26u; ++index)
	{
		unsigned expected{index < 9u    ? 0x81u + index
						  : index < 18u ? 0x91u + (index - 9u)
										: 0xa2u + (index - 18u)};
		if (normalized_expected_character_value<swap_wide>(
				characters, static_cast<char8_t>(u8'a' + index)) !=
			static_cast<unsigned_type>(expected))
		{
			return false;
		}
	}
	return true;
}

template <typename char_type, ::std::size_t size>
consteval bool expected_ebcdic_layout(
	char_type const (&characters)[size]) noexcept
{
	if constexpr (::std::same_as<char_type, wchar_t> && sizeof(wchar_t) != 1u)
	{
		return expected_ebcdic_invariant_layout<false>(characters) ||
			   expected_ebcdic_invariant_layout<true>(characters);
	}
	else
	{
		return expected_ebcdic_invariant_layout<false>(characters);
	}
}

template <typename char_type, ::std::size_t size>
consteval bool expected_classic_ebcdic(
	char_type const (&characters)[size]) noexcept
{
	if constexpr (::std::same_as<char_type, wchar_t> && sizeof(wchar_t) != 1u)
	{
		return expected_classic_ebcdic_layout<false>(characters) ||
			   expected_classic_ebcdic_layout<true>(characters);
	}
	else
	{
		return expected_classic_ebcdic_layout<false>(characters);
	}
}

static_assert(check_character_literals(narrow_characters));
static_assert(check_character_literals(wide_characters));
static_assert(check_arithmetic_character_literals(narrow_characters));
static_assert(check_arithmetic_character_literals(wide_characters));
static_assert(::fast_io::char_literal<char const>(u8'#') == '#');
static_assert(::fast_io::char_literal<wchar_t const>(u8'#') == L'#');
static_assert(::fast_io::arithmetic_char_literal<wchar_t const>(u8'#') ==
			  ::fast_io::arithmetic_char_literal<wchar_t>(u8'#'));
static_assert(::fast_io::details::execution_charset_name_is("uTf-8", "UTF-8"));
static_assert(::fast_io::details::execution_charset_name_starts_with(
	"uCs-4le", "UCS"));
static_assert(
	::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
		"Utf-8//tRaNsLiT", "UTF-8"));
static_assert(
	!::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
		"UTF-8/IGNORE", "UTF-8"));
static_assert(
	!::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
		"UTF-80//IGNORE", "UTF-8"));
static_assert(::fast_io::details::is_ascii<char> ==
			  expected_ascii_layout(narrow_characters));
static_assert(::fast_io::details::is_ascii<wchar_t> ==
			  expected_ascii_layout(wide_characters));
static_assert(::fast_io::details::is_ebcdic<char> ==
			  expected_ebcdic_layout(narrow_characters));
static_assert(::fast_io::details::is_ebcdic<wchar_t> ==
			  expected_ebcdic_layout(wide_characters));
static_assert(::fast_io::details::is_classic_ebcdic<char> ==
			  expected_classic_ebcdic(narrow_characters));
static_assert(::fast_io::details::is_classic_ebcdic<wchar_t> ==
			  expected_classic_ebcdic(wide_characters));
static_assert(::fast_io::details::wide_is_none_execution_endian ==
			  expected_none_native_execution_endian(wide_characters));
static_assert(::fast_io::char_category::is_c_cntrl(
	::fast_io::details::execution_newline_literal<char>()));
static_assert(::fast_io::char_category::is_c_cntrl(
	::fast_io::details::execution_newline_literal<wchar_t>()));
static_assert(::fast_io::char_category::is_c_space(
	::fast_io::details::execution_newline_literal<char>()));
static_assert(::fast_io::char_category::is_c_space(
	::fast_io::details::execution_newline_literal<wchar_t>()));

template <typename char_type, ::std::size_t size>
consteval bool check_execution_codecvt(
	char_type const (&expected)[size]) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		char_type source[]{expected[index]};
		char32_t unicode[1]{};
		auto const decoded{::fast_io::details::codecvt::general_code_cvt(
			source, source + 1u, unicode)};
		if (decoded.src != source + 1u || decoded.dst != unicode + 1u ||
			unicode[0] != static_cast<char32_t>(canonical_characters[index]))
		{
			return false;
		}

		char_type encoded[1]{};
		auto const roundtrip{::fast_io::details::codecvt::general_code_cvt(
			unicode, unicode + 1u, encoded)};
		if (roundtrip.src != unicode + 1u || roundtrip.dst != encoded + 1u ||
			encoded[0] != expected[index])
		{
			return false;
		}
	}
	return true;
}

static_assert(check_execution_codecvt(narrow_characters));
static_assert(check_execution_codecvt(wide_characters));
static_assert(::fast_io::details::is_utf8_execution_charset<char> ||
			  (::fast_io::execution_charset_encoding_scheme<char>() !=
				   ::fast_io::encoding_scheme::utf &&
			   ::fast_io::execution_charset_encoding_scheme<char>() !=
				   ::fast_io::encoding_scheme::utf_ebcdic));
static_assert(::fast_io::details::is_unicode_execution_charset<wchar_t> ||
			  (::fast_io::execution_charset_encoding_scheme<wchar_t>() !=
				   ::fast_io::encoding_scheme::utf &&
			   ::fast_io::execution_charset_encoding_scheme<wchar_t>() !=
				   ::fast_io::encoding_scheme::utf_ebcdic));

template <typename char_type>
constexpr bool check_base36_character(
	::std::uint_least32_t value, char_type expected) noexcept
{
	char_type output[2]{};
	auto const formatted{
		::fast_io::to_chars(output, output + 2u, value, 36)};
	if (formatted.ec != ::std::errc{} || formatted.ptr != output + 1u ||
		output[0] != expected)
	{
		return false;
	}

	// Keep one guard element outside the [first, last) range.  GCC's VRP
	// otherwise diagnoses the inlined, bounds-checked lookahead in the generic
	// wide-character parser as an out-of-bounds access for some one-byte wide
	// execution charsets (notably ISO_6937-2).
	char_type input[2]{expected, {}};
	::std::uint_least32_t parsed{};
	auto const scanned{
		::fast_io::from_chars(input, input + 1u, parsed, 36)};
	return scanned.ec == ::std::errc{} && scanned.ptr == input + 1u &&
		   parsed == value;
}

template <typename char_type>
constexpr bool check_base36(
	char_type i_character, char_type r_character,
	char_type z_character) noexcept
{
	return check_base36_character(18u, i_character) &&
		   check_base36_character(27u, r_character) &&
		   check_base36_character(35u, z_character);
}

template <typename char_type>
constexpr bool check_optimize_size_base36(
	char_type i_character, char_type r_character,
	char_type z_character) noexcept
{
	char_type output[3]{};
	::fast_io::details::optimize_size::with_length::output_unsigned<36u>(
		output, 18u, 1u);
	::fast_io::details::optimize_size::with_length::output_unsigned<36u>(
		output + 1u, 27u, 1u);
	::fast_io::details::optimize_size::with_length::output_unsigned<36u>(
		output + 2u, 35u, 1u);
	return output[0] == i_character && output[1] == r_character &&
		   output[2] == z_character;
}

static_assert(check_base36('i', 'r', 'z'));
static_assert(check_base36(L'i', L'r', L'z'));
static_assert(check_optimize_size_base36('i', 'r', 'z'));
static_assert(check_optimize_size_base36(L'i', L'r', L'z'));

} // namespace

int main()
{
	return !(check_base36('i', 'r', 'z') &&
			 check_base36(L'i', L'r', L'z') &&
			 check_optimize_size_base36('i', 'r', 'z') &&
			 check_optimize_size_base36(L'i', L'r', L'z'));
}
