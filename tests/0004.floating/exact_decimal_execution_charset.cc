#include <fast_io_freestanding.h>

namespace
{

template <typename char_type>
[[nodiscard]] constexpr char_type translate_ascii(char8_t value) noexcept
{
	if (u8'0' <= value && value <= u8'9')
	{
		return ::fast_io::char_literal_add<char_type>(value - u8'0');
	}
	switch (value)
	{
	case u8'.':
		return ::fast_io::char_literal_v<u8'.', char_type>;
	case u8',':
		return ::fast_io::char_literal_v<u8',', char_type>;
	case u8'+':
		return ::fast_io::char_literal_v<u8'+', char_type>;
	case u8'-':
		return ::fast_io::char_literal_v<u8'-', char_type>;
	case u8'e':
		return ::fast_io::char_literal_v<u8'e', char_type>;
	default:
		return char_type{};
	}
}

template <typename char_type, typename printable_type, ::std::size_t size>
[[nodiscard]] bool check_spelling(
	printable_type const &value, char8_t const (&expected)[size]) noexcept
{
	using tag = ::fast_io::io_reserve_type_t<char_type, printable_type>;
	constexpr auto expected_size{size - 1u};
	auto const matches = [&](char_type const *storage,
							 char_type const *end) noexcept {
		if (end != storage + expected_size)
		{
			return false;
		}
		for (::std::size_t index{}; index != expected_size; ++index)
		{
			if (storage[index] != translate_ascii<char_type>(expected[index]))
			{
				return false;
			}
		}
		return true;
	};
	char_type storage[2048u]{};
	auto *const end{
		print_reserve_define(tag{}, storage, value)};
	if (!matches(storage, end) ||
		print_reserve_precise_size(tag{}, value) != expected_size)
	{
		return false;
	}
	char_type precise[2048u]{};
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise, expected_size, value)};
	if (!matches(precise, precise_end))
	{
		return false;
	}
	char_type exact_fit[2048u]{};
	auto const exact_result{::fast_io::to_chars(
		exact_fit, exact_fit + expected_size, value)};
	if (exact_result.ec != ::std::errc{} ||
		exact_result.ptr != exact_fit + expected_size ||
		!matches(exact_fit, exact_result.ptr))
	{
		return false;
	}
	if constexpr (expected_size != 0u)
	{
		constexpr auto marker{::fast_io::char_literal_v<u8'?', char_type>};
		char_type rejected[2048u];
		for (::std::size_t index{}; index != expected_size + 1u; ++index)
		{
			rejected[index] = marker;
		}
		auto const rejected_result{::fast_io::to_chars(
			rejected, rejected + expected_size - 1u, value)};
		if (rejected_result.ec != ::std::errc::value_too_large ||
			rejected_result.ptr != rejected + expected_size - 1u)
		{
			return false;
		}
		for (::std::size_t index{}; index != expected_size + 1u; ++index)
		{
			if (rejected[index] != marker)
			{
				return false;
			}
		}
	}
	return true;
}

template <typename char_type>
[[nodiscard]] bool check_character_type() noexcept
{
	using namespace ::fast_io::mnp;
	constexpr double source{0.1};
	auto const exact{exact_decimal(source)};
	auto const range{precision_range(decimal(source), 3u, 17u)};
	auto const comma_range{
		precision_range(comma_decimal(source), 3u, 17u)};
	auto const scientific_range{
		precision_range(scientific(source), 3u, 17u)};
	return check_spelling<char_type>(
			   exact,
			   u8"0.1000000000000000055511151231257827021181583404541015625") &&
		   check_spelling<char_type>(range, u8"0.100") &&
		   check_spelling<char_type>(comma_range, u8"0,100") &&
		   check_spelling<char_type>(scientific_range, u8"1.00e-01");
}

} // namespace

int main()
{
	return !(check_character_type<char>() &&
			 check_character_type<wchar_t>() &&
			 check_character_type<char8_t>() &&
			 check_character_type<char16_t>() &&
			 check_character_type<char32_t>());
}
