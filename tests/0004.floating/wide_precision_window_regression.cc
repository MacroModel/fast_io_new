#include <bit>
#include <cstddef>
#include <cstdint>

#include <fast_io.h>

namespace
{

using format = ::fast_io::manipulators::floating_format;
using precision_mode = ::fast_io::manipulators::floating_precision;
using rounding = ::fast_io::manipulators::floating_rounding;

#if defined(__SIZEOF_INT128__)
consteval bool exact_tail_classification_is_correct() noexcept
{
	constexpr __uint128_t one{static_cast<__uint128_t>(1u) << 112u};
	auto const exact{::fast_io::details::
		exact_precision_wide_window_from_significand(
			one, 112u, -112, 20u)};
	auto const inexact{::fast_io::details::
		exact_precision_wide_window_from_significand(
			one | 1u, 112u, -112, 20u)};
	return exact.success && !exact.tail_nonzero &&
		exact.real_exponent == 0 && exact.decimal.digits[0] == 1u &&
		inexact.success && inexact.tail_nonzero;
}

static_assert(exact_tail_classification_is_correct());

template <typename floating_type, rounding rounding_mode,
	precision_mode precision_rule, format presentation>
[[nodiscard]] bool check_against_full_exact(
	floating_type value, ::std::size_t precision) noexcept
{
	char actual[20000u]{};
	auto const actual_end{
		::fast_io::details::print_rsvflt_precision_define_impl<
			false, false, false, false, presentation, precision_rule,
			rounding_mode, true, false, false>(
				actual, value, precision)};

	auto const fields{::fast_io::details::get_punned_result(value)};
	char reference[20000u]{};
	auto reference_end{::fast_io::details::print_rsv_fp_sign_impl<false>(
		reference, fields.sign)};
	reference_end =
		::fast_io::details::print_rsvflt_exact_precision_define_impl<
			floating_type, false, false, presentation, precision_rule,
			rounding_mode, false>(
				reference_end, fields.mantissa, fields.exponent,
				precision, fields.sign);
	auto const actual_size{
		static_cast<::std::size_t>(actual_end - actual)};
	auto const reference_size{
		static_cast<::std::size_t>(reference_end - reference)};
	if (actual_size != reference_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != actual_size; ++index)
	{
		if (actual[index] != reference[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool check_binary128() noexcept
{
#if defined(__SIZEOF_FLOAT128__)
	constexpr __uint128_t exponent{
		static_cast<__uint128_t>(0x3fffu) << 112u};
	constexpr __uint128_t maximum_exponent{
		static_cast<__uint128_t>(0x7ffeu) << 112u};
	auto const exact_one{::std::bit_cast<__float128>(exponent)};
	auto const adjacent{::std::bit_cast<__float128>(exponent | 1u)};
	auto const huge{::std::bit_cast<__float128>(
		maximum_exponent |
		(static_cast<__uint128_t>(UINT64_C(0x123456789abcdef0)) << 48u) |
		UINT64_C(0x0fedcba98765))};
	bool result{check_against_full_exact<
			   __float128, rounding::nearest_to_even,
			   precision_mode::significant, format::fixed>(huge, 6u) &&
		check_against_full_exact<
			__float128, rounding::nearest_to_odd,
			precision_mode::significant_preserve_trailing_zero,
			format::general>(exact_one, 128u) &&
		check_against_full_exact<
			__float128, rounding::nearest_toward_plus_infinity,
			precision_mode::significant_preserve_trailing_zero,
			format::scientific>(adjacent, 17u) &&
		check_against_full_exact<
			__float128, rounding::away_from_zero,
			precision_mode::fractional_preserve_trailing_zero,
			format::decimal>(adjacent, 9u)};
	/*
	The positive-exponent anchor table is used only after 64 base-1e9 power-of-two
	chunks.  Exercise each of its seven nonzero anchors against the independent
	512-bit prefix window: the latter supplies the selected P=6 result without
	materializing this complete integer coefficient.
	*/
	constexpr ::std::uint_least16_t anchor_exponents[]{
		18671u, 20847u, 23023u, 25199u, 27375u, 29551u, 31727u};
	for (auto raw_exponent : anchor_exponents)
	{
		auto const value{::std::bit_cast<__float128>(
			(static_cast<__uint128_t>(raw_exponent) << 112u) |
			(static_cast<__uint128_t>(UINT64_C(0x0fedcba987654321)) << 48u) |
			UINT64_C(0x0000abcd1234))};
		result = result && check_against_full_exact<
			__float128, rounding::nearest_to_even,
			precision_mode::significant, format::fixed>(value, 6u);
	}
	return result;
#else
	return true;
#endif
}

[[nodiscard]] bool check_binary80() noexcept
{
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64
	auto const patterned{__builtin_ldexpl(
		static_cast<long double>(UINT64_C(0xfedcba9876543210)),
		12000 - 63)};
	auto const adjacent{1.0L +
		__builtin_ldexpl(1.0L, -63)};
	bool result{check_against_full_exact<
			   long double, rounding::nearest_to_even,
			   precision_mode::significant, format::fixed>(
				   patterned, 6u) &&
		check_against_full_exact<
			long double, rounding::nearest_away_from_zero,
			precision_mode::fractional, format::scientific>(
				adjacent, 17u) &&
		check_against_full_exact<
			long double, rounding::toward_minus_infinity,
			precision_mode::fractional_preserve_trailing_zero,
			format::general>(-adjacent, 9u)};
	constexpr int anchor_exponents[]{
		2239, 4415, 6591, 8767, 10943, 13119, 15295};
	for (auto binary_exponent : anchor_exponents)
	{
		auto const value{__builtin_ldexpl(
			static_cast<long double>(UINT64_C(0xfedcba9876543210)),
			binary_exponent - 63)};
		result = result && check_against_full_exact<
			long double, rounding::nearest_to_even,
			precision_mode::significant, format::fixed>(value, 6u);
	}
	return result;
#else
	return true;
#endif
}
#endif

} // namespace

int main()
{
#if defined(__SIZEOF_INT128__)
	return !(check_binary128() && check_binary80());
#else
	return 0;
#endif
}
