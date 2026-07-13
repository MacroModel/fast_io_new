#pragma once


namespace fast_io
{

using to_chars_result = ::std::to_chars_result;

namespace details
{

template <::std::size_t base, ::std::unsigned_integral U>
inline constexpr ::fast_io::to_chars_result to_chars_integral_checked(char *first, char *last, U value,
																	  bool negative) noexcept
{
	::std::size_t const digits{::fast_io::details::chars_len<base, false>(value)};
	::std::size_t const length{digits + static_cast<::std::size_t>(negative)};
	if (static_cast<::std::size_t>(last - first) < length) [[unlikely]]
	{
		return {last, ::std::errc::value_too_large};
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char>;
	}
	if constexpr (base == 10u)
	{
		if (value < 10u)
		{
			*first = ::fast_io::char_literal_add<char>(value);
			return {first + 1u, {}};
		}
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
template <::std::unsigned_integral U>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr ::fast_io::to_chars_result to_chars_integral_decimal(char *first, char *last, U value,
																	  bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, 10u>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<10u>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char>;
	}
	if constexpr (!::fast_io::details::is_ebcdic<char> && sizeof(U) == sizeof(::std::uint_least64_t))
	{
		if (!::std::is_constant_evaluated())
		{
			return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
						first, static_cast<::std::uint_least64_t>(value)),
					{}};
		}
	}
	return ::fast_io::details::jeaiii::jeaiii_main<false, false, char, ::fast_io::to_chars_result>(first, value);
}
#endif

template <::std::size_t base, ::std::unsigned_integral U>
inline constexpr ::fast_io::to_chars_result to_chars_integral_fixed_base(char *first, char *last, U value,
																		 bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, base>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<base>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char>;
	}
	if constexpr (base == 10u && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
	{
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (!::fast_io::details::is_ebcdic<char> && sizeof(U) == sizeof(::std::uint_least64_t))
		{
			if (!::std::is_constant_evaluated())
			{
				return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
							first, static_cast<::std::uint_least64_t>(value)),
						{}};
			}
		}
#endif
		return ::fast_io::details::jeaiii::jeaiii_main<false, false, char, ::fast_io::to_chars_result>(first, value);
	}
	else if constexpr (base == 2u || base == 4u || base == 8u || base == 16u || base == 32u)
	{
		return ::fast_io::details::print_reserve_power_of_two_main<base, false, char,
																   ::fast_io::to_chars_result>(first, value);
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

template <::std::unsigned_integral U>
inline constexpr ::fast_io::to_chars_result to_chars_integral_runtime_base(char *first, char *last, U value,
																		   bool negative, int base) noexcept
{
	switch (base)
	{
	case 3:
		return ::fast_io::details::to_chars_integral_fixed_base<3u>(first, last, value, negative);
	case 5:
		return ::fast_io::details::to_chars_integral_fixed_base<5u>(first, last, value, negative);
	case 6:
		return ::fast_io::details::to_chars_integral_fixed_base<6u>(first, last, value, negative);
	case 7:
		return ::fast_io::details::to_chars_integral_fixed_base<7u>(first, last, value, negative);
	case 9:
		return ::fast_io::details::to_chars_integral_fixed_base<9u>(first, last, value, negative);
	case 11:
		return ::fast_io::details::to_chars_integral_fixed_base<11u>(first, last, value, negative);
	case 12:
		return ::fast_io::details::to_chars_integral_fixed_base<12u>(first, last, value, negative);
	case 13:
		return ::fast_io::details::to_chars_integral_fixed_base<13u>(first, last, value, negative);
	case 14:
		return ::fast_io::details::to_chars_integral_fixed_base<14u>(first, last, value, negative);
	case 15:
		return ::fast_io::details::to_chars_integral_fixed_base<15u>(first, last, value, negative);
	case 17:
		return ::fast_io::details::to_chars_integral_fixed_base<17u>(first, last, value, negative);
	case 18:
		return ::fast_io::details::to_chars_integral_fixed_base<18u>(first, last, value, negative);
	case 19:
		return ::fast_io::details::to_chars_integral_fixed_base<19u>(first, last, value, negative);
	case 20:
		return ::fast_io::details::to_chars_integral_fixed_base<20u>(first, last, value, negative);
	case 21:
		return ::fast_io::details::to_chars_integral_fixed_base<21u>(first, last, value, negative);
	case 22:
		return ::fast_io::details::to_chars_integral_fixed_base<22u>(first, last, value, negative);
	case 23:
		return ::fast_io::details::to_chars_integral_fixed_base<23u>(first, last, value, negative);
	case 24:
		return ::fast_io::details::to_chars_integral_fixed_base<24u>(first, last, value, negative);
	case 25:
		return ::fast_io::details::to_chars_integral_fixed_base<25u>(first, last, value, negative);
	case 26:
		return ::fast_io::details::to_chars_integral_fixed_base<26u>(first, last, value, negative);
	case 27:
		return ::fast_io::details::to_chars_integral_fixed_base<27u>(first, last, value, negative);
	case 28:
		return ::fast_io::details::to_chars_integral_fixed_base<28u>(first, last, value, negative);
	case 29:
		return ::fast_io::details::to_chars_integral_fixed_base<29u>(first, last, value, negative);
	case 30:
		return ::fast_io::details::to_chars_integral_fixed_base<30u>(first, last, value, negative);
	case 31:
		return ::fast_io::details::to_chars_integral_fixed_base<31u>(first, last, value, negative);
	case 33:
		return ::fast_io::details::to_chars_integral_fixed_base<33u>(first, last, value, negative);
	case 34:
		return ::fast_io::details::to_chars_integral_fixed_base<34u>(first, last, value, negative);
	case 35:
		return ::fast_io::details::to_chars_integral_fixed_base<35u>(first, last, value, negative);
	case 36:
		return ::fast_io::details::to_chars_integral_fixed_base<36u>(first, last, value, negative);
	[[unlikely]] default:
		::fast_io::fast_terminate();
	}
}

#if defined(__x86_64__) || defined(_M_X64)
template <::std::size_t base>
inline constexpr ::fast_io::to_chars_result to_chars_integral_two_digits_table(char *first, char *last,
																			   unsigned value, bool negative) noexcept
{
	if (static_cast<::std::size_t>(last - first) < 2u + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return {last, ::std::errc::value_too_large};
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char>;
	}
	constexpr auto const *tb{::fast_io::details::digits_table<char, base, false>};
	::std::size_t const index{static_cast<::std::size_t>(value) << 1u};
	first[0] = tb[index];
	first[1] = tb[index + 1u];
	return {first + 2u, {}};
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr ::fast_io::to_chars_result to_chars_integral_runtime_base_two_digits(char *first, char *last,
																					  unsigned value, bool negative,
																					  int base) noexcept
{
	switch (base)
	{
	case 2:
		return ::fast_io::details::to_chars_integral_two_digits_table<2u>(first, last, value, negative);
	case 3:
		return ::fast_io::details::to_chars_integral_two_digits_table<3u>(first, last, value, negative);
	case 4:
		return ::fast_io::details::to_chars_integral_two_digits_table<4u>(first, last, value, negative);
	case 5:
		return ::fast_io::details::to_chars_integral_two_digits_table<5u>(first, last, value, negative);
	case 6:
		return ::fast_io::details::to_chars_integral_two_digits_table<6u>(first, last, value, negative);
	case 7:
		return ::fast_io::details::to_chars_integral_two_digits_table<7u>(first, last, value, negative);
	case 8:
		return ::fast_io::details::to_chars_integral_two_digits_table<8u>(first, last, value, negative);
	case 9:
		return ::fast_io::details::to_chars_integral_two_digits_table<9u>(first, last, value, negative);
	case 10:
		return ::fast_io::details::to_chars_integral_two_digits_table<10u>(first, last, value, negative);
	case 11:
		return ::fast_io::details::to_chars_integral_two_digits_table<11u>(first, last, value, negative);
	case 12:
		return ::fast_io::details::to_chars_integral_two_digits_table<12u>(first, last, value, negative);
	case 13:
		return ::fast_io::details::to_chars_integral_two_digits_table<13u>(first, last, value, negative);
	case 14:
		return ::fast_io::details::to_chars_integral_two_digits_table<14u>(first, last, value, negative);
	case 15:
		return ::fast_io::details::to_chars_integral_two_digits_table<15u>(first, last, value, negative);
	case 16:
		return ::fast_io::details::to_chars_integral_two_digits_table<16u>(first, last, value, negative);
	case 17:
		return ::fast_io::details::to_chars_integral_two_digits_table<17u>(first, last, value, negative);
	case 18:
		return ::fast_io::details::to_chars_integral_two_digits_table<18u>(first, last, value, negative);
	case 19:
		return ::fast_io::details::to_chars_integral_two_digits_table<19u>(first, last, value, negative);
	case 20:
		return ::fast_io::details::to_chars_integral_two_digits_table<20u>(first, last, value, negative);
	case 21:
		return ::fast_io::details::to_chars_integral_two_digits_table<21u>(first, last, value, negative);
	case 22:
		return ::fast_io::details::to_chars_integral_two_digits_table<22u>(first, last, value, negative);
	case 23:
		return ::fast_io::details::to_chars_integral_two_digits_table<23u>(first, last, value, negative);
	case 24:
		return ::fast_io::details::to_chars_integral_two_digits_table<24u>(first, last, value, negative);
	case 25:
		return ::fast_io::details::to_chars_integral_two_digits_table<25u>(first, last, value, negative);
	case 26:
		return ::fast_io::details::to_chars_integral_two_digits_table<26u>(first, last, value, negative);
	case 27:
		return ::fast_io::details::to_chars_integral_two_digits_table<27u>(first, last, value, negative);
	case 28:
		return ::fast_io::details::to_chars_integral_two_digits_table<28u>(first, last, value, negative);
	case 29:
		return ::fast_io::details::to_chars_integral_two_digits_table<29u>(first, last, value, negative);
	case 30:
		return ::fast_io::details::to_chars_integral_two_digits_table<30u>(first, last, value, negative);
	case 31:
		return ::fast_io::details::to_chars_integral_two_digits_table<31u>(first, last, value, negative);
	case 32:
		return ::fast_io::details::to_chars_integral_two_digits_table<32u>(first, last, value, negative);
	case 33:
		return ::fast_io::details::to_chars_integral_two_digits_table<33u>(first, last, value, negative);
	case 34:
		return ::fast_io::details::to_chars_integral_two_digits_table<34u>(first, last, value, negative);
	case 35:
		return ::fast_io::details::to_chars_integral_two_digits_table<35u>(first, last, value, negative);
	case 36:
		return ::fast_io::details::to_chars_integral_two_digits_table<36u>(first, last, value, negative);
	[[unlikely]] default:
		::fast_io::fast_terminate();
	}
}

#endif

} // namespace details

template <::std::integral T>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
inline constexpr ::fast_io::to_chars_result to_chars(char *first, char *last, T value, int base = 10) noexcept
{
#if __has_cpp_attribute(assume)
	[[assume(2 <= base && base <= 36)]];
#endif
	using unsigned_type = ::std::make_unsigned_t<T>;
	bool negative{};
	unsigned_type magnitude{static_cast<unsigned_type>(value)};
	if constexpr (::std::signed_integral<T>)
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
			*first++ = ::fast_io::char_literal_v<u8'-', char>;
		}
		return ::fast_io::details::jeaiii::jeaiii_main<false, false, char, ::fast_io::to_chars_result>(
			first, magnitude);
#endif
	}

#if defined(__x86_64__) || defined(_M_X64)
	auto const short_base{static_cast<unsigned>(base)};
	auto const short_base_square{short_base * short_base};
	if (magnitude < static_cast<unsigned_type>(short_base))
	{
		if (static_cast<::std::size_t>(last - first) < 1u + static_cast<::std::size_t>(negative)) [[unlikely]]
		{
			return {last, ::std::errc::value_too_large};
		}
		if (negative)
		{
			*first++ = ::fast_io::char_literal_v<u8'-', char>;
		}
		*first = ::fast_io::details::charliteralofnumber<char, false>(static_cast<char8_t>(magnitude));
		return {first + 1u, {}};
	}
	if (magnitude < short_base_square)
	{
		return ::fast_io::details::to_chars_integral_runtime_base_two_digits(first, last,
																			 static_cast<unsigned>(magnitude),
																			 negative, base);
	}
#endif

	if ((base & (base - 1)) == 0)
	{
		if (base <= 4)
		{
			if (base == 2)
			{
				return ::fast_io::details::to_chars_integral_fixed_base<2u>(first, last, magnitude, negative);
			}
			return ::fast_io::details::to_chars_integral_fixed_base<4u>(first, last, magnitude, negative);
		}
		if (base == 8)
		{
			constexpr ::std::size_t maximum_octal_digits{
				::fast_io::details::cal_max_int_size<unsigned_type, 8u>()};
			if (static_cast<::std::size_t>(last - first) <
				maximum_octal_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
			{
				return ::fast_io::details::to_chars_integral_fixed_base<8u>(first, last, magnitude, negative);
			}
			if (negative)
			{
				*first++ = ::fast_io::char_literal_v<u8'-', char>;
			}
			return ::fast_io::details::print_reserve_power_of_two_main<8u, false, char,
																	   ::fast_io::to_chars_result>(first, magnitude);
		}
		if (base == 16)
		{
			constexpr ::std::size_t maximum_hexadecimal_digits{
				::fast_io::details::cal_max_int_size<unsigned_type, 16u>()};
			if (static_cast<::std::size_t>(last - first) <
				maximum_hexadecimal_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
			{
				return ::fast_io::details::to_chars_integral_fixed_base<16u>(first, last, magnitude, negative);
			}
			if (negative)
			{
				*first++ = ::fast_io::char_literal_v<u8'-', char>;
			}
			return ::fast_io::details::print_reserve_power_of_two_main<16u, false, char,
																	   ::fast_io::to_chars_result>(first, magnitude);
		}
		return ::fast_io::details::to_chars_integral_fixed_base<32u>(first, last, magnitude, negative);
	}
	return ::fast_io::details::to_chars_integral_runtime_base(first, last, magnitude, negative, base);
}

inline ::fast_io::to_chars_result to_chars(char *, char *, bool, int = 10) = delete;

} // namespace fast_io
