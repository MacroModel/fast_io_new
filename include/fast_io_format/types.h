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

template <auto value_literal>
struct static_format_arg
{
	static inline constexpr auto stored_value{value_literal};

	[[nodiscard]] inline static constexpr decltype(auto) get() noexcept
	{
		if constexpr (is_basic_fixed_string<
					  ::std::remove_cv_t<decltype(stored_value)>>::value)
		{
			// Preserve array extent so the ordinary string field rule, including
			// bounded null discovery and code-unit checks, remains authoritative.
			return (stored_value.elements);
		}
		else
		{
			return (stored_value);
		}
	}
};

template <typename T>
struct is_static_format_arg : ::std::false_type
{};

template <auto value_literal>
struct is_static_format_arg<static_format_arg<value_literal>> : ::std::true_type
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

/** Creates an unnamed NTTP-backed format argument. */
template <auto value_literal>
	requires (!is_basic_fixed_string<
		::std::remove_cv_t<decltype(value_literal)>>::value &&
		!is_format_character_pointer_v<decltype(value_literal)>)
[[nodiscard]] inline consteval auto static_arg() noexcept
{
	return static_format_arg<value_literal>{};
}

/** Creates an unnamed fixed-string NTTP-backed format argument. */
template <basic_fixed_string value_literal>
[[nodiscard]] inline consteval auto static_arg() noexcept
{
	return static_format_arg<value_literal>{};
}

/** Creates a named NTTP-backed format argument without a run-time member value. */
template <basic_fixed_string name_literal, auto value_literal>
	requires (!is_basic_fixed_string<
		::std::remove_cv_t<decltype(value_literal)>>::value &&
		!is_format_character_pointer_v<decltype(value_literal)>)
[[nodiscard]] inline consteval auto static_arg() noexcept
{
	return static_named_arg<name_literal, static_format_arg<value_literal>>{};
}

/** Creates a named fixed-string NTTP-backed format argument. */
template <basic_fixed_string name_literal, basic_fixed_string value_literal>
[[nodiscard]] inline consteval auto static_arg() noexcept
{
	return static_named_arg<name_literal, static_format_arg<value_literal>>{};
}

} // namespace fast_io::fmt
