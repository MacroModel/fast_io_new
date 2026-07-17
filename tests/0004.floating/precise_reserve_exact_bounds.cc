#include <cstddef>

#include <fast_io_freestanding.h>

namespace
{

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision mode =
			  ::fast_io::manipulators::floating_precision::significant>
inline constexpr auto flags_for = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.precision = mode;
	return flags;
}();

#if defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
using narrow_type = _Float16;
#else
using narrow_type = float;
#endif

template <typename char_type>
bool check_adjacent() noexcept
{
	using first_type = ::fast_io::manipulators::scalar_manip_t<
		flags_for<::fast_io::manipulators::floating_format::decimal>, narrow_type>;
	using second_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags_for<::fast_io::manipulators::floating_format::scientific,
				  ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero>,
		double>;
	using third_type = ::fast_io::manipulators::scalar_manip_t<
		flags_for<::fast_io::manipulators::floating_format::hexfloat>, float>;
	static_assert(::fast_io::precise_reserve_printable<char_type, first_type>);
	static_assert(::fast_io::precise_reserve_printable<char_type, second_type>);
	static_assert(::fast_io::precise_reserve_printable<char_type, third_type>);

	first_type first{static_cast<narrow_type>(0.1)};
	second_type second{1.2345678901234567, 16u};
	third_type third{0.125f};
	auto const first_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char_type, first_type>{}, first)};
	auto const second_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char_type, second_type>{}, second)};
	auto const third_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char_type, third_type>{}, third)};

	constexpr ::std::size_t prefix{32u};
	char_type storage[512u];
	for (auto &element : storage)
	{
		element = static_cast<char_type>(0x5a);
	}
	auto *const begin{storage + prefix};
	auto suffix_is_clean = [&](::std::size_t used) noexcept {
		for (auto index{prefix + used}; index != 512u; ++index)
		{
			if (storage[index] != static_cast<char_type>(0x5a))
			{
				return false;
			}
		}
		return true;
	};

	auto *iter{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type_t<char_type, first_type>{}, begin,
		first_size, first)};
	if (iter != begin + first_size || !suffix_is_clean(first_size))
	{
		return false;
	}
	iter = ::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type_t<char_type, second_type>{}, iter,
		second_size, second);
	if (iter != begin + first_size + second_size ||
		!suffix_is_clean(first_size + second_size))
	{
		return false;
	}
	iter = ::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type_t<char_type, third_type>{}, iter,
		third_size, third);
	if (iter != begin + first_size + second_size + third_size ||
		!suffix_is_clean(first_size + second_size + third_size))
	{
		return false;
	}
	for (::std::size_t index{}; index != prefix; ++index)
	{
		if (storage[index] != static_cast<char_type>(0x5a))
		{
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	return !(check_adjacent<char>() && check_adjacent<wchar_t>() &&
			 check_adjacent<char8_t>() && check_adjacent<char16_t>() &&
			 check_adjacent<char32_t>());
}
