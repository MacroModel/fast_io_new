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

/** Owns the fully decoded output of a format program containing only literal operations. */
template <auto format_literal, typename grammar_tag>
struct compiled_literal_program
{
	static inline constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	using char_type = typename decltype(format_literal)::value_type;

	[[nodiscard]] static consteval bool contains_only_literals() noexcept
	{
		for (::std::size_t i{}; i != program.operation_count; ++i)
		{
			if (program.operations[i].kind != format_operation_kind::literal)
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] static consteval ::std::size_t decoded_size() noexcept
	{
		::std::size_t result{};
		for (::std::size_t i{}; i != program.operation_count; ++i)
		{
			auto const operation{program.operations[i]};
			if (operation.kind != format_operation_kind::literal)
			{
				return 0u;
			}
			result += program.literal_runs[operation.payload_index].size;
		}
		return result;
	}

	static inline constexpr bool literal_only{contains_only_literals()};
	static inline constexpr ::std::size_t size{decoded_size()};

	[[nodiscard]] static consteval auto make_storage() noexcept
	{
		::std::array<char_type, size> result{};
		if constexpr (literal_only)
		{
			::std::size_t output_index{};
			for (::std::size_t i{}; i != program.operation_count; ++i)
			{
				auto const operation{program.operations[i]};
				auto const run{program.literal_runs[operation.payload_index]};
				for (::std::size_t j{}; j != run.size; ++j)
				{
					result[output_index] = program.literal_storage[run.offset + j];
					++output_index;
				}
			}
		}
		return result;
	}

	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{make_storage()};
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

	[[nodiscard]] static consteval auto make_storage() noexcept
	{
		::std::array<char_type, run.size> result{};
		for (::std::size_t i{}; i != run.size; ++i)
		{
			result[i] = program.literal_storage[run.offset + i];
		}
		return result;
	}

	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{make_storage()};
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
	else if constexpr (literal_type::run.size < 64u)
	{
		return ::fast_io::manipulators::static_scatter_t<
			typename literal_type::char_type, literal_type::run.size>{
			literal_type::storage.data()};
	}
	else
	{
		// Match the core array-alias boundary: a large literal is already stable
		// borrowed storage, so an unbuffered destination can pass its address
		// straight to write instead of copying it through a temporary reserve
		// buffer.  Shorter literals retain the fixed-extent reserve node because
		// it lets buffered destinations merge their stores with adjacent leaves.
		return ::fast_io::basic_io_scatter_t<typename literal_type::char_type>{
			literal_type::storage.data(), literal_type::run.size};
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

/** Placeholder for an argument which a statically evaluated replacement cannot observe. */
struct static_evaluation_unused_argument
{};

template <typename argument_type>
struct static_evaluation_argument
{
	using type = static_evaluation_unused_argument;
};

template <auto value_literal>
struct static_evaluation_argument<
	::fast_io::fmt::static_format_arg<value_literal>>
{
	using type = ::fast_io::fmt::static_format_arg<value_literal>;
};

template <::fast_io::fmt::basic_fixed_string name_literal,
		  typename storage_type>
struct static_evaluation_argument<
	::fast_io::fmt::static_named_arg<name_literal, storage_type>>
{
	using clean_storage_type = ::std::remove_cvref_t<storage_type>;
	using selected_storage_type = ::std::conditional_t<
		::fast_io::fmt::is_static_format_arg_v<clean_storage_type>,
		clean_storage_type, static_evaluation_unused_argument>;
	using type = ::fast_io::fmt::static_named_arg<
		name_literal, selected_storage_type>;
};

template <typename argument_type>
using static_evaluation_argument_t = typename static_evaluation_argument<
	::std::remove_cvref_t<argument_type>>::type;

template <::std::size_t index, typename value_type>
struct static_evaluation_argument_slot
{
	value_type value{};
};

template <typename index_sequence, typename... value_types>
struct static_evaluation_argument_pack;

template <::std::size_t... index, typename... value_types>
struct static_evaluation_argument_pack<
	::std::index_sequence<index...>, value_types...>
	: static_evaluation_argument_slot<index, value_types>...
{};

template <::std::size_t index, typename value_type>
[[nodiscard]] inline constexpr value_type &static_evaluation_argument_get(
	static_evaluation_argument_slot<index, value_type> &slot) noexcept
{
	return slot.value;
}

template <auto format_literal, argument_reference reference,
		  typename... argument_types>
[[nodiscard]] inline consteval bool static_format_reference() noexcept
{
	constexpr auto resolution{
		resolve_argument_reference<format_literal, reference,
								   argument_types...>()};
	if constexpr (resolution.error != argument_resolution_error::none)
	{
		return false;
	}
	else
	{
		using argument_pack_type = indexed_argument_pack<
			::std::index_sequence_for<argument_types...>, argument_types...>;
		using holder_type = ::std::remove_cvref_t<decltype(
			indexed_argument_get<resolution.index>(
				::std::declval<argument_pack_type &>()))>;
		if constexpr (!::fast_io::fmt::is_static_format_argument_holder_v<
						 holder_type>)
		{
			return false;
		}
		else
		{
			using evaluation_type = static_evaluation_argument_t<holder_type>;
			using value_reference = decltype(unwrap_static_named_argument(
				::std::declval<evaluation_type &>()));
			using clean_value_type = ::std::remove_cvref_t<value_reference>;
			using char_type = typename decltype(format_literal)::value_type;
			using unreferenced_value_type =
				::std::remove_reference_t<value_reference>;
			constexpr bool same_character_array{
				::std::is_array_v<unreferenced_value_type> &&
				::std::same_as<
					::std::remove_cv_t<::std::remove_extent_t<
						unreferenced_value_type>>,
					char_type>};
			return ::fast_io::details::my_integral<clean_value_type> ||
				   ::fast_io::details::my_floating_point<clean_value_type> ||
				   ::std::same_as<clean_value_type, ::std::byte> ||
				   ::std::same_as<clean_value_type, ::std::nullptr_t> ||
				   same_character_array;
		}
	}
}

template <auto format_literal, format_parameter parameter,
		  typename... argument_types>
[[nodiscard]] inline consteval bool static_format_parameter() noexcept
{
	if constexpr (parameter.kind == format_parameter_kind::argument)
	{
		return static_format_reference<format_literal, parameter.argument,
									   argument_types...>();
	}
	else
	{
		return true;
	}
}

/**
 * Proves that a built-in replacement depends exclusively on NTTP-backed data.
 *
 * This intentionally recognizes only the two built-in grammars and scalar or
 * text carriers.  A custom grammar may inspect arbitrary members of its
 * argument pack, so generic lowering must not infer that width/precision are
 * its complete dependency set.
 */
template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
[[nodiscard]] inline consteval bool static_format_replacement() noexcept
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	if constexpr (!::std::same_as<clean_grammar_type,
								::fast_io::fmt::brace_fmt_t> &&
				  !::std::same_as<clean_grammar_type,
								 ::fast_io::fmt::printf_fmt_t>)
	{
		return false;
	}
	else
	{
		return static_format_reference<format_literal, field.argument,
									   argument_types...>() &&
			   static_format_parameter<format_literal,
								   field.specification.width,
								   argument_types...>() &&
			   static_format_parameter<format_literal,
								   field.specification.precision,
								   argument_types...>();
	}
}

template <::std::integral char_type>
struct measure_static_format_component
{
	template <typename value_type>
	[[nodiscard]] inline consteval ::std::size_t operator()(
		value_type &value) const
	{
		if constexpr (::std::same_as<
					  ::std::remove_cvref_t<value_type>, char_type>)
		{
			return 1u;
		}
		else if constexpr (::fast_io::details::decay::
					  print_semantic_precise_size_ok<
						  char_type, value_type &>::value)
		{
			return ::fast_io::operations::decay::
				print_semantic_precise_size_arg<char_type>(value);
		}
		else
		{
			return ::fast_io::operations::decay::
				print_semantic_bounded_size_arg<char_type>(value);
		}
	}
};

template <::std::integral char_type>
struct emit_static_format_component
{
	char_type *output;

	template <typename value_type>
	[[nodiscard]] inline consteval char_type *operator()(
		value_type &value) const
	{
		if constexpr (::std::same_as<
					  ::std::remove_cvref_t<value_type>, char_type>)
		{
			*output = value;
			return output + 1u;
		}
		else if constexpr (::fast_io::details::decay::
					  print_semantic_precise_size_ok<
						  char_type, value_type &>::value)
		{
			return ::fast_io::operations::decay::
				print_semantic_emit_unchecked_run<false, char_type>(
					output, value);
		}
		else
		{
			return ::fast_io::operations::decay::
				print_semantic_emit_unchecked_run<false, char_type, true>(
					output, value);
		}
	}
};

/** Keeps pre-rendering below default GCC/Clang constexpr step and loop budgets. */
inline constexpr ::std::size_t static_format_output_code_unit_limit{1u << 14u};

/** Measures one NTTP-backed replacement without instantiating its byte storage. */
template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
struct static_replacement_evaluation
{
	using char_type = typename decltype(format_literal)::value_type;
	using owned_argument_pack = static_evaluation_argument_pack<
		::std::index_sequence_for<argument_types...>,
		static_evaluation_argument_t<argument_types>...>;

	template <typename callback_type, ::std::size_t... index>
	[[nodiscard]] inline static consteval decltype(auto) evaluate(
		callback_type callback, ::std::index_sequence<index...>)
	{
		owned_argument_pack owned_arguments{};
		auto arguments{make_indexed_argument_pack(
			static_evaluation_argument_get<index>(owned_arguments)...)};
		decltype(auto) value{grammar_lower_adl::invoke<
			format_literal, field, grammar_type>(arguments)};
		return callback(value);
	}

	[[nodiscard]] inline static consteval ::std::size_t calculate_bound()
	{
		return evaluate(measure_static_format_component<char_type>{},
			::std::index_sequence_for<argument_types...>{});
	}

	static inline constexpr ::std::size_t bound{calculate_bound()};
};

/** Owns the exact compile-time spelling of one NTTP-backed replacement. */
template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
struct compiled_static_replacement
{
	using evaluation_type = static_replacement_evaluation<
		format_literal, field, grammar_type, argument_types...>;
	using char_type = typename evaluation_type::char_type;
	static inline constexpr ::std::size_t bound{evaluation_type::bound};
	static_assert(bound != SIZE_MAX,
		"fast_io format: a static replacement must have a finite contiguous bound");
	static_assert(bound <= static_format_output_code_unit_limit,
		"fast_io format: a static replacement exceeds the compile-time output budget");

	[[nodiscard]] inline static consteval ::std::size_t calculate_size()
	{
		if constexpr (bound == SIZE_MAX ||
			bound > static_format_output_code_unit_limit || bound == 0u)
		{
			return 0u;
		}
		else
		{
			::std::array<char_type, bound> scratch{};
			auto const end{evaluation_type::evaluate(
				emit_static_format_component<char_type>{scratch.data()},
				::std::index_sequence_for<argument_types...>{})};
			return static_cast<::std::size_t>(end - scratch.data());
		}
	}

	static inline constexpr ::std::size_t size{calculate_size()};

	[[nodiscard]] inline static consteval auto make_storage()
	{
		if constexpr (bound == SIZE_MAX ||
			bound > static_format_output_code_unit_limit)
		{
			return ::std::array<char_type, 0u>{};
		}
		else
		{
			::std::array<char_type, size> result{};
			if constexpr (size != 0u)
			{
				::std::array<char_type, bound> scratch{};
				auto const end{evaluation_type::evaluate(
					emit_static_format_component<char_type>{scratch.data()},
					::std::index_sequence_for<argument_types...>{})};
				if (end != scratch.data() + size)
				{
					::fast_io::fast_terminate();
				}
				for (::std::size_t index{}; index != size; ++index)
				{
					result[index] = scratch[index];
				}
			}
			return result;
		}
	}

	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{make_storage()};
};

template <typename replacement_type, ::std::size_t... index>
[[nodiscard]] inline constexpr auto make_small_static_replacement_pack(
	::std::index_sequence<index...>) noexcept
{
	return ::fast_io::manipulators::pack(
		::fast_io::manipulators::chvw(
			replacement_type::storage[index])...);
}

template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr auto make_static_replacement_operation() noexcept
{
	using replacement_type = compiled_static_replacement<
		format_literal, field, grammar_type, argument_types...>;
	if constexpr (replacement_type::size == 0u)
	{
		return ::fast_io::io_null;
	}
	else if constexpr (replacement_type::size == 1u)
	{
		return ::fast_io::manipulators::chvw(
			replacement_type::storage[0u]);
	}
	else if constexpr (replacement_type::size <= 16u)
	{
		return make_small_static_replacement_pack<replacement_type>(
			::std::make_index_sequence<replacement_type::size>{});
	}
	else if constexpr (replacement_type::size < 64u)
	{
		return ::fast_io::manipulators::static_scatter_t<
			typename replacement_type::char_type, replacement_type::size>{
			replacement_type::storage.data()};
	}
	else
	{
		return ::fast_io::basic_io_scatter_t<
			typename replacement_type::char_type>{
			replacement_type::storage.data(), replacement_type::size};
	}
}

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

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index, ::std::size_t... index,
		  typename... argument_types>
[[nodiscard]] inline constexpr decltype(auto) make_static_format_operation(
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments)
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind == format_operation_kind::replacement)
	{
		constexpr auto field{program.fields[operation.payload_index]};
		using grammar_type = ::std::remove_cvref_t<grammar_tag>;
		if constexpr (static_format_replacement<
						  format_literal, field, grammar_type,
						  argument_types...>())
		{
			return make_static_replacement_operation<
				format_literal, field, grammar_type, argument_types...>();
		}
		else
		{
			return make_format_operation<
				format_literal, grammar_tag, operation_index>(arguments);
		}
	}
	else
	{
		return make_format_operation<
			format_literal, grammar_tag, operation_index>(arguments);
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

template <auto format_literal, typename grammar_tag, typename callback_type,
		  typename argument_pack, ::std::size_t... operation_index>
inline constexpr decltype(auto) lower_static_format_program_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<operation_index...>)
{
	return ::std::forward<callback_type>(callback)(
		make_static_format_operation<
			format_literal, grammar_tag, operation_index>(arguments)...);
}

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index, typename... argument_types>
[[nodiscard]] inline consteval bool static_format_operation() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind == format_operation_kind::literal)
	{
		return true;
	}
	else
	{
		constexpr auto field{program.fields[operation.payload_index]};
		return static_format_replacement<
			format_literal, field, grammar_tag, argument_types...>();
	}
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types, ::std::size_t... operation_index>
[[nodiscard]] inline consteval bool static_format_program_impl(
	::std::index_sequence<operation_index...>) noexcept
{
	return (static_format_operation<format_literal, grammar_tag,
									operation_index, argument_types...>() &&
			...);
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
[[nodiscard]] inline consteval bool static_format_program() noexcept
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_program<format_literal,
											 grammar_tag>.operation_count};
	return static_format_program_impl<format_literal, grammar_tag,
		argument_types...>(::std::make_index_sequence<operation_count>{});
}

template <::std::size_t operation_capacity>
struct static_format_group_plan
{
	::std::array<::std::size_t, operation_capacity> begin{};
	::std::array<::std::size_t, operation_capacity> count{};
	::std::array<bool, operation_capacity> is_static{};
	::std::size_t group_count{};
	bool has_static_replacement{};
};

template <auto format_literal, typename grammar_tag,
		  typename... argument_types, ::std::size_t... operation_index>
[[nodiscard]] inline consteval auto make_static_format_group_plan_impl(
	::std::index_sequence<operation_index...>) noexcept
{
	constexpr ::std::size_t operation_count{sizeof...(operation_index)};
	constexpr ::std::array<bool, operation_count> static_flags{
		static_format_operation<format_literal, grammar_tag,
			operation_index, argument_types...>()...};
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	static_format_group_plan<operation_count> result{};
	::std::size_t operation{};
	while (operation != operation_count)
	{
		auto const group_index{result.group_count};
		auto const group_static{static_flags[operation]};
		result.begin[group_index] = operation;
		result.is_static[group_index] = group_static;
		do
		{
			if (group_static &&
				program.operations[operation].kind ==
					format_operation_kind::replacement)
			{
				result.has_static_replacement = true;
			}
			++result.count[group_index];
			++operation;
		} while (operation != operation_count &&
				 static_flags[operation] == group_static && group_static);
		++result.group_count;
	}
	return result;
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
[[nodiscard]] inline consteval auto make_static_format_group_plan() noexcept
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_program<format_literal,
											 grammar_tag>.operation_count};
	return make_static_format_group_plan_impl<
		format_literal, grammar_tag, argument_types...>(
		::std::make_index_sequence<operation_count>{});
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
inline constexpr auto static_format_groups{
	make_static_format_group_plan<format_literal, grammar_tag,
		argument_types...>()};

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index, typename... argument_types>
[[nodiscard]] inline consteval ::std::size_t
static_format_operation_output_bound() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind == format_operation_kind::literal)
	{
		return program.literal_runs[operation.payload_index].size;
	}
	else
	{
		constexpr auto field{program.fields[operation.payload_index]};
		using grammar_type = ::std::remove_cvref_t<grammar_tag>;
		if constexpr (static_format_replacement<
			format_literal, field, grammar_type, argument_types...>())
		{
			using evaluation_type = static_replacement_evaluation<
				format_literal, field, grammar_type, argument_types...>;
			return evaluation_type::bound;
		}
		else
		{
			return 0u;
		}
	}
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types, ::std::size_t... operation_index>
[[nodiscard]] inline consteval ::std::size_t
static_format_output_bound_impl(
	::std::index_sequence<operation_index...>) noexcept
{
	constexpr ::std::array<::std::size_t, sizeof...(operation_index)> bounds{
		static_format_operation_output_bound<
			format_literal, grammar_tag, operation_index,
			argument_types...>()...};
	::std::size_t total{};
	for (auto const bound : bounds)
	{
		if (bound == SIZE_MAX ||
			static_format_output_code_unit_limit - total < bound)
		{
			return SIZE_MAX;
		}
		total += bound;
	}
	return total;
}

/** Checked aggregate bound computed before any static byte array is instantiated. */
template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
[[nodiscard]] inline consteval ::std::size_t
static_format_output_bound() noexcept
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_program<format_literal,
			grammar_tag>.operation_count};
	return static_format_output_bound_impl<
		format_literal, grammar_tag, argument_types...>(
		::std::make_index_sequence<operation_count>{});
}

template <::std::integral char_type>
struct measure_static_format_components
{
	template <typename... component_types>
	[[nodiscard]] inline consteval ::std::size_t operator()(
		component_types &&...components) const
	{
		return ::fast_io::operations::decay::
			print_semantic_precise_total_size<false, char_type>(components...);
	}
};

template <::std::integral char_type>
struct emit_static_format_components
{
	char_type *output;

	template <typename... component_types>
	[[nodiscard]] inline consteval char_type *operator()(
		component_types &&...components) const
	{
		return ::fast_io::operations::decay::
			print_semantic_emit_unchecked_run<false, char_type>(
				output, components...);
	}
};

template <auto format_literal, typename grammar_tag,
		  ::std::size_t run_begin, typename callback_type,
		  typename argument_pack, ::std::size_t... run_index>
[[nodiscard]] inline constexpr decltype(auto) lower_static_format_run_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<run_index...>)
{
	return ::std::forward<callback_type>(callback)(
		make_static_format_operation<format_literal, grammar_tag,
			run_begin + run_index>(arguments)...);
}

/** Owns one maximal consecutive literal/static-field run in a mixed program. */
template <auto format_literal, typename grammar_tag,
		  ::std::size_t run_begin, ::std::size_t run_count,
		  typename... argument_types>
struct compiled_static_format_run
{
	using char_type = typename decltype(format_literal)::value_type;
	using owned_argument_pack = static_evaluation_argument_pack<
		::std::index_sequence_for<argument_types...>,
		static_evaluation_argument_t<argument_types>...>;

	template <typename callback_type, ::std::size_t... index>
	[[nodiscard]] inline static consteval decltype(auto) evaluate(
		callback_type callback, ::std::index_sequence<index...>)
	{
		owned_argument_pack owned_arguments{};
		auto arguments{make_indexed_argument_pack(
			static_evaluation_argument_get<index>(owned_arguments)...)};
		return lower_static_format_run_impl<
			format_literal, grammar_tag, run_begin>(
				callback, arguments,
				::std::make_index_sequence<run_count>{});
	}

	[[nodiscard]] inline static consteval ::std::size_t calculate_size()
	{
		return evaluate(measure_static_format_components<char_type>{},
			::std::index_sequence_for<argument_types...>{});
	}

	static inline constexpr ::std::size_t size{calculate_size()};
	static_assert(size != SIZE_MAX,
		"fast_io format: a static run must have one precise contiguous spelling");
	static_assert(size <= static_format_output_code_unit_limit,
		"fast_io format: a static run exceeds the compile-time output budget");

	[[nodiscard]] inline static consteval auto make_storage()
	{
		if constexpr (size == SIZE_MAX ||
			size > static_format_output_code_unit_limit)
		{
			return ::std::array<char_type, 0u>{};
		}
		else
		{
			::std::array<char_type, size> result{};
			if constexpr (size != 0u)
			{
				auto const end{evaluate(
					emit_static_format_components<char_type>{result.data()},
					::std::index_sequence_for<argument_types...>{})};
				if (end != result.data() + size)
				{
					::fast_io::fast_terminate();
				}
			}
			return result;
		}
	}

	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{make_storage()};
};

template <auto format_literal, typename grammar_tag,
		  ::std::size_t group_index, ::std::size_t... argument_index,
		  typename... argument_types>
[[nodiscard]] inline constexpr decltype(auto) make_format_group(
	indexed_argument_pack<::std::index_sequence<argument_index...>,
		argument_types...> &arguments)
{
	constexpr auto plan{
		static_format_groups<format_literal, grammar_tag, argument_types...>};
	constexpr auto run_begin{plan.begin[group_index]};
	constexpr auto run_count{plan.count[group_index]};
	if constexpr (plan.is_static[group_index])
	{
		using run_type = compiled_static_format_run<
			format_literal, grammar_tag, run_begin, run_count,
			argument_types...>;
		if constexpr (run_type::size == 0u)
		{
			return ::fast_io::io_null;
		}
		else
		{
			return ::fast_io::basic_io_scatter_t<
				typename run_type::char_type>{
				run_type::storage.data(), run_type::size};
		}
	}
	else
	{
		static_assert(run_count == 1u);
		return make_format_operation<
			format_literal, grammar_tag, run_begin>(arguments);
	}
}

template <auto format_literal, typename grammar_tag, typename callback_type,
		  typename argument_pack, ::std::size_t... group_index>
inline constexpr decltype(auto) lower_format_program_grouped_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<group_index...>)
{
	return ::std::forward<callback_type>(callback)(
		make_format_group<format_literal, grammar_tag, group_index>(arguments)...);
}

/** Owns a completely static format program as one final code-unit sequence. */
template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
struct compiled_static_format_program
{
	using char_type = typename decltype(format_literal)::value_type;
	using owned_argument_pack = static_evaluation_argument_pack<
		::std::index_sequence_for<argument_types...>,
		static_evaluation_argument_t<argument_types>...>;
	static inline constexpr ::std::size_t operation_count{
		::fast_io::fmt::details::checked_program<format_literal,
											 grammar_tag>.operation_count};

	template <typename callback_type, ::std::size_t... index>
	[[nodiscard]] inline static consteval decltype(auto) evaluate(
		callback_type callback, ::std::index_sequence<index...>)
	{
		owned_argument_pack owned_arguments{};
		auto arguments{make_indexed_argument_pack(
			static_evaluation_argument_get<index>(owned_arguments)...)};
		return lower_static_format_program_impl<format_literal, grammar_tag>(
			callback, arguments,
			::std::make_index_sequence<operation_count>{});
	}

	[[nodiscard]] inline static consteval ::std::size_t calculate_size()
	{
		return evaluate(measure_static_format_components<char_type>{},
			::std::index_sequence_for<argument_types...>{});
	}

	static inline constexpr ::std::size_t size{calculate_size()};
	static_assert(size != SIZE_MAX,
		"fast_io format: a static program must have one precise contiguous spelling");
	static_assert(size <= static_format_output_code_unit_limit,
		"fast_io format: a static program exceeds the compile-time output budget");

	[[nodiscard]] inline static consteval auto make_storage()
	{
		if constexpr (size == SIZE_MAX ||
			size > static_format_output_code_unit_limit)
		{
			return ::std::array<char_type, 0u>{};
		}
		else
		{
			::std::array<char_type, size> result{};
			if constexpr (size != 0u)
			{
				auto const end{evaluate(
					emit_static_format_components<char_type>{result.data()},
					::std::index_sequence_for<argument_types...>{})};
				if (end != result.data() + size)
				{
					::fast_io::fast_terminate();
				}
			}
			return result;
		}
	}

	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{make_storage()};
};

template <auto format_literal, typename grammar_tag, typename callback_type, typename... argument_types>
inline constexpr decltype(auto) lower_format_program(
	callback_type &&callback, argument_types &...arguments)
{
	using literal_program = ::fast_io::fmt::details::compiled_literal_program<
		format_literal, grammar_tag>;
	if constexpr (literal_program::literal_only)
	{
		if constexpr (literal_program::size == 0u)
		{
			return ::std::forward<callback_type>(callback)();
		}
		else
		{
			return ::std::forward<callback_type>(callback)(
				::fast_io::basic_io_scatter_t<typename literal_program::char_type>{
					literal_program::storage.data(), literal_program::size});
		}
	}
	else if constexpr (!(::fast_io::fmt::
		is_static_format_argument_holder_v<argument_types> || ...))
	{
		// Preserve the original dynamic front-end instantiation graph exactly.
		// In particular, a translation unit which never names static_arg must not
		// instantiate static dependency resolution, grouping, or materialization.
		auto indexed_arguments{make_indexed_argument_pack(arguments...)};
		constexpr auto operation_count{
			::fast_io::fmt::details::checked_program<format_literal,
				grammar_tag>.operation_count};
		return lower_format_program_impl<format_literal, grammar_tag>(
			::std::forward<callback_type>(callback), indexed_arguments,
			::std::make_index_sequence<operation_count>{});
	}
	else if constexpr (
		static_format_groups<format_literal, grammar_tag,
			argument_types...>.has_static_replacement &&
		static_format_output_bound<format_literal, grammar_tag,
			argument_types...>() == SIZE_MAX)
	{
		static_assert(static_format_output_bound<
			format_literal, grammar_tag, argument_types...>() != SIZE_MAX,
			"fast_io format: the combined static runs exceed the compile-time output budget");
		return ::std::forward<callback_type>(callback)();
	}
	else if constexpr (static_format_program<
						  format_literal, grammar_tag,
						  argument_types...>())
	{
		using static_program = compiled_static_format_program<
			format_literal, grammar_tag, argument_types...>;
		if constexpr (static_program::size == 0u)
		{
			// Do not erase an explicitly supplied static field merely because its
			// final spelling is empty. io_null carries the print event through
			// status and mutex customization while the core performs no byte write.
			return ::std::forward<callback_type>(callback)(::fast_io::io_null);
		}
		else
		{
			return ::std::forward<callback_type>(callback)(
				::fast_io::manipulators::static_scatter_t<
					typename static_program::char_type, static_program::size>{
					static_program::storage.data()});
		}
	}
	else
	{
		auto indexed_arguments{make_indexed_argument_pack(arguments...)};
		constexpr auto operation_count{
			::fast_io::fmt::details::checked_program<format_literal, grammar_tag>.operation_count};
		constexpr auto group_plan{
			static_format_groups<format_literal, grammar_tag,
				argument_types...>};
		if constexpr (group_plan.has_static_replacement)
		{
			return lower_format_program_grouped_impl<
				format_literal, grammar_tag>(
					::std::forward<callback_type>(callback), indexed_arguments,
					::std::make_index_sequence<group_plan.group_count>{});
		}
		else
		{
			return lower_format_program_impl<format_literal, grammar_tag>(
				::std::forward<callback_type>(callback), indexed_arguments,
				::std::make_index_sequence<operation_count>{});
		}
	}
}

} // namespace fast_io::fmt::details
