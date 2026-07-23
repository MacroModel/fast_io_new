#include <fast_io_format.h>

#include <cstdlib>
#include <string>
#include <string_view>

namespace terminal_line_grammar_test
{

struct context_rule
{
	using format_grammar_tag = void;
};

struct consuming_rule
{
	using format_grammar_tag = void;
};

struct explicit_lowering_callback
{
	template <typename... component_types>
	[[nodiscard]] inline constexpr ::std::size_t operator()(
		component_types &&...) const noexcept
	{
		return sizeof...(component_types);
	}
};

template <::fast_io::fmt::basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(context_rule) noexcept
{
	using char_type = typename decltype(format_literal)::value_type;
	::fast_io::fmt::details::basic_format_program<
		char_type, format_literal.size()>
		program{};
	if (format_literal.size() == 0u ||
		format_literal[0u] != ::fast_io::char_literal_v<u8'@', char_type>)
	{
		::fast_io::fast_terminate();
	}
	::fast_io::fmt::details::replacement_field<char_type> field{};
	field.argument.kind =
		::fast_io::fmt::details::argument_reference_kind::index;
	field.argument.index = 0u;
	field.source = {0u, 1u};
	if (!program.append_replacement(field))
	{
		::fast_io::fast_terminate();
	}
	for (::std::size_t index{1u}; index != format_literal.size(); ++index)
	{
		if (!program.append_literal(format_literal[index]))
		{
			::fast_io::fast_terminate();
		}
	}
	return program;
}

template <::fast_io::fmt::basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(consuming_rule) noexcept
{
	using char_type = typename decltype(format_literal)::value_type;
	::fast_io::fmt::details::basic_format_program<
		char_type, format_literal.size()>
		program{};
	if (format_literal.size() != 2u ||
		format_literal[0u] != ::fast_io::char_literal_v<u8'!', char_type> ||
		format_literal[1u] != ::fast_io::char_literal_v<u8'\n', char_type>)
	{
		::fast_io::fast_terminate();
	}
	::fast_io::fmt::details::replacement_field<char_type> field{};
	field.argument.kind =
		::fast_io::fmt::details::argument_reference_kind::index;
	field.argument.index = 0u;
	field.source = {0u, 2u};
	if (!program.append_replacement(field))
	{
		::fast_io::fast_terminate();
	}
	return program;
}

template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	context_rule,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	static_assert(format_literal.size() == 2u);
	static_assert(format_literal[1u] ==
				  ::fast_io::char_literal_v<u8'\n', char_type>);
	return ::fast_io::fmt::details::indexed_argument_get<0u>(arguments);
}

template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	consuming_rule,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	static_assert(format_literal.size() == 2u);
	static_assert(format_literal[1u] ==
				  ::fast_io::char_literal_v<u8'\n', char_type>);
	return ::fast_io::fmt::details::indexed_argument_get<0u>(arguments);
}

inline constexpr ::fast_io::fmt::basic_fixed_string context_format{"@\n"};
inline constexpr ::fast_io::fmt::basic_fixed_string consuming_format{"!\n"};
inline constexpr ::fast_io::fmt::basic_fixed_string explicit_lowering_format{
	"{}"};
using legacy_concat_callback =
	::fast_io::fmt::details::concat_lowered_components<::std::string, char>;
static_assert(::std::is_empty_v<legacy_concat_callback>);

// A trailing line feed remains a literal operation when the grammar consumes
// only the replacement marker. A grammar that consumes the whole source range
// correctly contributes only its replacement operation.
static_assert(
	::fast_io::fmt::details::checked_program<context_format, context_rule>.operation_count == 2u);
static_assert(
	::fast_io::fmt::details::checked_program<consuming_format, consuming_rule>.operation_count == 1u);

} // namespace terminal_line_grammar_test

int main()
{
	using namespace terminal_line_grammar_test;
	::std::string_view const value{"value"};
	auto const context_result{
		::fast_io::fmt::details::concat_with_rule<
			::std::string, context_format>(context_rule{}, value)};
	auto const consuming_result{
		::fast_io::fmt::details::concat_with_rule<
			::std::string, consuming_format>(consuming_rule{}, value)};
	auto const directly_lowered_context_result{
		::fast_io::fmt::details::lower_format_program<
			context_format, context_rule>(legacy_concat_callback{}, value)};
	auto const &static_value{::fast_io::mnp::static_arg<"value">};
	auto const directly_lowered_static_context_result{
		::fast_io::fmt::details::lower_format_program<
			context_format, context_rule>(legacy_concat_callback{}, static_value)};
	auto const explicit_component_count{
		::fast_io::fmt::details::lower_format_program<
			explicit_lowering_format, ::fast_io::fmt::brace_fmt_t,
			explicit_lowering_callback>(explicit_lowering_callback{}, value)};
	if (context_result != "value\n" || consuming_result != "value" ||
		directly_lowered_context_result != "value\n" ||
		directly_lowered_static_context_result != "value\n" ||
		explicit_component_count != 1u)
	{
		::std::abort();
	}
}
