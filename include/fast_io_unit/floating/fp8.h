#pragma once

#include "interchange_cast.h"

namespace fast_io
{
namespace details
{

template <fp8_format F>
struct interchange_floating_traits<basic_float8<F>>
{
	static constexpr int radix{2};
	static constexpr bool negative(basic_float8<F> v) noexcept
	{
		return (v.bits & 128u) != 0;
	}
	static constexpr bool finite(basic_float8<F> v) noexcept
	{
		return (v.bits & 127u) <= fp8_last_finite<F>;
	}
	/// @brief Determines the decimal order for format selection; zero and specials return zero.
	static constexpr int adjusted_exponent(basic_float8<F> v) noexcept
	{
		auto code{static_cast<unsigned>(v.bits) & 127u};
		if (!code || code > fp8_last_finite<F>)
		{
			return 0;
		}
		// Each positive threshold is the first encoding with value >= 10^k.
		// Order of finite magnitude codes is numerical order, including subnormals.
		constexpr auto thresholds = [] {
			::fast_io::freestanding::array<unsigned, 9> first{};
			::std::uint_least64_t scale{10000u};
			::std::uint_least64_t power{1u};
			unsigned i{1};
			for (auto &threshold : first)
			{
				while (i <= fp8_last_finite<F> &&
					   static_cast<::std::uint_least64_t>(fp8_magnitude<F>(i) * 131072.0f) * scale < power * 131072u)
				{
					++i;
				}
				threshold = i;
				if (scale != 1u)
				{
					scale /= 10u;
				}
				else
				{
					power *= 10u;
				}
			}
			return first;
		}();
		int exponent{-5};
		for (auto threshold : thresholds)
		{
			exponent += static_cast<int>(code >= threshold);
		}
		return exponent;
	}
};

/// @brief Computes the exact round-trip interval in units of 2^-17.
/// @details This unit also represents the smallest halfway point of E5M2. Scaling a
/// finite code or its virtual successor is exact even at the largest exponent.
template <fp8_format F>
inline constexpr ::std::uint_least64_t fp8_units(unsigned index) noexcept
{
	return static_cast<::std::uint_least64_t>(fp8_magnitude<F>(index) * 131072.0f);
}

struct fp8_short_decimal
{
	unsigned coefficient;
	int exponent;
};

/// @brief Finds the closest shortest decimal in the target rounding cell.
/// @details Try each decimal grid from coarse to fine. The nearest grid point on each
/// side suffices because each rounding cell is an interval. Ties between spellings
/// prefer an even decimal coefficient. Three significant digits suffice for OFP8.
/// All comparisons use integer units and a common power-of-ten denominator.
template <fp8_format F, manipulators::floating_rounding R>
inline constexpr fp8_short_decimal fp8_shortest(unsigned index, bool negative) noexcept
{
	auto x{fp8_units<F>(index)}, prev{fp8_units<F>(index - 1)}, next{fp8_units<F>(index + 1)};
	::std::uint_least64_t low{}, high{};
	bool include_low{}, include_high{};
	if constexpr (scan_decfloat_nearest_rounding<R>)
	{
		low = (prev + x) / 2;
		high = (x + next) / 2;
		include_low = scan_decfloat_nearest_tie_round_up<R>(negative, (index - 1u) * 2u);
		include_high = !scan_decfloat_nearest_tie_round_up<R>(negative, index * 2u);
	}
	else if (scan_decfloat_directed_round_up<R>(negative))
	{
		low = prev;
		high = x;
		include_high = true;
	}
	else
	{
		low = x;
		high = next;
		include_low = true;
	}
	// Units times 10^7 fit in 64 bits, including the 65536 overflow neighbor.
	::std::uint_least64_t grid{UINT64_C(13107200000)};
	for (int exponent{5};; --exponent)
	{
		auto floor{x / grid};
		auto rem{x % grid};
		bool up{rem > grid - rem || (rem == grid - rem && (floor & 1u))};
		for (unsigned attempt{}; attempt != 2; ++attempt)
		{
			auto coefficient{floor + static_cast<unsigned>(up)};
			auto candidate{coefficient * grid};
			if (coefficient && (candidate > low || (include_low && candidate == low)) &&
				(candidate < high || (include_high && candidate == high)))
			{
				return {static_cast<unsigned>(coefficient), exponent};
			}
			up = !up;
		}
		if (exponent > 0)
		{
			grid /= 10u;
		}
		else
		{
			x *= 10u;
			low *= 10u;
			high *= 10u;
		}
	}
}

/// @brief Caches the decimal coefficient and exponent for each finite nonzero magnitude.
/// @details Signed rounding directions select a separate table only when the sign changes the rounding cell.
template <fp8_format F, manipulators::floating_rounding R, bool Negative>
inline constexpr auto fp8_shortest_table = [] {
	::fast_io::freestanding::array<fp8_short_decimal, fp8_last_finite<F> + 1> table{};
	for (unsigned i{1}; i < table.size(); ++i)
	{
		table[i] = fp8_shortest<F, R>(i, Negative);
	}
	return table;
}();

/// @brief Selects the decimal rounding cell for a finite nonzero OFP8 encoding.
template <manipulators::floating_rounding R, fp8_format F>
inline constexpr fp8_short_decimal fp8_shortest_for(basic_float8<F> value) noexcept
{
	using mode = manipulators::floating_rounding;
	if constexpr (R == mode::current_environment)
	{
		switch (current_floating_rounding())
		{
		case mode::toward_plus_infinity:
			return fp8_shortest_for<mode::toward_plus_infinity>(value);
		case mode::toward_minus_infinity:
			return fp8_shortest_for<mode::toward_minus_infinity>(value);
		case mode::toward_zero:
			return fp8_shortest_for<mode::toward_zero>(value);
		default:
			return fp8_shortest_for<mode::nearest_to_even>(value);
		}
	}
	else
	{
		unsigned const magnitude{static_cast<unsigned>(value.bits) & 127u};
		if constexpr (R == mode::toward_plus_infinity || R == mode::toward_minus_infinity ||
					  R == mode::nearest_toward_plus_infinity || R == mode::nearest_toward_minus_infinity)
		{
			if (value.bits & 128u)
			{
				return fp8_shortest_table<F, R, true>[magnitude];
			}
		}
		return fp8_shortest_table<F, R, false>[magnitude];
	}
}

/// @brief Formats an OFP8 infinity or NaN from its binary32 fields.
/// @details The caller has classified the magnitude as nonfinite. Keep the
///          expanded payload in integer storage because moving a signaling NaN
///          through a binary32 return register can quiet it and raise FE_INVALID.
template <manipulators::scalar_flags Flags, ::std::integral C, fp8_format F>
inline constexpr C *fp8_print_nonfinite(C *out, basic_float8<F> value) noexcept
{
	auto const fields{floating_scalar_proxy_fields<float>(fp8_expand_bits(value))};
	return prsv_fp_nan_impl<Flags.showpos, Flags.uppercase, Flags.nan_show_sign,
		Flags.nan_show_type, 23u>(out, fields.mantissa, fields.sign);
}

/// @brief Renders a shortest decimal or an exact hexadecimal OFP8 value.
template <manipulators::scalar_flags Flags, ::std::integral C, fp8_format F>
inline constexpr C *fp8_print(C *out, basic_float8<F> value) noexcept
{
	using bridge = manipulators::scalar_manip_t<Flags, float>;
	unsigned const code{static_cast<unsigned>(value.bits) & 127u};
	if (code > fp8_last_finite<F>)
	{
		return fp8_print_nonfinite<Flags>(out, value);
	}
	if constexpr (Flags.floating == manipulators::floating_format::hexfloat)
	{
		return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{fp8_expand(value)});
	}
	else
	{
		if (!code)
		{
			return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{fp8_expand(value)});
		}
		bool negative{(value.bits & 128u) != 0};
		if (negative)
		{
			*out++ = ::fast_io::char_literal_v<u8'-', C>;
		}
		else if constexpr (Flags.showpos)
		{
			*out++ = ::fast_io::char_literal_v<u8'+', C>;
		}
		auto dec{fp8_shortest_for<Flags.rounding>(value)};
		if constexpr (Flags.floating == manipulators::floating_format::fixed)
		{
			return print_rsv_fp_fixed_decision_impl<float, Flags.comma, Flags.json_float>(out, dec.coefficient, dec.exponent);
		}
		return print_rsv_fp_decision_impl<float, Flags.comma, Flags.uppercase_e, Flags.floating, Flags.json_float>(
			out, dec.coefficient, dec.exponent);
	}
}

} // namespace details

/// @brief Uses the ordinary scalar manipulator as the public output representation.
template <fp8_format F>
inline constexpr auto print_alias_define(io_alias_t, basic_float8<F> value) noexcept
{
	return manipulators::scalar_manip_t<manipulators::floating_point_default_scalar_flags, basic_float8<F>>{value};
}

/// @brief Captures the destination by reference for the streaming scanner.
template <fp8_format F>
inline constexpr auto scan_alias_define(io_alias_t, basic_float8<F> &value) noexcept
{
	return manipulators::scalar_manip_t<manipulators::floating_point_default_scalar_flags, basic_float8<F> &>{value};
}

/// @brief Reserves space for shortest OFP8 or a binary32 special-value spelling.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<C, manipulators::scalar_manip_t<Flags, basic_float8<F>>>) noexcept
{
	return print_reserve_size(io_reserve_type<C, manipulators::scalar_manip_t<Flags, float>>);
}

/// @brief Writes the signed OFP8 value under its presentation and rounding flags.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr C *print_reserve_define(io_reserve_type_t<C, manipulators::scalar_manip_t<Flags, basic_float8<F>>>, C *out,
										 manipulators::scalar_manip_t<Flags, basic_float8<F>> value) noexcept
{
	return details::fp8_print<Flags>(out, value.reference);
}

/// @brief Computes the binary32 reserve bound from the requested precision.
/// @details The bound is independent of the floating value, including NaNs.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<C, manipulators::scalar_manip_precision_t<Flags, basic_float8<F>>>,
												  manipulators::scalar_manip_precision_t<Flags, basic_float8<F>> value) noexcept
{
	using bridge = manipulators::scalar_manip_precision_t<Flags, float>;
	return print_reserve_size(io_reserve_type<C, bridge>, bridge{0.0f, value.precision});
}

/// @brief Rounds the exact dyadic value on the requested significant, fractional or hex grid.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr C *print_reserve_define(io_reserve_type_t<C, manipulators::scalar_manip_precision_t<Flags, basic_float8<F>>>, C *out,
										 manipulators::scalar_manip_precision_t<Flags, basic_float8<F>> value) noexcept
{
	using bridge = manipulators::scalar_manip_precision_t<Flags, float>;
	if ((value.reference.bits & 127u) > details::fp8_last_finite<F>)
	{
		return details::fp8_print_nonfinite<Flags>(out, value.reference);
	}
	return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{details::fp8_expand(value.reference), value.precision});
}

/// @brief Bounds a precision interval using the exact binary32 expansion as the largest presentation.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<C, manipulators::floating_scalar_precision_range_manip_t<Flags, basic_float8<F>>>,
												  manipulators::floating_scalar_precision_range_manip_t<Flags, basic_float8<F>> value) noexcept
{
	return details::print_floating_precision_range_reserve_size<Flags, float>(value.minimum_precision, value.maximum_precision);
}

/// @brief Preserves an OFP8 shortest spelling inside the interval and rounds the exact value outside it.
/// @details The interval is compared with the length of the target-format round-trip coefficient.
/// A binary32 spelling of that same value can require more digits.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr C *print_reserve_define(io_reserve_type_t<C, manipulators::floating_scalar_precision_range_manip_t<Flags, basic_float8<F>>>,
										 C *out, manipulators::floating_scalar_precision_range_manip_t<Flags, basic_float8<F>> value) noexcept
{
	using precision = manipulators::floating_precision;
	unsigned const code{static_cast<unsigned>(value.reference.bits) & 127u};
	auto digits{code && code <= details::fp8_last_finite<F>
					? details::chars_len<10u, true>(details::fp8_shortest_for<Flags.rounding>(value.reference).coefficient)
					: 1u};
	auto const plan{details::make_floating_precision_range_plan(digits, value.minimum_precision, value.maximum_precision)};
	if (plan.shortest || code > details::fp8_last_finite<F>)
	{
		return details::fp8_print<Flags>(out, value.reference);
	}
	if (plan.preserve)
	{
		using bridge = manipulators::scalar_manip_precision_t<details::floating_precision_mani_flags_cache<Flags, precision::significant_preserve_trailing_zero>, float>;
		return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{details::fp8_expand(value.reference), plan.precision});
	}
	using bridge = manipulators::scalar_manip_precision_t<details::floating_precision_mani_flags_cache<Flags, precision::significant>, float>;
	return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{details::fp8_expand(value.reference), plan.precision});
}

/// @brief Supplies the exact-decimal formatter with a losslessly widened source.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<C, manipulators::exact_decimal_manip_t<Flags, basic_float8<F>>>) noexcept
{
	return print_reserve_size(io_reserve_type<C, manipulators::exact_decimal_manip_t<Flags, float>>);
}

/// @brief Emits the full decimal expansion or a nonfinite spelling.
template <::std::integral C, manipulators::scalar_flags Flags, fp8_format F>
inline constexpr C *print_reserve_define(io_reserve_type_t<C, manipulators::exact_decimal_manip_t<Flags, basic_float8<F>>>, C *out,
										 manipulators::exact_decimal_manip_t<Flags, basic_float8<F>> value) noexcept
{
	using bridge = manipulators::exact_decimal_manip_t<Flags, float>;
	if ((value.reference.bits & 127u) > details::fp8_last_finite<F>)
	{
		return details::fp8_print_nonfinite<Flags>(out, value.reference);
	}
	return print_reserve_define(io_reserve_type<C, bridge>, out, bridge{details::fp8_expand(value.reference)});
}

namespace details
{

/// @brief Narrows an existing scanner result, resolving decimal or hexadecimal midpoint ambiguity from its lexical state.
/// @details FP8 values and the midpoints between adjacent magnitudes are exactly representable in binary32.
/// A directed rounding composes in the same direction on nested grids. For nearest rounding, a binary32
/// midpoint alone is ambiguous. Re-evaluate the saved digits toward and away from zero to distinguish the
/// lower open side, exact equality and upper open side. Pass the lexer result by reference so an x87
/// parameter copy cannot quiet a signaling NaN before its representation is narrowed.
template <manipulators::floating_rounding R, fp8_format F, typename Compare>
inline constexpr parse_code fp8_assign(basic_float8<F> &dest, float const &expanded, bool special, Compare compare) noexcept
{
	using mode = manipulators::floating_rounding;
	if constexpr (R == mode::current_environment)
	{
		switch (current_floating_rounding())
		{
		case mode::toward_plus_infinity:
			return fp8_assign<mode::toward_plus_infinity>(dest, expanded, special, compare);
		case mode::toward_minus_infinity:
			return fp8_assign<mode::toward_minus_infinity>(dest, expanded, special, compare);
		case mode::toward_zero:
			return fp8_assign<mode::toward_zero>(dest, expanded, special, compare);
		default:
			return fp8_assign<mode::nearest_to_even>(dest, expanded, special, compare);
		}
	}
	else
	{
		auto const bits{fp_translate_nan_encoding<23u, 8u>(::std::bit_cast<::std::uint_least32_t>(expanded))};
		unsigned const sign{(bits >> 24u) & 128u};
		auto absbits{bits & 0x7fffffffu};
		unsigned code{};
		if (special)
		{
			if (absbits == 0x7f800000u)
			{
				if constexpr (F == fp8_format::e4m3fn)
				{
					return parse_code::overflow;
				}
				code = 124u;
			}
			else if constexpr (F == fp8_format::e4m3fn)
			{
				code = 127u;
			}
			else
			{
				code = 124u | ((absbits >> 21u) & 3u);
			}
			if (absbits > 0x7f800000u && code == 124u)
			{
				code = 125u;
			}
		}
		else
		{
			constexpr unsigned m{fp8_fraction_bits<F>}, bias{(1u << (6u - m)) - 1u}, last{fp8_last_finite<F>};
			auto const x{::std::bit_cast<float>(absbits)};
			constexpr unsigned normal_start{(128u - bias) << 23u};
			if (absbits >= normal_start)
			{
				code = (absbits - ((127u - bias) << 23u)) >> (23u - m);
				if (code > last)
				{
					code = last;
				}
			}
			else
			{
				code = static_cast<unsigned>(x * static_cast<float>(1u << (bias + m - 1u)));
			}
			if (x != fp8_magnitude<F>(code))
			{
				if constexpr (scan_decfloat_nearest_rounding<R>)
				{
					auto const middle{(fp8_magnitude<F>(code) + fp8_magnitude<F>(code + 1u)) * 0.5f};
					if (x == middle)
					{
						int side{compare(middle)};
						if (side > 0 || (!side && scan_decfloat_nearest_tie_round_up<R>(sign != 0, code * 2u)))
						{
							++code;
						}
					}
					else if (x > middle)
					{
						++code;
					}
				}
				else if (scan_decfloat_directed_round_up<R>(sign != 0))
				{
					++code;
				}
				if (!code || code > last)
				{
					return parse_code::overflow;
				}
			}
		}
		dest.bits = static_cast<::std::uint_least8_t>(sign | code);
		return parse_code::ok;
	}
}

/// @brief Commits a complete field from a shared binary32 lexer with access to its original digits.
template <manipulators::scalar_flags Flags, bool WithPrecision = false, fp8_format F, typename State>
inline constexpr parse_code fp8_finish(State const &state, float const &value, parse_code result, basic_float8<F> &dest, ::std::size_t precision = 0u) noexcept
{
	if (result != parse_code::ok)
	{
		return result;
	}
	constexpr bool hex{Flags.floating == manipulators::floating_format::hexfloat};
	auto compare = [&](float middle) noexcept {
		constexpr auto down{set_floating_rounding_flag(Flags, manipulators::floating_rounding::toward_zero)};
		constexpr auto up{set_floating_rounding_flag(Flags, manipulators::floating_rounding::away_from_zero)};
		float lower{}, upper{};
		// Reaching this callback proves the original token is finite and within one
		// binary32 rounding step of a nonzero FP8 midpoint. Both assignments succeed.
		if constexpr (hex)
		{
			static_cast<void>(scan_hexfloat_context_assign<float, down>(state, lower));
			static_cast<void>(scan_hexfloat_context_assign<float, up>(state, upper));
		}
		else if constexpr (WithPrecision)
		{
			auto const exponent{state.exponent_has_digit ? scan_decfloat_context_exponent(state) : 0};
			static_cast<void>(scan_decfloat_assign_precision<float, Flags.precision, Flags.rounding, down.rounding>(
				lower, state.negative, state.significand_state, exponent, precision));
			static_cast<void>(scan_decfloat_assign_precision<float, Flags.precision, Flags.rounding, up.rounding>(
				upper, state.negative, state.significand_state, exponent, precision));
		}
		else
		{
			static_cast<void>(scan_decfloat_context_assign<float, down, false>(state, lower));
			static_cast<void>(scan_decfloat_context_assign<float, up, false>(state, upper));
		}
		auto magnitude = [](float f) { return ::std::bit_cast<::std::uint_least32_t>(f) & 0x7fffffffu; };
		return magnitude(lower) < magnitude(middle) ? -1 : static_cast<int>(magnitude(upper) > magnitude(middle));
	};
	return fp8_assign<Flags.rounding>(dest, value, state.phase == decltype(state.phase)::special, compare);
}

} // namespace details

namespace details
{

template <typename M>
struct fp8_scan_traits;

template <manipulators::scalar_flags Flags, fp8_format F>
struct fp8_scan_traits<manipulators::scalar_manip_t<Flags, basic_float8<F> &>>
{
	static constexpr auto flags{Flags};
	static constexpr bool precision{};
	using bridge = manipulators::scalar_manip_t<Flags, float &>;
};

template <manipulators::scalar_flags Flags, fp8_format F>
	requires(Flags.floating != manipulators::floating_format::hexfloat)
struct fp8_scan_traits<manipulators::scalar_manip_precision_t<Flags, basic_float8<F> &>>
{
	static constexpr auto flags{Flags};
	static constexpr bool precision{true};
	using bridge = manipulators::scalar_manip_precision_t<Flags, float &>;
};

template <typename M>
concept fp8_scanner = requires { typename fp8_scan_traits<M>::bridge; };

/// @brief Rebinds the lexical destination while retaining the original decimal precision request.
template <fp8_scanner M>
inline constexpr auto fp8_scan_bridge(M dest, float &value) noexcept
{
	using traits = fp8_scan_traits<M>;
	if constexpr (traits::precision)
	{
		return typename traits::bridge{value, dest.precision};
	}
	else
	{
		return typename traits::bridge{value};
	}
}

/// @brief Applies the declared FP8 rounding rule to the completed token.
template <fp8_scanner M, typename State>
inline constexpr parse_code fp8_commit(State const &state, M dest, float const &value, parse_code code) noexcept
{
	using traits = fp8_scan_traits<M>;
	if constexpr (traits::precision)
	{
		return fp8_finish<traits::flags, true>(state, value, code, dest.reference, dest.precision);
	}
	else
	{
		return fp8_finish<traits::flags>(state, value, code, dest.reference);
	}
}
} // namespace details

/// @brief Uses the binary32 numeric lexer, including its storage for fragmented special tokens.
template <::std::integral C, details::fp8_scanner M>
inline constexpr auto scan_context_type(io_reserve_type_t<C, M>) noexcept
{
	return scan_context_type(io_reserve_type<C, typename details::fp8_scan_traits<M>::bridge>);
}

/// @brief Advances the shared lexical state and publishes the destination when the field completes.
template <::std::integral C, details::fp8_scanner M>
inline constexpr parse_result<C const *> scan_context_define(io_reserve_type_t<C, M>,
															 typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, C const *begin, C const *end, M dest) noexcept
{
	float value{};
	auto bridge{details::fp8_scan_bridge(dest, value)};
	auto result{scan_context_define(io_reserve_type<C, decltype(bridge)>, state, begin, end, bridge)};
	result.code = details::fp8_commit(state, dest, value, result.code);
	return result;
}

/// @brief Finalizes a token whose last code unit coincides with end of input.
template <::std::integral C, details::fp8_scanner M>
inline constexpr parse_code scan_context_eof_define(io_reserve_type_t<C, M>,
													typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, M dest) noexcept
{
	float value{};
	auto bridge{details::fp8_scan_bridge(dest, value)};
	auto code{scan_context_eof_define(io_reserve_type<C, decltype(bridge)>, state, bridge)};
	return details::fp8_commit(state, dest, value, code);
}

/// @brief Forwards the underlying lexer's rewind convention for tentative special-token suffixes.
template <::std::integral C, details::fp8_scanner M>
	requires requires(typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state,
					  typename details::fp8_scan_traits<M>::bridge bridge) {
		scan_context_eof_rewind_size(io_reserve_type<C, decltype(bridge)>, state, bridge);
	}
inline constexpr ::std::size_t scan_context_eof_rewind_size(io_reserve_type_t<C, M>,
															typename decltype(scan_context_type(io_reserve_type<C, M>))::type &state, M dest) noexcept
{
	float value{};
	auto bridge{details::fp8_scan_bridge(dest, value)};
	return scan_context_eof_rewind_size(io_reserve_type<C, decltype(bridge)>, state, bridge);
}
} // namespace fast_io
