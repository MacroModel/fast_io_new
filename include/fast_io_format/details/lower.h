#pragma once

#include "arguments.h"
#include "compile.h"
#include "rule_protocol.h"

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Carries one structural replacement descriptor to a grammar-lowering CPO. */
template <auto field>
struct compiled_replacement_t
{
	static inline constexpr auto value{field};
};

template <auto format_literal, typename grammar_tag, ::std::size_t operation_index>
struct compiled_literal_run
{
	static inline constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	static inline constexpr auto operation{program.operations[operation_index]};
	static_assert(operation.kind == format_operation_kind::literal);
	static inline constexpr auto run{program.literal_runs[operation.payload_index]};
	using char_type = typename decltype(format_literal)::value_type;
	static inline constexpr auto storage = []() consteval {
		::std::array<char_type, run.size> result{};
		for (::std::size_t i{}; i != run.size; ++i)
		{
			result[i] = program.literal_storage[run.offset + i];
		}
		return result;
	}();
};

template <typename literal_type, ::std::size_t... index>
[[nodiscard]] inline constexpr auto make_small_literal_pack(
	::std::index_sequence<index...>) noexcept
{
	// Each code unit is transported by value.  No pointer to the inline
	// compiled-literal storage enters the ABI, so ELF/Mach-O COMDAT visibility
	// cannot force a GOT load merely because this header is instantiated in
	// another translation unit.
	return ::fast_io::manipulators::pack(
		::fast_io::manipulators::chvw(literal_type::storage[index])...);
}

template <auto format_literal, typename grammar_tag, ::std::size_t operation_index>
[[nodiscard]] inline constexpr auto make_literal_operation() noexcept
{
	using literal_type = compiled_literal_run<format_literal, grammar_tag, operation_index>;
	static_assert(literal_type::run.size != 0u,
				  "fast_io format: a literal operation must not be empty");
	if constexpr (literal_type::run.size == 1u)
	{
		// This is the same canonicalization performed by fast_io's array alias:
		// a one-code-unit literal is a character semantic node, not a scatter
		// carrying a pointer.  Besides matching the direct io::print type graph,
		// the distinction is material on GCC: static_scatter<1> otherwise leaves
		// one address load and one weak one-byte storage object per separator,
		// while chvw carries the code unit directly in its ABI value.  The core can
		// then combine spaces/newlines with adjacent reserve leaves exactly as if
		// the user had written mnp::chvw in the original print call.
		return ::fast_io::manipulators::chvw(literal_type::storage[0u]);
	}
	else if constexpr (literal_type::run.size <= 16u)
	{
		// fast_io's semantic pack is the pointer-free canonical form for a short
		// format-owned literal.  Its print/concat front doors flatten the pack
		// before strategy selection, after which adjacent characters and reserve
		// leaves share one contiguous materialization.  GCC 15 AArch64 evidence
		// shows exact direct-literal code for lengths 2..12 and better inlining for
		// 13..16, with no pack object, weak storage, or GOT access left in output.
		// The cutoff matches the core small-copy policy; expanding larger runs
		// would inflate template state and function ABI instead of improving the
		// memcpy-shaped backend path.
		return make_small_literal_pack<literal_type>(
			::std::make_index_sequence<literal_type::run.size>{});
	}
	else
	{
		return ::fast_io::manipulators::static_scatter_t<
			typename literal_type::char_type, literal_type::run.size>{
			literal_type::storage.data()};
	}
}

template <typename grammar_type, auto format_literal, replacement_field field,
		  ::std::size_t... index, typename... argument_types>
[[nodiscard]] inline constexpr decltype(auto) make_rule_replacement(
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments)
{
	constexpr auto resolution{
		resolve_argument_reference<format_literal, field.argument, argument_types...>()};
	if constexpr (resolution.error != argument_resolution_error::none)
	{
		diagnose_argument_resolution<resolution.error, field.source.offset>();
		return ::fast_io::io_null;
	}
	else
	{
		auto &holder{indexed_argument_get<resolution.index>(arguments)};
		decltype(auto) value{unwrap_static_named_argument(holder)};
		using value_reference = decltype(value);
		using argument_pack_type = ::std::remove_reference_t<decltype(arguments)>;
		if constexpr (format_replacement_rule_for<grammar_type,
												  format_literal, field, value_reference, argument_pack_type>)
		{
			return format_replacement_rule_adl::invoke<grammar_type,
													   format_literal, field>(value, arguments);
		}
		else
		{
			static_assert(format_replacement_rule_for<grammar_type,
													  format_literal, field, value_reference, argument_pack_type>,
						  "fast_io format: no concept-defined replacement rule accepts this field and value type");
			return ::fast_io::io_null;
		}
	}
}

namespace grammar_lower_adl
{

template <auto format_literal, auto field, typename grammar_type,
		  typename argument_pack>
void lower_format_replacement_define(
	grammar_type, compile_time_value<format_literal>, compiled_replacement_t<field>,
	argument_pack &) = delete;

template <auto format_literal, auto field, typename grammar_type,
		  typename argument_pack>
concept expression = requires(argument_pack &arguments) {
	lower_format_replacement_define(
		::std::remove_cvref_t<grammar_type>{},
		compile_time_value<format_literal>{},
		compiled_replacement_t<field>{}, arguments);
};

template <auto format_literal, auto field, typename grammar_type,
		  typename argument_pack>
	requires expression<format_literal, field, grammar_type, argument_pack>
[[nodiscard]] inline constexpr decltype(auto) invoke(argument_pack &arguments)
{
	return lower_format_replacement_define(
		::std::remove_cvref_t<grammar_type>{},
		compile_time_value<format_literal>{},
		compiled_replacement_t<field>{}, arguments);
}

} // namespace grammar_lower_adl

template <auto format_literal, auto field, typename grammar_type,
		  typename argument_pack>
concept compilable_format_replacement =
	compilable_format_grammar<format_literal, grammar_type> &&
	grammar_lower_adl::expression<
		format_literal, field, grammar_type, argument_pack>;

template <auto format_literal, typename grammar_tag, ::std::size_t operation_index,
		  ::std::size_t... index, typename... argument_types>
[[nodiscard]] inline constexpr decltype(auto) make_format_operation(
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments)
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind == format_operation_kind::literal)
	{
		return make_literal_operation<format_literal, grammar_tag, operation_index>();
	}
	else
	{
		constexpr auto field{program.fields[operation.payload_index]};
		using grammar_type = ::std::remove_cvref_t<grammar_tag>;
		using argument_pack_type = indexed_argument_pack<
			::std::index_sequence<index...>, argument_types...>;
		if constexpr (compilable_format_replacement<
						  format_literal, field, grammar_type, argument_pack_type>)
		{
			return grammar_lower_adl::invoke<
				format_literal, field, grammar_type>(arguments);
		}
		else
		{
			static_assert(compilable_format_replacement<
							  format_literal, field, grammar_type, argument_pack_type>,
						  "fast_io format: grammar rule has no ADL lower_format_replacement_define CPO for this field");
			return ::fast_io::io_null;
		}
	}
}

template <auto format_literal, typename grammar_tag, typename callback_type,
		  typename argument_pack, ::std::size_t... operation_index>
inline constexpr decltype(auto) lower_format_program_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<operation_index...>)
{
	// This is the only type expansion in the emitter.  There is no recursive AST walk,
	// token-kind switch, parser cursor, or type-erased argument visit in generated code.
	return ::std::forward<callback_type>(callback)(
		make_format_operation<format_literal, grammar_tag, operation_index>(arguments)...);
}

template <auto format_literal, typename grammar_tag, typename callback_type, typename... argument_types>
inline constexpr decltype(auto) lower_format_program(
	callback_type &&callback, argument_types &...arguments)
{
	auto indexed_arguments{make_indexed_argument_pack(arguments...)};
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>.operation_count};
	return lower_format_program_impl<format_literal, grammar_tag>(
		::std::forward<callback_type>(callback), indexed_arguments,
		::std::make_index_sequence<operation_count>{});
}

} // namespace fast_io::fmt::details
