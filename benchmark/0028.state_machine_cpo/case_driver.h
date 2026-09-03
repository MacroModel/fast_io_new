#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fast_io_state_machine_cpo
{

inline void compiler_observe_bytes(
	void const *data, ::std::size_t size) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	/*
	The opaque address/extent pair keeps the complete produced interval live.
	The memory clobber is a compiler ordering boundary only: it performs no byte
	walk, so semantic comparison remains outside the measured region.
	*/
	__asm__ __volatile__("" : : "r"(data), "r"(size) : "memory");
#else
	(void)data;
	(void)size;
	::std::atomic_signal_fence(::std::memory_order_seq_cst);
#endif
}

[[nodiscard]] inline bool parse_unsigned(
	char const *text, ::std::uint_least64_t &value) noexcept
{
	if (text == nullptr || *text == '\0')
	{
		return false;
	}
	::std::uint_least64_t result{};
	for (; *text != '\0'; ++text)
	{
		auto const character{static_cast<unsigned char>(*text)};
		if (character < static_cast<unsigned char>('0') ||
			character > static_cast<unsigned char>('9'))
		{
			return false;
		}
		auto const digit{static_cast<unsigned>(
			character - static_cast<unsigned char>('0'))};
		if (result >
			((::std::numeric_limits<::std::uint_least64_t>::max)() - digit) /
				10u)
		{
			return false;
		}
		result = result * 10u + digit;
	}
	value = result;
	return true;
}

struct measurement
{
	::std::size_t iterations{};
	::std::uint_least64_t elapsed_nanoseconds{};
	::std::uint_least64_t checksum{};
};

template <typename function_type>
[[nodiscard]] inline measurement run_iterations(
	function_type &function, ::std::size_t iterations)
{
	auto checksum{UINT64_C(14695981039346656037)};
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const value{static_cast<::std::uint_least64_t>(function(index))};
		checksum = (checksum ^ value ^ static_cast<::std::uint_least64_t>(index)) *
				   UINT64_C(1099511628211);
	}
	auto const finish{::std::chrono::steady_clock::now()};
	auto const elapsed{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
						   finish - start)
						   .count()};
	return {iterations, static_cast<::std::uint_least64_t>(elapsed), checksum};
}

template <typename function_type>
[[nodiscard]] inline measurement calibrate_and_measure(
	function_type &function, ::std::uint_least64_t target_milliseconds)
{
	static constexpr ::std::uint_least64_t pilot_goal_nanoseconds{
		UINT64_C(1000000)};
	static constexpr ::std::size_t maximum_iterations{50000000u};
	::std::size_t pilot_iterations{1u};
	measurement pilot{};
	for (;;)
	{
		pilot = run_iterations(function, pilot_iterations);
		if (pilot.elapsed_nanoseconds >= pilot_goal_nanoseconds ||
			pilot_iterations >= maximum_iterations / 2u)
		{
			break;
		}
		pilot_iterations *= 2u;
	}
	auto pilot_elapsed{pilot.elapsed_nanoseconds};
	if (pilot_elapsed == 0u)
	{
		pilot_elapsed = 1u;
	}
	auto const target_nanoseconds{target_milliseconds * UINT64_C(1000000)};
	auto scaled{static_cast<long double>(target_nanoseconds) *
				static_cast<long double>(pilot_iterations) /
				static_cast<long double>(pilot_elapsed)};
	if (scaled < 1.0L)
	{
		scaled = 1.0L;
	}
	if (scaled > static_cast<long double>(maximum_iterations))
	{
		scaled = static_cast<long double>(maximum_iterations);
	}
	return run_iterations(function, static_cast<::std::size_t>(scaled));
}

[[nodiscard]] inline constexpr bool valid_target_milliseconds(
	::std::uint_least64_t value) noexcept
{
	/*
	A 200 ms ceiling leaves headroom for the approximately 1 ms pilot, process
	startup, and untimed oracle while keeping each executable below 0.8 seconds.
	*/
	return 20u <= value && value <= 200u;
}

} // namespace fast_io_state_machine_cpo
