#pragma once

#include "../types.h"
#include "brace_parser.h"
#include "builtin_diagnostics.h"
#include "compile.h"
#include "replacement_rules.h"
#include "lower.h"

namespace fast_io::fmt
{

/**
 * Registers the brace syntax as one compile-time grammar rule.
 *
 * This overload, rather than the generic print/concat implementation, owns the
 * meaning of left and right braces.  The rule returns the syntax-neutral flat
 * program consumed through the grammar CPO protocol; no parser state survives
 * the immediate invocation.
 */
template <basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(brace_fmt_t) noexcept
{
	constexpr auto result{
		::fast_io::fmt::details::parse_brace_format<format_literal>()};
	if constexpr (result.error !=
				  ::fast_io::fmt::details::format_parse_error::none)
	{
		::fast_io::fmt::details::diagnose_parse_error<
			result.error, result.error_position>();
	}
	return result.program;
}

/** Public proof that the brace rule owns a compile-time program for a literal. */
template <basic_fixed_string format_literal>
concept brace_format_rule = format_rule_for<format_literal, brace_fmt_t>;

/** Registers brace argument selection and value-rule lowering through ADL. */
template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	brace_fmt_t,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	return ::fast_io::fmt::details::make_rule_replacement<
		brace_fmt_t, format_literal, field>(arguments);
}

} // namespace fast_io::fmt
