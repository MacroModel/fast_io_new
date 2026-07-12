#pragma once

#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)

namespace fast_io::details::jeaiii
{

using champagne_lemire_i64x8 [[__gnu__::__vector_size__(64)]] = long long;
using champagne_lemire_i8x64 [[__gnu__::__vector_size__(64)]] = char;
using champagne_lemire_i8x16 [[__gnu__::__vector_size__(16)]] = char;

inline constexpr ::std::size_t champagne_lemire_medium_digits(::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t powers[]{
		100000000u,
		1000000000u,
		static_cast<::std::uint_least64_t>(10000000000u),
		static_cast<::std::uint_least64_t>(100000000000u),
		static_cast<::std::uint_least64_t>(1000000000000u),
		static_cast<::std::uint_least64_t>(10000000000000u),
		static_cast<::std::uint_least64_t>(100000000000000u),
		static_cast<::std::uint_least64_t>(1000000000000000u),
		static_cast<::std::uint_least64_t>(100000000u) * static_cast<::std::uint_least64_t>(100000000u)};
	::std::size_t const bit_length{64u - static_cast<::std::size_t>(::std::countl_zero(value))};
	::std::size_t const estimate{(bit_length * 1233u) >> 12u};
	return estimate + (value >= powers[estimate - 8u]);
}

inline champagne_lemire_i8x16 champagne_lemire_16_digits(::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(100000000u)};
	::std::uint_least64_t const high{value / divisor};
	::std::uint_least64_t const low{value % divisor};
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
	champagne_lemire_i64x8 const tens{10, 10, 10, 10, 10, 10, 10, 10};
	champagne_lemire_i64x8 const zeroes{'0', '0', '0', '0', '0', '0', '0', '0'};
#if defined(__clang__)
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512(zeroes, tens, high_remainders)};
	champagne_lemire_i64x8 const low_digits{__builtin_ia32_vpmadd52huq512(zeroes, tens, low_remainders)};
#else
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes, tens, high_remainders, static_cast<unsigned char>(-1))};
	champagne_lemire_i64x8 const low_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes, tens, low_remainders, static_cast<unsigned char>(-1))};
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

inline char *champagne_lemire_main(char *iter, ::std::uint_least64_t value) noexcept
{
	if (value < static_cast<::std::uint_least64_t>(100000000u))
	{
		if (value < 100u)
		{
			return jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(value));
		}
		if (value < 10000u)
		{
			return jeaiii_range4(iter, static_cast<::std::uint_least32_t>(value));
		}
		if (value < 1000000u)
		{
			return jeaiii_range6(iter, static_cast<::std::uint_least32_t>(value));
		}
		return jeaiii_range8(iter, static_cast<::std::uint_least32_t>(value));
	}
	if (value < static_cast<::std::uint_least64_t>(10000000000000000u))
	{
		::std::size_t const length{champagne_lemire_medium_digits(value)};
		champagne_lemire_i8x16 const digits{champagne_lemire_16_digits(value)};
		if (length == 16u)
		{
			__builtin_memcpy(iter, &digits, sizeof(digits));
		}
		else
		{
			unsigned short const mask{static_cast<unsigned short>(0xffffu << (16u - length))};
			::std::uintptr_t const address{reinterpret_cast<::std::uintptr_t>(iter) + length - 16u};
#if defined(__clang__)
			__builtin_ia32_storedquqi128_mask(reinterpret_cast<champagne_lemire_i8x16 *>(address), digits, mask);
#else
			__builtin_ia32_storedquqi128_mask(reinterpret_cast<char *>(address), digits, mask);
#endif
		}
		return iter + length;
	}
	constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(100000000u) *
											static_cast<::std::uint_least64_t>(100000000u)};
	::std::uint_least64_t const high{value / divisor};
	::std::uint_least64_t const low{value % divisor};
	char *const low_iter{high < 100u ? jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(high))
									 : jeaiii_range4(iter, static_cast<::std::uint_least32_t>(high))};
	champagne_lemire_i8x16 const digits{champagne_lemire_16_digits(low)};
	__builtin_memcpy(low_iter, &digits, sizeof(digits));
	return low_iter + 16u;
}

} // namespace fast_io::details::jeaiii

#endif
