#pragma once

#include <charconv>

namespace fast_io
{

using from_chars_result = ::std::from_chars_result;

namespace details
{

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
	return {original_first, ::std::errc::invalid_argument};
}

template <::std::size_t base, ::std::integral T>
	requires(2u <= base && base <= 36u &&
			 !::std::same_as<::std::remove_cv_t<T>, bool>)
[[gnu::always_inline]] inline constexpr ::fast_io::from_chars_result
from_chars_integral_fixed_base(char const *first, char const *last, T &value) noexcept
{
	auto const original_first{first};
	auto const result =
		::fast_io::details::scan_int_contiguous_none_space_part_define_impl<
			static_cast<char8_t>(base), false, true, false, false, true>(
			first, last, value);
	return ::fast_io::details::from_chars_integral_map_result(result, original_first);
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
