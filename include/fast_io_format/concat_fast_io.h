#pragma once

// Declare the reserve-print protocol before instantiating the destination
// container; this header must remain usable without the hosted print facade.
#include "../fast_io_freestanding.h"
#include "../fast_io_dsal/string.h"
#include "details/brace_rule.h"
#include "details/concat.h"
#include "details/printf_rule.h"

#include <cstddef>
#include <utility>

namespace fast_io::fmt
{

// Brace grammar, fast_io::basic_string destination.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::fast_io::string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto wconcat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::fast_io::wstring, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u8concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::fast_io::u8string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u16concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::fast_io::u16string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u32concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::fast_io::u32string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::fast_io::string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto wconcatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::fast_io::wstring, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u8concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::fast_io::u8string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u16concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::fast_io::u16string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u32concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::fast_io::u32string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Runtime arrays are rejected for the same reason as the std destination.
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;

} // namespace fast_io::fmt
