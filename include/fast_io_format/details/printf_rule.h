#pragma once

#include "../types.h"
#include "printf_parser.h"
#include "builtin_diagnostics.h"
#include "compile.h"
#include "replacement_rules.h"
#include "lower.h"

namespace fast_io::fmt
{

/**
 * Registers percent conversions as an independent compile-time grammar rule.
 *
 * A percent sign is syntax only while this rule is selected.  Brace rules and
 * third-party rules therefore see it as ordinary data unless their own CPO
 * chooses otherwise, and the common emitter never tests for it.
 */
template <basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(printf_fmt_t) noexcept
{
	constexpr auto result{
		::fast_io::fmt::details::parse_printf_format<format_literal>()};
	if constexpr (result.error !=
				  ::fast_io::fmt::details::format_parse_error::none)
	{
		::fast_io::fmt::details::diagnose_parse_error<
			result.error, result.error_position>();
	}
	return result.program;
}

/** Public proof that the percent rule owns a compile-time program for a literal. */
template <basic_fixed_string format_literal>
concept percent_format_rule = format_rule_for<format_literal, printf_fmt_t>;

/** Registers percent argument selection and value-rule lowering through ADL. */
template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	printf_fmt_t,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	return ::fast_io::fmt::details::make_rule_replacement<
		printf_fmt_t, format_literal, field>(arguments);
}

} // namespace fast_io::fmt
