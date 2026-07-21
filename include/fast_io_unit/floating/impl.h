#pragma once

#include "punning.h"
#include "hexfloat.h"
#include "compiler_constant_hex.h"
#include "decfloat.h"
#include "roundtrip.h"
#include "wide_shortest.h"

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
#if defined(FAST_IO_HAS_FLOAT64_TYPE)
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

/*
Binary80 and binary128 precision formatting is implemented by the exact
decimal-expansion backend rather than the binary32/binary64 Dragonbox cache.
Admit only representations for which get_punned_result and the exact backend
have matching field models:

* x87 binary80 has a 63-bit stored fraction and a 15-bit exponent.  Its object
  representation is decoded by the explicitly proved little-endian x87
  overload in punning.h.
* IEEE binary128 has a 112-bit stored fraction and a 15-bit exponent in a
  same-size unsigned carrier.  The same-size condition excludes IBM double-
  double long double even when it has a wider std::numeric_limits precision.

This is a representation capability, not an ISA selection.  In particular it
does not make a wide type enter dragonbox_main, whose runtime power cache and
endpoint arithmetic are intentionally limited to binary32 and binary64.
*/
template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_exact_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_exact_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	inline static constexpr bool has_uint128{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		true
#else
		false
#endif
	};
	inline static constexpr bool binary80{
		has_uint128 &&
		::fast_io::details::fp_floating_point_is_float80<no_cvref_t> &&
		::std::endian::native == ::std::endian::little && trait::mbits == 63u &&
		trait::ebits == 15u &&
		sizeof(mantissa_type) == sizeof(::std::uint_least64_t)};
	inline static constexpr bool binary128{
		has_uint128 && trait::mbits == 112u && trait::ebits == 15u &&
		sizeof(no_cvref_t) == sizeof(mantissa_type) &&
		::std::numeric_limits<mantissa_type>::digits >= 128u};
	inline static constexpr bool value{binary80 || binary128};
};

template <typename T>
inline constexpr bool print_floating_decimal_exact_supported{
	::fast_io::details::print_floating_decimal_exact_supported_impl<T>::value};

// Clang's x86 bfloat16 lowering can rematerialize a nested scalar copy through
// __truncsfbf2 (or a narrowing instruction) and thereby destroy subnormal and
// NaN payload bits.  The upper scalar CPO therefore captures integer IEC 60559
// fields from its own manipulator object before entering a lower formatter.
// With AVX512-BF16 the owning aggregate itself has a bit-exact by-value ABI, so
// only this local field capture is needed.  Without it, even the owning CPO
// parameter must be borrowed.  Every ordinary low-level floating entry remains
// by value.  The capability macro prevents merely naming __bf16 on targets
// where Clang rejects the type.
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename T>
inline constexpr bool print_floating_requires_object_field_capture{
	::std::same_as<::std::remove_cvref_t<T>, __bf16>};

template <typename T>
inline constexpr bool print_floating_decimal_requires_integer_transport{
	::fast_io::details::print_floating_requires_object_field_capture<T>
#if defined(__AVX512BF16__)
	&& false
#endif
};
#else
template <typename T>
inline constexpr bool print_floating_requires_object_field_capture{};

template <typename T>
inline constexpr bool print_floating_decimal_requires_integer_transport{};
#endif

/*
The representation-sensitive x86 Clang bfloat16 domain must become integer
fields before an owning manipulator crosses a second formatter boundary.
Without AVX512-BF16 the public scalar CPO remains borrowed. With AVX512-BF16
the public CPO receives this compact owning carrier by value: its converting
constructor reads the caller's object bits directly, while the lower formatter
never rematerializes a signaling NaN through __truncsfbf2/VCVTNEPS2BF16.
Every other floating domain keeps its native by-value manipulator ABI.
*/
template <typename manipulator, typename flt>
struct floating_precise_field_parameter
{
	::fast_io::details::punning_result<::std::remove_cvref_t<flt>> fields{};
	::std::size_t precision{};

	inline constexpr
	floating_precise_field_parameter(manipulator const &value) noexcept
		: fields{::fast_io::details::compiler_constant_floating_capture_fields<
			  ::std::remove_cvref_t<flt>>(value.reference)}
	{
		if constexpr (requires { value.precision; })
		{
			precision = value.precision;
		}
	}
};

template <typename manipulator, typename flt>
using floating_value_or_field_parameter_t = ::std::conditional_t<
	::fast_io::details::print_floating_requires_object_field_capture<flt>,
	::fast_io::details::floating_precise_field_parameter<manipulator, flt>,
	manipulator>;

/// Decodes the integer-owned narrow floating proxy without ever reconstructing
/// a native bfloat16 object.  The representation layout is the same IEC 60559
/// layout already described by `iec559_traits`.
template <typename floating_type>
[[nodiscard]] inline constexpr auto
floating_scalar_proxy_fields(::std::uint_least16_t representation) noexcept
{
	using clean_type = ::std::remove_cvref_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto mantissa_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::mbits) - 1u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::ebits) - 1u};
	return ::fast_io::details::punning_result<clean_type>{
		static_cast<mantissa_type>(representation & mantissa_mask),
		static_cast<::std::uint_least32_t>(
			(representation >> trait::mbits) & exponent_mask),
		static_cast<bool>(representation >> (trait::mbits + trait::ebits))};
}

template <::fast_io::manipulators::floating_format format>
inline constexpr bool print_floating_format_valid{
	format == ::fast_io::manipulators::floating_format::general ||
	format == ::fast_io::manipulators::floating_format::scientific ||
	format == ::fast_io::manipulators::floating_format::fixed ||
	format == ::fast_io::manipulators::floating_format::decimal ||
	format == ::fast_io::manipulators::floating_format::hexfloat};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool print_floating_precision_valid{
	precision == ::fast_io::manipulators::floating_precision::significant ||
	precision == ::fast_io::manipulators::floating_precision::fractional ||
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero};

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool print_floating_rounding_valid{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::away_from_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::current_environment};

/*
Reserve customization is a public capability query, so unsupported floating
representations and forged enum values must be removed by constraints rather
than diagnosed from a function-body static_assert.  Hexadecimal formatting
requires a punned IEC 60559 field decomposition.  Scalar decimal formatting
further requires the exact binary64 ABI normalization, an exact binary32
widening, or a directly implemented binary32/binary64 shortest domain.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_ordinary_supported{
	flags.base == 10u &&
	::fast_io::details::print_floating_format_valid<flags.floating> &&
	::fast_io::details::print_floating_rounding_valid<flags.rounding> &&
	::fast_io::details::print_floating_has_iec559_traits<flt> &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat ||
	 (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
	  sizeof(::std::remove_cvref_t<flt>) == sizeof(double)) ||
	 ::fast_io::details::print_floating_decimal_via_float<flt> ||
	 ::fast_io::details::print_floating_decimal_direct_supported<flt> ||
	 ::fast_io::details::print_floating_decimal_exact_supported<flt>)};

/*
Scalar decimal formatting promises the shortest round-tripping spelling, while
runtime precision promises rounding on an explicitly requested decimal grid.
The binary80/binary128 scalar branch uses the exact-interval carrier in
wide_shortest.h; it never enters the binary32/binary64 Dragonbox cache.  Keeping
that algorithm boundary explicit lets the public scalar and precision CPOs
advertise the same representation set without conflating their conversion
engines.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_scalar_supported{
	::fast_io::details::print_floating_ordinary_supported<flags, flt>};

template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_precision_supported{
	flags.base == 10u &&
	::fast_io::details::print_floating_format_valid<flags.floating> &&
	::fast_io::details::print_floating_rounding_valid<flags.rounding> &&
	::fast_io::details::print_floating_has_iec559_traits<flt> &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat ||
	 ::fast_io::details::print_floating_ordinary_supported<flags, flt> ||
	 ::fast_io::details::print_floating_decimal_exact_supported<flt>)};


template <::fast_io::manipulators::scalar_flags flags, typename flt>
concept print_floating_staged_supported =
	::fast_io::details::my_floating_point<flt> &&
	(::std::same_as<::std::remove_cvref_t<flt>, float> ||
	 ::std::same_as<::std::remove_cvref_t<flt>, double>) &&
	::fast_io::details::da::staged_supported<::std::remove_cvref_t<flt>> &&
	::fast_io::details::print_floating_ordinary_supported<flags, flt> &&
	flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
	flags.rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even;

#if defined(__SIZEOF_INT128__)
/*
Binary80/binary128 scalar decimal output is deliberately kept outside
print_rsvflt_fields_define_impl: that established entry owns the binary32/64
Dragonbox and DA hot paths.  This representation-gated adapter performs the
same special/sign/zero grammar, obtains the independently proved exact-interval
shortest carrier, and then rejoins the common decimal presentation writer.
Thus enabling the public wide CPO cannot perturb any narrow dynamic path.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt,
	::std::integral char_type>
	requires(::fast_io::details::print_floating_decimal_exact_supported<flt> &&
		flags.floating !=
			::fast_io::manipulators::floating_format::hexfloat)
inline constexpr char_type *print_floating_wide_scalar_fields_define(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	if constexpr (flags.rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::
						toward_plus_infinity>, flt>(
					iter, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::
						toward_minus_infinity>, flt>(
					iter, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::toward_zero>,
				flt>(iter, mantissa, exponent, negative);
		default:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::nearest_to_even>,
				flt>(iter, mantissa, exponent, negative);
		}
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
		if (exponent == exponent_mask)
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(
					iter, mantissa, negative);
		}
		iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
			iter, negative);
		if (mantissa == 0u && exponent == 0u)
		{
			if constexpr (flags.floating ==
				::fast_io::manipulators::floating_format::scientific)
			{
				return ::fast_io::details::prsv_fp_dece0<flags.uppercase>(iter);
			}
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
			if constexpr (flags.json_float)
			{
				return ::fast_io::details::
					print_rsv_fp_append_json_float_zero<flags.comma>(iter);
			}
			return iter;
		}
		auto const decimal{
			::fast_io::details::wide_shortest_from_binary<flt, flags.rounding>(
				mantissa, exponent, negative)};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return ::fast_io::details::print_rsvflt_decimal_define_impl<
			flt, flags.comma, flags.uppercase_e, flags.floating,
			flags.json_float>(iter, decimal.m10, decimal.e10);
	}
}
#endif

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

/// @brief Caps floating conversion staging at the measured scheduler/register envelope.
/// @details `staged_printable` proves that independent values can be prepared before emission; it does not prove that
///          an arbitrarily long prepared-state array is profitable. Floating states carry substantially more live data
///          than the two-lane integer state, so inheriting the generic `SIZE_MAX` policy allowed long packs to create a
///          4-KiB optimization frame, extend every conversion's live range, and spill before serialization. Eight is
///          the largest lane count covered by the current staged floating code-generation/throughput matrix. Larger
///          runs retain the ordinary bounded formatter, preserving output and allocation semantics while avoiding an
///          unaudited register-pressure strategy.
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr ::std::size_t print_staged_max_count(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return 8u;
}

/// @brief Selects the audited staged fallback placement for one floating formatter type.
/// @details The in-caller path is limited to ASCII `char` shortest-decimal format because that is the complete caller
///          measured by the target policy. Wider characters, EBCDIC execution character sets, and other presentation
///          formats retain the conservative cold fallback until separately audited. This customization changes only
///          placement of the ordinary scalar formatter after eligibility fails; it cannot change emitted characters.
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr bool print_staged_fallback_inline(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return ::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type> &&
		   flags.floating == ::fast_io::manipulators::floating_format::decimal &&
		   ::fast_io::details::da::staged_inline_fallback_supported<::std::remove_cvref_t<flt>>;
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
		converted = ::fast_io::details::da::compute_binary32_staged(
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
				converted.has_last_digit, static_cast<bool>(sign)};
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
	if constexpr (::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type>)
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
				::fast_io::details::da::print_ascii_shortest<floating_type, flags, true>(iter, converted)};
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
	requires ::fast_io::details::print_floating_scalar_supported<flags, flt>
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
		/*
		The narrow ASCII DA renderer deliberately uses fixed-width stores.  Its
		binary32 scientific leaf can physically reach

		  sign + one leading-digit offset + nine carrier bytes + eight exponent bytes = 19,

		while the logical binary32 cache is 15.  The corresponding binary64 bound
		is 1 + 1 + 17 + 8 = 27 versus a logical cache of 24.  Decimal and
		scientific therefore need four and three additional writable bytes,
		respectively; general already contributes three logical-policy bytes but
		binary32 still needs one more.  Fixed has a much larger exponent-derived
		bound, and non-char/EBCDIC renderers do not enter this ASCII store contract.

		Both explicit nearest-even and current-environment formatting receive this
		capacity: the latter may dispatch to the former after reading the run-time
		rounding mode.  This value is reserve capacity, not reported output length.
		The precise-reserve CPO continues to use its independent exact size and
		exact-store renderer.  Taking the maximum rather than adding both allowances
		is valid because JSON's two-byte suffix is emitted only for zero, which never
		enters the staged finite-carrier writer.
		*/
		constexpr bool uses_da_ascii_staging{
			::std::same_as<char_type, char> &&
			::fast_io::details::is_ascii<char_type> &&
			(flags.rounding == manipulators::floating_rounding::nearest_to_even ||
			 flags.rounding == manipulators::floating_rounding::current_environment) &&
			flags.floating != manipulators::floating_format::fixed};
		constexpr ::std::size_t da_ascii_staging_extra{
			uses_da_ascii_staging
				? (trait::mbits == 23u && trait::ebits == 8u
					   ? 4u
					   : (trait::mbits == 52u && trait::ebits == 11u ? 3u : 0u))
				: 0u};
		constexpr ::std::size_t decimal_capacity_extra{
			decimal_extra < da_ascii_staging_extra ? da_ascii_staging_extra : decimal_extra};
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<double, flags.floating>,
															  flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<float, flags.floating>,
															  flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else if constexpr (
			::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<
					details::print_rsv_cache<::std::remove_cvref_t<flt>,
						flags.floating>, flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<
					details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>, flags.nan_show_type>,
				decimal_capacity_extra);
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_scalar_supported<flags, flt> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
											 char_type *iter,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_t<flags, flt>, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		auto const fields{f.fields};
		if constexpr (flags.floating ==
			manipulators::floating_format::hexfloat)
		{
			return ::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
				iter, fields);
		}
		else
		{
			return details::print_rsvflt_fields_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
				flags.floating, flags.rounding, flags.nan_show_sign,
				flags.nan_show_type, flags.json_float, floating_type>(
					iter, fields.mantissa, fields.exponent, fields.sign);
		}
	}
	else if constexpr (flags.floating == manipulators::floating_format::hexfloat)
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
			using trait = ::fast_io::details::iec559_traits<floating_type>;
			auto const [mantissa, exponent, sign]{
				::fast_io::details::get_punned_result(f.reference)};
			constexpr auto exponent_mask{
				(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
			if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
			{
				return ::fast_io::details::prsv_fp_nan_impl<
					flags.showpos, flags.uppercase, flags.nan_show_sign,
					flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
			}
			auto const widened{
				::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
					mantissa, exponent, sign)};
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(
				iter, widened);
		}
#if defined(__SIZEOF_INT128__)
		else if constexpr (
			::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			auto const [mantissa, exponent, negative]{
				::fast_io::details::get_punned_result(f.reference)};
			return ::fast_io::details::
				print_floating_wide_scalar_fields_define<flags,
					::std::remove_cvref_t<flt>>(
						iter, mantissa, exponent, negative);
		}
		#endif
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return details::print_rsvflt_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
				flags.floating, flags.rounding, flags.nan_show_sign,
				flags.nan_show_type, flags.json_float>(iter, f.reference);
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
	details::my_floating_point flt>
	requires(::fast_io::details::print_floating_ordinary_supported<flags, flt> &&
			 ::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	char_type *iter, manipulators::scalar_manip_t<flags, flt> const &f) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return ::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
			iter, fields);
	}
	else
	{
		auto const [mantissa, exponent, sign]{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type>(
				iter, mantissa, exponent, sign);
	}
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_t<flags, flt>>) noexcept
{
	return print_reserve_size(
		::fast_io::io_reserve_type<char_type,
			manipulators::scalar_manip_t<flags, flt>>);
}

/// The proxy is already integer-owned, so this lower leaf receives it by value
/// and feeds the established field formatter directly.  No native bfloat16
/// aggregate or floating conversion is formed at this boundary.
template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_field_manip_t<flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_hex_scalar_fields_define<
			flags>(iter, fields);
	}
	else
	{
		return ::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type>(
				iter, fields.mantissa, fields.exponent, fields.sign);
	}
}

namespace details
{

/// @brief Returns the representation-specific base of a runtime-precision floating reserve bound.
/// @details The requested precision and the grammar's final fixed allowance are deliberately excluded. Keeping this
///          constant separate lets ordinary reserve sizing retain its exact historical `(base + precision) + suffix`
///          overflow sequence while speculative concat sizing can perform the same additions non-fatally.
template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
		requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
				 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t print_floating_precision_reserve_base_size() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	::std::size_t base_size{};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
	{
		static_assert(
			::fast_io::details::floating_precision_is_significant<flags.precision> ||
			::fast_io::details::floating_precision_is_fractional<flags.precision>,
			"fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
		using trait = ::fast_io::details::iec559_traits<flt>;
		if constexpr (::std::same_as<no_cvref_t, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<no_cvref_t, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
			{
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<
						flags.showbase,
						typename ::fast_io::details::iec559_traits<no_cvref_t>::mantissa_type>,
					flags.nan_show_type>;
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>,
					flags.nan_show_type>;
			}
			else
#endif
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<
						flags.showbase,
						typename ::fast_io::details::iec559_traits<double>::mantissa_type>,
					flags.nan_show_type>;
		}
		else
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsvhexfloat_size_cache<
					flags.showbase, typename trait::mantissa_type>,
				flags.nan_show_type>;
		}
		return base_size;
	}
	else
	{
		static_assert(
			::fast_io::manipulators::floating_format::general == flags.floating ||
			::fast_io::manipulators::floating_format::scientific == flags.floating ||
			::fast_io::manipulators::floating_format::fixed == flags.floating ||
			::fast_io::manipulators::floating_format::decimal == flags.floating);
		/*
		Fractional general formatting can retain the complete integral part before
		rounding on the 10^-P grid. At P=0 a max-finite binary80 value, for example,
		is a 4933-digit integer even though ordinary general formatting would choose
		scientific notation. Therefore both general and decimal fractional modes use
		the fixed reserve bound. Significant general/decimal use their native cache.
		*/
		constexpr auto reserve_floating{
			(::fast_io::details::floating_precision_is_fractional<flags.precision> &&
			 (flags.floating == ::fast_io::manipulators::floating_format::decimal ||
			  flags.floating == ::fast_io::manipulators::floating_format::general))
				? ::fast_io::manipulators::floating_format::fixed
				: flags.floating};
		if constexpr (::std::same_as<no_cvref_t, long double> &&
					  sizeof(flt) == sizeof(double))
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<double, reserve_floating>,
				flags.nan_show_type>;
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<float, reserve_floating>,
				flags.nan_show_type>;
		}
		else if constexpr (::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<no_cvref_t, reserve_floating>,
				flags.nan_show_type>;
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
				"currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
				"formats are printed through float");
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<no_cvref_t, reserve_floating>,
				flags.nan_show_type>;
		}
		return base_size;
	}
}

template <::fast_io::manipulators::scalar_flags flags>
inline consteval ::std::size_t print_floating_precision_reserve_suffix_size() noexcept
{
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat &&
				  ::fast_io::details::floating_precision_is_fractional<flags.precision>)
	{
		return 9u;
	}
	else
	{
		return 8u;
	}
}

} // namespace details

/// @brief Marks a run-time precision floating scalar as profitable for bounded one-pass materialization.
/// @details Computing this scalar's exact size performs the floating conversion which emission would otherwise repeat.
///          The marker is a source capability only: print and string construction independently decide whether their
///          destination can profit from a contiguous temporary or put area.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Authorizes direct put-area emission for a run-time precision floating scalar.
/// @details A cheap bounded-size query alone says nothing about exceptions or observable output state. Print therefore
///          requires this independent opt-in together with its structural noexcept proof and the destination's
///          deferred-cursor contract.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Returns the scalar's non-fatal upper bound when it fits `maximum_size`, or `SIZE_MAX` otherwise.
/// @details This definition lives with the floating formatter because it depends on representation-specific fixed
///          overhead. It never invokes the ordinary reserve-size CPO, whose overflow policy is termination.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>,
	::fast_io::manipulators::scalar_manip_precision_t<flags, value_type> value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, value_type>()};
	if (maximum_size < base_size || maximum_size - base_size < value.precision)
	{
		return SIZE_MAX;
	}
	auto const precision_size{base_size + value.precision};
	constexpr auto suffix_size{
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>()};
	if (maximum_size - precision_size < suffix_size)
	{
		return SIZE_MAX;
	}
	return precision_size + suffix_size;
}

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating == manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt> f) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating == manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt> f) noexcept
{
	static_assert(::fast_io::details::floating_precision_is_significant<flags.precision> ||
					  ::fast_io::details::floating_precision_is_fractional<flags.precision>,
				  "fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
	if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		auto const fields{f.fields};
		return ::fast_io::details::compiler_constant_hex_precision_fields_runtime_define<
			flags>(iter, fields, f.precision);
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
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
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating != manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt> f) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating != manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating);
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		auto const [mantissa, exponent, sign]{f.fields};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<no_cvref_t>(
				mantissa, exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
	else if constexpr (::std::same_as<no_cvref_t, long double> &&
				  sizeof(flt) == sizeof(double))
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, static_cast<double>(f.reference), f.precision);
	}
	else if constexpr (::std::same_as<no_cvref_t, float>)
	{
		using trait = ::fast_io::details::iec559_traits<float>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(f.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		// Classify from the original binary32 fields.  In particular, an sNaN
		// returns here and is never passed through a floating conversion.
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_binary32_finite_fields_to_binary64(
				static_cast<::std::uint_least32_t>(mantissa), exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(f.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<no_cvref_t>(
				mantissa, exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, widened, f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_exact_supported<flt>)
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, f.reference, f.precision);
	}
	else
	{
		static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
					  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
					  "formats are printed through float");
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, f.reference, f.precision);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
	details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 ::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
		manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	manipulators::scalar_manip_precision_t<flags, flt> const &f) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return ::fast_io::details::compiler_constant_hex_precision_fields_runtime_define<
			flags>(iter, fields, f.precision);
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
				mantissa, exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_precision_supported<flags, flt> &&
		::fast_io::details::print_floating_precision_valid<flags.precision> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(
			base_size, value.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_precision_supported<flags, flt> &&
		::fast_io::details::print_floating_precision_valid<flags.precision> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::
			compiler_constant_hex_precision_fields_runtime_define<flags>(
				iter, fields, value.precision);
	}
	else
	{
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (fields.exponent ==
			static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(
					iter, fields.mantissa, fields.sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<
				floating_type>(fields.mantissa, fields.exponent, fields.sign)};
		return ::fast_io::details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding,
			flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
				iter, widened, value.precision);
	}
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_precision_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_precision_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
		manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			flags, flt>()};
	if (maximum_size < base_size ||
		maximum_size - base_size < value.precision)
	{
		return SIZE_MAX;
	}
	auto const precision_size{base_size + value.precision};
	constexpr auto suffix_size{
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>()};
	if (maximum_size - precision_size < suffix_size)
	{
		return SIZE_MAX;
	}
	return precision_size + suffix_size;
}
} // namespace fast_io

#include "precise_size.h"
#include "compiler_constant.h"
