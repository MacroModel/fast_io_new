#include <fast_io.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cfenv>
#include <limits>

namespace
{

#ifndef FAST_IO_BINARY32_PROMOTION_TEST_CHAR
#define FAST_IO_BINARY32_PROMOTION_TEST_CHAR 0
#endif

#if FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 0
using proof_char_type = char;
#elif FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 1
using proof_char_type = wchar_t;
#elif FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 2
using proof_char_type = char8_t;
#elif FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 3
using proof_char_type = char16_t;
#elif FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 4
using proof_char_type = char32_t;
#else
#error "FAST_IO_BINARY32_PROMOTION_TEST_CHAR must be in [0,4]"
#endif

[[nodiscard]] constexpr double finite_binary32_fields_to_binary64(
	::std::uint_least32_t mantissa, ::std::uint_least32_t exponent,
	bool negative) noexcept
{
	::std::uint_least64_t bits{
		static_cast<::std::uint_least64_t>(negative) << 63u};
	if (exponent)
	{
		bits |= static_cast<::std::uint_least64_t>(exponent + 896u) << 52u;
		bits |= static_cast<::std::uint_least64_t>(mantissa) << 29u;
	}
	else if (mantissa)
	{
		auto const leading{static_cast<::std::uint_least32_t>(
			::std::bit_width(mantissa) - 1u)};
		bits |= static_cast<::std::uint_least64_t>(leading + 874u) << 52u;
		bits |= static_cast<::std::uint_least64_t>(
			mantissa ^ (static_cast<::std::uint_least32_t>(1u) << leading))
			<< (52u - leading);
	}
	return ::std::bit_cast<double>(bits);
}

template <::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding, bool comma,
	bool json_float>
inline constexpr ::fast_io::manipulators::scalar_flags proof_flags{[]() consteval {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.comma = comma;
	flags.json_float = json_float;
	flags.floating = format;
	flags.rounding = rounding;
	flags.precision = precision_mode;
	return flags;
}()};

template <::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding, bool comma = false,
	bool json_float = false, ::std::integral char_type = char>
[[nodiscard]] bool compare_one(::std::uint_least32_t bits,
	::std::size_t precision) noexcept
{
	auto const exponent{static_cast<::std::uint_least32_t>((bits >> 23u) & 0xffu)};
	auto const value{::std::bit_cast<float>(bits)};
	::std::array<char_type, 512u> direct{};
	::std::array<char_type, 512u> public_cpo{};
	::std::array<char_type, 512u> promoted{};
	auto const direct_end{
		::fast_io::details::print_rsvflt_precision_define_impl<
			false, false, false, comma, format, precision_mode, rounding,
			true, false, json_float>(direct.data(), value, precision)};
	using manip_type = ::fast_io::manipulators::scalar_manip_precision_t<
		proof_flags<format, precision_mode, rounding, comma, json_float>, float>;
	manip_type manip{value, precision};
	if (exponent == 0xffu)
	{
		// The public CPO must classify the original binary32 fields. Clearing the
		// environment immediately before that call makes an accidental sNaN
		// conversion observable even though its textual spelling still matched.
		::std::feclearexcept(FE_ALL_EXCEPT);
	}
	auto const public_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, manip_type>, public_cpo.data(),
		manip)};
	if (exponent == 0xffu &&
		(::std::fetestexcept(FE_INVALID) & FE_INVALID) != 0)
	{
		return false;
	}
	auto const direct_size{static_cast<::std::size_t>(direct_end - direct.data())};
	auto const public_size{
		static_cast<::std::size_t>(public_end - public_cpo.data())};
	if (direct_size != public_size ||
		!::std::equal(direct.begin(), direct.begin() + direct_size,
			public_cpo.begin()))
	{
		return false;
	}
	if (exponent == 0xffu)
	{
		return true;
	}
	auto const mantissa{static_cast<::std::uint_least32_t>(bits & 0x7fffffu)};
	auto const widened{finite_binary32_fields_to_binary64(
		mantissa, exponent, static_cast<bool>(bits >> 31u))};
	auto const promoted_end{
		::fast_io::details::print_rsvflt_precision_define_impl<
			false, false, false, comma, format, precision_mode, rounding,
			true, false, json_float>(promoted.data(), widened, precision)};
	auto const promoted_size{
		static_cast<::std::size_t>(promoted_end - promoted.data())};
	return direct_size == promoted_size &&
		::std::equal(direct.begin(), direct.begin() + direct_size,
			promoted.begin());
}

[[noreturn]] void fail(::std::uint_least32_t bits,
	::std::size_t precision) noexcept
{
	::fast_io::io::perr("binary32 promotion mismatch: bits=", bits,
		" precision=", precision, "\n");
	::std::abort();
}

inline constexpr ::std::array<::std::size_t, 29u> precision_boundaries{
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 14u, 15u,
	16u, 17u, 18u, 19u, 20u, 22u, 23u, 24u, 32u, 38u, 39u,
	63u, 64u, 127u, 128u, 256u};

inline constexpr ::std::array<::std::uint_least32_t, 40u> value_boundaries{
	0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u,
	0x00000002u, 0x007ffffeu, 0x007fffffu, 0x807fffffu,
	0x00800000u, 0x80800000u, 0x00800001u, 0x3dccccccu,
	0x3dcccccdu, 0x3dccccceu, 0x3effffffu, 0x3f000000u,
	0x3f000001u, 0x3f7fffffu, 0x3f800000u, 0x3f800001u,
	0x3fffffffu, 0x40000000u, 0x40000001u, 0x4affffffu,
	0x4b000000u, 0x4b000001u, 0x7f000000u, 0x7f7ffffeu,
	0x7f7fffffu, 0xff7fffffu, 0x00800002u, 0x80800002u,
	0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u,
	0x7f800001u, 0xff800001u, 0x7fbfffffu, 0xffbfffffu};

template <::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding,
	::std::integral char_type>
void compare_configuration()
{
	for (auto const bits : value_boundaries)
	{
		for (auto const precision : precision_boundaries)
		{
			if (!compare_one<format, precision_mode, rounding, false, false,
					char_type>(bits, precision) ||
				!compare_one<format, precision_mode, rounding, true, false,
					char_type>(bits, precision) ||
				!compare_one<format, precision_mode, rounding, false, true,
					char_type>(bits, precision) ||
				!compare_one<format, precision_mode, rounding, true, true,
					char_type>(bits, precision))
			{
				fail(bits, precision);
			}
		}
	}
	constexpr ::std::array<::std::uint_least32_t, 6u> mantissa_boundaries{
		0u, 1u, 0x3fffffu, 0x400000u, 0x7ffffeu, 0x7fffffu};
	for (::std::uint_least32_t exponent{}; exponent != 0xffu; ++exponent)
	{
		for (::std::size_t mantissa_index{};
			 mantissa_index != mantissa_boundaries.size(); ++mantissa_index)
		{
			for (::std::uint_least32_t sign{}; sign != 2u; ++sign)
			{
				auto const bits{(sign << 31u) | (exponent << 23u) |
					mantissa_boundaries[mantissa_index]};
				auto const precision{precision_boundaries[
					(exponent + mantissa_index + sign) %
					precision_boundaries.size()]};
				if (!compare_one<format, precision_mode, rounding, false, false,
						char_type>(bits, precision))
				{
					fail(bits, precision);
				}
			}
		}
	}
	::std::uint_least32_t state{0x9e3779b9u ^
		(static_cast<::std::uint_least32_t>(format) << 24u) ^
		(static_cast<::std::uint_least32_t>(precision_mode) << 16u) ^
		(static_cast<::std::uint_least32_t>(rounding) << 8u)};
	for (::std::size_t index{}; index != 128u; ++index)
	{
		state = state * 1664525u + 1013904223u;
		auto const precision{precision_boundaries[
			(state >> 24u) % precision_boundaries.size()]};
		if (!compare_one<format, precision_mode, rounding, false, false,
				char_type>(state, precision))
		{
			fail(state, precision);
		}
	}
}

template <::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::std::integral char_type>
void compare_roundings()
{
	using enum ::fast_io::manipulators::floating_rounding;
	compare_configuration<format, precision_mode, nearest_to_even, char_type>();
	compare_configuration<format, precision_mode, nearest_to_odd, char_type>();
	compare_configuration<format, precision_mode, nearest_toward_plus_infinity,
		char_type>();
	compare_configuration<format, precision_mode, nearest_toward_minus_infinity,
		char_type>();
	compare_configuration<format, precision_mode, nearest_toward_zero, char_type>();
	compare_configuration<format, precision_mode, nearest_away_from_zero, char_type>();
	compare_configuration<format, precision_mode, toward_plus_infinity, char_type>();
	compare_configuration<format, precision_mode, toward_minus_infinity, char_type>();
	compare_configuration<format, precision_mode, toward_zero, char_type>();
	compare_configuration<format, precision_mode, away_from_zero, char_type>();
}

template <::fast_io::manipulators::floating_format format,
	::std::integral char_type>
void compare_precision_modes()
{
	using enum ::fast_io::manipulators::floating_precision;
	compare_roundings<format, significant, char_type>();
	compare_roundings<format, fractional, char_type>();
	compare_roundings<format, significant_preserve_trailing_zero, char_type>();
	compare_roundings<format, fractional_preserve_trailing_zero, char_type>();
}

template <::std::integral char_type>
void compare_decimal_formats()
{
	using enum ::fast_io::manipulators::floating_format;
	compare_precision_modes<fixed, char_type>();
	compare_precision_modes<general, char_type>();
	compare_precision_modes<scientific, char_type>();
	compare_precision_modes<decimal, char_type>();
}

void fixed_fractional_preserve_nearest_deep_random()
{
	constexpr ::std::array<::std::size_t, 17u> precisions{
		0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 14u, 15u, 16u,
		17u, 32u, 64u, 128u};
	::std::uint_least32_t state{0x243f6a88u};
	for (::std::size_t index{}; index != 1000000u; ++index)
	{
		state = state * 1664525u + 1013904223u;
		for (auto const precision : precisions)
		{
			if (!compare_one<
					::fast_io::manipulators::floating_format::fixed,
					::fast_io::manipulators::floating_precision::
						fractional_preserve_trailing_zero,
					::fast_io::manipulators::floating_rounding::nearest_to_even>(
					state, precision))
			{
				fail(state, precision);
			}
		}
	}
}

} // namespace

int main()
{
	static_assert(finite_binary32_fields_to_binary64(0u, 0u, false) == 0.0);
	static_assert(::std::bit_cast<::std::uint_least64_t>(
		finite_binary32_fields_to_binary64(0u, 0u, true)) ==
		(static_cast<::std::uint_least64_t>(1u) << 63u));
	static_assert(finite_binary32_fields_to_binary64(0u, 127u, false) == 1.0);
	static_assert(finite_binary32_fields_to_binary64(1u, 0u, false) ==
		static_cast<double>((::std::numeric_limits<float>::denorm_min)()));
	compare_decimal_formats<proof_char_type>();
	if constexpr (FAST_IO_BINARY32_PROMOTION_TEST_CHAR == 0)
	{
		fixed_fractional_preserve_nearest_deep_random();
	}
}
