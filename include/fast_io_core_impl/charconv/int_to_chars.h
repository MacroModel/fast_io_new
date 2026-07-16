#pragma once


namespace fast_io
{

namespace details
{

template <::fast_io::details::character char_type>
struct basic_to_chars_result_impl
{
	char_type *ptr;
	::std::errc ec;
};

} // namespace details

template <::fast_io::details::character char_type>
using basic_to_chars_result = ::std::conditional_t<
	::std::same_as<char_type, char>, ::std::to_chars_result,
	::fast_io::details::basic_to_chars_result_impl<char_type>>;

using to_chars_result = ::fast_io::basic_to_chars_result<char>;

namespace details
{

template <::fast_io::details::character char_type>
inline consteval auto generate_to_chars_runtime_digits() noexcept
{
	::fast_io::freestanding::array<char_type, 36u> table;
	for (char8_t digit{}; digit != 36u; ++digit)
	{
		table[digit] = ::fast_io::details::charliteralofnumber<char_type, false>(digit);
	}
	return table;
}

template <::fast_io::details::character char_type>
inline constexpr auto to_chars_runtime_digits{generate_to_chars_runtime_digits<char_type>()};

template <::fast_io::details::character char_type, unsigned base, unsigned digits>
	requires((base == 2u && digits == 4u) || ((base == 8u || base == 16u) && digits == 2u))
inline consteval auto generate_to_chars_runtime_power_digits() noexcept
{
	constexpr ::std::size_t values{base == 2u ? 16u : base * base};
	::fast_io::freestanding::array<char_type, values * digits> table;
	for (::std::size_t value{}; value != values; ++value)
	{
		::std::size_t temporary{value};
		for (::std::size_t position{digits}; position != 0u; --position)
		{
			table[value * digits + position - 1u] =
				::fast_io::details::charliteralofnumber<char_type, false>(
					static_cast<char8_t>(temporary % base));
			temporary /= base;
		}
	}
	return table;
}

template <::fast_io::details::character char_type, unsigned base, unsigned digits>
inline constexpr auto to_chars_runtime_power_digits{
	::fast_io::details::generate_to_chars_runtime_power_digits<char_type, base, digits>()};

inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_magic{
		0x0000000000000000ULL, 0x5555555555555556ULL, 0x0000000000000000ULL,
		0x999999999999999aULL, 0x5555555555555556ULL, 0x2492492492492493ULL,
		0x0000000000000000ULL, 0xc71c71c71c71c71dULL, 0x999999999999999aULL,
		0x745d1745d1745d18ULL, 0x5555555555555556ULL, 0x3b13b13b13b13b14ULL,
		0x2492492492492493ULL, 0x1111111111111112ULL, 0x0000000000000000ULL,
		0xe1e1e1e1e1e1e1e2ULL, 0xc71c71c71c71c71dULL, 0xaf286bca1af286bdULL,
		0x999999999999999aULL, 0x8618618618618619ULL, 0x745d1745d1745d18ULL,
		0x642c8590b21642c9ULL, 0x5555555555555556ULL, 0x47ae147ae147ae15ULL,
		0x3b13b13b13b13b14ULL, 0x2f684bda12f684beULL, 0x2492492492492493ULL,
		0x1a7b9611a7b9611bULL, 0x1111111111111112ULL, 0x0842108421084211ULL,
		0x0000000000000000ULL, 0xf07c1f07c1f07c20ULL, 0xe1e1e1e1e1e1e1e2ULL,
		0xd41d41d41d41d41eULL, 0xc71c71c71c71c71dULL};

inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_base4_magic{
		0x0000000000000000ULL, 0x948b0fcd6e9e0653ULL, 0x0000000000000000ULL,
		0xa36e2eb1c432ca58ULL, 0x948b0fcd6e9e0653ULL, 0xb4b985cf97efcb1eULL,
		0x0000000000000000ULL, 0x3fa39ab547994db0ULL, 0xa36e2eb1c432ca58ULL,
		0x1e7a02e70c778749ULL, 0x948b0fcd6e9e0653ULL, 0x25b55f2e54c64dadULL,
		0xb4b985cf97efcb1eULL, 0x4b66dc33f6acdf16ULL, 0x0000000000000000ULL,
		0x91bf9a3091ccf10eULL, 0x3fa39ab547994db0ULL, 0x0179a9f4ca8f1cebULL,
		0xa36e2eb1c432ca58ULL, 0x5911016e4bcd44d5ULL, 0x1e7a02e70c778749ULL,
		0xdf9f13166086565bULL, 0x948b0fcd6e9e0653ULL, 0x5798ee2308c39dfaULL,
		0x25b55f2e54c64dadULL, 0xf91bd1b62b9cec8bULL, 0xb4b985cf97efcb1eULL,
		0x7b8813d37e4522d0ULL, 0x4b66dc33f6acdf16ULL, 0x22aa4d5fac2a7594ULL,
		0x0000000000000000ULL, 0xc4b42a833cc986c5ULL, 0x91bf9a3091ccf10eULL,
		0x65c3ceb16ef32218ULL, 0x3fa39ab547994db0ULL};

inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_base2_magic{
		0x0000000000000000ULL, 0xc71c71c71c71c71dULL, 0x0000000000000000ULL,
		0x47ae147ae147ae15ULL, 0xc71c71c71c71c71dULL, 0x4e5e0a72f0539783ULL,
		0x0000000000000000ULL, 0x948b0fcd6e9e0653ULL, 0x47ae147ae147ae15ULL,
		0x0ecf56be69c8fde3ULL, 0xc71c71c71c71c71dULL, 0x83c977ab2bedd28fULL,
		0x4e5e0a72f0539783ULL, 0x23456789abcdf013ULL, 0x0000000000000000ULL,
		0xc5894d10d4985c20ULL, 0x948b0fcd6e9e0653ULL, 0x6b1490aa31a3cfc8ULL,
		0x47ae147ae147ae15ULL, 0x293725bb804a4dcaULL, 0x0ecf56be69c8fde3ULL,
		0xef8bdb389ebacc39ULL, 0xc71c71c71c71c71dULL, 0xa36e2eb1c432ca58ULL,
		0x83c977ab2bedd28fULL, 0x67980e0bf08c7766ULL, 0x4e5e0a72f0539783ULL,
		0x37b48248727447d7ULL, 0x23456789abcdf013ULL, 0x10c8531d0952d8d8ULL,
		0x0000000000000000ULL, 0xe1709a3611655193ULL, 0xc5894d10d4985c20ULL,
		0xabfd7e03c2fa5b89ULL, 0x948b0fcd6e9e0653ULL};

inline constexpr ::std::uint_least64_t
to_chars_runtime_divide_u64(::std::uint_least64_t value, ::std::uint_least64_t magic,
							unsigned shift) noexcept
{
	auto const high{::fast_io::intrinsics::umulh(value, magic)};
	auto const adjusted{((value - high) >> 1u) + high};
	return adjusted >> shift;
}

template <::fast_io::details::my_unsigned_integral U>
inline constexpr ::std::size_t to_chars_runtime_bit_width(U value) noexcept
{
	constexpr ::std::size_t bits{::std::numeric_limits<U>::digits};
#if defined(__SIZEOF_INT128__)
	if constexpr (sizeof(U) == sizeof(__uint128_t))
	{
		auto const high{static_cast<::std::uint_least64_t>(value >> 64u)};
		if (high != 0u)
		{
			return bits - static_cast<::std::size_t>(::std::countl_zero(high));
		}
		auto const low{static_cast<::std::uint_least64_t>(value) |
					   static_cast<::std::uint_least64_t>(1u)};
		return 64u - static_cast<::std::size_t>(::std::countl_zero(low));
	}
	else
#endif
	{
		return bits - static_cast<::std::size_t>(
						  ::std::countl_zero(static_cast<U>(value | static_cast<U>(1u))));
	}
}

template <::std::size_t base, ::fast_io::details::my_unsigned_integral U,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_checked(char_type *first, char_type *last, U value, bool negative) noexcept
{
	::std::size_t const digits{::fast_io::details::chars_len<base, false>(value)};
	::std::size_t const length{digits + static_cast<::std::size_t>(negative)};
	if (static_cast<::std::size_t>(last - first) < length) [[unlikely]]
	{
		return {last, ::std::errc::value_too_large};
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	if constexpr (base == 10u)
	{
		if (value < 10u)
		{
			*first = ::fast_io::char_literal_add<char_type>(value);
			return {first + 1u, {}};
		}
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
template <::fast_io::details::my_unsigned_integral U, ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_decimal(char_type *first, char_type *last, U value, bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, 10u>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<10u>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	if constexpr (sizeof(char_type) == 1u && !::fast_io::details::is_ebcdic<char_type> &&
				  sizeof(U) == sizeof(::std::uint_least64_t))
	{
		if (!::std::is_constant_evaluated())
		{
			return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
						first, static_cast<::std::uint_least64_t>(value)),
					{}};
		}
	}
	return ::fast_io::details::jeaiii::jeaiii_main<
		false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
}
#endif

template <::std::size_t base, ::fast_io::details::my_unsigned_integral U,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_fixed_base(char_type *first, char_type *last, U value, bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, base>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<base>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	if constexpr (base == 10u && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
	{
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (sizeof(char_type) == 1u && !::fast_io::details::is_ebcdic<char_type> &&
					  sizeof(U) == sizeof(::std::uint_least64_t))
		{
			if (!::std::is_constant_evaluated())
			{
				return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
							first, static_cast<::std::uint_least64_t>(value)),
						{}};
			}
		}
#endif
		return ::fast_io::details::jeaiii::jeaiii_main<
			false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
	}
	else if constexpr (base == 2u || base == 4u || base == 8u || base == 16u || base == 32u)
	{
		if constexpr (::fast_io::details::need_seperate_print<U>)
		{
			return {::fast_io::details::print_reserve_integral_withfull_main_impl<
						false, base, false>(first, value),
					{}};
		}
		else
		{
			return ::fast_io::details::print_reserve_power_of_two_main<
				base, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
		}
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

template <::fast_io::details::my_unsigned_integral U, ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_runtime_base_compact(char_type *first, char_type *last, U value,
									   bool negative, unsigned base) noexcept
{
	constexpr auto const *digit_table{::fast_io::details::to_chars_runtime_digits<char_type>.data()};
	::std::size_t const sign_size{static_cast<::std::size_t>(negative)};
	::std::size_t const available{static_cast<::std::size_t>(last - first)};
	if (value < static_cast<U>(base))
	{
		if (available < sign_size + 1u) [[unlikely]]
		{
			return {last, ::std::errc::value_too_large};
		}
		if (negative)
		{
			*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		*first = digit_table[static_cast<::std::size_t>(value)];
		return {first + 1u, {}};
	}

	unsigned shift{};
	switch (base)
	{
	case 2u:
		shift = 1u;
		break;
	case 4u:
		shift = 2u;
		break;
	case 8u:
		shift = 3u;
		break;
	case 16u:
		shift = 4u;
		break;
	case 32u:
		shift = 5u;
		break;
	default:
		break;
	}
	if (shift != 0u)
	{
		::std::size_t const bit_width{::fast_io::details::to_chars_runtime_bit_width(value)};
		::std::size_t const digits{(bit_width - 1u) / shift + 1u};
		::std::size_t const length{digits + sign_size};
		if (available < length) [[unlikely]]
		{
			return {last, ::std::errc::value_too_large};
		}
		char_type *const result{first + length};
		char_type *iter{result};
		U const mask{static_cast<U>(base - 1u)};
		if (base == 2u)
		{
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 2u, 4u>.data()};
			while (value >= static_cast<U>(16u))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & static_cast<U>(15u)) * 4u};
				iter -= 4u;
				::fast_io::details::non_overlapped_copy_n(table + index, 4u, iter);
				value >>= 4u;
			}
		}
		else if (base == 8u)
		{
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 8u, 2u>.data()};
			while (value >= static_cast<U>(64u))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & static_cast<U>(63u)) * 2u};
				iter -= 2u;
				::fast_io::details::non_overlapped_copy_n(table + index, 2u, iter);
				value >>= 6u;
			}
		}
		else if (base == 16u)
		{
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 16u, 2u>.data()};
			while (value >= 256u)
			{
				::std::size_t const index{static_cast<::std::size_t>(value & static_cast<U>(255u)) * 2u};
				iter -= 2u;
				::fast_io::details::non_overlapped_copy_n(table + index, 2u, iter);
				value >>= 8u;
			}
		}
		do
		{
			*--iter = digit_table[static_cast<::std::size_t>(value & mask)];
			value >>= shift;
		} while (value != 0u);
		if (negative)
		{
			*first = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		return {result, {}};
	}

	using working_type = ::std::conditional_t<(sizeof(U) < sizeof(unsigned)), unsigned, U>;
	working_type const divisor{static_cast<working_type>(base)};
	working_type const divisor2{divisor * divisor};
	working_type const divisor3{divisor2 * divisor};
	working_type const divisor4{divisor2 * divisor2};
	working_type temporary{static_cast<working_type>(value)};
	::std::size_t digits{};
	for (;;)
	{
		if (temporary < divisor)
		{
			digits += 1u;
			break;
		}
		if (temporary < divisor2)
		{
			digits += 2u;
			break;
		}
		if (temporary < divisor3)
		{
			digits += 3u;
			break;
		}
		if (temporary < divisor4)
		{
			digits += 4u;
			break;
		}
#if (defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		if constexpr (sizeof(working_type) <= sizeof(::std::uint_least64_t))
		{
			auto const magic{
				::fast_io::details::to_chars_runtime_division_base4_magic[base - 2u]};
			auto const wide_divisor{static_cast<::std::uint_least64_t>(divisor4)};
			unsigned const divider_shift{::std::numeric_limits<::std::uint_least64_t>::digits - 1u -
										 static_cast<unsigned>(::std::countl_zero(wide_divisor))};
			temporary = static_cast<working_type>(::fast_io::details::to_chars_runtime_divide_u64(
				static_cast<::std::uint_least64_t>(temporary), magic, divider_shift));
		}
		else
#endif
		{
			temporary /= divisor4;
		}
		digits += 4u;
	}
	::std::size_t const length{digits + sign_size};
	if (available < length) [[unlikely]]
	{
		return {last, ::std::errc::value_too_large};
	}
	char_type *const result{first + length};
	char_type *iter{result};
#if (defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (sizeof(working_type) <= sizeof(::std::uint_least64_t))
	{
		auto const magic{::fast_io::details::to_chars_runtime_division_magic[base - 2u]};
		auto const pair_magic{
			::fast_io::details::to_chars_runtime_division_base2_magic[base - 2u]};
		unsigned const divider_shift{
			::std::numeric_limits<unsigned>::digits - 1u - static_cast<unsigned>(::std::countl_zero(base))};
		unsigned const pair_divider_shift{
			::std::numeric_limits<unsigned>::digits - 1u -
			static_cast<unsigned>(::std::countl_zero(static_cast<unsigned>(divisor2)))};
		::std::uint_least64_t output_value{static_cast<::std::uint_least64_t>(value)};
		auto const wide_base{static_cast<::std::uint_least64_t>(base)};
		auto const wide_divisor2{static_cast<::std::uint_least64_t>(divisor2)};
		while (output_value >= wide_divisor2)
		{
			auto const pair_quotient{::fast_io::details::to_chars_runtime_divide_u64(
				output_value, pair_magic, pair_divider_shift)};
			auto const pair_remainder{output_value - pair_quotient * wide_divisor2};
			auto const high_digit{::fast_io::details::to_chars_runtime_divide_u64(
				pair_remainder, magic, divider_shift)};
			auto const low_digit{pair_remainder - high_digit * wide_base};
			iter -= 2u;
			iter[0] = digit_table[static_cast<::std::size_t>(high_digit)];
			iter[1] = digit_table[static_cast<::std::size_t>(low_digit)];
			output_value = pair_quotient;
		}
		if (output_value >= wide_base)
		{
			auto const high_digit{::fast_io::details::to_chars_runtime_divide_u64(
				output_value, magic, divider_shift)};
			auto const low_digit{output_value - high_digit * wide_base};
			iter -= 2u;
			iter[0] = digit_table[static_cast<::std::size_t>(high_digit)];
			iter[1] = digit_table[static_cast<::std::size_t>(low_digit)];
		}
		else
		{
			*--iter = digit_table[static_cast<::std::size_t>(output_value)];
		}
	}
	else
#endif
	{
		working_type output_value{static_cast<working_type>(value)};
		do
		{
			working_type const quotient{output_value / divisor};
			auto const remainder{static_cast<::std::size_t>(output_value - quotient * divisor)};
			*--iter = digit_table[remainder];
			output_value = quotient;
		} while (output_value != 0u);
	}
	if (negative)
	{
		*first = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	return {result, {}};
}

} // namespace details

template <::fast_io::details::my_integral T, ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
#if defined(__GNUC__) && !defined(__clang__)
[[gnu::always_inline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value, int base = 10) noexcept
{
#if __has_cpp_attribute(assume)
	[[assume(2 <= base && base <= 36)]];
#endif
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	bool negative{};
	unsigned_type magnitude{static_cast<unsigned_type>(value)};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (value < 0)
		{
			negative = true;
			magnitude = static_cast<unsigned_type>(unsigned_type{} - magnitude);
		}
	}

#if defined(__GNUC__) || defined(__clang__)
	if (__builtin_constant_p(base) && base == 2)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<2u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 3)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<3u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 4)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<4u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 5)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<5u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 6)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<6u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 7)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<7u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 8)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<8u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 9)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<9u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 10)
	{
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (::std::numeric_limits<::std::uint_least32_t>::digits == 32u)
		{
			return ::fast_io::details::to_chars_integral_decimal(first, last, magnitude, negative);
		}
		else
		{
			return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
		}
#else
		return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
#endif
	}
	if (__builtin_constant_p(base) && base == 11)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<11u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 12)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<12u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 13)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<13u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 14)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<14u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 15)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<15u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 16)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<16u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 17)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<17u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 18)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<18u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 19)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<19u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 20)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<20u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 21)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<21u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 22)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<22u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 23)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<23u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 24)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<24u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 25)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<25u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 26)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<26u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 27)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<27u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 28)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<28u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 29)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<29u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 30)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<30u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 31)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<31u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 32)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<32u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 33)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<33u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 34)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<34u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 35)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<35u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 36)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<36u>(first, last, magnitude, negative);
	}
#endif

	if (base == 10) [[likely]]
	{
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (::std::numeric_limits<::std::uint_least32_t>::digits == 32u)
		{
			return ::fast_io::details::to_chars_integral_decimal(first, last, magnitude, negative);
		}
		else
		{
			return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
		}
#else
		constexpr ::std::size_t maximum_decimal_digits{
			::fast_io::details::cal_max_int_size<unsigned_type, 10u>()};
		if (static_cast<::std::size_t>(last - first) <
			maximum_decimal_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
		{
			return ::fast_io::details::to_chars_integral_checked<10u>(first, last, magnitude, negative);
		}
		if (negative)
		{
			*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		return ::fast_io::details::jeaiii::jeaiii_main<
			false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, magnitude);
#endif
	}

	return ::fast_io::details::to_chars_integral_runtime_base_compact(
		first, last, magnitude, negative, static_cast<unsigned>(base));
}

template <::fast_io::details::character char_type>
inline ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *, char_type *, bool, int = 10) = delete;

} // namespace fast_io
