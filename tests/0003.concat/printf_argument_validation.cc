#include <fast_io_format.h>

#include <string_view>
#include <type_traits>

namespace
{

using ::fast_io::fmt::details::brace_argument_list_error;
using ::fast_io::fmt::details::printf_argument_list_error;

using left_argument = decltype(::fast_io::mnp::static_arg<"left", 1>);
using right_argument = decltype(::fast_io::mnp::static_arg<"right", 2>);

static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<"{}{}", int, int>()
		.error == brace_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<"{1}{0}", int, int>()
		.error == brace_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<"{0}{0}{1}", int, int>()
		.error == brace_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<
		"{left}{right}", left_argument, right_argument>()
		.error == brace_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<
		"{:{}.{}}", double, int, int>()
		.error == brace_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_brace_argument_list<
		"{2:{0}.{1}f}", int, int, double>()
		.error == brace_argument_list_error::none);

inline constexpr auto missing_brace_argument{
	::fast_io::fmt::details::validate_brace_argument_list<"{}{}", int>()};
static_assert(missing_brace_argument.error ==
			  brace_argument_list_error::reference_resolution);

inline constexpr auto extra_brace_argument{
	::fast_io::fmt::details::validate_brace_argument_list<"{}", int, int>()};
static_assert(extra_brace_argument.error ==
			  brace_argument_list_error::unreferenced_argument);
static_assert(extra_brace_argument.argument_index == 1u);

inline constexpr auto repeated_brace_hole{
	::fast_io::fmt::details::validate_brace_argument_list<"{1}{1}", int, int>()};
static_assert(repeated_brace_hole.error ==
			  brace_argument_list_error::unreferenced_argument);
static_assert(repeated_brace_hole.argument_index == 0u);

inline constexpr auto named_brace_hole{
	::fast_io::fmt::details::validate_brace_argument_list<
		"{left}{left}", left_argument, right_argument>()};
static_assert(named_brace_hole.error ==
			  brace_argument_list_error::unreferenced_argument);
static_assert(named_brace_hole.argument_index == 1u);

inline constexpr auto literal_brace_extra{
	::fast_io::fmt::details::validate_brace_argument_list<"literal", int>()};
static_assert(literal_brace_extra.error ==
			  brace_argument_list_error::unreferenced_argument);
static_assert(literal_brace_extra.argument_index == 0u);

static_assert(
	::fast_io::fmt::details::validate_printf_argument_list<"%s", 1u>()
		.error == printf_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_printf_argument_list<"%1$s%1$s", 1u>()
		.error == printf_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_printf_argument_list<"%2$s%1$s", 2u>()
		.error == printf_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_printf_argument_list<"%3$*1$.*2$f", 3u>()
		.error == printf_argument_list_error::none);
static_assert(
	::fast_io::fmt::details::validate_printf_argument_list<"%*.*f", 3u>()
		.error == printf_argument_list_error::none);

inline constexpr auto missing_argument{
	::fast_io::fmt::details::validate_printf_argument_list<"%s%s", 1u>()};
static_assert(missing_argument.error ==
			  printf_argument_list_error::index_out_of_range);
static_assert(missing_argument.argument_index == 1u);

inline constexpr auto extra_argument{
	::fast_io::fmt::details::validate_printf_argument_list<"%s", 2u>()};
static_assert(extra_argument.error ==
			  printf_argument_list_error::unreferenced_argument);
static_assert(extra_argument.argument_index == 1u);

inline constexpr auto positional_hole{
	::fast_io::fmt::details::validate_printf_argument_list<"%2$s", 2u>()};
static_assert(positional_hole.error ==
			  printf_argument_list_error::unreferenced_argument);
static_assert(positional_hole.argument_index == 0u);

inline constexpr auto literal_extra{
	::fast_io::fmt::details::validate_printf_argument_list<"literal", 1u>()};
static_assert(literal_extra.error ==
			  printf_argument_list_error::unreferenced_argument);
static_assert(literal_extra.argument_index == 0u);

template <typename char_type>
[[nodiscard]] bool print_domain_is_exact()
{
	char_type storage[2u]{};
	::fast_io::basic_obuffer_view<char_type> output{storage, storage + 2u};
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"{}">(output, 'a');
		::fast_io::fmt::printf<"%c">(output, 'b');
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"{}">(output, L'a');
		::fast_io::fmt::wprintf<L"%c">(output, L'b');
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"{}">(output, u8'a');
		::fast_io::fmt::u8printf<u8"%c">(output, u8'b');
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"{}">(output, u'a');
		::fast_io::fmt::u16printf<u"%c">(output, u'b');
	}
	else
	{
		::fast_io::fmt::u32print<U"{}">(output, U'a');
		::fast_io::fmt::u32printf<U"%c">(output, U'b');
	}
	return output.size() == 2u && storage[0u] == static_cast<char_type>('a') &&
		   storage[1u] == static_cast<char_type>('b');
}

} // namespace

int main()
{
	using namespace ::std::literals;
	return ::fast_io::fmt::concatf_std<"a%2$sc%1$s">("d", "b") ==
					   "abcd"sv &&
				   ::fast_io::fmt::concat_std<"{1}{0}">("d", "b") == "bd"sv &&
				   ::fast_io::fmt::concatf_std<"%1$s%1$s">("x") == "xx"sv &&
				   ::fast_io::fmt::concatf_std<"%3$*1$.*2$f">(8, 2, 3.125) ==
					   "    3.12"sv &&
				   ::fast_io::fmt::wconcat_std<L"{}">(L'x') == L"x" &&
				   ::fast_io::fmt::u8concatf_std<u8"%c">(u8'x') == u8"x" &&
				   ::fast_io::fmt::u16concat_fast_io<u"{}">(u'x') == u"x" &&
				   ::fast_io::fmt::u32concatf_fast_io<U"%c">(U'x') == U"x" &&
				   print_domain_is_exact<char>() &&
				   print_domain_is_exact<wchar_t>() &&
				   print_domain_is_exact<char8_t>() &&
				   print_domain_is_exact<char16_t>() &&
				   print_domain_is_exact<char32_t>()
			   ? 0
			   : 1;
}
