#pragma once

#include "itoa_precise_length.h"

// This is diagnostic policy for a header-only implementation: a non-Clang
// GNU-compatible frontend would otherwise repeat implementation-detail
// warnings in every consumer.  The pragma changes no generated algorithm and
// is confined to GNU pragma syntax.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC system_header
#endif

#if defined(FAST_IO_ENABLE_FLOATING_POINT_ENVIRONMENT)
#if __has_include(<cfenv>)
#include <cfenv>
#define FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT 1
#endif
#endif

namespace fast_io
{

namespace details
{

inline constexpr ::std::size_t method_ptr_size{sizeof(::std::size_t) * 2};

inline constexpr ::std::size_t method_ptr_hold_size{::fast_io::details::method_ptr_size % sizeof(::std::size_t) == 0
														? ::fast_io::details::method_ptr_size / sizeof(::std::size_t)
														: ::fast_io::details::method_ptr_size / sizeof(::std::size_t) +
															  1};

struct scalar_manip_detail_tag
{
};

template <typename scalar_type>
concept has_scalar_manip_detail_tag =
	requires(scalar_type) { typename ::std::remove_cvref_t<scalar_type>::scalar_manip_detail_tag; };

} // namespace details

namespace manipulators
{

enum class scalar_placement : char8_t
{
	none,
	left,
	middle,
	right,
	internal
};

enum class floating_format : char8_t
{
	fixed,
	general,
	scientific,
	decimal,
	hexfloat
};

enum class floating_nan_payload_scan : ::std::uint_fast8_t
{
	none,
	consume,
	preserve
};

enum class floating_precision : ::std::uint_least8_t
{
	significant,
	fractional,
	significant_preserve_trailing_zero,
	fractional_preserve_trailing_zero
};

enum class floating_rounding : ::std::uint_least8_t
{
	nearest_to_even,
	nearest_to_odd,
	nearest_toward_plus_infinity,
	nearest_toward_minus_infinity,
	nearest_toward_zero,
	nearest_away_from_zero,
	toward_plus_infinity,
	toward_minus_infinity,
	toward_zero,
	away_from_zero,
	current_environment
};

enum class lc_time_flag : ::std::uint_least8_t
{
	none,
	d_t_fmt,
	d_fmt,
	t_fmt,
	t_fmt_ampm,
	date_fmt,
	era_d_t_fmt,
	era_d_fmt,
	era_t_fmt
};

enum class percentage_flag : ::std::uint_least8_t
{
	none,
	percent,
	sexratio
};

struct ip_flags
{
	bool v6shorten{true};
	bool v6uppercase{};
	bool v6bracket{true};
	bool v6full{};
	bool showport{};
	bool ipv4_mapped_ipv6{true};
};

inline constexpr ip_flags ip_default_flags{.showport = true};
inline constexpr ip_flags ip_default_inaddr_flags{};

template <ip_flags flags, typename T>
struct ip_manip_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
#if 0
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
#endif
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

struct ip_scan_flags
{
	bool allowv6shorten{true};
	bool allowv6uppercase{true};
	bool allowv6bracket{true};
	bool requirev6full{false};
	bool requireport{false};
	bool ipv4_mapped_ipv6{true};
};

inline constexpr ip_scan_flags ip_scan_default_flags{.requireport = true};
inline constexpr ip_scan_flags ip_scan_no_ipv4_mapped_ipv6{.requireport = true, .ipv4_mapped_ipv6 = false};
inline constexpr ip_scan_flags ip_scan_default_inaddr_flags{};

template <ip_scan_flags flags, typename T>
struct ip_scan_manip_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

struct scalar_flags
{
	// Scan and print: integer radix in the closed range [2, 36]. Floating
	// manipulators keep this at 10 as a dispatch invariant; the actual decimal
	// versus hexadecimal floating syntax is selected by `floating`.
	::std::size_t base{10};
	// Print only: use alphabetic spellings for scalar values that have such a
	// spelling, such as boolalpha-style bools and nullptr. Numeric scan paths do
	// not consult this flag.
	bool alphabet{};
	// Scan and print: print a base prefix for integer/hex-float output, or
	// require and consume that prefix during integer/hex-float input. Decimal
	// integer scan ignores the prefix request because base 10 has no prefix.
	bool showbase{};
	// Print only: emit an explicit '+' for non-negative signed numeric output
	// and for floating special values according to the NaN sign policy. Input
	// uses `allow_leading_plus` instead, matching from_chars-style defaults.
	bool showpos{};
	// Scan only: disable the usual leading whitespace skip. String-like scan
	// also uses this to switch from token parsing to exact-position parsing.
	bool noskipws{};
	// Print only: choose uppercase prefix letters for bases with alphabetic
	// prefixes, for example 0X/0B instead of 0x/0b. Scan accepts valid prefix
	// spellings independently of this output preference.
	bool uppercase_showbase{};
	// Scan and print: select the modern octal prefix grammar for base 8 where
	// the manipulator requests a prefix, such as 0o/0O instead of the legacy 0
	// spelling.
	bool modern_octal{};
	// Print only: use uppercase alphabetic digits and uppercase spellings for
	// affected scalar text, such as A-F, INF/NAN, TRUE/FALSE, and nullptr forms.
	// Scan is case-tolerant where the grammar allows it.
	bool uppercase{};
	// Print only: use an uppercase exponent marker for floating output, such as
	// E for decimal scientific notation and P for hexadecimal floating output.
	// Scan accepts both exponent-marker cases where applicable.
	bool uppercase_e{};
#if 0
	// Print only: reserved exponent-sign policy. If enabled, it would control
	// whether positive floating exponents are printed with an explicit '+'.
	bool showpos_e{true};
#endif
	// Print only: request the comma variant of scalar formatting. For floating
	// and timestamp-like output this changes the fractional separator, and for
	// percentage-style output it selects the comma-aware spelling.
	bool comma{};
	// Scan and print, with manipulator-specific meaning: integer print uses it
	// for full-width/zero-filled forms such as addresses; integer scan reuses
	// the same storage bit as the skip-leading-zero policy.
	bool full{};
	// Print only: JSON-friendly decimal floating spelling. For general/default
	// and fixed decimal output, integer-valued finite results keep a fractional
	// marker such as 0.0 or 10.0; scientific output is not forced this way.
	bool json_float{};
	// Print only: alignment metadata for scalar width formatting. Numeric scan
	// does not use placement; width wrappers consume this kind of policy while
	// composing formatted output.
	scalar_placement placement{scalar_placement::none};
	// Scan and print: select the floating grammar/format family. Print uses it
	// to choose fixed, general, scientific, shortest decimal, or hexfloat
	// output; scan uses it to constrain decimal fixed/scientific forms or to
	// select hexadecimal floating parsing.
	floating_format floating{};
	// Print only: locale time-format selector for timestamp manipulators, such
	// as d_t_fmt, d_fmt, t_fmt, and the era-aware variants. It is not part of
	// numeric scan.
	lc_time_flag time_flag{};
#if 0
	// Scan only: reserved locale-aware parse selector. It is currently disabled
	// and should not be assumed by active scan code.
	bool localeparse{};
#endif
	// Scan only: string-like input mode that stops at a line boundary instead
	// of using the ordinary whitespace-delimited token behavior.
	bool line{};
	// Print only: select percentage or sex-ratio scalar output. Ordinary
	// integer/floating scan does not interpret trailing percent notation through
	// this flag.
	percentage_flag percentage{};
	// Print only: when printing NaNs, preserve and print the NaN sign if the
	// representation carries one. Infinity sign output is governed by the
	// ordinary sign path, not by this NaN-specific switch.
	bool nan_show_sign{true};
	// Print only: append the implementation-specific NaN kind/type suffix, for
	// example forms such as signaling or indeterminate NaNs, when that
	// information is representable by the floating backend.
	bool nan_show_type{};
	// Scan only: when parsing NaN special values, decide whether a consumed
	// leading '-' is reflected into the resulting NaN sign bit. Infinity uses
	// the normal parsed sign regardless of this NaN-specific policy.
	bool nan_parse_sign{true};
	// Scan only: policy for a NaN parenthesized payload/type suffix. `none`
	// rejects or stops before payload handling, `consume` accepts and discards
	// the suffix, and `preserve` maps recognized suffix information into the
	// produced NaN where the target format supports it.
	floating_nan_payload_scan nan_payload_scan{floating_nan_payload_scan::consume};
	// Scan and print for floating values: rounding policy used by precision
	// output and by decimal-to-binary input. The current-environment variant is
	// only meaningful when floating environment support is enabled.
	floating_rounding rounding{};
	// Scan and print for precision-bearing floating manipulators: significant
	// precision means total significant digits; fractional precision means
	// digits after the radix point. Hexfloat precision supports significant
	// hexadecimal digits only.
	floating_precision precision{};
	// Scan only: accept an optional leading '+' before integer, decimal-float,
	// or hex-float input. The default is false to match from_chars-style
	// behavior; exponent signs such as e+1 or p+1 are controlled by the
	// floating grammar and remain separate.
	bool allow_leading_plus{};
};

inline constexpr scalar_flags integral_default_scalar_flags{};
inline constexpr scalar_flags floating_point_default_scalar_flags{.floating = floating_format::decimal};
inline constexpr scalar_flags address_default_scalar_flags{.base = 16, .showbase = true, .full = true};

template <scalar_flags flags, typename T>
struct scalar_manip_t
{
	using value_type = T;
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

/// @brief Print/concat-owned reserve proxy for an optimizer-proven constant scalar.
/// @details Ordinary scalar formatting deliberately keeps its established integer algorithm.  This distinct type is
///          created only by the compiler-constant strategy and selects a compact constant-friendly digit writer without
///          inserting a probe or branch into the hot run-time formatter.
template <scalar_flags flags, typename T>
struct compiler_constant_scalar_manip_t
{
	using value_type = T;
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
	T reference;
};

struct member_function_pointer_holder_t
{
	using manip_tag = manip_tag_t;
	::fast_io::freestanding::array<::std::size_t, ::fast_io::details::method_ptr_hold_size> reference;
};

template <typename T>
struct whole_get_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
	value_type reference;
};

} // namespace manipulators

namespace details
{
template <::std::size_t bs, bool upper, bool shbase, bool fll, bool showpos = false, bool comma = false, bool oct_c2y = false,
		  ::fast_io::manipulators::percentage_flag perflag = ::fast_io::manipulators::percentage_flag::none>
inline constexpr ::fast_io::manipulators::scalar_flags base_mani_flags_cache{
	.base = bs,
	.showbase = shbase,
	.showpos = showpos,
	.uppercase_showbase = ((bs == 2 || bs == 3 || (bs == 8 && oct_c2y) || bs == 16) ? upper : false),
	.modern_octal = (bs == 8 ? oct_c2y : false),
	.uppercase = ((bs <= 10) ? false : upper),
	.comma = comma,
	.full = fll,
	.percentage = perflag};

template <bool upper>
inline constexpr ::fast_io::manipulators::scalar_flags boolalpha_mani_flags_cache{.alphabet = true, .uppercase = upper};

template <bool uppercase, bool comma, bool showbase = false>
inline constexpr ::fast_io::manipulators::scalar_flags hexafloat_mani_flags_cache{
	.showbase = showbase,
	.uppercase_showbase = uppercase,
	.uppercase = uppercase,
	.uppercase_e = uppercase,
	.comma = comma,
	.floating = ::fast_io::manipulators::floating_format::hexfloat};

template <bool uppercase, bool comma, ::fast_io::manipulators::floating_format fm>
inline constexpr ::fast_io::manipulators::scalar_flags dcmfloat_mani_flags_cache{
	.uppercase = uppercase, .uppercase_e = uppercase, .comma = comma, .floating = fm};

inline constexpr ::fast_io::manipulators::scalar_flags
set_floating_rounding_flag(::fast_io::manipulators::scalar_flags flags,
						   ::fast_io::manipulators::floating_rounding rounding) noexcept
{
	flags.rounding = rounding;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, ::fast_io::manipulators::floating_rounding rounding>
inline constexpr ::fast_io::manipulators::scalar_flags floating_rounding_mani_flags_cache{
	::fast_io::details::set_floating_rounding_flag(flags, rounding)};

inline constexpr ::fast_io::manipulators::scalar_flags
set_floating_precision_flag(::fast_io::manipulators::scalar_flags flags,
							::fast_io::manipulators::floating_precision precision) noexcept
{
	flags.precision = precision;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, ::fast_io::manipulators::floating_precision precision>
inline constexpr ::fast_io::manipulators::scalar_flags floating_precision_mani_flags_cache{
	::fast_io::details::set_floating_precision_flag(flags, precision)};

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding>
inline constexpr ::fast_io::manipulators::scalar_flags floating_precision_rounding_mani_flags_cache{
	::fast_io::details::set_floating_rounding_flag(
		::fast_io::details::set_floating_precision_flag(flags, precision), rounding)};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_is_significant{
	precision == ::fast_io::manipulators::floating_precision::significant ||
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_is_fractional{
	precision == ::fast_io::manipulators::floating_precision::fractional ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_preserves_trailing_zero{
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero};

inline constexpr ::fast_io::manipulators::scalar_flags
set_json_float_flag(::fast_io::manipulators::scalar_flags flags, bool json_float) noexcept
{
	flags.json_float = json_float;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, bool json_float>
inline constexpr ::fast_io::manipulators::scalar_flags json_float_mani_flags_cache{
	::fast_io::details::set_json_float_flag(flags, json_float)};

inline constexpr ::fast_io::manipulators::scalar_flags
set_allow_leading_plus_flag(::fast_io::manipulators::scalar_flags flags, bool allow_leading_plus) noexcept
{
	flags.allow_leading_plus = allow_leading_plus;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, bool allow_leading_plus>
inline constexpr ::fast_io::manipulators::scalar_flags allow_leading_plus_mani_flags_cache{
	::fast_io::details::set_allow_leading_plus_flag(flags, allow_leading_plus)};

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool floating_rounding_is_nearest{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero};

inline
#if !defined(FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT)
	constexpr
#endif
	::fast_io::manipulators::floating_rounding current_floating_rounding() noexcept
{
#if defined(FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT)
	switch (::std::fegetround())
	{
#if defined(FE_UPWARD)
	case FE_UPWARD:
		return ::fast_io::manipulators::floating_rounding::toward_plus_infinity;
#endif
#if defined(FE_DOWNWARD)
	case FE_DOWNWARD:
		return ::fast_io::manipulators::floating_rounding::toward_minus_infinity;
#endif
#if defined(FE_TOWARDZERO)
	case FE_TOWARDZERO:
		return ::fast_io::manipulators::floating_rounding::toward_zero;
#endif
	default:
		return ::fast_io::manipulators::floating_rounding::nearest_to_even;
	}
#else
	return ::fast_io::manipulators::floating_rounding::nearest_to_even;
#endif
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool floating_rounding_directed_round_up(bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool floating_rounding_nearest_tie_round_up(bool negative,
																		   ::std::uint_least64_t mantissa) noexcept
{
	auto const rounded_down{mantissa >> 1u};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return (rounded_down & 1u) != 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return (rounded_down & 1u) == 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <bool uppercase, bool shbase>
inline constexpr ::fast_io::manipulators::scalar_flags cryptohash_mani_flags_cache{
	.base = 16, .showbase = shbase, .uppercase_showbase = uppercase, .uppercase = uppercase};

template <::std::size_t bs, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false>
inline constexpr ::fast_io::manipulators::scalar_flags base_scan_mani_flags_cache{
	.base = bs,
	.showbase = shbase,
	.noskipws = noskipws,
	.modern_octal = oct_c2y,
	.full = skipzero,
	.allow_leading_plus = allow_leading_plus};

template <bool shport>
inline constexpr ::fast_io::manipulators::ip_flags base_ip_prefix_flags_cache{.showport = shport};

template <typename inttype>
struct unsigned_integer_alias_type_traits_helper
{
	using alias_type = ::std::conditional_t<
		(sizeof(inttype) == sizeof(::std::uint_least8_t)), ::std::uint_least8_t,
		::std::conditional_t<
			(sizeof(inttype) == sizeof(::std::uint_least16_t)), ::std::uint_least16_t,
			::std::conditional_t<(sizeof(inttype) == sizeof(::std::uint_least32_t)), ::std::uint_least32_t,
								 ::std::conditional_t<(sizeof(inttype) == sizeof(::std::uint_least64_t)),
													  ::std::uint_least64_t, inttype>>>>;
};

template <typename inttype>
struct signed_integer_alias_type_traits_helper
{
	using alias_type = ::std::conditional_t<
		(sizeof(inttype) == sizeof(::std::int_least8_t)), ::std::int_least8_t,
		::std::conditional_t<
			(sizeof(inttype) == sizeof(::std::int_least16_t)), ::std::int_least16_t,
			::std::conditional_t<(sizeof(inttype) == sizeof(::std::int_least32_t)), ::std::int_least32_t,
								 ::std::conditional_t<(sizeof(inttype) == sizeof(::std::int_least64_t)),
													  ::std::int_least64_t, inttype>>>>;
};

template <typename inttype>
struct integer_alias_type_traits
{
	using alias_type = ::std::conditional_t<
		my_unsigned_integral<inttype>,
		::std::conditional_t<(sizeof(inttype) < sizeof(unsigned)),
							 typename unsigned_integer_alias_type_traits_helper<unsigned>::alias_type,
							 typename unsigned_integer_alias_type_traits_helper<inttype>::alias_type>,
		::std::conditional_t<(sizeof(inttype) < sizeof(int)),
							 typename signed_integer_alias_type_traits_helper<int>::alias_type,
							 typename signed_integer_alias_type_traits_helper<inttype>::alias_type>>;
};

template <typename inttype>
using integer_alias_type = typename integer_alias_type_traits<::std::remove_cvref_t<inttype>>::alias_type;

using uintptr_alias_type = ::fast_io::details::integer_alias_type<::std::size_t>;

template <typename inttype>
struct integer_full_alias_type_traits
{
	using alias_type = ::std::conditional_t<my_unsigned_integral<inttype>,
											typename unsigned_integer_alias_type_traits_helper<inttype>::alias_type,
											typename signed_integer_alias_type_traits_helper<inttype>::alias_type>;
};

template <typename inttype>
using integer_full_alias_type = typename integer_full_alias_type_traits<::std::remove_cvref_t<inttype>>::alias_type;

using uintptr_full_alias_type = ::fast_io::details::integer_full_alias_type<::std::size_t>;

template <typename flt>
struct float_alias_type_traits
{
	using alias_type = flt;
};

#if defined(FAST_IO_HAS_FLOAT64_TYPE)
template <>
struct float_alias_type_traits<double>
{
	using alias_type =
		::std::conditional_t<sizeof(_Float64) == sizeof(double) && ::std::numeric_limits<double>::is_iec559, _Float64,
							 double>;
};
#endif

#ifdef __STDCPP_FLOAT32_T__
template <>
struct float_alias_type_traits<float>
{
	using alias_type =
		::std::conditional_t<sizeof(_Float32) == sizeof(float) && ::std::numeric_limits<float>::is_iec559, _Float32,
							 float>;
};
#endif

inline constexpr bool long_double_alias_is_double{
	sizeof(long double) == sizeof(double) &&
	::std::numeric_limits<long double>::digits == ::std::numeric_limits<double>::digits &&
	::std::numeric_limits<long double>::max_exponent == ::std::numeric_limits<double>::max_exponent};

inline constexpr bool long_double_alias_is_float80{
	::std::numeric_limits<long double>::digits == 64 &&
	::std::numeric_limits<long double>::max_exponent == 16384};

#if defined(__SIZEOF_INT128__) && defined(__STDCPP_FLOAT128_T__)
inline constexpr bool long_double_alias_is_float128{
	sizeof(long double) == sizeof(_Float128) && ::std::numeric_limits<long double>::digits == 113 &&
	::std::numeric_limits<long double>::max_exponent == 16384};
#elif defined(__SIZEOF_INT128__) && defined(__FLOAT128__)
inline constexpr bool long_double_alias_is_float128{
	sizeof(long double) == sizeof(__float128) && ::std::numeric_limits<long double>::digits == 113 &&
	::std::numeric_limits<long double>::max_exponent == 16384};
#else
inline constexpr bool long_double_alias_is_float128{};
#endif

template <bool is_double, bool is_float80, bool is_float128>
struct long_double_alias_type_traits
{
	using alias_type = typename float_alias_type_traits<double>::alias_type;
};

template <bool is_float128>
struct long_double_alias_type_traits<false, true, is_float128>
{
	using alias_type = long double;
};

#if defined(__SIZEOF_INT128__) && defined(__STDCPP_FLOAT128_T__)
template <>
struct long_double_alias_type_traits<false, false, true>
{
	using alias_type = _Float128;
};
#elif defined(__SIZEOF_INT128__) && defined(__FLOAT128__)
template <>
struct long_double_alias_type_traits<false, false, true>
{
	using alias_type = __float128;
};
#endif

template <>
struct float_alias_type_traits<long double>
{
	using alias_type =
		typename long_double_alias_type_traits<long_double_alias_is_double, long_double_alias_is_float80,
											   long_double_alias_is_float128>::alias_type;
};

#if (defined(__SIZEOF_FLOAT16__) || defined(__FLOAT16__)) && defined(__STDCPP_FLOAT16_T__)
template <>
struct float_alias_type_traits<__float16>
{
	using alias_type = _Float16;
};
#endif

template <typename flt>
using float_alias_type = typename float_alias_type_traits<::std::remove_cvref_t<flt>>::alias_type;

inline constexpr ::fast_io::manipulators::scalar_flags
compute_bool_scalar_flags_cache(::fast_io::manipulators::scalar_flags flags) noexcept
{
	flags.uppercase = false;
	if (!flags.showbase)
	{
		flags.base = 2;
		flags.uppercase_showbase = false;
	}
	return flags;
}

template <::fast_io::manipulators::scalar_flags cache, typename scalar_type>
inline constexpr auto scalar_flags_int_cache(scalar_type t) noexcept
{
	using scalar_type_nocvref = ::std::remove_cvref_t<scalar_type>;
	if constexpr (cache.full)
	{
		if constexpr (::std::same_as<scalar_type_nocvref, ::std::nullptr_t>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, ::std::nullptr_t>{};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, bool>)
		{
			return ::fast_io::manipulators::scalar_manip_t<compute_bool_scalar_flags_cache(cache), bool>{t};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, ::std::byte>)
		{
			using alias_type = integer_full_alias_type<char unsigned>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::fast_io::details::my_integral<scalar_type_nocvref>)
		{
			using alias_type = integer_full_alias_type<scalar_type_nocvref>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::std::is_pointer_v<scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_full_alias_type>{
				static_cast<uintptr_full_alias_type>(::std::bit_cast<::std::size_t>(t))};
		}
		else if constexpr (::std::same_as<::fast_io::manipulators::member_function_pointer_holder_t,
										  scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache,
														   ::fast_io::manipulators::member_function_pointer_holder_t>{
				t};
		}
		else if constexpr (::fast_io::details::has_scalar_manip_detail_tag<scalar_type>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, typename scalar_type_nocvref::value_type>{
				t.reference};
		}
		else
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_full_alias_type>{
				static_cast<uintptr_full_alias_type>(::std::bit_cast<::std::size_t>(::std::to_address(t)))};
		}
	}
	else
	{
		if constexpr (::std::same_as<scalar_type_nocvref, ::std::nullptr_t>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, ::std::nullptr_t>{};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, bool>)
		{
			return ::fast_io::manipulators::scalar_manip_t<compute_bool_scalar_flags_cache(cache), bool>{t};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, ::std::byte>)
		{
			using alias_type = integer_alias_type<char unsigned>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::fast_io::details::my_integral<scalar_type_nocvref>)
		{
			using alias_type = integer_alias_type<scalar_type_nocvref>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::std::same_as<::fast_io::manipulators::member_function_pointer_holder_t,
										  scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache,
														   ::fast_io::manipulators::member_function_pointer_holder_t>{
				t};
		}
		else if constexpr (::std::is_pointer_v<scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_alias_type>{
				static_cast<uintptr_alias_type>(::std::bit_cast<::std::size_t>(t))};
		}
		else if constexpr (::fast_io::details::has_scalar_manip_detail_tag<scalar_type>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, typename scalar_type_nocvref::value_type>{
				t.reference};
		}
		else
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_alias_type>{
				static_cast<uintptr_alias_type>(::std::bit_cast<::std::size_t>(::std::to_address(t)))};
		}
	}
}

template <typename scalar_type>
concept scalar_integrals = ::fast_io::details::non_character_integral<scalar_type> ||
						   ::std::same_as<::std::nullptr_t, ::std::remove_cvref_t<scalar_type>> ||
						   ::std::same_as<::std::byte, ::std::remove_cvref_t<scalar_type>> ||
						   ::std::same_as<bool, ::std::remove_cvref_t<scalar_type>> ||
						   ::fast_io::details::has_scalar_manip_detail_tag<scalar_type>;

} // namespace details

namespace manipulators
{

template <scalar_flags flags, typename T>
struct scalar_manip_precision_t
{
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t precision;
};

/// Integer-owned run-time transport for the one Clang/x86 narrow-float ABI
/// domain whose scalar-to-aggregate construction can perform a second
/// bfloat16 conversion.  `floating_type` remains the semantic format domain;
/// `representation` is the original two-byte object representation.
template <scalar_flags flags, typename floating_type>
struct floating_scalar_field_manip_t
{
	using value_type = floating_type;
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
	::std::uint_least16_t representation;
};

template <scalar_flags flags, typename floating_type>
struct floating_scalar_field_manip_precision_t
{
	using value_type = floating_type;
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
	::std::uint_least16_t representation;
	::std::size_t precision;
};

} // namespace manipulators

namespace details
{

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!defined(__AVX512BF16__) &&                                       \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename T>
inline constexpr bool floating_scalar_requires_integer_proxy{
	::std::same_as<::std::remove_cvref_t<T>, __bf16>};

#else
template <typename T>
inline constexpr bool floating_scalar_requires_integer_proxy{};
#endif

template <typename T>
concept floating_scalar_integer_proxy_source =
	::fast_io::details::floating_scalar_requires_integer_proxy<T>;

/// Copies the bfloat16 object representation before scalar proxy ownership.
/// On Clang/x86 without native AVX512BF16 transport, the integer read/write
/// constraint freezes the representation at this first owning boundary.
/// Without it, the frontend may retain binary32 SSA and later rebuild bfloat16
/// with an exception-raising retruncation. AVX512BF16 deliberately bypasses
/// this workaround so public floating manipulators keep their native XMM
/// calling convention. The empty statement emits no instruction and requests
/// a general register only; constant evaluation bypasses it entirely.
template <typename scalar_type>
[[nodiscard]] inline constexpr ::std::uint_least16_t
capture_bfloat16_representation(scalar_type &&value) noexcept
{
	static_assert(
		::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>);
	auto representation{
		::fast_io::bit_cast<::std::uint_least16_t>(value)};
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!defined(__AVX512BF16__) &&                                       \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	FAST_IO_IF_NOT_CONSTEVAL
	{
		__asm__("" : "+r"(representation));
	}
#endif
	return representation;
}

/// Forms the ordinary by-value floating manipulator or the Clang/x86 bfloat16
/// integer proxy selected by the public capture boundary. Public constructors
/// for every other floating type remain by-value and retain their native
/// floating-register calling convention.
template <::fast_io::manipulators::scalar_flags flags, typename scalar_type>
	requires ::fast_io::details::my_floating_point<
		::std::remove_cvref_t<scalar_type>>
[[nodiscard]] inline constexpr auto
make_floating_scalar_manip(scalar_type &&value) noexcept
{
	using floating_type = ::fast_io::details::float_alias_type<scalar_type>;
	if constexpr (
		::fast_io::details::floating_scalar_requires_integer_proxy<floating_type>)
	{
		static_assert(sizeof(floating_type) == sizeof(::std::uint_least16_t));
		auto const representation{
			::fast_io::details::capture_bfloat16_representation(
				::std::forward<scalar_type>(value))};
		return ::fast_io::manipulators::floating_scalar_field_manip_t<
			flags, floating_type>{
			representation};
	}
	else
	{
		return ::fast_io::manipulators::scalar_manip_t<flags, floating_type>{
			static_cast<floating_type>(value)};
	}
}

template <::fast_io::manipulators::scalar_flags flags, typename scalar_type>
	requires ::fast_io::details::my_floating_point<
		::std::remove_cvref_t<scalar_type>>
[[nodiscard]] inline constexpr auto
make_floating_scalar_manip_precision(
	scalar_type &&value, ::std::size_t precision) noexcept
{
	using floating_type = ::fast_io::details::float_alias_type<scalar_type>;
	if constexpr (
		::fast_io::details::floating_scalar_requires_integer_proxy<floating_type>)
	{
		static_assert(sizeof(floating_type) == sizeof(::std::uint_least16_t));
		auto const representation{
			::fast_io::details::capture_bfloat16_representation(
				::std::forward<scalar_type>(value))};
		return ::fast_io::manipulators::floating_scalar_field_manip_precision_t<
			flags, floating_type>{
			representation, precision};
	}
	else
	{
		return ::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>{static_cast<floating_type>(value), precision};
	}
}

} // namespace details

namespace manipulators
{

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto rounding(scalar_manip_t<flags, T> value) noexcept
{
	return scalar_manip_t<::fast_io::details::floating_rounding_mani_flags_cache<flags, rounding_policy>, T>{
		value.reference};
}

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto rounding(scalar_manip_precision_t<flags, T> value) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::floating_rounding_mani_flags_cache<flags, rounding_policy>, T>{
		value.reference, value.precision};
}

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto
rounding(floating_scalar_field_manip_t<flags, T> value) noexcept
{
	return floating_scalar_field_manip_t<
		::fast_io::details::floating_rounding_mani_flags_cache<
			flags, rounding_policy>,
		T>{value.representation};
}

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto
rounding(floating_scalar_field_manip_precision_t<flags, T> value) noexcept
{
	return floating_scalar_field_manip_precision_t<
		::fast_io::details::floating_rounding_mani_flags_cache<
			flags, rounding_policy>,
		T>{value.representation, value.precision};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto json_float(scalar_manip_t<flags, T> value) noexcept
{
	return scalar_manip_t<::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{value.reference};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto json_float(scalar_manip_precision_t<flags, T> value) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{
		value.reference, value.precision};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto
json_float(floating_scalar_field_manip_t<flags, T> value) noexcept
{
	return floating_scalar_field_manip_t<
		::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{
		value.representation};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto
json_float(floating_scalar_field_manip_precision_t<flags, T> value) noexcept
{
	return floating_scalar_field_manip_precision_t<
		::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{
		value.representation, value.precision};
}

template <floating_rounding rounding_policy, bool allow_leading_plus = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto rounding_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_rounding_mani_flags_cache<
								  ::fast_io::manipulators::floating_point_default_scalar_flags, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <scalar_placement flags, typename T>
struct width_t
{
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
};

template <scalar_placement flags, typename T, ::std::integral ch_type>
struct width_ch_t
{
	using manip_tag = manip_tag_t;
	using char_type = ch_type;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
	char_type ch;
};

template <typename T>
struct width_runtime_t
{
	using manip_tag = manip_tag_t;
	scalar_placement placement;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
};

template <typename T, ::std::integral ch_type>
struct width_runtime_ch_t
{
	using manip_tag = manip_tag_t;
	using char_type = ch_type;
	scalar_placement placement;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
	char_type ch;
};

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::scalar_integrals<scalar_type>))
inline constexpr auto base(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, false, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::character_integral<scalar_type>))
inline constexpr auto base(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, false, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::scalar_integrals<scalar_type>))
inline constexpr auto baseupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, true, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::character_integral<scalar_type>))
inline constexpr auto baseupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, true, shbase, full, false, false, oct_c2y>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hexupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hexupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, full>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex0x(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex0x(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex0xupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, true, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex0xupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, true, true, full>>(
		t);
}

template <bool shbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto uhexfull(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, true>>(
		static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto addrvw(scalar_type t) noexcept
{
	if constexpr (::fast_io::details::my_signed_integral<scalar_type>)
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
	}
}

template <bool uppercase = false, typename scalar_type>
	requires((::std::is_pointer_v<scalar_type> || ::std::contiguous_iterator<scalar_type>) &&
			 (!::std::is_function_v<::std::remove_cvref_t<scalar_type>>))
inline constexpr auto pointervw(scalar_type t) noexcept
{
	// Object pointers and contiguous iterators: print the address value, not pointee text.
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_function_v<scalar_type>)
inline constexpr auto funcvw(scalar_type *t) noexcept
{
	// Function pointer entry addresses are kept separate from object/iterator addresses.
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(::std::bit_cast<::std::size_t>(t));
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_member_object_pointer_v<scalar_type>)
inline constexpr auto fieldptrvw(scalar_type t) noexcept
{
	// Pointer-to-member fields are ABI encodings, not object addresses.
	if constexpr (sizeof(t) == sizeof(::fast_io::manipulators::member_function_pointer_holder_t))
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::fast_io::manipulators::member_function_pointer_holder_t>(t));
	}
	else
	{
		using equivalentsizetype = ::std::conditional_t<
			sizeof(scalar_type) == sizeof(::std::size_t), ::std::size_t,
			::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least64_t),
								 ::std::uint_least64_t,
								 ::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least32_t),
													  ::std::uint_least32_t,
													  ::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least16_t),
																		   ::std::uint_least16_t,
																		   ::std::uint_least8_t>>>>;
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(::std::bit_cast<equivalentsizetype>(t));
	}
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_member_function_pointer_v<scalar_type> && (sizeof(scalar_type) % sizeof(::std::size_t) == 0))
inline constexpr auto methodvw(scalar_type t) noexcept
{
	// Pointer-to-member functions may be multi-word ABI encodings.
	if constexpr (sizeof(scalar_type) == sizeof(::std::size_t))
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::std::size_t>(t));
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::fast_io::manipulators::member_function_pointer_holder_t>(t));
	}
}

template <bool uppercase = false, typename scalar_type>
	requires((::std::is_pointer_v<scalar_type> && !::std::is_function_v<::std::remove_cvref_t<scalar_type>>) ||
			 ::std::integral<scalar_type>)
inline constexpr auto handlevw(scalar_type t) noexcept
{
	if constexpr (::std::integral<scalar_type>)
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<10, false, false, false, false>>(t);
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
	}
}

template <typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto dfvw(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, true, true, true>>(t);
}

template <bool shbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto uhexupperfull(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, true>>(
		static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
}

// Character code units are deliberately separated from the default numeric
// print path even though they satisfy ::std::integral. Use chvw for a character
// view, and dec when the numeric value of a code unit is intended.
template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto dec(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<10, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto dec(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<10, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, bool oct_c2y = false, bool uppercase_showbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, uppercase_showbase, shbase, full, false, false, oct_c2y>>(t);
}

template <bool shbase = false, bool full = false, bool oct_c2y = false, bool uppercase_showbase = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, uppercase_showbase, shbase, full, false, false, oct_c2y>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct0(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<8, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct0(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<8, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct0o(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, false, true, full, false, false, true>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct0o(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, false, true, full, false, false, true>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto bin(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<2, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto bin(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<2, false, shbase, full>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto bin0b(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<2, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto bin0b(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<2, false, true, full>>(
		t);
}

template <scalar_flags flags, typename scalar_type>
	requires(((2 <= flags.base && flags.base <= 36 && (::fast_io::details::scalar_integrals<scalar_type>)) ||
			  (flags.base == 10 &&
			   ::fast_io::details::my_floating_point<scalar_type> &&
			   !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)))
inline constexpr auto scalar_generic(scalar_type t) noexcept
{
	if constexpr (::fast_io::details::my_floating_point<scalar_type>)
	{
		// scalar_generic is the public escape hatch for a complete scalar_flags
		// policy.  Preserve that NTTP for floating inputs just as the integral arm
		// does; substituting the default flags silently discarded formatting,
		// rounding, sign and spelling choices before the IO layer saw them.
		return ::fast_io::details::make_floating_scalar_manip<
			flags>(t);
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<flags>(t);
	}
}

template <bool upper = false>
inline constexpr scalar_manip_t<::fast_io::details::boolalpha_mani_flags_cache<upper>, bool> boolalpha(bool b) noexcept
{
	return {b};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>, precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return hexfloat<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>, precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return hexfloat0x<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>, precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return comma_hexfloat<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>, precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return comma_hexfloat0x<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto decimal(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, manipulators::floating_format::decimal>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, manipulators::floating_format::decimal>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto decimal(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::decimal>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto decimal(scalar_type t, ::std::size_t n) noexcept
{
	return decimal<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::decimal>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t, ::std::size_t n) noexcept
{
	return comma_decimal<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto general(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, manipulators::floating_format::general>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_general(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, manipulators::floating_format::general>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto general(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto general(scalar_type t, ::std::size_t n) noexcept
{
	return general<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_general(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_general(scalar_type t, ::std::size_t n) noexcept
{
	return comma_general<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto fixed(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, manipulators::floating_format::fixed>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, manipulators::floating_format::fixed>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto fixed(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto fixed(scalar_type t, ::std::size_t n) noexcept
{
	return fixed<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t, ::std::size_t n) noexcept
{
	return comma_fixed<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto scientific(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, manipulators::floating_format::scientific>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, manipulators::floating_format::scientific>>(t);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto scientific(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::scientific>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto scientific(scalar_type t, ::std::size_t n) noexcept
{
	return scientific<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t, ::std::size_t n) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::scientific>,
			precision_mode, rounding_policy>>(t, n);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type> &&
			 !::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t, ::std::size_t n) noexcept
{
	return comma_scientific<uppercase, precision_mode, rounding_policy>(t, n);
}

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!defined(__AVX512BF16__) &&                                       \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
// Clang/x86 without AVX512BF16 may retain a bfloat16 object as its exact
// binary32 extension and retruncate it at a by-value inline boundary. These
// source-only overloads win over the generic by-value templates and capture
// the original expression directly into the integer-owned transport. Native
// AVX512BF16 and every other floating type keep the by-value ABI above.
template <bool uppercase = false>
inline constexpr auto hexfloat(::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>>(value);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto hexfloat0x(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
			precision_mode, rounding_policy>>(value, precision);
}

template <bool uppercase = false>
inline constexpr auto decimal(::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, floating_format::decimal>>(value);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto decimal(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, false, floating_format::decimal>,
			precision_mode, rounding_policy>>(value, precision);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_decimal(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, true, floating_format::decimal>,
			precision_mode, rounding_policy>>(value, precision);
}

template <bool uppercase = false>
inline constexpr auto general(::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, floating_format::general>>(value);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto general(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, false, floating_format::general>,
			precision_mode, rounding_policy>>(value, precision);
}

template <bool uppercase = false>
inline constexpr auto fixed(::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, floating_format::fixed>>(value);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto fixed(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, false, floating_format::fixed>,
			precision_mode, rounding_policy>>(value, precision);
}

template <bool uppercase = false>
inline constexpr auto scientific(::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, false, floating_format::scientific>>(value);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto scientific(::fast_io::details::floating_scalar_integer_proxy_source auto &&value, ::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, false, floating_format::scientific>,
			precision_mode, rounding_policy>>(value, precision);
}

template <scalar_flags flags>
	requires(flags.base == 10)
inline constexpr auto scalar_generic(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	// Keep the representation-sensitive bfloat16 capture distinct from policy
	// selection: the integer proxy changes only transport, so it must carry the
	// exact scalar_flags NTTP accepted by the ordinary scalar_generic overload.
	return ::fast_io::details::make_floating_scalar_manip<
		flags>(
		::std::forward<decltype(value)>(value));
}

template <bool uppercase = false>
inline constexpr auto hexfloat0x(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>>(
		::std::forward<decltype(value)>(value));
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto hexfloat(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto hexfloat(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return hexfloat<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto hexfloat0x(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return hexfloat0x<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false>
inline constexpr auto comma_hexfloat(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>>(
		::std::forward<decltype(value)>(value));
}

template <bool uppercase = false>
inline constexpr auto comma_hexfloat0x(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>>(
		::std::forward<decltype(value)>(value));
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_hexfloat(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_hexfloat(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_hexfloat<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_hexfloat0x(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_hexfloat0x(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_hexfloat0x<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false>
inline constexpr auto comma_decimal(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, floating_format::decimal>>(
		::std::forward<decltype(value)>(value));
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto decimal(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return decimal<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_decimal(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_decimal<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false>
inline constexpr auto comma_general(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, floating_format::general>>(
		::std::forward<decltype(value)>(value));
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto general(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return general<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_general(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, true, floating_format::general>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_general(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_general<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false>
inline constexpr auto comma_fixed(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, floating_format::fixed>>(
		::std::forward<decltype(value)>(value));
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto fixed(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return fixed<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_fixed(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, true, floating_format::fixed>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_fixed(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_fixed<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false>
inline constexpr auto comma_scientific(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		::fast_io::details::dcmfloat_mani_flags_cache<
			uppercase, true, floating_format::scientific>>(
		::std::forward<decltype(value)>(value));
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto scientific(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return scientific<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even>
inline constexpr auto comma_scientific(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip_precision<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<
				uppercase, true, floating_format::scientific>,
			precision_mode, rounding_policy>>(
		::std::forward<decltype(value)>(value), precision);
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false>
inline constexpr auto comma_scientific(
	::fast_io::details::floating_scalar_integer_proxy_source auto &&value,
	::std::size_t precision) noexcept
{
	return comma_scientific<uppercase, precision_mode, rounding_policy>(
		::std::forward<decltype(value)>(value), precision);
}
#endif

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto decimal_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::manipulators::floating_point_default_scalar_flags,
								  floating_precision::significant, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto decimal_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::manipulators::floating_point_default_scalar_flags,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto fixed_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::fixed>,
								  floating_precision::fractional, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto fixed_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::fixed>,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto scientific_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::scientific>,
								  floating_precision::fractional, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto scientific_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::scientific>,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <::std::integral inttype>
inline constexpr ::std::remove_cvref_t<inttype> bitfieldvw(inttype v) noexcept
{
	// Copy bit-fields into an ordinary integer value before formatting.
	return v;
}

} // namespace manipulators

namespace details
{

template <::std::integral char_type, ::std::size_t base>
inline constexpr auto generate_base_prefix_array() noexcept
{
	static_assert(2 <= base && base <= 36);
	if constexpr (base < 10)
	{
		// 0[9]0000
		return ::fast_io::freestanding::array<char_type, 4>{
			char_literal_v<u8'0', char_type>, char_literal_v<u8'[', char_type>,
			::fast_io::char_literal_add<char_type>(base), char_literal_v<u8']', char_type>};
	}
	else
	{
		constexpr char8_t decade{static_cast<char8_t>(static_cast<char8_t>(base) / static_cast<char8_t>(10u))},
			unit{static_cast<char8_t>(static_cast<char8_t>(base) % static_cast<char8_t>(10u))};
		return ::fast_io::freestanding::array<char_type, 5>{
			char_literal_v<u8'0', char_type>, char_literal_v<u8'[', char_type>,
			::fast_io::char_literal_add<char_type>(decade), ::fast_io::char_literal_add<char_type>(unit),
			char_literal_v<u8']', char_type>};
	}
}

template <::std::integral char_type, ::std::size_t base>
inline constexpr auto base_prefix_array{generate_base_prefix_array<char_type, base>()};

template <::std::integral char_type, ::std::size_t base, ::std::size_t digits, bool uppercase = false>
	requires((base == 2u && (digits == 4u || digits == 8u)) ||
			 (base == 4u && (digits == 2u || digits == 4u)) ||
			 (base == 8u && (digits == 3u || digits == 4u)) ||
			 ((base == 16u || base == 32u) && digits == 2u))
consteval auto generate_power_of_two_digits_table() noexcept
{
	constexpr ::std::size_t table_size{compile_pow_n<::std::size_t, base, digits>};
	::fast_io::freestanding::array<char_type, table_size * digits> table;
	for (::std::size_t value{}; value != table_size; ++value)
	{
		::std::size_t remaining{value};
		for (::std::size_t position{digits}; position != 0u; --position)
		{
			table[value * digits + position - 1u] = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
				static_cast<char8_t>(remaining % base));
			remaining /= base;
		}
	}
	return table;
}

template <::std::integral char_type, ::std::size_t base, ::std::size_t digits, bool uppercase = false>
alignas(64) static constexpr auto power_of_two_digits_table{
	generate_power_of_two_digits_table<char_type, base, digits, uppercase>()};

template <typename result_type, ::std::integral char_type>
inline constexpr result_type print_reserve_power_of_two_result(char_type *iter) noexcept
{
	if constexpr (::std::same_as<result_type, char_type *>)
	{
		return iter;
	}
	else
	{
		return {iter, {}};
	}
}

/*
Convert one unsigned value whose storage width matches uint_least64_t to
exactly sixteen hexadecimal digits with SSSE3.
The source register is little-endian on every target admitted by this gate.
duplicate_bytes therefore reverses the eight source bytes and places two copies
of each byte in output order.  A four-bit logical right shift within each
16-bit lane makes the original high nibble available in the first byte of every
pair; the alternating masks select that nibble and the original low nibble.
Both are in [0, 15], so the final pshufb is an exact lookup in digit_lookup.
The single memcpy is valid for an unaligned destination and normally becomes
one 16-byte store.

The caller must prove that the representation has sixteen digits.  On the
admitted x86-64 targets, both T and uint_least64_t have 64 value bits, so this
is equivalent to value >= 2^60; without that precondition this fixed-width
transform would emit leading zeroes.  The helper itself is intentionally
ordinary inline: fixed-base and the selected runtime call sites can inline it
without imposing an unconditional force-inline policy.

This is compile-target dispatch, not run-time CPUID dispatch.  __SSSE3__ names
the earliest permitted ISA, while AVX/AVX2 builds may encode the same 128-bit
operations with VEX prefixes.  Raw builtins and compiler vector types avoid a
SIMD-header dependency.  Builtin probes, the non-ARM64EC x86-64 gate, and the
one-byte non-EBCDIC constraint make the byte order and ASCII table proof local
to this implementation.  constexpr, wide-character, EBCDIC, ARM64EC, and
non-x86 callers remain on the generic formatter.
*/
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__arm64ec__) && \
	!defined(_M_ARM64EC) && defined(__SSSE3__) &&                        \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb128) &&                     \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_psrlwi128)
template <bool uppercase, ::std::integral char_type, my_unsigned_integral T>
	requires(sizeof(T) == sizeof(::std::uint_least64_t) &&
			 sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type>)
inline char_type *print_reserve_hexadecimal_16_ssse3(char_type *first, T value) noexcept
{
	using v16qi [[__gnu__::__vector_size__(16)]] = char;
	using v8hi [[__gnu__::__vector_size__(16)]] = short;
	using v2du [[__gnu__::__vector_size__(16)]] = unsigned long long;
	constexpr v16qi duplicate_bytes{7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0};
	constexpr v16qi high_nibble_mask{15, 0, 15, 0, 15, 0, 15, 0,
									 15, 0, 15, 0, 15, 0, 15, 0};
	constexpr v16qi low_nibble_mask{0, 15, 0, 15, 0, 15, 0, 15,
									0, 15, 0, 15, 0, 15, 0, 15};
	constexpr char alpha_a{uppercase ? 65 : 97};
	constexpr v16qi digit_lookup{48, 49, 50, 51, 52, 53, 54, 55,
								 56, 57, alpha_a, static_cast<char>(alpha_a + 1),
								 static_cast<char>(alpha_a + 2), static_cast<char>(alpha_a + 3),
								 static_cast<char>(alpha_a + 4), static_cast<char>(alpha_a + 5)};
	v2du const source{static_cast<unsigned long long>(value), 0u};
	auto const duplicated{__builtin_ia32_pshufb128(
		__builtin_bit_cast(v16qi, source), duplicate_bytes)};
	auto const shifted{__builtin_bit_cast(
		v16qi, __builtin_ia32_psrlwi128(__builtin_bit_cast(v8hi, duplicated), 4))};
	auto const nibbles{(shifted & high_nibble_mask) | (duplicated & low_nibble_mask)};
	auto const digits{__builtin_ia32_pshufb128(digit_lookup, nibbles)};
	__builtin_memcpy(first, __builtin_addressof(digits), sizeof(digits));
	return first + 16u;
}

#endif


template <::std::size_t base, bool uppercase = false, ::std::integral char_type,
		  typename result_type = char_type *, my_unsigned_integral T>
	requires(base == 2u || base == 4u || base == 8u || base == 16u || base == 32u)
inline constexpr result_type print_reserve_power_of_two_main(char_type *first, T value) noexcept
{
	/*
	AArch64 uses bounded table compositions for common short binary, octal, and
	hexadecimal ranges.  Each range check proves the leading width; every table
	copy emits a fixed-width suffix, so their concatenation is the unique radix
	expansion without a leading zero.  The countl_zero/backward writer below is
	the semantic fallback.  Native M4 measurements and the inspected Cortex-A76
	and Neoverse-N2 llvm-mca models support the shared paths, but the latter are
	static scheduling evidence only.
	*/
#if defined(__aarch64__) || defined(_M_ARM64)
	if constexpr (base == 2u)
	{
		if (value < static_cast<T>(65536u)) [[likely]]
		{
			constexpr auto const *table4{power_of_two_digits_table<char_type, base, 4u>.data()};
			constexpr auto const *table8{power_of_two_digits_table<char_type, base, 8u>.data()};
			if (value < static_cast<T>(16u))
			{
				if (value < static_cast<T>(2u))
				{
					*first = ::fast_io::char_literal_add<char_type>(value);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 1u);
				}
				if (value < static_cast<T>(4u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(value >> 1u);
					first[1] = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(1u));
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 2u);
				}
				if (value < static_cast<T>(8u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(value >> 2u);
					first[1] = ::fast_io::char_literal_add<char_type>((value >> 1u) & static_cast<T>(1u));
					first[2] = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(1u));
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 3u);
				}
				::std::size_t const index{static_cast<::std::size_t>(value) * 4u};
				non_overlapped_copy_n(table4 + index, 4u, first);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 4u);
			}
			if (value < static_cast<T>(256u))
			{
				T const high{static_cast<T>(value >> 4u)};
				::std::size_t const low_index{
					static_cast<::std::size_t>(value & static_cast<T>(15u)) * 4u};
				if (high < static_cast<T>(2u))
				{
					first[0] = char_literal_v<u8'1', char_type>;
					non_overlapped_copy_n(table4 + low_index, 4u, first + 1u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 5u);
				}
				if (high < static_cast<T>(4u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(high >> 1u);
					first[1] = ::fast_io::char_literal_add<char_type>(high & static_cast<T>(1u));
					non_overlapped_copy_n(table4 + low_index, 4u, first + 2u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 6u);
				}
				if (high < static_cast<T>(8u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(high >> 2u);
					first[1] = ::fast_io::char_literal_add<char_type>((high >> 1u) & static_cast<T>(1u));
					first[2] = ::fast_io::char_literal_add<char_type>(high & static_cast<T>(1u));
					non_overlapped_copy_n(table4 + low_index, 4u, first + 3u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 7u);
				}
				::std::size_t const index{static_cast<::std::size_t>(value) * 8u};
				non_overlapped_copy_n(table8 + index, 8u, first);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 8u);
			}
			::std::size_t const low_index{
				static_cast<::std::size_t>(value & static_cast<T>(255u)) * 8u};
			T const high{static_cast<T>(value >> 8u)};
			if (high < static_cast<T>(16u))
			{
				if (high < static_cast<T>(2u))
				{
					first[0] = char_literal_v<u8'1', char_type>;
					non_overlapped_copy_n(table8 + low_index, 8u, first + 1u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 9u);
				}
				if (high < static_cast<T>(4u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(high >> 1u);
					first[1] = ::fast_io::char_literal_add<char_type>(high & static_cast<T>(1u));
					non_overlapped_copy_n(table8 + low_index, 8u, first + 2u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 10u);
				}
				if (high < static_cast<T>(8u))
				{
					::std::size_t const high_index{static_cast<::std::size_t>(high) * 4u};
					non_overlapped_copy_n(table4 + high_index + 1u, 3u, first);
					non_overlapped_copy_n(table8 + low_index, 8u, first + 3u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 11u);
				}
				::std::size_t const high_index{static_cast<::std::size_t>(high) * 4u};
				non_overlapped_copy_n(table4 + high_index, 4u, first);
				non_overlapped_copy_n(table8 + low_index, 8u, first + 4u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 12u);
			}
			::std::size_t const high_index{static_cast<::std::size_t>(high) * 8u};
			if (high < static_cast<T>(32u))
			{
				non_overlapped_copy_n(table8 + high_index + 3u, 5u, first);
				non_overlapped_copy_n(table8 + low_index, 8u, first + 5u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 13u);
			}
			if (high < static_cast<T>(64u))
			{
				non_overlapped_copy_n(table8 + high_index + 2u, 6u, first);
				non_overlapped_copy_n(table8 + low_index, 8u, first + 6u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 14u);
			}
			if (high < static_cast<T>(128u))
			{
				non_overlapped_copy_n(table8 + high_index + 1u, 7u, first);
				non_overlapped_copy_n(table8 + low_index, 8u, first + 7u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 15u);
			}
			non_overlapped_copy_n(table8 + high_index, 8u, first);
			non_overlapped_copy_n(table8 + low_index, 8u, first + 8u);
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 16u);
		}
	}
	else if constexpr (base == 8u)
	{
		if (value < static_cast<T>(262144u)) [[likely]]
		{
			if (value < static_cast<T>(512u))
			{
				if (value < static_cast<T>(8u))
				{
					*first = ::fast_io::char_literal_add<char_type>(value);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 1u);
				}
				if (value < static_cast<T>(64u))
				{
					first[0] = ::fast_io::char_literal_add<char_type>(value >> 3u);
					first[1] = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(7u));
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 2u);
				}
				first[0] = ::fast_io::char_literal_add<char_type>(value >> 6u);
				first[1] = ::fast_io::char_literal_add<char_type>((value >> 3u) & static_cast<T>(7u));
				first[2] = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(7u));
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 3u);
			}
			constexpr auto const *table{power_of_two_digits_table<char_type, base, 3u>.data()};
			T const high{static_cast<T>(value >> 9u)};
			::std::size_t const low_index{static_cast<::std::size_t>(value & static_cast<T>(511u)) * 3u};
			if (high < static_cast<T>(8u))
			{
				first[0] = ::fast_io::char_literal_add<char_type>(high);
				non_overlapped_copy_n(table + low_index, 3u, first + 1u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 4u);
			}
			if (high < static_cast<T>(64u))
			{
				first[0] = ::fast_io::char_literal_add<char_type>(high >> 3u);
				first[1] = ::fast_io::char_literal_add<char_type>(high & static_cast<T>(7u));
				non_overlapped_copy_n(table + low_index, 3u, first + 2u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 5u);
			}
			::std::size_t const high_index{static_cast<::std::size_t>(high) * 3u};
			non_overlapped_copy_n(table + high_index, 3u, first);
			non_overlapped_copy_n(table + low_index, 3u, first + 3u);
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 6u);
		}
		/*
		Seven octal digits are the sole Apple-specific power-output case.  The
		bound proves a one-digit prefix plus two padded three-digit table blocks.
		The scoped paired M4 run favored this path by roughly 2x.  In contrast,
		Cortex-A57/A76 and Neoverse-N1/V1 llvm-mca models favored the common
		fallback; those are model results, not native Cortex/Neoverse timings.
		*/
#if defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64))
		if constexpr (::std::numeric_limits<T>::digits > 18u)
		{
			if (value < static_cast<T>(2097152u))
			{
				constexpr auto const *table{power_of_two_digits_table<char_type, base, 3u>.data()};
				T const high{static_cast<T>(value >> 18u)};
				::std::size_t const middle_index{
					static_cast<::std::size_t>((value >> 9u) & static_cast<T>(511u)) * 3u};
				::std::size_t const low_index{
					static_cast<::std::size_t>(value & static_cast<T>(511u)) * 3u};
				first[0] = ::fast_io::char_literal_add<char_type>(high);
				non_overlapped_copy_n(table + middle_index, 3u, first + 1u);
				non_overlapped_copy_n(table + low_index, 3u, first + 4u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 7u);
			}
		}
#endif
	}
	else if constexpr (base == 16u)
	{
		if (value < static_cast<T>(16777216u)) [[likely]]
		{
			if (value < static_cast<T>(256u))
			{
				if (value < static_cast<T>(16u))
				{
					*first = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
						static_cast<char8_t>(value));
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 1u);
				}
				constexpr auto const *table{power_of_two_digits_table<char_type, base, 2u, uppercase>.data()};
				::std::size_t const index{static_cast<::std::size_t>(value) * 2u};
				non_overlapped_copy_n(table + index, 2u, first);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 2u);
			}
			constexpr auto const *table{power_of_two_digits_table<char_type, base, 2u, uppercase>.data()};
			if (value < static_cast<T>(65536u))
			{
				T const high{static_cast<T>(value >> 8u)};
				::std::size_t const low_index{static_cast<::std::size_t>(value & static_cast<T>(255u)) * 2u};
				if (high < static_cast<T>(16u))
				{
					first[0] = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
						static_cast<char8_t>(high));
					non_overlapped_copy_n(table + low_index, 2u, first + 1u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 3u);
				}
				::std::size_t const high_index{static_cast<::std::size_t>(high) * 2u};
				non_overlapped_copy_n(table + high_index, 2u, first);
				non_overlapped_copy_n(table + low_index, 2u, first + 2u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 4u);
			}
			T const high{static_cast<T>(value >> 16u)};
			::std::size_t const middle_index{
				static_cast<::std::size_t>((value >> 8u) & static_cast<T>(255u)) * 2u};
			::std::size_t const low_index{static_cast<::std::size_t>(value & static_cast<T>(255u)) * 2u};
			if (high < static_cast<T>(16u))
			{
				first[0] = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
					static_cast<char8_t>(high));
				non_overlapped_copy_n(table + middle_index, 2u, first + 1u);
				non_overlapped_copy_n(table + low_index, 2u, first + 3u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 5u);
			}
			::std::size_t const high_index{static_cast<::std::size_t>(high) * 2u};
			non_overlapped_copy_n(table + high_index, 2u, first);
			non_overlapped_copy_n(table + middle_index, 2u, first + 2u);
			non_overlapped_copy_n(table + low_index, 2u, first + 4u);
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 6u);
		}
		constexpr ::std::size_t aarch64_type_bits{::std::numeric_limits<T>::digits};
		if constexpr (aarch64_type_bits > 24u)
		{
			bool within_short_range{true};
			if constexpr (aarch64_type_bits > 40u)
			{
				within_short_range = value < static_cast<T>(1099511627776u);
			}
			if (within_short_range)
			{
				constexpr auto const *table{power_of_two_digits_table<char_type, base, 2u, uppercase>.data()};
				::std::size_t const index0{
					static_cast<::std::size_t>(value & static_cast<T>(255u)) * 2u};
				::std::size_t const index1{
					static_cast<::std::size_t>((value >> 8u) & static_cast<T>(255u)) * 2u};
				::std::size_t const index2{
					static_cast<::std::size_t>((value >> 16u) & static_cast<T>(255u)) * 2u};
				if constexpr (aarch64_type_bits > 32u)
				{
					if (value >= static_cast<T>(4294967296u))
					{
						T const high{static_cast<T>(value >> 32u)};
						::std::size_t const index3{
							static_cast<::std::size_t>((value >> 24u) & static_cast<T>(255u)) * 2u};
						if (high < static_cast<T>(16u))
						{
							first[0] = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
								static_cast<char8_t>(high));
							non_overlapped_copy_n(table + index3, 2u, first + 1u);
							non_overlapped_copy_n(table + index2, 2u, first + 3u);
							non_overlapped_copy_n(table + index1, 2u, first + 5u);
							non_overlapped_copy_n(table + index0, 2u, first + 7u);
							return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 9u);
						}
						::std::size_t const high_index{static_cast<::std::size_t>(high) * 2u};
						non_overlapped_copy_n(table + high_index, 2u, first);
						non_overlapped_copy_n(table + index3, 2u, first + 2u);
						non_overlapped_copy_n(table + index2, 2u, first + 4u);
						non_overlapped_copy_n(table + index1, 2u, first + 6u);
						non_overlapped_copy_n(table + index0, 2u, first + 8u);
						return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 10u);
					}
				}
				T const high{static_cast<T>(value >> 24u)};
				if (high < static_cast<T>(16u))
				{
					first[0] = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
						static_cast<char8_t>(high));
					non_overlapped_copy_n(table + index2, 2u, first + 1u);
					non_overlapped_copy_n(table + index1, 2u, first + 3u);
					non_overlapped_copy_n(table + index0, 2u, first + 5u);
					return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 7u);
				}
				::std::size_t const high_index{static_cast<::std::size_t>(high) * 2u};
				non_overlapped_copy_n(table + high_index, 2u, first);
				non_overlapped_copy_n(table + index2, 2u, first + 2u);
				non_overlapped_copy_n(table + index1, 2u, first + 4u);
				non_overlapped_copy_n(table + index0, 2u, first + 6u);
				return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 8u);
			}
		}
	}
	else if constexpr (base == 32u)
	{
		if (value < static_cast<T>(32u)) [[likely]]
		{
			*first = ::fast_io::details::charliteralofnumber<char_type, uppercase>(static_cast<char8_t>(value));
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(first + 1u);
		}
	}
#endif
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__arm64ec__) && \
	!defined(_M_ARM64EC) && defined(__SSSE3__) &&                        \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb128) &&                     \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_psrlwi128)
	if constexpr (base == 16u && sizeof(T) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type>)
	{
		/*
		The threshold is the no-leading-zero proof: every admitted value has
		exactly sixteen digits, so the fixed-width store returns first + 16
		without a trimming pass.  In the accepted Core i9-14900HX paired corpus
		(-O3 -march=native), the exact u64/16-digit char and char8_t points were
		2.74x--5.00x faster than the former scalar graph across GCC 13--16 and
		Clang 18--21.  The helper's adjacent lane proof accounts for the bounded
		shuffle/shift/store instruction graph; llvm-mca was not used to extend
		that native result to an unmeasured processor.

		SSSE3-capable x86-64 compilers and cores outside that matrix inherit the
		same semantically proved ISA route because this header does not dispatch
		by microarchitecture, but they inherit no numeric throughput claim.
		Smaller values, constant evaluation, wide or EBCDIC output, ARM64EC, and
		targets lacking the required builtins retain the generic scalar/table
		formatter.
		*/
		if (!::std::is_constant_evaluated() && value >= (static_cast<T>(1u) << 60u))
		{
			auto *const iter{
				::fast_io::details::print_reserve_hexadecimal_16_ssse3<uppercase>(first, value)};
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(iter);
		}
	}
#endif
	constexpr ::std::size_t type_bits{::std::numeric_limits<T>::digits};
	::std::size_t const bit_length{
		type_bits - static_cast<::std::size_t>(::std::countl_zero(static_cast<T>(value | static_cast<T>(1u))))};
	constexpr ::std::size_t bits_per_digit{
		base == 2u ? 1u : (base == 4u ? 2u : (base == 8u ? 3u : (base == 16u ? 4u : 5u)))};
	::std::size_t const length{(bit_length - 1u) / bits_per_digit + 1u};
	char_type *const last{first + length};
	char_type *iter{last};
	if constexpr (base == 2u)
	{
		constexpr ::std::size_t digits_per_iteration{8u};
		constexpr ::std::size_t bits_per_iteration{8u};
		constexpr T mask{static_cast<T>((1u << bits_per_iteration) - 1u)};
		constexpr auto const *table{power_of_two_digits_table<char_type, base, digits_per_iteration>.data()};
		if constexpr (type_bits > bits_per_iteration * 4u)
		{
			// The body already processes four independent table blocks.  These
			// compiler-specific pragmas request the same no-further-unroll layout;
			// they do not affect digits, stores, or the loop termination proof.  A
			// paired Apple-Clang/M4 removal scored 0.9914x for binary inputs of at
			// least 33 digits, so compiler-selected additional unrolling is not
			// retained.  These directives preserve the pre-existing Clang/GCC layout
			// policy; no GCC or non-M4 speedup is inferred from that M4 measurement.
#if defined(__clang__)
#pragma clang loop unroll(disable)
#elif defined(__GNUC__)
#pragma GCC unroll 0
#endif
			while (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 4u)))
			{
				::std::size_t const index0{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const index1{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				::std::size_t const index2{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 2u)) & mask) * digits_per_iteration};
				::std::size_t const index3{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 3u)) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 4u;
				iter -= digits_per_iteration * 4u;
				non_overlapped_copy_n(table + index3, digits_per_iteration, iter);
				non_overlapped_copy_n(table + index2, digits_per_iteration, iter + digits_per_iteration);
				non_overlapped_copy_n(table + index1, digits_per_iteration, iter + digits_per_iteration * 2u);
				non_overlapped_copy_n(table + index0, digits_per_iteration, iter + digits_per_iteration * 3u);
			}
		}
		if constexpr (type_bits > bits_per_iteration * 2u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 2u)))
			{
				::std::size_t const low_index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const high_index{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 2u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + low_index, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + high_index, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << bits_per_iteration))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				value >>= bits_per_iteration;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index, digits_per_iteration, iter);
			}
		}
		if (value >= static_cast<T>(16u))
		{
			constexpr ::std::size_t tail_digits{4u};
			constexpr T tail_mask{static_cast<T>(15u)};
			constexpr auto const *tail_table{power_of_two_digits_table<char_type, base, tail_digits>.data()};
			::std::size_t const index{static_cast<::std::size_t>(value & tail_mask) * tail_digits};
			value >>= 4u;
			iter -= tail_digits;
			non_overlapped_copy_n(tail_table + index, tail_digits, iter);
		}
		do
		{
			*--iter = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(1u));
			value >>= 1u;
		} while (value != 0u);
	}
	else if constexpr (base == 4u)
	{
		constexpr ::std::size_t digits_per_iteration{4u};
		constexpr ::std::size_t bits_per_iteration{8u};
		constexpr T mask{static_cast<T>((1u << bits_per_iteration) - 1u)};
		constexpr auto const *table{power_of_two_digits_table<char_type, base, digits_per_iteration>.data()};
		if constexpr (type_bits > bits_per_iteration * 4u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 4u)))
			{
				::std::size_t const index0{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const index1{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				::std::size_t const index2{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 2u)) & mask) * digits_per_iteration};
				::std::size_t const index3{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 3u)) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 4u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index0, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index1, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index2, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index3, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration * 2u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 2u)))
			{
				::std::size_t const low_index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const high_index{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 2u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + low_index, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + high_index, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << bits_per_iteration))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				value >>= bits_per_iteration;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index, digits_per_iteration, iter);
			}
		}
		if (value >= static_cast<T>(16u))
		{
			constexpr ::std::size_t tail_digits{2u};
			constexpr T tail_mask{static_cast<T>(15u)};
			constexpr auto const *tail_table{power_of_two_digits_table<char_type, base, tail_digits>.data()};
			::std::size_t const index{static_cast<::std::size_t>(value & tail_mask) * tail_digits};
			value >>= 4u;
			iter -= tail_digits;
			non_overlapped_copy_n(tail_table + index, tail_digits, iter);
		}
		do
		{
			*--iter = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(3u));
			value >>= 2u;
		} while (value != 0u);
	}
	else if constexpr (base == 8u)
	{
		constexpr ::std::size_t digits_per_iteration{4u};
		constexpr ::std::size_t bits_per_iteration{12u};
		constexpr T mask{static_cast<T>((1u << bits_per_iteration) - 1u)};
		constexpr auto const *table{power_of_two_digits_table<char_type, base, digits_per_iteration>.data()};
		if constexpr (type_bits > bits_per_iteration * 4u)
		{
			// Preserve the same four-block software-unroll unit for octal.  Removing
			// the directive was neutral/mixed for octal inputs of at least 17 digits
			// (1.0024x) but the combined binary/octal Apple-Clang/M4 gate was
			// 0.9957x, so one bounded layout policy is retained for both loops.  The
			// directives preserve the pre-existing Clang/GCC layout; no GCC or
			// non-M4 speedup is inferred from that M4 measurement.
#if defined(__clang__)
#pragma clang loop unroll(disable)
#elif defined(__GNUC__)
#pragma GCC unroll 0
#endif
			while (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 4u)))
			{
				::std::size_t const index0{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const index1{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				::std::size_t const index2{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 2u)) & mask) * digits_per_iteration};
				::std::size_t const index3{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 3u)) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 4u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index0, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index1, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index2, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index3, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration * 2u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 2u)))
			{
				::std::size_t const low_index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const high_index{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 2u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + low_index, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + high_index, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << bits_per_iteration))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				value >>= bits_per_iteration;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > 9u)
		{
			if (value >= static_cast<T>(512u))
			{
				constexpr ::std::size_t tail_digits{3u};
				constexpr T tail_mask{static_cast<T>(511u)};
				constexpr auto const *tail_table{power_of_two_digits_table<char_type, base, tail_digits>.data()};
				::std::size_t const index{static_cast<::std::size_t>(value & tail_mask) * tail_digits};
				value >>= 9u;
				iter -= tail_digits;
				non_overlapped_copy_n(tail_table + index, tail_digits, iter);
			}
		}
		do
		{
			*--iter = ::fast_io::char_literal_add<char_type>(value & static_cast<T>(7u));
			value >>= 3u;
		} while (value != 0u);
	}
	else
	{
		constexpr ::std::size_t digits_per_iteration{2u};
		constexpr ::std::size_t bits_per_iteration{base == 16u ? 8u : 10u};
		constexpr T mask{static_cast<T>((static_cast<T>(1u) << bits_per_iteration) - 1u)};
		constexpr auto const *table{
			power_of_two_digits_table<char_type, base, digits_per_iteration, uppercase>.data()};
		if constexpr (type_bits > bits_per_iteration * 4u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 4u)))
			{
				::std::size_t const index0{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const index1{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				::std::size_t const index2{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 2u)) & mask) * digits_per_iteration};
				::std::size_t const index3{
					static_cast<::std::size_t>((value >> (bits_per_iteration * 3u)) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 4u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index0, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index1, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index2, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index3, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration * 2u)
		{
			if (value >= static_cast<T>(static_cast<T>(1u) << (bits_per_iteration * 2u)))
			{
				::std::size_t const low_index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				::std::size_t const high_index{
					static_cast<::std::size_t>((value >> bits_per_iteration) & mask) * digits_per_iteration};
				value >>= bits_per_iteration * 2u;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + low_index, digits_per_iteration, iter);
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + high_index, digits_per_iteration, iter);
			}
		}
		if constexpr (type_bits > bits_per_iteration)
		{
			/*
			The comparison is a width proof: after the wider blocks have been removed,
			it is true exactly when another fixed-width table pair is required.  Do not
			attach a probability attribute here.  Apple Clang 21 propagated the former
			AArch64 [[likely]] hint far enough backwards to outline the value >= 2^40
			hexadecimal path into a cold helper.  Values with 11--16 hexadecimal digits
			then paid for a non-leaf call, frame setup, and a spilled result pointer.
			Removing the hint kept the same operations inline and improved every
			round of the isolated M4 matrix.  The earlier isolated complete-core
			symbol audit measured 900 to 852 bytes on Apple M4 and 912 to 864 bytes
			for Cortex-A76 and Neoverse-N2.  The final dead-stripped linked-root audit
			measured the Apple-Clang internal fixed-base root at 896 to 844 bytes and
			the public literal-base root at 1836 to 1816 bytes.  These are distinct
			measurement scopes; the Cortex and Neoverse figures are static
			code-generation evidence, not native timing.
			*/
			if (value >= static_cast<T>(static_cast<T>(1u) << bits_per_iteration))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & mask) * digits_per_iteration};
				value >>= bits_per_iteration;
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index, digits_per_iteration, iter);
			}
		}
		/*
		After all fixed pairs are removed, hexadecimal leaves either one digit or
		one two-digit table entry.  The value < 16 comparison proves the former
		needs no leading zero.  AArch64 spells this tail explicitly; the generic
		backward loop is algebraically equivalent.  Replacing it with that loop
		scored 0.9912x over base 16 on paired M4 runs (0.9748x/0.9710x for
		u64/i64), so the explicit tail is retained.  This is M4 evidence, not native
		Cortex/Neoverse timing.
		*/
#if defined(__aarch64__) || defined(_M_ARM64)
		if constexpr (base == 16u)
		{
			if (value < static_cast<T>(16u)) [[likely]]
			{
				*--iter = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
					static_cast<char8_t>(value));
			}
			else
			{
				::std::size_t const index{static_cast<::std::size_t>(value) * digits_per_iteration};
				iter -= digits_per_iteration;
				non_overlapped_copy_n(table + index, digits_per_iteration, iter);
			}
			return ::fast_io::details::print_reserve_power_of_two_result<result_type>(last);
		}
#endif
		do
		{
			*--iter = ::fast_io::details::charliteralofnumber<char_type, uppercase>(
				static_cast<char8_t>(value & static_cast<T>(base - 1u)));
			value >>= bits_per_digit;
		} while (value != 0u);
	}
	return ::fast_io::details::print_reserve_power_of_two_result<result_type>(last);
}


template <::std::size_t base, bool uppercase_showbase, bool oct_c2y, ::std::integral char_type>
inline constexpr char_type *print_reserve_show_base_impl(char_type *iter)
{
	static_assert(2 <= base && base <= 36);
	if constexpr (base == 2)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0B", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0B", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0B", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0B", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0B", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0b", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0b", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0b", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0b", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0b", iter);
			}
		}
	}
	else if constexpr (base == 3)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0T", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0T", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0T", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0T", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0T", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0t", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0t", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0t", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0t", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0t", iter);
			}
		}
	}
	else if constexpr (base == 8)
	{
		if constexpr (oct_c2y)
		{
			if constexpr (uppercase_showbase)
			{
				if constexpr (::std::same_as<char_type, char>)
				{
					iter = copy_string_literal("0O", iter);
				}
				else if constexpr (::std::same_as<char_type, wchar_t>)
				{
					iter = copy_string_literal(L"0O", iter);
				}
				else if constexpr (::std::same_as<char_type, char16_t>)
				{
					iter = copy_string_literal(u"0O", iter);
				}
				else if constexpr (::std::same_as<char_type, char32_t>)
				{
					iter = copy_string_literal(U"0O", iter);
				}
				else
				{
					iter = copy_string_literal(u8"0O", iter);
				}
			}
			else
			{
				if constexpr (::std::same_as<char_type, char>)
				{
					iter = copy_string_literal("0o", iter);
				}
				else if constexpr (::std::same_as<char_type, wchar_t>)
				{
					iter = copy_string_literal(L"0o", iter);
				}
				else if constexpr (::std::same_as<char_type, char16_t>)
				{
					iter = copy_string_literal(u"0o", iter);
				}
				else if constexpr (::std::same_as<char_type, char32_t>)
				{
					iter = copy_string_literal(U"0o", iter);
				}
				else
				{
					iter = copy_string_literal(u8"0o", iter);
				}
			}
		}
		else
		{
			*iter = char_literal_v<u8'0', char_type>;
			++iter;
		}
	}
	else if constexpr (base == 16)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0X", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0X", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0X", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0X", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0X", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0x", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0x", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0x", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0x", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0x", iter);
			}
		}
	}
	else if constexpr (base != 10)
	{
		constexpr auto arr{base_prefix_array<char_type, base>};
		constexpr ::std::size_t sz{arr.size()};
		iter = non_overlapped_copy_n(arr.data(), sz, iter);
	}
	return iter;
}

template <::std::size_t base, bool uppercase, bool ryu_mode = false, ::std::integral char_type, typename T>
inline constexpr void print_reserve_integral_main_impl(char_type *iter, T t, ::std::size_t len)
{
	if constexpr (base <= 10 && uppercase)
	{
		print_reserve_integral_main_impl<base, false, ryu_mode>(iter, t, len); // prevent duplications
	}
	else if constexpr (need_seperate_print<T>)
	{
		// A split carrier changes only the arithmetic width of each formatting
		// step.  Its high and low halves are still digits of the same field, so
		// dropping `uppercase` in either recursive call makes wide integers use
		// lowercase A-Z while their prefix and narrow peers remain uppercase.
		// Bases at or below ten have already canonicalized this flag above; for
		// alphabetic bases it must be propagated unchanged to every half.
		constexpr ::std::size_t basetdigits{::fast_io::details::cal_max_int_size<T, base>()};
		constexpr ::std::size_t sizetdigits{::fast_io::details::cal_max_int_size<optimal_print_unsigned_type, base>()};
		static_assert(basetdigits != 0 && sizetdigits != 0);
		if constexpr (base == 2 || base == 4 || base == 16)
		{
			optimal_print_unsigned_type high;
			optimal_print_unsigned_type low{
				::fast_io::details::intrinsics::unpack_generic<T, optimal_print_unsigned_type>(t, high)};
			if (len > sizetdigits)
			{
				print_reserve_integral_main_impl<base, uppercase, false>(iter - sizetdigits, high, len - sizetdigits);
				len = sizetdigits;
			}
			print_reserve_integral_main_impl<base, uppercase, false>(iter, low, len);
		}
		else
		{
			constexpr ::std::size_t sizetdigitsm1{sizetdigits - 1};
			constexpr ::std::size_t remain_digits{basetdigits - sizetdigitsm1 * 2};
			constexpr T maxhighdigits{compile_pow_n<T, base, (sizetdigitsm1 * 2)>};
			if constexpr (!ryu_mode && remain_digits != 0)
			{
				static_assert(remain_digits < 3);
				if constexpr (remain_digits == 1)
				{
					if (len == basetdigits)
					{
						T high{t / maxhighdigits};
						t = t % maxhighdigits;
						if constexpr (base <= 10)
						{
							*(iter - basetdigits) = ::fast_io::char_literal_add<char_type>(high);
						}
						else
						{
							constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
							*(iter - basetdigits) =
								static_cast<char_type>(tb[(static_cast<::std::size_t>(high) << 1u) + 1u]);
						}
						--len;
					}
				}
				else
				{
					constexpr ::std::size_t least_digits{basetdigits - remain_digits};
					if (len > least_digits)
					{
						T high{t / maxhighdigits};
						t = t % maxhighdigits;
						::std::size_t rem{static_cast<::std::size_t>(high)};
						if (len == basetdigits)
						{
							constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
							non_overlapped_copy_n(tb + (rem << 1), 2, iter - basetdigits);
							len -= 2u;
						}
						else
						{
							if constexpr (base <= 10)
							{
								*(iter + 1 - basetdigits) = ::fast_io::char_literal_add<char_type>(high);
							}
							else
							{
								constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
								*(iter + 1 - basetdigits) =
									static_cast<char_type>(tb[(static_cast<::std::size_t>(high) << 1u) + 1u]);
							}
							--len;
						}
					}
				}
			}
			optimal_print_unsigned_type low;
			if (len > sizetdigitsm1)
			{
				constexpr T halfdigits{compile_pow_n<T, base, sizetdigitsm1>};
				optimal_print_unsigned_type high{static_cast<optimal_print_unsigned_type>(t / halfdigits)};
				low = static_cast<optimal_print_unsigned_type>(t % halfdigits);
				print_reserve_integral_main_impl<base, uppercase, false>(iter - sizetdigitsm1, high, len - sizetdigitsm1);
				len = sizetdigitsm1;
			}
			else
			{
				low = static_cast<optimal_print_unsigned_type>(t);
			}
			print_reserve_integral_main_impl<base, uppercase, false>(iter, low, len);
		}
	}
	else
	{
		if constexpr (ryu_mode)
		{
			print_reserve_integral_main_impl<base, uppercase, false>(iter, t, len);
		}
		else
		{
			constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
			constexpr T pw{static_cast<T>(base * base)};
			::std::size_t const len2{len >> static_cast<::std::size_t>(1u)};
			for (::std::size_t i{}; i != len2; ++i)
			{
				auto const rem{t % pw};
				t /= pw;
				non_overlapped_copy_n(tb + (rem << 1), 2, iter -= 2);
			}
			if ((len & 1))
			{
				if constexpr (base <= 10)
				{
					*--iter = ::fast_io::char_literal_add<char_type>(t);
				}
				else
				{
					*--iter = static_cast<char_type>(tb[(t << 1) + 1]);
				}
			}
		}
	}
}

template <bool full, ::std::size_t base, bool uppercase, ::std::integral char_type, my_unsigned_integral T>
inline constexpr char_type *print_reserve_integral_withfull_main_impl(char_type *first, T u)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_withfull_main_impl<full, base, false>(first, u); // prevent duplications
	}
	else
	{
		if constexpr (full)
		{
			constexpr ::std::size_t sz{::fast_io::details::cal_max_int_size<T, base>()};
			if constexpr (sizeof(u) <= sizeof(unsigned))
			{
				print_reserve_integral_main_impl<base, uppercase>(first += sz, static_cast<unsigned>(u), sz);
			}
			else
			{
				print_reserve_integral_main_impl<base, uppercase>(first += sz, u, sz);
			}
			return first;
		}
		else
		{
			if constexpr ((base == 2u || base == 4u || base == 8u || base == 16u || base == 32u) &&
						  !need_seperate_print<T>)
			{
				return print_reserve_power_of_two_main<base, uppercase>(first, u);
			}
			else if constexpr (base == 10 && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
			{
				/*
				The decimal vector writer requires IFMA, VBMI, BW, and VL at compile
				time.  Its nested constraint proves one-byte ASCII-compatible output
				from an unsigned type whose storage width matches uint_least64_t, and
				constant evaluation remains on JEAIII.  SDE correctness and assembly
				were checked; no native AVX-512 throughput is claimed here.
				*/
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
				if constexpr ((::std::same_as<char_type, char8_t> ||
							   (::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type>)) &&
							  sizeof(T) == sizeof(::std::uint_least64_t))
				{
					if (!::std::is_constant_evaluated())
					{
						return ::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
							first, static_cast<::std::uint_least64_t>(u));
					}
				}
#endif
				if constexpr (false)
				{
					return ::fast_io::details::uprsv::uprsv_main<base, uppercase>(first, u);
				}
				else
				{
					return ::fast_io::details::jeaiii::jeaiii_main(first, u);
				}
			}
			else
			{
				::std::size_t const sz{chars_len<base, false>(u)};
				if constexpr (sizeof(u) <= sizeof(unsigned))
				{
					print_reserve_integral_main_impl<base, uppercase>(first += sz, static_cast<unsigned>(u), sz);
				}
				else
				{
					print_reserve_integral_main_impl<base, uppercase>(first += sz, u, sz);
				}
				return first;
			}
		}
	}
}

template <::std::size_t base, bool uppercase, ::std::integral char_type, my_unsigned_integral T>
inline constexpr void print_reserve_integral_withfull_precise_main_impl(char_type *last, T u, ::std::size_t n)
{

	if constexpr (sizeof(u) <= sizeof(unsigned) && sizeof(unsigned) <= sizeof(::std::size_t))
	{
		print_reserve_integral_main_impl<base, uppercase>(last, static_cast<unsigned>(u), n);
	}
	else
	{
		print_reserve_integral_main_impl<base, uppercase>(last, u, n);
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool oct_c2y = false, typename int_type, ::std::integral char_type>
inline constexpr char_type *print_reserve_integral_define(char_type *first, int_type t)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, false, full, oct_c2y>(
			first, t); // prevent duplications
	}
	else
	{
		static_assert((2 <= base) && (base <= 36));
		if constexpr (::std::same_as<bool, ::std::remove_cvref_t<int_type>>)
		{
			if constexpr (showpos)
			{
				*first = char_literal_v<u8'+', char_type>;
				++first;
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			*first = t ? char_literal_v<u8'1', char_type> : char_literal_v<u8'0', char_type>;
			++first;
			return first;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<int_type>;
			unsigned_type u{static_cast<unsigned_type>(t)};
			if constexpr (showpos)
			{
				if constexpr (::fast_io::details::my_unsigned_integral<int_type>)
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				else
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
					else
					{
						*first = char_literal_v<u8'+', char_type>;
					}
				}
				++first;
			}
			else
			{
				if constexpr (::fast_io::details::my_signed_integral<int_type>)
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						++first;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
				}
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			return print_reserve_integral_withfull_main_impl<full, base, uppercase>(first, u);
		}
	}
}

/// Selects one explicitly converted execution character without assuming that
/// the lowercase and uppercase alphabets are arithmetically related.
template <bool uppercase, char8_t lowercase_character,
		  char8_t uppercase_character, ::std::integral char_type>
inline constexpr char_type compiler_constant_case_literal_v{[]() constexpr {
	if constexpr (uppercase)
	{
		return ::fast_io::char_literal_v<uppercase_character, char_type>;
	}
	else
	{
		return ::fast_io::char_literal_v<lowercase_character, char_type>;
	}
}()};

/// @brief Writes a base prefix using semantic execution characters only.
/// @details The established formatter uses native string literals for a few
///          two-character prefixes.  GCC can pack a one-byte wide execution
///          charset into those literals, so a compiler-constant proxy needs an
///          explicit code-unit spelling to remain correct for wide EBCDIC.
template <::std::size_t base, bool uppercase_showbase, bool modern_octal,
		  ::std::integral char_type>
	requires(2u <= base && base <= 36u)
inline constexpr char_type *
print_compiler_constant_show_base_define(char_type *iter) noexcept
{
	if constexpr (base != 10u)
	{
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
	}
	if constexpr (base == 2u)
	{
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase_showbase, u8'b', u8'B', char_type>;
	}
	else if constexpr (base == 3u)
	{
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase_showbase, u8't', u8'T', char_type>;
	}
	else if constexpr (base == 8u)
	{
		if constexpr (modern_octal)
		{
			*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
				uppercase_showbase, u8'o', u8'O', char_type>;
		}
	}
	else if constexpr (base == 16u)
	{
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase_showbase, u8'x', u8'X', char_type>;
	}
	else if constexpr (base != 10u)
	{
		*iter++ = ::fast_io::char_literal_v<u8'[', char_type>;
		if constexpr (base < 10u)
		{
			*iter++ = ::fast_io::char_literal_add<char_type>(base);
		}
		else
		{
			*iter++ = ::fast_io::char_literal_add<char_type>(base / 10u);
			*iter++ = ::fast_io::char_literal_add<char_type>(base % 10u);
		}
		*iter++ = ::fast_io::char_literal_v<u8']', char_type>;
	}
	return iter;
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *print_compiler_constant_boolalpha_define(
	char_type *iter, bool value) noexcept
{
	if (value)
	{
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8't', u8'T', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'r', u8'R', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'u', u8'U', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'e', u8'E', char_type>;
	}
	else
	{
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'f', u8'F', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'a', u8'A', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'l', u8'L', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8's', u8'S', char_type>;
		*iter++ = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'e', u8'E', char_type>;
	}
	return iter;
}

/// @brief Emits one optimizer-proven constant integer without entering the run-time decimal algorithm.
/// @details This is intentionally a sibling of `print_reserve_integral_define`, not a branch inside it.  The public
///          print/concat strategy reaches this helper only through `compiler_constant_scalar_manip_t`; an unknown value
///          therefore instantiates and executes the original formatter with no extra test or altered inlining body.
template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool oct_c2y = false, typename int_type, ::std::integral char_type>
inline constexpr char_type *
print_reserve_integral_compiler_constant_define(char_type *first, int_type t)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_compiler_constant_define<base, showbase, uppercase_showbase, showpos,
															   false, full, oct_c2y>(first, t);
	}
	else
	{
		static_assert((2 <= base) && (base <= 36));
		if constexpr (::std::same_as<bool, ::std::remove_cvref_t<int_type>>)
		{
			if constexpr (showpos)
			{
				*first++ = char_literal_v<u8'+', char_type>;
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_compiler_constant_show_base_define<
					base, uppercase_showbase, oct_c2y>(first);
			}
			*first++ = t ? char_literal_v<u8'1', char_type> : char_literal_v<u8'0', char_type>;
			return first;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<int_type>;
			unsigned_type value{static_cast<unsigned_type>(t)};
			if constexpr (showpos)
			{
				if constexpr (::fast_io::details::my_unsigned_integral<int_type>)
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				else if (t < 0)
				{
					*first = char_literal_v<u8'-', char_type>;
					constexpr unsigned_type zero{};
					value = zero - value;
				}
				else
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				++first;
			}
			else if constexpr (::fast_io::details::my_signed_integral<int_type>)
			{
				if (t < 0)
				{
					*first++ = char_literal_v<u8'-', char_type>;
					constexpr unsigned_type zero{};
					value = zero - value;
				}
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_compiler_constant_show_base_define<
					base, uppercase_showbase, oct_c2y>(first);
			}
			constexpr ::std::size_t full_digits{
				::fast_io::details::cal_max_int_size<unsigned_type, base>()};
			::std::size_t const digits{full ? full_digits : chars_len<base, false>(value)};
			if (digits == 1u)
			{
				// Keep the overwhelmingly common literal case as a forward store.
				// Besides being the minimal lowering, this gives object-size analysis
				// an explicit in-bounds write instead of asking it to reconstruct the
				// one-step `*--end` invariant of the generic reverse formatter.
				if constexpr (base <= 10u)
				{
					*first++ = ::fast_io::char_literal_add<char_type>(value);
				}
				else
				{
					constexpr auto table{
						::fast_io::details::digits_table<char_type, base, uppercase>};
					*first++ = static_cast<char_type>(
						table[(static_cast<::std::size_t>(value) << 1u) + 1u]);
				}
				return first;
			}
			char_type *const last{first + digits};
			if constexpr (sizeof(value) <= sizeof(unsigned))
			{
				print_reserve_integral_main_impl<base, uppercase>(
					last, static_cast<unsigned>(value), digits);
			}
			else
			{
				print_reserve_integral_main_impl<base, uppercase>(last, value, digits);
			}
			return last;
		}
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool oct_c2y = false, typename int_type, ::std::integral char_type>
inline constexpr void print_reserve_integral_define_precise(char_type *start, ::std::size_t n, int_type t)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_define_precise<base, showbase, uppercase_showbase, showpos, false, oct_c2y>(
			start, n, t); // prevent duplications
	}
	else
	{
		auto first{start};
		static_assert((2 <= base) && (base <= 36));
		if constexpr (::std::same_as<bool, ::std::remove_cvref_t<int_type>>)
		{
			if constexpr (showpos)
			{
				*first = char_literal_v<u8'+', char_type>;
				++first;
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			*first = t ? char_literal_v<u8'1', char_type> : char_literal_v<u8'0', char_type>;
			++first;
			return;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<int_type>;
			unsigned_type u{static_cast<unsigned_type>(t)};
			if constexpr (showpos)
			{
				if constexpr (::fast_io::details::my_unsigned_integral<int_type>)
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				else
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
					else
					{
						*first = char_literal_v<u8'+', char_type>;
					}
				}
				++first;
			}
			else
			{
				if constexpr (::fast_io::details::my_signed_integral<int_type>)
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						++first;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
				}
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			if constexpr (base == 10 && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
			{
				auto const digits{static_cast<::std::size_t>((start + n) - first)};
				if (digits == chars_len<base, false>(u))
				{
					return ::fast_io::details::jeaiii::jeaiii_main_len(
						first, u, static_cast<::std::uint_least32_t>(digits));
				}
				print_reserve_integral_withfull_precise_main_impl<base, uppercase>(start + n, u, digits);
			}
			else
			{
				auto ed{start + n};
				if constexpr (my_unsigned_integral<int_type> && !showbase && !showpos)
				{
					print_reserve_integral_withfull_precise_main_impl<base, uppercase>(ed, u, n);
				}
				else
				{
					print_reserve_integral_withfull_precise_main_impl<base, uppercase>(
						ed, u, static_cast<::std::size_t>(ed - first));
				}
			}
		}
	}
}

template <::std::size_t base, bool oct_c2y>
inline constexpr ::std::size_t compute_print_showbase_length() noexcept
{
	if (base == 2 || base == 3 || base == 16)
	{
		return 2; // 0b 0t 0x
	}
	else if (base == 8)
	{
		return oct_c2y ? 2 : 1; // oct_c2y: 0o, default: 0
	}
	else if (base < 10)
	{
		return 4; // 0[9]
	}
	else if (10 < base)
	{
		return 5; // 0[36]
	}
	return 0;
}

template <::std::size_t base, bool oct_c2y>
inline constexpr ::std::size_t print_showbase_length{compute_print_showbase_length<base, oct_c2y>()};

template <::std::size_t base = 10, bool showbase = false, bool showpos = false, bool oct_c2y = false, my_integral T>
inline constexpr ::std::size_t print_reserve_scalar_size_impl()
{
	::std::size_t total_sum{};
	if constexpr (showpos)
	{
		++total_sum;
	}
	if constexpr (showbase)
	{
		total_sum += ::fast_io::details::print_showbase_length<base, oct_c2y>;
	}
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
	{
		++total_sum;
	}
	else
	{
		if constexpr (!showpos)
		{
			if constexpr (my_signed_integral<T>)
			{
				++total_sum;
			}
		}
		using unsigned_type = ::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
		total_sum += ::fast_io::details::cal_max_int_size<unsigned_type, base>();
	}
	return total_sum;
}

template <::std::size_t base, bool showbase, bool showpos, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_size_cache{
	print_reserve_scalar_size_impl<base, showbase, showpos, oct_c2y, T>()};

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_reserve_scalar_cal_precise_cache_size_impl()
{
	::std::size_t total_sum{};
	if constexpr (showpos)
	{
		++total_sum;
	}
	if constexpr (showbase)
	{
		constexpr auto curr_length{::fast_io::details::print_showbase_length<base, oct_c2y>};
		total_sum += curr_length;
	}
	if constexpr (full)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
		{
			++total_sum;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
			total_sum += ::fast_io::details::cal_max_int_size<unsigned_type, base>();
		}
	}
	return total_sum;
}

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_precise_size_cache{
	print_reserve_scalar_cal_precise_cache_size_impl<base, showbase, showpos, full, oct_c2y, T>()};

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_precise_size(T t)
{
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
	{
		return print_integer_reserved_size_cache<base, showbase, showpos, oct_c2y, T>;
	}
	else if constexpr (full)
	{
		if constexpr (!showpos && my_signed_integral<T>)
		{
			::std::size_t total_sum{print_integer_reserved_precise_size_cache<base, showbase, showpos, full, oct_c2y, T>};
			if (t < 0)
			{
				++total_sum;
			}
			return total_sum;
		}
		else
		{
			return print_integer_reserved_precise_size_cache<base, showbase, showpos, full, oct_c2y, T>;
		}
	}
	else
	{
		// Non-full formatting uses the extracted JEAIII detector and composes the exact sign, prefix, and digit lengths.
		return ::fast_io::details::itoa_precise_length<base, showbase, showpos, oct_c2y>(t);
	}
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr ::std::size_t nullptr_print_optimization_call_size_impl() noexcept
{
	::fast_io::freestanding::array<char_type,
								   print_integer_reserved_size_cache<base, showbase, showpos, oct_c2y, ::std::size_t>>
		arr;
	auto res{print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
		arr.data(), ::std::size_t{})};
	return static_cast<::std::size_t>(res - arr.data());
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr ::std::size_t nullptr_print_optimization_call_size_cache{
	nullptr_print_optimization_call_size_impl<char_type, base, showbase, uppercase_showbase, showpos, uppercase,
											  full, oct_c2y>()};

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr auto nullptr_print_optimization_call_impl() noexcept
{
	constexpr ::std::size_t sz{nullptr_print_optimization_call_size_cache<char_type, base, showbase, uppercase_showbase,
																		  showpos, uppercase, full, oct_c2y>};
	::fast_io::freestanding::array<char_type, sz> arr{};
	[[maybe_unused]] auto res{print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
		arr.data(), ::std::size_t{})};
	return arr;
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr auto nullptr_print_optimization_call_cache{
	nullptr_print_optimization_call_impl<char_type, base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>()};

template <bool uppercase, ::std::random_access_iterator Iter>
	requires(::std::integral<::std::iter_value_t<Iter>>)
inline constexpr Iter print_reserve_boolalpha_impl(Iter iter, bool b)
{
	using char_type = ::std::iter_value_t<Iter>;
	if (b)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal("TRUE", iter);
			}
			else
			{
				return copy_string_literal("true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(L"TRUE", iter);
			}
			else
			{
				return copy_string_literal(L"true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u"TRUE", iter);
			}
			else
			{
				return copy_string_literal(u"true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(U"TRUE", iter);
			}
			else
			{
				return copy_string_literal(U"true", iter);
			}
		}
		else
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u8"TRUE", iter);
			}
			else
			{
				return copy_string_literal(u8"true", iter);
			}
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal("FALSE", iter);
			}
			else
			{
				return copy_string_literal("false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(L"FALSE", iter);
			}
			else
			{
				return copy_string_literal(L"false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u"FALSE", iter);
			}
			else
			{
				return copy_string_literal(u"false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(U"FALSE", iter);
			}
			else
			{
				return copy_string_literal(U"false", iter);
			}
		}
		else
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u8"FALSE", iter);
			}
			else
			{
				return copy_string_literal(u8"false", iter);
			}
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *print_reserve_nullptr_alphabet_impl(char_type *iter)
{
	if constexpr (::std::same_as<char_type, char>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal("NULLPTR", iter);
		}
		else
		{
			return copy_string_literal("nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(L"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(L"nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(u"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(u"nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, char32_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(U"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(U"nullptr", iter);
		}
	}
	else
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(u8"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(u8"nullptr", iter);
		}
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool oct_c2y = false, ::std::integral char_type>
inline constexpr char_type *print_reserve_method_impl(char_type *iter,
													  ::fast_io::manipulators::member_function_pointer_holder_t mfph)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_method_impl<base, showbase, uppercase_showbase, showpos, false, full, oct_c2y>(
			iter, mfph); // prevent duplications
	}
	else if constexpr (::fast_io::details::method_ptr_hold_size == 0)
	{
		return iter;
	}
	else
	{
		iter = details::print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
			iter, mfph.reference.front());
		using myssizet = ::std::make_signed_t<::std::size_t>;
		if constexpr (::fast_io::details::method_ptr_hold_size == 2)
		{
			::std::size_t backptr{mfph.reference.back()};
			return details::print_reserve_integral_define<base, showbase, uppercase_showbase, true, uppercase, false, oct_c2y>(
				iter, static_cast<myssizet>(backptr));
		}
		else
		{

			::std::size_t last{::fast_io::details::method_ptr_hold_size};
			for (::std::size_t i{1}; i != last; ++i)
			{
				iter =
					details::print_reserve_integral_define<base, showbase, uppercase_showbase, true, uppercase, false, oct_c2y>(
						iter, static_cast<myssizet>(mfph.reference[i]));
			}
			return iter;
		}
	}
}

/// @brief Prepared representation for the Apple-AArch64 two-value decimal integer schedule.
/// @details The sign occupies padding after the 32-bit leading block, so signed and unsigned 64-bit formatters share
///          one 24-byte state without increasing the two-value register footprint.  Keeping one common type is also
///          required by the semantic staged-group proof when a signed and an unsigned value occur in the same run.
struct print_integer_staged_u64_state
{
	::std::uint_least64_t middle{};
	::std::uint_least64_t low{};
	::std::uint_least32_t top{};
	bool negative{};
};

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
static_assert(sizeof(print_integer_staged_u64_state) == 24u);
#endif

/// @brief Selects the exactly measured integer staged domain.
/// @details Only ordinary decimal, non-full 64-bit scalar output participates. Base prefixes, explicit signs,
///          alphabetic forms, zero-filled full width, and scalar-placement metadata have different emission and
///          fallback costs and retain their established formatter.  Apple AArch64 `char` is the sole enabled target:
///          the complete two-value conversion and its 24-byte state were measured on M4, whereas traditional AArch64,
///          x86-64, wide-character stores, and EBCDIC have no equivalent whole-call evidence. The arithmetic helper
///          remains portable, but it is deliberately not inserted into the ordinary scalar classifier: per-width M4
///          probes showed that the resulting large-function layout regressed shorter decimal widths even when the
///          new long branch was not taken.
template <::fast_io::manipulators::scalar_flags flags, typename T, typename char_type>
inline constexpr bool print_integer_staged_u64_supported{
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
	::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type> &&
	::fast_io::details::my_integral<T> &&
	!::std::same_as<::std::remove_cv_t<T>, bool> &&
	sizeof(T) == sizeof(::std::uint_least64_t) && flags.base == 10u && !flags.alphabet &&
	!flags.showbase && !flags.showpos && !flags.uppercase_showbase && !flags.modern_octal &&
	!flags.uppercase && !flags.comma && !flags.full &&
	flags.placement == ::fast_io::manipulators::scalar_placement::none &&
	flags.percentage == ::fast_io::manipulators::percentage_flag::none
#else
	false
#endif
};

} // namespace details

/*
The scalar alias boundary deliberately uses ordinary inline semantics. An O3
A/B matrix covering GCC 11--16 and Clang 17--23 instantiated integer, binary32
and binary64 alias-only and formatted wrappers; removing forced inline produced
byte-identical objects in every compiler and left no alias call. Keeping an
attribute here would therefore increase policy surface without changing code
generation, while ordinary inline lets future optimizers make their own cost
decision.
*/
template <typename scalar_type>
	requires(details::non_character_integral<scalar_type> || ::fast_io::details::my_floating_point<scalar_type> ||
			 ::std::same_as<::std::nullptr_t, ::std::remove_cvref_t<scalar_type>>) &&
			(!::fast_io::details::floating_scalar_requires_integer_proxy<scalar_type>)
inline constexpr auto print_alias_define(io_alias_t, scalar_type t) noexcept
{
	if constexpr (details::non_character_integral<scalar_type>)
	{
		using int_alias_type = ::fast_io::details::integer_alias_type<scalar_type>;
		return manipulators::scalar_manip_t<manipulators::integral_default_scalar_flags, int_alias_type>{
			static_cast<int_alias_type>(t)};
	}
	else if constexpr (details::my_floating_point<scalar_type>)
	{
		return ::fast_io::details::make_floating_scalar_manip<
			manipulators::floating_point_default_scalar_flags>(t);
	}
	else
	{
		return manipulators::scalar_manip_t<manipulators::scalar_flags{.alphabet = true}, ::std::nullptr_t>{};
	}
}

/// Captures Clang/x86 bfloat16 at the original expression boundary before a
/// by-value scalar copy can be lowered as a float-to-bfloat16 retruncation.
/// The returned leaf owns only integer fields, so downstream print and concat
/// boundaries retain cheap by-value transport without changing other floating
/// ABIs.
template <::fast_io::details::floating_scalar_integer_proxy_source scalar_type>
inline constexpr auto
print_alias_define(io_alias_t, scalar_type &&value) noexcept
{
	return ::fast_io::details::make_floating_scalar_manip<
		manipulators::floating_point_default_scalar_flags>(
		::std::forward<scalar_type>(value));
}

template <::std::integral char_type, typename T>
	requires(details::non_character_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, T>) noexcept
{
	if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_integer_reserved_size_cache<10, false, false, false, char8_t>;
	}
	else
	{
		return details::print_integer_reserved_size_cache<10, false, false, false, ::std::remove_cvref_t<T>>;
	}
}

template <::std::integral char_type, typename T>
	requires(details::non_character_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
// This attribute consumes the wrapper-placement policy documented above; the
// ordinary-inline fallback calls the same print_reserve_integral_define arm.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]] // always inline to reduce inline depth in GCC and LLVM clang
#endif
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, T>, char_type *iter, T t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_reserve_integral_define<10, false, false, false, false, false>(iter,
																							 static_cast<char8_t>(t));
	}
	else
	{
		return details::print_reserve_integral_define<10, false, false, false, false, false>(iter, t);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
	requires(details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte> ||
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t> ||
			 ::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	if constexpr (flags.alphabet)
	{
		static_assert((::std::same_as<::std::remove_cvref_t<T>, bool> ||
					   ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>),
					  "only bool and ::std::nullptr_t support alphabet output");
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
		{
			return 5; // u8"false"
		}
		else
		{
			return 7; // u8"nullptr"
		}
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
	{
		constexpr ::std::size_t method_size{
			(details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, ::std::size_t> + 1) *
				::fast_io::details::method_ptr_hold_size -
			1};
		return method_size;
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>)
	{
		return details::nullptr_print_optimization_call_size_cache<char_type, flags.base, flags.showbase,
																   flags.uppercase_showbase, flags.showpos,
																   flags.uppercase, flags.full, flags.modern_octal>;
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, char8_t>;
	}
	else
	{
		return details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, T>;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires(details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte> ||
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t> ||
			 ::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
// As above, force-inlining changes only the reserve-wrapper boundary; every
// attribute-disabled build retains the same compile-time flag dispatch.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]] // always inline to reduce inline depth in GCC and LLVM clang
#endif
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, char_type *iter,
					 ::fast_io::manipulators::scalar_manip_t<flags, T> t) noexcept
{
	static_assert(flags.percentage == ::fast_io::manipulators::percentage_flag::none);
	if constexpr (flags.alphabet)
	{
		static_assert((::std::same_as<::std::remove_cvref_t<T>, bool> ||
					   ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>),
					  "only bool and ::std::nullptr_t support alphabet output");
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
		{
			return details::print_reserve_boolalpha_impl<flags.uppercase>(iter, t.reference);
		}
		else
		{
			return details::print_reserve_nullptr_alphabet_impl<flags.uppercase>(iter);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
	{
		return ::fast_io::details::print_reserve_method_impl<flags.base, flags.showbase, flags.uppercase_showbase,
															 flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(iter,
																															 t.reference);
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::nullptr_t>)
	{
		constexpr auto &cache{details::nullptr_print_optimization_call_cache<char_type, flags.base, flags.showbase,
																			 flags.uppercase_showbase, flags.showpos,
																			 flags.uppercase, flags.full, flags.modern_octal>};
		constexpr ::std::size_t n{cache.size()};
		return details::non_overlapped_copy_n(cache.element, n, iter);
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_reserve_integral_define<flags.base, flags.showbase, flags.uppercase_showbase,
													  flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(
			iter, static_cast<char8_t>(t.reference));
	}
	else
	{
		return details::print_reserve_integral_define<flags.base, flags.showbase, flags.uppercase_showbase,
													  flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(iter, t.reference);
	}
}

/// @brief Exposes the type-static, non-throwing reserve contract of an integral scalar to semantic IO policy.
/// @details For base b >= 2, an N-bit magnitude has at most ceil(N/log2(b)) digits; the existing reserve size adds the
///          finite sign and prefix maxima encoded by `flags`. `print_reserve_define` above implements exactly that flag
///          set and is noexcept, so its emitted length never exceeds the type-only reserve extent. This marker selects
///          no strategy by itself; it merely lets IO combine the scalar with a separately proved width bound.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
	requires(::fast_io::details::my_integral<T> &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
inline constexpr ::std::true_type print_extended_bounded_passive_companion_safe(
	::fast_io::io_reserve_type_t<
		char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// @brief Reports whether the optimizer proves the complete integral scalar payload constant at this call site.
/// @details The function is forced inline so `__builtin_constant_p` observes the public print/concat expression rather
///          than an out-of-line parameter.  Unsupported compilers conservatively retain the ordinary formatter.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr ::std::true_type
	print_compiler_constant_materialization_query_inline_safe(
		::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, T>, T const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value);
#else
	(void)value;
	return false;
#endif
}

/// @brief Materializes a raw default-format integer before its ordinary scalar alias is constructed.
/// @details This is the source-level counterpart of the scalar-manipulator overload below. Keeping the alias integer
///          type in the proxy makes the true arm byte-for-byte equivalent to the established default alias, while the
///          early gate's false arm performs no cast and enters that alias unchanged.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, T>, T const &value) noexcept
{
	using alias_type = ::fast_io::details::integer_alias_type<T>;
	return ::fast_io::manipulators::compiler_constant_scalar_manip_t<
		::fast_io::manipulators::integral_default_scalar_flags, alias_type>{
		static_cast<alias_type>(value)};
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr ::std::true_type
	print_compiler_constant_pre_normalization_safe(
		::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

/// @brief Records the permanent query/deletion classification for the raw integral provider graph.
/// @details Paired constant/unknown roots cover the scalar payload, while print, concat, and conversion consumers retain
///          independent compiler/version admission. This type-only marker carries no value and selects no consumer.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr ::std::true_type
	print_compiler_constant_materialization_graph_proven(
		::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

/// @brief Classifies a raw integer as one flat compiler-constant scalar source.
/// @details Its query reads only the integer value, and materialization creates one scalar proxy with the default flags;
///          no semantic condition, dynamic width, or precision state is introduced.
template <::std::integral char_type, typename T>
	requires(::fast_io::details::non_character_integral<T>)
[[nodiscard]] inline constexpr ::std::true_type
	print_compiler_constant_simple_scalar_source(
		::fast_io::io_reserve_type_t<char_type, T>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<char_type,
					  manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	io_reserve_type_t<char_type,
					  manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// @brief Records the same permanent classification for the flagged integral/boolalpha provider graph.
/// @details The type-owned flags are immutable; the only value-bearing field is `reference`, whose unknown query root
///          must return false. Consumer gates remain responsible for proving deletion of the selected spelling.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<char_type,
					  manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// @brief Classifies an explicitly flagged integer or boolalpha value as one flat scalar source.
/// @details The immutable flag object is part of the type and materialization copies only `reference` into one scalar
///          proxy. Percentage and non-bool alphabet modes are excluded by the same constraints as the source protocol.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_simple_scalar_source(
	io_reserve_type_t<char_type,
					  manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
	manipulators::scalar_manip_t<flags, T> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.reference);
#else
	(void)value;
	return false;
#endif
}

/// @brief Rebinds a proven constant scalar to its isolated reserve formatter.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
	manipulators::scalar_manip_t<flags, T> const &value) noexcept
{
	return manipulators::compiler_constant_scalar_manip_t<flags, T>{value.reference};
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>) noexcept
{
	return print_reserve_size(
		io_reserve_type<char_type, manipulators::scalar_manip_t<flags, T>>);
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>,
	char_type *iter,
	manipulators::compiler_constant_scalar_manip_t<flags, T> value) noexcept
{
	if constexpr (flags.alphabet)
	{
		return details::print_compiler_constant_boolalpha_define<flags.uppercase>(
			iter, value.reference);
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_reserve_integral_compiler_constant_define<
			flags.base, flags.showbase, flags.uppercase_showbase, flags.showpos,
			flags.uppercase, flags.full, flags.modern_octal>(
			iter, static_cast<char8_t>(value.reference));
	}
	else
	{
		return details::print_reserve_integral_compiler_constant_define<
			flags.base, flags.showbase, flags.uppercase_showbase, flags.showpos,
			flags.uppercase, flags.full, flags.modern_octal>(iter, value.reference);
	}
}

namespace details
{

/// Immutable digit storage used by the compiler-constant scatter formatter.
/// A descriptor points at one element of this table; no digit is first copied
/// through the caller's stack merely to make it addressable by writev.
template <::std::integral char_type, bool uppercase>
inline constexpr ::fast_io::freestanding::array<char_type, 26u>
	compiler_constant_integral_alphabet_fragments{
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'a', u8'A', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'b', u8'B', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'c', u8'C', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'd', u8'D', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'e', u8'E', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'f', u8'F', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'g', u8'G', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'h', u8'H', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'i', u8'I', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'j', u8'J', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'k', u8'K', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'l', u8'L', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'm', u8'M', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'n', u8'N', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'o', u8'O', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'p', u8'P', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'q', u8'Q', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'r', u8'R', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8's', u8'S', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8't', u8'T', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'u', u8'U', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'v', u8'V', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'w', u8'W', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'x', u8'X', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'y', u8'Y', char_type>,
		::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'z', u8'Z', char_type>};

template <::std::integral char_type, bool uppercase>
inline constexpr auto compiler_constant_integral_digit_fragments{[]() constexpr {
	::fast_io::freestanding::array<char_type, 36u> result{};
	for (::std::size_t index{}; index != 10u; ++index)
	{
		result[index] = ::fast_io::char_literal_add<char_type>(index);
	}
	for (::std::size_t index{10u}; index != 36u; ++index)
	{
		result[index] =
			::fast_io::details::compiler_constant_integral_alphabet_fragments<
				char_type, uppercase>[index - 10u];
	}
	return result;
}()};

/// Immutable two-digit spellings for one radix.  Each entry is an actual
/// `char_type[2]` fragment, so the scatter path never relies on ASCII byte
/// identity or reinterprets the narrow table for another character domain.
/// Every code unit originates in `compiler_constant_integral_digit_fragments`,
/// whose decimal digits use `char_literal_add` and whose alphabetic digits use
/// explicit `char_literal_v` entries (required because EBCDIC letters are not
/// one contiguous arithmetic range).
template <::std::integral char_type, ::std::size_t base, bool uppercase>
	requires(2u <= base && base <= 36u)
inline constexpr auto compiler_constant_integral_pair_fragments{[]() constexpr {
	::fast_io::freestanding::array<char_type, base * base * 2u> result{};
	auto const &digits{
		::fast_io::details::compiler_constant_integral_digit_fragments<
			char_type, uppercase>};
	for (::std::size_t high{}; high != base; ++high)
	{
		for (::std::size_t low{}; low != base; ++low)
		{
			auto const offset{(high * base + low) * 2u};
			result[offset] = digits[high];
			result[offset + 1u] = digits[low];
		}
	}
	return result;
}()};

template <typename integer_type>
struct compiler_constant_integral_unsigned_type
{
	using type = ::fast_io::details::my_make_unsigned_t<integer_type>;
};

// `std::make_unsigned<bool>` is intentionally ill-formed.  Numeric bool still
// participates in the scalar protocol, but its magnitude is always one digit.
template <>
struct compiler_constant_integral_unsigned_type<bool>
{
	using type = unsigned char;
};

template <typename integer_type>
using compiler_constant_integral_unsigned_type_t =
	typename compiler_constant_integral_unsigned_type<integer_type>::type;

template <::std::integral char_type>
inline constexpr ::fast_io::freestanding::array<char_type, 2u>
	compiler_constant_integral_sign_fragments{
		::fast_io::char_literal_v<u8'+', char_type>,
		::fast_io::char_literal_v<u8'-', char_type>};

template <::std::integral char_type, bool uppercase, bool value>
inline constexpr auto compiler_constant_boolalpha_fragment{[]() constexpr {
	constexpr ::std::size_t size{value ? 4u : 5u};
	::fast_io::freestanding::array<char_type, size> result{};
	if constexpr (value)
	{
		result[0u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8't', u8'T', char_type>;
		result[1u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'r', u8'R', char_type>;
		result[2u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'u', u8'U', char_type>;
		result[3u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'e', u8'E', char_type>;
	}
	else
	{
		result[0u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'f', u8'F', char_type>;
		result[1u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'a', u8'A', char_type>;
		result[2u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'l', u8'L', char_type>;
		result[3u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8's', u8'S', char_type>;
		result[4u] = ::fast_io::details::compiler_constant_case_literal_v<
			uppercase, u8'e', u8'E', char_type>;
	}
	return result;
}()};

template <::std::integral char_type, ::std::size_t base,
		  bool uppercase_showbase, bool modern_octal>
inline constexpr auto compiler_constant_integral_prefix_fragment{[]() constexpr {
	constexpr ::std::size_t size{
		::fast_io::details::print_showbase_length<base, modern_octal>};
	::fast_io::freestanding::array<char_type, size> result{};
	auto const end{
		::fast_io::details::print_compiler_constant_show_base_define<
			base, uppercase_showbase, modern_octal>(result.data())};
	if (end != result.data() + size)
	{
		::fast_io::fast_terminate();
	}
	return result;
}()};

} // namespace details

/// Maximum descriptor count for the immutable-fragment spelling of one
/// optimizer-proven integer. Signs and base prefixes each occupy one fragment;
/// digit payloads use one leading code unit when needed followed by shared
/// two-code-unit fragments.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
inline constexpr ::std::size_t print_compiler_constant_static_fragments_size(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>) noexcept
{
	if constexpr (flags.alphabet)
	{
		return 1u;
	}
	else
	{
		using integer_type = ::std::conditional_t<
			::std::same_as<::std::remove_cv_t<T>, ::std::byte>, char8_t,
			::std::remove_cv_t<T>>;
		using unsigned_type = ::fast_io::details::
			compiler_constant_integral_unsigned_type_t<integer_type>;
		constexpr ::std::size_t digits{[]() constexpr {
			if constexpr (::std::same_as<integer_type, bool>)
			{
				return 1u;
			}
			else
			{
				return ::fast_io::details::cal_max_int_size<
					unsigned_type, flags.base>();
			}
		}()};
		constexpr ::std::size_t sign{
			flags.showpos ||
			(::fast_io::details::my_signed_integral<integer_type> &&
			 !::std::same_as<integer_type, bool>)};
		constexpr ::std::size_t prefix{
			flags.showbase && flags.base != 10u};
		return (digits + 1u) / 2u + sign + prefix;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
inline constexpr ::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	manipulators::compiler_constant_scalar_manip_t<flags, T> const &value) noexcept
{
	if constexpr (flags.alphabet)
	{
		if (value.reference)
		{
			auto const &storage{
				::fast_io::details::compiler_constant_boolalpha_fragment<
					char_type, flags.uppercase, true>};
			*first++ = {storage.data(), storage.size()};
		}
		else
		{
			auto const &storage{
				::fast_io::details::compiler_constant_boolalpha_fragment<
					char_type, flags.uppercase, false>};
			*first++ = {storage.data(), storage.size()};
		}
		return first;
	}
	else
	{
		using integer_type = ::std::conditional_t<
			::std::same_as<::std::remove_cv_t<T>, ::std::byte>, char8_t,
			::std::remove_cv_t<T>>;
		using unsigned_type = ::fast_io::details::
			compiler_constant_integral_unsigned_type_t<integer_type>;
		integer_type const signed_value{[](
											auto const &source) constexpr -> integer_type {
			if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
			{
				return static_cast<char8_t>(source);
			}
			else
			{
				return source;
			}
		}(value.reference)};
		unsigned_type magnitude{static_cast<unsigned_type>(signed_value)};
		bool negative{};
		if constexpr (
			::fast_io::details::my_signed_integral<integer_type> &&
			!::std::same_as<integer_type, bool>)
		{
			negative = signed_value < 0;
			if (negative)
			{
				constexpr unsigned_type zero{};
				magnitude = zero - magnitude;
			}
		}
		if constexpr (
			flags.showpos ||
			(::fast_io::details::my_signed_integral<integer_type> &&
			 !::std::same_as<integer_type, bool>))
		{
			if (flags.showpos || negative)
			{
				auto const &signs{
					::fast_io::details::compiler_constant_integral_sign_fragments<
						char_type>};
				*first++ = {signs.data() + static_cast<::std::size_t>(negative), 1u};
			}
		}
		if constexpr (flags.showbase && flags.base != 10u)
		{
			auto const &prefix{
				::fast_io::details::compiler_constant_integral_prefix_fragment<
					char_type, flags.base, flags.uppercase_showbase,
					flags.modern_octal>};
			*first++ = {prefix.data(), prefix.size()};
		}
		if constexpr (::std::same_as<integer_type, bool>)
		{
			auto const &storage{
				::fast_io::details::compiler_constant_integral_digit_fragments<
					char_type, flags.uppercase>};
			*first++ = {storage.data() + static_cast<::std::size_t>(magnitude),
						1u};
			return first;
		}
		// Splitting a wide carrier never changes the requested digit alphabet.
		// Keep immutable fragments byte-for-byte aligned with the ordinary writer,
		// whose high and low halves both preserve the original uppercase policy.
		constexpr bool effective_uppercase{flags.uppercase};
		constexpr ::std::size_t maximum_digits{
			::fast_io::details::cal_max_int_size<unsigned_type, flags.base>()};
		::std::size_t digits{maximum_digits};
		if constexpr (!flags.full)
		{
			digits = 1u;
			for (auto remaining{magnitude};
				 remaining >= static_cast<unsigned_type>(flags.base);
				 remaining = static_cast<unsigned_type>(
					 remaining / static_cast<unsigned_type>(flags.base)))
			{
				++digits;
			}
		}
		auto const digit_fragments{(digits + 1u) / 2u};
		auto digit_iter{first + digit_fragments};
		first = digit_iter;
		auto const &single_storage{
			::fast_io::details::compiler_constant_integral_digit_fragments<
				char_type, effective_uppercase>};
		auto const &pair_storage{
			::fast_io::details::compiler_constant_integral_pair_fragments<
				char_type, flags.base, effective_uppercase>};
		constexpr unsigned_type base_value{
			static_cast<unsigned_type>(flags.base)};
		for (auto remaining{digits}; remaining >= 2u; remaining -= 2u)
		{
			auto const low{static_cast<::std::size_t>(magnitude % base_value)};
			magnitude = static_cast<unsigned_type>(
				magnitude / base_value);
			auto const high{static_cast<::std::size_t>(magnitude % base_value)};
			magnitude = static_cast<unsigned_type>(
				magnitude / base_value);
			auto const pair_offset{(high * flags.base + low) * 2u};
			*--digit_iter = {pair_storage.data() + pair_offset, 2u};
		}
		if ((digits & 1u) != 0u)
		{
			auto const digit{static_cast<::std::size_t>(magnitude % base_value)};
			*--digit_iter = {single_storage.data() + digit, 1u};
		}
		return first;
	}
}

/// @brief Reports the exact contiguous spelling length of one proven-constant integer proxy.
/// @details Concat consumes this protocol only after its optimizer gate has replaced the mature run-time scalar.  The
///          length calculation mirrors the immutable-fragment spelling but does not construct descriptors or copy any
///          character payload: the companion precise writer still emits directly into concat's one final destination.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::size_t
print_reserve_precise_size(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>,
	manipulators::compiler_constant_scalar_manip_t<flags, T> const &value) noexcept
{
	if constexpr (flags.alphabet)
	{
		return value.reference ? 4u : 5u;
	}
	else
	{
		using integer_type = ::std::conditional_t<
			::std::same_as<::std::remove_cv_t<T>, ::std::byte>, char8_t,
			::std::remove_cv_t<T>>;
		using unsigned_type = ::fast_io::details::
			compiler_constant_integral_unsigned_type_t<integer_type>;
		integer_type const signed_value{[](auto const &source) constexpr
											-> integer_type {
			if constexpr (
				::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
			{
				return static_cast<char8_t>(source);
			}
			else
			{
				return source;
			}
		}(value.reference)};
		unsigned_type magnitude{static_cast<unsigned_type>(signed_value)};
		bool negative{};
		if constexpr (
			::fast_io::details::my_signed_integral<integer_type> &&
			!::std::same_as<integer_type, bool>)
		{
			negative = signed_value < 0;
			if (negative)
			{
				constexpr unsigned_type zero{};
				magnitude = zero - magnitude;
			}
		}

		::std::size_t size{};
		if (negative || flags.showpos)
		{
			++size;
		}
		if constexpr (flags.showbase && flags.base != 10u)
		{
			size += ::fast_io::details::print_showbase_length<
				flags.base, flags.modern_octal>;
		}
		if constexpr (::std::same_as<integer_type, bool>)
		{
			return size + 1u;
		}
		else if constexpr (flags.full)
		{
			return size + ::fast_io::details::cal_max_int_size<
							  unsigned_type, flags.base>();
		}
		else
		{
			::std::size_t digits{1u};
			constexpr unsigned_type base_value{
				static_cast<unsigned_type>(flags.base)};
			for (; magnitude >= base_value;
				 magnitude = static_cast<unsigned_type>(magnitude / base_value))
			{
				++digits;
			}
			return size + digits;
		}
	}
}

/// @brief Emits a proven-constant integer into concat's exact destination slice.
/// @details The exact-size protocol is a destination/allocation refinement only.  It deliberately reuses the isolated
///          compiler-constant reserve formatter and never converts concat into a retained-scatter output operation.
///          This leaf remains forced inline: Clang 23 otherwise outlines the precise writer for multi-digit 64/128-bit
///          and non-decimal constants, leaving a conversion call in an otherwise fully constant print expression.
///          Optimizer-unknown integers never acquire this proxy type, so their established run-time writer is unaffected.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_reserve_precise_define(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>
		tag,
	char_type *iter, ::std::size_t precise_size,
	manipulators::compiler_constant_scalar_manip_t<flags, T> value) noexcept
{
	(void)precise_size;
	return print_reserve_define(tag, iter, value);
}

/// @brief Proves that this materialized proxy owns one bounded integer writer graph.
/// @details The immutable flags are part of the type and `reference` is the only value-bearing field. The exact writer
///          delegates directly to the integral compiler-constant leaf above; it cannot enter a precision planner,
///          dynamic-width policy, semantic condition, allocation, or the native source formatter.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 !flags.alphabet &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_flat_integer_replacement(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// @brief Selects the exact-size compact protocol for a proven-constant integer proxy.
/// @details The precise size query is a pure integer calculation and the companion pointer-returning writer emits
///          exactly that extent.  Print/concat may therefore ignore the proxy's all-values reserve bound after the
///          compiler-constant gate has selected this type.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// @brief Returns one provider-owned immutable spelling when the complete integer is a single table slice.
/// @details Alphabetic booleans already occupy one typed static object.  A numeric value can similarly use one digit
///          or one radix-pair entry when it has no observable sign/base prefix and its effective digit count is at most
///          two.  Every table is built from semantic character literals, so `char`/`wchar_t` remain correct for EBCDIC
///          execution sets and the three Unicode character types never borrow narrow storage.  More complex spellings
///          return an empty descriptor and retain the established multi-fragment protocol.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  typename T>
	requires((details::my_integral<T> ||
			  ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet ||
			  ::std::same_as<::std::remove_cv_t<T>, bool>) &&
			 flags.percentage ==
				 ::fast_io::manipulators::percentage_flag::none)
[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char_type>
print_compiler_constant_single_static_fragment(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>,
	manipulators::compiler_constant_scalar_manip_t<flags, T> const &value) noexcept
{
	if constexpr (flags.alphabet)
	{
		if (value.reference)
		{
			auto const &storage{
				::fast_io::details::compiler_constant_boolalpha_fragment<
					char_type, flags.uppercase, true>};
			return {storage.data(), storage.size()};
		}
		auto const &storage{
			::fast_io::details::compiler_constant_boolalpha_fragment<
				char_type, flags.uppercase, false>};
		return {storage.data(), storage.size()};
	}
	else
	{
		using integer_type = ::std::conditional_t<
			::std::same_as<::std::remove_cv_t<T>, ::std::byte>, char8_t,
			::std::remove_cv_t<T>>;
		using unsigned_type = ::fast_io::details::
			compiler_constant_integral_unsigned_type_t<integer_type>;
		integer_type const signed_value{[](auto const &source) constexpr
											-> integer_type {
			if constexpr (
				::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
			{
				return static_cast<char8_t>(source);
			}
			else
			{
				return source;
			}
		}(value.reference)};
		unsigned_type magnitude{static_cast<unsigned_type>(signed_value)};
		bool negative{};
		if constexpr (
			::fast_io::details::my_signed_integral<integer_type> &&
			!::std::same_as<integer_type, bool>)
		{
			negative = signed_value < 0;
			if (negative)
			{
				constexpr unsigned_type zero{};
				magnitude = static_cast<unsigned_type>(zero - magnitude);
			}
		}
		if (negative || flags.showpos ||
			(flags.showbase && flags.base != 10u))
		{
			return {};
		}

		::std::size_t digits{};
		if constexpr (flags.full)
		{
			digits = ::fast_io::details::cal_max_int_size<
				unsigned_type, flags.base>();
		}
		else
		{
			digits = 1u;
			constexpr unsigned_type base_value{
				static_cast<unsigned_type>(flags.base)};
			for (auto remaining{magnitude}; remaining >= base_value;
				 remaining = static_cast<unsigned_type>(remaining / base_value))
			{
				++digits;
				if (2u < digits)
				{
					return {};
				}
			}
		}
		// A one-fragment result is semantically independent of carrier splitting;
		// select the alphabet requested by the scalar flags for every width.
		constexpr bool effective_uppercase{flags.uppercase};
		if (digits == 1u)
		{
			auto const &storage{
				::fast_io::details::compiler_constant_integral_digit_fragments<
					char_type, effective_uppercase>};
			return {storage.data() + static_cast<::std::size_t>(magnitude),
					1u};
		}
		if (digits == 2u)
		{
			constexpr unsigned_type base_value{
				static_cast<unsigned_type>(flags.base)};
			auto const low{static_cast<::std::size_t>(magnitude % base_value)};
			auto const high{static_cast<::std::size_t>(
				(magnitude / base_value) % base_value)};
			auto const &storage{
				::fast_io::details::compiler_constant_integral_pair_fragments<
					char_type, flags.base, effective_uppercase>};
			return {storage.data() + (high * flags.base + low) * 2u, 2u};
		}
		return {};
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 !flags.alphabet &&
			 (flags.showpos || (details::my_signed_integral<T> &&
								!::std::same_as<::std::remove_cv_t<T>, bool>)))
inline constexpr ::std::size_t print_define_internal_shift(
	io_reserve_type_t<char_type,
					  manipulators::compiler_constant_scalar_manip_t<flags, T>>,
	manipulators::compiler_constant_scalar_manip_t<flags, T> value) noexcept
{
	if constexpr (flags.showpos)
	{
		return 1u;
	}
	else
	{
		return value.reference < 0;
	}
}

/// @brief Advertises the shared signed/unsigned 64-bit decimal prepared state on its audited target.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
inline constexpr auto print_staged_type(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return ::fast_io::io_type_t<::fast_io::details::print_integer_staged_u64_state>{};
}

/// @brief Requires two independent integer conversions before selecting staged scheduling.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
inline constexpr ::std::size_t print_staged_width(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return 2u;
}

/// @brief Caps integer staging at the measured two-value register envelope.
/// @details M4 assembly keeps two 24-byte prepared states in registers. Four states enlarge the frame and eight states
///          spill to an explicit prepared array with a stack protector; complete timings then erase or reverse the
///          scheduling gain. Returning two makes larger integer runs retain the ordinary mature formatter rather than
///          treating the minimum threshold as permission to prepare the complete run.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
inline constexpr ::std::size_t print_staged_max_count(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return 2u;
}

/// @brief Keeps the overwhelmingly common short-integer fallback adjacent to the eligibility branch.
/// @details Staging accepts only the 17--20 digit domain. Sending counters and identifiers through the generic cold
///          fallback would trade a predictable comparison for a call and a separate large formatter body. This policy
///          affects placement only; the fallback remains `print_reserve_define` with identical capacity and bytes.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
inline constexpr bool print_staged_fallback_inline(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return true;
}

/// @brief Tests the proved 17--20 digit domain without signed overflow.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool print_staged_eligible(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
	manipulators::scalar_manip_t<flags, T> const &value) noexcept
{
	auto const magnitude{::fast_io::details::itoa_unsigned_magnitude(value.reference)};
	return magnitude >= static_cast<decltype(magnitude)>(10000000000000000ull);
}

/// @brief Prepares sign, decimal blocks, and both fixed-block multiplier chains before either value emits.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::details::print_integer_staged_u64_state
print_staged_prepare(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
	manipulators::scalar_manip_t<flags, T> const &value) noexcept
{
	auto const magnitude{::fast_io::details::itoa_unsigned_magnitude(value.reference)};
	auto const arithmetic{::fast_io::details::jeaiii::jeaiii_prepare_u64_long(
		static_cast<::std::uint_least64_t>(magnitude))};
	bool negative{};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		negative = value.reference < 0;
	}
	return {arithmetic.middle, arithmetic.low, arithmetic.top, negative};
}

/// @brief Emits one prepared integer directly into the caller's bounded contiguous range.
template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::print_integer_staged_u64_supported<flags, T, char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_staged_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>, char_type *iter,
	manipulators::scalar_manip_t<flags, T> const &,
	::fast_io::details::print_integer_staged_u64_state const &state) noexcept
{
	if (state.negative)
	{
		*iter++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	return ::fast_io::details::jeaiii::jeaiii_emit_u64_long(
		iter, {state.middle, state.low, state.top});
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>))
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
						   manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (flags.alphabet)
	{
		return t.reference ? 4u : 5u;
	}
	else if constexpr (::std::same_as<T, ::std::byte>)
	{
		return details::print_integer_reserved_precise_size<flags.base, flags.showbase, flags.showpos, flags.full,
															flags.modern_octal>(static_cast<char8_t>(t.reference));
	}
	else
	{
		return details::print_integer_reserved_precise_size<flags.base, flags.showbase, flags.showpos, flags.full,
															flags.modern_octal>(t.reference);
	}
}

template <::std::integral char_type, ::std::random_access_iterator Iter, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) &&
			 (!flags.alphabet || ::std::same_as<::std::remove_cv_t<T>, bool>))
inline constexpr void print_reserve_precise_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
												   Iter iter, ::std::size_t n,
												   manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (flags.alphabet)
	{
		[[maybe_unused]] auto end{details::print_reserve_boolalpha_impl<flags.uppercase>(iter, t.reference)};
	}
	else if constexpr (::std::same_as<T, ::std::byte>)
	{
		details::print_reserve_integral_define_precise<flags.base, flags.showbase, flags.uppercase_showbase,
													   flags.showpos, flags.uppercase, flags.modern_octal>(
			iter, n, static_cast<char8_t>(t.reference));
	}
	else
	{
		details::print_reserve_integral_define_precise<flags.base, flags.showbase, flags.uppercase_showbase,
													   flags.showpos, flags.uppercase, flags.modern_octal>(iter, n, t.reference);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) && !flags.alphabet &&
			 (flags.showpos || (details::my_signed_integral<T> && !::std::same_as<::std::remove_cv_t<T>, bool>)))
inline constexpr ::std::size_t
print_define_internal_shift(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
							manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (flags.showpos)
	{
		return 1;
	}
	else
	{
		return t.reference < 0;
	}
}

} // namespace fast_io
