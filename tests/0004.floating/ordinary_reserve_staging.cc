#include <bit>
#include <cstddef>
#include <cstdint>

#include <fast_io_freestanding.h>

namespace
{

template <::fast_io::manipulators::floating_format format, bool decorated,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	flags.comma = decorated;
	flags.json_float = decorated &&
					   format != ::fast_io::manipulators::floating_format::scientific;
	return flags;
}();

template <typename flt, ::fast_io::manipulators::floating_format format,
		  bool decorated, ::fast_io::manipulators::floating_rounding rounding = ::fast_io::manipulators::floating_rounding::nearest_to_even>
bool check_value(flt value) noexcept
{
	constexpr auto flags{test_flags<format, decorated, rounding>};
	using manipulator = ::fast_io::manipulators::scalar_manip_t<flags, flt>;
	constexpr auto reserve_size{::fast_io::print_reserve_size(
		::fast_io::io_reserve_type<char, manipulator>)};
	constexpr unsigned char sentinel{0xa5u};
	char storage[reserve_size + 16u];
	for (auto &element : storage)
	{
		element = static_cast<char>(sentinel);
	}
	auto const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char, manipulator>, storage,
		manipulator{value})};
	if (end < storage || storage + reserve_size < end)
	{
		return false;
	}
	for (auto const *iter{storage + reserve_size}; iter != storage + sizeof(storage);
		 ++iter)
	{
		if (static_cast<unsigned char>(*iter) != sentinel)
		{
			return false;
		}
	}
	return true;
}

template <typename flt, typename uint_type, uint_type raw,
		  ::std::size_t minimum_reserve>
bool check_type() noexcept
{
	constexpr auto value{::std::bit_cast<flt>(raw)};
	using decimal_manipulator = ::fast_io::manipulators::scalar_manip_t<
		test_flags<::fast_io::manipulators::floating_format::decimal, false>, flt>;
	using scientific_manipulator = ::fast_io::manipulators::scalar_manip_t<
		test_flags<::fast_io::manipulators::floating_format::scientific, false>, flt>;
	using general_manipulator = ::fast_io::manipulators::scalar_manip_t<
		test_flags<::fast_io::manipulators::floating_format::general, false>, flt>;
	static_assert(::fast_io::details::is_ebcdic<char> ||
				  ::fast_io::print_reserve_size(
					  ::fast_io::io_reserve_type<char, decimal_manipulator>) >= minimum_reserve);
	static_assert(::fast_io::details::is_ebcdic<char> ||
				  ::fast_io::print_reserve_size(
					  ::fast_io::io_reserve_type<char, scientific_manipulator>) >= minimum_reserve);
	static_assert(::fast_io::details::is_ebcdic<char> ||
				  ::fast_io::print_reserve_size(
					  ::fast_io::io_reserve_type<char, general_manipulator>) >= minimum_reserve);
	return check_value<flt, ::fast_io::manipulators::floating_format::decimal,
					   false>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::scientific,
					   false>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::general,
					   false>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::decimal,
					   true>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::general,
					   true>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::decimal,
					   false, ::fast_io::manipulators::floating_rounding::current_environment>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::scientific,
					   false, ::fast_io::manipulators::floating_rounding::current_environment>(value) &&
		   check_value<flt, ::fast_io::manipulators::floating_format::general,
					   false, ::fast_io::manipulators::floating_rounding::current_environment>(value);
}

} // namespace

int main()
{
	return !(check_type<float, ::std::uint_least32_t, 0x8a7d3a13u, 19u>() &&
			 check_type<double, ::std::uint_least64_t, 0xb8bb4bbcb910a18dULL, 27u>());
}
