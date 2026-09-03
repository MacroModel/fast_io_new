#include <cstddef>
#include <cstdint>

#include <fast_io.h>

namespace
{

consteval auto make_flags(
	::fast_io::manipulators::floating_format format, bool comma = false,
	bool json = false, bool showpos = false, bool uppercase = false)
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.comma = comma;
	flags.json_float = json;
	flags.showpos = showpos;
	flags.uppercase = uppercase;
	flags.uppercase_e = uppercase;
	return flags;
}

inline constexpr auto decimal_flags{
	make_flags(::fast_io::manipulators::floating_format::decimal)};
inline constexpr auto fixed_flags{
	make_flags(::fast_io::manipulators::floating_format::fixed, false, false,
			   true)};
inline constexpr auto scientific_flags{
	make_flags(::fast_io::manipulators::floating_format::scientific, false,
			   false, false, true)};
inline constexpr auto general_flags{
	make_flags(::fast_io::manipulators::floating_format::general, true)};
inline constexpr auto json_flags{
	make_flags(::fast_io::manipulators::floating_format::decimal, true, true)};
inline ::std::uint_least64_t volatile precise_size_audit_sink{};

template <auto flags, ::std::integral char_type, typename floating_type>
[[nodiscard]] bool check_fields(
	typename ::fast_io::details::iec559_traits<
		floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	constexpr auto reserve_size{
		::fast_io::details::print_floating_exact_decimal_reserve_size<
			floating_type, flags.floating, flags.json_float>()};
	char_type buffer[reserve_size + 1u];
	buffer[reserve_size] = static_cast<char_type>(0x5au);
	auto const precise_size{
		::fast_io::details::floating_precise_exact_decimal_fields_size<
			flags, floating_type>(mantissa, exponent, negative)};
	auto *const end{
		::fast_io::details::print_floating_exact_decimal_fields_define<
			flags, floating_type>(buffer, mantissa, exponent, negative)};
	if (end != buffer + precise_size ||
		buffer[reserve_size] != static_cast<char_type>(0x5au))
	{
		return false;
	}
	auto hash{static_cast<::std::uint_least64_t>(precise_size)};
	if (precise_size)
	{
		hash ^= static_cast<::std::uint_least64_t>(buffer[0u]) << 8u;
		hash ^= static_cast<::std::uint_least64_t>(
					buffer[precise_size / 2u])
				<< 24u;
		hash ^= static_cast<::std::uint_least64_t>(
					buffer[precise_size - 1u])
				<< 40u;
	}
	precise_size_audit_sink = precise_size_audit_sink ^ hash;
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_every_finite_exponent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	constexpr auto mantissa_mask{
		static_cast<mantissa_type>(
			(static_cast<mantissa_type>(1u) << trait::mbits) - 1u)};
	for (::std::uint_least32_t exponent{}; exponent != exponent_mask;
		 ++exponent)
	{
		mantissa_type mantissas[]{
			static_cast<mantissa_type>(1u),
			static_cast<mantissa_type>(
				(mantissa_mask / static_cast<mantissa_type>(3u)) |
				static_cast<mantissa_type>(1u)),
			mantissa_mask};
		for (unsigned variant{}; variant != 3u; ++variant)
		{
			auto const mantissa{mantissas[variant]};
			auto const negative{variant == 2u};
			if (!check_fields<decimal_flags, char, floating_type>(
					mantissa, exponent, negative) ||
				!check_fields<fixed_flags, wchar_t, floating_type>(
					mantissa, exponent, negative) ||
				!check_fields<scientific_flags, char8_t, floating_type>(
					mantissa, exponent, negative) ||
				!check_fields<general_flags, char16_t, floating_type>(
					mantissa, exponent, negative) ||
				!check_fields<json_flags, char32_t, floating_type>(
					mantissa, exponent, negative))
			{
				return false;
			}
		}
	}
	return check_fields<decimal_flags, char32_t, floating_type>(
			   0u, 0u, false) &&
		   check_fields<scientific_flags, wchar_t, floating_type>(
			   0u, exponent_mask, false) &&
		   check_fields<json_flags, char8_t, floating_type>(
			   1u, exponent_mask, true);
}

} // namespace

int main()
{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	if (!check_every_finite_exponent<__bf16>())
	{
		return 1;
	}
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	if (!check_every_finite_exponent<_Float16>())
	{
		return 2;
	}
#endif
	if (!check_every_finite_exponent<float>())
	{
		return 3;
	}
	if (!check_every_finite_exponent<double>())
	{
		return 4;
	}
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	if (!check_every_finite_exponent<long double>())
	{
		return 5;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (!check_every_finite_exponent<__float128>())
	{
		return 6;
	}
#endif
}
