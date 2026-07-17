#include <bit>
#include <cstdint>
#include <string>

#include <fast_io_freestanding.h>
#include <fast_io_unit/string.h>

int main()
{
	using namespace ::fast_io::mnp;
	// The tiny-zero shortcut must obey the same zero canonicalization as the
	// ordinary fractional-rounding path.  Non-preserving general output removes
	// the obsolete 10^-P quantum; preserving output retains it and applies the
	// established fixed/scientific threshold at P=5.
	constexpr float tiny_value{1.0e-30f};
	constexpr auto nearest{floating_rounding::nearest_to_even};
	constexpr auto fractional_mode{floating_precision::fractional};
	constexpr auto preserving_mode{
		floating_precision::fractional_preserve_trailing_zero};
	auto const tiny_general{::fast_io::concat_std(
		general<fractional_mode, nearest>(tiny_value, 6u))};
	auto const tiny_negative{::fast_io::concat_std(
		general<fractional_mode, nearest>(-tiny_value, 6u))};
	auto const tiny_json{::fast_io::concat_std(json_float(
		general<fractional_mode, nearest>(tiny_value, 6u)))};
	auto const tiny_comma_json{::fast_io::concat_std(json_float(
		comma_general<fractional_mode, nearest>(tiny_value, 6u)))};
	auto const tiny_preserved_fixed{::fast_io::concat_std(
		general<preserving_mode, nearest>(tiny_value, 4u))};
	auto const tiny_preserved_scientific{::fast_io::concat_std(
		general<preserving_mode, nearest>(tiny_value, 5u))};
	auto const tiny_preserved_comma{::fast_io::concat_std(
		comma_general<preserving_mode, nearest>(tiny_value, 4u))};
	auto const tiny_preserved_zero{::fast_io::concat_std(
		general<preserving_mode, nearest>(tiny_value, 0u))};
	auto const tiny_preserved_zero_json{::fast_io::concat_std(json_float(
		general<preserving_mode, nearest>(tiny_value, 0u)))};
	if (tiny_general != "0" || tiny_negative != "-0" ||
		tiny_json != "0.0" || tiny_comma_json != "0,0" ||
		tiny_preserved_fixed != "0.0000" ||
		tiny_preserved_scientific != "0e-05" ||
		tiny_preserved_comma != "0,0000" ||
		tiny_preserved_zero != "0" || tiny_preserved_zero_json != "0.0")
	{
		return 1;
	}

	// The ordinary carrier path can also round a non-tiny value to zero.  General
	// preserving mode must retain the requested quantum after that rounding step,
	// while the non-preserving mode canonicalizes the same coefficient.  The P5
	// case exercises the fixed/scientific threshold without entering the tiny-
	// magnitude shortcut, and 1.25 verifies nonzero fixed-field padding.
	constexpr float ordinary_zero_value{-51.0f / 512.0f};
	constexpr auto upward{floating_rounding::toward_plus_infinity};
	auto const ordinary_preserved{::fast_io::concat_std(
		general<preserving_mode, upward>(ordinary_zero_value, 1u))};
	auto const ordinary_nonpreserved{::fast_io::concat_std(
		general<fractional_mode, upward>(ordinary_zero_value, 1u))};
	auto const ordinary_preserved_comma{::fast_io::concat_std(
		comma_general<preserving_mode, upward>(ordinary_zero_value, 1u))};
	auto const ordinary_preserved_json{::fast_io::concat_std(json_float(
		general<preserving_mode, upward>(ordinary_zero_value, 1u)))};
	auto const ordinary_preserved_scientific{::fast_io::concat_std(
		general<preserving_mode, upward>(-9.0e-6f, 5u))};
	// This exact binary32 value reaches the compact exact-window rounder rather
	// than the tiny-value shortcut.  Directed P5 rounding produces coefficient
	// zero with an intermediate exponent of -5; non-preserving general output
	// must canonicalize that pair before choosing fixed versus scientific form.
	constexpr auto exact_window_zero_value{::std::bit_cast<float>(
		::std::uint32_t{0xb7271de7u})};
	auto const exact_window_nonpreserved{::fast_io::concat_std(
		general<fractional_mode, upward>(exact_window_zero_value, 5u))};
	auto const exact_window_nonpreserved_comma{::fast_io::concat_std(
		comma_general<fractional_mode, upward>(exact_window_zero_value, 5u))};
	auto const exact_window_nonpreserved_json{::fast_io::concat_std(json_float(
		general<fractional_mode, upward>(exact_window_zero_value, 5u)))};
	auto const ordinary_padded{::fast_io::concat_std(
		general<preserving_mode, nearest>(1.25f, 3u))};
	auto const wide_padded{::fast_io::concat_std(
		general<preserving_mode, nearest>(1.25, 20u))};
	if (ordinary_preserved != "-0.0" || ordinary_nonpreserved != "-0" ||
		ordinary_preserved_comma != "-0,0" || ordinary_preserved_json != "-0.0" ||
		ordinary_preserved_scientific != "-0e-05" ||
		exact_window_nonpreserved != "-0" || exact_window_nonpreserved_comma != "-0" ||
		exact_window_nonpreserved_json != "-0.0" || ordinary_padded != "1.250" ||
		wide_padded != "1.25000000000000000000")
	{
		return 1;
	}

	// Significant P2 gives a value in [1,10) one fractional place.  Rounding
	// 9.99 across the decade therefore changes F from one to zero: the ordinary
	// spelling must remove both the obsolete terminal zero and radix point,
	// while JSON restores its mandatory floating marker.  A carry from below one
	// changes F from two to one and consequently retains exactly one fractional
	// zero.  Directed cases prove that this is a presentation invariant after the
	// rounding decision, not a nearest-even special case.
	constexpr auto significant_preserving_mode{
		floating_precision::significant_preserve_trailing_zero};
	constexpr auto downward{floating_rounding::toward_minus_infinity};
	auto const decade_carry{::fast_io::concat_std(
		fixed<significant_preserving_mode, nearest>(9.99, 2u))};
	auto const decade_carry_decimal{::fast_io::concat_std(
		decimal<significant_preserving_mode, nearest>(9.99, 2u))};
	auto const decade_carry_comma{::fast_io::concat_std(
		comma_fixed<significant_preserving_mode, nearest>(9.99, 2u))};
	auto const decade_carry_json{::fast_io::concat_std(json_float(
		fixed<significant_preserving_mode, nearest>(9.99, 2u)))};
	auto const decade_carry_comma_json{::fast_io::concat_std(json_float(
		comma_fixed<significant_preserving_mode, nearest>(9.99, 2u)))};
	auto const below_one_carry{::fast_io::concat_std(
		fixed<significant_preserving_mode, nearest>(0.999, 2u))};
	auto const directed_positive{::fast_io::concat_std(
		fixed<significant_preserving_mode, upward>(9.91, 2u))};
	auto const directed_negative{::fast_io::concat_std(
		fixed<significant_preserving_mode, downward>(-9.91, 2u))};
	auto const wide_decade_carry{::fast_io::wconcat_std(
		fixed<significant_preserving_mode, nearest>(9.99, 2u))};
	if (decade_carry != "10" || decade_carry_decimal != "10" ||
		decade_carry_comma != "10" || decade_carry_json != "10.0" ||
		decade_carry_comma_json != "10,0" || below_one_carry != "1.0" ||
		directed_positive != "10" || directed_negative != "-10" ||
		wide_decade_carry != L"10")
	{
		return 1;
	}

	// Exercise the public binary16 promotion path only when the frontend exposes
	// that source type.  Targets without binary16 cannot form the triggering
	// value and return successfully; binary32 has a different shortest carrier
	// and therefore is not a sound substitute for this regression.
#if defined(__SIZEOF_FLOAT16__) || defined(__FLOAT16__) || \
	defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
	// 51/512 is exactly representable in binary16 and
	// trunc((51/512) * 10) = trunc(510/512) = 0.  Fractional P1 first
	// produces 0.0; the non-preserving fixed, general and decimal presentations
	// must then remove both the zero digit and the now-empty radix point.
	constexpr _Float16 value{static_cast<_Float16>(51.0 / 512.0)};
	constexpr auto precision_mode{floating_precision::fractional};
	constexpr auto rounding_mode{floating_rounding::toward_zero};

	auto const fixed_result{::fast_io::concat_std(
		fixed<precision_mode, rounding_mode>(value, 1u))};
	auto const general_result{::fast_io::concat_std(
		general<precision_mode, rounding_mode>(value, 1u))};
	auto const decimal_result{::fast_io::concat_std(
		decimal<precision_mode, rounding_mode>(value, 1u))};
	auto const comma_result{::fast_io::concat_std(
		comma_fixed<precision_mode, rounding_mode>(value, 1u))};
	auto const comma_decimal_result{::fast_io::concat_std(
		comma_decimal<precision_mode, rounding_mode>(value, 1u))};
	auto const negative_result{::fast_io::concat_std(
		fixed<precision_mode, rounding_mode>(-value, 1u))};
	auto const json_result{::fast_io::concat_std(json_float(
		fixed<precision_mode, rounding_mode>(value, 1u)))};
	auto const comma_json_result{::fast_io::concat_std(json_float(
		comma_fixed<precision_mode, rounding_mode>(value, 1u)))};

	if (fixed_result != "0" || general_result != "0" ||
		decimal_result != "0" || comma_result != "0" ||
		comma_decimal_result != "0" || negative_result != "-0" ||
		json_result != "0.0" ||
		comma_json_result != "0,0")
	{
		return 1;
	}
	return 0;
#else
	return 0;
#endif
}
