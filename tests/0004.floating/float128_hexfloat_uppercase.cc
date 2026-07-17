#include <fast_io_freestanding.h>
#include <fast_io_unit/floating.h>

#include <array>
#include <bit>
#include <cstddef>
#include <string_view>

namespace
{

/*
This test constructs binary128 values from their IEEE 754 interchange bits so
that the hexadecimal fraction length is independent of decimal parsing.  It is
enabled only when the compiler exposes both a native 16-byte __float128 and a
16-byte unsigned integer carrier.  Other targets still compile the translation
unit and return success without naming either extension type.
*/
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)

inline constexpr bool has_native_binary128{true};

template <::std::integral char_type, typename manipulator_type>
bool rendering_equals(manipulator_type manipulator,
					  ::std::u8string_view expected)
{
	char_type buffer[128u];
	auto const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type_t<char_type, manipulator_type>{}, buffer,
		manipulator)};
	if (static_cast<::std::size_t>(end - buffer) != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		if (buffer[index] != ::fast_io::char_literal<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

constexpr __uint128_t binary128_bits(bool negative,
									 __uint128_t fraction) noexcept
{
	constexpr __uint128_t fraction_mask{(static_cast<__uint128_t>(1u) << 112u) -
										1u};
	auto bits{(static_cast<__uint128_t>(16383u) << 112u) |
			  (fraction & fraction_mask)};
	if (negative)
	{
		bits |= static_cast<__uint128_t>(1u) << 127u;
	}
	return bits;
}

constexpr __float128 binary128(bool negative, __uint128_t fraction) noexcept
{
	return ::std::bit_cast<__float128>(binary128_bits(negative, fraction));
}

template <::std::integral char_type>
bool test_character_type()
{
	struct boundary_case
	{
		__uint128_t fraction;
		::std::u8string_view lowercase;
		::std::u8string_view uppercase;
	};

	/*
	The 15- and 16-digit cases end on opposite sides of the 64-bit limb
	boundary.  The first 17-digit case crosses it with alphabetic digits in both
	limbs; the second proves that the low fixed-width limb preserves its leading
	zeroes.  The 28-digit case exercises the complete binary128 fraction.
	*/
	constexpr ::std::array cases{
		boundary_case{static_cast<__uint128_t>(0xabcdef123456789ull) << 52u,
					  u8"1.abcdef123456789p+0", u8"1.ABCDEF123456789P+0"},
		boundary_case{static_cast<__uint128_t>(0xabcdef123456789aull) << 48u,
					  u8"1.abcdef123456789ap+0", u8"1.ABCDEF123456789AP+0"},
		boundary_case{
			((static_cast<__uint128_t>(0xau) << 64u) | 0xbcdef123456789abull)
				<< 44u,
			u8"1.abcdef123456789abp+0", u8"1.ABCDEF123456789ABP+0"},
		boundary_case{((static_cast<__uint128_t>(0xau) << 64u) | 0xbcull) << 44u,
					  u8"1.a00000000000000bcp+0", u8"1.A00000000000000BCP+0"},
		boundary_case{(static_cast<__uint128_t>(0xabcdef012345ull) << 64u) |
						  0x6789abcdef012345ull,
					  u8"1.abcdef0123456789abcdef012345p+0",
					  u8"1.ABCDEF0123456789ABCDEF012345P+0"}};

	using namespace ::fast_io::mnp;
	for (auto const &item : cases)
	{
		auto const value{binary128(false, item.fraction)};
		if (!rendering_equals<char_type>(hexfloat(value), item.lowercase) ||
			!rendering_equals<char_type>(hexfloat<true>(value), item.uppercase))
		{
			return false;
		}
	}

	auto const negative{binary128(true, cases.back().fraction)};
	if (!rendering_equals<char_type>(hexfloat0x<true>(negative),
									 u8"-0X1.ABCDEF0123456789ABCDEF012345P+0") ||
		!rendering_equals<char_type>(comma_hexfloat<true>(negative),
									 u8"-1,ABCDEF0123456789ABCDEF012345P+0"))
	{
		return false;
	}

	constexpr ::fast_io::manipulators::scalar_flags
		uppercase_showpos_hexfloat_flags{
			.showpos = true,
			.uppercase = true,
			.uppercase_e = true,
			.floating = ::fast_io::manipulators::floating_format::hexfloat};
	auto const positive{binary128(false, cases.back().fraction)};
	return rendering_equals<char_type>(
		::fast_io::manipulators::scalar_manip_t<uppercase_showpos_hexfloat_flags,
												__float128>{positive},
		u8"+1.ABCDEF0123456789ABCDEF012345P+0");
}

#else

inline constexpr bool has_native_binary128{false};

#endif

} // namespace

int main()
{
	if constexpr (has_native_binary128)
	{
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16 &&     \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
		return !(test_character_type<char>() && test_character_type<wchar_t>() &&
				 test_character_type<char8_t>() &&
				 test_character_type<char16_t>() &&
				 test_character_type<char32_t>());
#endif
	}
	return 0;
}
