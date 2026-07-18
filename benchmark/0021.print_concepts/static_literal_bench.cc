#include <fast_io_core.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace
{

struct static_literal_bench_sink
{
	using output_char_type = char;
};

inline constexpr static_literal_bench_sink
output_stream_ref_define(static_literal_bench_sink sink) noexcept
{
	return sink;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline void write_all_overflow_define(
	static_literal_bench_sink, char const *first, char const *last) noexcept
{
	// Observe only the completed range. The timed operation retains print composition, materialization, its wrapper
	// call, and this opaque call, but deliberately excludes a syscall, checksum, or second payload copy.
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
void split_once()
{
	::fast_io::operations::print_freestanding<false>(
		static_literal_bench_sink{}, "a", "bbb", ::fast_io::mnp::chvw('c'));
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
void whole_once()
{
	::fast_io::operations::print_freestanding<false>(
		static_literal_bench_sink{}, "abbbc");
}

[[nodiscard]] inline ::std::uint64_t monotonic_nanoseconds() noexcept
{
	::timespec value{};
#if defined(CLOCK_MONOTONIC_RAW)
	constexpr auto clock_id{CLOCK_MONOTONIC_RAW};
#else
	constexpr auto clock_id{CLOCK_MONOTONIC};
#endif
	if (::clock_gettime(clock_id, __builtin_addressof(value)) != 0) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return static_cast<::std::uint64_t>(value.tv_sec) * 1000000000u +
		   static_cast<::std::uint64_t>(value.tv_nsec);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2 || (argv[1][0] != 's' && argv[1][0] != 'w') || argv[1][1] != '\0')
	{
		::std::fputs("usage: static_literal_bench {s|w}\n", stderr);
		return 2;
	}
	bool const split{argv[1][0] == 's'};
	auto const operation{split ? split_once : whole_once};
	constexpr ::std::size_t warmup_iterations{10000u};
	constexpr ::std::size_t measured_iterations{100000000u};
	for (::std::size_t iteration{}; iteration != warmup_iterations; ++iteration)
	{
		operation();
	}
	auto const begin{monotonic_nanoseconds()};
	for (::std::size_t iteration{}; iteration != measured_iterations; ++iteration)
	{
		operation();
	}
	auto const end{monotonic_nanoseconds()};
	auto const nanoseconds{static_cast<double>(end - begin)};
	::std::printf("%s %zu %.9f ns/op\n", split ? "split" : "whole", measured_iterations,
				  nanoseconds / static_cast<double>(measured_iterations));
}
