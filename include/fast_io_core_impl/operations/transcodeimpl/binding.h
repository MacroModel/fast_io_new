#pragma once

/**
 * @file
 * @brief Defines public stream-character to engine-endpoint binding.
 *
 * Equal sizeof is intentionally not a binding rule. A public character domain
 * either matches the engine unit exactly, or the engine explicitly consumes or
 * produces the object representation as std::byte.
 */

namespace fast_io
{

/** @brief Selects direct typed access or explicit byte-object bridging. */
enum class transcode_unit_binding : ::std::uint_least8_t
{
	// Public characters and engine units are the same semantic C++ type.
	exact_units,
	// The engine endpoint is std::byte and observes object representations.
	object_bytes
};

/** @brief Detects endpoints that can share one typed code-unit representation. */
template <typename engine_unit, typename public_char>
concept transcode_exact_units_bindable =
	::fast_io::transcode_unit<engine_unit> &&
	::std::integral<::std::remove_cv_t<public_char>> &&
	::std::same_as<::std::remove_cv_t<engine_unit>,
				   ::std::remove_cv_t<public_char>>;

/** @brief Detects byte endpoints that may view complete public objects as bytes. */
template <typename engine_unit, typename public_char>
concept transcode_object_bytes_bindable =
	::std::same_as<::std::remove_cv_t<engine_unit>, ::std::byte> &&
	::std::integral<::std::remove_cv_t<public_char>>;

/**
 * @brief Permits publishing arbitrary object bytes as complete values through a typed-only stream.
 * @details Every bit pattern must denote an integral value. Boolean and padded integer representations
 * are excluded. The adapter retains a partial unit across calls and diagnoses an incomplete final unit.
 */
template <typename Char>
concept transcode_wire_character = ::std::integral<Char> && !::std::same_as<::std::remove_cv_t<Char>, bool> &&
	::std::numeric_limits<Char>::digits + ::std::numeric_limits<Char>::is_signed ==
		sizeof(Char) * ::std::numeric_limits<unsigned char>::digits;

/**
 * @brief Recognizes a one-byte get or put area whose byte I/O observes the same objects and cursor.
 * @details An exact unit match uses the ordinary buffer contract. A byte engine may borrow a typed
 * window only when every bit pattern is valid, every prefix ends on a unit boundary, and the provider
 * certifies that direct access preserves its byte-channel semantics. A wider unit needs persistent
 * scratch because a process step may consume or produce a partial object.
 */
template <typename Unit, typename Char, typename Ref>
concept transcode_window_compatible = transcode_exact_units_bindable<Unit, Char> ||
	(::std::same_as<Unit, ::std::byte> && transcode_wire_character<Char> && sizeof(Char) == 1 &&
	 requires { { transcode_byte_window_define(io_type<Ref>) } noexcept -> ::std::same_as<::std::true_type>; });

/** @brief Requires an engine-readable window and a get cursor that accepts its consumed prefix. */
template <typename Unit, typename Ref>
concept transcode_input_window = ::fast_io::operations::decay::defines::has_ibuffer_basic_operations<Ref> &&
	transcode_window_compatible<Unit, typename Ref::input_char_type, Ref>;

/** @brief Requires exclusive writable storage and a put cursor that publishes only initialized units. */
template <typename Unit, typename Ref>
concept transcode_output_window = ::fast_io::operations::decay::defines::has_obuffer_basic_operations<Ref> &&
	transcode_window_compatible<Unit, typename Ref::output_char_type, Ref>;

/** @brief Accepts endpoint/public pairs supported by either binding strategy. */
template <typename engine_unit, typename public_char>
concept transcode_automatically_bindable =
	transcode_exact_units_bindable<engine_unit, public_char> ||
	transcode_object_bytes_bindable<engine_unit, public_char>;

/** @brief Chooses the compile-time binding strategy for an endpoint pair. */
template <typename engine_unit, typename public_char>
	requires transcode_automatically_bindable<engine_unit, public_char>
inline constexpr transcode_unit_binding transcode_unit_binding_for() noexcept
{
	if constexpr (transcode_exact_units_bindable<engine_unit, public_char>)
	{
		// Equal unit types can be passed directly without representation bridging.
		return transcode_unit_binding::exact_units;
	}
	else
	{
		// A byte endpoint observes complete object representations explicitly.
		return transcode_unit_binding::object_bytes;
	}
}

} // namespace fast_io
