#pragma once

#include "punning.h"
#include "roundtrip.h"

#if defined(__SIZEOF_INT128__)

namespace fast_io::details
{

/*
`wide_shortest_result` is the presentation-independent carrier for exact
binary80/binary128 shortest conversion.  A binary80 shortest coefficient may
contain 21 decimal digits, so a uint64_t carrier is not sufficient.  Every
currently admitted IEC 60559 domain needs at most 37 digits and therefore fits
in native uint128 together with the one-unit upper candidate.
*/
struct wide_shortest_result
{
	__uint128_t m10{};
	::std::int_least32_t e10{};
	bool success{};
};

inline constexpr void wide_shortest_trim(
	__uint128_t &coefficient, ::std::int_least32_t &exponent) noexcept
{
	while (coefficient && coefficient % 10u == 0u)
	{
		coefficient /= 10u;
		++exponent;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool wide_shortest_prefer_upper(
	__uint128_t prefix, unsigned char guard, bool after_guard,
	bool negative) noexcept
{
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (5u < guard || (guard == 5u && after_guard))
		{
			return true;
		}
		if (guard < 5u)
		{
			return false;
		}
		if constexpr (rounding ==
					  ::fast_io::manipulators::floating_rounding::nearest_to_even)
		{
			return (prefix & 1u) != 0u;
		}
		else if constexpr (rounding ==
						   ::fast_io::manipulators::floating_rounding::nearest_to_odd)
		{
			return (prefix & 1u) == 0u;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_toward_plus_infinity)
		{
			return !negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_toward_minus_infinity)
		{
			return negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_away_from_zero)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(
			negative);
	}
}

/*
Convert an arbitrary positive dyadic `significand * 2^binary_exponent` to the
same canonical exact decimal used by the precision backend.  Interval
endpoints need one more significand bit than the source format, so they cannot
be forged into a raw floating field and passed to exact_precision_from_binary.
The base-1e9 recurrence and its capacity proof are otherwise identical.
*/
template <typename flt>
[[nodiscard]] inline constexpr ::fast_io::details::exact_precision_decimal<flt>
wide_shortest_exact_from_significand(
	__uint128_t significand, ::std::int_least32_t binary_exponent) noexcept
{
	::fast_io::details::exact_precision_limb_type
		limbs[::fast_io::details::exact_precision_limb_capacity<flt>]{};
	::std::size_t limb_size{};
	::fast_io::details::exact_precision_initialize_wide_limbs(
		limbs, limb_size, significand);
	auto decimal_exponent{binary_exponent < 0 ? binary_exponent : 0};
	if (binary_exponent < 0)
	{
		auto count{static_cast<::std::uint_least32_t>(-binary_exponent)};
		for (; ::fast_io::details::exact_precision_pow5_chunk <= count;
			 count -= ::fast_io::details::exact_precision_pow5_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_pow5_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_small_power(5u, count));
		}
	}
	else
	{
		auto count{static_cast<::std::uint_least32_t>(binary_exponent)};
		for (; ::fast_io::details::exact_precision_pow2_chunk <= count;
			 count -= ::fast_io::details::exact_precision_pow2_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_pow2_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				static_cast<::fast_io::details::exact_precision_multiplier_type>(1u)
					<< count);
		}
	}

	::fast_io::details::exact_precision_decimal<flt> decimal{};
	decimal.exponent = decimal_exponent;
	auto top{limbs[limb_size - 1u]};
	unsigned char reversed[::fast_io::details::exact_precision_limb_digits + 1u]{};
	::std::size_t reversed_size{};
	for (; top; top /= 10u)
	{
		reversed[reversed_size++] = static_cast<unsigned char>(top % 10u);
	}
	while (reversed_size)
	{
		decimal.digits[decimal.size++] = reversed[--reversed_size];
	}
	for (auto index{limb_size - 1u}; index; --index)
	{
		auto value{limbs[index - 1u]};
		auto position{
			decimal.size + ::fast_io::details::exact_precision_limb_digits};
		decimal.size = position;
		for (unsigned digit{};
			 digit != ::fast_io::details::exact_precision_limb_digits; ++digit)
		{
			decimal.digits[--position] =
				static_cast<unsigned char>(value % 10u);
			value /= 10u;
		}
	}
	while (decimal.size != 1u && !decimal.digits[decimal.size - 1u])
	{
		--decimal.size;
		++decimal.exponent;
	}
	return decimal;
}

inline constexpr ::std::size_t wide_shortest_compact_digits{40u};

struct wide_shortest_compact_decimal
{
	unsigned char digits[wide_shortest_compact_digits]{};
	::std::size_t stored_size{};
	::std::size_t full_size{};
	::std::int_least32_t exponent{};
	bool zero{};
};

/*
The caller invokes this helper in separate full expressions for each endpoint.
Only the 40-byte quotient escapes, so the three binary80/binary128 exact
objects have disjoint lifetimes and one exact buffer is the semantic peak.
Forty digits cover the 37-digit carrier, a possible carry digit, and two proof
digits.  If a longer canonical endpoint shares the stored prefix, its final
digit is nonzero and therefore the omitted tail is known to be positive.
*/
template <typename flt>
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_compact_decimal
wide_shortest_compact_from_significand(
	__uint128_t significand, ::std::int_least32_t binary_exponent) noexcept
{
	if (!significand)
	{
		wide_shortest_compact_decimal result{};
		result.digits[0] = 0u;
		result.stored_size = result.full_size = 1u;
		result.zero = true;
		return result;
	}
	auto const exact{
		::fast_io::details::wide_shortest_exact_from_significand<flt>(
			significand, binary_exponent)};
	wide_shortest_compact_decimal result{};
	result.full_size = exact.size;
	result.stored_size = exact.size < wide_shortest_compact_digits
							 ? exact.size
							 : wide_shortest_compact_digits;
	result.exponent = exact.exponent;
	for (::std::size_t index{}; index != result.stored_size; ++index)
	{
		result.digits[index] = exact.digits[index];
	}
	return result;
}

[[nodiscard]] inline constexpr unsigned wide_shortest_u128_digits(
	__uint128_t value) noexcept
{
	unsigned size{1u};
	for (; 10u <= value; value /= 10u)
	{
		++size;
	}
	return size;
}

/* Return negative, zero, or positive according as candidate is below, equal
   to, or above endpoint. */
[[nodiscard]] inline constexpr int wide_shortest_compare_candidate(
	__uint128_t coefficient, ::std::int_least32_t exponent,
	::fast_io::details::wide_shortest_compact_decimal const &endpoint) noexcept
{
	if (endpoint.zero)
	{
		return coefficient ? 1 : 0;
	}
	unsigned char candidate_digits[wide_shortest_compact_digits]{};
	auto const candidate_size{
		static_cast<::std::size_t>(
			::fast_io::details::wide_shortest_u128_digits(coefficient))};
	auto cursor{candidate_size};
	for (auto value{coefficient}; cursor; value /= 10u)
	{
		candidate_digits[--cursor] = static_cast<unsigned char>(value % 10u);
	}
	auto const candidate_real_exponent{
		static_cast<::std::int_least64_t>(exponent) +
		static_cast<::std::int_least64_t>(candidate_size) - 1};
	auto const endpoint_real_exponent{
		static_cast<::std::int_least64_t>(endpoint.exponent) +
		static_cast<::std::int_least64_t>(endpoint.full_size) - 1};
	if (candidate_real_exponent != endpoint_real_exponent)
	{
		return candidate_real_exponent < endpoint_real_exponent ? -1 : 1;
	}
	auto const compare_size{candidate_size < endpoint.stored_size
								? endpoint.stored_size
								: candidate_size};
	for (::std::size_t index{}; index != compare_size; ++index)
	{
		auto const candidate_digit{
			index < candidate_size ? candidate_digits[index] : 0u};
		auto const endpoint_digit{
			index < endpoint.stored_size ? endpoint.digits[index] : 0u};
		if (candidate_digit != endpoint_digit)
		{
			return candidate_digit < endpoint_digit ? -1 : 1;
		}
	}
	// The candidate has at most 38 digits.  A canonical endpoint longer than
	// the retained 40 digits has a positive omitted tail, so equality of the
	// retained prefix still places the candidate strictly below it.
	return endpoint.stored_size < endpoint.full_size ? -1 : 0;
}

struct wide_shortest_interval
{
	wide_shortest_compact_decimal lower;
	wide_shortest_compact_decimal upper;
	bool lower_closed{};
	bool upper_closed{};
};

template <typename flt,
		  ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_interval
wide_shortest_make_interval(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t raw_exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	__uint128_t significand{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t binary_exponent{};
	if (raw_exponent)
	{
		significand |= static_cast<__uint128_t>(1u) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(raw_exponent) -
						  bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}
	wide_shortest_interval result{};
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		auto const shorter{!mantissa && 1u < raw_exponent};
		if (shorter)
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					(significand << 2u) - 1u, binary_exponent - 2);
			result.lower_closed = ::fast_io::details::
				dragonbox_nearest_shorter_left_closed<rounding>(negative);
			result.upper_closed = ::fast_io::details::
				dragonbox_nearest_shorter_right_closed<rounding>(negative);
		}
		else
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					(significand << 1u) - 1u, binary_exponent - 1);
			auto const even{(significand & 1u) == 0u};
			result.lower_closed = ::fast_io::details::
				dragonbox_nearest_normal_left_closed<rounding>(negative, even);
			result.upper_closed = ::fast_io::details::
				dragonbox_nearest_normal_right_closed<rounding>(negative, even);
		}
		result.upper = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				(significand << 1u) + 1u, binary_exponent - 1);
	}
	else
	{
		auto const right_closed{
			::fast_io::details::floating_rounding_directed_round_up<rounding>(
				negative)};
		if (right_closed)
		{
			auto const shorter{!mantissa && 1u < raw_exponent};
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					shorter ? (significand << 1u) - 1u : significand - 1u,
					shorter ? binary_exponent - 1 : binary_exponent);
			result.upper = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand, binary_exponent);
			result.lower_closed = false;
			result.upper_closed = true;
		}
		else
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand, binary_exponent);
			result.upper = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand + 1u, binary_exponent);
			result.lower_closed = true;
			result.upper_closed = false;
		}
	}
	return result;
}

[[nodiscard]] inline constexpr bool wide_shortest_interval_contains(
	::fast_io::details::wide_shortest_interval const &interval,
	__uint128_t coefficient, ::std::int_least32_t exponent) noexcept
{
	auto const lower_order{::fast_io::details::wide_shortest_compare_candidate(
		coefficient, exponent, interval.lower)};
	if (lower_order < 0 || (!lower_order && !interval.lower_closed))
	{
		return false;
	}
	auto const upper_order{::fast_io::details::wide_shortest_compare_candidate(
		coefficient, exponent, interval.upper)};
	return upper_order < 0 || (!upper_order && interval.upper_closed);
}

/*
Let the canonical exact expansion be D*10^e with N digits.  For a requested
P, L=floor(D/10^(N-P))*10^(e+N-P) and U=L+10^(e+N-P) bracket the exact binary
value.  Every IEC 60559 rounding preimage is one monotone interval containing
that exact value.  Therefore, if any P-digit decimal round-trips, L or U does;
a farther grid point cannot enter the interval without the nearer bracketing
point entering first.  Trying only those two candidates is complete.

P is visited in increasing order, proving minimal significant-digit count.
When both candidates round-trip, `wide_shortest_prefer_upper` applies the exact
decimal-distance/tie policy used by the narrow Dragonbox implementation.  The
IEC 60559 max_digits10 separation bound guarantees a successful bracketing
candidate by `m10digits` (21 for binary80, 37 for binary128), including every
directed and nearest endpoint convention.  The final carrier plus one is below
10^37 for the admitted wide types, strictly inside uint128.
*/
template <typename flt,
		  ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__pure__)
[[__gnu__::__pure__]]
#endif
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_result
wide_shortest_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t binary_exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	static_assert(sizeof(mantissa_type) <= sizeof(__uint128_t));
	static_assert(trait::m10digits <= 38u);
	static_assert(rounding !=
					  ::fast_io::manipulators::floating_rounding::current_environment,
				  "wide shortest materialization requires one explicit rounding policy");
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (binary_exponent == exponent_mask)
	{
		return {};
	}
	if (!mantissa && !binary_exponent)
	{
		return {0u, 0, true};
	}
	auto const interval{
		::fast_io::details::wide_shortest_make_interval<flt, rounding>(
			mantissa, binary_exponent, negative)};
	auto const decimal{::fast_io::details::exact_precision_from_binary<flt>(
		mantissa, binary_exponent)};
	__uint128_t prefix{};
	constexpr auto maximum_digits{
		static_cast<::std::size_t>(trait::m10digits)};
	auto const limit{
		decimal.size < maximum_digits ? decimal.size : maximum_digits};
	for (::std::size_t precision{1u}; precision <= limit; ++precision)
	{
		prefix = prefix * 10u + decimal.digits[precision - 1u];
		auto const candidate_exponent{static_cast<::std::int_least32_t>(
			decimal.exponent +
			static_cast<::std::int_least32_t>(decimal.size - precision))};
		if (precision == decimal.size)
		{
			return {prefix, candidate_exponent,
					::fast_io::details::wide_shortest_interval_contains(
						interval, prefix, candidate_exponent)};
		}
		auto const guard{decimal.digits[precision]};
		// exact_precision_from_binary removes every trailing decimal zero.  Thus
		// a digit after the guard exists iff the discarded tail after it is nonzero.
		auto const after_guard{precision + 1u < decimal.size};
		auto const prefer_upper{
			::fast_io::details::wide_shortest_prefer_upper<rounding>(
				prefix, guard, after_guard, negative)};
		auto try_candidate = [&](bool upper) constexpr noexcept {
			auto coefficient{
				prefix + static_cast<__uint128_t>(upper)};
			auto exponent{candidate_exponent};
			::fast_io::details::wide_shortest_trim(coefficient, exponent);
			return ::fast_io::details::wide_shortest_result{
				coefficient, exponent,
				::fast_io::details::wide_shortest_interval_contains(
					interval, coefficient, exponent)};
		};
		auto const preferred{try_candidate(prefer_upper)};
		if (preferred.success)
		{
			return preferred;
		}
		auto const alternate{try_candidate(!prefer_upper)};
		if (alternate.success)
		{
			return alternate;
		}
	}
	return {};
}

} // namespace fast_io::details

#endif
