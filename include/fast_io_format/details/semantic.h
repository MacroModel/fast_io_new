#pragma once

// These semantic leaves model reserve-print protocols, not output devices.
// Keeping the dependency freestanding preserves that distinction for both
// concat front doors and for non-ASCII execution character sets.
#include "../../fast_io_freestanding.h"

// Format details are parsed after the freestanding umbrella has restored the
// caller's macros. Re-enter fast_io's internal macro scope for capability probes.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

#if !FAST_IO_HAS_BUILTIN(__builtin_signbit) || \
	!FAST_IO_HAS_BUILTIN(__builtin_isfinite)
#include <cmath>
#endif
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace fast_io::manipulators
{

/// Preserves format-specific sign/prefix semantics while delegating scalar conversion.
///
/// `width(..., internal, '0')` asks a printable leaf where the digit sequence begins.  The
/// generic integer scalar reports only its sign and the generic floating scalar has no such
/// CPO; neither answer includes a base prefix.  A format such as `#06x` would consequently
/// place zeros before `0x`.  This wrapper supplies the complete shift while retaining the
/// existing integer/float implementation for size calculation and digit generation.
///
/// `space_sign` is implemented by asking the mature scalar formatter for an ordinary plus
/// sign and replacing that one code unit after emission.  This preserves negative zero, NaN
/// sign policy, precise sizing, and internal padding without maintaining a second sign parser.
template <typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
struct format_scalar_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	scalar_type scalar;
};

} // namespace fast_io::manipulators

namespace fast_io
{

#if defined(__clang__)
/// Opts the library-owned run-time precision floating wrapper into bounded local materialization.
///
/// The core print entry uses this marker only when a fixed static prefix precedes exactly one such scalar and the
/// destination already exposes a writable put area. Formatting into the local frame is observationally safe here:
/// both sizing and emission are fast_io's non-throwing scalar operations, rather than arbitrary user customization.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type,
		  ::std::size_t base_prefix_size, bool space_sign>
inline constexpr bool print_format_scalar_local_buffer_eligible(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
			base_prefix_size, space_sign>>) noexcept
{
	return true;
}
#endif

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>);
}

/// Forwards an object-dependent reserve bound without pretending that it is static.
///
/// Runtime-precision floating manipulators deliberately expose the two-argument reserve
/// protocol: their requested precision participates in the upper bound.  Providing only
/// the static overload above would make the format wrapper cease to model
/// `dynamic_reserve_printable`, even though emission itself can still be delegated.  The
/// constrained overload keeps that capability distinction intact and therefore preserves
/// the core dispatcher's existing allocation strategy.
template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value) {
		print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>, value);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	noexcept(noexcept(print_reserve_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)))
{
	return print_reserve_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value, char_type *iter) {
		print_reserve_define(::fast_io::io_reserve_type<char_type, scalar_type>, iter, value);
	}
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	noexcept(noexcept(print_reserve_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, value.scalar)))
{
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, scalar_type>);
	}
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, scalar_type>);
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value) {
		print_reserve_precise_size(::fast_io::io_reserve_type<char_type, scalar_type>, value);
	}
inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	noexcept(noexcept(print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)))
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, ::std::random_access_iterator iterator,
		  typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value, iterator iter, ::std::size_t size) {
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value);
	}
inline constexpr decltype(auto) print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	iterator iter, ::std::size_t size,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	noexcept(noexcept(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar)))
{
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar));
	if constexpr (::std::same_as<define_result, void>)
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar);
		if constexpr (space_sign)
		{
			if (size != 0u && *iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
	}
	else
	{
		// Preserve an endpoint-reporting producer.  Returning `void` unconditionally
		// would remain semantically correct, but it would erase the stronger
		// `nothrow_precise_reserve_printable` proof used by overwrite-capable concat
		// destinations to avoid an initialization pass.
		auto result{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar)};
		if constexpr (space_sign)
		{
			if (iter != result && *iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
		return result;
	}
}

namespace fmt::details
{

/**
 * Emits repetitions of one encoded format fill scalar.
 *
 * A fill scalar occupies one to four destination code units.  The common
 * one-unit case deliberately reaches core's constexpr-aware fill primitive;
 * at run time that has the memset shape expected by GCC and Clang.  Wider
 * encodings seed one scalar and then double the initialized prefix with
 * non-overlapping copies, avoiding a branch and store for every repetition.
 */
template <::std::integral char_type>
inline constexpr char_type *emit_repeated_code_unit_pattern(
	char_type *output, char_type const *pattern,
	::std::size_t pattern_size, ::std::size_t repetitions) noexcept
{
	if (repetitions == 0u || pattern_size == 0u)
	{
		return output;
	}
	if (4u < pattern_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	if constexpr (::std::is_volatile_v<char_type>)
	{
		for (::std::size_t repetition{}; repetition != repetitions;
			 ++repetition)
		{
			for (::std::size_t index{}; index != pattern_size; ++index)
			{
				*output++ = pattern[index];
			}
		}
		return output;
	}
	else if (pattern_size == 1u)
	{
		return ::fast_io::details::my_fill_n(
			output, repetitions, pattern[0u]);
	}
	else
	{
		auto const total_size{
			::fast_io::details::intrinsics::mul_or_overflow_die(
				pattern_size, repetitions)};
		auto *const first{output};
		output = ::fast_io::details::non_overlapped_copy_n(
			pattern, pattern_size, output);
		::std::size_t produced{pattern_size};
		while (produced != total_size)
		{
			auto const remaining{total_size - produced};
			auto const copy_size{
				produced < remaining ? produced : remaining};
			output = ::fast_io::details::non_overlapped_copy_n(
				first, copy_size, output);
			produced += copy_size;
		}
		return output;
	}
}

template <typename value_type>
[[nodiscard]] inline constexpr bool scalar_negative(value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
#if FAST_IO_HAS_BUILTIN(__builtin_signbit)
		return __builtin_signbit(value);
#else
		return ::std::signbit(value);
#endif
	}
	else if constexpr (::fast_io::details::my_signed_integral<clean_type>)
	{
		return value < 0;
	}
	else
	{
		return false;
	}
}

template <typename value_type>
[[nodiscard]] inline constexpr bool scalar_finite(value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
#if FAST_IO_HAS_BUILTIN(__builtin_isfinite)
		return __builtin_isfinite(value);
#else
		return ::std::isfinite(value);
#endif
	}
	else
	{
		return true;
	}
}

template <::fast_io::manipulators::scalar_flags flags, typename value_type>
[[nodiscard]] inline constexpr ::std::size_t formatted_scalar_internal_shift(
	value_type value, ::std::size_t base_prefix_size) noexcept
{
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || ::fast_io::fmt::details::scalar_negative(value))};
	auto const prefix_size{
		::fast_io::fmt::details::scalar_finite(value) ? base_prefix_size : 0u};
	return sign_size + prefix_size;
}

} // namespace fmt::details

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::scalar_manip_t<flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::scalar_manip_t<flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

} // namespace fast_io

#include "../../fast_io_dsal/impl/misc/pop_macros.h"
