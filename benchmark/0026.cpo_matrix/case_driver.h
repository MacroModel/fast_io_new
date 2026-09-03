#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/time.h>
#endif

namespace fast_io_cpo_matrix
{

class process_deadline_guard
{
#if defined(__unix__) || defined(__APPLE__)
	bool armed_{};
#endif

public:
	explicit process_deadline_guard(
		::std::uint_least64_t deadline_microseconds = UINT64_C(800000)) noexcept
	{
#if defined(__unix__) || defined(__APPLE__)
		/*
		ITIMER_REAL is a process-local hard boundary, so it adds no watchdog
		thread to the single-task M4 run.  The measured interval remains governed
		by steady_clock; this independent timer exists only to terminate a broken
		pilot, unexpectedly slow preflight, or scheduler stall before one process
		violates the sub-second experiment contract.
		*/
		::itimerval timer{};
		timer.it_value.tv_sec = static_cast<decltype(timer.it_value.tv_sec)>(
			deadline_microseconds / UINT64_C(1000000));
		timer.it_value.tv_usec = static_cast<decltype(timer.it_value.tv_usec)>(
			deadline_microseconds % UINT64_C(1000000));
		armed_ = ::setitimer(ITIMER_REAL, __builtin_addressof(timer), nullptr) == 0;
#else
		(void)deadline_microseconds;
#endif
	}

	process_deadline_guard(process_deadline_guard const &) = delete;
	process_deadline_guard &operator=(process_deadline_guard const &) = delete;

	~process_deadline_guard()
	{
#if defined(__unix__) || defined(__APPLE__)
		if (armed_)
		{
			::itimerval timer{};
			(void)::setitimer(ITIMER_REAL, __builtin_addressof(timer), nullptr);
		}
#endif
	}

	[[nodiscard]] bool armed() const noexcept
	{
#if defined(__unix__) || defined(__APPLE__)
		return armed_;
#else
		return false;
#endif
	}
};

inline void compiler_observe_bytes(
	void const *data, ::std::size_t size) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	/*
	The pointer and length make the produced interval addressable to opaque code,
	while the memory clobber prevents stores preceding the barrier from being
	discarded or moved beyond it.  The barrier performs no byte traversal, so the
	timed region remains an output/materialization measurement rather than a hash
	benchmark.  Full-byte semantic validation is performed separately.
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
		if (result > ((::std::numeric_limits<::std::uint_least64_t>::max)() -
					  digit) /
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

template <typename Function>
[[nodiscard]] inline ::std::pair<::std::uint_least64_t, ::std::uint_least64_t>
run_iterations(Function &function, ::std::size_t iterations)
{
	auto const start{::std::chrono::steady_clock::now()};
	::std::uint_least64_t checksum{};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		checksum += static_cast<::std::uint_least64_t>(function(index));
		checksum ^= static_cast<::std::uint_least64_t>(index) << (index & 7u);
	}
	auto const finish{::std::chrono::steady_clock::now()};
	auto const elapsed{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
						   finish - start)
						   .count()};
	return {static_cast<::std::uint_least64_t>(elapsed), checksum};
}

template <typename Function, typename Reset>
[[nodiscard]] inline measurement calibrate_and_measure(
	Function &function, Reset &reset,
	::std::uint_least64_t target_milliseconds)
{
	static constexpr ::std::uint_least64_t minimum_pilot_ns{UINT64_C(1000000)};
	static constexpr ::std::size_t maximum_iterations{100000000u};
	::std::size_t pilot_iterations{16u};
	::std::uint_least64_t pilot_elapsed{};
	do
	{
		pilot_elapsed = run_iterations(function, pilot_iterations).first;
		if (pilot_elapsed >= minimum_pilot_ns ||
			pilot_iterations >= maximum_iterations / 2u)
		{
			break;
		}
		pilot_iterations *= 2u;
	} while (true);
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
	auto const iterations{static_cast<::std::size_t>(scaled)};
	/*
	The pilot is an estimator, not part of the sample.  Resetting all mutable
	destination state makes the formal run begin from the same cursor, counters,
	and cache-address schedule for old and new even when their pilot iteration
	counts differ.  Stateless concat supplies a no-op reset with the same rule.
	*/
	reset();
	auto const [elapsed, checksum]{run_iterations(function, iterations)};
	return {iterations, elapsed, checksum};
}

} // namespace fast_io_cpo_matrix
