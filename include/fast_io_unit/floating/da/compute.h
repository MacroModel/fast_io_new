#pragma once

#include "cache.h"

namespace fast_io::details::da
{

struct conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t last_digit;
	bool has_last_digit;
};

[[nodiscard]] inline constexpr conversion_result compute_irregular(
	::std::uint_least64_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const decimal_exponent{compute_decimal_exponent(binary_exponent, false)};
	auto const shift{static_cast<::std::uint_least8_t>(
		compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift)};
	auto const power{cached_power10[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const half_ulp{power.hi >> (extra_shift + 1u - shift)};
	auto const round_up{half_ulp > UINT64_MAX - fractional};
	auto const round_down{(half_ulp >> 1u) > fractional};
	integral += round_up;
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) - 1u))};
	auto const lower{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional - (half_ulp >> 1u), 10u, UINT64_MAX))};
	if (digit < lower)
	{
		digit = lower;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

[[nodiscard]] inline constexpr conversion_result compute_binary32(
	::std::uint_least32_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const decimal_exponent{compute_decimal_exponent_binary32(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_exponent_shifts.data[static_cast<::std::size_t>(
			binary_exponent + exponent_shift_cache::binary64_exponent_offset)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto const power_high{cached_power10[-decimal_exponent - 1].hi};
	auto const product{::fast_io::intrinsics::umulh(
		power_high + 1u, static_cast<::std::uint_least64_t>(binary_significand) << shift)};
	constexpr ::std::uint_least64_t fractional_mask{(static_cast<::std::uint_least64_t>(1) << extra_shift) - 1u};
	auto const fractional{product & fractional_mask};
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power_high >> (65u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{static_cast<bool>((fractional + half_ulp) >> extra_shift)};
	auto const round_down{half_ulp > fractional};
	auto const integral{static_cast<::std::uint_least64_t>((product >> extra_shift) + round_up)};
	auto digit{static_cast<::std::uint_least32_t>(
		(fractional * 10u + (static_cast<::std::uint_least64_t>(1) << (extra_shift - 1u))) >> extra_shift)};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << (extra_shift - 2u)))
	{
		digit = 2u;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

[[nodiscard]] inline constexpr conversion_result compute_binary64(
	::std::uint_least64_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	::std::int_least32_t decimal_exponent;
#if defined(__APPLE__) && defined(__SIZEOF_INT128__)
	decimal_exponent = static_cast<::std::int_least32_t>(::fast_io::intrinsics::umulh(
		static_cast<::std::uint_least64_t>(static_cast<::std::int_least64_t>(binary_exponent)),
		static_cast<::std::uint_least64_t>(78913) << 46u));
#else
	decimal_exponent = compute_decimal_exponent(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_exponent_shifts.data[static_cast<::std::size_t>(
		binary_exponent + exponent_shift_cache::binary64_exponent_offset)]};
	auto const power{cached_power10[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power.hi >> (extra_shift + 1u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{fractional + half_ulp < fractional};
	auto const round_down{half_ulp > fractional};
	integral += round_up;
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) + 6u))};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << 62u))
	{
		digit = 2u;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

} // namespace fast_io::details::da
