#pragma once

#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)

namespace fast_io::details::jeaiii
{

using champagne_lemire_i64x8 [[__gnu__::__vector_size__(64)]] = long long;
using champagne_lemire_i8x64 [[__gnu__::__vector_size__(64)]] = char;
using champagne_lemire_i8x16 [[__gnu__::__vector_size__(16)]] = char;

#if defined(__GNUC__) && !defined(__clang__)
// Keep GCC's constant broadcasts as memory-source instructions.  Materializing them through GPRs
// otherwise makes GCC spill a dead ZMM value and adds shuffle-port pressure to the hot path.
[[__gnu__::__visibility__("hidden")]]
inline constexpr ::std::uint_least64_t champagne_lemire_ten{10u};
[[__gnu__::__visibility__("hidden")]]
inline constexpr ::std::uint_least64_t champagne_lemire_zero{static_cast<::std::uint_least64_t>(u8'0')};
#endif

// Four IFMA operations set a fixed cost floor, so the scalar path wins for shorter values.
inline constexpr ::std::uint_least64_t champagne_lemire_threshold{
	static_cast<::std::uint_least64_t>(10000000000ull)}; // 11 digits

inline constexpr ::std::uint_least64_t champagne_lemire_power10_table[]{
	static_cast<::std::uint_least64_t>(1u),
	static_cast<::std::uint_least64_t>(10u),
	static_cast<::std::uint_least64_t>(100u),
	static_cast<::std::uint_least64_t>(1000u),
	static_cast<::std::uint_least64_t>(10000u),
	static_cast<::std::uint_least64_t>(100000u),
	static_cast<::std::uint_least64_t>(1000000u),
	static_cast<::std::uint_least64_t>(10000000u),
	static_cast<::std::uint_least64_t>(100000000u),
	static_cast<::std::uint_least64_t>(1000000000u),
	static_cast<::std::uint_least64_t>(10000000000ull),
	static_cast<::std::uint_least64_t>(100000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000ull),
	static_cast<::std::uint_least64_t>(100000000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000000ull),
	static_cast<::std::uint_least64_t>(100000000000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000000000ull)};

inline constexpr ::std::size_t champagne_lemire_digits(::std::uint_least64_t value) noexcept
{
	if (value == 0u)
	{
		return 1u;
	}
	unsigned const bit_width{64u -
							 static_cast<unsigned>(__builtin_clzll(static_cast<unsigned long long>(value)))};
	unsigned const estimate{(bit_width * 1233u) >> 12u};
	return estimate + static_cast<unsigned>(value >= champagne_lemire_power10_table[estimate]);
}

inline champagne_lemire_i8x16 champagne_lemire_16_digits_from_groups(::std::uint_least64_t high,
																	 ::std::uint_least64_t low) noexcept
{
#if __has_cpp_attribute(assume)
	[[assume(high < static_cast<::std::uint_least64_t>(100000000u))]];
	[[assume(low < static_cast<::std::uint_least64_t>(100000000u))]];
#endif
	constexpr ::std::uint_least64_t two52{static_cast<::std::uint_least64_t>(1u) << 52u};
	champagne_lemire_i64x8 const multipliers{
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(1000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(1000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10u))};
	champagne_lemire_i64x8 const high_values{static_cast<long long>(high), static_cast<long long>(high),
											 static_cast<long long>(high), static_cast<long long>(high), static_cast<long long>(high),
											 static_cast<long long>(high), static_cast<long long>(high), static_cast<long long>(high)};
	champagne_lemire_i64x8 const low_values{static_cast<long long>(low), static_cast<long long>(low),
											static_cast<long long>(low), static_cast<long long>(low), static_cast<long long>(low),
											static_cast<long long>(low), static_cast<long long>(low), static_cast<long long>(low)};
#if defined(__clang__)
	champagne_lemire_i64x8 const high_remainders{
		__builtin_ia32_vpmadd52luq512(multipliers, high_values, multipliers)};
	champagne_lemire_i64x8 const low_remainders{
		__builtin_ia32_vpmadd52luq512(multipliers, low_values, multipliers)};
#else
	champagne_lemire_i64x8 const high_remainders{
		__builtin_ia32_vpmadd52luq512_mask(multipliers, high_values, multipliers, static_cast<unsigned char>(-1))};
	champagne_lemire_i64x8 const low_remainders{
		__builtin_ia32_vpmadd52luq512_mask(multipliers, low_values, multipliers, static_cast<unsigned char>(-1))};
#endif
#if defined(__clang__)
	champagne_lemire_i64x8 const tens{10, 10, 10, 10, 10, 10, 10, 10};
	champagne_lemire_i64x8 const zeroes{u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0'};
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512(zeroes, tens, high_remainders)};
	champagne_lemire_i64x8 const low_digits{__builtin_ia32_vpmadd52huq512(zeroes, tens, low_remainders)};
#else
	champagne_lemire_i64x8 tens_for_gcc;
	champagne_lemire_i64x8 zeroes_for_gcc;
	__asm__("vpbroadcastq {%1, %0|%0, %1}" : "=v"(tens_for_gcc) : "m"(champagne_lemire_ten));
	__asm__("vpbroadcastq {%1, %0|%0, %1}" : "=v"(zeroes_for_gcc) : "m"(champagne_lemire_zero));
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes_for_gcc, tens_for_gcc, high_remainders,
										   static_cast<unsigned char>(-1))};
	champagne_lemire_i64x8 const low_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes_for_gcc, tens_for_gcc, low_remainders,
										   static_cast<unsigned char>(-1))};
#endif
	champagne_lemire_i8x64 const indices{0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38,
										 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78};
	champagne_lemire_i8x64 const high_bytes{__builtin_bit_cast(champagne_lemire_i8x64, high_digits)};
	champagne_lemire_i8x64 const low_bytes{__builtin_bit_cast(champagne_lemire_i8x64, low_digits)};
#if defined(__clang__)
	champagne_lemire_i8x64 const digits{__builtin_ia32_vpermi2varqi512(high_bytes, indices, low_bytes)};
#else
	champagne_lemire_i8x64 const digits{__builtin_ia32_vpermt2varqi512_mask(
		indices, high_bytes, low_bytes, static_cast<unsigned long long>(-1))};
#endif
	return __builtin_shufflevector(digits, digits, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline char *champagne_lemire_write_digits(char *iter, champagne_lemire_i8x16 digits,
										   ::std::size_t length) noexcept
{
	if (length == 16u)
	{
		__builtin_memcpy(iter, &digits, sizeof(digits));
		return iter + 16u;
	}
	unsigned short const mask{static_cast<unsigned short>(0xffffu << (16u - length))};
	::std::uintptr_t const address{reinterpret_cast<::std::uintptr_t>(iter) + length - 16u};
#if defined(__clang__)
	__builtin_ia32_storedquqi128_mask(reinterpret_cast<champagne_lemire_i8x16 *>(address), digits, mask);
#else
	__builtin_ia32_storedquqi128_mask(reinterpret_cast<char *>(address), digits, mask);
#endif
	return iter + length;
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline char *champagne_lemire_main(char *iter, ::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(100000000u)};
	if (value >= static_cast<::std::uint_least64_t>(100000000000000u))
	{
		::std::uint_least64_t const quotient{value / divisor};
		::std::uint_least64_t const low{value - quotient * divisor};
		if (value < static_cast<::std::uint_least64_t>(10000000000000000u))
		{
			::std::size_t const length{15u +
									   (value >= static_cast<::std::uint_least64_t>(1000000000000000u))};
			champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(quotient, low)};
			return champagne_lemire_write_digits(iter, digits, length);
		}
		constexpr ::std::uint_least64_t wide_divisor{divisor * divisor};
		::std::uint_least64_t const high{value / wide_divisor};
		::std::uint_least64_t const middle{quotient - high * divisor};
		champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(middle, low)};
		char *const low_iter{high < 100u ? jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(high))
										 : jeaiii_range4(iter, static_cast<::std::uint_least32_t>(high))};
		return champagne_lemire_write_digits(low_iter, digits, 16u);
	}
	if (value < champagne_lemire_threshold)
	{
		return jeaiii_main(iter, value);
	}
	::std::size_t const length{champagne_lemire_digits(value)};
	::std::uint_least64_t const quotient{value / divisor};
	::std::uint_least64_t const low{value - quotient * divisor};
	champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(quotient, low)};
	return champagne_lemire_write_digits(iter, digits, length);
}

} // namespace fast_io::details::jeaiii

#endif
