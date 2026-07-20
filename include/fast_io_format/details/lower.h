#pragma once

#include "arguments.h"
#include "compile.h"
#include "replacement_rules.h"
#include "rule_protocol.h"
#include "../../fast_io_dsal/array.h"

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Proves that the compiled grammar leaves one line feed in its final literal operation. */
template <auto format_literal, typename grammar_tag>
[[nodiscard]] inline consteval bool
format_program_has_terminal_literal_line_feed() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	if constexpr (program.operation_count == 0u)
	{
		return false;
	}
	else
	{
		constexpr auto operation{
			program.operations[program.operation_count - 1u]};
		if constexpr (operation.kind != format_operation_kind::literal)
		{
			return false;
		}
		else
		{
			constexpr auto run{program.literal_runs[operation.payload_index]};
			if constexpr (run.size == 0u)
			{
				return false;
			}
			else
			{
				using char_type = typename decltype(format_literal)::value_type;
				return program.literal_storage[run.offset + run.size - 1u] ==
					   ::fast_io::char_literal_v<u8'\n', char_type>;
			}
		}
	}
}

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

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index,
		  bool trim_terminal_literal_line_feed = false>
[[nodiscard]] inline constexpr auto make_literal_operation() noexcept
{
	using literal_type = compiled_literal_run<format_literal, grammar_tag, operation_index>;
	static_assert(literal_type::run.size != 0u,
				  "fast_io format: a literal operation must not be empty");
	static_assert(!trim_terminal_literal_line_feed ||
					  literal_type::storage[literal_type::run.size - 1u] ==
						  ::fast_io::char_literal_v<u8'\n', typename literal_type::char_type>,
				  "fast_io format: only a proved terminal literal line feed may be trimmed");
	constexpr ::std::size_t exposed_size{
		literal_type::run.size -
		static_cast<::std::size_t>(trim_terminal_literal_line_feed)};
	if constexpr (exposed_size == 0u)
	{
		// io_null lets concat's ordinary normalization erase the empty tail before
		// strategy selection; the continuation owns the separately proved LF.
		return ::fast_io::io_null;
	}
	else if constexpr (exposed_size < 64u)
	{
		// A format literal is provider-owned immutable storage.  Preserve that fact
		// in the lowered print component even for one code unit: unbuffered native-
		// scatter outputs can then pass the provider pointer directly, while concat
		// and buffered print continue to consume static_scatter's reserve spelling.
		return ::fast_io::manipulators::static_scatter_t<
			typename literal_type::char_type, exposed_size>{
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
			literal_type::storage.data(), exposed_size};
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
// GCC 13 does not inline this stateless ADL transport through the two grouped
// wrappers below.  That turns a literal floating argument into an opaque
// returned aggregate before core can evaluate its compiler-constant query.
// The three legacy-GNU placements are an indivisible A/B chain: removing any
// one leaves static_format_endpoint at exit 25; keeping all three makes the
// complete 31-byte record one bounded staged write. GCC 15 and Clang already
// inline the chain and are deliberately left to their normal cost model.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 15
[[__gnu__::__always_inline__]]
#endif
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

template <::fast_io::manipulators::static_argument_constant value_literal>
struct static_evaluation_argument<
	::fast_io::manipulators::static_arg_t<value_literal>>
{
	using type = ::fast_io::manipulators::static_arg_t<value_literal>;
};

template <auto... value_literals>
struct static_evaluation_argument<
	::fast_io::fmt::static_tuple_format_arg<value_literals...>>
{
	using type = ::fast_io::fmt::static_tuple_format_arg<value_literals...>;
};

template <::fast_io::fmt::basic_fixed_string name_literal,
		  typename storage_type>
struct static_evaluation_argument<
	::fast_io::fmt::static_named_arg<name_literal, storage_type>>
{
	using clean_storage_type = ::std::remove_cvref_t<storage_type>;
	using selected_storage_type = ::std::conditional_t<
		::fast_io::fmt::is_static_format_argument_holder_v<clean_storage_type>,
		clean_storage_type, static_evaluation_unused_argument>;
	using type = ::fast_io::fmt::static_named_arg<
		name_literal, selected_storage_type>;
};

template <::fast_io::manipulators::static_argument_constant name_literal,
	::fast_io::manipulators::static_argument_constant value_literal>
struct static_evaluation_argument<
	::fast_io::manipulators::static_named_arg_t<
		name_literal, value_literal>>
{
	using type = ::fast_io::manipulators::static_named_arg_t<
		name_literal, value_literal>;
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

template <bool valid, auto format_literal, argument_reference reference,
		  typename... argument_types>
struct static_format_argument_descriptor_impl
{
	static inline constexpr bool holder_is_static{};
};

template <auto format_literal, argument_reference reference,
		  typename... argument_types>
struct static_format_argument_descriptor_impl<
	true, format_literal, reference, argument_types...>
{
	static inline constexpr auto resolution{
		resolve_argument_reference<format_literal, reference,
								   argument_types...>()};
	using argument_pack_type = indexed_argument_pack<
		::std::index_sequence_for<argument_types...>, argument_types...>;
	using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
		::std::declval<argument_pack_type &>()))>;
	static inline constexpr bool holder_is_static{
		::fast_io::fmt::is_static_format_argument_holder_v<holder_type>};
	using evaluation_holder_type = static_evaluation_argument_t<holder_type>;
	using value_reference = decltype(unwrap_static_named_argument(
		::std::declval<evaluation_holder_type &>()));
};

template <auto format_literal, argument_reference reference,
		  typename... argument_types>
using static_format_argument_descriptor =
	static_format_argument_descriptor_impl<
		resolve_argument_reference<format_literal, reference,
								   argument_types...>()
				.error == argument_resolution_error::none,
		format_literal, reference, argument_types...>;

template <bool enabled, auto format_literal, replacement_field field,
		  typename value_reference>
struct static_custom_formatter_selection
{
	static inline constexpr bool available{};
};

template <auto format_literal, replacement_field field,
		  typename value_reference>
struct static_custom_formatter_selection<
	true, format_literal, field, value_reference>
{
	using type = custom_format_state_tag<
		format_literal, field.specification.raw_format_specification,
		value_reference>;
	static inline constexpr bool available{true};
};

template <typename rule_type, auto format_literal, replacement_field field,
		  typename value_reference>
struct static_output_formatter_selection
{
	static inline constexpr bool available{};
};

template <auto format_literal, replacement_field field,
		  typename value_reference>
struct static_output_formatter_selection<
	brace_format_as_rule, format_literal, field, value_reference>
{
	using type = ::fast_io::fmt::basic_static_format_as_t<
		typename decltype(format_literal)::value_type>;
	static inline constexpr bool available{
		printable_adl_format_as<
			typename decltype(format_literal)::value_type,
			value_reference>};
};

template <auto format_literal, replacement_field field,
		  typename value_reference>
struct static_output_formatter_selection<
	brace_custom_format_rule, format_literal, field, value_reference>
	: static_custom_formatter_selection<
		  structurally_compiled_custom_format<
			  format_literal,
			  field.specification.raw_format_specification,
			  value_reference> &&
			  custom_format_printable<
				  format_literal,
				  field.specification.raw_format_specification,
				  value_reference>,
		  format_literal, field, value_reference>
{};

template <auto format_literal, replacement_field field,
		  typename grammar_type, ::std::size_t depth,
		  typename... argument_types>
struct static_output_replacement_traits
{
	using descriptor = static_format_argument_descriptor<
		format_literal, field.argument, argument_types...>;
	static inline constexpr bool brace_grammar{
		::std::same_as<::std::remove_cvref_t<grammar_type>,
					   ::fast_io::fmt::brace_fmt_t>};

	[[nodiscard]] inline static consteval bool has_rule() noexcept
	{
		if constexpr (!brace_grammar ||
					  field.specification.locale_specific ||
					  !descriptor::holder_is_static)
		{
			return false;
		}
		else
		{
			return format_replacement_rule_type_adl::expression<
				grammar_type, format_literal, field,
				typename descriptor::value_reference>;
		}
	}

	static inline constexpr bool rule_available{has_rule()};
};

template <bool rule_available, auto format_literal,
		  replacement_field field, typename grammar_type,
		  ::std::size_t depth, typename... argument_types>
struct static_output_replacement_selection
{
	static inline constexpr bool available{};
};

template <auto format_literal, replacement_field field,
		  typename grammar_type, ::std::size_t depth,
		  typename... argument_types>
struct static_output_replacement_selection<
	true, format_literal, field, grammar_type, depth, argument_types...>
{
	using descriptor = static_format_argument_descriptor<
		format_literal, field.argument, argument_types...>;
	using rule_type =
		format_replacement_rule_type_adl::selected_rule_t<
			grammar_type, format_literal, field,
			typename descriptor::value_reference>;
	using formatter_selection = static_output_formatter_selection<
		rule_type, format_literal, field,
		typename descriptor::value_reference>;
	static inline constexpr bool formatter_available{
		formatter_selection::available};

	template <bool enabled, typename unused_type = void>
	struct protocol_selection
	{
		static inline constexpr bool available{};
	};

	template <typename unused_type>
	struct protocol_selection<true, unused_type>
	{
		using formatter_type = typename formatter_selection::type;
		using context_type =
			::fast_io::fmt::basic_static_format_context_t<
				field.specification, depth>;
		static inline constexpr bool available{
			static_format_output_adl::expression<
				context_type, formatter_type,
				typename descriptor::value_reference>};
	};

	using protocol = protocol_selection<formatter_available>;
	static inline constexpr bool available{protocol::available};
};

template <auto format_literal, replacement_field field,
		  typename grammar_type, ::std::size_t depth,
		  typename... argument_types>
using static_output_replacement = static_output_replacement_selection<
	static_output_replacement_traits<
		format_literal, field, grammar_type, depth,
		argument_types...>::rule_available,
	format_literal, field, grammar_type, depth, argument_types...>;

/** Compile-time aggregate budgets keep recursive rendering predictable. */
inline constexpr ::std::size_t static_format_output_code_unit_limit{1u << 14u};
inline constexpr ::std::size_t static_format_aggregate_element_limit{256u};
inline constexpr ::std::size_t static_format_tuple_element_limit{64u};
inline constexpr ::std::size_t static_format_aggregate_recursion_limit{
	static_format_recursion_limit};

template <typename T>
struct static_std_array_traits
{
	static inline constexpr bool value{};
};

template <typename element_type, ::std::size_t extent>
struct static_std_array_traits<::std::array<element_type, extent>>
{
	static inline constexpr bool value{true};
	using value_type = element_type;
	static inline constexpr ::std::size_t size{extent};
};

template <typename T>
inline constexpr bool static_fixed_aggregate_v{
	(::std::is_array_v<::std::remove_cv_t<T>> &&
	 !::fast_io::fmt::format_character<::std::remove_cv_t<
		 ::std::remove_extent_t<::std::remove_cv_t<T>>>>) ||
	static_std_array_traits<::std::remove_cv_t<T>>::value ||
	tuple_format_source<::std::remove_cv_t<T>>};

template <typename... types>
struct static_format_aggregate_type_stack
{};

template <typename value_type, typename stack_type>
struct static_format_aggregate_stack_contains;

template <typename value_type, typename... stack_types>
struct static_format_aggregate_stack_contains<
	value_type, static_format_aggregate_type_stack<stack_types...>>
	: ::std::bool_constant<
		  (::std::same_as<value_type, stack_types> || ...)>
{};

template <typename stack_type, typename value_type>
struct static_format_aggregate_stack_push;

template <typename... stack_types, typename value_type>
struct static_format_aggregate_stack_push<
	static_format_aggregate_type_stack<stack_types...>, value_type>
{
	using type = static_format_aggregate_type_stack<
		stack_types..., value_type>;
};

template <typename stack_type, typename value_type>
using static_format_aggregate_stack_push_t =
	typename static_format_aggregate_stack_push<
		stack_type, value_type>::type;

struct static_format_aggregate_shape
{
	bool supported{};
	::std::size_t elements{};
	::std::size_t depth{};
	bool tuple_budget{true};
};

inline constexpr ::std::size_t static_format_aggregate_element_overflow{
	static_format_aggregate_element_limit + 1u};

[[nodiscard]] inline consteval ::std::size_t
add_static_format_aggregate_elements(::std::size_t left,
									 ::std::size_t right) noexcept
{
	if (left > static_format_aggregate_element_limit ||
		right > static_format_aggregate_element_limit ||
		right > static_format_aggregate_element_limit - left)
	{
		return static_format_aggregate_element_overflow;
	}
	return left + right;
}

[[nodiscard]] inline consteval ::std::size_t
repeat_static_format_aggregate_elements(::std::size_t extent,
										::std::size_t child_elements) noexcept
{
	if (extent > static_format_aggregate_element_limit)
	{
		return static_format_aggregate_element_overflow;
	}
	auto const remaining{static_format_aggregate_element_limit - extent};
	if (child_elements != 0u && extent > remaining / child_elements)
	{
		return static_format_aggregate_element_overflow;
	}
	return extent + extent * child_elements;
}

[[nodiscard]] inline consteval static_format_aggregate_shape
merge_static_format_aggregate_shape(static_format_aggregate_shape result,
									static_format_aggregate_shape child) noexcept
{
	if (!child.supported)
	{
		result.supported = false;
	}
	if (!child.tuple_budget)
	{
		result.tuple_budget = false;
	}
	result.elements = add_static_format_aggregate_elements(
		result.elements, child.elements);
	auto const child_depth{child.depth + 1u};
	if (result.depth < child_depth)
	{
		result.depth = child_depth;
	}
	return result;
}

template <::std::integral char_type, typename value_type,
		  ::std::size_t remaining_depth =
			  static_format_aggregate_recursion_limit,
		  typename stack_type = static_format_aggregate_type_stack<>>
[[nodiscard]] inline consteval static_format_aggregate_shape
make_static_format_aggregate_shape() noexcept;

template <::std::integral char_type, typename tuple_type,
		  ::std::size_t remaining_depth, typename stack_type,
		  ::std::size_t index, static_format_aggregate_shape result>
[[nodiscard]] inline consteval static_format_aggregate_shape
make_static_format_tuple_shape() noexcept
{
	constexpr ::std::size_t extent{::std::tuple_size_v<tuple_type>};
	if constexpr (index == extent || !result.supported ||
				  result.elements > static_format_aggregate_element_limit)
	{
		return result;
	}
	else
	{
		using child_type = ::std::remove_cvref_t<decltype(brace_tuple_get<index>(::std::declval<tuple_type &>()))>;
		constexpr auto next{merge_static_format_aggregate_shape(
			result,
			make_static_format_aggregate_shape<
				char_type, child_type, remaining_depth, stack_type>())};
		return make_static_format_tuple_shape<
			char_type, tuple_type, remaining_depth, stack_type,
			index + 1u, next>();
	}
}

template <::std::integral char_type, typename value_type,
		  ::std::size_t remaining_depth, typename stack_type>
[[nodiscard]] inline consteval static_format_aggregate_shape
make_static_format_aggregate_shape() noexcept
{
	using clean_type = ::std::remove_cv_t<value_type>;
	if constexpr (::fast_io::details::my_integral<clean_type> ||
				  ::fast_io::details::my_floating_point<clean_type> ||
				  ::std::same_as<clean_type, ::std::byte> ||
				  ::std::same_as<clean_type, ::std::nullptr_t>)
	{
		return {true, 0u, 0u};
	}
	else if constexpr (::std::is_array_v<clean_type>)
	{
		using element_type =
			::std::remove_cv_t<::std::remove_extent_t<clean_type>>;
		constexpr ::std::size_t extent{::std::extent_v<clean_type>};
		if constexpr (::fast_io::fmt::format_character<element_type>)
		{
			return {::std::same_as<element_type, char_type>, 0u, 0u};
		}
		else if constexpr (static_format_aggregate_stack_contains<
							   clean_type, stack_type>::value)
		{
			return {false,
					extent > static_format_aggregate_element_limit
						? static_format_aggregate_element_overflow
						: extent,
					1u};
		}
		else if constexpr (remaining_depth == 0u)
		{
			return {true,
					extent > static_format_aggregate_element_limit
						? static_format_aggregate_element_overflow
						: extent,
					1u};
		}
		else
		{
			using next_stack = static_format_aggregate_stack_push_t<
				stack_type, clean_type>;
			constexpr auto child{
				make_static_format_aggregate_shape<
					char_type, element_type, remaining_depth - 1u,
					next_stack>()};
			if constexpr (!child.supported)
			{
				return {false, child.elements, child.depth + 1u,
						child.tuple_budget};
			}
			else
			{
				return {true,
						repeat_static_format_aggregate_elements(
							extent, child.elements),
						child.depth + 1u, child.tuple_budget};
			}
		}
	}
	else if constexpr (static_std_array_traits<clean_type>::value)
	{
		using element_type = typename static_std_array_traits<clean_type>::value_type;
		constexpr ::std::size_t extent{
			static_std_array_traits<clean_type>::size};
		if constexpr (static_format_aggregate_stack_contains<
						  clean_type, stack_type>::value)
		{
			return {false,
					extent > static_format_aggregate_element_limit
						? static_format_aggregate_element_overflow
						: extent,
					1u};
		}
		else if constexpr (remaining_depth == 0u)
		{
			return {true,
					extent > static_format_aggregate_element_limit
						? static_format_aggregate_element_overflow
						: extent,
					1u};
		}
		else
		{
			using next_stack = static_format_aggregate_stack_push_t<
				stack_type, clean_type>;
			constexpr auto child{
				make_static_format_aggregate_shape<
					char_type, element_type, remaining_depth - 1u,
					next_stack>()};
			if constexpr (!child.supported)
			{
				return {false, child.elements, child.depth + 1u,
						child.tuple_budget};
			}
			else
			{
				return {true,
						repeat_static_format_aggregate_elements(
							extent, child.elements),
						child.depth + 1u, child.tuple_budget};
			}
		}
	}
	else if constexpr (tuple_format_source<clean_type>)
	{
		constexpr ::std::size_t extent{::std::tuple_size_v<clean_type>};
		if constexpr (extent > static_format_tuple_element_limit)
		{
			return {true, extent, 1u, false};
		}
		else if constexpr (static_format_aggregate_stack_contains<
							   clean_type, stack_type>::value)
		{
			return {false, extent, 1u};
		}
		else if constexpr (remaining_depth == 0u)
		{
			return {true, extent, 1u};
		}
		else
		{
			using next_stack = static_format_aggregate_stack_push_t<
				stack_type, clean_type>;
			constexpr static_format_aggregate_shape initial{
				true, extent, 1u};
			return make_static_format_tuple_shape<
				char_type, clean_type, remaining_depth - 1u,
				next_stack, 0u, initial>();
		}
	}
	else
	{
		return {};
	}
}

template <auto format_literal, argument_reference reference,
			  typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_reference_is_aggregate() noexcept
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
		using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
			::std::declval<argument_pack_type &>()))>;
		if constexpr (!::fast_io::fmt::is_static_format_argument_holder_v<
						  holder_type>)
		{
			return false;
		}
		else
		{
			using evaluation_type = static_evaluation_argument_t<holder_type>;
			using value_type = ::std::remove_cvref_t<decltype(unwrap_static_named_argument(
				::std::declval<evaluation_type &>()))>;
			return static_fixed_aggregate_v<value_type>;
		}
	}
}

template <typename value_type>
struct static_format_native_scalar_manip : ::std::false_type
{};

template <::fast_io::manipulators::scalar_flags flags, typename value_type>
struct static_format_native_scalar_manip<
	::fast_io::manipulators::scalar_manip_t<flags, value_type>>
	: ::std::bool_constant<
		  ::fast_io::details::my_integral<::std::remove_cvref_t<value_type>> ||
		  ::fast_io::details::my_floating_point<
			  ::std::remove_cvref_t<value_type>> ||
		  ::std::same_as<::std::remove_cvref_t<value_type>, ::std::byte> ||
		  ::std::same_as<::std::remove_cvref_t<value_type>,
					 ::std::nullptr_t>>
{};

template <::fast_io::manipulators::scalar_flags flags, typename value_type>
struct static_format_native_scalar_manip<
	::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>
	: ::std::bool_constant<
		  ::fast_io::details::my_integral<::std::remove_cvref_t<value_type>> ||
		  ::fast_io::details::my_floating_point<
			  ::std::remove_cvref_t<value_type>> ||
		  ::std::same_as<::std::remove_cvref_t<value_type>, ::std::byte> ||
		  ::std::same_as<::std::remove_cvref_t<value_type>,
					 ::std::nullptr_t>>
{};

template <typename value_type>
inline constexpr bool static_format_native_scalar_manip_v{
	static_format_native_scalar_manip<
		::std::remove_cvref_t<value_type>>::value};

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
		using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
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
			constexpr bool scalar_or_text{
				::fast_io::details::my_integral<clean_value_type> ||
					::fast_io::details::my_floating_point<clean_value_type> ||
					::std::same_as<clean_value_type, ::std::byte> ||
					::std::same_as<clean_value_type, ::std::nullptr_t> ||
					static_format_native_scalar_manip_v<clean_value_type> ||
					same_character_array};
			if constexpr (scalar_or_text)
			{
				return true;
			}
			else if constexpr (static_fixed_aggregate_v<clean_value_type>)
			{
				constexpr auto shape{
					make_static_format_aggregate_shape<char_type,
											   clean_value_type>()};
				if constexpr (!shape.supported)
				{
					// A structural aggregate may still be dynamically formatable through
					// an element custom formatter.  Unsupported static leaves are a
					// fail-closed fallback, not an explicit static-contract violation.
					return false;
				}
				else
				{
					static_assert(shape.elements <=
									  static_format_aggregate_element_limit,
								  "fast_io format: a static aggregate exceeds the compile-time element budget");
					static_assert(shape.tuple_budget,
								  "fast_io format: a static tuple exceeds the compile-time tuple expansion budget");
					static_assert(shape.depth <=
									  static_format_aggregate_recursion_limit,
								  "fast_io format: a static aggregate exceeds the compile-time recursion budget");
					return shape.tuple_budget &&
						   shape.elements <=
							   static_format_aggregate_element_limit &&
						   shape.depth <=
							   static_format_aggregate_recursion_limit;
				}
			}
			else
			{
				return false;
			}
		}
	}
}

template <auto format_literal, argument_reference reference,
		  typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_enum_reference() noexcept
{
	using descriptor = static_format_argument_descriptor<
		format_literal, reference, argument_types...>;
	if constexpr (!descriptor::holder_is_static)
	{
		return false;
	}
	else
	{
		return ::std::is_enum_v<::std::remove_cvref_t<
			typename descriptor::value_reference>>;
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

template <auto format_literal, format_parameter parameter,
		  ::std::size_t divisor, typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_parameter_within_output_budget() noexcept
{
	static_assert(divisor != 0u);
	constexpr ::std::size_t per_element_limit{
		static_format_output_code_unit_limit / divisor};
	if constexpr (parameter.kind == format_parameter_kind::none)
	{
		return true;
	}
	else if constexpr (parameter.kind == format_parameter_kind::literal)
	{
		return parameter.value <= per_element_limit;
	}
	else
	{
		constexpr auto resolution{resolve_argument_reference<
			format_literal, parameter.argument, argument_types...>()};
		if constexpr (resolution.error != argument_resolution_error::none)
		{
			return false;
		}
		else
		{
			using argument_pack_type = indexed_argument_pack<
				::std::index_sequence_for<argument_types...>, argument_types...>;
			using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
				::std::declval<argument_pack_type &>()))>;
			if constexpr (!::fast_io::fmt::is_static_format_argument_holder_v<
							  holder_type>)
			{
				return false;
			}
			else
			{
				using evaluation_type = static_evaluation_argument_t<holder_type>;
				evaluation_type holder{};
				decltype(auto) value{unwrap_static_named_argument(holder)};
				using clean_type = ::std::remove_cvref_t<decltype(value)>;
				if constexpr (!::fast_io::details::my_integral<clean_type> ||
							  ::std::same_as<clean_type, bool> ||
							  ::fast_io::details::character_integral<clean_type>)
				{
					return false;
				}
				else
				{
					using unsigned_type =
						::fast_io::details::my_make_unsigned_t<clean_type>;
					auto const bits{static_cast<unsigned_type>(value)};
					unsigned_type magnitude{bits};
					if constexpr (::fast_io::details::my_signed_integral<clean_type>)
					{
						if (value < 0)
						{
							magnitude = static_cast<unsigned_type>(
								unsigned_type{} - bits);
						}
					}
					if constexpr (sizeof(unsigned_type) <= sizeof(::std::size_t))
					{
						return static_cast<::std::size_t>(magnitude) <=
							   per_element_limit;
					}
					else
					{
						return magnitude <=
							   static_cast<unsigned_type>(per_element_limit);
					}
				}
			}
		}
	}
}

template <auto format_literal, format_parameter parameter,
		  typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_parameter_output_budget_available() noexcept
{
	if constexpr (parameter.kind != format_parameter_kind::argument)
	{
		return true;
	}
	else
	{
		constexpr auto resolution{resolve_argument_reference<
			format_literal, parameter.argument, argument_types...>()};
		if constexpr (resolution.error != argument_resolution_error::none)
		{
			return false;
		}
		else
		{
			using argument_pack_type = indexed_argument_pack<
				::std::index_sequence_for<argument_types...>, argument_types...>;
			using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
				::std::declval<argument_pack_type &>()))>;
			if constexpr (!::fast_io::fmt::is_static_format_argument_holder_v<
							  holder_type>)
			{
				return false;
			}
			else
			{
				using evaluation_type = static_evaluation_argument_t<holder_type>;
				using clean_type = ::std::remove_cvref_t<decltype(unwrap_static_named_argument(
					::std::declval<evaluation_type &>()))>;
				return ::fast_io::details::my_integral<clean_type> &&
					   !::std::same_as<clean_type, bool> &&
					   !::fast_io::details::character_integral<clean_type>;
			}
		}
	}
}

template <typename aggregate_type>
[[nodiscard]] inline consteval ::std::size_t
static_format_aggregate_direct_extent() noexcept
{
	using clean_type = ::std::remove_cv_t<aggregate_type>;
	if constexpr (::std::is_array_v<clean_type>)
	{
		return ::std::extent_v<clean_type>;
	}
	else if constexpr (static_std_array_traits<clean_type>::value)
	{
		return static_std_array_traits<clean_type>::size;
	}
	else
	{
		return ::std::tuple_size_v<clean_type>;
	}
}

template <typename aggregate_type>
struct static_format_aggregate_element
{};

template <typename element_type, ::std::size_t extent>
struct static_format_aggregate_element<element_type[extent]>
{
	using type = element_type;
};

template <typename element_type, ::std::size_t extent>
struct static_format_aggregate_element<::std::array<element_type, extent>>
{
	using type = element_type;
};

struct static_format_aggregate_range_dependency_result
{
	bool dependencies{};
	bool output_budget_exceeded{};
};

template <auto format_literal, replacement_field field,
		  typename... argument_types>
[[nodiscard]] inline consteval static_format_aggregate_range_dependency_result
make_static_format_aggregate_range_dependency() noexcept
{
	if constexpr (!static_format_reference_is_aggregate<
					  format_literal, field.argument, argument_types...>())
	{
		return {true, false};
	}
	else
	{
		constexpr auto resolution{resolve_argument_reference<
			format_literal, field.argument, argument_types...>()};
		using argument_pack_type = indexed_argument_pack<
			::std::index_sequence_for<argument_types...>, argument_types...>;
		using holder_type = ::std::remove_cvref_t<decltype(indexed_argument_get<resolution.index>(
			::std::declval<argument_pack_type &>()))>;
		using evaluation_type = static_evaluation_argument_t<holder_type>;
		using aggregate_type = ::std::remove_cvref_t<decltype(unwrap_static_named_argument(
			::std::declval<evaluation_type &>()))>;
		constexpr auto specification{
			checked_range_specification_from_common<
				format_literal, field.specification>()};
		if constexpr (!specification.has_element_specification)
		{
			return {true, false};
		}
		else if constexpr (tuple_format_source<aggregate_type>)
		{
			// The existing tuple grammar deliberately accepts only `n`; let its
			// normal diagnostic remain authoritative for an invalid element spec.
			return {false, false};
		}
		else
		{
			using element_type = ::std::remove_cv_t<typename static_format_aggregate_element<aggregate_type>::type>;
			if constexpr (static_fixed_aggregate_v<element_type>)
			{
				// A nested range owns another type-directed suffix. Supporting it
				// requires recursively proving that suffix's argument references.
				return {false, false};
			}
			else
			{
				constexpr auto element_field{
					checked_range_element_field<specification>};
				constexpr ::std::size_t direct_extent{
					static_format_aggregate_direct_extent<aggregate_type>()};
				constexpr ::std::size_t divisor{
					direct_extent == 0u ? 1u : direct_extent};
				constexpr bool width_available{
					static_format_parameter_output_budget_available<
						format_literal, element_field.specification.width,
						argument_types...>()};
				constexpr bool precision_available{
					static_format_parameter_output_budget_available<
						format_literal, element_field.specification.precision,
						argument_types...>()};
				if constexpr (!width_available || !precision_available)
				{
					return {false, false};
				}
				else if constexpr (direct_extent == 0u)
				{
					// No element formatter runs, so even large static element
					// parameters cannot contribute output.
					return {true, false};
				}
				else
				{
					constexpr bool width_within_budget{
						static_format_parameter_within_output_budget<
							format_literal, element_field.specification.width,
							divisor, argument_types...>()};
					constexpr bool precision_within_budget{
						static_format_parameter_within_output_budget<
							format_literal, element_field.specification.precision,
							divisor, argument_types...>()};
					// Width is a minimum field length, so width * extent alone can
					// prove an overflow. Precision may be an upper bound, truncation,
					// or custom semantics; retain its previous dynamic fallback.
					return {width_within_budget && precision_within_budget,
							!width_within_budget};
				}
			}
		}
	}
}

template <auto format_literal, replacement_field field,
		  typename... argument_types>
inline constexpr auto static_format_aggregate_range_dependency{
	make_static_format_aggregate_range_dependency<
		format_literal, field, argument_types...>()};

template <auto format_literal, replacement_field field,
		  typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_aggregate_range_dependencies() noexcept
{
	return static_format_aggregate_range_dependency<
			   format_literal, field, argument_types...>
		.dependencies;
}

template <auto format_literal, replacement_field field,
		  typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_aggregate_range_output_budget_exceeded() noexcept
{
	return static_format_aggregate_range_dependency<
			   format_literal, field, argument_types...>
		.output_budget_exceeded;
}

template <typename T>
struct static_brace_range_view_traits
{
	static inline constexpr bool value{};
};

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename source_type, typename argument_pack_type>
struct static_brace_range_view_traits<
	basic_brace_range_view<char_type, specification, source_type,
						   argument_pack_type>>
{
	static inline constexpr bool value{true};
};

template <::std::integral char_type>
struct static_format_count_output
{
	using output_char_type = char_type;
	::std::size_t size{};
};

template <::std::integral char_type>
struct static_format_count_output_ref
{
	using output_char_type = char_type;
	static_format_count_output<char_type> *output{};
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr static_format_count_output_ref<char_type>
output_stream_ref_define(static_format_count_output<char_type> &output) noexcept
{
	return {__builtin_addressof(output)};
}

template <::std::integral char_type>
inline constexpr void write_all_overflow_define(
	static_format_count_output_ref<char_type> output,
	char_type const *first, char_type const *last) noexcept
{
	auto const count{static_cast<::std::size_t>(last - first)};
	if (SIZE_MAX - output.output->size < count)
	{
		output.output->size = SIZE_MAX;
	}
	else
	{
		output.output->size += count;
	}
}

template <::std::integral char_type>
struct static_format_emit_output
{
	using output_char_type = char_type;
	char_type *current{};
};

template <::std::integral char_type>
struct static_format_emit_output_ref
{
	using output_char_type = char_type;
	static_format_emit_output<char_type> *output{};
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr static_format_emit_output_ref<char_type>
output_stream_ref_define(static_format_emit_output<char_type> &output) noexcept
{
	return {__builtin_addressof(output)};
}

template <::std::integral char_type>
inline constexpr void write_all_overflow_define(
	static_format_emit_output_ref<char_type> output,
	char_type const *first, char_type const *last) noexcept
{
	while (first != last)
	{
		*output.output->current++ = *first++;
	}
}

template <::std::integral char_type, typename range_view_type>
[[nodiscard]] inline consteval ::std::size_t
measure_static_brace_range_view(range_view_type &value)
{
	static_format_count_output<char_type> output{};
	auto output_reference{output_stream_ref_define(output)};
	::fast_io::print_define(
		::fast_io::io_reserve_type<char_type,
								   ::std::remove_cvref_t<range_view_type>>,
		output_reference, value);
	return output.size;
}

template <::std::integral char_type, typename range_view_type>
[[nodiscard]] inline consteval char_type *emit_static_brace_range_view(
	char_type *output, range_view_type &value)
{
	static_format_emit_output<char_type> stream{output};
	auto output_reference{output_stream_ref_define(stream)};
	::fast_io::print_define(
		::fast_io::io_reserve_type<char_type,
								   ::std::remove_cvref_t<range_view_type>>,
		output_reference, value);
	return stream.current;
}

template <::std::integral char_type>
struct measure_static_format_component
{
	template <typename value_type>
	[[nodiscard]] inline consteval ::std::size_t operator()(
		value_type &value) const
	{
		if constexpr (static_brace_range_view_traits<
						  ::std::remove_cvref_t<value_type>>::value)
		{
			return measure_static_brace_range_view<char_type>(value);
		}
		else if constexpr (::std::same_as<
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
		if constexpr (static_brace_range_view_traits<
						  ::std::remove_cvref_t<value_type>>::value)
		{
			return emit_static_brace_range_view<char_type>(output, value);
		}
		else if constexpr (::std::same_as<
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

/** Measures one NTTP-backed replacement without instantiating its byte storage. */
template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
struct static_replacement_evaluation
{
	using char_type = typename decltype(format_literal)::value_type;
	using static_output = static_output_replacement<
		format_literal, field, grammar_type, 0u, argument_types...>;
	static inline constexpr bool uses_static_output{
		static_output::available};
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

	template <::std::size_t... index>
	[[nodiscard]] inline static consteval ::std::size_t
		calculate_static_output_size(
			::std::index_sequence<index...>) noexcept
	{
		using descriptor = typename static_output::descriptor;
		using protocol = typename static_output::protocol;
		owned_argument_pack owned_arguments{};
		auto arguments{make_indexed_argument_pack(
			static_evaluation_argument_get<index>(owned_arguments)...)};
		auto &holder{
			indexed_argument_get<descriptor::resolution.index>(arguments)};
		decltype(auto) value{unwrap_static_named_argument(holder)};
		auto const width{resolve_format_parameter<
			format_literal, field.specification.width>(arguments)};
		auto const precision{resolve_format_parameter<
			format_literal, field.specification.precision>(arguments)};
		typename protocol::context_type context{
			{width.value, width.present, width.negative},
			{precision.value, precision.present, precision.negative}};
		return static_format_output_adl::size<
			typename protocol::context_type,
			typename protocol::formatter_type>(context, value);
	}

	template <::std::size_t... index>
	[[nodiscard]] inline static consteval char_type *emit_static_output(
		char_type *output, ::std::index_sequence<index...>) noexcept
	{
		using descriptor = typename static_output::descriptor;
		using protocol = typename static_output::protocol;
		owned_argument_pack owned_arguments{};
		auto arguments{make_indexed_argument_pack(
			static_evaluation_argument_get<index>(owned_arguments)...)};
		auto &holder{
			indexed_argument_get<descriptor::resolution.index>(arguments)};
		decltype(auto) value{unwrap_static_named_argument(holder)};
		auto const width{resolve_format_parameter<
			format_literal, field.specification.width>(arguments)};
		auto const precision{resolve_format_parameter<
			format_literal, field.specification.precision>(arguments)};
		typename protocol::context_type context{
			{width.value, width.present, width.negative},
			{precision.value, precision.present, precision.negative}};
		return static_format_output_adl::define<
			typename protocol::context_type,
			typename protocol::formatter_type>(context, output, value);
	}

	[[nodiscard]] inline static consteval ::std::size_t calculate_bound()
	{
		if constexpr (uses_static_output)
		{
			return calculate_static_output_size(
				::std::index_sequence_for<argument_types...>{});
		}
		else
		{
			return evaluate(measure_static_format_component<char_type>{},
							::std::index_sequence_for<argument_types...>{});
		}
	}

	[[nodiscard]] inline static consteval char_type *emit(
		char_type *output) noexcept
	{
		if constexpr (uses_static_output)
		{
			return emit_static_output(
				output,
				::std::index_sequence_for<argument_types...>{});
		}
		else
		{
			return evaluate(
				emit_static_format_component<char_type>{output},
				::std::index_sequence_for<argument_types...>{});
		}
	}

	static inline constexpr ::std::size_t bound{calculate_bound()};
};

template <typename evaluation_type, ::std::size_t bound>
[[nodiscard]] inline consteval bool
validate_automatic_static_replacement_emit() noexcept
{
	using char_type = typename evaluation_type::char_type;
	::std::array<char_type, bound == 0u ? 1u : bound> scratch{};
	auto *const begin{scratch.data()};
	auto *const end{evaluation_type::emit(begin)};
	return end >= begin && end <= begin + bound;
}

template <bool within_budget, typename evaluation_type,
		  ::std::size_t bound, typename = void>
struct automatic_static_replacement_emit_probe : ::std::false_type
{};

template <typename evaluation_type, ::std::size_t bound>
struct automatic_static_replacement_emit_probe<
	true, evaluation_type, bound,
	::std::void_t<::std::bool_constant<
		validate_automatic_static_replacement_emit<
			evaluation_type, bound>()>>>
	: ::std::bool_constant<
		  validate_automatic_static_replacement_emit<
			  evaluation_type, bound>()>
{};

template <bool selected, auto format_literal, replacement_field field,
		  typename grammar_type, typename void_type,
		  typename... argument_types>
struct automatic_static_replacement_probe_impl : ::std::false_type
{
	static inline constexpr bool bound_available{};
	static inline constexpr bool within_budget{};
};

template <auto format_literal, replacement_field field,
		  typename grammar_type, typename... argument_types>
struct automatic_static_replacement_probe_impl<
	true, format_literal, field, grammar_type,
	::std::void_t<::std::integral_constant<
		::std::size_t,
		static_replacement_evaluation<
			format_literal, field, grammar_type,
			argument_types...>::calculate_bound()>>,
	argument_types...>
{
	using evaluation_type = static_replacement_evaluation<
		format_literal, field, grammar_type, argument_types...>;
	static inline constexpr ::std::size_t bound{
		evaluation_type::calculate_bound()};
	static inline constexpr bool bound_available{true};
	static inline constexpr bool within_budget{
		bound != SIZE_MAX &&
		bound <= static_format_output_code_unit_limit};
	static inline constexpr bool value{
		automatic_static_replacement_emit_probe<
			within_budget,
			evaluation_type, bound>::value};
};

template <auto format_literal, replacement_field field,
		  typename static_output>
[[nodiscard]] inline consteval bool
automatic_static_replacement_rule_supported() noexcept
{
	using rule_type = typename static_output::rule_type;
	using value_reference = typename static_output::descriptor::value_reference;
	if constexpr (::std::same_as<
					  rule_type,
					  ::fast_io::fmt::details::brace_format_as_rule>)
	{
		return static_output::formatter_available;
	}
	else if constexpr (::std::same_as<
						   rule_type,
						   ::fast_io::fmt::details::brace_chrono_format_rule>)
	{
		using chrono_field_type = ::std::remove_cvref_t<decltype(make_chrono_field<
																 format_literal,
																 field.specification.type_directed_specification>(
			::std::declval<value_reference>()))>;
		using storage_type = typename chrono_field_type::value_type;
		return chrono_static_program_is_locale_free<
			format_literal,
			field.specification.type_directed_specification,
			chrono_has_utc_offset_v<storage_type>,
			chrono_has_time_zone_name_v<storage_type>>();
	}
	else if constexpr (::std::same_as<
						   rule_type,
						   ::fast_io::fmt::details::brace_direct_identity_format_rule>)
	{
		using clean_value_type = ::std::remove_cvref_t<value_reference>;
		return time_format_value<clean_value_type> ||
			   ::std::is_enum_v<clean_value_type>;
	}
	else
	{
		return false;
	}
}

template <bool rule_available, auto format_literal,
		  replacement_field field, typename static_output>
struct automatic_static_replacement_rule : ::std::false_type
{};

template <auto format_literal, replacement_field field,
		  typename static_output>
struct automatic_static_replacement_rule<
	true, format_literal, field, static_output>
	: ::std::bool_constant<
		  !static_output::available &&
		  automatic_static_replacement_rule_supported<
			  format_literal, field, static_output>()>
{};

template <bool rule_available, typename static_output,
		  typename expected_rule>
struct selected_static_replacement_rule : ::std::false_type
{};

template <typename static_output, typename expected_rule>
struct selected_static_replacement_rule<
	true, static_output, expected_rule>
	: ::std::bool_constant<::std::same_as<
		  typename static_output::rule_type, expected_rule>>
{};

template <bool rule_available, bool aggregate,
		  typename static_output>
struct intrinsic_static_replacement_rule : ::std::false_type
{};

template <bool aggregate, typename static_output>
struct intrinsic_static_replacement_rule<
	true, aggregate, static_output>
	: ::std::bool_constant<
		  aggregate
			  ? ::std::same_as<
					typename static_output::rule_type,
					::fast_io::fmt::details::brace_range_format_rule>
			  : (::std::same_as<
					 typename static_output::rule_type,
					 ::fast_io::fmt::details::brace_builtin_format_rule> ||
				 ::std::same_as<
					 typename static_output::rule_type,
					 ::fast_io::fmt::details::brace_direct_identity_format_rule> ||
				 ::std::same_as<
					 typename static_output::rule_type,
					 ::fast_io::fmt::details::brace_unclaimed_identity_format_rule>)>
{};

template <auto format_literal, replacement_field field,
		  typename grammar_type, typename... argument_types>
struct automatic_static_replacement_probe
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	using output_traits = static_output_replacement_traits<
		format_literal, field, clean_grammar_type, 0u, argument_types...>;
	using static_output = static_output_replacement<
		format_literal, field, clean_grammar_type, 0u, argument_types...>;
	static inline constexpr bool selected{
		automatic_static_replacement_rule<
			output_traits::rule_available, format_literal, field,
			static_output>::value};
	using implementation = automatic_static_replacement_probe_impl<
		selected, format_literal, field, clean_grammar_type, void,
		argument_types...>;
	static inline constexpr bool bound_available{
		implementation::bound_available};
	static inline constexpr bool within_budget{
		implementation::within_budget};
	static inline constexpr bool value{
		implementation::value};
};

template <auto format_literal, replacement_field field,
		  typename grammar_type, typename... argument_types>
[[nodiscard]] inline consteval bool
static_format_replacement_after_dependencies() noexcept
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	using output_traits = static_output_replacement_traits<
		format_literal, field, clean_grammar_type, 0u,
		argument_types...>;
	using static_output = static_output_replacement<
		format_literal, field, clean_grammar_type, 0u,
		argument_types...>;
	constexpr bool brace_grammar{::std::same_as<
		clean_grammar_type, ::fast_io::fmt::brace_fmt_t>};
	constexpr bool aggregate_reference{
		static_format_reference_is_aggregate<
			format_literal, field.argument, argument_types...>()};
	if constexpr (!brace_grammar)
	{
		// printf has no user-extensible replacement-rule protocol; preserve
		// its existing scalar/text static-reference path.
		return static_format_reference<
			   format_literal, field.argument, argument_types...>() ||
		   static_format_enum_reference<
			   format_literal, field.argument, argument_types...>();
	}
	else
	{
		constexpr bool intrinsic_rule{
			intrinsic_static_replacement_rule<
				output_traits::rule_available, aggregate_reference,
				static_output>::value};
		if constexpr (intrinsic_rule)
		{
			constexpr bool intrinsic_reference{
				static_format_reference<
					format_literal, field.argument,
					argument_types...>()};
			if constexpr (!intrinsic_reference)
			{
				// Native time also selects direct identity for an empty brace
				// specification, but it is intentionally outside the built-in
				// scalar/text proof.  Give the fail-closed automatic whitelist a
				// chance; arbitrary direct printables still resolve to false.
				return static_output::available ||
					   automatic_static_replacement_probe<
						   format_literal, field, clean_grammar_type,
						   argument_types...>::value;
			}
			else if constexpr (aggregate_reference)
			{
				// A fixed shape proves storage and compile-time budgets, but an
				// element ADL hook may still be non-constexpr.  Prove the actual
				// selected range measurement and emission before committing the
				// whole replacement to static storage.
				return automatic_static_replacement_probe_impl<
					true, format_literal, field, clean_grammar_type,
					void, argument_types...>::value;
			}
			else
			{
				return true;
			}
		}
		else
		{
			return static_output::available ||
				   automatic_static_replacement_probe<
					   format_literal, field, clean_grammar_type,
					   argument_types...>::value;
		}
	}
}

/**
 * Proves that one replacement depends exclusively on NTTP-backed data.
 *
 * Built-in scalar, text, and aggregate sources use the explicit dependency
 * proof above.  A selected format_as (and the locale-free native time rule)
 * may additionally opt in automatically only after the actual value
 * conversion, measurement, and emission all survive constant evaluation.
 * Arbitrary grammar and custom-formatter rules remain fail-closed unless they
 * provide the strict terminal static-output CPO.
 */
template <auto format_literal, replacement_field field, typename grammar_type,
		  typename... argument_types>
[[nodiscard]] inline consteval bool static_format_replacement() noexcept
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	if constexpr (!::std::same_as<
					  clean_grammar_type, ::fast_io::fmt::brace_fmt_t> &&
				  !::std::same_as<
					  clean_grammar_type, ::fast_io::fmt::printf_fmt_t>)
	{
		return false;
	}
	else if constexpr (!static_format_parameter<
						 format_literal, field.specification.width,
						 argument_types...>() ||
					 !static_format_parameter<
						 format_literal, field.specification.precision,
						 argument_types...>())
	{
		// Resolve dependencies before probing an emitter.  In particular, a
		// runtime width or precision must not instantiate a compile-time rule
		// with the placeholder used for dynamic argument-pack members.
		return false;
	}
	else
	{
		using output_traits = static_output_replacement_traits<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		using static_output = static_output_replacement<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		constexpr bool brace_grammar{::std::same_as<
			clean_grammar_type, ::fast_io::fmt::brace_fmt_t>};
		if constexpr (brace_grammar &&
				  selected_static_replacement_rule<
					  output_traits::rule_available, static_output,
					  ::fast_io::fmt::details::brace_range_format_rule>::value)
		{
			if constexpr (!static_format_aggregate_range_dependencies<
						  format_literal, field, argument_types...>())
			{
				return false;
			}
			else
			{
				return static_format_replacement_after_dependencies<
					format_literal, field, clean_grammar_type,
					argument_types...>();
			}
		}
		else
		{
			return static_format_replacement_after_dependencies<
				format_literal, field, clean_grammar_type,
				argument_types...>();
		}
	}
}

template <auto format_literal, replacement_field field,
		  typename grammar_type, typename... argument_types>
[[nodiscard]] inline consteval bool
automatic_static_replacement_output_budget_exceeded_after_dependencies() noexcept
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	if constexpr (!::std::same_as<
					  clean_grammar_type, ::fast_io::fmt::brace_fmt_t>)
	{
		return false;
	}
	else
	{
		using output_traits = static_output_replacement_traits<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		using static_output = static_output_replacement<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		constexpr bool aggregate_reference{
			static_format_reference_is_aggregate<
				format_literal, field.argument, argument_types...>()};
		constexpr bool intrinsic_rule{
			intrinsic_static_replacement_rule<
				output_traits::rule_available, aggregate_reference,
				static_output>::value};
		if constexpr (intrinsic_rule)
		{
			constexpr bool intrinsic_reference{
				static_format_reference<
					format_literal, field.argument,
					argument_types...>()};
			if constexpr (intrinsic_reference && aggregate_reference)
			{
				using probe = automatic_static_replacement_probe_impl<
					true, format_literal, field, clean_grammar_type,
					void, argument_types...>;
				return probe::bound_available && !probe::within_budget;
			}
			else if constexpr (!intrinsic_reference &&
							   !static_output::available)
			{
				using probe = automatic_static_replacement_probe<
					format_literal, field, clean_grammar_type,
					argument_types...>;
				return probe::bound_available && !probe::within_budget;
			}
			else
			{
				return false;
			}
		}
		else if constexpr (!static_output::available)
		{
			using probe = automatic_static_replacement_probe<
				format_literal, field, clean_grammar_type,
				argument_types...>;
			return probe::bound_available && !probe::within_budget;
		}
		else
		{
			return false;
		}
	}
}

template <auto format_literal, replacement_field field,
		  typename grammar_type, typename... argument_types>
[[nodiscard]] inline consteval bool
automatic_static_replacement_output_budget_exceeded() noexcept
{
	using clean_grammar_type = ::std::remove_cvref_t<grammar_type>;
	if constexpr (!::std::same_as<
					  clean_grammar_type, ::fast_io::fmt::brace_fmt_t> ||
				  !static_format_parameter<
					  format_literal, field.specification.width,
					  argument_types...>() ||
				  !static_format_parameter<
					  format_literal, field.specification.precision,
					  argument_types...>())
	{
		return false;
	}
	else
	{
		using output_traits = static_output_replacement_traits<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		using static_output = static_output_replacement<
			format_literal, field, clean_grammar_type, 0u,
			argument_types...>;
		if constexpr (selected_static_replacement_rule<
						  output_traits::rule_available, static_output,
						  ::fast_io::fmt::details::brace_range_format_rule>::value)
		{
			if constexpr (static_format_aggregate_range_output_budget_exceeded<
							  format_literal, field, argument_types...>())
			{
				return true;
			}
			else if constexpr (!static_format_aggregate_range_dependencies<
								   format_literal, field,
								   argument_types...>())
			{
				return false;
			}
			else
			{
				return automatic_static_replacement_output_budget_exceeded_after_dependencies<
					format_literal, field, clean_grammar_type,
					argument_types...>();
			}
		}
		else
		{
			return automatic_static_replacement_output_budget_exceeded_after_dependencies<
				format_literal, field, clean_grammar_type,
				argument_types...>();
		}
	}
}

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index, typename... argument_types>
[[nodiscard]] inline consteval bool
automatic_static_format_operation_output_budget_exceeded() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind != format_operation_kind::replacement)
	{
		return false;
	}
	else
	{
		constexpr auto field{program.fields[operation.payload_index]};
		return automatic_static_replacement_output_budget_exceeded<
			format_literal, field, grammar_tag, argument_types...>();
	}
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types, ::std::size_t... operation_index>
[[nodiscard]] inline consteval bool
	automatic_static_format_output_budget_exceeded_impl(
		::std::index_sequence<operation_index...>) noexcept
{
	return (automatic_static_format_operation_output_budget_exceeded<
				format_literal, grammar_tag, operation_index,
				argument_types...>() ||
			...);
}

template <auto format_literal, typename grammar_tag,
		  typename... argument_types>
[[nodiscard]] inline consteval bool
automatic_static_format_output_budget_exceeded() noexcept
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_program<format_literal,
												 grammar_tag>
			.operation_count};
	return automatic_static_format_output_budget_exceeded_impl<
		format_literal, grammar_tag, argument_types...>(
		::std::make_index_sequence<operation_count>{});
}

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
					  bound > static_format_output_code_unit_limit)
		{
			return 0u;
		}
		else if constexpr (evaluation_type::uses_static_output)
		{
			// The terminal CPO's size is exact by contract.  Its single output
			// invocation in make_storage verifies the returned end pointer.
			return bound;
		}
		else if constexpr (bound == 0u)
		{
			return 0u;
		}
		else
		{
			::std::array<char_type, bound> scratch{};
			auto const end{evaluation_type::emit(scratch.data())};
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
			if constexpr (evaluation_type::uses_static_output)
			{
				// Keep a real one-element object for an empty output so the CPO
				// receives and returns a valid pointer without null + 0 arithmetic.
				::std::array<char_type, bound == 0u ? 1u : bound> scratch{};
				auto const end{evaluation_type::emit(scratch.data())};
				if (end != scratch.data() + bound)
				{
					::fast_io::fast_terminate();
				}
				for (::std::size_t index{}; index != size; ++index)
				{
					result[index] = scratch[index];
				}
			}
			else if constexpr (size != 0u)
			{
				::std::array<char_type, bound> scratch{};
				auto const end{evaluation_type::emit(scratch.data())};
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

template <auto format_literal, typename grammar_tag,
		  ::std::size_t operation_index,
		  bool trim_terminal_literal_line_feed = false,
		  ::std::size_t... index, typename... argument_types>
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 15
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr decltype(auto) make_format_operation(
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments)
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<format_literal, grammar_tag>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.kind == format_operation_kind::literal)
	{
		return make_literal_operation<
			format_literal, grammar_tag, operation_index,
			trim_terminal_literal_line_feed>();
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

template <auto format_literal, typename grammar_tag,
		  bool trim_terminal_literal_line_feed = false,
		  typename callback_type, typename argument_pack,
		  ::std::size_t... operation_index>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) lower_format_program_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<operation_index...>)
{
	static_assert(!trim_terminal_literal_line_feed ||
					  format_program_has_terminal_literal_line_feed<
						  format_literal, grammar_tag>(),
				  "fast_io format: terminal LF trimming requires a compiled-program proof");
	// This is the only type expansion in the emitter.  There is no recursive AST walk,
	// token-kind switch, parser cursor, or type-erased argument visit in generated code.
	return ::std::forward<callback_type>(callback)(
		make_format_operation<
			format_literal, grammar_tag, operation_index,
			(trim_terminal_literal_line_feed &&
			 operation_index + 1u == sizeof...(operation_index))>(arguments)...);
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
												 grammar_tag>
			.operation_count};
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
												 grammar_tag>
			.operation_count};
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
												 grammar_tag>
			.operation_count};
	return static_format_output_bound_impl<
		format_literal, grammar_tag, argument_types...>(
		::std::make_index_sequence<operation_count>{});
}

template <::std::integral char_type>
struct measure_static_format_components
{
	template <typename... component_types>
	[[nodiscard]] inline constexpr ::std::size_t operator()(
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
	[[nodiscard]] inline constexpr char_type *operator()(
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
			return ::fast_io::containers::array<char_type, 0u>{};
		}
		else
		{
			// The final spelling owns its code units in fast_io's DSAL array.  In
			// particular, no pointer into a consteval scratch object escapes into
			// the print layer: `storage` itself is the immutable provider object.
			::fast_io::containers::array<char_type, size> result{};
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
		  ::std::size_t group_index,
		  bool trim_terminal_literal_line_feed = false,
		  ::std::size_t... argument_index,
		  typename... argument_types>
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 15
[[__gnu__::__always_inline__]]
#endif
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
		static_assert(!trim_terminal_literal_line_feed ||
						  (run_type::size != 0u &&
						   run_type::storage[run_type::size - 1u] ==
							   ::fast_io::char_literal_v<
								   u8'\n', typename run_type::char_type>),
					  "fast_io format: only a proved static-run terminal LF may be trimmed");
		constexpr ::std::size_t exposed_size{
			run_type::size -
			static_cast<::std::size_t>(trim_terminal_literal_line_feed)};
		if constexpr (exposed_size == 0u)
		{
			// Preserve the explicit static-field event while exposing no bytes.
			return ::fast_io::io_null;
		}
		else
		{
			return ::fast_io::manipulators::static_scatter_t<
				typename run_type::char_type, exposed_size>{
				run_type::storage.data()};
		}
	}
	else
	{
		static_assert(!trim_terminal_literal_line_feed,
					  "fast_io format: a trimmed terminal group must be static");
		static_assert(run_count == 1u);
		return make_format_operation<
			format_literal, grammar_tag, run_begin>(arguments);
	}
}

template <auto format_literal, typename grammar_tag,
		  bool trim_terminal_literal_line_feed = false,
		  typename callback_type, typename argument_pack,
		  ::std::size_t... group_index>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) lower_format_program_grouped_impl(
	callback_type &&callback, argument_pack &arguments,
	::std::index_sequence<group_index...>)
{
	static_assert(!trim_terminal_literal_line_feed ||
					  format_program_has_terminal_literal_line_feed<
						  format_literal, grammar_tag>(),
				  "fast_io format: terminal LF trimming requires a compiled-program proof");
	return ::std::forward<callback_type>(callback)(
		make_format_group<
			format_literal, grammar_tag, group_index,
			(trim_terminal_literal_line_feed &&
			 group_index + 1u == sizeof...(group_index))>(arguments)...);
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
												 grammar_tag>
			.operation_count};

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
			return ::fast_io::containers::array<char_type, 0u>{};
		}
		else
		{
			// This inline variable is the provider whose address reaches a
			// synchronous unbuffered write.  Own the final code units in a DSAL
			// array rather than borrowing consteval scratch storage.
			::fast_io::containers::array<char_type, size> result{};
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
		  bool trim_terminal_literal_line_feed,
		  typename callback_type, typename... argument_types>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) lower_format_program_with_trim_policy(
	callback_type &&callback, argument_types &...arguments)
{
	if constexpr (::fast_io::fmt::details::format_argument_list_validation_adl::
					  expression<format_literal, grammar_tag,
								 argument_types...>)
	{
		::fast_io::fmt::details::format_argument_list_validation_adl::invoke<
			format_literal, grammar_tag, argument_types...>();
	}
	static_assert(!trim_terminal_literal_line_feed ||
					  format_program_has_terminal_literal_line_feed<
						  format_literal, grammar_tag>(),
				  "fast_io format: terminal LF trimming requires a compiled-program proof");
	using literal_program = ::fast_io::fmt::details::compiled_literal_program<
		format_literal, grammar_tag>;
	if constexpr (literal_program::literal_only)
	{
		static_assert(!trim_terminal_literal_line_feed ||
						  (literal_program::size != 0u &&
						   literal_program::storage[literal_program::size - 1u] ==
							   ::fast_io::char_literal_v<
								   u8'\n', typename literal_program::char_type>),
					  "fast_io format: only a proved literal-program terminal LF may be trimmed");
		constexpr ::std::size_t exposed_size{
			literal_program::size -
			static_cast<::std::size_t>(trim_terminal_literal_line_feed)};
		if constexpr (exposed_size == 0u)
		{
			return ::std::forward<callback_type>(callback)();
		}
		else if constexpr (exposed_size < 64u)
		{
			return ::std::forward<callback_type>(callback)(
				::fast_io::manipulators::static_scatter_t<
					typename literal_program::char_type, exposed_size>{
					literal_program::storage.data()});
		}
		else
		{
			return ::std::forward<callback_type>(callback)(
				::fast_io::basic_io_scatter_t<typename literal_program::char_type>{
					literal_program::storage.data(), exposed_size});
		}
	}
	else if constexpr (!(::fast_io::fmt::
							 is_static_format_argument_holder_v<argument_types> ||
						 ...))
	{
		// Preserve the original dynamic front-end instantiation graph exactly.
		// In particular, a translation unit which never names static_arg must not
		// instantiate static dependency resolution, grouping, or materialization.
		auto indexed_arguments{make_indexed_argument_pack(arguments...)};
		constexpr auto operation_count{
			::fast_io::fmt::details::checked_program<format_literal,
													 grammar_tag>
				.operation_count};
		return lower_format_program_impl<
			format_literal, grammar_tag,
			trim_terminal_literal_line_feed>(
			::std::forward<callback_type>(callback), indexed_arguments,
			::std::make_index_sequence<operation_count>{});
	}
	else if constexpr (automatic_static_format_output_budget_exceeded<
						   format_literal, grammar_tag,
						   argument_types...>())
	{
		static_assert(!automatic_static_format_output_budget_exceeded<
						  format_literal, grammar_tag,
						  argument_types...>(),
					  "fast_io format: an automatically static replacement has no finite output within the compile-time budget");
		return ::std::forward<callback_type>(callback)();
	}
	else if constexpr (
		static_format_groups<format_literal, grammar_tag,
							 argument_types...>
			.has_static_replacement &&
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
		static_assert(!trim_terminal_literal_line_feed ||
						  (static_program::size != 0u &&
						   static_program::storage[static_program::size - 1u] ==
							   ::fast_io::char_literal_v<
								   u8'\n', typename static_program::char_type>),
					  "fast_io format: only a proved static-program terminal LF may be trimmed");
		constexpr ::std::size_t exposed_size{
			static_program::size -
			static_cast<::std::size_t>(trim_terminal_literal_line_feed)};
		if constexpr (exposed_size == 0u)
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
					typename static_program::char_type, exposed_size>{
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
				format_literal, grammar_tag,
				trim_terminal_literal_line_feed>(
				::std::forward<callback_type>(callback), indexed_arguments,
				::std::make_index_sequence<group_plan.group_count>{});
		}
		else
		{
			return lower_format_program_impl<
				format_literal, grammar_tag,
				trim_terminal_literal_line_feed>(
				::std::forward<callback_type>(callback), indexed_arguments,
				::std::make_index_sequence<operation_count>{});
		}
	}
}

/** Preserves the original public lowering entry and its explicit template-argument order. */
template <auto format_literal, typename grammar_tag, typename callback_type,
		  typename... argument_types>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) lower_format_program(
	callback_type &&callback, argument_types &...arguments)
{
	return ::fast_io::fmt::details::lower_format_program_with_trim_policy<
		format_literal, grammar_tag, false>(
		::std::forward<callback_type>(callback), arguments...);
}

/** Lowers a proved terminal literal LF as concat's separate line flag. */
template <auto format_literal, typename grammar_tag, typename callback_type,
		  typename... argument_types>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) lower_format_program_trim_terminal_line_feed(
	callback_type &&callback, argument_types &...arguments)
{
	return ::fast_io::fmt::details::lower_format_program_with_trim_policy<
		format_literal, grammar_tag, true>(
		::std::forward<callback_type>(callback), arguments...);
}

} // namespace fast_io::fmt::details
