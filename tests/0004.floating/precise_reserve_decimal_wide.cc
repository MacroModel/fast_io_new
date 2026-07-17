#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_freestanding.h>

namespace
{

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding>
inline constexpr auto decimal_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.precision = precision;
	flags.rounding = rounding;
	flags.showpos = true;
	flags.uppercase = true;
	flags.uppercase_e = true;
	flags.comma = true;
	flags.nan_show_sign = true;
	flags.nan_show_type = true;
	flags.json_float = true;
	return flags;
}();

template <::std::integral char_type, typename flt, auto flags>
bool check_value(flt value, ::std::size_t precision) noexcept
{
	using runtime_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, flt>;
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<flags, flt>;
	static_assert(::fast_io::precise_reserve_printable<char_type, runtime_type>);
	static_assert(!::fast_io::precise_reserve_printable<char_type, scalar_type>);
	// The admitted wide domains have e10max <= 4966 and at most 37 carrier
	// digits.  P<=256 plus sign, point, exponent and JSON decoration therefore
	// remains strictly below this test buffer even for fixed max-finite output.
	static_assert(4966u + 37u + 256u + 16u < 20000u);
	char_type ordinary[20000u];
	char_type precise[20000u];
	runtime_type const manipulator{value, precision};
	auto const exact_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, runtime_type>, manipulator)};
	auto const capacity{::fast_io::print_reserve_size(
		::fast_io::io_reserve_type<char_type, runtime_type>, manipulator)};
	if (capacity < exact_size || sizeof(ordinary) / sizeof(*ordinary) < exact_size)
	{
		return false;
	}
	auto const ordinary_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, runtime_type>, ordinary, manipulator)};
	auto const precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, runtime_type>, precise, exact_size,
		manipulator)};
	if (static_cast<::std::size_t>(ordinary_end - ordinary) != exact_size ||
		static_cast<::std::size_t>(precise_end - precise) != exact_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != exact_size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, typename flt,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::std::size_t size>
bool check_rounding(flt const (&values)[size]) noexcept
{
	for (auto const requested : {::std::size_t{0u}, ::std::size_t{1u}, ::std::size_t{17u},
								 ::std::size_t{128u}, ::std::size_t{129u}, ::std::size_t{256u}})
	{
		for (auto const value : values)
		{
			if (!check_value<char_type, flt,
							 decimal_flags<format, precision, rounding>>(value, requested))
			{
				return false;
			}
		}
	}
	return true;
}

template <::std::integral char_type, typename flt,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision,
		  ::std::size_t size>
bool check_roundings(flt const (&values)[size]) noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return check_rounding<char_type, flt, format, precision,
						  rounding::nearest_to_even>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::nearest_to_odd>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::nearest_toward_plus_infinity>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::nearest_toward_minus_infinity>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::nearest_toward_zero>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::nearest_away_from_zero>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::toward_plus_infinity>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::toward_minus_infinity>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::toward_zero>(values) &&
		   check_rounding<char_type, flt, format, precision,
						  rounding::away_from_zero>(values);
}

template <::std::integral char_type, typename flt,
		  ::fast_io::manipulators::floating_format format, ::std::size_t size>
bool check_precision_modes(flt const (&values)[size]) noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	return check_roundings<char_type, flt, format, precision::significant>(values) &&
		   check_roundings<char_type, flt, format, precision::fractional>(values) &&
		   check_roundings<char_type, flt, format,
						   precision::significant_preserve_trailing_zero>(values) &&
		   check_roundings<char_type, flt, format,
						   precision::fractional_preserve_trailing_zero>(values);
}

template <::std::integral char_type, typename flt, ::std::size_t size>
bool check_formats(flt const (&values)[size]) noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	return check_precision_modes<char_type, flt, format::general>(values) &&
		   check_precision_modes<char_type, flt, format::decimal>(values) &&
		   check_precision_modes<char_type, flt, format::fixed>(values) &&
		   check_precision_modes<char_type, flt, format::scientific>(values);
}

template <typename flt, ::std::size_t size>
bool check_characters(flt const (&values)[size]) noexcept
{
	return check_formats<char, flt>(values) &&
		   check_formats<wchar_t, flt>(values) &&
		   check_formats<char8_t, flt>(values) &&
		   check_formats<char16_t, flt>(values) &&
		   check_formats<char32_t, flt>(values);
}

inline constexpr auto precision_sweep_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::scientific;
	flags.precision =
		::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero;
	flags.rounding = ::fast_io::manipulators::floating_rounding::nearest_to_even;
	return flags;
}();

template <::std::integral char_type, typename flt, ::std::size_t size>
bool check_precision_sweep(flt const (&values)[size]) noexcept
{
	for (::std::size_t requested{}; requested <= 129u; ++requested)
	{
		for (auto const value : values)
		{
			if (!check_value<char_type, flt, precision_sweep_flags>(value, requested))
			{
				return false;
			}
		}
	}
	for (auto const value : values)
	{
		if (!check_value<char_type, flt, precision_sweep_flags>(value, 256u))
		{
			return false;
		}
	}
	return true;
}

template <typename flt, ::std::size_t size>
bool check_precision_sweep_characters(flt const (&values)[size]) noexcept
{
	return check_precision_sweep<char, flt>(values) &&
		   check_precision_sweep<wchar_t, flt>(values) &&
		   check_precision_sweep<char8_t, flt>(values) &&
		   check_precision_sweep<char16_t, flt>(values) &&
		   check_precision_sweep<char32_t, flt>(values);
}

template <typename flt>
bool check_binary80_impl() noexcept
{
	if constexpr (::std::numeric_limits<flt>::digits == 64 &&
				  ::std::numeric_limits<flt>::max_exponent == 16384)
	{
		flt const values[]{static_cast<flt>(0.0L), static_cast<flt>(-0.0L),
						   static_cast<flt>(1.25L), static_cast<flt>(-1.25L),
						   static_cast<flt>(9.5L),
						   (::std::numeric_limits<flt>::denorm_min)(),
						   (::std::numeric_limits<flt>::min)(),
						   (::std::numeric_limits<flt>::max)(),
						   (::std::numeric_limits<flt>::infinity)(),
						   (::std::numeric_limits<flt>::quiet_NaN)()};
		flt const sweep_values[]{(::std::numeric_limits<flt>::denorm_min)(),
								 (::std::numeric_limits<flt>::max)()};
		return check_characters(values) &&
			   check_precision_sweep_characters(sweep_values);
	}
	return true;
}

#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
constexpr __float128 binary128(__uint128_t bits) noexcept
{
	return ::std::bit_cast<__float128>(bits);
}

bool check_binary128() noexcept
{
	constexpr __uint128_t sign{static_cast<__uint128_t>(1u) << 127u};
	constexpr __uint128_t one{static_cast<__uint128_t>(16383u) << 112u};
	constexpr __uint128_t maximum{
		(static_cast<__uint128_t>(0x7ffeu) << 112u) |
		((static_cast<__uint128_t>(1u) << 112u) - 1u)};
	constexpr __uint128_t special{static_cast<__uint128_t>(0x7fffu) << 112u};
	__float128 const values[]{binary128(0u), binary128(sign),
							  binary128(one), binary128(sign | one), static_cast<__float128>(9.5L),
							  binary128(1u), binary128(static_cast<__uint128_t>(1u) << 112u),
							  binary128(maximum), binary128(special),
							  binary128(special | (static_cast<__uint128_t>(1u) << 111u))};
	__float128 const sweep_values[]{binary128(1u), binary128(maximum)};
	return check_characters(values) &&
		   check_precision_sweep_characters(sweep_values);
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
	return !(check_binary80_impl<long double>() && check_binary128());
}
