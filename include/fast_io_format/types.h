#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "details/fixed_string.h"

namespace fast_io::fmt
{

/// Internal grammar identity used by the compile-time parser and lowering layer.
///
/// This is a type, rather than an enum value shared with the printf grammar, so overload
/// resolution and ADL can select a parser without instantiating the other grammar.  Keeping
/// the grammars in separate overload sets is also important for compile-time diagnostics:
/// a percent sign is ordinary text in this grammar and must never be tentatively parsed by
/// the printf implementation.
struct brace_fmt_t
{
	using format_grammar_tag = void;
	explicit constexpr brace_fmt_t() noexcept = default;
};

/// Internal identity for the type-safe percent grammar.
struct printf_fmt_t
{
	using format_grammar_tag = void;
	explicit constexpr printf_fmt_t() noexcept = default;
};

/// Recognizes a stateless grammar rule without naming any concrete syntax.
///
/// This marker is only the inexpensive overload-selection gate.  The stronger
/// `compilable_format_grammar<literal, T>` concept (declared by compile.h) also
/// requires an ADL `compile_format_program<literal>(T)` CPO.  Separating those
/// checks keeps arbitrary values out of rule kernels while making the grammar
/// set open: `print_with_rule` and `concat_with_rule` do not contain a closed
/// brace-versus-percent type test.
///
/// The object is a type token, not runtime policy storage.  Parsing and lowering
/// intentionally reconstruct `T{}` inside immediate/dependent calls, so accepting
/// a stateful instance would silently discard observable state.  The empty,
/// nothrow, and trivial requirements make that ABI fact part of the concept
/// instead of relying on a convention that generic code cannot verify.
template <typename T>
concept format_grammar =
	requires { typename ::std::remove_cvref_t<T>::format_grammar_tag; } &&
	::std::default_initializable<::std::remove_cvref_t<T>> &&
	::std::is_empty_v<::std::remove_cvref_t<T>> &&
	::std::is_nothrow_default_constructible_v<::std::remove_cvref_t<T>> &&
	::std::is_trivially_copyable_v<::std::remove_cvref_t<T>> &&
	::std::is_trivially_destructible_v<::std::remove_cvref_t<T>>;

/// Owns an rvalue named argument and borrows an lvalue named argument.
///
/// The name is structural compile-time data, so name lookup cannot become the linear runtime
/// string search used by fmt's runtime named arguments. The value storage follows the usual
/// forwarding lifetime rule: an rvalue is owned and an lvalue remains borrowed. Lowering then
/// exposes the stable named member as an lvalue, exactly as an ordinary named function parameter
/// reaches fast_io::io::print; final alias and ABI transport remain backend decisions.
template <basic_fixed_string name_literal, typename storage_type>
struct static_named_arg
{
	using value_type = storage_type;
	static inline constexpr auto name{name_literal};

	storage_type value;
};

/**
 * Carries a formatting value entirely in its type.
 *
 * An ordinary function argument such as `42u` remains a run-time argument in
 * the C++ abstract machine even when the call expression spells a literal.
 * This carrier is the explicit proof that the value may participate in format
 * lowering as an NTTP.  It has no run-time state; lowering reads
 * `stored_value` only while producing format-owned constant storage.
 */
template <typename T>
struct is_basic_fixed_string : ::std::false_type
{};

template <format_character char_type, ::std::size_t extent>
struct is_basic_fixed_string<basic_fixed_string<char_type, extent>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_format_character_pointer_v{
	::std::is_pointer_v<T> &&
	format_character<::std::remove_cv_t<::std::remove_pointer_t<T>>>};

template <typename value_type>
inline constexpr void copy_static_c_array_element(
	value_type &destination, value_type const &source) noexcept
{
	if constexpr (::std::is_array_v<value_type>)
	{
		for (::std::size_t index{};
			 index != ::std::extent_v<value_type>; ++index)
		{
			copy_static_c_array_element(destination[index], source[index]);
		}
	}
	else
	{
		destination = source;
	}
}

/** Structural by-value copy used to prove that a C array is constant-readable. */
template <typename element_type, ::std::size_t extent>
struct basic_static_c_array_value
{
	element_type elements[extent]{};

	consteval basic_static_c_array_value(
		element_type const (&source)[extent]) noexcept
	{
		for (::std::size_t index{}; index != extent; ++index)
		{
			copy_static_c_array_element(elements[index], source[index]);
		}
	}
};

template <typename element_type, ::std::size_t extent>
basic_static_c_array_value(element_type const (&)[extent])
	-> basic_static_c_array_value<element_type, extent>;

template <typename T>
struct is_basic_static_c_array_value : ::std::false_type
{};

template <typename element_type, ::std::size_t extent>
struct is_basic_static_c_array_value<
	basic_static_c_array_value<element_type, extent>> : ::std::true_type
{};

/**
 * Structural wrapper used by the `static_arg<...>` variable template.
 *
 * Keeping the selected value as a public subobject is more than API plumbing:
 * older Clang releases accept a deduced structural class NTTP containing a
 * floating member even where spelling that floating value again as a direct
 * `auto` NTTP is rejected.  Every holder below therefore remains parameterized
 * by this wrapper and reads its member; it never re-injects the member as a
 * second non-type template argument.
 */
template <typename value_type>
struct static_argument_constant
{
	value_type value;

	// Take an already deduced scalar/structural value by value.  Clang 17
	// rejects a reference-taking converting constructor here because the
	// converted constant expression used for a class NTTP would bind that
	// reference to the literal's temporary.  This object exists only during
	// constant evaluation, so copying is both semantically exact and free at
	// run time.
	consteval static_argument_constant(value_type source) noexcept
		: value(source)
	{}

	// A character array needs its own exact-match constructor: routing it
	// through the by-value overload would require a second user-defined
	// conversion to basic_fixed_string.  The fixed string owns every code unit,
	// including the terminator, and therefore remains a structural NTTP value.
	template <format_character char_type, ::std::size_t extent>
		requires ::std::same_as<
			value_type, basic_fixed_string<char_type, extent>>
	consteval static_argument_constant(
		char_type const (&source)[extent]) noexcept
		: value(source)
	{}
};

template <typename value_type>
static_argument_constant(value_type const &)
	-> static_argument_constant<value_type>;

template <format_character char_type, ::std::size_t extent>
static_argument_constant(char_type const (&)[extent])
	-> static_argument_constant<basic_fixed_string<char_type, extent>>;

template <static_argument_constant value_literal>
struct static_format_arg
{
	static inline constexpr auto stored_value{value_literal.value};

	[[nodiscard]] inline static constexpr decltype(auto) get() noexcept
	{
		if constexpr (is_basic_fixed_string<
						  ::std::remove_cv_t<decltype(stored_value)>>::value)
		{
			// Preserve array extent so the ordinary string field rule, including
			// bounded null discovery and code-unit checks, remains authoritative.
			return (stored_value.elements);
		}
		else if constexpr (is_basic_static_c_array_value<
							   ::std::remove_cv_t<decltype(stored_value)>>::value)
		{
			return (stored_value.elements);
		}
		else
		{
			return (stored_value);
		}
	}
};

template <::std::size_t index, auto value_literal>
struct static_tuple_value_slot
{
	static inline constexpr auto stored_value{value_literal};
};

template <typename index_sequence, auto... value_literals>
struct static_tuple_value_impl;

template <::std::size_t... index, auto... value_literals>
struct static_tuple_value_impl<::std::index_sequence<index...>,
							   value_literals...> : static_tuple_value_slot<index, value_literals>...
{};

template <::std::size_t index, auto value_literal>
[[nodiscard]] inline constexpr decltype(auto) static_tuple_value_slot_get(
	static_tuple_value_slot<index, value_literal> const &) noexcept
{
	if constexpr (is_basic_fixed_string<
					  ::std::remove_cv_t<decltype(static_tuple_value_slot<index,
																		  value_literal>::stored_value)>>::value)
	{
		return (static_tuple_value_slot<index,
										value_literal>::stored_value.elements);
	}
	else
	{
		return (static_tuple_value_slot<index,
										value_literal>::stored_value);
	}
}

/** A tuple-like view whose heterogeneous elements are individual NTTPs. */
template <auto... value_literals>
struct static_tuple_value
	: static_tuple_value_impl<
		  ::std::index_sequence_for<decltype(value_literals)...>,
		  value_literals...>
{};

template <::std::size_t index, auto... value_literals>
	requires(index < sizeof...(value_literals))
[[nodiscard]] inline constexpr decltype(auto) get(
	static_tuple_value<value_literals...> const &value) noexcept
{
	return ::fast_io::fmt::static_tuple_value_slot_get<index>(value);
}

template <::std::size_t index, auto... value_literals>
	requires(index < sizeof...(value_literals))
[[nodiscard]] inline constexpr decltype(auto) get(
	static_tuple_value<value_literals...> &value) noexcept
{
	return ::fast_io::fmt::get<index>(
		static_cast<static_tuple_value<value_literals...> const &>(value));
}

/** Carries a heterogeneous tuple as an NTTP pack without run-time state. */
template <auto... value_literals>
struct static_tuple_format_arg
{
	static inline constexpr static_tuple_value<value_literals...> stored_value{};

	[[nodiscard]] inline static constexpr auto const &get() noexcept
	{
		return stored_value;
	}
};

template <typename T>
struct is_static_format_arg : ::std::false_type
{};

template <static_argument_constant value_literal>
struct is_static_format_arg<static_format_arg<value_literal>> : ::std::true_type
{};

template <auto... value_literals>
struct is_static_format_arg<static_tuple_format_arg<value_literals...>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_format_arg_v{
	is_static_format_arg<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_static_format_argument_holder : is_static_format_arg<::std::remove_cvref_t<T>>
{};

template <basic_fixed_string name_literal, typename storage_type>
struct is_static_format_argument_holder<static_named_arg<name_literal, storage_type>>
	: is_static_format_arg<::std::remove_cvref_t<storage_type>>
{};

template <typename T>
inline constexpr bool is_static_format_argument_holder_v{
	is_static_format_argument_holder<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_static_named_arg : ::std::false_type
{};

template <basic_fixed_string name_literal, typename storage_type>
struct is_static_named_arg<static_named_arg<name_literal, storage_type>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_named_arg_v{
	is_static_named_arg<::std::remove_cvref_t<T>>::value};

/// Creates a named argument whose name participates in constant evaluation.
template <basic_fixed_string name_literal, typename T>
[[nodiscard]] inline constexpr auto arg(T &&value)
{
	using storage_type = ::std::conditional_t<
		::std::is_lvalue_reference_v<T &&>, T &&, ::std::remove_cvref_t<T>>;
	return static_named_arg<name_literal, storage_type>{::std::forward<T>(value)};
}

namespace details
{

template <static_argument_constant value_literal>
	requires (!is_format_character_pointer_v<
		::std::remove_cv_t<decltype(value_literal.value)>>)
[[nodiscard]] inline consteval auto make_static_argument() noexcept
{
	return static_format_arg<value_literal>{};
}

template <static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires (is_basic_fixed_string<
		::std::remove_cv_t<decltype(name_literal.value)>>::value &&
		!is_format_character_pointer_v<
			::std::remove_cv_t<decltype(value_literal.value)>>)
[[nodiscard]] inline consteval auto make_static_argument() noexcept
{
	return static_named_arg<
		name_literal.value, static_format_arg<value_literal>>{};
}

} // namespace details

/**
 * An NTTP-backed format argument with no run-time value member.
 *
 * This is intentionally a variable template, so the proof-bearing spelling is
 * `static_arg<42>` (or `static_arg<"name", 42>`).  The resulting object has no
 * call operator: both `static_arg<42>()` and calls with run-time arguments are
 * rejected instead of silently resembling the old factory-function API.
 */
template <static_argument_constant... value_literals>
	requires requires {
		::fast_io::fmt::details::make_static_argument<value_literals...>();
	}
inline constexpr auto static_arg{
	::fast_io::fmt::details::make_static_argument<value_literals...>()};

/** Creates an NTTP-reference carrier for a fixed non-character C array. */
template <auto &array_literal,
		  auto copied_literal = basic_static_c_array_value{array_literal}>
	requires(::std::is_array_v<
				 ::std::remove_reference_t<decltype(array_literal)>> &&
			 ::std::is_const_v<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>> &&
			 !format_character<::std::remove_cv_t<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>>>)
[[nodiscard]] inline consteval auto static_array_arg() noexcept
{
	return static_format_arg<copied_literal>{};
}

/** Creates a named NTTP-reference carrier for a fixed non-character C array. */
template <basic_fixed_string name_literal, auto &array_literal,
		  auto copied_literal = basic_static_c_array_value{array_literal}>
	requires(::std::is_array_v<
				 ::std::remove_reference_t<decltype(array_literal)>> &&
			 ::std::is_const_v<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>> &&
			 !format_character<::std::remove_cv_t<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>>>)
[[nodiscard]] inline consteval auto static_named_array_arg() noexcept
{
	return static_named_arg<
		name_literal, static_format_arg<copied_literal>>{};
}

/** Creates a tuple-like argument whose elements are heterogeneous NTTPs. */
template <auto... value_literals>
[[nodiscard]] inline consteval auto static_tuple_arg() noexcept
{
	return static_tuple_format_arg<value_literals...>{};
}

/** Creates a named tuple-like argument from a heterogeneous NTTP pack. */
template <basic_fixed_string name_literal, auto... value_literals>
[[nodiscard]] inline consteval auto static_named_tuple_arg() noexcept
{
	return static_named_arg<
		name_literal, static_tuple_format_arg<value_literals...>>{};
}

} // namespace fast_io::fmt

namespace std
{

template <auto... value_literals>
struct tuple_size<::fast_io::fmt::static_tuple_value<value_literals...>>
	: ::std::integral_constant<::std::size_t, sizeof...(value_literals)>
{};

template <::std::size_t index, auto... value_literals>
struct tuple_element<
	index, ::fast_io::fmt::static_tuple_value<value_literals...>>
{
	using type = ::std::remove_reference_t<decltype(::fast_io::fmt::get<index>(::std::declval<
																			   ::fast_io::fmt::static_tuple_value<value_literals...> const &>()))>;
};

} // namespace std
