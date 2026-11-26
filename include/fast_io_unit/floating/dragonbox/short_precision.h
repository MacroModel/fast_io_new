#pragma once

namespace fast_io::details
{

inline constexpr ::std::int_least32_t mul_ln2_div_ln10_floor(::std::int_least32_t) noexcept;
inline constexpr ::std::int_least32_t mul_ln10_div_ln2_floor(::std::int_least32_t) noexcept;

struct short_decimal_precision_result
{
	::std::uint_least32_t coefficient;
	::std::int_least32_t exponent;
};

// Nearest conversion to at most nine digits needs scales in [-307, 332].
// The DA cache supplies [-293, 324]. These are the high words of the few
// remaining powers, floor(10^scale * 2^(63 - floor(log2(10^scale)))).
inline constexpr ::std::uint_least64_t short_decimal_precision_low_powers[]{
	0x8fd0c16206306babULL, 0xb3c4f1ba87bc8696ULL, 0xe0b62e2929aba83cULL,
	0x8c71dcd9ba0b4925ULL, 0xaf8e5410288e1b6fULL, 0xdb71e91432b1a24aULL,
	0x892731ac9faf056eULL, 0xab70fe17c79ac6caULL, 0xd64d3d9db981787dULL,
	0x85f0468293f0eb4eULL, 0xa76c582338ed2621ULL, 0xd1476e2c07286faaULL,
	0x82cca4db847945caULL, 0xa37fce126597973cULL};

inline constexpr ::std::uint_least64_t short_decimal_precision_high_powers[]{
	0xc5a05277621be293ULL, 0xf70867153aa2db38ULL, 0x9a65406d44a5c903ULL,
	0xc0fe908895cf3b44ULL, 0xf13e34aabb430a15ULL, 0x96c6e0eab509e64dULL,
	0xbc789925624c5fe0ULL, 0xeb96bf6ebadf77d8ULL};

[[nodiscard]] inline constexpr ::std::uint_least64_t short_decimal_precision_power(
	::std::int_least32_t scale) noexcept
{
	if (scale < -293) [[unlikely]]
	{
		return short_decimal_precision_low_powers[static_cast<::std::size_t>(scale + 307)];
	}
	if (324 < scale) [[unlikely]]
	{
		return short_decimal_precision_high_powers[static_cast<::std::size_t>(scale - 325)];
	}
	return ::fast_io::details::da::cached_data.powers[scale].hi;
}

/// @brief Rounds a nonzero finite binary32 or binary64 magnitude to 1–9 digits.
/// @return The coefficient and its scientific exponent. Zero requests the exact fallback.
template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr short_decimal_precision_result round_short_decimal_precision(
	typename iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t raw_exponent, ::std::uint_least32_t digits) noexcept
{
	using trait = iec559_traits<flt>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(1u << (trait::ebits - 1u)) - 1u)};
	constexpr auto normal_shift{static_cast<::std::uint_least32_t>(63u - trait::mbits)};
	auto normalized{static_cast<::std::uint_least64_t>(mantissa)};
	auto binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - bias};
	if (raw_exponent)
	{
		normalized = (normalized << normal_shift) | (::std::uint_least64_t{1} << 63u);
	}
	else
	{
		auto const shift{static_cast<::std::uint_least32_t>(::std::countl_zero(normalized))};
		normalized <<= shift;
		binary_exponent += 1 + static_cast<::std::int_least32_t>(normal_shift) -
			static_cast<::std::int_least32_t>(shift);
	}

	// With b = floor(log2(x)), floor(log10(2^b)) is exact or one below
	// floor(log10(x)). The guessed scale gives 10^(digits-1) <= x*10^scale
	// < 2*10^digits. The high product word underestimates the exact value
	// on that binary grid by fewer than three units. The strict lower
	// cache word loses less than two and truncating the product loses
	// less than one.
	auto const decimal_exponent{::fast_io::details::mul_ln2_div_ln10_floor(binary_exponent)};
	auto const scale{static_cast<::std::int_least32_t>(digits) - 1 - decimal_exponent};
	auto const power{short_decimal_precision_power(scale) - 1u};
	auto const product{::fast_io::intrinsics::umulh(normalized, power)};
	auto const power_exponent{::fast_io::details::mul_ln10_div_ln2_floor(scale)};
	auto const shift{static_cast<::std::uint_least32_t>(61 - binary_exponent - power_exponent)};
	auto const mask{(::std::uint_least64_t{1} << shift) - 1u};
	auto const doubled{static_cast<::std::uint_least32_t>(product >> shift)};
	auto const limit{static_cast<::std::uint_least32_t>(
		::fast_io::details::da::uint64_power10_table[digits])};
	bool const higher_decade{2u * limit <= doubled};

	// If the exact value can cross the next grid point, only a rounding
	// midpoint is ambiguous. Crossing the decade limit yields the same
	// normalized coefficient via a carry. Exact ties use the existing
	// formatter, which applies the caller's nearest-rounding tie policy.
	if ((~product & mask) < 2u) [[unlikely]]
	{
		bool const midpoint{higher_decade ? doubled % 20u == 9u : (doubled & 1u) == 0u};
		if (midpoint)
		{
			return {};
		}
	}

	// The higher decade has a 20-step rounding period in this grid.
	// Both sums fit since doubled is below 4*10^9.
	auto const rounded{(doubled + 1u) / 2u};
	auto const rebased{(doubled + 10u) / 20u};
	auto const coefficient{higher_decade ? rebased : rounded};
	bool const carry{coefficient == limit};
	return {carry ? limit / 10u : coefficient,
		decimal_exponent + static_cast<::std::int_least32_t>(higher_decade || carry)};
}

} // namespace fast_io::details
