#pragma once

#include <charconv>

namespace fast_io
{

using from_chars_result = ::std::from_chars_result;

namespace details
{

template <bool signed_integer>
inline constexpr ::fast_io::from_chars_result
from_chars_integral_map_result(::fast_io::parse_result<char const *> result,
							   char const *original_first) noexcept
{
	if (result.code == ::fast_io::parse_code::ok) [[likely]]
	{
		return {result.iter, {}};
	}
	if (result.code == ::fast_io::parse_code::overflow)
	{
		return {result.iter, ::std::errc::result_out_of_range};
	}
	if constexpr (signed_integer)
	{
		return {original_first, ::std::errc::invalid_argument};
	}
	else
	{
		return {result.iter, ::std::errc::invalid_argument};
	}
}

template <::std::size_t base, ::std::integral T>
	requires(2u <= base && base <= 36u &&
			 !::std::same_as<::std::remove_cv_t<T>, bool>)
[[gnu::always_inline]] inline constexpr ::fast_io::from_chars_result
from_chars_integral_fixed_base(char const *first, char const *last, T &value) noexcept
{
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if (first == last) [[unlikely]]
	{
		return {first, ::std::errc::invalid_argument};
	}
	if constexpr (base == 10u)
	{
		if constexpr (::std::signed_integral<T>)
		{
			auto const original_first{first};
			if (*first == '-')
			{
				++first;
				if (first == last) [[unlikely]]
				{
					return {original_first, ::std::errc::invalid_argument};
				}
				auto digit{static_cast<unsigned char>(*first)};
				if (!::fast_io::details::char_digit_to_literal<10u, char>(digit)) [[likely]]
				{
					auto const next{first + 1u};
					if (next == last ||
						!::fast_io::details::char_is_digit<10u, char>(
							static_cast<unsigned char>(*next)))
					{
						using unsigned_type = ::std::make_unsigned_t<T>;
						value = static_cast<T>(static_cast<unsigned_type>(0) -
											   static_cast<unsigned_type>(digit));
						return {next, {}};
					}
				}
				first = original_first;
			}
			else
			{
				auto digit{static_cast<unsigned char>(*first)};
				if (!::fast_io::details::char_digit_to_literal<10u, char>(digit)) [[likely]]
				{
					auto const next{first + 1u};
					if (next == last ||
						!::fast_io::details::char_is_digit<10u, char>(
							static_cast<unsigned char>(*next)))
					{
						value = static_cast<T>(digit);
						return {next, {}};
					}
				}
			}
		}
		else
		{
			auto digit{static_cast<unsigned char>(*first)};
			if (!::fast_io::details::char_digit_to_literal<10u, char>(digit)) [[likely]]
			{
				auto const next{first + 1u};
				if (next == last ||
					!::fast_io::details::char_is_digit<10u, char>(
						static_cast<unsigned char>(*next)))
				{
					value = static_cast<T>(digit);
					return {next, {}};
				}
			}
		}
	}
#endif
	if constexpr (::std::unsigned_integral<T> && sizeof(T) == sizeof(::std::uint_least64_t) &&
				  base == 8u)
	{
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if ((remaining == 10u &&
			 ::fast_io::details::char_is_digit<8u, char>(
				 static_cast<unsigned char>(first[9u]))) ||
			(remaining == 11u &&
			 !::fast_io::details::char_is_digit<8u, char>(
				 static_cast<unsigned char>(first[10u])))) [[unlikely]]
		{
			auto const digit0{static_cast<unsigned char>(first[0u] - '0')};
			auto const digit1{static_cast<unsigned char>(first[1u] - '0')};
			auto const digit2{static_cast<unsigned char>(first[2u] - '0')};
			auto const digit3{static_cast<unsigned char>(first[3u] - '0')};
			auto const digit4{static_cast<unsigned char>(first[4u] - '0')};
			auto const digit5{static_cast<unsigned char>(first[5u] - '0')};
			auto const digit6{static_cast<unsigned char>(first[6u] - '0')};
			auto const digit7{static_cast<unsigned char>(first[7u] - '0')};
			auto const digit8{static_cast<unsigned char>(first[8u] - '0')};
			auto const digit9{static_cast<unsigned char>(first[9u] - '0')};
			if ((digit0 | digit1 | digit2 | digit3 | digit4 | digit5 | digit6 |
				 digit7 | digit8 | digit9) <= 7u) [[likely]]
			{
				auto const parsed{
					(static_cast<::std::uint_least64_t>(digit0) << 27u) |
					(static_cast<::std::uint_least64_t>(digit1) << 24u) |
					(static_cast<::std::uint_least64_t>(digit2) << 21u) |
					(static_cast<::std::uint_least64_t>(digit3) << 18u) |
					(static_cast<::std::uint_least64_t>(digit4) << 15u) |
					(static_cast<::std::uint_least64_t>(digit5) << 12u) |
					(static_cast<::std::uint_least64_t>(digit6) << 9u) |
					(static_cast<::std::uint_least64_t>(digit7) << 6u) |
					(static_cast<::std::uint_least64_t>(digit8) << 3u) |
					static_cast<::std::uint_least64_t>(digit9)};
				value = static_cast<T>(parsed);
				return {first + 10u, {}};
			}
		}
	}
	if constexpr (::std::unsigned_integral<T> && sizeof(T) == sizeof(::std::uint_least64_t) &&
				  (base == 3u || base == 4u || (11u <= base && base <= 16u)))
	{
		constexpr ::std::size_t short_limit{8u};
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (remaining <= short_limit ||
			(remaining == short_limit + 1u &&
			 !::fast_io::details::char_is_digit<static_cast<char8_t>(base), char>(
				 static_cast<unsigned char>(last[-1])))) [[likely]]
		{
			using unsigned_type = ::std::make_unsigned_t<T>;
			unsigned_type accumulator{};
			auto iter{first};
			::std::size_t digits{};
			for (; iter != last && digits != short_limit; ++iter, ++digits)
			{
				auto digit{static_cast<unsigned char>(*iter)};
				if (::fast_io::details::char_digit_to_literal<static_cast<char8_t>(base), char>(
						digit)) [[unlikely]]
				{
					break;
				}
				if constexpr (base == 4u)
				{
					accumulator = static_cast<unsigned_type>((accumulator << 2u) | digit);
				}
				else
				{
					accumulator = static_cast<unsigned_type>(accumulator * base + digit);
				}
			}
			if (iter == last ||
				!::fast_io::details::char_is_digit<static_cast<char8_t>(base), char>(
					static_cast<unsigned char>(*iter))) [[likely]]
			{
				if (digits == 0u) [[unlikely]]
				{
					return {first, ::std::errc::invalid_argument};
				}
				value = static_cast<T>(accumulator);
				return {iter, {}};
			}
		}
	}
	auto const original_first{first};
	auto const result =
		::fast_io::details::scan_int_contiguous_none_space_part_define_impl<
			static_cast<char8_t>(base), false, true, false, false, true>(
			first, last, value);
	return ::fast_io::details::from_chars_integral_map_result<::std::signed_integral<T>>(
		result, original_first);
}

template <::std::integral T>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
[[gnu::always_inline]] inline constexpr ::fast_io::from_chars_result
from_chars_integral_runtime_base(char const *first, char const *last, T &value,
								 int base) noexcept
{
	switch (base)
	{
	case 2:
		return ::fast_io::details::from_chars_integral_fixed_base<2u>(first, last, value);
	case 3:
		return ::fast_io::details::from_chars_integral_fixed_base<3u>(first, last, value);
	case 4:
		return ::fast_io::details::from_chars_integral_fixed_base<4u>(first, last, value);
	case 5:
		return ::fast_io::details::from_chars_integral_fixed_base<5u>(first, last, value);
	case 6:
		return ::fast_io::details::from_chars_integral_fixed_base<6u>(first, last, value);
	case 7:
		return ::fast_io::details::from_chars_integral_fixed_base<7u>(first, last, value);
	case 8:
		return ::fast_io::details::from_chars_integral_fixed_base<8u>(first, last, value);
	case 9:
		return ::fast_io::details::from_chars_integral_fixed_base<9u>(first, last, value);
	[[likely]] case 10:
		return ::fast_io::details::from_chars_integral_fixed_base<10u>(first, last, value);
	case 11:
		return ::fast_io::details::from_chars_integral_fixed_base<11u>(first, last, value);
	case 12:
		return ::fast_io::details::from_chars_integral_fixed_base<12u>(first, last, value);
	case 13:
		return ::fast_io::details::from_chars_integral_fixed_base<13u>(first, last, value);
	case 14:
		return ::fast_io::details::from_chars_integral_fixed_base<14u>(first, last, value);
	case 15:
		return ::fast_io::details::from_chars_integral_fixed_base<15u>(first, last, value);
	case 16:
		return ::fast_io::details::from_chars_integral_fixed_base<16u>(first, last, value);
	case 17:
		return ::fast_io::details::from_chars_integral_fixed_base<17u>(first, last, value);
	case 18:
		return ::fast_io::details::from_chars_integral_fixed_base<18u>(first, last, value);
	case 19:
		return ::fast_io::details::from_chars_integral_fixed_base<19u>(first, last, value);
	case 20:
		return ::fast_io::details::from_chars_integral_fixed_base<20u>(first, last, value);
	case 21:
		return ::fast_io::details::from_chars_integral_fixed_base<21u>(first, last, value);
	case 22:
		return ::fast_io::details::from_chars_integral_fixed_base<22u>(first, last, value);
	case 23:
		return ::fast_io::details::from_chars_integral_fixed_base<23u>(first, last, value);
	case 24:
		return ::fast_io::details::from_chars_integral_fixed_base<24u>(first, last, value);
	case 25:
		return ::fast_io::details::from_chars_integral_fixed_base<25u>(first, last, value);
	case 26:
		return ::fast_io::details::from_chars_integral_fixed_base<26u>(first, last, value);
	case 27:
		return ::fast_io::details::from_chars_integral_fixed_base<27u>(first, last, value);
	case 28:
		return ::fast_io::details::from_chars_integral_fixed_base<28u>(first, last, value);
	case 29:
		return ::fast_io::details::from_chars_integral_fixed_base<29u>(first, last, value);
	case 30:
		return ::fast_io::details::from_chars_integral_fixed_base<30u>(first, last, value);
	case 31:
		return ::fast_io::details::from_chars_integral_fixed_base<31u>(first, last, value);
	case 32:
		return ::fast_io::details::from_chars_integral_fixed_base<32u>(first, last, value);
	case 33:
		return ::fast_io::details::from_chars_integral_fixed_base<33u>(first, last, value);
	case 34:
		return ::fast_io::details::from_chars_integral_fixed_base<34u>(first, last, value);
	case 35:
		return ::fast_io::details::from_chars_integral_fixed_base<35u>(first, last, value);
	case 36:
		return ::fast_io::details::from_chars_integral_fixed_base<36u>(first, last, value);
	[[unlikely]] default:
		::fast_io::fast_terminate();
	}
}

} // namespace details

template <::std::integral T>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
[[gnu::always_inline]] inline constexpr ::fast_io::from_chars_result
from_chars(char const *first, char const *last, T &value, int base = 10) noexcept
{
#if __has_cpp_attribute(assume)
	[[assume(2 <= base && base <= 36)]];
#endif
	return ::fast_io::details::from_chars_integral_runtime_base(first, last, value, base);
}

inline ::fast_io::from_chars_result from_chars(char const *, char const *, bool &,
											   int = 10) = delete;

} // namespace fast_io
