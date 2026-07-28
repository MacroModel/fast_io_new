#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

#ifndef FAST_IO_PRECISE_TEST_CHAR_TYPE
#define FAST_IO_PRECISE_TEST_CHAR_TYPE char
#endif

#ifndef FAST_IO_PRECISE_RANDOM_VALUES
#define FAST_IO_PRECISE_RANDOM_VALUES 0u
#endif

namespace
{

using test_char_type = FAST_IO_PRECISE_TEST_CHAR_TYPE;

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	flags.precision = precision_mode;
	flags.showbase = decorated;
	flags.showpos = decorated;
	flags.uppercase_showbase = decorated;
	flags.uppercase = decorated;
	flags.uppercase_e = decorated;
	flags.comma = decorated;
	flags.json_float = decorated && format != ::fast_io::manipulators::floating_format::hexfloat;
	flags.nan_show_type = decorated;
	return flags;
}();

constexpr auto current_decimal_flags{test_flags<
	::fast_io::manipulators::floating_format::general,
	::fast_io::manipulators::floating_rounding::current_environment,
	::fast_io::manipulators::floating_precision::significant, false>};
constexpr auto current_hex_flags{test_flags<
	::fast_io::manipulators::floating_format::hexfloat,
	::fast_io::manipulators::floating_rounding::current_environment,
	::fast_io::manipulators::floating_precision::significant, false>};
using current_decimal_scalar =
	::fast_io::manipulators::scalar_manip_t<current_decimal_flags, double>;
using current_decimal_precision =
	::fast_io::manipulators::scalar_manip_precision_t<current_decimal_flags, double>;
using current_hex_scalar =
	::fast_io::manipulators::scalar_manip_t<current_hex_flags, double>;
using current_hex_precision =
	::fast_io::manipulators::scalar_manip_precision_t<current_hex_flags, double>;
static_assert(!::fast_io::precise_reserve_printable<test_char_type, current_decimal_scalar>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, current_decimal_precision>);
static_assert(::fast_io::precise_reserve_printable<test_char_type, current_hex_scalar>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, current_hex_precision>);

constexpr auto invalid_precision_flags{[]() constexpr noexcept {
	auto flags{test_flags<::fast_io::manipulators::floating_format::general,
						  ::fast_io::manipulators::floating_rounding::nearest_to_even,
						  ::fast_io::manipulators::floating_precision::significant, false>};
	flags.precision = static_cast<::fast_io::manipulators::floating_precision>(255u);
	return flags;
}()};
constexpr auto invalid_format_flags{[]() constexpr noexcept {
	auto flags{test_flags<::fast_io::manipulators::floating_format::general,
						  ::fast_io::manipulators::floating_rounding::nearest_to_even,
						  ::fast_io::manipulators::floating_precision::significant, false>};
	flags.floating = static_cast<::fast_io::manipulators::floating_format>(255u);
	return flags;
}()};
constexpr auto invalid_rounding_flags{[]() constexpr noexcept {
	auto flags{test_flags<::fast_io::manipulators::floating_format::general,
						  ::fast_io::manipulators::floating_rounding::nearest_to_even,
						  ::fast_io::manipulators::floating_precision::significant, false>};
	flags.rounding = static_cast<::fast_io::manipulators::floating_rounding>(255u);
	return flags;
}()};
constexpr auto invalid_base_flags{[]() constexpr noexcept {
	auto flags{test_flags<::fast_io::manipulators::floating_format::general,
						  ::fast_io::manipulators::floating_rounding::nearest_to_even,
						  ::fast_io::manipulators::floating_precision::significant, false>};
	flags.base = 16u;
	return flags;
}()};
using invalid_precision_type =
	::fast_io::manipulators::scalar_manip_precision_t<invalid_precision_flags, double>;
using invalid_format_scalar =
	::fast_io::manipulators::scalar_manip_t<invalid_format_flags, double>;
using invalid_format_precision =
	::fast_io::manipulators::scalar_manip_precision_t<invalid_format_flags, double>;
using invalid_rounding_scalar =
	::fast_io::manipulators::scalar_manip_t<invalid_rounding_flags, double>;
using invalid_rounding_precision =
	::fast_io::manipulators::scalar_manip_precision_t<invalid_rounding_flags, double>;
using invalid_base_scalar =
	::fast_io::manipulators::scalar_manip_t<invalid_base_flags, double>;
using invalid_base_precision =
	::fast_io::manipulators::scalar_manip_precision_t<invalid_base_flags, double>;
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_precision_type>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_format_scalar>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_format_precision>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_rounding_scalar>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_rounding_precision>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_base_scalar>);
static_assert(!::fast_io::precise_reserve_printable<test_char_type, invalid_base_precision>);

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
bool check_value(flt value, ::std::size_t precision) noexcept
{
	constexpr auto flags{test_flags<format, rounding, precision_mode, decorated>};
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<flags, flt>;
	using precision_type = ::fast_io::manipulators::scalar_manip_precision_t<flags, flt>;
	static_assert(::fast_io::precise_reserve_printable<test_char_type, scalar_type>);
	static_assert(::fast_io::precise_reserve_printable<test_char_type, precision_type>);

	test_char_type buffer[4096u];
	test_char_type precise_buffer[4096u];
	scalar_type scalar{value};
	auto const scalar_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, scalar)};
	auto const scalar_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, buffer, scalar)};
	if (static_cast<::std::size_t>(scalar_end - buffer) != scalar_size)
	{
		::std::fprintf(stderr, "scalar mismatch: format=%u rounding=%u decorated=%u expected=%zu actual=%zu\n",
					   static_cast<unsigned>(format), static_cast<unsigned>(rounding),
					   static_cast<unsigned>(decorated), scalar_size,
					   static_cast<::std::size_t>(scalar_end - buffer));
		return false;
	}
	auto const scalar_precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, precise_buffer, scalar_size, scalar)};
	if (static_cast<::std::size_t>(scalar_precise_end - precise_buffer) != scalar_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != scalar_size; ++index)
	{
		if (buffer[index] != precise_buffer[index])
		{
			return false;
		}
	}

	precision_type with_precision{value, precision};
	auto const precise_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<test_char_type, precision_type>, with_precision)};
	if (sizeof(buffer) / sizeof(*buffer) < precise_size)
	{
		return false;
	}
	auto const precise_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<test_char_type, precision_type>, buffer, with_precision)};
	if (static_cast<::std::size_t>(precise_end - buffer) != precise_size)
	{
		::std::fprintf(stderr,
					   "precision mismatch: format=%u rounding=%u mode=%u decorated=%u p=%zu expected=%zu actual=%zu\n",
					   static_cast<unsigned>(format), static_cast<unsigned>(rounding),
					   static_cast<unsigned>(precision_mode), static_cast<unsigned>(decorated), precision,
					   precise_size, static_cast<::std::size_t>(precise_end - buffer));
		return false;
	}
	auto const protocol_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<test_char_type, precision_type>, precise_buffer,
		precise_size, with_precision)};
	if (static_cast<::std::size_t>(protocol_end - precise_buffer) != precise_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (buffer[index] != precise_buffer[index])
		{
			return false;
		}
	}
	return true;
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
bool check_configuration(flt value) noexcept
{
	for (::std::size_t precision{}; precision != 21u; ++precision)
	{
		if (!check_value<flt, format, rounding, precision_mode, decorated>(value, precision))
		{
			return false;
		}
	}
	for (auto precision : {32u, 64u, 128u})
	{
		if (!check_value<flt, format, rounding, precision_mode, decorated>(value, precision))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode>
bool check_decorations(flt value) noexcept
{
	return check_configuration<flt, format, rounding, precision_mode, false>(value) &&
		   check_configuration<flt, format, rounding, precision_mode, true>(value);
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
bool check_precision_modes(flt value) noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	return check_decorations<flt, format, rounding, precision::significant>(value) &&
		   check_decorations<flt, format, rounding, precision::fractional>(value) &&
		   check_decorations<flt, format, rounding, precision::significant_preserve_trailing_zero>(value) &&
		   check_decorations<flt, format, rounding, precision::fractional_preserve_trailing_zero>(value);
}

template <typename flt, ::fast_io::manipulators::floating_format format>
bool check_roundings(flt value) noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return check_precision_modes<flt, format, rounding::nearest_to_even>(value) &&
		   check_precision_modes<flt, format, rounding::nearest_to_odd>(value) &&
		   check_precision_modes<flt, format, rounding::nearest_toward_plus_infinity>(value) &&
		   check_precision_modes<flt, format, rounding::nearest_toward_minus_infinity>(value) &&
		   check_precision_modes<flt, format, rounding::nearest_toward_zero>(value) &&
		   check_precision_modes<flt, format, rounding::nearest_away_from_zero>(value) &&
		   check_precision_modes<flt, format, rounding::toward_plus_infinity>(value) &&
		   check_precision_modes<flt, format, rounding::toward_minus_infinity>(value) &&
		   check_precision_modes<flt, format, rounding::toward_zero>(value) &&
		   check_precision_modes<flt, format, rounding::away_from_zero>(value);
}

template <typename flt>
bool check_formats(flt value) noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	return check_roundings<flt, format::general>(value) &&
		   check_roundings<flt, format::decimal>(value) &&
		   check_roundings<flt, format::fixed>(value) &&
		   check_roundings<flt, format::scientific>(value) &&
		   check_roundings<flt, format::hexfloat>(value);
}

template <typename flt, typename uint_type>
bool check_bits(uint_type bits) noexcept
{
	return check_formats(::std::bit_cast<flt>(bits));
}

template <typename flt, typename uint_type>
bool check_type() noexcept
{
	constexpr auto mantissa_bits{::fast_io::details::iec559_traits<flt>::mbits};
	constexpr auto exponent_bits{::fast_io::details::iec559_traits<flt>::ebits};
	constexpr uint_type special_exponent{
		((uint_type{1u} << exponent_bits) - 1u) << mantissa_bits};
	constexpr uint_type quiet_nan{special_exponent | (uint_type{1u} << (mantissa_bits - 1u))};
	constexpr uint_type patterns[]{
		0u,
		uint_type{1u},
		uint_type{1u} << (sizeof(uint_type) * 8u - 1u),
		(uint_type{1u} << (sizeof(uint_type) * 8u - 1u)) | uint_type{1u},
		::std::bit_cast<uint_type>(static_cast<flt>(0.000123456789)),
		::std::bit_cast<uint_type>(static_cast<flt>(-0.000123456789)),
		::std::bit_cast<uint_type>(static_cast<flt>(0.1)),
		::std::bit_cast<uint_type>(static_cast<flt>(0.5)),
		::std::bit_cast<uint_type>(static_cast<flt>(1.0)),
		::std::bit_cast<uint_type>(static_cast<flt>(9.999999)),
		::std::bit_cast<uint_type>((::std::numeric_limits<flt>::min)()),
		::std::bit_cast<uint_type>((::std::numeric_limits<flt>::max)()),
		special_exponent,
		quiet_nan};
	for (auto bits : patterns)
	{
		if (!check_bits<flt>(bits))
		{
			return false;
		}
	}
	uint_type state{static_cast<uint_type>(0x9e3779b97f4a7c15ull)};
	for (::std::size_t index{}; index != FAST_IO_PRECISE_RANDOM_VALUES; ++index)
	{
		// This Weyl/xorshift sequence is deterministic across compilers and covers
		// signs, exponent classes and mantissa boundaries without depending on a
		// hosted random-number facility.
		state += static_cast<uint_type>(0x9e3779b97f4a7c15ull);
		state ^= static_cast<uint_type>(state << 7u);
		state ^= static_cast<uint_type>(state >> 9u);
		if (!check_bits<flt>(state))
		{
			return false;
		}
	}
	return true;
}

template <auto flags, typename flt>
bool check_exact_protocol(flt value, ::std::size_t precision) noexcept
{
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<flags, flt>;
	using precision_type = ::fast_io::manipulators::scalar_manip_precision_t<flags, flt>;
	static_assert(::fast_io::precise_reserve_printable<test_char_type, scalar_type>);
	static_assert(::fast_io::precise_reserve_printable<test_char_type, precision_type>);
	test_char_type buffer[512u];
	test_char_type precise_buffer[512u];
	scalar_type scalar{value};
	auto const scalar_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, scalar)};
	auto const scalar_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, buffer, scalar)};
	if (static_cast<::std::size_t>(scalar_end - buffer) != scalar_size)
	{
		return false;
	}
	auto const scalar_precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<test_char_type, scalar_type>, precise_buffer,
		scalar_size, scalar)};
	if (static_cast<::std::size_t>(scalar_precise_end - precise_buffer) != scalar_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != scalar_size; ++index)
	{
		if (buffer[index] != precise_buffer[index])
		{
			return false;
		}
	}
	precision_type runtime{value, precision};
	auto const runtime_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<test_char_type, precision_type>, runtime)};
	auto const runtime_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<test_char_type, precision_type>, buffer, runtime)};
	if (static_cast<::std::size_t>(runtime_end - buffer) != runtime_size)
	{
		return false;
	}
	auto const runtime_precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<test_char_type, precision_type>, precise_buffer,
		runtime_size, runtime)};
	if (static_cast<::std::size_t>(runtime_precise_end - precise_buffer) != runtime_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != runtime_size; ++index)
	{
		if (buffer[index] != precise_buffer[index])
		{
			return false;
		}
	}
	return true;
}

template <typename flt, bool showpos, bool nan_show_sign, bool nan_show_type,
		  ::fast_io::manipulators::floating_format format>
bool check_special_policy_value(flt value) noexcept
{
	constexpr auto flags{[]() constexpr noexcept {
		auto result{::fast_io::manipulators::floating_point_default_scalar_flags};
		result.floating = format;
		result.precision = ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero;
		result.showpos = showpos;
		result.nan_show_sign = nan_show_sign;
		result.nan_show_type = nan_show_type;
		return result;
	}()};
	return check_exact_protocol<flags>(value, 7u);
}

template <typename flt, typename uint_type, bool showpos, bool nan_show_sign, bool nan_show_type>
bool check_special_policy() noexcept
{
	constexpr auto trait_mbits{::fast_io::details::iec559_traits<flt>::mbits};
	constexpr auto trait_ebits{::fast_io::details::iec559_traits<flt>::ebits};
	constexpr uint_type sign_bit{uint_type{1u} << (trait_mbits + trait_ebits)};
	constexpr uint_type exponent_mask{((uint_type{1u} << trait_ebits) - 1u) << trait_mbits};
	constexpr uint_type quiet_bit{uint_type{1u} << (trait_mbits - 1u)};
	constexpr uint_type values[]{
		exponent_mask,
		sign_bit | exponent_mask,
		exponent_mask | quiet_bit,
		sign_bit | exponent_mask | quiet_bit, // fast_io's indeterminate spelling
		exponent_mask | uint_type{1u},
		sign_bit | exponent_mask | uint_type{1u},
		uint_type{},
		sign_bit};
	for (auto bits : values)
	{
		auto const value{::std::bit_cast<flt>(bits)};
		if (!check_special_policy_value<flt, showpos, nan_show_sign, nan_show_type,
										::fast_io::manipulators::floating_format::decimal>(value) ||
			!check_special_policy_value<flt, showpos, nan_show_sign, nan_show_type,
										::fast_io::manipulators::floating_format::hexfloat>(value))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, typename uint_type>
bool check_special_policies() noexcept
{
	return check_special_policy<flt, uint_type, false, false, false>() &&
		   check_special_policy<flt, uint_type, false, false, true>() &&
		   check_special_policy<flt, uint_type, false, true, false>() &&
		   check_special_policy<flt, uint_type, false, true, true>() &&
		   check_special_policy<flt, uint_type, true, false, false>() &&
		   check_special_policy<flt, uint_type, true, false, true>() &&
		   check_special_policy<flt, uint_type, true, true, false>() &&
		   check_special_policy<flt, uint_type, true, true, true>();
}

bool check_decimal_boundaries() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	using rounding = ::fast_io::manipulators::floating_rounding;
	using precision = ::fast_io::manipulators::floating_precision;
	constexpr double values[]{
		9.49, 9.5, 9.51, 9.99, 99.49, 99.5, 99.9, 999.5,
		1e9, 1e10, 1e99, 1e100, 1e-9, 1e-10, 1e-99, 1e-100};
	for (auto value : values)
	{
		for (::std::size_t p{1u}; p != 4u; ++p)
		{
			if (!check_value<double, format::general, rounding::nearest_to_even,
							 precision::significant_preserve_trailing_zero, false>(value, p) ||
				!check_value<double, format::decimal, rounding::nearest_to_odd,
							 precision::significant, true>(value, p) ||
				!check_value<double, format::fixed, rounding::toward_plus_infinity,
							 precision::fractional_preserve_trailing_zero, false>(value, p) ||
				!check_value<double, format::scientific, rounding::toward_minus_infinity,
							 precision::significant_preserve_trailing_zero, true>(value, p))
			{
				return false;
			}
		}
	}
	return true;
}

bool check_general_significant_layout_boundaries() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	using rounding = ::fast_io::manipulators::floating_rounding;
	using precision = ::fast_io::manipulators::floating_precision;
	using namespace ::fast_io::manipulators;
	constexpr ::std::uint_least32_t patterns[]{
		0xbcb8b5fcu, 0x3f47314cu, 0x4da41375u, 0x4a90948au,
		0x3bd361bfu, 0xcae0aebeu, 0xc0b6a521u};
	constexpr ::std::string_view expected[]{
		"-0.0225477", "0.778096", "3.44092e+08", "4.7376e+06",
		"0.00645086", "-7.3624e+06", "-5.70766"};
	for (::std::size_t index{}; index != ::std::size(patterns); ++index)
	{
		::std::uint_least32_t volatile runtime_bits{patterns[index]};
		auto const value{::std::bit_cast<float>(
			static_cast<::std::uint_least32_t>(runtime_bits))};
		auto manip{
			general<precision::significant, rounding::nearest_to_even>(
				value, 6u)};
		char buffer[64u];
		auto const end{::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char, decltype(manip)>,
			buffer, manip)};
		if (::std::string_view(
				buffer, static_cast<::std::size_t>(end - buffer)) !=
				expected[index] ||
			!check_value<float, format::general, rounding::nearest_to_even,
						 precision::significant, false>(value, 6u))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, ::fast_io::manipulators::floating_format format>
bool check_huge_fractional_precision_format() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	using precision = ::fast_io::manipulators::floating_precision;
	constexpr auto uint32_max{(::std::numeric_limits<::std::uint_least32_t>::max)()};
	constexpr auto size_max{(::std::numeric_limits<::std::size_t>::max)()};
	auto check = [](flt value, ::std::size_t requested) noexcept {
		return check_value<flt, format, rounding::nearest_to_even,
						   precision::fractional, false>(value, requested);
	};
	for (auto value : {static_cast<flt>(0.5), static_cast<flt>(-0.5)})
	{
		if (!check(value, static_cast<::std::size_t>(uint32_max)) ||
			!check(value, size_max))
		{
			return false;
		}
		if constexpr (static_cast<::std::uint_least64_t>(uint32_max) <
					  static_cast<::std::uint_least64_t>(size_max))
		{
			if (!check(value, static_cast<::std::size_t>(uint32_max) + 1u))
			{
				return false;
			}
		}
	}
	return true;
}

template <typename flt>
bool check_huge_fractional_precision() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	// Non-preserving fractional output remains short for these values, so both
	// the ordinary and precise protocols can be compared safely even at the
	// maximum value of size_type.
	// Scientific preserving output is deliberately absent: its P-wide field must
	// fail the reserve-size overflow contract before any destination write.
	return check_huge_fractional_precision_format<flt, format::general>() &&
		   check_huge_fractional_precision_format<flt, format::decimal>() &&
		   check_huge_fractional_precision_format<flt, format::fixed>();
}

} // namespace

int main()
{
#if defined(FAST_IO_PRECISE_TEST_HUGE_FRACTIONAL)
	return !(check_huge_fractional_precision<float>() &&
			 check_huge_fractional_precision<double>());
#elif defined(FAST_IO_PRECISE_TEST_BFLOAT16)
#if defined(__GNUC__) && !defined(__clang__) && defined(__BFLT16_MANT_DIG__)
	// GCC's C++20 __bf16 domain is exactly widened to binary32 by decimal
	// emission; the precise protocol must match that widening for every policy.
	return !check_type<__bf16, ::std::uint_least16_t>();
#else
	return 0;
#endif
#elif defined(FAST_IO_PRECISE_TEST_FLOAT16)
	// _Float16 is the C++20 spelling advertised by __FLT16_MANT_DIG__ on the
	// Clang/GCC targets that provide this IEC 60559 format.
	return !check_type<_Float16, ::std::uint_least16_t>();
#else
	return !(check_type<float, ::std::uint_least32_t>() &&
			 check_type<double, ::std::uint_least64_t>() &&
			 check_special_policies<float, ::std::uint_least32_t>() &&
			 check_special_policies<double, ::std::uint_least64_t>() &&
			 check_decimal_boundaries() &&
			 check_general_significant_layout_boundaries() &&
			 check_huge_fractional_precision<float>() &&
			 check_huge_fractional_precision<double>());
#endif
}
