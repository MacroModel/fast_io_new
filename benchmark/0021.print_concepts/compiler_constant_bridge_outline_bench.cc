#include <fast_io.h>
#include <fast_io_format.h>

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace compiler_constant_bridge_outline_bench
{

#if defined(__GNUC__) && !defined(__clang__)
#define FAST_IO_BRIDGE_BENCH_NOINLINE [[gnu::noinline, gnu::noipa]]
#else
#define FAST_IO_BRIDGE_BENCH_NOINLINE [[gnu::noinline]]
#endif

FAST_IO_BRIDGE_BENCH_NOINLINE char *runtime_int(
	char *buffer, ::std::uint64_t value)
{
	::fast_io::obuffer_view output{buffer, buffer + 256u};
	::fast_io::fmt::print<"value={}">(output, value);
	return output.curr_ptr;
}

FAST_IO_BRIDGE_BENCH_NOINLINE char *runtime_float(
	char *buffer, double value)
{
	::fast_io::obuffer_view output{buffer, buffer + 256u};
	::fast_io::fmt::print<"value={:.6f}">(output, value);
	return output.curr_ptr;
}

template <::std::size_t index>
[[nodiscard]] consteval auto indexed_format()
{
	constexpr char digits[]{"0123456789abcdef"};
	char text[]{
		'f', digits[(index >> 4u) & 0x0fu], digits[index & 0x0fu],
		'=', '{', '}', '\0'};
	return ::fast_io::fmt::basic_fixed_string{text};
}

template <::std::size_t index>
FAST_IO_BRIDGE_BENCH_NOINLINE char *runtime_indexed_format(
	char *buffer, ::std::uint64_t value)
{
	::fast_io::obuffer_view output{buffer, buffer + 256u};
	if constexpr ((index & 1u) == 0u)
	{
		::fast_io::fmt::print<indexed_format<index>()>(output, value);
	}
	else
	{
		::fast_io::fmt::print<indexed_format<index>()>(
			output, static_cast<::std::int64_t>(value));
	}
	return output.curr_ptr;
}

using indexed_function = char *(*)(char *, ::std::uint64_t);

template <::std::size_t... index>
[[nodiscard]] consteval auto make_indexed_functions(
	::std::index_sequence<index...>)
{
	return ::std::array<indexed_function, sizeof...(index)>{
		&runtime_indexed_format<index>...};
}

inline constexpr auto indexed_functions{
	make_indexed_functions(::std::make_index_sequence<128u>{})};

void single_runtime_int(::benchmark::State &state)
{
	alignas(64) char buffer[256u];
	::std::uint64_t value{0x123456789abcdef0ull};
	for (auto _ : state)
	{
		(void)_;
		value = value * 0x9e3779b97f4a7c15ull + 0xda3e39cb94b95bdbull;
		char *const end{runtime_int(buffer, value)};
		::benchmark::DoNotOptimize(end);
		::benchmark::DoNotOptimize(buffer[0]);
	}
}

void single_runtime_float(::benchmark::State &state)
{
	alignas(64) char buffer[256u];
	double value{12345.125};
	for (auto _ : state)
	{
		(void)_;
		value += 0.0009765625;
		if (value > 12400.0)
		{
			value = 12345.125;
		}
		char *const end{runtime_float(buffer, value)};
		::benchmark::DoNotOptimize(end);
		::benchmark::DoNotOptimize(buffer[0]);
	}
}

void random_128_format_working_set(::benchmark::State &state)
{
	alignas(64) char buffer[256u];
	::std::uint64_t random{0x243f6a8885a308d3ull};
	for (auto _ : state)
	{
		(void)_;
		random = random * 0x9e3779b97f4a7c15ull + 0xda3e39cb94b95bdbull;
		auto const function{indexed_functions[(random >> 57u) & 127u]};
		char *const end{function(buffer, random)};
		::benchmark::DoNotOptimize(end);
		::benchmark::DoNotOptimize(buffer[0]);
	}
}

BENCHMARK(single_runtime_int);
BENCHMARK(single_runtime_float);
BENCHMARK(random_128_format_working_set);

#undef FAST_IO_BRIDGE_BENCH_NOINLINE

} // namespace compiler_constant_bridge_outline_bench

BENCHMARK_MAIN();
