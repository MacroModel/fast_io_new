#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_freestanding.h>

namespace
{

using format = ::fast_io::manipulators::floating_format;
using rounding = ::fast_io::manipulators::floating_rounding;

template <format presentation, rounding rounding_policy,
	bool comma = false, bool json = false, bool decorated = false>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = presentation;
	flags.rounding = rounding_policy;
	flags.comma = comma;
	flags.json_float = json;
	flags.showpos = decorated;
	flags.showbase = decorated;
	flags.uppercase_showbase = decorated;
	flags.uppercase = decorated;
	flags.uppercase_e = decorated;
	flags.nan_show_sign = true;
	flags.nan_show_type = decorated;
	return flags;
}();

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
[[nodiscard]] bool check_value(floating_type value) noexcept
{
	using proxy_type =
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	using source_type =
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	static_assert(::fast_io::compiler_constant_static_fragment_printable<
		char_type, proxy_type>);
	static_assert(::fast_io::precise_reserve_printable<char_type, proxy_type>);
	static_assert(
		::fast_io::print_compiler_constant_static_fragments_size(proxy_tag{}) ==
		32u);

	char_type proxy_text[
		::fast_io::details::compiler_constant_floating_scalar_capacity]{};
	char_type precise_text[
		::fast_io::details::compiler_constant_floating_scalar_capacity]{};
	char_type ordinary_text[
		::fast_io::details::compiler_constant_floating_scalar_capacity]{};
	::fast_io::basic_io_scatter_t<char_type> fragments[32u]{};
	char_type *proxy_end{};
	char_type *precise_end{};
	char_type *ordinary_end{};
	::std::size_t precise_size{};
	::fast_io::basic_io_scatter_t<char_type> *fragment_end{};
	{
		auto const proxy{
			::fast_io::details::compiler_constant_floating_scalar_materialize<
				char_type, flags>(value)};
		proxy_end = ::fast_io::print_reserve_define(
			proxy_tag{}, proxy_text, proxy);
		precise_size = ::fast_io::print_reserve_precise_size(
			proxy_tag{}, proxy);
		precise_end = ::fast_io::print_reserve_precise_define(
			proxy_tag{}, precise_text, precise_size, proxy);
		fragment_end =
			::fast_io::print_compiler_constant_static_fragments_define(
				proxy_tag{}, fragments, proxy);
		source_type source{value};
		ordinary_end = ::fast_io::print_reserve_define(
			source_tag{}, ordinary_text, source);
	}

	auto const proxy_size{static_cast<::std::size_t>(proxy_end - proxy_text)};
	auto const ordinary_size{
		static_cast<::std::size_t>(ordinary_end - ordinary_text)};
	if (proxy_size != ordinary_size || precise_size != proxy_size ||
		precise_end != precise_text + precise_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != proxy_size; ++index)
	{
		if (proxy_text[index] != ordinary_text[index] ||
			precise_text[index] != ordinary_text[index])
		{
			return false;
		}
	}

	::std::size_t fragment_size{};
	for (auto *current{fragments}; current != fragment_end; ++current)
	{
		if (current->base == nullptr || current->len == 0u ||
			proxy_size - fragment_size < current->len)
		{
			return false;
		}
		for (::std::size_t index{}; index != current->len; ++index)
		{
			if (current->base[index] != proxy_text[fragment_size + index])
			{
				return false;
			}
		}
		fragment_size += current->len;
	}
	return fragment_size == proxy_size;
}

template <::fast_io::manipulators::floating_precision precision,
	rounding rounding_policy, bool decorated = false>
inline constexpr auto hex_precision_flags = []() constexpr noexcept {
	auto flags{test_flags<format::hexfloat, rounding_policy, decorated,
		false, decorated>};
	flags.precision = precision;
	return flags;
}();

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
[[nodiscard]] bool check_hex_precision_value(
	floating_type value, ::std::size_t precision) noexcept
{
	using proxy_type =
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, floating_type>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	using source_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	static_assert(::fast_io::compiler_constant_static_fragment_printable<
		char_type, proxy_type>);
	static_assert(
		::fast_io::print_compiler_constant_static_fragments_size(proxy_tag{}) ==
		32u);

	char_type proxy_text[128u]{};
	char_type ordinary_text[128u]{};
	::fast_io::basic_io_scatter_t<char_type> fragments[32u]{};
	// The by-value ABI carrier alias is intentionally a nondeduced context;
	// spell out the source type so this test also guards that contract.
	auto const proxy{proxy_type{
		::fast_io::details::
			compiler_constant_floating_capture_fields<floating_type>(value),
		precision}};
	auto const proxy_end{
		::fast_io::print_reserve_define(proxy_tag{}, proxy_text, proxy)};
	auto const fragment_end{
		::fast_io::print_compiler_constant_static_fragments_define(
			proxy_tag{}, fragments, proxy)};
	auto const ordinary_end{::fast_io::print_reserve_define(
		source_tag{}, ordinary_text, source_type{value, precision})};
	auto const proxy_size{static_cast<::std::size_t>(proxy_end - proxy_text)};
	auto const ordinary_size{
		static_cast<::std::size_t>(ordinary_end - ordinary_text)};
	if (proxy_size != ordinary_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != proxy_size; ++index)
	{
		if (proxy_text[index] != ordinary_text[index])
		{
			return false;
		}
	}

	::std::size_t fragment_size{};
	for (auto *current{fragments}; current != fragment_end; ++current)
	{
		if (current->base == nullptr || current->len == 0u ||
			proxy_size - fragment_size < current->len)
		{
			return false;
		}
		for (::std::size_t index{}; index != current->len; ++index)
		{
			if (current->base[index] != proxy_text[fragment_size + index])
			{
				return false;
			}
		}
		fragment_size += current->len;
	}
	return fragment_size == proxy_size;
}

template <::std::integral char_type>
[[nodiscard]] bool check_hex_precision_fragments() noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	return check_hex_precision_value<char_type,
		hex_precision_flags<precision::significant,
			rounding::nearest_to_even>>(1.53125, 3u) &&
		check_hex_precision_value<char_type,
			hex_precision_flags<precision::fractional,
				rounding::nearest_to_odd, true>>(-1.53125, 3u) &&
		check_hex_precision_value<char_type,
			hex_precision_flags<precision::significant_preserve_trailing_zero,
				rounding::away_from_zero, true>>(1.25, 40u) &&
		check_hex_precision_value<char_type,
			hex_precision_flags<precision::fractional_preserve_trailing_zero,
				rounding::toward_minus_infinity, true>>(-0.0, 40u) &&
		check_hex_precision_value<char_type,
			hex_precision_flags<precision::fractional,
				rounding::toward_plus_infinity>>(
			(::std::numeric_limits<double>::denorm_min)(), 0u) &&
		check_hex_precision_value<char_type,
			hex_precision_flags<precision::significant,
				rounding::toward_zero, true>>(
			::fast_io::details::fp_make_nan<double, false, true>(false), 7u);
}

template <rounding rounding_policy>
[[nodiscard]] bool check_hex_precision_rounding() noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	return check_hex_precision_value<char,
		hex_precision_flags<precision::fractional, rounding_policy>>(
		1.53125, 3u) &&
		check_hex_precision_value<char,
			hex_precision_flags<precision::significant, rounding_policy>>(
		-1.53125, 3u);
}

[[nodiscard]] bool check_all_hex_precision_roundings() noexcept
{
	return check_hex_precision_rounding<rounding::nearest_to_even>() &&
		check_hex_precision_rounding<rounding::nearest_to_odd>() &&
		check_hex_precision_rounding<rounding::nearest_toward_plus_infinity>() &&
		check_hex_precision_rounding<rounding::nearest_toward_minus_infinity>() &&
		check_hex_precision_rounding<rounding::nearest_toward_zero>() &&
		check_hex_precision_rounding<rounding::nearest_away_from_zero>() &&
		check_hex_precision_rounding<rounding::toward_plus_infinity>() &&
		check_hex_precision_rounding<rounding::toward_minus_infinity>() &&
		check_hex_precision_rounding<rounding::toward_zero>() &&
		check_hex_precision_rounding<rounding::away_from_zero>();
}

template <::std::integral char_type>
[[nodiscard]] bool check_character_domain() noexcept
{
	return check_value<char_type,
		test_flags<format::decimal, rounding::nearest_to_even>>(1.25) &&
		check_value<char_type,
			test_flags<format::general, rounding::nearest_to_odd, true>>(
			-0.0000125) &&
		check_value<char_type,
			test_flags<format::fixed,
				rounding::nearest_toward_plus_infinity, false, true>>(12.5) &&
		check_value<char_type,
			test_flags<format::scientific,
				rounding::nearest_toward_minus_infinity, true, false, true>>(
			-1250.0) &&
		check_value<char_type,
			test_flags<format::hexfloat, rounding::toward_zero,
				false, false, true>>(1.25) &&
		check_value<char_type,
			test_flags<format::decimal, rounding::away_from_zero,
				false, false, true>>(
			::fast_io::details::fp_make_infinity<double>(false)) &&
		check_value<char_type,
			test_flags<format::decimal, rounding::nearest_to_even,
				false, false, true>>(
			::fast_io::details::fp_make_nan<double, true>(true)) &&
		check_value<char_type,
			test_flags<format::decimal, rounding::nearest_to_even,
				false, false, true>>(
			::fast_io::details::fp_make_nan<double, false, true>(false));
}

template <::std::integral char_type>
[[nodiscard]] bool check_fixed_character_domain() noexcept
{
	return check_value<char_type,
		test_flags<format::fixed, rounding::nearest_to_even>>(
		(::std::numeric_limits<double>::denorm_min)()) &&
		check_value<char_type,
			test_flags<format::fixed, rounding::away_from_zero,
				true, true, true>>((::std::numeric_limits<double>::max)());
}

template <rounding rounding_policy>
[[nodiscard]] bool check_rounding() noexcept
{
	return check_value<char,
		test_flags<format::decimal, rounding_policy>>(
		1.2345678901234567) &&
		check_value<char,
			test_flags<format::general, rounding_policy>>(
			-9.9999999999999982e-7);
}

[[nodiscard]] bool check_all_roundings() noexcept
{
	return check_rounding<rounding::nearest_to_even>() &&
		check_rounding<rounding::nearest_to_odd>() &&
		check_rounding<rounding::nearest_toward_plus_infinity>() &&
		check_rounding<rounding::nearest_toward_minus_infinity>() &&
		check_rounding<rounding::nearest_toward_zero>() &&
		check_rounding<rounding::nearest_away_from_zero>() &&
		check_rounding<rounding::toward_plus_infinity>() &&
		check_rounding<rounding::toward_minus_infinity>() &&
		check_rounding<rounding::toward_zero>() &&
		check_rounding<rounding::away_from_zero>();
}

template <typename floating_type>
[[nodiscard]] bool check_representation() noexcept
{
	if constexpr (::fast_io::details::compiler_constant_floating_type_supported<
		floating_type>)
	{
		return check_value<char,
			test_flags<format::decimal, rounding::nearest_to_even>>(
			static_cast<floating_type>(1.25L)) &&
			check_value<char16_t,
				test_flags<format::scientific, rounding::toward_minus_infinity,
					true, false, true>>(
				static_cast<floating_type>(-0.03125L)) &&
			check_value<wchar_t,
				test_flags<format::hexfloat, rounding::nearest_to_odd,
					false, false, true>>(
				static_cast<floating_type>(1.25L));
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_fixed_representation() noexcept
{
	if constexpr (::fast_io::details::compiler_constant_floating_type_supported<
		floating_type>)
	{
		return check_value<char,
			test_flags<format::fixed, rounding::nearest_to_even>>(
			(::std::numeric_limits<floating_type>::denorm_min)()) &&
			check_value<char16_t,
				test_flags<format::fixed, rounding::toward_minus_infinity,
					true, true, true>>(
				(::std::numeric_limits<floating_type>::max)());
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_hex_precision_representation() noexcept
{
	if constexpr (::fast_io::details::compiler_constant_floating_hex_type_supported<
		floating_type>)
	{
		using precision = ::fast_io::manipulators::floating_precision;
		return check_hex_precision_value<char,
			hex_precision_flags<precision::significant,
				rounding::nearest_to_even>>(
			static_cast<floating_type>(1.25L), 7u) &&
			check_hex_precision_value<char16_t,
				hex_precision_flags<precision::fractional_preserve_trailing_zero,
					rounding::toward_minus_infinity, true>>(
				static_cast<floating_type>(-0.03125L), 17u);
	}
	return true;
}

} // namespace

int main()
{
	if (!check_character_domain<char>() ||
		!check_character_domain<wchar_t>() ||
		!check_character_domain<char8_t>() ||
		!check_character_domain<char16_t>() ||
		!check_character_domain<char32_t>() || !check_all_roundings() ||
		!check_fixed_character_domain<char>() ||
		!check_fixed_character_domain<wchar_t>() ||
		!check_fixed_character_domain<char8_t>() ||
		!check_fixed_character_domain<char16_t>() ||
		!check_fixed_character_domain<char32_t>() ||
		!check_hex_precision_fragments<char>() ||
		!check_hex_precision_fragments<wchar_t>() ||
		!check_hex_precision_fragments<char8_t>() ||
		!check_hex_precision_fragments<char16_t>() ||
		!check_hex_precision_fragments<char32_t>() ||
		!check_all_hex_precision_roundings() ||
		!check_representation<float>() || !check_representation<double>() ||
		!check_representation<long double>() ||
		!check_fixed_representation<float>() ||
		!check_fixed_representation<double>() ||
		!check_fixed_representation<long double>() ||
		!check_hex_precision_representation<float>() ||
		!check_hex_precision_representation<double>() ||
		!check_hex_precision_representation<long double>())
	{
		return 1;
	}
#if defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
	if (!check_representation<_Float16>())
	{
		return 2;
	}
	if (!check_hex_precision_representation<_Float16>())
	{
		return 5;
	}
	if (!check_fixed_representation<_Float16>())
	{
		return 8;
	}
#endif
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__)
	if (!check_representation<__bf16>())
	{
		return 3;
	}
	if (!check_hex_precision_representation<__bf16>())
	{
		return 6;
	}
	if (!check_fixed_representation<__bf16>())
	{
		return 9;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (!check_representation<__float128>())
	{
		return 4;
	}
	if (!check_hex_precision_representation<__float128>())
	{
		return 7;
	}
	if (!check_fixed_representation<__float128>())
	{
		return 10;
	}
#endif
	return 0;
}
