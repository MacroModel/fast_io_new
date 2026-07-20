#include <fast_io_format.h>

#include <concepts>
#include <cstddef>

namespace
{

template <typename result_type, typename char_type, ::std::size_t extent>
[[nodiscard]] consteval bool equals_literal(
	result_type const &result, char_type const (&expected)[extent]) noexcept
{
	if (result.size() != extent - 1u)
	{
		return false;
	}
	for (::std::size_t index{}; index != extent - 1u; ++index)
	{
		if (result.data()[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

template <typename char_type, ::std::size_t extent>
[[nodiscard]] consteval bool output_equals_literal(
	::fast_io::basic_obuffer_view<char_type> const &output,
	char_type const (&expected)[extent]) noexcept
{
	return equals_literal(output, expected);
}

[[nodiscard]] consteval bool std_concat_is_constant_evaluated()
{
	return equals_literal(
			   ::fast_io::fmt::concat_std<"a{1}c{0}">("d", "b"), "abcd") &&
		   equals_literal(
			   ::fast_io::fmt::concatf_std<"a%2$sc%1$s">("d", "b"),
			   "abcd") &&
		   equals_literal(
			   ::fast_io::fmt::wconcat_std<L"a{1}c{0}">(L"d", L"b"),
			   L"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::wconcatf_std<L"a%2$sc%1$s">(L"d", L"b"),
			   L"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u8concat_std<u8"a{1}c{0}">(u8"d", u8"b"),
			   u8"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u8concatf_std<u8"a%2$sc%1$s">(u8"d", u8"b"),
			   u8"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u16concat_std<u"a{1}c{0}">(u"d", u"b"),
			   u"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u16concatf_std<u"a%2$sc%1$s">(u"d", u"b"),
			   u"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u32concat_std<U"a{1}c{0}">(U"d", U"b"),
			   U"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u32concatf_std<U"a%2$sc%1$s">(U"d", U"b"),
			   U"abcd");
}

#if defined(__GNUC__) && !defined(__clang__)
[[nodiscard]] consteval bool fast_io_concat_is_constant_evaluated()
{
	return equals_literal(
			   ::fast_io::fmt::concat_fast_io<"a{1}c{0}">("d", "b"), "abcd") &&
		   equals_literal(
			   ::fast_io::fmt::concatf_fast_io<"a%2$sc%1$s">("d", "b"),
			   "abcd") &&
		   equals_literal(
			   ::fast_io::fmt::wconcat_fast_io<L"a{1}c{0}">(L"d", L"b"),
			   L"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::wconcatf_fast_io<L"a%2$sc%1$s">(L"d", L"b"),
			   L"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u8concat_fast_io<u8"a{1}c{0}">(u8"d", u8"b"),
			   u8"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u8concatf_fast_io<u8"a%2$sc%1$s">(u8"d", u8"b"),
			   u8"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u16concat_fast_io<u"a{1}c{0}">(u"d", u"b"),
			   u"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u16concatf_fast_io<u"a%2$sc%1$s">(u"d", u"b"),
			   u"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u32concat_fast_io<U"a{1}c{0}">(U"d", U"b"),
			   U"abcd") &&
		   equals_literal(
			   ::fast_io::fmt::u32concatf_fast_io<U"a%2$sc%1$s">(U"d", U"b"),
			   U"abcd");
}
#endif

template <typename char_type>
[[nodiscard]] consteval bool print_domain_is_constant_evaluated()
{
	char_type storage[32u]{};
	::fast_io::basic_obuffer_view<char_type> output{storage, storage + 32u};
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"a{1}c{0}">(output, "d", "b");
		::fast_io::fmt::printf<"e%2$sg%1$s">(output, "h", "f");
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"a{1}c{0}">(output, L"d", L"b");
		::fast_io::fmt::wprintf<L"e%2$sg%1$s">(output, L"h", L"f");
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"a{1}c{0}">(output, u8"d", u8"b");
		::fast_io::fmt::u8printf<u8"e%2$sg%1$s">(output, u8"h", u8"f");
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"a{1}c{0}">(output, u"d", u"b");
		::fast_io::fmt::u16printf<u"e%2$sg%1$s">(output, u"h", u"f");
	}
	else
	{
		::fast_io::fmt::u32print<U"a{1}c{0}">(output, U"d", U"b");
		::fast_io::fmt::u32printf<U"e%2$sg%1$s">(output, U"h", U"f");
	}
	constexpr char_type expected[]{
		static_cast<char_type>('a'), static_cast<char_type>('b'),
		static_cast<char_type>('c'), static_cast<char_type>('d'),
		static_cast<char_type>('e'), static_cast<char_type>('f'),
		static_cast<char_type>('g'), static_cast<char_type>('h'), char_type{}};
	return output_equals_literal(output, expected);
}

[[nodiscard]] consteval bool std_dynamic_width_precision_is_constant_evaluated()
{
	return equals_literal(
			   ::fast_io::fmt::concat_std<"{0:{1}.{2}f}">(3.125, 8, 2),
			   "    3.12") &&
		   equals_literal(
			   ::fast_io::fmt::concatf_std<"%3$*1$.*2$f">(8, 2, 3.125),
			   "    3.12");
}

#if defined(__GNUC__) && !defined(__clang__)
[[nodiscard]] consteval bool
fast_io_dynamic_width_precision_is_constant_evaluated()
{
	return equals_literal(
			   ::fast_io::fmt::concat_fast_io<"{0:{1}.{2}f}">(3.125, 8, 2),
			   "    3.12") &&
		   equals_literal(
			   ::fast_io::fmt::concatf_fast_io<"%3$*1$.*2$f">(8, 2, 3.125),
			   "    3.12");
}
#endif

template <typename char_type>
[[nodiscard]] consteval bool numeric_print_is_constant_evaluated()
{
	char_type storage[64u]{};
	::fast_io::basic_obuffer_view<char_type> output{storage, storage + 64u};
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"i={0}|{1:{2}.{3}f}">(output, 1, 3.125, 8, 2);
		::fast_io::fmt::printf<"|%3$*1$.*2$f">(output, 8, 2, 3.125);
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"i={0}|{1:{2}.{3}f}">(output, 1, 3.125, 8, 2);
		::fast_io::fmt::wprintf<L"|%3$*1$.*2$f">(output, 8, 2, 3.125);
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"i={0}|{1:{2}.{3}f}">(output, 1, 3.125, 8, 2);
		::fast_io::fmt::u8printf<u8"|%3$*1$.*2$f">(output, 8, 2, 3.125);
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"i={0}|{1:{2}.{3}f}">(output, 1, 3.125, 8, 2);
		::fast_io::fmt::u16printf<u"|%3$*1$.*2$f">(output, 8, 2, 3.125);
	}
	else
	{
		::fast_io::fmt::u32print<U"i={0}|{1:{2}.{3}f}">(output, 1, 3.125, 8, 2);
		::fast_io::fmt::u32printf<U"|%3$*1$.*2$f">(output, 8, 2, 3.125);
	}
	constexpr char expected_narrow[]{"i=1|    3.12|    3.12"};
	if (output.size() != sizeof(expected_narrow) - 1u)
	{
		return false;
	}
	for (::std::size_t index{}; index != sizeof(expected_narrow) - 1u; ++index)
	{
		if (output.data()[index] !=
			static_cast<char_type>(expected_narrow[index]))
		{
			return false;
		}
	}
	return true;
}

static_assert(std_concat_is_constant_evaluated());
#if defined(__GNUC__) && !defined(__clang__)
// GCC 15 starts the character-array lifetime in fast_io::string's constexpr
// allocation path. Clang 23 currently diagnoses writes into that allocation
// as writes outside the array lifetime, so it cannot evaluate this destination.
static_assert(fast_io_concat_is_constant_evaluated());
#endif
static_assert(print_domain_is_constant_evaluated<char>());
static_assert(print_domain_is_constant_evaluated<wchar_t>());
static_assert(print_domain_is_constant_evaluated<char8_t>());
static_assert(print_domain_is_constant_evaluated<char16_t>());
static_assert(print_domain_is_constant_evaluated<char32_t>());
static_assert(std_dynamic_width_precision_is_constant_evaluated());
#if defined(__GNUC__) && !defined(__clang__)
static_assert(fast_io_dynamic_width_precision_is_constant_evaluated());
#endif
static_assert(numeric_print_is_constant_evaluated<char>());
static_assert(numeric_print_is_constant_evaluated<wchar_t>());
static_assert(numeric_print_is_constant_evaluated<char8_t>());
static_assert(numeric_print_is_constant_evaluated<char16_t>());
static_assert(numeric_print_is_constant_evaluated<char32_t>());

} // namespace

int main()
{}
