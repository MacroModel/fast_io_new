#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_freestanding.h>
#include <fast_io_unit/floating/decfloat.h>
#include <fast_io_unit/floating/wide_shortest.h>

#if defined(__SIZEOF_INT128__)

namespace
{

using rounding = ::fast_io::manipulators::floating_rounding;

struct decimal_candidate
{
	__uint128_t coefficient{};
	::std::int_least32_t exponent{};
};

struct reference_result
{
	decimal_candidate value{};
	bool success{};
};

[[nodiscard]] inline constexpr decimal_candidate normalize(
	decimal_candidate value) noexcept
{
	while (value.coefficient && value.coefficient % 10u == 0u)
	{
		value.coefficient /= 10u;
		++value.exponent;
	}
	return value;
}

[[nodiscard]] inline constexpr unsigned decimal_digits(
	__uint128_t value) noexcept
{
	unsigned result{1u};
	while (10u <= value)
	{
		value /= 10u;
		++result;
	}
	return result;
}

template <typename flt>
inline constexpr void make_scanner_state(
	::fast_io::details::scan_decfloat_significand_state<flt> &state,
	__uint128_t coefficient) noexcept
{
	char8_t reversed[40u]{};
	::std::size_t size{};
	do
	{
		reversed[size++] = static_cast<char8_t>(coefficient % 10u);
		coefficient /= 10u;
	} while (coefficient);
	state.has_digit = true;
	state.has_nonzero_digit = !(size == 1u && reversed[0] == 0u);
	state.significant_digits = size;
	state.stored_digits = size < 19u ? size : 19u;
	for (::std::size_t index{}; index != state.stored_digits; ++index)
	{
		state.significand = state.significand * 10u +
							static_cast<::std::uint_least64_t>(reversed[size - index - 1u]);
	}
	for (auto index{size}; index;)
	{
		auto const digit{reversed[--index]};
		if (19u <= state.exact_stored_digits && digit)
		{
			state.truncated_nonzero = true;
		}
		::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
	}
}

template <typename flt>
[[nodiscard]] inline constexpr bool same_fields(flt const &left,
												flt const &right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa && lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

/*
The decfloat nearest-to-odd kernel is a jamming primitive rather than the
public halfway-to-odd policy.  The two directed-nearest parses agree away from
a midpoint; at a midpoint their adjacent results let the odd significand bit
select the public result without depending on the shortest implementation.
*/
template <typename flt, rounding policy>
[[nodiscard]] inline constexpr bool roundtrips(decimal_candidate candidate,
											   bool negative, flt const &source) noexcept
{
	::fast_io::details::scan_decfloat_significand_state<flt> state{};
	make_scanner_state(state, candidate.coefficient);
	flt parsed{};
	if constexpr (policy == rounding::nearest_to_odd)
	{
		flt toward_zero{};
		flt away{};
		auto const zero_code{::fast_io::details::scan_decfloat_assign<
			flt, rounding::nearest_toward_zero>(
			toward_zero, negative, state, candidate.exponent)};
		auto const away_code{::fast_io::details::scan_decfloat_assign<
			flt, rounding::nearest_away_from_zero>(
			away, negative, state, candidate.exponent)};
		if (zero_code != ::fast_io::parse_code::ok ||
			away_code != ::fast_io::parse_code::ok)
		{
			return false;
		}
		auto const zero_fields{
			::fast_io::details::get_punned_result(toward_zero)};
		auto const away_fields{::fast_io::details::get_punned_result(away)};
		if (zero_fields.mantissa == away_fields.mantissa &&
			zero_fields.exponent == away_fields.exponent)
		{
			parsed = toward_zero;
		}
		else
		{
			parsed = (zero_fields.mantissa & 1u) ? toward_zero : away;
		}
	}
	else
	{
		auto const code{::fast_io::details::scan_decfloat_assign<flt, policy>(
			parsed, negative, state, candidate.exponent)};
		if (code != ::fast_io::parse_code::ok)
		{
			return false;
		}
	}
	return same_fields(parsed, source);
}

template <rounding policy>
[[nodiscard]] inline constexpr bool reference_tie_rounds_up(
	__uint128_t lower, bool negative) noexcept
{
	if constexpr (policy == rounding::nearest_to_even)
	{
		return (lower & 1u) != 0u;
	}
	else if constexpr (policy == rounding::nearest_to_odd)
	{
		return (lower & 1u) == 0u;
	}
	else if constexpr (policy == rounding::nearest_toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (policy == rounding::nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (policy == rounding::nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <rounding policy>
[[nodiscard]] inline constexpr bool reference_directed_rounds_up(
	bool negative) noexcept
{
	if constexpr (policy == rounding::toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (policy == rounding::toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (policy == rounding::away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*
Independent shortest oracle.  At each decimal precision the truncated exact
expansion and its successor are the two grid points bracketing the source.
Both are parsed through decfloat.  The first precision with a successful point
proves minimality; exact decimal distance and the requested tie rule select the
canonical point when both succeed.
*/
template <typename flt, rounding policy>
[[nodiscard]] inline constexpr reference_result reference_shortest(
	flt const &source) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	auto const fields{::fast_io::details::get_punned_result(source)};
	auto const exact{::fast_io::details::exact_precision_from_binary<flt>(
		fields.mantissa, fields.exponent)};
	__uint128_t prefix{};
	auto const limit{exact.size < static_cast<::std::size_t>(trait::m10digits)
						 ? exact.size
						 : static_cast<::std::size_t>(trait::m10digits)};
	for (::std::size_t precision{1u}; precision <= limit; ++precision)
	{
		prefix = prefix * 10u + exact.digits[precision - 1u];
		auto const exponent{static_cast<::std::int_least32_t>(
			exact.exponent +
			static_cast<::std::int_least32_t>(exact.size - precision))};
		auto const lower{normalize({prefix, exponent})};
		if (precision == exact.size)
		{
			return {lower,
					roundtrips<flt, policy>(lower, fields.sign, source)};
		}
		auto const upper{normalize({prefix + 1u, exponent})};
		auto const lower_ok{
			roundtrips<flt, policy>(lower, fields.sign, source)};
		auto const upper_ok{
			roundtrips<flt, policy>(upper, fields.sign, source)};
		if (!lower_ok && !upper_ok)
		{
			continue;
		}
		if (lower_ok != upper_ok)
		{
			return {lower_ok ? lower : upper, true};
		}

		bool choose_upper{};
		if constexpr (::fast_io::details::floating_rounding_is_nearest<policy>)
		{
			auto const guard{exact.digits[precision]};
			if (5u < guard || (guard == 5u && precision + 1u < exact.size))
			{
				choose_upper = true;
			}
			else if (guard == 5u)
			{
				choose_upper = reference_tie_rounds_up<policy>(
					prefix, fields.sign);
			}
		}
		else
		{
			choose_upper = reference_directed_rounds_up<policy>(fields.sign);
		}
		return {choose_upper ? upper : lower, true};
	}
	return {};
}

template <typename flt>
[[nodiscard]] inline flt make_value(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	flt value{};
	if constexpr (::fast_io::details::fp_floating_point_is_float80<flt>)
	{
		auto stored{static_cast<::std::uint_least64_t>(mantissa)};
		if (exponent)
		{
			stored |= ::std::uint_least64_t{1u} << 63u;
		}
		::fast_io::details::fp_assign_float80_bits(
			value, stored, exponent, negative);
	}
	else
	{
		auto bits{static_cast<mantissa_type>(mantissa |
											 (static_cast<mantissa_type>(exponent) << trait::mbits))};
		if (negative)
		{
			bits |= static_cast<mantissa_type>(1u) << (trait::mbits + trait::ebits);
		}
		::fast_io::details::fp_assign_bits(value, bits);
	}
	return value;
}

inline constexpr ::std::uint_least64_t next_random(
	::std::uint_least64_t &state) noexcept
{
	state = state * UINT64_C(6364136223846793005) +
			UINT64_C(1442695040888963407);
	return state;
}

template <typename mantissa_type>
[[nodiscard]] inline constexpr mantissa_type random_mantissa(
	::std::uint_least64_t &state) noexcept
{
	auto result{static_cast<mantissa_type>(next_random(state))};
	if constexpr (sizeof(mantissa_type) > sizeof(::std::uint_least64_t))
	{
		result |= static_cast<mantissa_type>(next_random(state)) << 64u;
	}
	return result;
}

template <typename flt, rounding policy>
[[nodiscard]] bool check_narrow_value(flt const &value) noexcept
{
	auto const fields{::fast_io::details::get_punned_result(value)};
	auto const result{
		::fast_io::details::wide_shortest_from_binary<flt, policy>(
			fields.mantissa, fields.exponent, fields.sign)};
	auto const expected{::fast_io::details::dragonbox_impl<flt, policy>(
		fields.mantissa,
		static_cast<::std::int_least32_t>(fields.exponent), fields.sign)};
	return result.success && result.m10 == expected.m10 &&
		   result.e10 == expected.e10;
}

consteval bool constant_binary64_open_midpoint_regression() noexcept
{
	constexpr auto source{
		::std::bit_cast<double>(UINT64_C(0xc3d5819119d3b7a8))};
	constexpr auto fields{
		::fast_io::details::get_punned_result(source)};
	constexpr auto result{
		::fast_io::details::dragonbox_impl<
			double, rounding::nearest_to_odd>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent),
			fields.sign)};
	return result.m10 == UINT64_C(6198717147617599) &&
		   result.e10 == 3;
}

static_assert(constant_binary64_open_midpoint_regression());

[[nodiscard]] bool check_binary64_open_midpoint_regression() noexcept
{
	/*
	The old recovered-cache `is_integer` test admitted 61987171476176*10^5
	for this negative binary64 under nearest-to-odd.  That decimal is exactly
	the open midpoint above the source, so reparsing selected the adjacent
	carrier.  Exact endpoint divisibility must instead continue to the finer
	grid and choose 6198717147617599*10^3.

	The asserted carrier proves canonical selection, while `roundtrips` uses
	the independent decimal scanner to prove that the chosen point belongs to
	the source interval.  Keeping both checks prevents a matching formatter and
	parser error from hiding the original open-endpoint bug.
	*/
	constexpr ::std::uint_least64_t bits{UINT64_C(0xc3d5819119d3b7a8)};
	auto const source{::std::bit_cast<double>(bits)};
	auto const fields{::fast_io::details::get_punned_result(source)};
	auto const result{
		::fast_io::details::dragonbox_impl<
			double, rounding::nearest_to_odd>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent),
			fields.sign)};
	decimal_candidate const candidate{result.m10, result.e10};
	return result.m10 == UINT64_C(6198717147617599) &&
		   result.e10 == 3 &&
		   roundtrips<double, rounding::nearest_to_odd>(
			   candidate, fields.sign, source);
}

template <typename flt, rounding policy>
[[nodiscard]] bool check_narrow_policy() noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u)};
	constexpr auto mantissa_mask{static_cast<mantissa_type>(
		(static_cast<mantissa_type>(1u) << trait::mbits) - 1u)};
	constexpr auto bias{exponent_mask / 2u};
	struct fields
	{
		mantissa_type mantissa;
		::std::uint_least32_t exponent;
		bool negative;
	};
	fields const boundaries[]{
		{1u, 0u, false},
		{mantissa_mask, 0u, true},
		{0u, 1u, false},
		{mantissa_mask, 1u, true},
		{mantissa_mask, bias - 1u, false},
		{0u, bias, false},
		{1u, bias, true},
		{mantissa_mask, exponent_mask - 1u, false}};
	for (auto const item : boundaries)
	{
		if (!check_narrow_value<flt, policy>(make_value<flt>(
				item.mantissa, item.exponent, item.negative)))
		{
			return false;
		}
	}
	::std::uint_least64_t state{UINT64_C(0x123456789abcdef0)};
	for (::std::size_t index{}; index != 2048u; ++index)
	{
		auto mantissa{static_cast<mantissa_type>(
			random_mantissa<mantissa_type>(state) & mantissa_mask)};
		auto const exponent{static_cast<::std::uint_least32_t>(
			next_random(state) % exponent_mask)};
		if (!mantissa && !exponent)
		{
			mantissa = 1u;
		}
		if (!check_narrow_value<flt, policy>(make_value<flt>(
				mantissa, exponent, static_cast<bool>(state >> 63u))))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, rounding policy>
[[nodiscard]] bool check_wide_value(flt const &value) noexcept
{
	auto const fields{::fast_io::details::get_punned_result(value)};
	auto const result{
		::fast_io::details::wide_shortest_from_binary<flt, policy>(
			fields.mantissa, fields.exponent, fields.sign)};
	auto const reference{reference_shortest<flt, policy>(value)};
	return result.success && reference.success &&
		   result.m10 == reference.value.coefficient &&
		   result.e10 == reference.value.exponent &&
		   decimal_digits(result.m10) <=
			   ::fast_io::details::iec559_traits<flt>::m10digits;
}

template <typename flt, rounding policy>
[[nodiscard]] bool check_wide_policy() noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u)};
	constexpr auto mantissa_mask{static_cast<mantissa_type>(
		(static_cast<mantissa_type>(1u) << trait::mbits) - 1u)};
	constexpr auto bias{exponent_mask / 2u};
	struct fields
	{
		mantissa_type mantissa;
		::std::uint_least32_t exponent;
		bool negative;
	};
	fields const boundaries[]{
		{1u, 0u, false},
		{mantissa_mask, 0u, true},
		{0u, 1u, false},
		{mantissa_mask, 1u, true},
		{mantissa_mask, bias - 1u, false},
		{0u, bias, false},
		{1u, bias, true},
		{mantissa_mask, exponent_mask - 1u, true}};
	for (auto const item : boundaries)
	{
		if (!check_wide_value<flt, policy>(make_value<flt>(
				item.mantissa, item.exponent, item.negative)))
		{
			return false;
		}
	}
	::std::uint_least64_t state{UINT64_C(0xfedcba9876543210)};
	for (::std::size_t index{}; index != 16u; ++index)
	{
		auto mantissa{static_cast<mantissa_type>(
			random_mantissa<mantissa_type>(state) & mantissa_mask)};
		auto const exponent{static_cast<::std::uint_least32_t>(
			next_random(state) % exponent_mask)};
		if (!mantissa && !exponent)
		{
			mantissa = 1u;
		}
		if (!check_wide_value<flt, policy>(make_value<flt>(
				mantissa, exponent, static_cast<bool>(state >> 63u))))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, rounding policy>
struct narrow_checker
{
	[[nodiscard]] static bool run() noexcept
	{
		return check_narrow_policy<flt, policy>();
	}
};

template <typename flt, rounding policy>
struct wide_checker
{
	[[nodiscard]] static bool run() noexcept
	{
		return check_wide_policy<flt, policy>();
	}
};

template <typename flt, template <typename, rounding> typename checker>
[[nodiscard]] bool run_all_policies() noexcept
{
	return checker<flt, rounding::nearest_to_even>::run() &&
		   checker<flt, rounding::nearest_to_odd>::run() &&
		   checker<flt, rounding::nearest_toward_plus_infinity>::run() &&
		   checker<flt, rounding::nearest_toward_minus_infinity>::run() &&
		   checker<flt, rounding::nearest_toward_zero>::run() &&
		   checker<flt, rounding::nearest_away_from_zero>::run() &&
		   checker<flt, rounding::toward_plus_infinity>::run() &&
		   checker<flt, rounding::toward_minus_infinity>::run() &&
		   checker<flt, rounding::toward_zero>::run() &&
		   checker<flt, rounding::away_from_zero>::run();
}

template <typename flt>
consteval bool constant_shortest_works() noexcept
{
	constexpr flt value{static_cast<flt>(1.25L)};
	constexpr auto fields{::fast_io::details::get_punned_result(value)};
	constexpr auto result{
		::fast_io::details::wide_shortest_from_binary<
			flt, rounding::nearest_to_even>(
			fields.mantissa, fields.exponent, fields.sign)};
	return result.success && result.m10 == 125u && result.e10 == -2;
}

static_assert(constant_shortest_works<long double>());

#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
static_assert(constant_shortest_works<__float128>());

/*
The minimum positive binary128 is 2^-16494 = 6.475...e-4966.  Under positive
toward-minus-infinity its decimal preimage is [x,2x), so both 7e-4966 and
1e-4965 round-trip with one significant digit.  The former is closer.  This
cross-decimal-binade case proves that the fixed-width runtime path must stop
once grid coarsening cannot reduce the normalized digit count; otherwise it
chooses the farther one-digit point while the exact constant path chooses 7.
*/
[[nodiscard]] bool binary128_directed_binade_regression() noexcept
{
	auto const result{
		::fast_io::details::wide_shortest_from_binary<
			__float128, rounding::toward_minus_infinity>(
			static_cast<__uint128_t>(1u), 0u, false)};
	return result.success && result.m10 == 7u && result.e10 == -4966;
}
#endif

} // namespace

#endif

int main()
{
#if defined(__SIZEOF_INT128__)
	if (!check_binary64_open_midpoint_regression() ||
		!run_all_policies<float, narrow_checker>() ||
		!run_all_policies<double, narrow_checker>())
	{
		return 1;
	}
	if constexpr ((::std::numeric_limits<long double>::digits) == 64 &&
				  (::std::numeric_limits<long double>::max_exponent) == 16384)
	{
		if (!run_all_policies<long double, wide_checker>())
		{
			return 1;
		}
	}
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	if (!binary128_directed_binade_regression() ||
		!run_all_policies<__float128, wide_checker>())
	{
		return 1;
	}
#endif
#endif
	return 0;
}
