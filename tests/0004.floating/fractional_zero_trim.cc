#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

#include <fast_io.h>

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
	constexpr auto significant_mode{floating_precision::significant};
	constexpr auto preserving_mode{
		floating_precision::fractional_preserve_trailing_zero};

	/*
	A one-digit scientific coefficient still has a four-character exponent
	suffix (`e`, sign, two digits).  Thus `0.001` and `1e-03`, and likewise
	`10000` and `1e+04`, are equal-length alternatives.  Decimal format must
	choose fixed on equality.  The volatile field source exercises the runtime
	DA path; literals exercise the compiler-constant proxy; wide output reaches
	the code-unit-generic renderer; significant precision reaches the exact
	virtual-length selector.  Exact five-byte to_chars buffers additionally pin
	the precise-size/emission agreement and the no-overstore boundary.
	*/
	volatile ::std::uint64_t low_bits{
		::std::bit_cast<::std::uint64_t>(0.001)};
	volatile ::std::uint64_t high_bits{
		::std::bit_cast<::std::uint64_t>(10000.0)};
	auto const runtime_low{
		::std::bit_cast<double>(
			static_cast<::std::uint64_t>(low_bits))};
	auto const runtime_high{
		::std::bit_cast<double>(
			static_cast<::std::uint64_t>(high_bits))};
	auto const runtime_low_text{::fast_io::concat_std(runtime_low)};
	auto const runtime_high_text{::fast_io::concat_std(runtime_high)};
	auto const constant_low_text{::fast_io::concat_std(0.001)};
	auto const constant_high_text{::fast_io::concat_std(10000.0)};
	auto const wide_low_text{::fast_io::wconcat_std(runtime_low)};
	auto const precision_low_text{::fast_io::concat_std(
		decimal<significant_mode, nearest>(runtime_low, 1u))};
	auto const precision_high_text{::fast_io::concat_std(
		decimal<significant_mode, nearest>(runtime_high, 1u))};
	char low_buffer[6u]{};
	char high_buffer[6u]{};
	low_buffer[5] = '!';
	high_buffer[5] = '!';
	auto const low_to_chars{
		::fast_io::to_chars(
			low_buffer, low_buffer + 5u, runtime_low)};
	auto const high_to_chars{
		::fast_io::to_chars(
			high_buffer, high_buffer + 5u, runtime_high)};
	if (runtime_low_text != "0.001" ||
		runtime_high_text != "10000" ||
		constant_low_text != "0.001" ||
		constant_high_text != "10000" ||
		wide_low_text != L"0.001" ||
		precision_low_text != "0.001" ||
		precision_high_text != "10000" ||
		low_to_chars.ec != ::std::errc{} ||
		low_to_chars.ptr != low_buffer + 5u ||
		::std::string_view(low_buffer, 5u) != "0.001" ||
		high_to_chars.ec != ::std::errc{} ||
		high_to_chars.ptr != high_buffer + 5u ||
		::std::string_view(high_buffer, 5u) != "10000" ||
		low_buffer[5] != '!' || high_buffer[5] != '!')
	{
		return 1;
	}

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
	// The compact binary64 carrier may end before the requested fractional
	// quantum.  If general selects scientific notation, preserving P decimal
	// places still requires P+R coefficient places, where R is the displayed
	// exponent.  The terminal zero below represents 10^-27 and is not optional.
	constexpr auto scientific_quantum_value{::std::bit_cast<double>(
		::std::uint64_t{0x3d44e27112e2ae54ULL})};
	constexpr auto nearest_upward{
		floating_rounding::nearest_toward_plus_infinity};
	auto const scientific_quantum{::fast_io::concat_std(
		general<preserving_mode, nearest_upward>(scientific_quantum_value, 27u))};
	auto const scientific_quantum_comma{::fast_io::concat_std(
		comma_general<preserving_mode, nearest_upward>(scientific_quantum_value, 27u))};
	auto const scientific_quantum_wide{::fast_io::wconcat_std(
		general<preserving_mode, nearest_upward>(scientific_quantum_value, 27u))};
	// Conversely, a dyadic value already exact on the requested grid does not
	// acquire a synthetic quantum merely because the shortest carrier is shorter
	// than P+R.  The exact-window tail bit distinguishes this case from the
	// rounded value above.
	constexpr auto exact_scientific_value{::std::bit_cast<double>(
		::std::uint64_t{0x4110000001000000ULL})};
	constexpr auto nearest_odd{floating_rounding::nearest_to_odd};
	auto const exact_scientific{::fast_io::concat_std(
		general<preserving_mode, nearest_odd>(exact_scientific_value, 42u))};
	// Exact reservation is an independent length computation, not a formatted
	// scratch-buffer measurement.  It must observe the same distinction between
	// a rounded synthetic quantum and an already-grid-exact dyadic value.
	auto scientific_quantum_manip{
		general<preserving_mode, nearest_upward>(scientific_quantum_value, 27u)};
	auto exact_scientific_manip{
		general<preserving_mode, nearest_odd>(exact_scientific_value, 42u)};
	auto const scientific_quantum_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<char, decltype(scientific_quantum_manip)>,
		scientific_quantum_manip)};
	auto const exact_scientific_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<char, decltype(exact_scientific_manip)>,
		exact_scientific_manip)};
	if (ordinary_preserved != "-0.0" || ordinary_nonpreserved != "-0" ||
		ordinary_preserved_comma != "-0,0" || ordinary_preserved_json != "-0.0" ||
		ordinary_preserved_scientific != "-0e-05" ||
		exact_window_nonpreserved != "-0" || exact_window_nonpreserved_comma != "-0" ||
		exact_window_nonpreserved_json != "-0.0" || ordinary_padded != "1.250" ||
		wide_padded != "1.25000000000000000000" ||
		scientific_quantum != "1.48393566724010e-13" ||
		scientific_quantum_comma != "1,48393566724010e-13" ||
		scientific_quantum_wide != L"1.48393566724010e-13" ||
		exact_scientific != "2.621440009765625e+05" ||
		scientific_quantum_size != scientific_quantum.size() ||
		exact_scientific_size != exact_scientific.size())
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

	// Binary64 values with raw exponent at least 1075 are exact integers and use
	// the P1-P14 direct integer writer.  P0 must remain on the generic terminal
	// path because JSON still requires a floating marker even though no fractional
	// digits were requested.  Exercise both format aliases, punctuation choices,
	// signs and a non-narrow destination at the first qualifying exponent.
	constexpr double exact_integer{4503599627370496.0};
	auto const exact_integer_fixed{::fast_io::concat_std(
		fixed<preserving_mode, nearest>(exact_integer, 0u))};
	auto const exact_integer_fixed_json{::fast_io::concat_std(json_float(
		fixed<preserving_mode, nearest>(exact_integer, 0u)))};
	auto const exact_integer_decimal_json{::fast_io::concat_std(json_float(
		decimal<preserving_mode, nearest>(exact_integer, 0u)))};
	auto const exact_integer_comma_json{::fast_io::concat_std(json_float(
		comma_fixed<preserving_mode, nearest>(exact_integer, 0u)))};
	auto const exact_integer_negative_json{::fast_io::concat_std(json_float(
		fixed<preserving_mode, nearest>(-exact_integer, 0u)))};
	auto const exact_integer_wide_json{::fast_io::wconcat_std(json_float(
		fixed<preserving_mode, nearest>(exact_integer, 0u)))};
	if (exact_integer_fixed != "4503599627370496" ||
		exact_integer_fixed_json != "4503599627370496.0" ||
		exact_integer_decimal_json != "4503599627370496.0" ||
		exact_integer_comma_json != "4503599627370496,0" ||
		exact_integer_negative_json != "-4503599627370496.0" ||
		exact_integer_wide_json != L"4503599627370496.0")
	{
		return 1;
	}

	// A shortest decimal carrier with P digits is not necessarily the correctly
	// rounded P-digit coefficient.  This normal binary64 value lies below the
	// midpoint following ...3044, while its independently valid shortest carrier
	// ends in ...3045.  Every significant/scientific entry must therefore reach
	// the DA interval proof or exact guard/sticky path instead of accepting the
	// shortest carrier merely because its length equals the requested precision.
	constexpr auto equal_length_boundary{::std::bit_cast<double>(
		::std::uint64_t{0x0060000000000000ULL})};
	auto const equal_length_general{::fast_io::concat_std(
		general<significant_mode, nearest>(equal_length_boundary, 16u))};
	auto const equal_length_scientific{::fast_io::concat_std(
		scientific<significant_mode, nearest>(equal_length_boundary, 16u))};
	auto const equal_length_preserved{::fast_io::concat_std(
		scientific<significant_preserving_mode, nearest>(equal_length_boundary, 16u))};
	auto const equal_length_fractional{::fast_io::concat_std(
		scientific<preserving_mode, nearest>(equal_length_boundary, 15u))};
	auto const equal_length_comma{::fast_io::concat_std(
		comma_scientific<significant_mode, nearest>(equal_length_boundary, 16u))};
	auto const equal_length_wide{::fast_io::wconcat_std(
		scientific<significant_mode, nearest>(equal_length_boundary, 16u))};
	if (equal_length_general != "7.120236347223044e-307" ||
		equal_length_scientific != "7.120236347223044e-307" ||
		equal_length_preserved != "7.120236347223044e-307" ||
		equal_length_fractional != "7.120236347223044e-307" ||
		equal_length_comma != "7,120236347223044e-307" ||
		equal_length_wide != L"7.120236347223044e-307")
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
