#pragma once

#include "compute.h"

namespace fast_io::details::da
{

template <typename flt>
using decimal_mantissa_type =
	::std::conditional_t<(sizeof(flt) <= sizeof(float)), ::std::uint_least32_t, ::std::uint_least64_t>;

template <typename flt>
struct decimal_result
{
	decimal_mantissa_type<flt> m10;
	::std::int_least32_t e10;
};

/// @brief  Indicates whether the floating decimal type customization provides staged conversion on this ISA.
/// @details This is a capability of the concrete floating implementation, not a condition in the generic
///          staged_printable protocol or print orchestration.
template <typename flt>
inline constexpr bool staged_supported{
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__x86_64__) || defined(_M_X64)
	::std::same_as<::std::remove_cvref_t<flt>, float> ||
	::std::same_as<::std::remove_cvref_t<flt>, double>
#else
	false
#endif
};

/// @brief Indicates whether the staged decimal state carries the original sign on this compiler target.
/// @details Carrying the sign removes a second value load during emission when it improves the concrete staged
///          schedule. Other compiler/type combinations retain the independent sign extraction in their emitter.
template <typename flt>
inline constexpr bool staged_prepares_sign{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__clang__)
	::std::same_as<::std::remove_cvref_t<flt>, float> ||
	::std::same_as<::std::remove_cvref_t<flt>, double>
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(__GNUC__)
	::std::same_as<::std::remove_cvref_t<flt>, double>
#else
	false
#endif
};

struct signed_conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t last_digit;
	bool has_last_digit;
	bool negative;
};

template <typename flt>
using staged_conversion_result = ::std::conditional_t<
	::fast_io::details::da::staged_prepares_sign<flt>,
	::fast_io::details::da::signed_conversion_result,
	::fast_io::details::da::conversion_result>;

/// @brief  Returns the preferred number of independent conversions in one staged floating run.
/// @details Target tuning belongs to the decimal implementation; it does not change whether a floating
///          manipulator models staged_printable or alter the generic print orchestration.
/// @tparam flt the floating-point type
template <typename flt>
[[nodiscard]] inline consteval ::std::size_t staged_width() noexcept
{
#if defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64))
	return ::std::same_as<::std::remove_cvref_t<flt>, float> ? 8u : 4u;
#else
	return ::std::same_as<::std::remove_cvref_t<flt>, float> ? 8u : 6u;
#endif
}

/// @brief  Tests the regular-normal precondition required by the prepared decimal conversion.
/// @details The Apple AArch64 constraint is a code-generation barrier only; every target evaluates the
///          same mantissa and exponent predicate.
/// @tparam flt           the floating-point type
/// @tparam mantissa_type the unsigned representation used by the floating mantissa
/// @tparam exponent_type the unsigned representation used by the raw exponent
/// @param  mantissa      the explicit binary mantissa bits
/// @param  exponent      the raw binary exponent bits
/// @param  exponent_mask a mask containing every raw exponent bit
/// @return bool true when the direct staged conversion accepts the value
template <typename flt, typename mantissa_type, typename exponent_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool staged_eligible(
	mantissa_type mantissa, exponent_type exponent, mantissa_type exponent_mask) noexcept
{
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
	if constexpr (sizeof(::std::remove_cvref_t<flt>) > sizeof(float))
	{
		if (!::std::is_constant_evaluated())
		{
			__asm__("" : "+r"(mantissa), "+r"(exponent));
		}
	}
#endif
	return (mantissa != 0u) &
		   (static_cast<mantissa_type>(exponent - 1u) < exponent_mask - 1u);
}

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_result<flt> finalize(
	conversion_result converted) noexcept
{
	if (converted.has_last_digit)
	{
		return {static_cast<decimal_mantissa_type<flt>>(
					converted.significand * 10u + converted.last_digit),
				converted.exponent};
	}
	return {static_cast<decimal_mantissa_type<flt>>(converted.significand), converted.exponent + 1};
}

template <typename decimal_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_type trim_trailing_zeros(
	decimal_type result) noexcept
{
	if (result.m10 % 10u == 0u) [[unlikely]]
	{
		auto const [m10, zeroes]{::fast_io::bitops::rtz_iec559(result.m10)};
		result.m10 = m10;
		result.e10 += static_cast<::std::int_least32_t>(static_cast<::std::uint_least32_t>(zeroes));
	}
	return result;
}

template <typename flt, typename mantissa_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr decimal_result<flt> to_decimal(
	mantissa_type mantissa, ::std::int_least32_t raw_exponent) noexcept
{
	constexpr bool binary32{sizeof(flt) <= sizeof(float)};
	constexpr ::std::uint_least32_t significand_bits{binary32 ? 23u : 52u};
	constexpr ::std::int_least32_t exponent_offset{binary32 ? 150 : 1075};
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1) << significand_bits};
	auto binary_exponent{raw_exponent - exponent_offset};
	auto effective_raw_exponent{static_cast<::std::uint_least32_t>(raw_exponent)};
	auto binary_significand{static_cast<::std::uint_least64_t>(mantissa)};
	bool regular{true};
	if (raw_exponent == 0)
	{
		++binary_exponent;
		effective_raw_exponent = 1u;
	}
	else
	{
		regular = binary_significand != 0u;
		binary_significand |= implicit_bit;
	}
	conversion_result converted;
	if (!regular)
	{
		converted = ::fast_io::details::da::compute_irregular(binary_significand, binary_exponent);
	}
	else if constexpr (binary32)
	{
		converted = ::fast_io::details::da::compute_binary32(
			static_cast<::std::uint_least32_t>(binary_significand), effective_raw_exponent);
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64(binary_significand, effective_raw_exponent);
	}
	return ::fast_io::details::da::finalize<flt>(converted);
}

} // namespace fast_io::details::da

#include "ascii.h"
