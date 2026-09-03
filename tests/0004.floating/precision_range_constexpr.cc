#include <cstddef>
#include <limits>
#include <type_traits>

#include <fast_io.h>

namespace
{

template <::std::size_t extent, typename printable_type>
[[nodiscard]] consteval bool check_spelling(
	printable_type const &value, char const (&expected)[extent]) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	if (precise_size != extent - 1u)
	{
		return false;
	}
	char storage[extent]{};
	auto *const end{print_reserve_precise_define(
		tag{}, storage, precise_size, value)};
	if (end != storage + precise_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (storage[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, typename printable_type>
[[nodiscard]] consteval ::std::size_t precise_size_of(
	printable_type const &value) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	return print_reserve_precise_size(
		::fast_io::io_reserve_type_t<char_type, clean_type>{}, value);
}

[[nodiscard]] consteval bool check_range_spellings() noexcept
{
	using namespace ::fast_io::mnp;
	using rounding_mode = ::fast_io::manipulators::floating_rounding;
	constexpr auto minimum{
		(::std::numeric_limits<double>::denorm_min)()};
	constexpr auto minimum_fixed{
		precision_range(fixed(minimum), 1u, 17u)};
	return check_spelling(
			   precision_range(decimal(0.1), 3u, 17u), "0.100") &&
		   check_spelling(
			   precision_range(decimal(1.234567), 1u, 5u), "1.2346") &&
		   check_spelling(
			   precision_range(scientific(0.1), 3u, 17u), "1.00e-01") &&
		   check_spelling(
			   json_float(precision_range(decimal(1.0), 1u, 17u)), "1.0") &&
		   check_spelling(
			   precision_range(decimal(minimum), 1u, 17u), "5e-324") &&
		   check_spelling(rounding<rounding_mode::nearest_to_even>(
							  precision_range(decimal(1.75), 1u, 2u)),
						  "1.8") &&
		   check_spelling(rounding<rounding_mode::nearest_to_odd>(
							  precision_range(decimal(1.75), 1u, 2u)),
						  "1.7") &&
		   check_spelling(rounding<rounding_mode::toward_zero>(
							  precision_range(decimal(1.75), 1u, 2u)),
						  "1.7") &&
		   check_spelling(rounding<rounding_mode::toward_plus_infinity>(
							  precision_range(decimal(1.75), 1u, 2u)),
						  "1.8") &&
		   check_spelling(rounding<rounding_mode::toward_minus_infinity>(
							  precision_range(decimal(1.75), 1u, 2u)),
						  "1.7") &&
		   precise_size_of<char>(minimum_fixed) == 326u &&
		   precise_size_of<wchar_t>(minimum_fixed) == 326u &&
		   precise_size_of<char8_t>(minimum_fixed) == 326u &&
		   precise_size_of<char16_t>(minimum_fixed) == 326u &&
		   precise_size_of<char32_t>(minimum_fixed) == 326u;
}

static_assert(check_range_spellings());

} // namespace

int main()
{}
