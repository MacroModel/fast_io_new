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

} // namespace fast_io::fmt
