#pragma once

#include "lower.h"

#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Named continuation that materializes a lowered component pack into the requested result string. */
template <typename result_type, ::std::integral char_type>
struct concat_lowered_components
{
	template <typename... component_types>
	[[nodiscard]] inline constexpr result_type operator()(component_types &&...components) const
	{
		if constexpr (sizeof...(component_types) == 0u)
		{
			return {};
		}
		else
		{
			return ::fast_io::basic_general_concat_checked<false, char_type, result_type>(
				::std::forward<component_types>(components)...);
		}
	}
};

/** Named continuation which appends one separately proved terminal line feed. */
template <typename result_type, ::std::integral char_type>
struct concat_line_lowered_components
{
	template <typename... component_types>
	[[nodiscard]] inline constexpr result_type operator()(component_types &&...components) const
	{
		return ::fast_io::basic_general_concat_checked<true, char_type, result_type>(
			::std::forward<component_types>(components)...);
	}
};

/**
 * Materializes one compiled grammar into an explicitly selected string type.
 *
 * `result_type` is a policy chosen by the public destination facade, not a
 * syntax property. Keeping it independent from `grammar_type` prevents a
 * brace/percent cross-product in the lowering implementation. The grammar
 * object is an empty rule token used only for CPO selection, while every final
 * component is forwarded to fast_io's checked concat front door. Consequently
 * the ordinary concat concepts retain sole ownership of allocation, sizing,
 * alias normalization, and ABI transport decisions.
 */
template <typename result_type, basic_fixed_string format_literal,
		  format_grammar grammar_type, typename... argument_types>
[[nodiscard]] inline constexpr result_type concat_with_rule(
	grammar_type, argument_types &&...arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	constexpr bool program_has_terminal_literal_line_feed{
		::fast_io::fmt::details::format_program_has_terminal_literal_line_feed<
			format_literal, rule_type>()};
	if constexpr (program_has_terminal_literal_line_feed)
	{
		return ::fast_io::fmt::details::lower_format_program_trim_terminal_line_feed<
			format_literal, rule_type>(
			::fast_io::fmt::details::concat_line_lowered_components<
				result_type, char_type>{},
			arguments...);
	}
	else
	{
		return ::fast_io::fmt::details::lower_format_program<
			format_literal, rule_type>(
			::fast_io::fmt::details::concat_lowered_components<
				result_type, char_type>{},
			arguments...);
	}
}

/** Applies one named character-domain facade to the common concat kernel. */
template <typename expected_char_type, typename result_type,
		  basic_fixed_string format_literal, format_grammar grammar_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type concat_builtin_with_rule(
	grammar_type grammar, argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected concat function");
	return ::fast_io::fmt::details::concat_with_rule<
		result_type, format_literal>(
		grammar, ::std::forward<argument_types>(arguments)...);
}

} // namespace fast_io::fmt::details
