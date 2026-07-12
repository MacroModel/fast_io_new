#pragma once


namespace fast_io
{

using to_chars_result = ::std::to_chars_result;

namespace details
{

template <::std::size_t base, ::std::unsigned_integral U>
inline constexpr ::fast_io::to_chars_result to_chars_integral_fixed_base(char *first, char *last, U value,
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
		*first++ = '-';
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

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

	// quick path
	switch (base)
	{
	[[likely]] case 10:
		return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
	case 16:
		return ::fast_io::details::to_chars_integral_fixed_base<16u>(first, last, magnitude, negative);
	case 2:
		return ::fast_io::details::to_chars_integral_fixed_base<2u>(first, last, magnitude, negative);
	case 8:
		return ::fast_io::details::to_chars_integral_fixed_base<8u>(first, last, magnitude, negative);
	}

	switch (base)
	{
	case 3:
		return ::fast_io::details::to_chars_integral_fixed_base<3u>(first, last, magnitude, negative);
	case 4:
		return ::fast_io::details::to_chars_integral_fixed_base<4u>(first, last, magnitude, negative);
	case 5:
		return ::fast_io::details::to_chars_integral_fixed_base<5u>(first, last, magnitude, negative);
	case 6:
		return ::fast_io::details::to_chars_integral_fixed_base<6u>(first, last, magnitude, negative);
	case 7:
		return ::fast_io::details::to_chars_integral_fixed_base<7u>(first, last, magnitude, negative);
	case 9:
		return ::fast_io::details::to_chars_integral_fixed_base<9u>(first, last, magnitude, negative);
	case 11:
		return ::fast_io::details::to_chars_integral_fixed_base<11u>(first, last, magnitude, negative);
	case 12:
		return ::fast_io::details::to_chars_integral_fixed_base<12u>(first, last, magnitude, negative);
	case 13:
		return ::fast_io::details::to_chars_integral_fixed_base<13u>(first, last, magnitude, negative);
	case 14:
		return ::fast_io::details::to_chars_integral_fixed_base<14u>(first, last, magnitude, negative);
	case 15:
		return ::fast_io::details::to_chars_integral_fixed_base<15u>(first, last, magnitude, negative);
	case 17:
		return ::fast_io::details::to_chars_integral_fixed_base<17u>(first, last, magnitude, negative);
	case 18:
		return ::fast_io::details::to_chars_integral_fixed_base<18u>(first, last, magnitude, negative);
	case 19:
		return ::fast_io::details::to_chars_integral_fixed_base<19u>(first, last, magnitude, negative);
	case 20:
		return ::fast_io::details::to_chars_integral_fixed_base<20u>(first, last, magnitude, negative);
	case 21:
		return ::fast_io::details::to_chars_integral_fixed_base<21u>(first, last, magnitude, negative);
	case 22:
		return ::fast_io::details::to_chars_integral_fixed_base<22u>(first, last, magnitude, negative);
	case 23:
		return ::fast_io::details::to_chars_integral_fixed_base<23u>(first, last, magnitude, negative);
	case 24:
		return ::fast_io::details::to_chars_integral_fixed_base<24u>(first, last, magnitude, negative);
	case 25:
		return ::fast_io::details::to_chars_integral_fixed_base<25u>(first, last, magnitude, negative);
	case 26:
		return ::fast_io::details::to_chars_integral_fixed_base<26u>(first, last, magnitude, negative);
	case 27:
		return ::fast_io::details::to_chars_integral_fixed_base<27u>(first, last, magnitude, negative);
	case 28:
		return ::fast_io::details::to_chars_integral_fixed_base<28u>(first, last, magnitude, negative);
	case 29:
		return ::fast_io::details::to_chars_integral_fixed_base<29u>(first, last, magnitude, negative);
	case 30:
		return ::fast_io::details::to_chars_integral_fixed_base<30u>(first, last, magnitude, negative);
	case 31:
		return ::fast_io::details::to_chars_integral_fixed_base<31u>(first, last, magnitude, negative);
	case 32:
		return ::fast_io::details::to_chars_integral_fixed_base<32u>(first, last, magnitude, negative);
	case 33:
		return ::fast_io::details::to_chars_integral_fixed_base<33u>(first, last, magnitude, negative);
	case 34:
		return ::fast_io::details::to_chars_integral_fixed_base<34u>(first, last, magnitude, negative);
	case 35:
		return ::fast_io::details::to_chars_integral_fixed_base<35u>(first, last, magnitude, negative);
	case 36:
		return ::fast_io::details::to_chars_integral_fixed_base<36u>(first, last, magnitude, negative);
	default:
		::fast_io::fast_terminate();
	}
}

inline ::fast_io::to_chars_result to_chars(char *, char *, bool, int = 10) = delete;

} // namespace fast_io
