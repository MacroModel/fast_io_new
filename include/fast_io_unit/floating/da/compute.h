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

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_irregular(
	::std::uint_least64_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	auto const decimal_exponent{compute_decimal_exponent(binary_exponent, false)};
	auto const shift{static_cast<::std::uint_least8_t>(
		compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift)};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const half_ulp{power.hi >> (extra_shift + 1u - shift)};
	auto const round_up{half_ulp > uint64_max - fractional};
	auto const round_down{(half_ulp >> 1u) > fractional};
	integral += round_up;
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) - 1u))};
	auto const lower{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional - (half_ulp >> 1u), 10u, uint64_max))};
	if (digit < lower)
	{
		digit = lower;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto const power_high{cached_data.powers[-decimal_exponent - 1].hi};
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
/// @brief Converts one regular binary32 value while exposing a loop-invariant AArch64 power-table base.
/// @details This variant is reserved for staged preparation; scalar formatting keeps its more compact address form.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32_staged(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto base{cached_data.powers.data + power10_cache::size + power10_cache::minimum_exponent};
	if (!::std::is_constant_evaluated())
	{
		__asm__("" : "+r"(base));
	}
	auto const power_high{base[static_cast<::std::ptrdiff_t>(decimal_exponent)]};
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
#else
	return ::fast_io::details::da::compute_binary32(binary_significand, raw_exponent);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary64(
	::std::uint_least64_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
#if defined(__APPLE__) && defined(__SIZEOF_INT128__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = static_cast<::std::int_least32_t>(::fast_io::intrinsics::umulh(
		static_cast<::std::uint_least64_t>(static_cast<::std::int_least64_t>(binary_exponent)),
		static_cast<::std::uint_least64_t>(78913) << 46u));
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) + 6u))};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << 62u))
	{
		digit = 2u;
	}
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power.hi >> (extra_shift + 1u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{fractional + half_ulp < fractional};
	auto const round_down{half_ulp > fractional};
	integral += round_up;
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

} // namespace fast_io::details::da
