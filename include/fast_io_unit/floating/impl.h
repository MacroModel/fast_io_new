#pragma once

#include "punning.h"
#include "hexfloat.h"
#include "decfloat.h"
#include "roundtrip.h"

namespace fast_io
{

namespace details
{

template <typename T>
concept print_floating_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::mantissa_type;
};

template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_direct_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_direct_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{
		(trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
		 trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits &&
		 sizeof(no_cvref_t) <= sizeof(float)) ||
		::std::same_as<no_cvref_t, double>
#ifdef __STDCPP_FLOAT32_T__
		|| ::std::same_as<no_cvref_t, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
		|| ::std::same_as<no_cvref_t, _Float64>
#endif
	};
};

template <typename T>
inline constexpr bool print_floating_decimal_direct_supported{
	::fast_io::details::print_floating_decimal_direct_supported_impl<T>::value};

template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_via_float_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_via_float_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{
		!::fast_io::details::print_floating_decimal_direct_supported<no_cvref_t> &&
		trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
		trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits};
};

template <typename T>
inline constexpr bool print_floating_decimal_via_float{
	::fast_io::details::print_floating_decimal_via_float_impl<T>::value};

template <::fast_io::manipulators::scalar_flags flags, typename flt>
concept print_floating_staged_supported =
	::fast_io::details::my_floating_point<flt> &&
	(::std::same_as<::std::remove_cvref_t<flt>, float> ||
	 ::std::same_as<::std::remove_cvref_t<flt>, double>) &&
	::fast_io::details::da::staged_supported<::std::remove_cvref_t<flt>> &&
	flags.base == 10u && flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
	flags.rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even;

} // namespace details

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr auto print_staged_type(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	return ::fast_io::io_type_t<::fast_io::details::da::staged_conversion_result<floating_type>>{};
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr ::std::size_t print_staged_width(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return ::fast_io::details::da::staged_width<::std::remove_cvref_t<flt>>();
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool print_staged_eligible(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	manipulators::scalar_manip_t<flags, flt> const &value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
	auto [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
	(void)sign;
	return ::fast_io::details::da::staged_eligible<floating_type>(
		mantissa, exponent, exponent_mask);
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::details::da::staged_conversion_result<::std::remove_cvref_t<flt>>
print_staged_prepare(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	manipulators::scalar_manip_t<flags, flt> const &value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
	(void)sign;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const significand{static_cast<::std::uint_least64_t>(mantissa) |
						   (static_cast<::std::uint_least64_t>(1u) << trait::mbits)};
	::fast_io::details::da::conversion_result converted;
	if constexpr (sizeof(floating_type) <= sizeof(float))
	{
		converted = ::fast_io::details::da::compute_binary32(
			static_cast<::std::uint_least32_t>(significand),
			static_cast<::std::uint_least32_t>(exponent));
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64(
			significand, static_cast<::std::uint_least32_t>(exponent));
	}
	if constexpr (::fast_io::details::da::staged_prepares_sign<floating_type>)
	{
		return {converted.significand, converted.exponent, converted.last_digit,
				converted.has_last_digit, sign};
	}
	else
	{
		return converted;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_staged_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>, char_type *iter,
	manipulators::scalar_manip_t<flags, flt> const &value,
	::fast_io::details::da::staged_conversion_result<::std::remove_cvref_t<flt>> const &prepared) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	::fast_io::details::da::conversion_result const converted{
		prepared.significand, prepared.exponent, prepared.last_digit, prepared.has_last_digit};
	bool negative;
	if constexpr (::fast_io::details::da::staged_prepares_sign<floating_type>)
	{
		(void)value;
		negative = prepared.negative;
	}
	else
	{
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
		(void)mantissa;
		(void)exponent;
		negative = sign;
	}
	if constexpr (::std::same_as<char_type, char> && !::fast_io::details::is_ebcdic<char_type>)
	{
		if (!::std::is_constant_evaluated())
		{
			if constexpr (flags.showpos)
			{
				*iter = static_cast<char>(negative ? u8'-' : u8'+');
				++iter;
			}
			else
			{
				*iter = static_cast<char>(u8'-');
				iter += static_cast<::std::size_t>(negative);
			}
			auto const result{
				::fast_io::details::da::print_ascii_shortest<floating_type, flags>(iter, converted)};
			if (result != nullptr)
			{
				return result;
			}
			auto const finalized{::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::da::finalize<floating_type>(converted))};
			return ::fast_io::details::print_rsvflt_decimal_define_impl<
				floating_type, flags.comma, flags.uppercase_e, flags.floating, flags.json_float>(
				iter, finalized.m10, finalized.e10);
		}
	}
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(iter, negative);
	auto const finalized{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::da::finalize<floating_type>(converted))};
	return ::fast_io::details::print_rsvflt_decimal_define_impl<
		floating_type, flags.comma, flags.uppercase_e, flags.floating, flags.json_float>(
		iter, finalized.m10, finalized.e10);
}

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<::std::remove_cvref_t<flt>>::mantissa_type>,
					flags.nan_show_type>;
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>, flags.nan_show_type>;
			}
			else
#endif
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<double>::mantissa_type>,
					flags.nan_show_type>;
		}
		else
		{
			return details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>,
				flags.nan_show_type>;
		}
	}
	else
	{
		constexpr ::std::size_t decimal_extra{
			((flags.floating == manipulators::floating_format::general) ? 3u : 0u) +
			((flags.json_float && flags.floating != manipulators::floating_format::scientific) ? 2u : 0u)};
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<double, flags.floating>,
															  flags.nan_show_type>,
				decimal_extra);
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<float, flags.floating>,
															  flags.nan_show_type>,
				decimal_extra);
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<
					details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>, flags.nan_show_type>,
				decimal_extra);
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
												 char_type *iter, manipulators::scalar_manip_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, f.reference);
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float128>(f.reference));
			}
			else
#endif
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<double>(f.reference));
		}
		else
		{
			return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
														  flags.uppercase, flags.uppercase_e, flags.comma,
														  flags.nan_show_sign, flags.nan_show_type>(iter,
																									f.reference);
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(
				iter, static_cast<double>(f.reference));
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(
				iter, static_cast<float>(f.reference));
		}
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(iter, f.reference);
		}
	}
}

#if 0
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>;
			}
			else
#endif
				return details::print_rsvhexfloat_size_cache<flags.showbase,
															 typename details::iec559_traits<double>::mantissa_type>;
		}
		else
		{
			return details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>;
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsv_cache<double, flags.floating>;
		}
		static_assert((::std::same_as<::std::remove_cvref_t<flt>, double> ||
					   ::std::same_as<::std::remove_cvref_t<flt>, float>
#ifdef __STDCPP_FLOAT32_T__
					   || ::std::same_as<::std::remove_cvref_t<flt>, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
					   || ::std::same_as<::std::remove_cvref_t<flt>, _Float64>
#endif
					   ),
					  "currently only support iec559 float32 and float64, sorry");
		return details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
												 char_type *iter, manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float128>(f.reference));
			}
			else
#endif
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<double>(f.reference));
		}
		else
		{
			return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
														  flags.uppercase, flags.uppercase_e, flags.comma,
														  flags.nan_show_sign, flags.nan_show_type>(iter,
																										   f.reference);
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<double>(f.reference));
		}
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert((::std::same_as<::std::remove_cvref_t<flt>, double> ||
						   ::std::same_as<::std::remove_cvref_t<flt>, float>
#ifdef __STDCPP_FLOAT32_T__
						   || ::std::same_as<::std::remove_cvref_t<flt>, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
						   || ::std::same_as<::std::remove_cvref_t<flt>, _Float64>
#endif
						   ),
						  "currently only support iec559 float32 and float64, sorry");
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.nan_show_sign, flags.nan_show_type>(iter,
																											  f.reference);
		}
	}
}
#endif

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10 && flags.floating == manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
				   manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(::fast_io::details::floating_precision_is_significant<flags.precision> ||
					  ::fast_io::details::floating_precision_is_fractional<flags.precision>,
				  "fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
	using trait = ::fast_io::details::iec559_traits<flt>;
	::std::size_t base_size{};
	if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
	)
	{
		if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
		{
			base_size = details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<
					flags.showbase, typename details::iec559_traits<::std::remove_cvref_t<flt>>::mantissa_type>,
				flags.nan_show_type>;
		}
		else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
		{
			base_size = details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>, flags.nan_show_type>;
		}
		else
#endif
			base_size = details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<flags.showbase,
													  typename details::iec559_traits<double>::mantissa_type>,
				flags.nan_show_type>;
	}
	else
	{
		base_size = details::print_rsv_fp_size_with_special_cache<
			details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>,
			flags.nan_show_type>;
	}
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision),
		::fast_io::details::floating_precision_is_fractional<flags.precision> ? 9u : 8u);
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10 && flags.floating == manipulators::floating_format::hexfloat)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter, manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(::fast_io::details::floating_precision_is_significant<flags.precision> ||
					  ::fast_io::details::floating_precision_is_fractional<flags.precision>,
				  "fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
	if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
	)
	{
		if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
		{
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, f.reference, f.precision);
		}
		else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
		{
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<__float128>(f.reference), f.precision);
		}
		else
#endif
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<double>(f.reference), f.precision);
	}
	else
	{
		return details::print_rsvhexfloat_precision_define_impl<
			flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(iter, f.reference, f.precision);
	}
}

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10 && flags.floating != manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
				   manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating);
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	constexpr auto reserve_floating{
		(::fast_io::details::floating_precision_is_fractional<flags.precision> &&
		 flags.floating == manipulators::floating_format::decimal)
			? manipulators::floating_format::fixed
			: flags.floating};
	::std::size_t base_size{};
	if constexpr (::std::same_as<no_cvref_t, long double> &&
				  sizeof(flt) == sizeof(double))
	{
		base_size = details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<double, reserve_floating>,
																  flags.nan_show_type>;
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		base_size = details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<float, reserve_floating>,
																  flags.nan_show_type>;
	}
	else
	{
		static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
					  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
					  "formats are printed through float");
		base_size = details::print_rsv_fp_size_with_special_cache<
			details::print_rsv_cache<no_cvref_t, reserve_floating>, flags.nan_show_type>;
	}
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision), 8u);
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10 && flags.floating != manipulators::floating_format::hexfloat)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter, manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating);
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	if constexpr (::std::same_as<no_cvref_t, long double> &&
				  sizeof(flt) == sizeof(double))
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, static_cast<double>(f.reference), f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, static_cast<float>(f.reference), f.precision);
	}
	else
	{
		static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
					  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
					  "formats are printed through float");
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, f.reference, f.precision);
	}
}
} // namespace fast_io
