#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_freestanding.h>

namespace
{

template <::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding, bool decorated>
inline constexpr auto hex_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	flags.precision = precision;
	flags.rounding = rounding;
	flags.showbase = decorated;
	flags.showpos = decorated;
	flags.uppercase = decorated;
	flags.uppercase_e = decorated;
	flags.uppercase_showbase = decorated;
	flags.comma = decorated;
	flags.nan_show_sign = decorated;
	flags.nan_show_type = decorated;
	return flags;
}();

template <::std::integral char_type, typename flt, auto flags>
bool check_value(flt value, ::std::size_t precision) noexcept
{
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<flags, flt>;
	using runtime_type = ::fast_io::manipulators::scalar_manip_precision_t<flags, flt>;
	static_assert(::fast_io::precise_reserve_printable<char_type, scalar_type>);
	static_assert(::fast_io::precise_reserve_printable<char_type, runtime_type>);
	char_type ordinary[512u];
	char_type precise[512u];

	auto compare = [&](auto manipulator) noexcept {
		using manipulator_type = decltype(manipulator);
		auto const size{::fast_io::print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, manipulator_type>, manipulator)};
		if (sizeof(ordinary) / sizeof(*ordinary) < size)
		{
			return false;
		}
		auto const ordinary_end{::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, manipulator_type>, ordinary, manipulator)};
		auto const precise_end{::fast_io::print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, manipulator_type>, precise, size, manipulator)};
		if (static_cast<::std::size_t>(ordinary_end - ordinary) != size ||
			static_cast<::std::size_t>(precise_end - precise) != size)
		{
			return false;
		}
		for (::std::size_t index{}; index != size; ++index)
		{
			if (ordinary[index] != precise[index])
			{
				return false;
			}
		}
		return true;
	};
	return compare(scalar_type{value}) && compare(runtime_type{value, precision});
}

template <::std::integral char_type, typename flt,
		  ::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding>
bool check_decorations(flt value, ::std::size_t requested) noexcept
{
	return check_value<char_type, flt, hex_flags<precision, rounding, false>>(
			   value, requested) &&
		   check_value<char_type, flt, hex_flags<precision, rounding, true>>(
			   value, requested);
}

template <::std::integral char_type, typename flt,
		  ::fast_io::manipulators::floating_precision precision>
bool check_roundings(flt value, ::std::size_t requested) noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return check_decorations<char_type, flt, precision, rounding::nearest_to_even>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::nearest_to_odd>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::nearest_toward_plus_infinity>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::nearest_toward_minus_infinity>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::nearest_toward_zero>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::nearest_away_from_zero>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::toward_plus_infinity>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::toward_minus_infinity>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::toward_zero>(value, requested) &&
		   check_decorations<char_type, flt, precision, rounding::away_from_zero>(value, requested);
}

template <::std::integral char_type, typename flt>
bool check_precision_modes(flt value) noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	for (auto requested : {::std::size_t{}, ::std::size_t{1u}, ::std::size_t{15u},
						   ::std::size_t{16u}, ::std::size_t{17u}, ::std::size_t{32u},
						   ::std::size_t{64u}, ::std::size_t{128u}})
	{
		if (!check_roundings<char_type, flt, precision::significant>(value, requested) ||
			!check_roundings<char_type, flt, precision::fractional>(value, requested) ||
			!check_roundings<char_type, flt,
							 precision::significant_preserve_trailing_zero>(value, requested) ||
			!check_roundings<char_type, flt,
							 precision::fractional_preserve_trailing_zero>(value, requested))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, ::std::size_t size>
bool check_all_characters(flt const (&values)[size]) noexcept
{
	for (auto value : values)
	{
		if (!check_precision_modes<char>(value) ||
			!check_precision_modes<wchar_t>(value) ||
			!check_precision_modes<char8_t>(value) ||
			!check_precision_modes<char16_t>(value) ||
			!check_precision_modes<char32_t>(value))
		{
			return false;
		}
	}
	return true;
}

template <typename flt>
consteval bool wide_decimal_capability_has_scalar_and_precise() noexcept
{
	constexpr auto decimal_flags{[]() constexpr noexcept {
		auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
		flags.floating = ::fast_io::manipulators::floating_format::general;
		return flags;
	}()};
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<decimal_flags, flt>;
	using runtime_type = ::fast_io::manipulators::scalar_manip_precision_t<decimal_flags, flt>;
	/*
	The exact-interval shortest backend now supplies the scalar CPO, while the
	runtime-precision CPO continues to use the exact decimal backend.  Both are
	part of the strict binary80/binary128 representation capability.
	*/
	return ::fast_io::precise_reserve_printable<char, scalar_type> &&
		   ::fast_io::precise_reserve_printable<char, runtime_type>;
}

template <typename flt>
bool check_binary80_impl() noexcept
{
	if constexpr ((::std::numeric_limits<flt>::digits) == 64 &&
				  (::std::numeric_limits<flt>::max_exponent) == 16384)
	{
		static_assert(wide_decimal_capability_has_scalar_and_precise<flt>());
		flt const values[]{
			static_cast<flt>(0.0L),
			static_cast<flt>(-0.0L),
			static_cast<flt>(0.1L),
			static_cast<flt>(1.0L),
			static_cast<flt>(-1.5L),
			(::std::numeric_limits<flt>::denorm_min)(),
			(::std::numeric_limits<flt>::min)(),
			(::std::numeric_limits<flt>::max)(),
			(::std::numeric_limits<flt>::infinity)(),
			(::std::numeric_limits<flt>::quiet_NaN)()};
		return check_all_characters(values);
	}
	return true;
}

bool check_binary80() noexcept
{
	return check_binary80_impl<long double>();
}

// This gate describes the compiler ABI needed to construct binary128 test
// bit patterns exactly: a 16-byte floating type and a 16-byte integer carrier.
// Implementations without either representation still compile the binary80
// coverage and do not pretend that an arithmetic fallback exercises binary128.
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
constexpr __float128 binary128(__uint128_t bits) noexcept
{
	return ::std::bit_cast<__float128>(bits);
}

bool check_binary128() noexcept
{
	static_assert(wide_decimal_capability_has_scalar_and_precise<__float128>());
	constexpr __uint128_t sign{static_cast<__uint128_t>(1u) << 127u};
	constexpr __uint128_t exponent_one{static_cast<__uint128_t>(16383u) << 112u};
	constexpr __uint128_t special{static_cast<__uint128_t>(0x7fffu) << 112u};
	constexpr __uint128_t fraction_mask{(static_cast<__uint128_t>(1u) << 112u) - 1u};
	__float128 const values[]{
		binary128(0u),
		binary128(sign),
		binary128(exponent_one),
		binary128(sign | exponent_one | (static_cast<__uint128_t>(1u) << 111u)),
		binary128(1u),
		binary128(static_cast<__uint128_t>(1u) << 112u),
		binary128((static_cast<__uint128_t>(0x7ffeu) << 112u) | fraction_mask),
		binary128(special),
		binary128(special | (static_cast<__uint128_t>(1u) << 111u))};
	return check_all_characters(values);
}
#else
bool check_binary128() noexcept
{
	return true;
}
#endif

} // namespace

int main()
{
	return !(check_binary80() && check_binary128());
}
