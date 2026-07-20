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

/// @brief Propagates concat's one-pass bounded cost marker through the format-specific scalar wrapper.
/// @details The wrapper changes sign/prefix spelling but delegates conversion and its dynamic reserve bound to the
///          wrapped scalar. Only a child with the exact fast_io source opt-in can select concat's bounded stack path.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			concat_single_pass_bounded_materialization_preferred(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
inline constexpr ::std::true_type concat_single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Propagates print's independent direct-put-area authorization through the format scalar wrapper.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_single_pass_bounded_direct_put_area_safe(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Forwards concat's non-fatal candidate bound through the format scalar wrapper.
/// @details Prefix placement and space-sign spelling do not add code units; they only reinterpret bytes already
///          covered by the child scalar's reserve bound. Preserving the caller's limit lets an extreme dynamic
///          precision reject speculative materialization before the ordinary fatal-overflow reserve protocol runs.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
		requires requires(scalar_type value, ::std::size_t maximum_size) {
			{
				concat_single_pass_bounded_materialization_size(
					::fast_io::io_reserve_type<char_type, scalar_type>, value,
					maximum_size)
			} noexcept -> ::std::same_as<::std::size_t>;
		}
inline constexpr ::std::size_t concat_single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> value,
	::std::size_t maximum_size) noexcept
{
	return concat_single_pass_bounded_materialization_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar,
		maximum_size);
}

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

/// Keeps the optional compiler-constant floating proxy visible through format's
/// spelling wrapper.
///
/// The generic wrapper above intentionally retains its established inlining
/// policy.  A constant hexadecimal float has a larger independent formatter,
/// however, and Clang otherwise outlines this one forwarding frame even though
/// the value is already proven constant.  Restricting the force-inline overload
/// to the replacement proxy exposes that value to its dedicated writer without
/// changing the code shape of any ordinary run-time format leaf.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
		requires requires(
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type> scalar,
		char_type *iter) {
		print_reserve_define(
			::fast_io::io_reserve_type<
				char_type,
				::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
					char_type, flags, floating_type>>,
			iter, scalar);
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<
			char_type,
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type>>,
		iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

/// Keeps the integer-fields hexadecimal precision proxy visible through the
/// same format spelling wrapper.  This is the precision counterpart of the
/// scalar overload above; format continues to translate syntax only, while
/// print/concat owns constant recognition and materialization.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
		 requires requires(
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type> scalar,
			char_type *iter) {
		print_reserve_define(
			::fast_io::io_reserve_type<
				char_type,
				::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
					char_type, flags, floating_type>>,
			iter, scalar);
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<
			char_type,
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>>,
		iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

/// Propagates the core compiler-constant protocol through format's sign/prefix spelling wrapper.
///
/// Format lowering only translates the parsed field to this semantic scalar.  The value-level decision and the
/// replacement formatter remain owned by the shared print/concat protocol below it.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_query_inline_safe<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// Format lowering contributes only the spelling wrapper. The wrapped scalar's core opt-in is the proof that the
/// complete lowered leaf may cross print's pre-normalization replacement boundary.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_pre_normalization_safe<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Forwards the child's proof that a successful eligibility query already enforces the compact byte budget.
/// @details Width consumes this type-only fact to avoid materializing and exactly sizing an expensive precision float a
///          second time merely to repeat the same bound. Format itself makes no value-level decision here.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_eligible_implies_compact_size(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	 requires ::fast_io::compiler_constant_printable<char_type, scalar_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	return print_compiler_constant_materialization_eligible(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	 requires ::fast_io::compiler_constant_printable<char_type, scalar_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	using materialized_scalar =
		::fast_io::details::compiler_constant_materialized_t<char_type, scalar_type>;
	return ::fast_io::manipulators::format_scalar_t<
		materialized_scalar, base_prefix_size, space_sign>{
		print_compiler_constant_materialize(
			::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)};
}

/// Propagates the immutable-fragment representation through format's spelling-only scalar wrapper.
/// Format contributes no character buffer here: the core scalar owns every digit/punctuation table and the print
/// destination decides whether descriptors or ordinary reserve output are appropriate.  The sole spelling adjustment
/// made by this wrapper is printf/brace's space-sign rule, which substitutes a static space descriptor for a leading
/// plus without modifying any payload storage.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_static_fragment_printable<
		char_type, scalar_type>
inline constexpr ::std::size_t print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return print_compiler_constant_static_fragments_size(
		::fast_io::io_reserve_type<char_type, scalar_type>);
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_static_fragment_printable<
		char_type, scalar_type>
inline constexpr ::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	auto const last{print_compiler_constant_static_fragments_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, first,
		value.scalar)};
	if constexpr (space_sign)
	{
		if (first != last && first->len != 0u &&
			*first->base == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			first->base = __builtin_addressof(
				::fast_io::char_literal_v<u8' ', char_type>);
			first->len = 1u;
		}
	}
	return last;
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

/// @brief Keeps a compiler-constant precision-float proxy visible across format's final precise-define forwarding leaf.
/// @details Clang 23 otherwise outlined only this spelling wrapper for `fmt::print<"v={:.3a}">(out(), 1.25)`, leaving
///          a 120-byte frame and a call after the core proxy had already become fully constant.  The overload is
///          intentionally limited to that replacement type; ordinary run-time scalar and format lowering paths keep
///          the generic compiler-selected inlining policy above.
template <::std::integral char_type, ::std::random_access_iterator iterator,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires ::std::same_as<char_type, proxy_char_type> &&
		requires(
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, floating_type> scalar,
			iterator iter, ::std::size_t size) {
			print_reserve_precise_define(
				::fast_io::io_reserve_type<
					char_type,
					::fast_io::manipulators::
						compiler_constant_floating_precision_manip_t<
							proxy_char_type, flags, floating_type>>,
				iter, size, scalar);
		}
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	iterator iter, ::std::size_t size,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	using scalar_type =
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>;
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
		value.scalar));
	if constexpr (::std::same_as<define_result, void>)
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
			value.scalar);
		if constexpr (space_sign)
		{
			if (size != 0u &&
				*iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
	}
	else
	{
		auto result{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
			value.scalar)};
		if constexpr (space_sign)
		{
			if (iter != result &&
				*iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
		return result;
	}
}

/// Propagates print's compact-before-fragments profitability marker through format's spelling-only wrapper.
/// Format contributes no strategy decision here: it merely preserves a type-level promise made by the lowered
/// scalar.  The print layer remains responsible for its output-device and size thresholds.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_prefer_precise_compact(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// Propagates a provider-owned single immutable spelling through format's scalar wrapper.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type const &value) {
		{
			print_compiler_constant_single_static_fragment(
				::fast_io::io_reserve_type<char_type, scalar_type>, value)
		} noexcept -> ::std::same_as<
			::fast_io::basic_io_scatter_t<char_type>>;
	}
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
	::fast_io::basic_io_scatter_t<char_type>
print_compiler_constant_single_static_fragment(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	return print_compiler_constant_single_static_fragment(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
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

/// @brief Preserves format-internal padding after an integer scalar is replaced by its compiler-constant proxy.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_scalar_manip_t<
				flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

/// @brief Preserves sign/prefix placement for an optimizer-proven default/scalar floating replacement.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				proxy_char_type, flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, value_type>,
		base_prefix_size, space_sign> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || value.scalar.negative)};
	auto const prefix_size{value.scalar.binary_exponent == exponent_mask
		? 0u
		: base_prefix_size};
	return sign_size + prefix_size;
}

/// @brief Preserves sign/prefix placement for a compiler-constant explicit-precision floating replacement.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, value_type>,
		base_prefix_size, space_sign> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || static_cast<bool>(value.scalar.fields.sign))};
	auto const prefix_size{value.scalar.fields.exponent == exponent_mask
		? 0u
		: base_prefix_size};
	return sign_size + prefix_size;
}

} // namespace fast_io

#include "../../fast_io_dsal/impl/misc/pop_macros.h"
