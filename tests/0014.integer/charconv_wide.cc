#include <fast_io_core.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <system_error>
#include <type_traits>

template <typename char_type, typename integer_type>
inline void check_value(integer_type value, int base)
{
	std::array<char_type, 192> buffer{};
	buffer.fill(static_cast<char_type>(0x5a));
	auto const formatted{fast_io::to_chars(buffer.data(), buffer.data() + buffer.size(), value, base)};
	if (formatted.ec != std::errc{} || formatted.ptr == buffer.data())
	{
		std::abort();
	}
	for (auto const *iter{formatted.ptr}; iter != buffer.data() + buffer.size(); ++iter)
	{
		if (*iter != static_cast<char_type>(0x5a))
		{
			std::abort();
		}
	}

	integer_type parsed{static_cast<integer_type>(42)};
	auto const scanned{fast_io::from_chars(buffer.data(), formatted.ptr, parsed, base)};
	if (scanned.ec != std::errc{} || scanned.ptr != formatted.ptr || parsed != value)
	{
		std::abort();
	}

	auto const length{static_cast<std::size_t>(formatted.ptr - buffer.data())};
	constexpr std::array<std::size_t, 8> extras{0u, 1u, 2u, 3u, 7u, 8u, 15u, 31u};
	for (auto const extra : extras)
	{
		std::array<char_type, 192> bounded_buffer{};
		bounded_buffer.fill(static_cast<char_type>(0x5a));
		auto const bounded_result{fast_io::to_chars(
			bounded_buffer.data(), bounded_buffer.data() + length + extra, value, base)};
		if (bounded_result.ec != std::errc{} ||
			bounded_result.ptr != bounded_buffer.data() + length)
		{
			std::abort();
		}
		for (std::size_t index{}; index != length; ++index)
		{
			if (bounded_buffer[index] != buffer[index])
			{
				std::abort();
			}
		}
		for (std::size_t index{length}; index != bounded_buffer.size(); ++index)
		{
			if (bounded_buffer[index] != static_cast<char_type>(0x5a))
			{
				std::abort();
			}
		}
	}

	std::array<char_type, 192> short_buffer{};
	short_buffer.fill(static_cast<char_type>(0x5a));
	auto const short_result{
		fast_io::to_chars(short_buffer.data(), short_buffer.data() + length - 1u, value, base)};
	if (short_result.ec != std::errc::value_too_large ||
		short_result.ptr != short_buffer.data() + length - 1u)
	{
		std::abort();
	}
	for (auto const code_unit : short_buffer)
	{
		if (code_unit != static_cast<char_type>(0x5a))
		{
			std::abort();
		}
	}
}

template <typename char_type>
inline void check_character()
{
	for (int base{2}; base != 37; ++base)
	{
		check_value<char_type>(std::uint64_t{}, base);
		check_value<char_type>(std::numeric_limits<std::uint64_t>::max(), base);
		check_value<char_type>(std::numeric_limits<std::int64_t>::min(), base);
		check_value<char_type>(std::numeric_limits<std::int64_t>::max(), base);
		check_value<char_type>(static_cast<std::int64_t>(-123456789), base);
	}
}

template <typename char_type>
consteval bool check_constant_evaluation()
{
	char_type buffer[64]{};
	auto const formatted{fast_io::to_chars(buffer, buffer + 64, std::int64_t{-123456789}, 36)};
	if (formatted.ec != std::errc{} || formatted.ptr == buffer)
	{
		return false;
	}
	std::int64_t value{};
	auto const scanned{fast_io::from_chars(buffer, formatted.ptr, value, 36)};
	return scanned.ec == std::errc{} && scanned.ptr == formatted.ptr &&
		   value == std::int64_t{-123456789};
}

template <typename char_type>
consteval bool check_constant_hex_evaluation()
{
	char_type buffer[16]{};
	auto const formatted{fast_io::to_chars(
		buffer, buffer + 16, std::numeric_limits<std::uint64_t>::max(), 16)};
	if (formatted.ec != std::errc{} || formatted.ptr != buffer + 16)
	{
		return false;
	}
	for (auto const code_unit : buffer)
	{
		if (code_unit != fast_io::char_literal_v<u8'f', char_type>)
		{
			return false;
		}
	}
	return true;
}

template <typename char_type>
consteval bool check_constant_binary_evaluation()
{
	char_type buffer[65]{};
	for (std::size_t index{}; index != 64u; ++index)
	{
		buffer[index] = fast_io::char_literal_v<u8'1', char_type>;
	}
	buffer[64] = fast_io::char_literal_v<u8'#', char_type>;
	std::uint64_t value{};
	auto const scanned{fast_io::from_chars(buffer, buffer + 65, value, 2)};
	return scanned.ec == std::errc{} && scanned.ptr == buffer + 64 &&
		   value == std::numeric_limits<std::uint64_t>::max();
}

template <typename char_type>
consteval bool check_constant_decimal_tail_evaluation()
{
	char_type buffer[8]{};
	for (auto &code_unit : buffer)
	{
		code_unit = static_cast<char_type>(0x5a);
	}
	auto const formatted{fast_io::to_chars(buffer, buffer + 8, std::int16_t{}, 10)};
	if (formatted.ec != std::errc{} || formatted.ptr != buffer + 1 ||
		buffer[0] != fast_io::char_literal_v<u8'0', char_type>)
	{
		return false;
	}
	for (::std::size_t index{1u}; index != 8u; ++index)
	{
		if (buffer[index] != static_cast<char_type>(0x5a))
		{
			return false;
		}
	}
	return true;
}

static_assert(check_constant_evaluation<char>());
static_assert(check_constant_evaluation<wchar_t>());
static_assert(check_constant_evaluation<char8_t>());
static_assert(check_constant_evaluation<char16_t>());
static_assert(check_constant_evaluation<char32_t>());
static_assert(check_constant_hex_evaluation<char>());
static_assert(check_constant_hex_evaluation<wchar_t>());
static_assert(check_constant_hex_evaluation<char8_t>());
static_assert(check_constant_hex_evaluation<char16_t>());
static_assert(check_constant_hex_evaluation<char32_t>());
static_assert(check_constant_binary_evaluation<char>());
static_assert(check_constant_binary_evaluation<wchar_t>());
static_assert(check_constant_binary_evaluation<char8_t>());
static_assert(check_constant_binary_evaluation<char16_t>());
static_assert(check_constant_binary_evaluation<char32_t>());
static_assert(check_constant_decimal_tail_evaluation<char>());
static_assert(check_constant_decimal_tail_evaluation<wchar_t>());
static_assert(check_constant_decimal_tail_evaluation<char8_t>());
static_assert(check_constant_decimal_tail_evaluation<char16_t>());
static_assert(check_constant_decimal_tail_evaluation<char32_t>());
static_assert(std::same_as<fast_io::basic_from_chars_result<char>, std::from_chars_result>);
static_assert(std::same_as<fast_io::basic_to_chars_result<char>, std::to_chars_result>);

int main()
{
	char explicit_buffer[64]{};
	auto const explicit_format{
		fast_io::to_chars<std::uint64_t>(explicit_buffer, explicit_buffer + 64, UINT64_C(123456789), 10)};
	std::uint64_t explicit_value{};
	auto const explicit_scan{fast_io::from_chars<std::uint64_t>(
		explicit_buffer, explicit_format.ptr, explicit_value, 10)};
	if (explicit_format.ec != std::errc{} || explicit_scan.ec != std::errc{} ||
		explicit_scan.ptr != explicit_format.ptr || explicit_value != UINT64_C(123456789))
	{
		std::abort();
	}

	check_character<char>();
	check_character<wchar_t>();
	check_character<char8_t>();
	check_character<char16_t>();
	check_character<char32_t>();
}
