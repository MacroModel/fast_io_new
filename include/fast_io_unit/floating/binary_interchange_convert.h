#pragma once

#include "binary_interchange_type.h"
#include "../decimal/round.h"

namespace fast_io::details
{
inline constexpr bool binary_words_nonzero(binary128_words w) noexcept
{
	return w.low || w.high;
}
inline constexpr int binary_words_width(binary128_words w) noexcept
{
	return w.high ? 64 + static_cast<int>(::std::bit_width(w.high)) : static_cast<int>(::std::bit_width(w.low));
}

/// Positive shifts move toward the high word. Shifts of 64 and 128 never reach
/// a native shift expression whose count equals the operand width.
inline constexpr binary128_words binary_words_shift(binary128_words w, int shift) noexcept
{
	if (shift >= 128 || shift <= -128)
	{
		return {};
	}
	if (shift >= 64)
	{
		return {0u, w.low << (shift - 64)};
	}
	if (shift <= -64)
	{
		return {w.high >> (-shift - 64), 0u};
	}
	if (shift > 0)
	{
		return {w.low << shift, (w.high << shift) | (w.low >> (64 - shift))};
	}
	if (shift < 0)
	{
		return {(w.low >> -shift) | (w.high << (64 + shift)), w.high >> -shift};
	}
	return w;
}

inline constexpr void binary_words_increment(binary128_words &w) noexcept
{
	if (++w.low == 0)
	{
		++w.high;
	}
}

struct interchange_binary_parts
{
	binary128_words coefficient{}; // finite significand or NaN diagnostic bits, with the quiet bit removed
	int exponent{};                // finite value is coefficient times 2^exponent
	bool negative{};
	decimal_class category{};
	unsigned payload_width{};
};

inline constexpr interchange_binary_parts binary_parts_from_words(binary128_words w, unsigned m, unsigned e, bool finite_nan_only = false) noexcept
{
	auto exponent{decimal_field(w, m, e)};
	auto maximum{(1u << e) - 1u};
	auto bias{static_cast<int>(maximum >> 1u)};
	auto fraction{decimal_low_bits(w, m)};
	interchange_binary_parts result{fraction, static_cast<int>(exponent ? exponent : 1u) - bias - static_cast<int>(m),
									decimal_field(w, m + e, 1u) != 0u, decimal_class::finite, m - 1u};
	if (exponent == maximum && (!finite_nan_only || fraction.low == 7u))
	{
		result.category = binary_words_nonzero(fraction) ? (decimal_field(fraction, m - 1u, 1u) ? decimal_class::quiet_nan : decimal_class::signaling_nan) : decimal_class::infinity;
		result.coefficient = decimal_low_bits(fraction, m - 1u);
	}
	else if (exponent)
	{
		decimal_set_field(result.coefficient, m, 1u, 1u);
	}
	return result;
}

template <typename T>
	requires is_software_binary<T>
inline constexpr auto binary_interchange_parts(T value) noexcept
{
	using L = binary_interchange_layout<T::format>;
	return binary_parts_from_words(binary_interchange_words(value), L::mbits, L::ebits);
}

inline constexpr binary128_words binary_words_special(unsigned m, unsigned e, bool negative, bool infinity, binary128_words payload, bool finite_nan_only = false) noexcept
{
	auto result{decimal_low_bits(payload, m - 1u)};
	if (finite_nan_only)
	{
		result = {7u, 0u};
	}
	else if (infinity)
	{
		result = {};
	}
	else
	{
		decimal_set_field(result, m - 1u, 1u, 1u);
	}
	decimal_set_field(result, m, e, (1u << e) - 1u);
	if (negative)
	{
		decimal_set_field(result, m + e, 1u, 1u);
	}
	return result;
}

inline constexpr binary128_words binary_words_pack(unsigned m, unsigned e, bool negative, binary128_words significand,
												   int top, bool finite_nan_only = false) noexcept
{
	auto const bias{static_cast<int>((1u << (e - 1u)) - 1u)};
	auto const minimum{1 - bias};
	auto const maximum{bias + static_cast<int>(finite_nan_only)};
	if (binary_words_width(significand) > static_cast<int>(m) + 1)
	{
		significand = binary_words_shift(significand, -1);
		++top;
	}
	if (maximum < top || (finite_nan_only && top == maximum && significand.low == 15u))
	{
		return binary_words_special(m, e, negative, true, {}, finite_nan_only);
	}
	auto const normal{decimal_field(significand, m, 1u) != 0u};
	auto result{decimal_low_bits(significand, m)};
	unsigned exponent{};
	if (normal)
	{
		exponent = static_cast<unsigned>((top < minimum ? minimum : top) + bias);
	}
	decimal_set_field(result, m, e, exponent);
	if (negative)
	{
		decimal_set_field(result, m + e, 1u, 1u);
	}
	return result;
}

inline constexpr binary128_words binary_cast_words(interchange_binary_parts value, unsigned m, unsigned e, bool finite_nan_only = false) noexcept
{
	if (value.category != decimal_class::finite)
	{
		auto payload{binary_words_shift(value.coefficient, static_cast<int>(m - 1u) - static_cast<int>(value.payload_width))};
		return binary_words_special(m, e, value.negative, value.category == decimal_class::infinity, payload, finite_nan_only);
	}
	auto magnitude{value.coefficient};
	auto const width{binary_words_width(magnitude)};
	if (!width)
	{
		return binary_words_pack(m, e, value.negative, {}, 0, finite_nan_only);
	}
	auto const top{value.exponent + width - 1};
	auto const minimum{2 - static_cast<int>(1u << (e - 1u))};
	auto const cut{(minimum < top ? top : minimum) - static_cast<int>(m) - value.exponent};
	auto retained{binary_words_shift(magnitude, -cut)};
	if (0 < cut && cut <= width)
	{
		auto const guard{decimal_field(magnitude, static_cast<unsigned>(cut - 1), 1u)};
		auto const sticky{binary_words_nonzero(decimal_low_bits(magnitude, static_cast<unsigned>(cut - 1)))};
		if (guard && (sticky || (retained.low & 1u)))
		{
			binary_words_increment(retained);
		}
	}
	return binary_words_pack(m, e, value.negative, retained, top, finite_nan_only);
}

/// Narrows a software binary32 or binary128 value directly to binary16.
template <typename T>
	requires(::std::same_as<T, software_float32> || ::std::same_as<T, software_float128>)
inline constexpr software_float16 binary_narrow_to_half(T source) noexcept
{
	constexpr bool wide{::std::same_as<T, software_float128>};
	using L = binary_interchange_layout<T::format>;
	using word = ::std::conditional_t<wide, ::std::uint_least64_t, ::std::uint_least32_t>;
	constexpr unsigned fraction_bits{wide ? 48u : 23u};
	constexpr word hidden_bit{word{1u} << fraction_bits};
	constexpr unsigned half_fraction_bits{10u};
	constexpr int half_bias{15};
	constexpr unsigned round_bits{fraction_bits - half_fraction_bits};

	auto raw{binary_interchange_words(source)};
	auto const upper{static_cast<word>(wide ? raw.high : raw.low)};
	auto const sign{static_cast<unsigned>(upper >> (fraction_bits + L::ebits)) << 15u};
	auto fraction{upper & (hidden_bit - 1u)};
	if constexpr (wide)
	{
		// Every rounding window below discards the low bit of the high
		// word. Fold the full low word into it for an exact sticky bit.
		fraction |= static_cast<word>(raw.low != 0u);
	}
	constexpr unsigned source_exponent_max{(1u << L::ebits) - 1u};
	auto const source_exponent{static_cast<unsigned>(upper >> fraction_bits) & source_exponent_max};
	auto const exponent{static_cast<int>(source_exponent) - (L::bias - half_bias)};
	auto round = [](word significand, unsigned cut) constexpr {
		auto const midpoint{word{1u} << (cut - 1u)};
		auto const parity{(significand >> cut) & 1u};
		// Half an ulp minus one and the retained parity implement ties-to-even.
		return static_cast<unsigned>((significand + midpoint - 1u + parity) >> cut);
	};
	if (static_cast<unsigned>(exponent - 1) < 30u)
	{
		auto const prefix{static_cast<unsigned>(exponent) << half_fraction_bits};
		return software_float16::from_bits(static_cast<::std::uint_least16_t>(sign | (prefix + round(fraction, round_bits))));
	}
	if (exponent >= 31)
	{
		unsigned magnitude{0x7c00u};
		if (source_exponent == source_exponent_max && fraction)
		{
			// Keep the leading diagnostic bits and quiet signaling NaNs.
			magnitude |= 0x200u | static_cast<unsigned>((fraction >> round_bits) & 0x1ffu);
		}
		return software_float16::from_bits(static_cast<::std::uint_least16_t>(sign | magnitude));
	}
	if (exponent < -10)
	{
		return software_float16::from_bits(static_cast<::std::uint_least16_t>(sign));
	}
	auto const cut{round_bits + static_cast<unsigned>(1 - exponent)};
	return software_float16::from_bits(static_cast<::std::uint_least16_t>(sign | round(fraction | hidden_bit, cut)));
}

template <binary_format F>
struct binary_interchange_decimal_digits
{
	// A binary128 midpoint may require 11564 decimal digits. The narrower
	// bounds cover their smallest subnormal and one additional midpoint bit.
	inline static constexpr unsigned capacity{F == binary_format::binary128 ? 12000u : F == binary_format::binary64 ? 800u
																				   : F == binary_format::binary16   ? 48u
																													: 128u};
	unsigned char digits[capacity]{};
	::std::size_t size{1};
	int exponent{};
};

template <typename T>
	requires is_software_binary<T>
inline constexpr auto binary_interchange_exact_decimal(T value) noexcept
{
	using decimal = binary_interchange_decimal_digits<T::format>;
	decimal result{};
	auto const parts{binary_interchange_parts(value)};
	auto coefficient{parts.coefficient};
	if (!binary_words_nonzero(coefficient))
	{
		return result;
	}
	int exponent{parts.exponent};
	while (exponent < 0 && !(coefficient.low & 1u))
	{
		coefficient = binary_words_shift(coefficient, -1);
		++exponent;
	}
	constexpr unsigned capacity{(decimal::capacity + 8u) / 9u};
	::std::uint_least32_t limbs[capacity]{};
	unsigned size{};
	do
	{
		limbs[size++] = decimal_divide_billion(coefficient);
	} while (binary_words_nonzero(coefficient));
	auto remaining{static_cast<unsigned>(exponent < 0 ? -exponent : exponent)};
	result.exponent = exponent < 0 ? exponent : 0;
	// 5^13 and 2^30 fit one multiplication of a radix-10^9 limb in uint64.
	while (remaining)
	{
		unsigned limit{exponent < 0 ? 13u : 30u};
		unsigned step{remaining < limit ? remaining : limit};
		::std::uint_least64_t factor{1u};
		if (exponent < 0)
		{
			for (unsigned i{}; i != step; ++i)
			{
				factor *= 5u;
			}
		}
		else
		{
			factor <<= step;
		}
		::std::uint_least64_t carry{};
		for (unsigned i{}; i != size; ++i)
		{
			auto const product{limbs[i] * factor + carry};
			limbs[i] = static_cast<::std::uint_least32_t>(product % 1000000000u);
			carry = product / 1000000000u;
		}
		while (carry)
		{
			limbs[size++] = static_cast<::std::uint_least32_t>(carry % 1000000000u);
			carry /= 1000000000u;
		}
		remaining -= step;
	}
	unsigned char top[9]{};
	exact_decimal_write_nine_digits(top, limbs[--size]);
	unsigned first{};
	while (first != 8u && !top[first])
	{
		++first;
	}
	result.size = 9u - first;
	::fast_io::details::non_overlapped_copy_n(top + first, result.size, result.digits);
	while (size)
	{
		exact_decimal_write_nine_digits(result.digits + result.size, limbs[--size]);
		result.size += 9u;
	}
	exact_precision_trim(result);
	return result;
}

struct binary_interchange_division
{
	binary128_words quotient;
	int half;
	bool remainder;
};

template <unsigned precision, ::std::size_t capacity>
inline constexpr binary_interchange_division binary_interchange_divide(scan_decfloat_bigint<capacity> &numerator,
																	   scan_decfloat_bigint<capacity> &denominator, int shift) noexcept
{
	if constexpr (precision <= scan_decfloat_quotient_bits)
	{
		auto division{scan_decfloat_bigint_div_shifted_to_quotient(numerator, denominator, shift)};
		return {decimal_split_words(division.quotient), division.twice_remainder_compare, division.remainder_nonzero};
	}
	else
	{
		// Only the portable binary128 destination needs the two-word quotient.
		// The chosen quantum bounds the quotient to at most 113 bits, irrespective
		// of the thousands of bits in the power of five.
		if (0 < shift)
		{
			(void)scan_decfloat_bigint_shift_left_inplace(numerator, static_cast<unsigned>(shift));
		}
		else if (shift < 0)
		{
			(void)scan_decfloat_bigint_shift_left_inplace(denominator, static_cast<unsigned>(-shift));
		}
		auto qbit{static_cast<int>(scan_decfloat_bigint_bit_width(numerator)) - static_cast<int>(scan_decfloat_bigint_bit_width(denominator))};
		binary128_words quotient{};
		if (qbit >= 0)
		{
			scan_decfloat_bigint<capacity> aligned{};
			(void)scan_decfloat_bigint_shift_left(aligned, denominator, static_cast<unsigned>(qbit));
			for (;;)
			{
				if (scan_decfloat_bigint_compare(numerator, aligned) >= 0)
				{
					scan_decfloat_bigint_sub_assign(numerator, aligned);
					decimal_set_field(quotient, static_cast<unsigned>(qbit), 1u, 1u);
				}
				if (!qbit--)
				{
					break;
				}
				scan_decfloat_bigint_shift_right_one_inplace(aligned);
			}
		}
		auto const nonzero{numerator.size != 0u};
		(void)scan_decfloat_bigint_shl1_add_bit(numerator, false);
		return {quotient, scan_decfloat_bigint_compare(numerator, denominator), nonzero};
	}
}

template <typename T, decimal_rounding R = decimal_rounding::nearest_to_even, typename Digits>
	requires is_software_binary<T>
inline constexpr T binary_interchange_from_decimal(Digits const &source, bool negative, bool sticky = false) noexcept
{
	using L = binary_interchange_layout<T::format>;
	constexpr auto m{L::mbits}, e{L::ebits};
	constexpr auto minimum{1 - L::bias};
	auto overflow = [&]() constexpr {
		auto mode{decimal_effective_rounding(R)};
		bool finite{mode == decimal_rounding::toward_zero || (negative && mode == decimal_rounding::toward_plus_infinity) || (!negative && mode == decimal_rounding::toward_minus_infinity)};
		auto w{binary_words_special(m, e, negative, true, {})};
		if (finite)
		{
			if (!w.low--)
			{
				--w.high;
			}
		}
		return binary_interchange_from_words<T>(w);
	};
	auto zero = [&](bool minimum_unit) constexpr {
		return binary_interchange_from_words<T>(binary_words_pack(m, e, negative, {static_cast<unsigned>(minimum_unit), 0u}, minimum));
	};
	if (!source.size || !source.digits[0])
	{
		return zero(false);
	}
	// Decimal exponents strictly beyond these guards cannot reach any nearest
	// rounding boundary. Even a million unneeded zeroes cannot expand storage.
	auto adjusted{static_cast<::std::int_least64_t>(source.exponent) + static_cast<::std::int_least64_t>(source.size) - 1};
	constexpr int guard{T::format == binary_format::binary128 ? 5000 : T::format == binary_format::binary64 ? 350
																											: 60};
	if (guard < adjusted)
	{
		return overflow();
	}
	if (adjusted < -guard)
	{
		return zero(decimal_increment(R, negative, 0u, true, false));
	}
	constexpr unsigned cap{binary_interchange_decimal_digits<T::format>::capacity};
	auto const keep{source.size < cap ? source.size : cap};
	auto const exp{static_cast<int>(adjusted) - static_cast<int>(keep) + 1};
	for (auto i{keep}; i < source.size; ++i)
	{
		sticky |= source.digits[i] != 0u;
	}
	constexpr unsigned limbs{T::format == binary_format::binary128 ? 640u : T::format == binary_format::binary64 ? 48u
																												 : 12u};
	// For d retained digits and adjusted decimal exponent a, the ratio is
	// (C*5^max(e,0))/5^max(-e,0), with e=a+1-d. Bound both operands and
	// the alignment used to extract p bits. 3.322 and 2.322 strictly bound
	// log2(10) and log2(5). A ratio below the least subnormal can enlarge
	// the aligned denominator by at most subnormal_slack bits. Two scratch
	// limbs cover the pessimistic word-shift check and normalized division.
	constexpr int max_digits{static_cast<int>(cap) < guard + 1 ? guard + 1 : static_cast<int>(cap)};
	constexpr int numerator_bits{(3322 * max_digits + 999) / 1000};
	constexpr int denominator_bits{(2322 * (guard + static_cast<int>(cap) - 1) + 999) / 1000};
	constexpr int subnormal_slack{(3322 * guard + 999) / 1000 + minimum - static_cast<int>(m)};
	static_assert(numerator_bits + (subnormal_slack > 0 ? subnormal_slack : 0) + 1 + 128 <= static_cast<int>(64u * limbs));
	static_assert(denominator_bits + static_cast<int>(m) + 2 + 128 <= static_cast<int>(64u * limbs));
	scan_decfloat_bigint<limbs> numerator{}, denominator{};
	scan_decfloat_bigint_set_u64(denominator, 1u);
	for (::std::size_t i{}; i != keep; ++i)
	{
		(void)scan_decfloat_bigint_mul_small(numerator, 10u);
		(void)scan_decfloat_bigint_add_small(numerator, source.digits[i]);
	}
	if (exp < 0)
	{
		(void)scan_decfloat_bigint_mul_pow5(denominator, static_cast<unsigned>(-exp));
	}
	else
	{
		(void)scan_decfloat_bigint_mul_pow5(numerator, static_cast<unsigned>(exp));
	}
	auto top{static_cast<int>(scan_decfloat_bigint_floor_log2_ratio(numerator, denominator)) + exp};
	if (L::bias < top)
	{
		return overflow();
	}
	auto const quantum{(top < minimum ? minimum : top) - static_cast<int>(m)};
	auto division{binary_interchange_divide<m + 1u>(numerator, denominator, exp - quantum)};
	unsigned guard_digit{division.half < 0 ? 0u : division.half == 0 ? 5u
																	 : 6u};
	auto tail{sticky || (division.half < 0 && division.remainder)};
	if (decimal_increment(R, negative, guard_digit, tail, static_cast<bool>(division.quotient.low & 1u)))
	{
		binary_words_increment(division.quotient);
	}
	if (top == L::bias && static_cast<int>(m) + 1 < binary_words_width(division.quotient))
	{
		return overflow();
	}
	return binary_interchange_from_words<T>(binary_words_pack(m, e, negative, division.quotient, top));
}
} // namespace fast_io::details
