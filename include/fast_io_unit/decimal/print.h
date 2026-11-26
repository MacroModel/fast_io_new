#pragma once

#include "round.h"

namespace fast_io::details
{

struct decimal_notation
{
	::std::size_t fractional{};
	bool scientific{};
	bool cohort{};
};

inline constexpr ::std::size_t decimal_field_size(::std::size_t integer, ::std::size_t fractional) noexcept
{
	return fractional ? ::fast_io::details::intrinsics::add_or_overflow_die(integer + 1u, fractional) : integer;
}

inline constexpr unsigned decimal_exponent_digits(int exponent, bool cohort) noexcept
{
	auto magnitude{static_cast<unsigned>(exponent < 0 ? -exponent : exponent)};
	auto digits{::fast_io::details::iec559_decimal_digits(magnitude)};
	return !cohort && digits < 2u ? 2u : digits;
}

template <typename Decimal>
inline constexpr ::std::size_t decimal_spelling_size(Decimal const &value, decimal_notation notation) noexcept
{
	auto const exponent{value.exponent + static_cast<int>(value.size) - 1};
	if (notation.scientific)
	{
		auto size{decimal_field_size(1u, notation.fractional)};
		return ::fast_io::details::intrinsics::add_or_overflow_die(size, static_cast<::std::size_t>(2u + decimal_exponent_digits(exponent, notation.cohort)));
	}
	return decimal_field_size(exponent < 0 ? 1u : static_cast<::std::size_t>(exponent) + 1u, notation.fractional);
}

enum class decimal_print_mode : unsigned char
{
	shortest,
	precision,
	range
};

template <::fast_io::manipulators::scalar_flags flags, decimal_print_mode mode, typename Decimal>
inline constexpr decimal_notation decimal_notation_for(Decimal &value, ::std::size_t precision,
													   ::std::size_t maximum) noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	using policy = ::fast_io::manipulators::floating_precision;
	if constexpr (flags.floating == format::hexfloat)
	{
		// ISO C23 decimal a/A is a cohort-preserving decimal conversion.
		// Explicit precision limits significant digits. The output exponent
		// is unbounded by the interchange format after a carry (WG14 CFP.11).
		if constexpr (mode == decimal_print_mode::precision)
		{
			if (precision && precision < value.size)
			{
				decimal_round(value, static_cast<int>(precision), false, flags.rounding);
			}
		}
		auto fixed{value.exponent <= 0 && -(static_cast<int>(value.size) + 5) <= value.exponent};
		return {fixed ? static_cast<::std::size_t>(-value.exponent) : value.size - 1u, !fixed, true};
	}

	exact_precision_trim(value);
	bool fractional{mode == decimal_print_mode::precision && floating_precision_is_fractional<flags.precision>};
	bool preserve{mode == decimal_print_mode::precision && floating_precision_preserves_trailing_zero<flags.precision>};
	bool with_precision{mode == decimal_print_mode::precision};
	if constexpr (mode == decimal_print_mode::range)
	{
		auto plan{make_floating_precision_range_plan(value.size, precision, maximum)};
		with_precision = !plan.shortest;
		preserve = plan.preserve;
		precision = plan.precision;
	}
	auto significant{precision ? precision : 1u};
	if (with_precision)
	{
		auto const adjusted{value.exponent + static_cast<int>(value.size) - 1};
		// A precision larger than every coefficient plus every possible
		// exponent requires only padding. Keep signed arithmetic bounded.
		auto count{precision < 32000u ? static_cast<int>(precision) : 32000};
		int keep{fractional ? (flags.floating == format::scientific ? count + 1 : adjusted + 1 + count)
							: (count ? count : 1)};
		decimal_round(value, keep, false, flags.rounding);
		exact_precision_trim(value);
	}

	auto const adjusted{value.exponent + static_cast<int>(value.size) - 1};
	auto sci_fraction{value.size - 1u};
	auto fixed_fraction{value.exponent < 0 ? static_cast<::std::size_t>(-value.exponent) : 0u};
	if (preserve)
	{
		if (fractional)
		{
			if constexpr (flags.floating == format::scientific)
			{
				sci_fraction = precision;
			}
			else
			{
				fixed_fraction = precision;
				// General fractional precision uses the fixed decimal grid even
				// when its rounded result is eventually printed in e notation.
				if (adjusted < 0)
				{
					auto shift{static_cast<::std::size_t>(-adjusted)};
					sci_fraction = shift < precision ? precision - shift : 0u;
				}
				else
				{
					sci_fraction = exact_precision_saturating_add(precision, static_cast<::std::size_t>(adjusted));
				}
			}
		}
		else
		{
			sci_fraction = significant - 1u;
			if (adjusted < 0)
			{
				fixed_fraction = exact_precision_saturating_add(sci_fraction, static_cast<::std::size_t>(-adjusted));
			}
			else
			{
				fixed_fraction = static_cast<::std::size_t>(adjusted) < sci_fraction ? sci_fraction - static_cast<::std::size_t>(adjusted) : 0u;
			}
		}
	}
	if constexpr (flags.json_float)
	{
		if (!fixed_fraction)
		{
			fixed_fraction = 1u;
		}
	}
	decimal_notation fixed{fixed_fraction, false, false};
	decimal_notation scientific{sci_fraction, true, false};
	if constexpr (flags.floating == format::fixed)
	{
		return fixed;
	}
	else if constexpr (flags.floating == format::scientific)
	{
		return scientific;
	}
	else if constexpr (flags.floating == format::general)
	{
		bool fixed_form{-4 <= adjusted && adjusted < 6};
		if constexpr (flags.precision == policy::charconv_significant)
		{
			fixed_form = -4 <= adjusted && (adjusted < 0 || static_cast<::std::size_t>(adjusted) < significant);
		}
		return fixed_form ? fixed : scientific;
	}
	else
	{
		if (with_precision && fractional)
		{
			return fixed;
		}
		return decimal_spelling_size(value, fixed) <= decimal_spelling_size(value, scientific) ? fixed : scientific;
	}
}

template <::fast_io::manipulators::scalar_flags flags, ::std::integral Char, typename Decimal>
inline constexpr Char *decimal_write(Char *out, Decimal const &value, decimal_notation layout) noexcept
{
	if (value.category == decimal_class::infinity)
	{
		return prsv_fp_inf_literal_impl<flags.uppercase>(print_rsv_fp_sign_impl<flags.showpos>(out, value.negative));
	}
	if (value.category != decimal_class::finite)
	{
		if constexpr (flags.nan_show_sign)
		{
			out = print_rsv_fp_sign_impl<flags.showpos>(out, value.negative);
		}
		out = prsv_fp_nan_literal_impl<flags.uppercase>(out);
		if constexpr (flags.nan_show_type)
		{
			if (value.category == decimal_class::signaling_nan)
			{
				out = prsv_fp_nan_snan_literal_impl<flags.uppercase>(out);
			}
		}
		return out;
	}
	out = print_rsv_fp_sign_impl<flags.showpos>(out, value.negative);
	if (!layout.scientific)
	{
		return exact_precision_fixed<flags.comma, false>(out, value, value.size, layout.fractional != 0u, layout.fractional);
	}
	*out++ = ::fast_io::char_literal_add<Char>(value.digits[0]);
	if (layout.fractional)
	{
		*out++ = ::fast_io::char_literal_v < flags.comma ? u8',' : u8'.', Char > ;
		out = exact_precision_copy(out, value, 1u, value.size);
		out = fill_zeros_impl(out, layout.fractional - (value.size - 1u));
	}
	auto const exponent{value.exponent + static_cast<int>(value.size) - 1};
	*out++ = ::fast_io::char_literal_v < flags.uppercase_e ? u8'E' : u8'e', Char > ;
	*out++ = exponent < 0 ? ::fast_io::char_literal_v<u8'-', Char> : ::fast_io::char_literal_v<u8'+', Char>;
	auto magnitude{static_cast<unsigned>(exponent < 0 ? -exponent : exponent)};
	if (layout.cohort)
	{
		return prt_rsv_exponent_impl<4u, false>(out, magnitude);
	}
	return prt_rsv_exponent_impl<4u, true>(out, magnitude);
}

template <typename M>
struct decimal_print_traits;

template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_print_traits<::fast_io::manipulators::scalar_manip_t<options, T>>
{
	inline static constexpr auto flags{options};
	inline static constexpr auto mode{decimal_print_mode::shortest};
};
template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_print_traits<::fast_io::manipulators::exact_decimal_manip_t<options, T>>
	: decimal_print_traits<::fast_io::manipulators::scalar_manip_t<options, T>>
{};
template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_print_traits<::fast_io::manipulators::scalar_manip_precision_t<options, T>>
{
	inline static constexpr auto flags{options};
	inline static constexpr auto mode{decimal_print_mode::precision};
};
template <::fast_io::manipulators::scalar_flags options, decimal_interchange_value T>
struct decimal_print_traits<::fast_io::manipulators::floating_scalar_precision_range_manip_t<options, T>>
{
	inline static constexpr auto flags{options};
	inline static constexpr auto mode{decimal_print_mode::range};
};

template <typename M>
concept decimal_printable = requires { decimal_print_traits<M>::mode; } && !::std::is_reference_v<decltype(::std::declval<M>().reference)>;

template <unsigned width>
struct decimal_print_plan
{
	decimal_components<width> value;
	decimal_notation layout;
};

template <decimal_printable M>
inline constexpr auto decimal_plan(M object) noexcept
{
	using policy = decimal_print_traits<M>;
	auto value{decimal_decode(object.reference)};
	::std::size_t precision{}, maximum{};
	if constexpr (policy::mode == decimal_print_mode::precision)
	{
		precision = object.precision;
	}
	else if constexpr (policy::mode == decimal_print_mode::range)
	{
		precision = object.minimum_precision;
		maximum = object.maximum_precision;
	}
	decimal_notation layout{};
	if (value.category == decimal_class::finite)
	{
		layout = decimal_notation_for<policy::flags, policy::mode>(value, precision, maximum);
	}
	return decimal_print_plan<decimal_interchange_binding<decltype(object.reference)>::bits>{value, layout};
}
} // namespace fast_io::details

namespace fast_io
{

template <::std::integral Char, details::decimal_printable M>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<Char, M>, M object) noexcept
{
	constexpr auto flags{details::decimal_print_traits<M>::flags};
	auto const plan{details::decimal_plan(object)};
	auto sign{static_cast<::std::size_t>(flags.showpos || plan.value.negative)};
	if (plan.value.category == details::decimal_class::infinity)
	{
		return 3u + sign;
	}
	if (plan.value.category != details::decimal_class::finite)
	{
		return 3u + (flags.nan_show_sign ? sign : 0u) +
			   ((flags.nan_show_type && plan.value.category == details::decimal_class::signaling_nan) ? 6u : 0u);
	}
	return details::intrinsics::add_or_overflow_die(sign, details::decimal_spelling_size(plan.value, plan.layout));
}

template <::std::integral Char, details::decimal_printable M>
inline constexpr Char *print_reserve_define(io_reserve_type_t<Char, M>, Char *out, M object) noexcept
{
	auto const plan{details::decimal_plan(object)};
	return details::decimal_write<details::decimal_print_traits<M>::flags>(out, plan.value, plan.layout);
}

template <::std::integral Char, details::decimal_printable M>
inline constexpr ::std::size_t print_reserve_precise_size(io_reserve_type_t<Char, M> tag, M object) noexcept
{
	return print_reserve_size(tag, object);
}

template <::std::integral Char, details::decimal_printable M>
inline constexpr Char *print_reserve_precise_define(io_reserve_type_t<Char, M> tag, Char *out, ::std::size_t, M object) noexcept
{
	return print_reserve_define(tag, out, object);
}

} // namespace fast_io
