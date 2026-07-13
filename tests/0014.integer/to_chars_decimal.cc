#include <fast_io_core.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <system_error>

inline int volatile runtime_decimal_base{10};

template <typename T>
inline void test_decimal(T value)
{
	char fast_buffer[64];
	char std_buffer[64];
	auto const fast_result{fast_io::to_chars(fast_buffer, fast_buffer + sizeof(fast_buffer), value)};
	auto const std_result{std::to_chars(std_buffer, std_buffer + sizeof(std_buffer), value)};
	std::size_t const length{static_cast<std::size_t>(std_result.ptr - std_buffer)};
	if (fast_result.ec != std_result.ec || fast_result.ptr - fast_buffer != std_result.ptr - std_buffer ||
		std::memcmp(fast_buffer, std_buffer, length) != 0)
	{
		std::abort();
	}
	auto const runtime_result{
		fast_io::to_chars(fast_buffer, fast_buffer + sizeof(fast_buffer), value, runtime_decimal_base)};
	if (runtime_result.ec != std_result.ec || runtime_result.ptr - fast_buffer != std_result.ptr - std_buffer ||
		std::memcmp(fast_buffer, std_buffer, length) != 0)
	{
		std::abort();
	}

	auto const exact_result{fast_io::to_chars(fast_buffer, fast_buffer + length, value)};
	if (exact_result.ec != std::errc{} || exact_result.ptr != fast_buffer + length ||
		std::memcmp(fast_buffer, std_buffer, length) != 0)
	{
		std::abort();
	}
	if (length != 0u)
	{
		auto const short_result{fast_io::to_chars(fast_buffer, fast_buffer + length - 1u, value)};
		if (short_result.ec != std::errc::value_too_large || short_result.ptr != fast_buffer + length - 1u)
		{
			std::abort();
		}
	}
}

inline std::uint_least64_t splitmix64(std::uint_least64_t &state) noexcept
{
	state += UINT64_C(0x9e3779b97f4a7c15);
	auto value{state};
	value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
	value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
	return value ^ (value >> 31u);
}

int main()
{
	test_decimal(UINT64_C(0));
	test_decimal(UINT64_MAX);
	test_decimal(INT64_MIN);
	test_decimal(INT64_MAX);

	std::uint_least64_t power{1u};
	for (std::size_t digits{1u}; digits != 20u; ++digits)
	{
		test_decimal(power - 1u);
		test_decimal(power);
		test_decimal(power + 1u);
		if (power > 9u)
		{
			test_decimal(power - 9u);
		}
		power *= 10u;
	}
	test_decimal(power - 1u);
	test_decimal(power);
	test_decimal(power + 1u);

	std::uint_least64_t state{UINT64_C(0x243f6a8885a308d3)};
	for (std::size_t index{}; index != 250000u; ++index)
	{
		auto const value{splitmix64(state)};
		test_decimal(value);
		test_decimal(static_cast<std::int_least64_t>(value));
	}
}
