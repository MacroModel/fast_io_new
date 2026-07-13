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

template <typename flt, typename mantissa_type>
[[nodiscard]] inline constexpr decimal_result<flt> to_decimal(
	mantissa_type mantissa, ::std::int_least32_t raw_exponent) noexcept
{
	constexpr bool binary32{sizeof(flt) <= sizeof(float)};
	constexpr ::std::uint_least32_t significand_bits{binary32 ? 23u : 52u};
	constexpr ::std::int_least32_t exponent_offset{binary32 ? 150 : 1075};
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1) << significand_bits};
	auto binary_exponent{raw_exponent - exponent_offset};
	auto binary_significand{static_cast<::std::uint_least64_t>(mantissa)};
	bool regular{true};
	if (raw_exponent == 0)
	{
		++binary_exponent;
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
			static_cast<::std::uint_least32_t>(binary_significand), binary_exponent);
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64(binary_significand, binary_exponent);
	}
	if (converted.has_last_digit)
	{
		auto const decimal_significand{converted.significand * 10u + converted.last_digit};
		return {static_cast<decimal_mantissa_type<flt>>(decimal_significand), converted.exponent};
	}
	return {static_cast<decimal_mantissa_type<flt>>(converted.significand), converted.exponent + 1};
}

} // namespace fast_io::details::da
