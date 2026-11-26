#pragma once

#include "details/async_conversion.h"

namespace fast_io::fmt::details
{
/** @brief Admits the same owned replacement fields as async print in the selected character domain. */
template <basic_fixed_string Literal, typename Grammar, typename C, typename T, typename... A>
concept annotated_to_format = ::std::same_as<C,typename ::std::remove_cvref_t<decltype(Literal)>::value_type> &&
	::fast_io::async::details::returning_conversion<C,T,A...> && async_literals<Literal,Grammar,sizeof...(A)>.valid;

/** @brief A marked target accepts ordinary grammar lowering. Marked sources require an identity-preserving field. */
template <basic_fixed_string Literal, typename Grammar, typename C, typename T, typename... A>
concept annotated_inplace_format = ::std::same_as<C,typename ::std::remove_cvref_t<decltype(Literal)>::value_type> &&
	::fast_io::async::details::inplace_conversion<C,T,A...> &&
	((!::fast_io::async::annotated_operand<A> && ...) || async_literals<Literal,Grammar,sizeof...(A)>.valid);

/** @brief Preserves source numbering for a value-returning formatted conversion. */
template <basic_fixed_string Literal, typename Grammar, typename Result, typename... A>
inline auto to_annotated(A &&...args)
{
	namespace op = ::fast_io::async::details;
	using C = typename ::std::remove_cvref_t<decltype(Literal)>::value_type;
	using graph = op::conversion_graph<decltype(op::describe_operand<false,C,A>())...>;
	return convert_format<Literal,Grammar,Result,graph,op::conversion_request<A...>>(io_type<Result>,static_cast<A &&>(args)...);
}

/** @brief Reserves argument zero for the destination even when the grammar inserts or removes literals. */
template <basic_fixed_string Literal, typename Grammar, typename T, typename... A>
inline auto inplace_to_annotated(T &&target,A &&...args)
{
	namespace op = ::fast_io::async::details;
	using C = typename ::std::remove_cvref_t<decltype(Literal)>::value_type;
	using graph = op::conversion_graph<decltype(op::describe_operand<true,C,T>()),decltype(op::describe_operand<false,C,A>())...>;
	return convert_format<Literal,Grammar,void,graph,op::conversion_request<T,A...>>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}
} // namespace fast_io::fmt::details

namespace fast_io::fmt
{
/** @brief Converts selected char fields through the compiled format grammar. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char,T,A...>
[[nodiscard]] inline auto to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char,T,A...>
[[nodiscard]] inline auto to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,printf_fmt_t,char,T,A...>
[[nodiscard]] inline auto printf_to(A &&...args)
{
	return details::to_annotated<Literal,printf_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Binds a formatted char conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,brace_fmt_t,char,T,A...>
inline auto inplace_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,brace_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Binds a formatted char conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,printf_fmt_t,char,T,A...>
inline auto inplace_printf_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,printf_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Converts selected wchar_t fields through the compiled format grammar. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,wchar_t,T,A...>
[[nodiscard]] inline auto wto(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected wchar_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,wchar_t,T,A...>
[[nodiscard]] inline auto wto(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected wchar_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,printf_fmt_t,wchar_t,T,A...>
[[nodiscard]] inline auto wprintf_to(A &&...args)
{
	return details::to_annotated<Literal,printf_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Binds a formatted wchar_t conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,brace_fmt_t,wchar_t,T,A...>
inline auto winplace_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,brace_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Converts selected char8_t fields through the compiled format grammar. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char8_t,T,A...>
[[nodiscard]] inline auto u8to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char8_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char8_t,T,A...>
[[nodiscard]] inline auto u8to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char8_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,printf_fmt_t,char8_t,T,A...>
[[nodiscard]] inline auto u8printf_to(A &&...args)
{
	return details::to_annotated<Literal,printf_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Binds a formatted char8_t conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,brace_fmt_t,char8_t,T,A...>
inline auto u8inplace_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,brace_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Converts selected char16_t fields through the compiled format grammar. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char16_t,T,A...>
[[nodiscard]] inline auto u16to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char16_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char16_t,T,A...>
[[nodiscard]] inline auto u16to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char16_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,printf_fmt_t,char16_t,T,A...>
[[nodiscard]] inline auto u16printf_to(A &&...args)
{
	return details::to_annotated<Literal,printf_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Binds a formatted char16_t conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,brace_fmt_t,char16_t,T,A...>
inline auto u16inplace_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,brace_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Converts selected char32_t fields through the compiled format grammar. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char32_t,T,A...>
[[nodiscard]] inline auto u32to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char32_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,brace_fmt_t,char32_t,T,A...>
[[nodiscard]] inline auto u32to(A &&...args)
{
	return details::to_annotated<Literal,brace_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts selected char32_t fields through the compiled format grammar. */
template <typename T, basic_fixed_string Literal, typename... A>
	requires details::annotated_to_format<Literal,printf_fmt_t,char32_t,T,A...>
[[nodiscard]] inline auto u32printf_to(A &&...args)
{
	return details::to_annotated<Literal,printf_fmt_t,T>(static_cast<A &&>(args)...);
}

/** @brief Binds a formatted char32_t conversion with an existing destination. */
template <basic_fixed_string Literal, typename T, typename... A>
	requires details::annotated_inplace_format<Literal,brace_fmt_t,char32_t,T,A...>
inline auto u32inplace_to(T &&target,A &&...args)
{
	return details::inplace_to_annotated<Literal,brace_fmt_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

} // namespace fast_io::fmt
