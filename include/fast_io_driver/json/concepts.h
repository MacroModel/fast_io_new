#pragma once

#include <concepts>
#include <type_traits>

#include "../../fast_io_core.h"

namespace fast_io
{

/**
 * Classifies the mathematical JSON number category represented by a C++
 * type.  The enum is deliberately independent of the DOM representation so
 * that parsers, serializers, and user-defined arbitrary precision types use
 * the same vocabulary.
 */
enum class json_number_kind : unsigned char
{
	floating_point,
	signed_integer,
	unsigned_integer
};

/** Tag passed to the ADL-only json_number_kind_define customization point. */
struct json_number_kind_tag_t
{
	explicit constexpr json_number_kind_tag_t() noexcept = default;
};

inline constexpr json_number_kind_tag_t json_number_kind_tag{};

/**
 * Tag for the optional custom-number replay-safety proof used by composite
 * reserve protocols.
 */
struct json_number_print_replay_safe_tag_t
{
	explicit constexpr json_number_print_replay_safe_tag_t() noexcept = default;
};

inline constexpr json_number_print_replay_safe_tag_t json_number_print_replay_safe_tag{};

namespace details
{

template <typename T>
using json_number_remove_cvref_t = ::std::remove_cvref_t<T>;

template <typename T>
concept adl_json_number_kind = requires {
	{
		json_number_kind_define(json_number_kind_tag,
								::std::type_identity<json_number_remove_cvref_t<T>>{})
	} noexcept -> ::std::same_as<json_number_kind>;
	requires requires {
		typename ::std::integral_constant<
			json_number_kind,
			json_number_kind_define(json_number_kind_tag,
									::std::type_identity<json_number_remove_cvref_t<T>>{})>;
	};
};

template <typename T>
	requires adl_json_number_kind<T>
inline constexpr json_number_kind adl_json_number_kind_v =
	json_number_kind_define(json_number_kind_tag,
							::std::type_identity<json_number_remove_cvref_t<T>>{});

template <typename T>
concept adl_json_number_is_finite = requires(json_number_remove_cvref_t<T> const &value) {
	{ json_number_is_finite_define(value) } noexcept -> ::std::same_as<bool>;
};

template <typename T>
concept adl_json_number_print_replay_safe = requires {
	{
		json_number_print_replay_safe_define(
			json_number_print_replay_safe_tag,
			::std::type_identity<json_number_remove_cvref_t<T>>{})
	} noexcept -> ::std::same_as<::std::true_type>;
};

} // namespace details

/**
 * A custom JSON numeric type opts in only through an exact ADL declaration:
 *
 * consteval fast_io::json_number_kind json_number_kind_define(
 *     fast_io::json_number_kind_tag_t, std::type_identity<T>) noexcept;
 *
 * Requiring the exact enum return type, noexcept, and a constant expression
 * prevents unrelated conversion functions from accidentally classifying a
 * type as a JSON number.
 */
template <typename T>
concept custom_json_number = details::adl_json_number_kind<T>;

template <typename T>
concept json_floating_point =
	::fast_io::details::my_floating_point<details::json_number_remove_cvref_t<T>> ||
	(custom_json_number<T> &&
	 details::adl_json_number_kind_v<T> == json_number_kind::floating_point);

template <typename T>
concept json_signed_integer =
	(!::std::same_as<details::json_number_remove_cvref_t<T>, bool>) &&
	(::fast_io::details::my_signed_integral<details::json_number_remove_cvref_t<T>> ||
	 (custom_json_number<T> &&
	  details::adl_json_number_kind_v<T> == json_number_kind::signed_integer));

template <typename T>
concept json_unsigned_integer =
	(!::std::same_as<details::json_number_remove_cvref_t<T>, bool>) &&
	(::fast_io::details::my_unsigned_integral<details::json_number_remove_cvref_t<T>> ||
	 (custom_json_number<T> &&
	  details::adl_json_number_kind_v<T> == json_number_kind::unsigned_integer));

/**
 * Custom floating-point types must provide
 *
 *     bool json_number_is_finite_define(T const &) noexcept;
 *
 * by ADL.  Built-in and compiler floating types are handled by fast_io's
 * native floating implementation and therefore need no marker function.
 */
template <typename T>
concept json_finite_number =
	::fast_io::details::my_floating_point<details::json_number_remove_cvref_t<T>> ||
	(json_floating_point<T> && custom_json_number<T> &&
	 details::adl_json_number_is_finite<T>);

/**
 * Proves that a number may participate in a composite measure-then-emit
 * protocol. Built-in integer and floating formatters already have fast_io's
 * stable multi-pass contract. A custom number opts in with
 *
 *     std::true_type json_number_print_replay_safe_define(
 *         fast_io::json_number_print_replay_safe_tag_t,
 *         std::type_identity<T>) noexcept;
 *
 * The proof covers the complete normalization and formatting chain: for one
 * unchanged const value, repeated finiteness/validation queries, alias/status
 * normalization, reserve queries, and emission must denote the same JSON
 * number token, must not consume the source, and must have no externally
 * visible side effects. Merely being printable is insufficient because a
 * custom CPO may legally carry state. Without this marker a DOM containing T remains fully
 * one-pass printable/context-printable, but does not advertise dynamic or
 * precise reserve materialization for the whole document.
 */
template <typename T>
concept json_number_print_replay_safe =
	::fast_io::details::my_integral<details::json_number_remove_cvref_t<T>> ||
	::fast_io::details::my_floating_point<details::json_number_remove_cvref_t<T>> ||
	(custom_json_number<T> && details::adl_json_number_print_replay_safe<T>);

static_assert(!json_signed_integer<bool>);
static_assert(!json_unsigned_integer<bool>);

} // namespace fast_io
