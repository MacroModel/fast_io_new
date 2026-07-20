#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace fast_io::tests::compiler_constant_precision
{

using ::fast_io::manipulators::floating_format;
using ::fast_io::manipulators::floating_precision;
using ::fast_io::manipulators::floating_rounding;

consteval auto make_flags(floating_format format,
						  floating_precision precision,
						  floating_rounding rounding)
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.precision = precision;
	flags.rounding = rounding;
	flags.showpos = true;
	flags.uppercase = true;
	flags.uppercase_e = true;
	flags.nan_show_sign = true;
	flags.nan_show_type = true;
	if (format == floating_format::hexfloat)
	{
		flags.showbase = true;
		flags.uppercase_showbase = true;
		flags.comma = true;
	}
	else
	{
		flags.comma = true;
		flags.json_float = true;
	}
	return flags;
}

struct rounding_sample
{
	template <typename floating_type>
	[[nodiscard]] static consteval floating_type value() noexcept
	{
		// Exactly representable in every supported domain, but both P=3
		// significant and P=3 fractional formatting discard decimal digits.
		// This therefore exercises every deterministic tie/directed policy rather
		// than merely comparing a no-rounding spelling.
		return static_cast<floating_type>(1.53125);
	}
};

enum class edge_kind
{
	positive_zero,
	negative_zero,
	denormal_minimum,
	minimum_normal,
	maximum_finite,
	positive_infinity,
	negative_infinity,
	quiet_nan,
	signaling_nan
};

template <edge_kind kind>
struct edge_value
{
	template <typename floating_type>
	[[nodiscard]] static consteval floating_type value() noexcept
	{
		using clean_type = ::std::remove_cv_t<floating_type>;
		using trait = ::fast_io::details::iec559_traits<clean_type>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto mantissa_mask{static_cast<mantissa_type>(
			(static_cast<mantissa_type>(1u) << trait::mbits) - 1u)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u)};
		::fast_io::details::punning_result<clean_type> fields{};
		if constexpr (kind == edge_kind::negative_zero)
		{
			fields.sign = true;
		}
		else if constexpr (kind == edge_kind::denormal_minimum)
		{
			fields.mantissa = static_cast<mantissa_type>(1u);
		}
		else if constexpr (kind == edge_kind::minimum_normal)
		{
			fields.exponent = 1u;
		}
		else if constexpr (kind == edge_kind::maximum_finite)
		{
			fields.mantissa = mantissa_mask;
			fields.exponent = exponent_mask - 1u;
		}
		else if constexpr (kind == edge_kind::positive_infinity ||
			kind == edge_kind::negative_infinity)
		{
			fields.exponent = exponent_mask;
			fields.sign = kind == edge_kind::negative_infinity;
		}
		else if constexpr (kind == edge_kind::quiet_nan)
		{
			fields.mantissa = static_cast<mantissa_type>(
				static_cast<mantissa_type>(1u) << (trait::mbits - 1u));
			fields.exponent = exponent_mask;
		}
		else if constexpr (kind == edge_kind::signaling_nan)
		{
			fields.mantissa = static_cast<mantissa_type>(1u);
			fields.exponent = exponent_mask;
		}
		return ::fast_io::details::
			compiler_constant_floating_value_from_fields<clean_type>(fields);
	}
};

template <::std::integral char_type, auto flags, typename floating_type,
	typename value_provider, ::std::size_t requested_precision,
	bool expected_public_selection = true>
[[nodiscard]] bool check_constant_value() noexcept
{
	using source_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	constexpr source_type source{
		value_provider::template value<floating_type>(), requested_precision};
	static_assert(::fast_io::compiler_constant_printable<char_type, source_type>);
	static_assert(::fast_io::compiler_constant_pre_normalization_safe<
		char_type, source_type>);
	constexpr bool publicly_selected{
		print_compiler_constant_materialization_eligible(source_tag{}, source)};
	static_assert(publicly_selected == expected_public_selection);
	constexpr auto proxy{
		print_compiler_constant_materialize(source_tag{}, source)};
	using proxy_type = ::std::remove_cv_t<decltype(proxy)>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	static_assert(::fast_io::precise_reserve_printable<char_type, proxy_type>);
	static_assert(::fast_io::nothrow_precise_reserve_printable<
		char_type, proxy_type>);
	static_assert(::fast_io::compiler_constant_static_fragment_printable<
		char_type, proxy_type>);
	static_assert(requires {
		{
			print_compiler_constant_prefer_precise_compact(proxy_tag{})
		} -> ::std::same_as<::std::true_type>;
	});

	char_type ordinary[128u]{};
	char_type replacement[128u]{};
	char_type fragmented[128u]{};
	auto const ordinary_size{print_reserve_precise_size(source_tag{}, source)};
	auto const replacement_size{
		print_reserve_precise_size(proxy_tag{}, proxy)};
	if (ordinary_size != replacement_size || sizeof(ordinary) / sizeof(*ordinary) < ordinary_size)
	{
		return false;
	}
	auto const ordinary_end{print_reserve_precise_define(
		source_tag{}, ordinary, ordinary_size, source)};
	auto const replacement_end{print_reserve_precise_define(
		proxy_tag{}, replacement, replacement_size, proxy)};
	if (ordinary_end != ordinary + ordinary_size ||
		replacement_end != replacement + replacement_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != ordinary_size; ++index)
	{
		if (ordinary[index] != replacement[index])
		{
			return false;
		}
	}

	if constexpr (!publicly_selected)
	{
		// The public source stays on its established formatter when the carrier
		// proof is intentionally conservative.  The manually materialized proxy
		// still verifies the exact integer-fields fallback, but its immutable
		// fragment invariant is not part of that rejected source arm.
		return true;
	}
	::fast_io::basic_io_scatter_t<char_type> scatters[32u]{};
	auto const scatter_end{print_compiler_constant_static_fragments_define(
		proxy_tag{}, scatters, proxy)};
	auto fragment_iter{fragmented};
	for (auto scatter_iter{scatters}; scatter_iter != scatter_end;
		 ++scatter_iter)
	{
		if (scatter_iter->base == nullptr || scatter_iter->len == 0u ||
			static_cast<::std::size_t>(fragmented + 128u - fragment_iter) <
				scatter_iter->len)
		{
			return false;
		}
		for (::std::size_t index{}; index != scatter_iter->len; ++index)
		{
			*fragment_iter++ = scatter_iter->base[index];
		}
	}
	if (fragment_iter != fragmented + ordinary_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != ordinary_size; ++index)
	{
		if (ordinary[index] != fragmented[index])
		{
			return false;
		}
	}
	return true;
}

template <typename floating_type, ::std::integral char_type,
	floating_format format, floating_precision precision>
[[nodiscard]] bool check_roundings() noexcept
{
	return
		check_constant_value<char_type,
			make_flags(format, precision, floating_rounding::nearest_to_even),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision, floating_rounding::nearest_to_odd),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::nearest_toward_plus_infinity),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::nearest_toward_minus_infinity),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::nearest_toward_zero),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::nearest_away_from_zero),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::toward_plus_infinity),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision,
				floating_rounding::toward_minus_infinity),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision, floating_rounding::toward_zero),
			floating_type, rounding_sample, 3u>() &&
		check_constant_value<char_type,
			make_flags(format, precision, floating_rounding::away_from_zero),
			floating_type, rounding_sample, 3u>();
}

template <typename floating_type, ::std::integral char_type,
	floating_format format>
[[nodiscard]] bool check_precision_modes() noexcept
{
	return check_roundings<floating_type, char_type, format,
			floating_precision::significant>() &&
		check_roundings<floating_type, char_type, format,
			floating_precision::fractional>() &&
		check_roundings<floating_type, char_type, format,
			floating_precision::significant_preserve_trailing_zero>() &&
		check_roundings<floating_type, char_type, format,
			floating_precision::fractional_preserve_trailing_zero>();
}

template <typename floating_type, ::std::integral char_type>
[[nodiscard]] bool check_formats() noexcept
{
	return check_precision_modes<floating_type, char_type,
			floating_format::general>() &&
		check_precision_modes<floating_type, char_type,
			floating_format::fixed>() &&
		check_precision_modes<floating_type, char_type,
			floating_format::scientific>() &&
		check_precision_modes<floating_type, char_type,
			floating_format::decimal>() &&
		check_precision_modes<floating_type, char_type,
			floating_format::hexfloat>();
}

template <typename floating_type, ::std::integral char_type, edge_kind kind>
[[nodiscard]] bool check_edge() noexcept
{
	constexpr auto flags{make_flags(
		floating_format::scientific, floating_precision::significant,
		floating_rounding::nearest_to_even)};
	// A shortest decimal carrier is deliberately insufficient for P=6 at the
	// minimum subnormal (for example binary64 needs 4.94066e-324, not 5e-324).
	// Narrow formats retain their complete coefficient or an exact guard/sticky
	// carrier. Binary80/binary128 use the proved bounded 512-bit prefix window;
	// the other formats use their exact precision window. Every edge therefore
	// enters the compiler-constant precision arm.
	constexpr bool expected_selection{true};
	return check_constant_value<char_type, flags, floating_type,
		edge_value<kind>, 6u, expected_selection>();
}

template <typename floating_type, ::std::integral char_type>
[[nodiscard]] bool check_edges_for_character() noexcept
{
	return check_edge<floating_type, char_type,
			edge_kind::positive_zero>() &&
		check_edge<floating_type, char_type,
			edge_kind::negative_zero>() &&
		check_edge<floating_type, char_type,
			edge_kind::denormal_minimum>() &&
		check_edge<floating_type, char_type,
			edge_kind::minimum_normal>() &&
		check_edge<floating_type, char_type,
			edge_kind::maximum_finite>() &&
		check_edge<floating_type, char_type,
			edge_kind::positive_infinity>() &&
		check_edge<floating_type, char_type,
			edge_kind::negative_infinity>() &&
		check_edge<floating_type, char_type,
			edge_kind::quiet_nan>() &&
		check_edge<floating_type, char_type,
			edge_kind::signaling_nan>();
}

template <typename floating_type, ::std::integral char_type,
	floating_format format, floating_precision precision,
	floating_rounding rounding, ::std::size_t requested_precision = 3u>
[[nodiscard]] bool check_bounded_sample() noexcept
{
	return check_constant_value<char_type,
		make_flags(format, precision, rounding), floating_type,
		rounding_sample, requested_precision>();
}

// The full Cartesian product is intentionally not the default translation
// unit.  It creates more than one thousand independent formatter
// instantiations per floating type; measured -O2 builds took 3.5--4.6 minutes
// and 1.4--3.3 GiB per TU on Clang 23/GCC 15.  The bounded gate below covers
// every format, precision mode, deterministic rounding policy, character
// domain and edge category in 25 instantiations.  Sanitizer fuzzing exercises
// the complete cross product.  Defining the opt-in macro retains the exhaustive
// source-level matrix for dedicated compile-pressure runs outside normal CI.
template <typename floating_type>
[[nodiscard]] bool run_bounded() noexcept
{
	return
		// Ten calls cover the ten deterministic rounding policies while cycling
		// over all five formats and all four precision modes.
		check_bounded_sample<floating_type, char,
			floating_format::general, floating_precision::significant,
			floating_rounding::nearest_to_even>() &&
		check_bounded_sample<floating_type, char,
			floating_format::fixed, floating_precision::fractional,
			floating_rounding::nearest_to_odd>() &&
		check_bounded_sample<floating_type, char,
			floating_format::scientific,
			floating_precision::significant_preserve_trailing_zero,
			floating_rounding::nearest_toward_plus_infinity>() &&
		check_bounded_sample<floating_type, char,
			floating_format::decimal,
			floating_precision::fractional_preserve_trailing_zero,
			floating_rounding::nearest_toward_minus_infinity>() &&
		check_bounded_sample<floating_type, char,
			floating_format::hexfloat, floating_precision::significant,
			floating_rounding::nearest_toward_zero>() &&
		check_bounded_sample<floating_type, char,
			floating_format::general, floating_precision::fractional,
			floating_rounding::nearest_away_from_zero>() &&
		check_bounded_sample<floating_type, char,
			floating_format::fixed,
			floating_precision::significant_preserve_trailing_zero,
			floating_rounding::toward_plus_infinity>() &&
		check_bounded_sample<floating_type, char,
			floating_format::scientific,
			floating_precision::fractional_preserve_trailing_zero,
			floating_rounding::toward_minus_infinity>() &&
		check_bounded_sample<floating_type, char,
			floating_format::decimal, floating_precision::significant,
			floating_rounding::toward_zero>() &&
		check_bounded_sample<floating_type, char,
			floating_format::hexfloat, floating_precision::fractional,
			floating_rounding::away_from_zero>() &&

		// One independent spelling per public character domain.  The non-hex
		// flags include JSON-float and comma-radix semantics.
		check_bounded_sample<floating_type, wchar_t,
			floating_format::fixed, floating_precision::fractional,
			floating_rounding::nearest_to_even>() &&
		check_bounded_sample<floating_type, char8_t,
			floating_format::scientific, floating_precision::significant,
			floating_rounding::nearest_to_even>() &&
		check_bounded_sample<floating_type, char16_t,
			floating_format::decimal,
			floating_precision::fractional_preserve_trailing_zero,
			floating_rounding::nearest_to_even>() &&
		check_bounded_sample<floating_type, char32_t,
			floating_format::hexfloat,
			floating_precision::significant_preserve_trailing_zero,
			floating_rounding::nearest_to_even>() &&

		// Exercise both ends of the admitted runtime precision range used by the
		// compiler-constant materializer.
		check_bounded_sample<floating_type, char,
			floating_format::fixed,
			floating_precision::fractional_preserve_trailing_zero,
			floating_rounding::nearest_to_even, 0u>() &&
		check_bounded_sample<floating_type, char32_t,
			floating_format::hexfloat,
			floating_precision::fractional_preserve_trailing_zero,
			floating_rounding::nearest_to_even, 40u>() &&

		// Edge categories are distributed across the five character domains so
		// wide-output special values and original-field sNaN handling stay gated.
		check_edge<floating_type, char,
			edge_kind::positive_zero>() &&
		check_edge<floating_type, wchar_t,
			edge_kind::negative_zero>() &&
		check_edge<floating_type, char8_t,
			edge_kind::denormal_minimum>() &&
		check_edge<floating_type, char16_t,
			edge_kind::minimum_normal>() &&
		check_edge<floating_type, char32_t,
			edge_kind::maximum_finite>() &&
		check_edge<floating_type, char,
			edge_kind::positive_infinity>() &&
		check_edge<floating_type, wchar_t,
			edge_kind::negative_infinity>() &&
		check_edge<floating_type, char16_t,
			edge_kind::quiet_nan>() &&
		check_edge<floating_type, char32_t,
			edge_kind::signaling_nan>();
}

template <typename floating_type>
[[nodiscard]] bool run() noexcept
{
#if defined(FAST_IO_TEST_EXHAUSTIVE_COMPILER_CONSTANT_PRECISION)
	return check_formats<floating_type, char>() &&
		check_formats<floating_type, wchar_t>() &&
		check_formats<floating_type, char8_t>() &&
		check_formats<floating_type, char16_t>() &&
		check_formats<floating_type, char32_t>() &&
		check_edges_for_character<floating_type, char>() &&
		check_edges_for_character<floating_type, wchar_t>() &&
		check_edges_for_character<floating_type, char8_t>() &&
		check_edges_for_character<floating_type, char16_t>() &&
		check_edges_for_character<floating_type, char32_t>();
#else
	return run_bounded<floating_type>();
#endif
}

} // namespace fast_io::tests::compiler_constant_precision
