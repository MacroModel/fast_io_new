#pragma once
#include "sto_generate_base_tb.h"

namespace fast_io
{

namespace details
{

inline constexpr auto generate_sto_ascii_digit_table() noexcept
{
	::fast_io::freestanding::array<char8_t, 256> table;
	for (auto &e : table)
	{
		e = static_cast<char8_t>(0xFFu);
	}
	for (::std::size_t i{}; i != 10u; ++i)
	{
		table.index_unchecked(static_cast<::std::size_t>(u8'0') + i) = static_cast<char8_t>(i);
	}
	for (::std::size_t i{}; i != 26u; ++i)
	{
		auto const digit{static_cast<char8_t>(10u + i)};
		table.index_unchecked(static_cast<::std::size_t>(u8'A') + i) = digit;
		table.index_unchecked(static_cast<::std::size_t>(u8'a') + i) = digit;
	}
	return table;
}

inline constexpr auto sto_ascii_digit_table{::fast_io::details::generate_sto_ascii_digit_table()};

template <::std::integral char_type>
	requires(!::fast_io::details::is_ebcdic<char_type>)
inline constexpr char8_t sto_ascii_digit_table_lookup(my_make_unsigned_t<char_type> ch) noexcept
{
	return ::fast_io::details::sto_ascii_digit_table.index_unchecked(static_cast<::std::size_t>(ch));
}

template <char8_t base, ::std::integral char_type>
	requires(2 <= base && base <= 36)
inline constexpr bool char_digit_to_literal(my_make_unsigned_t<char_type> &ch) noexcept
{
	using unsigned_char_type = my_make_unsigned_t<char_type>;
	constexpr bool ebcdic{::fast_io::details::is_ebcdic<char_type>};
	if constexpr (::std::same_as<char_type, wchar_t> && ::fast_io::details::wide_is_none_utf_endian)
	{
		ch = static_cast<char_type>(::fast_io::byte_swap(static_cast<unsigned_char_type>(ch)));
	}
	if constexpr (base <= 10)
	{
		constexpr unsigned_char_type base_char_type(base);
		if constexpr (ebcdic)
		{
			ch -= static_cast<unsigned_char_type>(240);
		}
		else
		{
			ch -= static_cast<unsigned_char_type>(u8'0');
		}
		return base_char_type <= ch;
	}
	else
	{
		if constexpr (ebcdic)
		{
			if constexpr (base <= 19)
			{
				constexpr unsigned_char_type mns{base - 10};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				ch -= 0xF0;
				if (ch2 < mns)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < mns)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
			else if constexpr (base <= 28)
			{
				constexpr unsigned_char_type mns{base - 19};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				ch -= 0xF0;
				if (ch4 < mns)
				{
					ch = ch4 + static_cast<unsigned_char_type>(19);
				}
				else if (ch5 < mns)
				{
					ch = ch5 + static_cast<unsigned_char_type>(19);
				}
				else if (ch2 < 9)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < 9)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
			else
			{
				constexpr unsigned_char_type mns{base - 28};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				unsigned_char_type ch6(ch);
				ch6 -= 0xE2;
				unsigned_char_type ch7(ch);
				ch7 -= 0xA2;
				ch -= 0xF0;
				if (ch6 < mns)
				{
					ch = ch6 + static_cast<unsigned_char_type>(28);
				}
				else if (ch7 < mns)
				{
					ch = ch7 + static_cast<unsigned_char_type>(28);
				}
				else if (ch4 < 9)
				{
					ch = ch4 + static_cast<unsigned_char_type>(19);
				}
				else if (ch5 < 9)
				{
					ch = ch5 + static_cast<unsigned_char_type>(19);
				}
				else if (ch2 < 9)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < 9)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
		}
		else
		{
			if constexpr (sizeof(char_type) == sizeof(char8_t))
			{
				auto const digit{::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch)};
				ch = static_cast<unsigned_char_type>(digit);
				return base <= digit;
			}
			else if constexpr (base == 16)
			{
				auto const cch{static_cast<char_type>(ch)};
				using family = ::fast_io::char_category::char_category_family;
				if (!::fast_io::char_category::char_category_traits<family::c_xdigit, false>::char_is(cch))
				{
					return true;
				}
				if (::fast_io::char_category::char_category_traits<family::c_digit, false>::char_is(cch))
				{
					ch -= static_cast<unsigned_char_type>(::fast_io::char_literal_v<u8'0', char_type>);
					return false;
				}
				auto const lower{::fast_io::char_category::to_c_lower(cch)};
				ch = static_cast<unsigned_char_type>(lower - ::fast_io::char_literal_v<u8'a', char_type> + 10u);
				return false;
			}

			constexpr unsigned_char_type mns{base - 10};
			unsigned_char_type ch2(ch);
			ch2 -= u8'A';
			unsigned_char_type ch3(ch);
			ch3 -= u8'a';
			ch -= u8'0';
			if (ch2 < mns)
			{
				ch = ch2 + static_cast<unsigned_char_type>(10);
			}
			else if (ch3 < mns)
			{
				ch = ch3 + static_cast<unsigned_char_type>(10);
			}
			else if (10 <= ch)
			{
				return true;
			}
			return false;
		}
	}
}

template <char8_t base, ::std::integral char_type>
	requires(2 <= base && base <= 36)
inline constexpr bool char_is_digit(my_make_unsigned_t<char_type> ch) noexcept
{
	using unsigned_char_type = my_make_unsigned_t<char_type>;
	constexpr bool ebcdic{::fast_io::details::is_ebcdic<char_type>};
	constexpr unsigned_char_type base_char_type(base);
	if constexpr (::std::same_as<char_type, wchar_t> && ::fast_io::details::wide_is_none_utf_endian)
	{
		ch = static_cast<char_type>(::fast_io::byte_swap(static_cast<unsigned_char_type>(ch)));
	}
	if constexpr (base <= 10)
	{
		if constexpr (ebcdic)
		{
			ch -= static_cast<unsigned_char_type>(240);
		}
		else
		{
			ch -= static_cast<unsigned_char_type>(u8'0');
		}
		return ch < base_char_type;
	}
	else
	{
		if constexpr (ebcdic)
		{
			if constexpr (base <= 19)
			{
				constexpr unsigned_char_type mns{base - 10};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				ch -= 0xF0;
				return (ch2 < mns) | (ch3 < mns) | (ch < 10u);
			}
			else if constexpr (base <= 28)
			{
				constexpr unsigned_char_type mns{base - 19};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				ch -= 0xF0;
				return (ch4 < mns) | (ch5 < mns) | (ch2 < 9u) | (ch3 < 9u) | (ch < 10u);
			}
			else
			{
				constexpr unsigned_char_type mns{base - 28};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				unsigned_char_type ch6(ch);
				ch6 -= 0xE2;
				unsigned_char_type ch7(ch);
				ch7 -= 0xA2;
				ch -= 0xF0;
				return (ch6 < mns) | (ch7 < mns) | (ch4 < 9u) | (ch5 < 9u) | (ch2 < 9u) | (ch3 < 9u) | (ch < 10u);
			}
		}
		else
		{
			if constexpr (sizeof(char_type) == sizeof(char8_t))
			{
				return ::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch) < base;
			}
			else if constexpr (base == 16)
			{
				unsigned_char_type digit(ch);
				digit -= u8'0';
				unsigned_char_type alpha(ch);
				alpha |= static_cast<unsigned_char_type>(0x20u);
				alpha -= u8'a';
				return (digit < 10u) | (alpha < 6u);
			}

			constexpr unsigned_char_type mns{base - 10};
			unsigned_char_type ch2(ch);
			ch2 -= u8'A';
			unsigned_char_type ch3(ch);
			ch3 -= u8'a';
			ch -= u8'0';
			return (ch2 < mns) | (ch3 < mns) | (ch < 10u);
		}
	}
}

template <::std::integral char_type>
inline constexpr char_type const *find_none_zero_simd_impl(char_type const *first, char_type const *last) noexcept;

struct simd_parse_result
{
	::std::size_t digits;
	fast_io::parse_code code;
};

inline constexpr char unsigned simd16_shift_table[32]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
													  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 1, 2, 3, 4, 5,
													  6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))

template <bool char_execharset>
inline ::std::uint_least32_t detect_length(char unsigned const *buffer) noexcept
{
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	constexpr char8_t v176_constant{static_cast<char8_t>((zero_constant + static_cast<char8_t>(128)) & 255u)};
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), buffer, 16);
	x86_64_v16qu const v176{v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant};
	x86_64_v16qu const t0{chunk - v176};
	x86_64_v16qs const minus118{-118, -118, -118, -118, -118, -118, -118, -118,
								-118, -118, -118, -118, -118, -118, -118, -118};
	x86_64_v16qs const mask{(x86_64_v16qs)t0 < minus118};
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)mask))};
#else
	__m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buffer));
	__m128i const t0 = _mm_sub_epi8(chunk, _mm_set1_epi8(v176_constant));
	__m128i const mask = _mm_cmplt_epi8(t0, _mm_set1_epi8(-118));
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(_mm_movemask_epi8(mask))};
#endif
	return static_cast<::std::uint_least32_t>(::std::countr_one(v));
}

template <bool char_execharset>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline ::std::size_t sse_skip_overflow_digits(char unsigned const *buffer,
											  char unsigned const *buffer_end) noexcept
{
	auto it{buffer};
	for (; 16 <= buffer_end - it; it += 16)
	{
		auto new_length{detect_length<char_execharset>(it)};
		if (new_length != 16)
		{
			return static_cast<::std::size_t>(it - buffer + new_length);
		}
	};
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	for (; it != buffer_end && static_cast<char unsigned>(*it - zero_constant) < 10u; ++it)
	{
	}
	return static_cast<::std::size_t>(it - buffer);
}

template <bool char_execharset, bool less_than_64_bits>
#if __has_cpp_attribute(__gnu__::__hot__)
[[__gnu__::__hot__]]
#endif
inline simd_parse_result sse_parse(char unsigned const *buffer, char unsigned const *buffer_end,
								   ::std::uint_least64_t &res) noexcept
{
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	constexpr char8_t v176_constant{static_cast<char8_t>((zero_constant + static_cast<char8_t>(128)) & 255u)};
	using fast_io::parse_code;
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), buffer, 16);
	x86_64_v16qu const v176{v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant};
	x86_64_v16qu const t0{chunk - v176};
	x86_64_v16qs const minus118{-118, -118, -118, -118, -118, -118, -118, -118,
								-118, -118, -118, -118, -118, -118, -118, -118};
	x86_64_v16qs const mask{(x86_64_v16qs)t0 < minus118};
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)mask))};
	::std::uint_least32_t digits{static_cast<::std::uint_least32_t>(::std::countr_one(v))};
	if (digits == 0)
	{
		return {0, parse_code::invalid};
	}
	x86_64_v16qu const zeros{zero_constant, zero_constant, zero_constant, zero_constant, zero_constant, zero_constant,
							 zero_constant, zero_constant, zero_constant, zero_constant, zero_constant, zero_constant,
							 zero_constant, zero_constant, zero_constant, zero_constant};
	chunk -= zeros;
	// A full 16-digit chunk already has the required byte order. Avoid the
	// shuffle-table load and PSHUFB on the long-decimal hot path.
	if (digits != 16u) [[unlikely]]
	{
		x86_64_v16qi shuffle_mask;
		__builtin_memcpy(__builtin_addressof(shuffle_mask), simd16_shift_table + digits, sizeof(x86_64_v16qi));
		chunk = (x86_64_v16qu)__builtin_ia32_pshufb128((x86_64_v16qi)chunk, shuffle_mask);
	}
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)chunk, x86_64_v16qi{10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)chunk, x86_64_v8hi{100, 1, 100, 1, 100, 1, 100, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_packusdw128((x86_64_v4si)chunk, (x86_64_v4si)chunk);
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)chunk, x86_64_v8hi{10000, 1, 10000, 1, 0, 0, 0, 0});
	::std::uint_least64_t chunk0;
	__builtin_memcpy(__builtin_addressof(chunk0), __builtin_addressof(chunk), sizeof(chunk0));
#else
	__m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buffer));
	__m128i const t0 = _mm_sub_epi8(chunk, _mm_set1_epi8(v176_constant));
	__m128i const mask = _mm_cmplt_epi8(t0, _mm_set1_epi8(-118));
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(_mm_movemask_epi8(mask))};
	::std::uint_least32_t digits{static_cast<::std::uint_least32_t>(::std::countr_one(v))};
	if (digits == 0)
	{
		return {0, parse_code::invalid};
	}
	chunk = _mm_sub_epi8(chunk, _mm_set1_epi8(zero_constant));
	// A full 16-digit chunk already has the required byte order. Avoid the
	// shuffle-table load and PSHUFB on the long-decimal hot path.
	if (digits != 16u) [[unlikely]]
	{
		chunk = _mm_shuffle_epi8(chunk, _mm_loadu_si128(reinterpret_cast<__m128i const *>(simd16_shift_table + digits)));
	}
	chunk = _mm_maddubs_epi16(chunk, _mm_set_epi8(1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10));
	chunk = _mm_madd_epi16(chunk, _mm_set_epi16(1, 100, 1, 100, 1, 100, 1, 100));
	chunk = _mm_packus_epi32(chunk, chunk);
	chunk = _mm_madd_epi16(chunk, _mm_set_epi16(0, 0, 0, 0, 1, 10000, 1, 10000));
	::std::uint_least64_t chunk0;
	::std::memcpy(__builtin_addressof(chunk0), __builtin_addressof(chunk), sizeof(chunk0));
#endif
	::std::uint_least64_t result{
		static_cast<::std::uint_least64_t>(((chunk0 & 0xffffffff) * static_cast<::std::uint_least64_t>(100000000)) + (chunk0 >> 32))};
	if (digits == 16) [[unlikely]]
	{
		if constexpr (less_than_64_bits)
		{
			//::std::uint_least32_t can never have 16 digits
			return {sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end) + 16,
					parse_code::overflow};
		}
		else
		{
			::std::size_t digits1;
			if (16 <= buffer_end - (buffer + 16))
			{
				digits1 = detect_length<char_execharset>(buffer + 16);
			}
			else
			{
				digits1 = sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end);
			}
			// 18446744073709551615 20 digits
			switch (digits1)
			{
			case 3:
			{
				res = result * UINT16_C(1000) +
					  ((buffer[16] - zero_constant) * UINT16_C(100) + (buffer[17] - zero_constant) * UINT16_C(10) +
					   (buffer[18] - zero_constant));
				return {19, parse_code::ok};
			}
			case 2:
			{
				res = result * UINT16_C(100) + ((buffer[16] - zero_constant) * UINT16_C(10) +
												static_cast<::std::uint_least64_t>(buffer[17] - zero_constant));
				return {18, parse_code::ok};
			}
			case 1:
			{
				res = result * UINT16_C(10) + (buffer[16] - zero_constant);
				return {17, parse_code::ok};
			}
			case 0:
			{
				res = result;
				return {16, parse_code::ok};
			}
			case 4:
			{
				constexpr ::std::uint_least64_t risky_value{UINT_LEAST64_MAX / static_cast<::std::uint_least64_t>(10000)};
				constexpr ::std::uint_fast16_t risky_mod{UINT_LEAST64_MAX % static_cast<::std::uint_least64_t>(10000)};
				if (result > risky_value)
				{
					return {20, parse_code::overflow};
				}
				::std::uint_fast16_t partial{static_cast<::std::uint_fast16_t>(
					static_cast<::std::uint_fast16_t>(buffer[16] - zero_constant) * UINT16_C(1000) +
					static_cast<::std::uint_fast8_t>(buffer[17] - zero_constant) * UINT16_C(100) +
					static_cast<::std::uint_fast8_t>(buffer[18] - zero_constant) * UINT16_C(10) +
					static_cast<::std::uint_fast8_t>(buffer[19] - zero_constant))};
				if (result == risky_value && risky_mod < partial)
				{
					return {20, parse_code::overflow};
				}
				res = result * UINT16_C(10000) + partial;
				return {20, parse_code::ok};
			}
			case 16:
			{
				digits1 = sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end);
				[[fallthrough]];
			}
			default:
			{
				return {digits1 + 16, parse_code::overflow};
			}
			}
		}
	}
	res = result;
	return {digits, parse_code::ok};
}

#endif

template <char8_t base, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr char_type const *skip_digits(char_type const *first, char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	for (; first != last && char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)); ++first)
		;
	return first;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_simd_space_part_check_overflow_impl(char_type const *first, char_type const *last, T &res) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr unsigned_char_type base_char_type{base};
	constexpr unsigned_type risky_uint_max{static_cast<unsigned_type>(-1)};
	constexpr unsigned_type risky_value{risky_uint_max / base};
	constexpr unsigned_char_type risky_digit(risky_uint_max % base);
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};

	bool overflow{};
	if (first != last) [[likely]]
	{
		unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
		if constexpr (isspecialbase)
		{
			if (char_is_digit<base, char_type>(ch))
			{
				++first;
				first = skip_digits<base>(first, last);
				overflow = true;
			}
		}
		else
		{
			if (!char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
			{
				overflow = res > risky_value || (risky_value == res && ch > risky_digit);
				if (!overflow)
				{
					res *= base_char_type;
					res += ch;
				}
				++first;
				if (first != last && char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)))
				{
					++first;
					first = skip_digits<base>(first, last);
					overflow = true;
				}
			}
		}
	}
	return {first, (overflow ? (parse_code::overflow) : (parse_code::ok))};
}

template <char8_t base, my_unsigned_integral T, ::std::size_t n>
inline constexpr ::fast_io::freestanding::array<T, n> generate_pow_table() noexcept
{
	::fast_io::freestanding::array<T, n> tmp;
	T b{1};
	for (auto &e : tmp)
	{
		e = b;
		b *= base;
	}
	return tmp;
}

template <char8_t base, my_unsigned_integral T, ::std::size_t n>
inline constexpr ::fast_io::freestanding::array<T, n> pow_table_n{::fast_io::details::generate_pow_table<base, T, n>()};

template <::std::integral char_type>
	requires(!::fast_io::details::is_ebcdic<char_type> && sizeof(char_type) == sizeof(char8_t))
inline constexpr char8_t ascii_hex_digit_value(my_make_unsigned_t<char_type> ch) noexcept
{
	// Scalar tails contain an unpredictable mix of decimal and alphabetic
	// digits. The shared table avoids a data-dependent branch for that mix.
	return ::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch);
}

inline constexpr ::std::uint_least64_t ascii_hex_word_invalid_mask(::std::uint_least64_t val) noexcept
{
	return (((((val + static_cast<::std::uint_least64_t>(0x4646464646464646)) | (val - static_cast<::std::uint_least64_t>(0x3030303030303030))) &
			  ((val + static_cast<::std::uint_least64_t>(0x3939393939393939)) | (val - static_cast<::std::uint_least64_t>(0x4040404040404040))) &
			  ((val + static_cast<::std::uint_least64_t>(0x1919191919191919)) | (val - static_cast<::std::uint_least64_t>(0x6060606060606060)))) |
			 ~(((val + static_cast<::std::uint_least64_t>(0x3f3f3f3f3f3f3f3f)) | (val - static_cast<::std::uint_least64_t>(0x4040404040404040))) &
			   ((val + static_cast<::std::uint_least64_t>(0x1f1f1f1f1f1f1f1f)) | (val - static_cast<::std::uint_least64_t>(0x6060606060606060))))) &
			static_cast<::std::uint_least64_t>(0x8080808080808080));
}

inline constexpr ::std::uint_least32_t ascii_hex_word_to_u32(::std::uint_least64_t val) noexcept
{
	constexpr ::std::uint_least64_t mask{static_cast<::std::uint_least64_t>(0x000000FF000000FF)};
	constexpr ::std::uint_least64_t mul1{static_cast<::std::uint_least64_t>(0x0100000000000100)};
	constexpr ::std::uint_least64_t mul2{static_cast<::std::uint_least64_t>(0x0001000000000001)};
	val -= static_cast<::std::uint_least64_t>(0x3030303030303030);
	val = (val & static_cast<::std::uint_least64_t>(0x0f0f0f0f0f0f0f0f)) + ((val & static_cast<::std::uint_least64_t>(0x1010101010101010)) >> 4u) * 9u;
	val = (val * 16u) + (val >> 8u);
	return static_cast<::std::uint_least32_t>((((val & mask) * mul1) + (((val >> 16u) & mask) * mul2)) >> 32u);
}

template <::std::integral char_type, my_unsigned_integral T>
	requires(!::fast_io::details::is_ebcdic<char_type> && sizeof(char_type) == sizeof(char8_t))
inline constexpr char_type const *scan_ascii_hex_digits_scalar(char_type const *first, char_type const *last,
															   T &res) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	for (; first != last; ++first)
	{
		auto const digit{
			::fast_io::details::ascii_hex_digit_value<char_type>(static_cast<unsigned_char_type>(*first))};
		if (15u < digit) [[unlikely]]
		{
			break;
		}
		res = static_cast<T>((static_cast<unsigned_type>(res) << 4u) | static_cast<unsigned_type>(digit));
	}
	return first;
}

template <::std::integral char_type, my_unsigned_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_ascii_hex_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, 16>};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};
	if (diff < mn_val)
	{
		mn_val = diff;
	}
	auto first_phase_last{first + mn_val};
	T res{out};
	if constexpr (::std::numeric_limits<::std::uint_least64_t>::digits == 64u && 8u <= max_size)
	{
		while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
		{
			::std::uint_least64_t val;
			::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
			if constexpr (::std::endian::little != ::std::endian::native)
			{
				val = ::fast_io::little_endian(val);
			}
			if (::std::uint_least64_t const invalid_mask{::fast_io::details::ascii_hex_word_invalid_mask(val)};
				invalid_mask != 0) [[unlikely]]
			{
				auto const valid_bytes{
					static_cast<::std::size_t>(static_cast<unsigned>(::std::countr_zero(invalid_mask)) >> 3u)};
				first = ::fast_io::details::scan_ascii_hex_digits_scalar(first, first + valid_bytes, res);
				first_phase_last = first;
				break;
			}
			auto const chunk{::fast_io::details::ascii_hex_word_to_u32(val)};
			if constexpr (sizeof(unsigned_type) <= sizeof(::std::uint_least32_t))
			{
				res = static_cast<T>(chunk);
			}
			else
			{
				res = static_cast<T>((static_cast<unsigned_type>(res) << 32u) |
									 static_cast<unsigned_type>(chunk));
			}
			first += sizeof(::std::uint_least64_t);
		}
	}
	first = ::fast_io::details::scan_ascii_hex_digits_scalar(first, first_phase_last, res);

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}
	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<16, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

#if (defined(__aarch64__) || defined(__arm64__)) && (!defined(_MSC_VER) || defined(__clang__))
template <::std::integral char_type>
	requires(sizeof(char_type) == sizeof(char8_t))
[[gnu::always_inline]] inline bool
aarch64_builtin_parse_16_decimal_digits(char_type const *first,
										::std::uint_least64_t &value) noexcept
{
	using u8x8 [[gnu::vector_size(8)]] = unsigned char;
	using u8x16 [[gnu::vector_size(16)]] = unsigned char;
	using u16x4 [[gnu::vector_size(8)]] = unsigned short;
	using u32x2 [[gnu::vector_size(8)]] = unsigned int;
#if defined(__clang__)
	using i8x8 [[gnu::vector_size(8)]] = signed char;
	using i8x16 [[gnu::vector_size(16)]] = signed char;
	using u16x8 [[gnu::vector_size(16)]] = unsigned short;
	using u32x4 [[gnu::vector_size(16)]] = unsigned int;
	using u64x2 [[gnu::vector_size(16)]] = unsigned long long;
#endif

	u8x16 raw;
#if defined(__clang__)
	raw = __builtin_bit_cast(
		u8x16, __builtin_neon_vld1q_v(reinterpret_cast<unsigned char const *>(first), 48));
#else
	raw = __builtin_aarch64_ld1v16qi_us(
		reinterpret_cast<__builtin_aarch64_simd_qi const *>(first));
#endif
	auto const digits{raw - u8x16{48u, 48u, 48u, 48u, 48u, 48u, 48u, 48u,
								  48u, 48u, 48u, 48u, 48u, 48u, 48u, 48u}};
#if defined(__clang__)
	auto const maximum_digit{static_cast<unsigned char>(__builtin_neon_vmaxvq_u8(digits))};
#else
	auto const maximum_digit{
		static_cast<unsigned char>(__builtin_aarch64_reduc_umax_scal_v16qi_uu(digits))};
#endif
	if (maximum_digit >= 10u) [[unlikely]]
	{
		return false;
	}

	u8x8 const low_digits{__builtin_shufflevector(digits, digits, 0, 1, 2, 3, 4, 5, 6, 7)};
	u8x8 const high_digits{__builtin_shufflevector(digits, digits, 8, 9, 10, 11, 12, 13, 14, 15)};
	u8x8 const pair_weights{10u, 1u, 10u, 1u, 10u, 1u, 10u, 1u};
#if defined(__clang__)
	auto const pair_products_low{__builtin_bit_cast(
		u16x8, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_digits),
									  __builtin_bit_cast(i8x8, pair_weights), 49))};
	auto const pair_products_high{__builtin_bit_cast(
		u16x8, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_digits),
									  __builtin_bit_cast(i8x8, pair_weights), 49))};
	auto const pairs{__builtin_bit_cast(
		u16x8, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, pair_products_low),
									   __builtin_bit_cast(i8x16, pair_products_high), 49))};
#else
	auto const pair_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v8qi_uuu(low_digits, pair_weights)};
	auto const pair_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v8qi_uuu(high_digits, pair_weights)};
	auto const pairs{
		__builtin_aarch64_addpv8hi_uuu(pair_products_low, pair_products_high)};
#endif

	u16x4 const low_pairs{__builtin_shufflevector(pairs, pairs, 0, 1, 2, 3)};
	u16x4 const high_pairs{__builtin_shufflevector(pairs, pairs, 4, 5, 6, 7)};
	u16x4 const quad_weights{100u, 1u, 100u, 1u};
#if defined(__clang__)
	auto const quad_products_low{__builtin_bit_cast(
		u32x4, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_pairs),
									  __builtin_bit_cast(i8x8, quad_weights), 50))};
	auto const quad_products_high{__builtin_bit_cast(
		u32x4, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_pairs),
									  __builtin_bit_cast(i8x8, quad_weights), 50))};
	auto const quads{__builtin_bit_cast(
		u32x4, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, quad_products_low),
									   __builtin_bit_cast(i8x16, quad_products_high), 50))};
#else
	auto const quad_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v4hi_uuu(low_pairs, quad_weights)};
	auto const quad_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v4hi_uuu(high_pairs, quad_weights)};
	auto const quads{
		__builtin_aarch64_addpv4si_uuu(quad_products_low, quad_products_high)};
#endif

	u32x2 const low_quads{__builtin_shufflevector(quads, quads, 0, 1)};
	u32x2 const high_quads{__builtin_shufflevector(quads, quads, 2, 3)};
	u32x2 const octet_weights{10000u, 1u};
#if defined(__clang__)
	auto const octet_products_low{__builtin_bit_cast(
		u64x2, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_quads),
									  __builtin_bit_cast(i8x8, octet_weights), 51))};
	auto const octet_products_high{__builtin_bit_cast(
		u64x2, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_quads),
									  __builtin_bit_cast(i8x8, octet_weights), 51))};
	auto const octets{__builtin_bit_cast(
		u64x2, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, octet_products_low),
									   __builtin_bit_cast(i8x16, octet_products_high), 51))};
#else
	auto const octet_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v2si_uuu(low_quads, octet_weights)};
	auto const octet_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v2si_uuu(high_quads, octet_weights)};
	auto const octets{
		__builtin_aarch64_addpv2di_uuu(octet_products_low, octet_products_high)};
#endif
	value = octets[0] * 100000000u + octets[1];
	return true;
}
#endif

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline parse_result<char_type const *>
runtime_scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr char8_t base_char_type{base};
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, base> - (!isspecialbase)};
	constexpr auto shifter{2 + ::std::bit_width(sizeof(char_type))};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};

	if (diff < mn_val)
	{
		mn_val = diff;
	}

	auto first_phase_last{first + mn_val};
	T res{out};

#if defined(__aarch64__) || defined(_M_ARM64)
	if constexpr (base == 10u && sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
	{
		if (20u <= diff) [[unlikely]]
		{
			auto parse_eight_digits = [](char_type const *digits,
										 ::std::uint_least64_t &value) noexcept {
				::std::uint_least64_t word;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(word), digits, sizeof(word));
				word = ::fast_io::little_endian(word);
				if ((((word + 0x4646464646464646u) | (word - 0x3030303030303030u)) &
					 0x8080808080808080u) != 0u) [[unlikely]]
				{
					return false;
				}
				constexpr ::std::uint_least64_t mask{0x000000FF000000FFu};
				constexpr ::std::uint_least64_t mul1{
					100u + (static_cast<::std::uint_least64_t>(1000000u) << 32u)};
				constexpr ::std::uint_least64_t mul2{
					1u + (static_cast<::std::uint_least64_t>(10000u) << 32u)};
				word -= 0x3030303030303030u;
				word = word * 10u + (word >> 8u);
				value = (((word & mask) * mul1) + (((word >> 16u) & mask) * mul2)) >> 32u;
				return true;
			};
			::std::uint_least64_t high;
			::std::uint_least64_t low;
			if (parse_eight_digits(first, high) && parse_eight_digits(first + 8, low)) [[likely]]
			{
				::std::uint_least32_t word;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(word), first + 16, sizeof(word));
				word = ::fast_io::little_endian(word);
				if ((((word + 0x46464646u) | (word - 0x30303030u)) & 0x80808080u) == 0u) [[likely]]
				{
					word -= 0x30303030u;
					word = word * 10u + (word >> 8u);
					auto const tail{static_cast<::std::uint_least64_t>(
						((word & 0x000000FFu) * 100u) + ((word >> 16u) & 0x000000FFu))};
					auto const high16{high * 100000000u + low};
					auto const next{first + 20};
					if (next != last && char_is_digit<10u, char_type>(
											static_cast<unsigned_char_type>(*next))) [[unlikely]]
					{
						return {skip_digits<10u>(next + 1, last), parse_code::overflow};
					}
					constexpr auto risky_value{static_cast<::std::uint_least64_t>(-1) / 10000u};
					constexpr auto risky_digit{static_cast<::std::uint_least64_t>(-1) % 10000u};
					if (risky_value < high16 || (high16 == risky_value && risky_digit < tail)) [[unlikely]]
					{
						return {next, parse_code::overflow};
					}
					out = static_cast<T>(high16 * 10000u + tail);
					return {next, parse_code::ok};
				}
			}
		}
	}
#endif

	constexpr bool isebcdic{::fast_io::details::is_ebcdic<char_type>};
	if constexpr (!isebcdic && (::std::numeric_limits<::std::uint_least64_t>::digits == 64u))
	{
		if constexpr (sizeof(::std::uint_least32_t) < sizeof(::std::size_t))
		{
			if constexpr (base_char_type <= 10)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least64_t))
					{
						constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_4{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 4>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_6{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 6>};
						constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least64_t)>};


						constexpr ::std::uint_least64_t baseval{0x0101010101010101};
						constexpr ::std::uint_least64_t zero_lower_bound{isebcdic ? baseval * 0xF0 : baseval * 0x30};
						constexpr ::std::uint_least64_t first_bound{0x4646464646464646 + baseval * (10 - base_char_type)};
						constexpr ::std::uint_least64_t mul1{pow_base_sizeof_base_2 + (pow_base_sizeof_base_6 << 32)};
						constexpr ::std::uint_least64_t mul2{1 + (pow_base_sizeof_base_4 << 32)};
						constexpr ::std::uint_least64_t mask{0x000000FF000000FF};
						constexpr ::std::uint_least64_t fullmask{baseval * 0x80};

						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((val + first_bound) | (val - zero_lower_bound)) & fullmask}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{zero_lower_bound};

									all_zero >>= valid_bits;

									val |= all_zero;
									val -= zero_lower_bound;

									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 8>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							val -= zero_lower_bound;
							val = (val * base_char_type) + (val >> 8);
							val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
							res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							first += sizeof(::std::uint_least64_t);
						}
					}

					if constexpr (max_size >= sizeof(::std::uint_least32_t))
					{
						constexpr ::std::uint_least32_t pow_base_sizeof_u32{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least32_t)>};
						constexpr ::std::uint_least32_t first_bound{0x46464646 + 0x01010101 * (10 - base_char_type)};
						constexpr ::std::uint_least32_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, 2>};
						constexpr ::std::uint_least32_t mask{0x000000FF};

						if (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least32_t))
						{
							::std::uint_least32_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least32_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least32_t const cval{((val + first_bound) | (val - 0x30303030)) & 0x80808080}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 32 - valid_bits;

									::std::uint_least32_t all_zero{0x30303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x30303030;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));

									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least32_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}
							else
							{
								val -= 0x30303030;
								val = (val * base_char_type) + (val >> 8);
								val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
								res = static_cast<T>(res * pow_base_sizeof_u32 + val);
								first += sizeof(::std::uint_least32_t);
							}
						}
					}
				}
				else if constexpr (sizeof(char_type) == sizeof(char16_t))
				{
					constexpr ::std::size_t u64_size_of_c16{sizeof(::std::uint_least64_t) / sizeof(char16_t)};
					constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, u64_size_of_c16>};
					constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
					constexpr ::std::uint_least64_t mask{0x000000000000FFFF};
					constexpr ::std::uint_least64_t first_bound{0x7fc67fc67fc67fc6 + 0x0001000100010001 * (10 - base)};
					if constexpr (max_size >= u64_size_of_c16)
					{
						while (static_cast<::std::size_t>(first_phase_last - first) >= u64_size_of_c16) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((val + first_bound) | (val - 0x0030003000300030)) & 0x8000800080008000}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-16)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{0x0030003000300030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x0030003000300030;
									val = (val * base_char_type) + (val >> 16);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 32) & mask));

									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}
							val -= 0x0030003000300030;
							val = (val * base_char_type) + (val >> 16);
							val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 32) & mask));
							res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							first += u64_size_of_c16;
						}
					}
				}
			}
			else if constexpr (base_char_type <= 16)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least64_t))
					{
						constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_4{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 4>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_6{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 6>};
						constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least64_t)>};
						constexpr ::std::uint_least64_t first_bound1{0x3939393939393939 + 0x0101010101010101 * (16 - base_char_type)};
						constexpr ::std::uint_least64_t first_bound2{0x1919191919191919 + 0x0101010101010101 * (16 - base_char_type)};

						constexpr ::std::uint_least64_t mask{0x000000FF000000FF};
						constexpr ::std::uint_least64_t mul1{pow_base_sizeof_base_2 + (pow_base_sizeof_base_6 << 32)};
						constexpr ::std::uint_least64_t mul2{1 + (pow_base_sizeof_base_4 << 32)};
						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((((val + 0x4646464646464646) | (val - 0x3030303030303030)) &
																   ((val + first_bound1) | (val - 0x4040404040404040)) &
																   ((val + first_bound2) | (val - 0x6060606060606060))) |
																  ~(((val + 0x3f3f3f3f3f3f3f3f) | (val - 0x4040404040404040)) &
																	((val + 0x1f1f1f1f1f1f1f1f) | (val - 0x6060606060606060)))) &
																 0x8080808080808080};
								cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{0x3030303030303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x3030303030303030;
									val = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;

									ctrz_cval >>= shifter;

									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 8>.index_unchecked(ctrz_cval) + val);
									first += ctrz_cval;
								}

#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							val -= 0x3030303030303030;
							val = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9;
							val = (val * base_char_type) + (val >> 8);
							val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
							res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							first += sizeof(::std::uint_least64_t);
						}
					}
				}
			}
		}
		else if constexpr (sizeof(::std::uint_least16_t) < sizeof(::std::size_t))
		{
			if constexpr (base_char_type <= 10)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least32_t))
					{
						constexpr ::std::uint_least32_t pow_base_sizeof_u32{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, sizeof(::std::uint_least32_t)>};
						constexpr ::std::uint_least32_t first_bound{0x46464646 + 0x01010101 * (10 - base_char_type)};

						constexpr ::std::uint_least32_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, 2>};
						constexpr ::std::uint_least32_t mask{0x000000FF};
						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least32_t)) [[likely]]
						{
							::std::uint_least32_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least32_t));

							val = ::fast_io::little_endian(val);

							if (::std::uint_least32_t const cval{((val + first_bound) | (val - 0x30303030)) & 0x80808080}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 32 - valid_bits;

									::std::uint_least32_t all_zero{0x30303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x30303030;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
									ctrz_cval >>= shifter;

									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least32_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							val -= 0x30303030;
							val = (val * base_char_type) + (val >> 8);
							val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
							res = static_cast<T>(res * pow_base_sizeof_u32 + val);
							first += sizeof(::std::uint_least32_t);
						}
					}
				}
			}
		}
	}

	for (; first != first_phase_last; ++first) [[likely]]
	{
		unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
		if (char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
		{
			break;
		}
		res *= base_char_type;
		res += ch;
	}

#if !defined(_MSC_VER) || defined(__clang__)
[[maybe_unused]] nextlabel:;
#endif

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}

	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline constexpr parse_result<char_type const *>
compile_time_scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr char8_t base_char_type{base};
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, base> - (!isspecialbase)};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};

	if (diff < mn_val)
	{
		mn_val = diff;
	}

	auto first_phase_last{first + mn_val};
	T res{out};

	for (; first != first_phase_last; ++first)
	{
		unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
		if (char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
		{
			break;
		}
		res *= base_char_type;
		res += ch;
	}

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}

	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &res) noexcept
{
#ifdef __cpp_if_consteval
	if !consteval
#else
	if (!__builtin_is_constant_evaluated())
#endif
	{
		using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
		if constexpr (base == 16 && sizeof(char_type) == sizeof(char8_t) &&
					  !::fast_io::details::is_ebcdic<char_type> &&
					  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
		{
			return ::fast_io::details::scan_int_contiguous_ascii_hex_space_part_define_impl<char_type, T>(
				first, last, res);
		}
		return runtime_scan_int_contiguous_none_simd_space_part_define_impl<base, char_type, T>(first, last, res);
	}
	else
	{
		return compile_time_scan_int_contiguous_none_simd_space_part_define_impl<base, char_type, T>(first, last, res);
	}
}

#if (defined(__GNUC__) || defined(__clang__)) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <::std::size_t base, ::std::integral char_type>
	requires(2u <= base && base <= 10u && sizeof(char_type) == sizeof(char8_t) &&
			 !::fast_io::details::is_ebcdic<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_parse_four_digits(char_type const *first,
									  ::std::uint_least64_t &value) noexcept
{
	::std::uint_least32_t chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	chunk -= 0x30303030u;
	constexpr auto limit_bias{static_cast<::std::uint_least32_t>(16u - base) *
							  0x01010101u};
	if ((chunk & 0xf0f0f0f0u) != 0u ||
		((chunk + limit_bias) & 0x10101010u) != 0u) [[unlikely]]
	{
		return false;
	}
	auto const pairs{(chunk * base + (chunk >> 8u)) & 0x00ff00ffu};
	value = (pairs * (base * base) + (pairs >> 16u)) & 0xffffu;
	return true;
}
#endif

#if (defined(__GNUC__) || defined(__clang__)) && defined(__SSE4_1__) && \
	((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
template <::std::size_t base, ::std::integral char_type>
	requires(5u <= base && base <= 36u && sizeof(char_type) == sizeof(char8_t) &&
			 !::fast_io::details::is_ebcdic<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_sse_parse_eight(char_type const *first,
									::std::uint_least64_t &value) noexcept
{
	using namespace ::fast_io::intrinsics;
	x86_64_v16qu chunk{};
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(::std::uint_least64_t));
	x86_64_v16qu const lower{
		chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
								 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	constexpr char digit_upper{static_cast<char>(base <= 10u ? '0' + base : ':')};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{'/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/'}) &
		(x86_64_v16qs{digit_upper, digit_upper, digit_upper, digit_upper,
						 digit_upper, digit_upper, digit_upper, digit_upper,
						 digit_upper, digit_upper, digit_upper, digit_upper,
						 digit_upper, digit_upper, digit_upper, digit_upper} > schunk)};
	x86_64_v16qs valid_vector{digit_mask};
	x86_64_v16qu values{
		chunk - x86_64_v16qu{'0', '0', '0', '0', '0', '0', '0', '0',
								  '0', '0', '0', '0', '0', '0', '0', '0'}};
	if constexpr (10u < base)
	{
		x86_64_v16qs const slower{(x86_64_v16qs)lower};
		constexpr char alpha_last{static_cast<char>('a' + (base - 10u))};
		x86_64_v16qs const alpha_mask{
			(slower > x86_64_v16qs{'`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`'}) &
			(x86_64_v16qs{alpha_last, alpha_last, alpha_last, alpha_last,
							 alpha_last, alpha_last, alpha_last, alpha_last,
							 alpha_last, alpha_last, alpha_last, alpha_last,
							 alpha_last, alpha_last, alpha_last, alpha_last} > slower)};
		valid_vector |= alpha_mask;
		x86_64_v16qu const alpha_values{
			lower - x86_64_v16qu{'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10,
								  'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10,
								  'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10,
								  'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10}};
		values = (values & (x86_64_v16qu)digit_mask) |
				 (alpha_values & ~(x86_64_v16qu)digit_mask);
	}
	auto const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)valid_vector))};
	if (static_cast<::std::uint_least8_t>(valid_mask) != 0xffu) [[unlikely]]
	{
		return false;
	}
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values,
		x86_64_v16qi{base, 1, base, 1, base, 1, base, 1,
						 base, 1, base, 1, base, 1, base, 1});
	constexpr auto base_squared{static_cast<::std::uint_least16_t>(base * base)};
	values = (x86_64_v16qu)__builtin_ia32_pmaddwd128(
		(x86_64_v8hi)values,
		x86_64_v8hi{base_squared, 1, base_squared, 1,
						 base_squared, 1, base_squared, 1});
	::std::uint_least64_t quads;
	__builtin_memcpy(__builtin_addressof(quads), __builtin_addressof(values),
					 sizeof(quads));
	constexpr auto base_fourth{static_cast<::std::uint_least64_t>(base_squared) *
								  static_cast<::std::uint_least64_t>(base_squared)};
	value = static_cast<::std::uint_least32_t>(quads) * base_fourth +
			(quads >> 32u);
	return true;
}
#endif

#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
template <::std::integral char_type, my_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_hex8_space_part_define_impl(
	char_type const *first, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res) constexpr noexcept -> parse_result<char_type const *> {
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk{};
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(::std::uint_least64_t));
	x86_64_v16qu const lower{chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
												  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const slower{(x86_64_v16qs)lower};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{'/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/'}) &
		(x86_64_v16qs{':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':'} > schunk)};
	x86_64_v16qs const alpha_mask{
		(slower > x86_64_v16qs{'`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`'}) &
		(x86_64_v16qs{'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g'} > slower)};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)(digit_mask | alpha_mask)))};
#else
	__m128i const chunk{_mm_loadl_epi64(reinterpret_cast<__m128i const *>(first))};
	__m128i const lower{_mm_or_si128(chunk, _mm_set1_epi8(0x20))};
	__m128i const digit_mask{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>('/'))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(':')), chunk))};
	__m128i const alpha_mask{
		_mm_and_si128(_mm_cmpgt_epi8(lower, _mm_set1_epi8(static_cast<char>('`'))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>('g')), lower))};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		_mm_movemask_epi8(_mm_or_si128(digit_mask, alpha_mask)))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu const digit_values{
		chunk - x86_64_v16qu{'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'}};
	x86_64_v16qu const alpha_values{
		lower - x86_64_v16qu{'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10,
							 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10}};
	x86_64_v16qu values{(digit_values & (x86_64_v16qu)digit_mask) |
						(alpha_values & ~(x86_64_v16qu)digit_mask)};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1});
	values = (x86_64_v16qu)__builtin_ia32_packuswb128((x86_64_v8hi)values, (x86_64_v8hi)values);
	::std::uint_least32_t res;
	__builtin_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#else
	__m128i const digit_values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>('0')))};
	__m128i const alpha_values{_mm_sub_epi8(lower, _mm_set1_epi8(static_cast<char>('a' - 10)))};
	__m128i values{_mm_blendv_epi8(alpha_values, digit_values, digit_mask)};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	values = _mm_maddubs_epi16(values, _mm_set_epi8(1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16));
	values = _mm_packus_epi16(values, values);
	::std::uint_least32_t res;
	::fast_io::freestanding::my_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#endif
	res = ::fast_io::byte_swap(res);
	if (digits != 8u)
	{
		res >>= static_cast<unsigned>((8u - digits) << 2u);
	}
	return finish_ok(first + digits, static_cast<unsigned_type>(res));
}

template <::std::integral char_type, my_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_hex16_space_part_define_impl(
	char_type const *first, char_type const *last, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res) constexpr noexcept -> parse_result<char_type const *> {
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	x86_64_v16qu const lower{chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
												  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const slower{(x86_64_v16qs)lower};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{'/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/'}) &
		(x86_64_v16qs{':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':', ':'} > schunk)};
	x86_64_v16qs const alpha_mask{
		(slower > x86_64_v16qs{'`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`', '`'}) &
		(x86_64_v16qs{'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g'} > slower)};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)(digit_mask | alpha_mask)))};
#else
	__m128i const chunk{_mm_loadu_si128(reinterpret_cast<__m128i const *>(first))};
	__m128i const lower{_mm_or_si128(chunk, _mm_set1_epi8(0x20))};
	__m128i const digit_mask{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>('/'))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(':')), chunk))};
	__m128i const alpha_mask{
		_mm_and_si128(_mm_cmpgt_epi8(lower, _mm_set1_epi8(static_cast<char>('`'))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>('g')), lower))};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		_mm_movemask_epi8(_mm_or_si128(digit_mask, alpha_mask)))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu const digit_values{
		chunk - x86_64_v16qu{'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'}};
	x86_64_v16qu const alpha_values{
		lower - x86_64_v16qu{'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10,
							 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10, 'a' - 10}};
	x86_64_v16qu values{(digit_values & (x86_64_v16qu)digit_mask) |
						(alpha_values & ~(x86_64_v16qu)digit_mask)};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1});
	values = (x86_64_v16qu)__builtin_ia32_packuswb128((x86_64_v8hi)values, (x86_64_v8hi)values);
	::std::uint_least64_t res;
	__builtin_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#else
	__m128i const digit_values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>('0')))};
	__m128i const alpha_values{_mm_sub_epi8(lower, _mm_set1_epi8(static_cast<char>('a' - 10)))};
	__m128i values{_mm_blendv_epi8(alpha_values, digit_values, digit_mask)};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	values = _mm_maddubs_epi16(values, _mm_set_epi8(1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16));
	values = _mm_packus_epi16(values, values);
	::std::uint_least64_t res;
	::fast_io::freestanding::my_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#endif
	res = ::fast_io::byte_swap(res);
	if (digits != 16u)
	{
		res >>= static_cast<unsigned>((16u - digits) << 2u);
	}
	else if (last != first + 16u &&
			 char_is_digit<16u, char_type>(static_cast<unsigned_char_type>(first[16u]))) [[unlikely]]
	{
		return {skip_digits<16u>(first + 17u, last), parse_code::overflow};
	}
	return finish_ok(first + digits, static_cast<unsigned_type>(res));
}

template <::std::integral char_type, my_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_oct16_space_part_define_impl(
	char_type const *first, char_type const *last, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res, ::std::size_t digits) constexpr noexcept -> parse_result<char_type const *> {
		if (22u < digits || (digits == 22u && 1u < static_cast<unsigned_char_type>(*first - char_literal_v<u8'0', char_type>))) [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const valid_vector{
		(schunk > x86_64_v16qs{'/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/', '/'}) &
		(x86_64_v16qs{'8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8', '8'} > schunk)};
	::std::uint_least16_t const valid_mask{
		static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)valid_vector))};
#else
	__m128i const chunk{_mm_loadu_si128(reinterpret_cast<__m128i const *>(first))};
	__m128i const valid_vector{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>('/'))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>('8')), chunk))};
	::std::uint_least16_t const valid_mask{
		static_cast<::std::uint_least16_t>(_mm_movemask_epi8(valid_vector))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu values{
		chunk - x86_64_v16qu{'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'}};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	auto pairs{(x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1})};
	auto quads{(x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)pairs, x86_64_v8hi{64, 1, 64, 1, 64, 1, 64, 1})};
	::std::uint_least32_t quad_values[4];
	__builtin_memcpy(quad_values, __builtin_addressof(quads), sizeof(quad_values));
#else
	__m128i values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>('0')))};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	auto pairs{_mm_maddubs_epi16(values, _mm_set_epi8(1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8))};
	auto quads{_mm_madd_epi16(pairs, _mm_set_epi16(1, 64, 1, 64, 1, 64, 1, 64))};
	::std::uint_least32_t quad_values[4];
	::fast_io::freestanding::my_memcpy(quad_values, __builtin_addressof(quads), sizeof(quad_values));
#endif
	::std::uint_least64_t res{
		(static_cast<::std::uint_least64_t>(quad_values[0]) << 36u) |
		(static_cast<::std::uint_least64_t>(quad_values[1]) << 24u) |
		(static_cast<::std::uint_least64_t>(quad_values[2]) << 12u) |
		static_cast<::std::uint_least64_t>(quad_values[3])};
	if (digits != 16u)
	{
		res >>= static_cast<unsigned>((16u - digits) * 3u);
		return finish_ok(first + digits, static_cast<unsigned_type>(res), digits);
	}

	auto it{first + 16u};
	::std::size_t total_digits{16u};
	for (; it != last && total_digits != 22u; ++it)
	{
		unsigned_char_type digit{static_cast<unsigned_char_type>(*it)};
		if (char_digit_to_literal<8u, char_type>(digit)) [[unlikely]]
		{
			return finish_ok(it, static_cast<unsigned_type>(res), total_digits);
		}
		res = (res << 3u) | digit;
		++total_digits;
	}
	if (it != last && char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(*it))) [[unlikely]]
	{
		return {skip_digits<8u>(it + 1, last), parse_code::overflow};
	}
	return finish_ok(it, static_cast<unsigned_type>(res), total_digits);
}
#endif

inline constexpr parse_code ongoing_parse_code{static_cast<parse_code>(::std::numeric_limits<char unsigned>::max())};

template <char8_t base, bool oct_c2y, ::std::integral char_type>
inline constexpr parse_result<char_type const *> scan_shbase_impl(char_type const *first,
																  char_type const *last) noexcept
{
	if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	if ((++first) == last) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	if constexpr (base == 2 || base == 3 || (base == 8 && oct_c2y) || base == 16)
	{
		auto ch{*first};
		if ((ch != char_literal_v<(base == 2 ? u8'B' : (base == 3 ? u8'T' : (base == 8 ? u8'O' : u8'X'))), char_type>)&(
				ch != char_literal_v<(base == 2 ? u8'b' : (base == 3 ? u8't' : (base == 8 ? u8'o' : u8'x'))), char_type>)) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
	}
	else
	{
		if (*first != char_literal_v<u8'[', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		constexpr auto digit0{char_literal_v<u8'0' + (base < 10 ? base : base / 10), char_type>};
		if (*first != digit0) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if ((++first) == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if constexpr (10 < base)
		{
			constexpr auto digit1{char_literal_v<u8'0' + (base % 10), char_type>};
			if (*first != digit1) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if ((++first) == last) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
		}
		if (*first != char_literal_v<u8']', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
	}
	return {first, ongoing_parse_code};
}

template <::std::integral char_type>
inline constexpr char_type const *skip_hexdigits(char_type const *first, char_type const *last) noexcept;

template <char8_t base, bool shbase = false, bool skipzero = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, bool zero_terminated_ok = false,
		  ::std::integral char_type, my_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_space_part_define_impl(char_type const *first, char_type const *last, T &t) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	[[maybe_unused]] bool sign{};
	if constexpr (my_signed_integral<T>)
	{
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		constexpr auto minus_sign{char_literal_v<u8'-', char_type>};
		if ((sign = (minus_sign == *first)))
		{
			++first;
		}
		else if constexpr (allow_leading_plus)
		{
			if (*first == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if constexpr (shbase && base != 10)
		{
			if constexpr (base == 8 && !oct_c2y)
			{
				if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				++first;
			}
			else
			{
				auto phase_ret = scan_shbase_impl<base, oct_c2y>(first, last);
				if (phase_ret.code != ongoing_parse_code) [[unlikely]]
				{
					return phase_ret;
				}
				first = phase_ret.iter;
			}
		}
	}
	constexpr auto zero{char_literal_v<u8'0', char_type>};
	if (first == last) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	auto first_ch{*first};
	unsigned_char_type first_digit{static_cast<unsigned_char_type>(first_ch)};
	if (char_digit_to_literal<base, char_type>(first_digit)) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	else if (first_ch == zero) [[unlikely]]
	{
		if constexpr (skipzero)
		{
			++first;
			if constexpr (zero_terminated_ok)
			{
				while (first != last && *first == zero)
				{
					++first;
				}
			}
			else
			{
				first = ::fast_io::details::find_none_zero_simd_impl(first, last);
			}
			if (first == last) [[likely]]
			{
				t = 0;
				return {first, parse_code::ok};
			}
			first_digit = static_cast<unsigned_char_type>(*first);
			if (char_digit_to_literal<base, char_type>(first_digit)) [[unlikely]]
			{
				if constexpr (zero_terminated_ok)
				{
					t = 0;
					return {first, parse_code::ok};
				}
				return {first, parse_code::invalid};
			}
		}
		else
		{
			++first;
			if ((first == last) || (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)))) [[likely]]
			{
				t = {};
				return {first, parse_code::ok};
			}
			return {first, parse_code::invalid};
		}
	}
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
#if (defined(__GNUC__) || defined(__clang__)) && \
	((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	if constexpr (base <= 10u && my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type>)
	{
		auto const swar_remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 4u <= swar_remaining &&
			swar_remaining <= 7u) [[unlikely]]
		{
			::std::uint_least64_t accumulator;
			if (::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(
					first, accumulator)) [[likely]]
			{
				auto iter{first + 4u};
				for (; iter != last; ++iter)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					if (base <= digit) [[unlikely]]
					{
						break;
					}
					accumulator = accumulator * base + digit;
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
	if constexpr (base == 8u && my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type>)
	{
		constexpr auto max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		auto const swar_eight_remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 8u <= swar_eight_remaining &&
			(swar_eight_remaining != 8u ||
			 ::fast_io::details::char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1]))) &&
			(swar_eight_remaining < max_digits ||
			 (swar_eight_remaining == max_digits &&
			  !::fast_io::details::char_is_digit<base, char_type>(
				  static_cast<unsigned_char_type>(last[-1]))))) [[likely]]
		{
			::std::uint_least64_t high;
			::std::uint_least64_t low;
			if (::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(first, high) &&
				::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(first + 4u, low)) [[likely]]
			{
				constexpr auto base_squared{static_cast<::std::uint_least64_t>(base * base)};
				constexpr auto base_fourth{base_squared * base_squared};
				::std::uint_least64_t accumulator{high * base_fourth + low};
				auto iter{first + 8u};
				for (; iter != last; ++iter)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					if (base <= digit) [[unlikely]]
					{
						break;
					}
					accumulator = accumulator * base + digit;
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
#if defined(__SSE4_1__)
	if constexpr ((base == 5u || base == 6u || base == 7u || base == 9u ||
				  base == 14u || 16u <= base) &&
				  my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type>)
	{
		constexpr auto max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 8u <= remaining &&
			(remaining != 8u ||
			 ::fast_io::details::char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1]))) &&
			(remaining < max_digits ||
			 (remaining == max_digits &&
			  !::fast_io::details::char_is_digit<base, char_type>(
				  static_cast<unsigned_char_type>(last[-1]))))) [[likely]]
		{
			::std::uint_least64_t accumulator;
			if (::fast_io::details::scan_int_contiguous_x86_sse_parse_eight<base>(
					first, accumulator)) [[likely]]
			{
				auto iter{first + 8u};
				for (; iter != last; ++iter)
				{
					auto const digit{
						::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
							static_cast<unsigned_char_type>(*iter))};
					if (base <= digit) [[unlikely]]
					{
						break;
					}
					accumulator = accumulator * base + digit;
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
#endif
#endif
	if constexpr (base <= 16 && sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type> &&
				  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
	{
			constexpr bool inline_nonoverflowing_alnum{
				10u < base && base < 16u &&
				sizeof(unsigned_type) == sizeof(::std::uint_least64_t)};
		constexpr ::std::size_t default_inline_limit{inline_nonoverflowing_alnum
														 ? ::fast_io::details::max_int_size_result<unsigned_type, base> - 1u
														 : 8u};
#if (defined(__aarch64__) || defined(_M_ARM64)) && defined(__clang__)
		constexpr ::std::size_t inline_limit{
			base == 2u || (5u <= base && base <= 10u) ? 9u : default_inline_limit};
#else
		constexpr ::std::size_t inline_limit{default_inline_limit};
#endif
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
		if constexpr ((base == 8u || base == 16u) && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			auto const x86_remaining{static_cast<::std::size_t>(last - first)};
			if (x86_remaining == 1u ||
				(1u < x86_remaining &&
				 !char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[1u]))))
			{
				if constexpr (my_signed_integral<T>)
				{
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(first_digit));
					}
					else
					{
						t = static_cast<T>(first_digit);
					}
				}
				else
				{
					t = static_cast<T>(first_digit);
				}
				return {first + 1u, parse_code::ok};
			}
			if constexpr (base == 8u)
			{
				if (2u < x86_remaining &&
					!char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[2u])))
				{
					auto digit{static_cast<unsigned_char_type>(first[1u])};
					digit -= static_cast<unsigned_char_type>(u8'0');
					auto short_value{static_cast<::std::uint_least64_t>(first_digit) * 8u + digit};
					if constexpr (my_signed_integral<T>)
					{
						if (sign)
						{
							t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
						}
						else
						{
							t = static_cast<T>(short_value);
						}
					}
					else
					{
						t = static_cast<T>(short_value);
					}
					return {first + 2u, parse_code::ok};
				}
			}
			if (4u < x86_remaining &&
				!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[4u])))
			{
				::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
				auto short_iter{first + 1u};
				do
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
					digit = static_cast<unsigned_char_type>(*short_iter);
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
					digit = static_cast<unsigned_char_type>(*short_iter);
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
				} while (false);
				if constexpr (my_signed_integral<T>)
				{
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
					}
					else
					{
						t = static_cast<T>(short_value);
					}
				}
				else
				{
					t = static_cast<T>(short_value);
				}
				return {short_iter, parse_code::ok};
			}
			if constexpr (base == 8u)
			{
				if (6u < x86_remaining &&
					!char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[6u])))
				{
					last = first + 6u;
				}
				else if (7u < x86_remaining &&
						 !char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[7u])))
				{
					last = first + 7u;
				}
			}
		}
#endif
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
		{
			if (inline_limit < static_cast<::std::size_t>(last - first) &&
				!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[inline_limit])))
			{
#if defined(__SSE4_1__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
				if constexpr (base == 16u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
				{
					return ::fast_io::details::scan_int_contiguous_x86_sse_hex8_space_part_define_impl(
						first, t, sign);
				}
#endif
				last = first + inline_limit;
			}
		}
#endif
		if (static_cast<::std::size_t>(last - first) <= inline_limit) [[likely]]
		{
			::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
			auto short_iter{first + 1};
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
			if constexpr (base == 8u)
			{
				do
				{
					if (short_iter == last)
					{
						break;
					}
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					auto next_value{short_value * 8u + digit};
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
				} while (false);
			}
			else
#endif
#if (defined(__aarch64__) || defined(_M_ARM64)) && defined(__clang__)
				if constexpr (base == 2u || (5u <= base && base <= 10u))
			{
#pragma clang loop unroll(full)
				for (::std::size_t short_index{1}; short_index != inline_limit; ++short_index)
				{
					if (short_iter == last)
					{
						break;
					}
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 2u)
					{
						short_value = (short_value << 1u) | digit;
					}
					else if constexpr (base == 4u)
					{
						short_value = (short_value << 2u) | digit;
					}
					else if constexpr (base == 8u)
					{
						short_value = (short_value << 3u) | digit;
					}
					else if constexpr (base == 16u)
					{
						short_value = (short_value << 4u) | digit;
					}
					else
					{
						short_value = short_value * base + digit;
					}
					++short_iter;
				}
			}
			else
#endif
			{
				for (; short_iter != last; ++short_iter)
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 2u)
					{
						short_value = (short_value << 1u) | digit;
					}
					else if constexpr (base == 4u)
					{
						short_value = (short_value << 2u) | digit;
					}
					else if constexpr (base == 8u)
					{
						short_value = (short_value << 3u) | digit;
					}
					else if constexpr (base == 16u)
					{
						short_value = (short_value << 4u) | digit;
					}
					else
					{
						short_value = short_value * base + digit;
					}
				}
			}
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			if constexpr (my_signed_integral<T>)
			{
				constexpr unsigned_type imax{umax >> 1};
				if (short_value > static_cast<::std::uint_least64_t>(imax) + sign) [[unlikely]]
				{
					return {short_iter, parse_code::overflow};
				}
				if (sign)
				{
					t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
				}
				else
				{
					t = static_cast<T>(short_value);
				}
			}
			else
			{
				if (short_value > static_cast<::std::uint_least64_t>(umax)) [[unlikely]]
				{
					return {short_iter, parse_code::overflow};
				}
				t = static_cast<T>(short_value);
			}
			return {short_iter, parse_code::ok};
		}
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
		if constexpr (base == 8u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			if (16u <= static_cast<::std::size_t>(last - first))
			{
				return ::fast_io::details::scan_int_contiguous_x86_sse_oct16_space_part_define_impl(
					first, last, t, sign);
			}
		}
		if constexpr (base == 16u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			if (16u <= static_cast<::std::size_t>(last - first))
			{
				return ::fast_io::details::scan_int_contiguous_x86_sse_hex16_space_part_define_impl(
					first, last, t, sign);
			}
		}
#endif
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
		if constexpr (base == 16u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			auto const diff{static_cast<::std::size_t>(last - first)};
			char_type const *last16{};
			if (diff <= 16u)
			{
				last16 = last;
			}
			else if (!char_is_digit<16u, char_type>(static_cast<unsigned_char_type>(first[16u])))
			{
				last16 = first + 16u;
			}
			if (last16 != nullptr)
			{
				::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
				auto short_iter{first + 1};
				for (; short_iter != last16; ++short_iter)
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<16u, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					short_value = (short_value << 4u) | digit;
				}
				if constexpr (my_signed_integral<T>)
				{
					constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
					constexpr unsigned_type imax{umax >> 1};
					if (short_value > static_cast<::std::uint_least64_t>(imax) + sign) [[unlikely]]
					{
						return {short_iter, parse_code::overflow};
					}
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
					}
					else
					{
						t = static_cast<T>(short_value);
					}
				}
				else
				{
					t = static_cast<T>(short_value);
				}
				return {short_iter, parse_code::ok};
			}
		}
#endif
	}
#if (defined(__aarch64__) || defined(__arm64__)) && (!defined(_MSC_VER) || defined(__clang__))
	if constexpr (base == 10u && my_unsigned_integral<T> &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
	{
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (remaining - 16u <= 4u)
		{
			auto const last_is_digit{char_is_digit<10u, char_type>(
				static_cast<unsigned_char_type>(last[-1]))};
			// A terminated 15-digit range and the exact 20-digit SWAR case
			// stay on their existing faster paths.
			if ((remaining != 16u || last_is_digit) &&
				!(remaining == 20u && last_is_digit)) [[unlikely]]
			{
				::std::uint_least64_t value;
				if (::fast_io::details::aarch64_builtin_parse_16_decimal_digits(
						first, value)) [[likely]]
				{
					auto next{first + 16};
					for (; next != last; ++next)
					{
						auto digit{static_cast<unsigned_char_type>(*next)};
						if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
						{
							break;
						}
						value = value * 10u + digit;
					}
					t = static_cast<T>(value);
					return {next, parse_code::ok};
				}
			}
		}
	}
#endif
	unsigned_type res{};
	auto parse_first{first};
#if defined(__aarch64__) || defined(_M_ARM64)
	if constexpr (((5u <= base && base <= 9u) || 16u < base) && my_unsigned_integral<T> &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  !::fast_io::details::is_ebcdic<char_type> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
	{
		constexpr ::std::size_t max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		if (static_cast<::std::size_t>(last - first) < max_digits) [[likely]]
		{
			// The first digit is already validated and mapped above. Starting the
			// AArch64 accumulator with it removes one table load and one loop trip.
			res = static_cast<unsigned_type>(first_digit);
			parse_first = first + 1;
		}
	}
#endif
	char_type const *it;
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	if constexpr (base == 10 && sizeof(char_type) == 1 && sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
	{
		if (
#if __cpp_lib_is_constant_evaluated >= 201811L
			!__builtin_is_constant_evaluated() &&
#endif
			last - first >= 32) [[likely]]
		{
			constexpr bool smaller_than_uint64{sizeof(unsigned_type) < sizeof(::std::uint_least64_t)};
			::std::uint_least64_t temp{};
			auto [digits, ec] = sse_parse<is_ebcdic<char_type>, smaller_than_uint64>(
				reinterpret_cast<char unsigned const *>(first), reinterpret_cast<char unsigned const *>(last), temp);
			it = first + digits;
			if (ec != parse_code::ok) [[unlikely]]
			{
				return {it, ec};
			}
			if constexpr (smaller_than_uint64)
			{
				constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
				if (temp > umax) [[unlikely]]
				{
					return {it, parse_code::overflow};
				}
				res = static_cast<unsigned_type>(temp);
			}
			else
			{
				res = temp;
			}
		}
		else [[unlikely]]
		{
			auto [it2, ec] = scan_int_contiguous_none_simd_space_part_define_impl<base>(parse_first, last, res);
			if (ec != parse_code::ok) [[unlikely]]
			{
				return {it2, ec};
			}
			it = it2;
		}
	}
	else
#endif
	{
		auto [it2, ec] = scan_int_contiguous_none_simd_space_part_define_impl<base>(parse_first, last, res);
		if (ec != parse_code::ok) [[unlikely]]
		{
			return {it2, ec};
		}
		it = it2;
	}
	if constexpr (my_signed_integral<T>)
	{
		constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
		constexpr unsigned_type imax{umax >> 1};
		if (res > (static_cast<my_make_unsigned_t<T>>(imax) + sign)) [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
		if (sign)
		{
			t = static_cast<T>(static_cast<unsigned_type>(0) - res);
		}
		else
		{
			t = static_cast<T>(res);
		}
	}
	else
	{
		t = res;
	}
	return {it, parse_code::ok};
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false,
		  ::std::integral char_type, details::my_integral T>
inline constexpr parse_result<char_type const *> scan_int_contiguous_define_impl(char_type const *first,
																				 char_type const *last, T &t) noexcept
{
	if constexpr (!noskipws)
	{
		first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
		if (first == last)
		{
			return {first, parse_code::end_of_file};
		}
	}
	if constexpr (my_unsigned_integral<T>)
	{
		if constexpr (allow_leading_plus)
		{
			if (first == last) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if (*first == char_literal_v<u8'+', char_type>)
			{
				++first;
				if (first == last) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
			}
		}
		if constexpr (shbase && base != 10)
		{
			if constexpr (base == 8 && !oct_c2y)
			{
				if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				++first;
			}
			else
			{
				auto phase_ret = scan_shbase_impl<base, oct_c2y>(first, last);
				if (phase_ret.code != ongoing_parse_code) [[unlikely]]
				{
					return phase_ret;
				}
				first = phase_ret.iter;
			}
		}
	}
	return scan_int_contiguous_none_space_part_define_impl<base, ((shbase && base != 10) && my_signed_integral<T>),
														   skipzero, oct_c2y, allow_leading_plus>(first, last, t);
}
} // namespace details

enum class scan_integral_context_phase : ::std::uint_least8_t
{
	space,
	sign,
	prefix,
	zero,
	zero_skip,
	zero_invalid,
	digit,
	overflow
};

namespace details
{
template <char8_t base, ::std::integral char_type, ::fast_io::details::my_integral T>
inline constexpr auto scan_context_type_impl_int() noexcept
{
	using unsigned_type = details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr ::std::size_t max_size{
		(::fast_io::details::print_integer_reserved_size_cache<base, false, ::fast_io::details::my_signed_integral<T>,
															   false, unsigned_type>)+2};
	struct scan_integer_context
	{
		::fast_io::freestanding::array<char_type, max_size> buffer;
		::std::uint_least8_t size{};
		scan_integral_context_phase integer_phase{};
		inline constexpr void reset() noexcept
		{
			size = 0;
			integer_phase = scan_integral_context_phase::space;
		}
	};
	return io_type_t<scan_integer_context>{};
}
} // namespace details

namespace details
{

template <::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_space_phase(char_type const *first,
																		char_type const *last) noexcept
{
	first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	return {first, ongoing_parse_code};
}

template <bool allow_negative, bool allow_positive, typename State, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_sign_phase(State &st, char_type const *first,
																	   char_type const *last) noexcept
{
	if (first == last)
	{
		st.integer_phase = scan_integral_context_phase::sign;
		return {first, parse_code::partial};
	}
	if constexpr (allow_negative)
	{
		if constexpr (allow_positive)
		{
			auto ch{*first};
			if (ch == char_literal_v<u8'-', char_type>)
			{
				*st.buffer.data() = ch;
				st.size = 1;
				++first;
			}
			else if (ch == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
		else
		{
			if (*first == char_literal_v<u8'-', char_type>)
			{
				*st.buffer.data() = char_literal_v<u8'-', char_type>;
				st.size = 1;
				++first;
			}
		}
	}
	else
	{
		if constexpr (allow_positive)
		{
			auto ch{*first};
			if (ch == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool oct_c2y, ::std::integral char_type>
	requires(base != 10)
inline constexpr parse_result<char_type const *>
sc_int_ctx_prefix_phase(::std::uint_least8_t &sz, char_type const *first, char_type const *last) noexcept
{
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	if constexpr (base == 8 && !oct_c2y)
	{
		if (sz != 0)
		{
			sz = 0;
			return {first, ongoing_parse_code};
		}
		if (*first != char_literal_v<u8'0', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
		if (first == last)
		{
			sz = 1;
			return {first, parse_code::partial};
		}
	}
	else
	{
		::std::uint_least8_t size_cache{sz};
		if (size_cache == 0)
		{
			if (*first != char_literal_v<u8'0', char_type>) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if ((++first) == last)
			{
				sz = 1;
				return {first, parse_code::partial};
			}
			if constexpr (base != 2 && base != 3 && base != 16)
			{
				size_cache = 1;
			}
		}
		if constexpr (base == 2 || base == 3 || (base == 8 && oct_c2y) || base == 16)
		{
			auto ch{*first};
			if ((ch == char_literal_v<(base == 2 ? u8'B' : (base == 3 ? u8't' : (base == 8 ? u8'O' : u8'X'))), char_type>) |
				(ch == char_literal_v<(base == 2 ? u8'b' : (base == 3 ? u8't' : (base == 8 ? u8'o' : u8'x'))), char_type>)) [[likely]]
			{
				sz = 0;
				++first;
				return {first, ongoing_parse_code};
			}
			else [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
		}
		else
		{
			if (size_cache == 1)
			{
				if (*first != char_literal_v<u8'[', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				if ((++first) == last)
				{
					sz = 2;
					return {first, parse_code::partial};
				}
			}
			constexpr auto digit0{char_literal_v<u8'0' + (base < 10 ? base : base / 10), char_type>};
			if (size_cache == 2)
			{
				if (*first != digit0) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				if ((++first) == last)
				{
					sz = 3;
					return {first, parse_code::partial};
				}
			}
			if constexpr (10 < base)
			{
				constexpr auto digit1{char_literal_v<u8'0' + (base % 10), char_type>};
				if (size_cache == 3)
				{
					if (*first != digit1) [[unlikely]]
					{
						return {first, parse_code::invalid};
					}
					if ((++first) == last)
					{
						sz = 4;
						return {first, parse_code::partial};
					}
				}
			}
			constexpr ::std::uint_least8_t last_index{base < 10 ? 3 : 4};
			if (size_cache == last_index)
			{
				if (*first != char_literal_v<u8']', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				sz = 0;
				++first;
			}
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool skipzero, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_zero_phase(scan_integral_context_phase &integer_phase,
																	   char_type const *first,
																	   char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	if (first == last)
	{
		integer_phase = scan_integral_context_phase::zero;
		return {first, parse_code::partial};
	}
	constexpr auto zero{char_literal_v<u8'0', char_type>};
	auto first_ch{*first};
	if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first_ch))) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	else if (first_ch == zero) [[unlikely]]
	{
		++first;
		if constexpr (skipzero)
		{
			first = find_none_zero_simd_impl(first, last);
		}
		if (first == last)
		{
			if constexpr (skipzero)
			{
				integer_phase = scan_integral_context_phase::zero_skip;
			}
			else
			{
				integer_phase = scan_integral_context_phase::zero_invalid;
			}
			return {first, parse_code::partial};
		}
		if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first))) [[likely]]
		{
			return {first, parse_code::ok};
		}
		if constexpr (!skipzero)
		{
			return {first, parse_code::invalid};
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool oct_c2y, ::std::integral char_type, typename State, my_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *> sc_int_ctx_digit_phase(State &st, char_type const *first,
																		char_type const *last, T &t) noexcept
{
	auto it{skip_digits<base>(first, last)};
	::std::size_t const diff{st.buffer.size() - static_cast<::std::size_t>(st.size)};
	::std::size_t const first_it_diff{static_cast<::std::size_t>(it - first)};
	if (first_it_diff < diff)
	{
		auto start{st.buffer.data() + st.size};
		auto e{non_overlapped_copy_n(first, first_it_diff, start)};
		st.size += static_cast<::std::uint_least8_t>(first_it_diff);
		if (it == last)
		{
			st.integer_phase = scan_integral_context_phase::digit;
			return {it, parse_code::partial};
		}
		if (st.size == 0) [[likely]]
		{
			t = {};
			return {it, parse_code::ok};
		}
		auto [p, ec] = scan_int_contiguous_none_space_part_define_impl<base, false, false, oct_c2y>(st.buffer.data(), e, t);
		return {p - start + first, ec};
	}
	else
	{
		if (it == last)
		{
			st.integer_phase = scan_integral_context_phase::overflow;
			return {it, parse_code::partial};
		}
		else [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
	}
}

template <char8_t base, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_zero_invalid_phase(char_type const *first,
																			   char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	++first;
	if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first))) [[likely]]
	{
		return {first, parse_code::ok};
	}
	return {first, parse_code::invalid};
}

template <char8_t base, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_skip_digits_phase(char_type const *first,
																			  char_type const *last) noexcept
{
	first = skip_digits<base>(first, last);
	return {first, (first == last) ? parse_code::partial : parse_code::invalid};
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false,
		  typename State,
		  ::std::integral char_type, my_integral T>
inline constexpr parse_result<char_type const *> scan_context_define_parse_impl(State &st, char_type const *first,
																				char_type const *last, T &t) noexcept
{
	auto phase{st.integer_phase};
#if __has_cpp_attribute(assume)
	if constexpr (noskipws)
	{
		[[assume(phase != scan_integral_context_phase::space)]];
	}
	if constexpr (my_unsigned_integral<T> && !allow_leading_plus)
	{
		[[assume(phase != scan_integral_context_phase::sign)]];
	}
	if constexpr (!shbase || base == 10)
	{
		[[assume(phase != scan_integral_context_phase::prefix)]];
	}
	if constexpr (skipzero)
	{
		[[assume(phase != scan_integral_context_phase::zero_invalid)]];
	}
	else
	{
		[[assume(phase != scan_integral_context_phase::zero_skip)]];
	}
#endif
	switch (phase)
	{
	case scan_integral_context_phase::space:
	{
		if constexpr (!noskipws)
		{
			auto phase_ret = sc_int_ctx_space_phase(first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::sign:
	{
		if constexpr (my_signed_integral<T> || allow_leading_plus)
		{
			auto phase_ret = sc_int_ctx_sign_phase<my_signed_integral<T>, allow_leading_plus>(st, first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::prefix:
	{
		if constexpr (shbase && base != 10)
		{
			st.integer_phase = scan_integral_context_phase::prefix;
			auto phase_ret = sc_int_ctx_prefix_phase<base, oct_c2y>(st.size, first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::zero:
	case scan_integral_context_phase::zero_skip:
	{
		if constexpr (!(shbase && base != 10))
		{
			auto phase_ret = sc_int_ctx_zero_phase<base, skipzero>(st.integer_phase, first, last);
			if (phase_ret.code != ongoing_parse_code)
			{
				if constexpr (skipzero)
				{
					if (phase_ret.code == parse_code::ok)
					{
						t = {};
					}
					else if (phase_ret.code == parse_code::invalid && phase == scan_integral_context_phase::zero_skip)
					{
						t = {};
						phase_ret.code = parse_code::ok;
					}
				}
				else
				{
					if (phase_ret.code == parse_code::ok)
					{
						t = {};
					}
				}
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::digit:
	{
		return sc_int_ctx_digit_phase<base, oct_c2y>(st, first, last, t);
	}
	case scan_integral_context_phase::zero_invalid:
	{
		if constexpr (skipzero)
		{
			return {first, parse_code::invalid};
		}
		else
		{
			auto phase_ret = sc_int_ctx_zero_invalid_phase<base>(first, last);
			if (phase_ret.code == parse_code::ok)
			{
				t = {};
			}
			return phase_ret;
		}
	}
	case scan_integral_context_phase::overflow:
	{
		first = skip_digits<base>(first, last);
		return {first, (first == last) ? parse_code::partial : parse_code::overflow};
	}
	default:
	{
		return sc_int_ctx_skip_digits_phase<base>(first, last);
	}
	}
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y, typename State, my_integral T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr parse_code scan_context_eof_define_parse_impl(State &st, T &t) noexcept
{
	auto phase{st.integer_phase};
#if __has_cpp_attribute(assume)
	if constexpr (!skipzero)
	{
		[[assume(phase != scan_integral_context_phase::zero_skip)]];
	}
#endif
	switch (phase)
	{
	case scan_integral_context_phase::space:
	{
		if constexpr (noskipws)
		{
			return parse_code::invalid;
		}
		else
		{
			return parse_code::end_of_file;
		}
	}
	case scan_integral_context_phase::digit:
		return scan_int_contiguous_none_space_part_define_impl<base, false, false, oct_c2y>(st.buffer.data(), st.buffer.data() + st.size, t).code;
	case scan_integral_context_phase::overflow:
		return parse_code::overflow;
	case scan_integral_context_phase::zero_skip:
	case scan_integral_context_phase::zero_invalid:
	{
		t = {};
		return parse_code::ok;
	}
	default:
		return parse_code::invalid;
	}
}

} // namespace details

namespace manipulators
{

template <typename char_type>
struct ch_get_t
{
	using manip_tag = manip_tag_t;
	char_type &reference;
};

template <::fast_io::details::my_integral T>
inline constexpr ch_get_t<T &> ch_get(T &reference) noexcept
{
	return {reference};
}

template <::std::size_t bs, bool noskipws = false, bool skipzero = false, bool prefix = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_integral scalar_type>
	requires(2 <= bs && bs <= 36)
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									bs, noskipws, (bs == 10 ? false : prefix), skipzero, oct_c2y,
									allow_leading_plus>,
								scalar_type &>
base_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									2, noskipws, prefix, skipzero, false, allow_leading_plus>,
								scalar_type &>
bin_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									2, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
bin0b_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, prefix, skipzero, oct_c2y, allow_leading_plus>,
								scalar_type &>
oct_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
oct0_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, true, skipzero, true, allow_leading_plus>,
								scalar_type &>
oct0o_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									10, noskipws, false, skipzero, false, allow_leading_plus>,
								scalar_type &>
dec_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, prefix, skipzero, false, allow_leading_plus>,
								scalar_type &>
hex_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
hex0x_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool allow_leading_plus = false, ::fast_io::details::my_unsigned_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, true, true, false, allow_leading_plus>,
								scalar_type &>
addrvw_get(scalar_type &t) noexcept
{
	return {t};
}

} // namespace manipulators

template <details::my_integral T>
inline constexpr ::fast_io::manipulators::scalar_manip_t<
	::fast_io::details::base_scan_mani_flags_cache<10, false, false, false, false>, T &>
scan_alias_define(io_alias_t, T &t) noexcept
{
	return {t};
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_integral T>
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return details::scan_context_type_impl_int<flags.base, char_type, T>();
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_int_contiguous_define_impl<flags.base, flags.noskipws, flags.showbase, flags.full,
													flags.modern_octal, flags.allow_leading_plus>(
		begin, end, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename State, details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>, State &state,
					char_type const *begin, char_type const *end,
					::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_context_define_parse_impl<flags.base, flags.noskipws, flags.showbase, flags.full,
												   flags.modern_octal, flags.allow_leading_plus>(
		state, begin, end, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename State, details::my_integral T>
inline constexpr parse_code
scan_context_eof_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>, State &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_context_eof_define_parse_impl<flags.base, flags.noskipws, flags.showbase, flags.full, flags.modern_octal>(
		state, t.reference);
}

namespace details
{
template <::std::integral char_type>
inline constexpr parse_result<char_type const *> ch_get_context_impl(char_type const *first, char_type const *last,
																	 char_type &t) noexcept
{
	first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	t = *first;
	++first;
	return {first, parse_code::ok};
}
} // namespace details

template <::std::integral char_type>
inline constexpr io_type_t<details::empty>
scan_context_type(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>, details::empty,
					char_type const *begin, char_type const *end, manipulators::ch_get_t<char_type &> t) noexcept
{
	return details::ch_get_context_impl(begin, end, t.reference);
}

template <::std::integral char_type>
inline constexpr parse_code scan_context_eof_define(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>,
													details::empty, manipulators::ch_get_t<char_type &>) noexcept
{
	return parse_code::end_of_file;
}

} // namespace fast_io
