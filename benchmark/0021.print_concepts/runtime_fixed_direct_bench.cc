#include <benchmark/benchmark.h>

#include <fast_io.h>
#include <fast_io_format.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <string_view>
#include <unistd.h>

namespace
{

float volatile runtime_float{12345.6789f};
double volatile runtime_double{12345.6789012345};
unsigned volatile runtime_width{20u};
unsigned volatile runtime_precision{6u};

struct fd_owner
{
	int fd{-1};

	fd_owner()
	{
		fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
		if (fd < 0)
		{
			::std::abort();
		}
	}

	fd_owner(fd_owner const &) = delete;
	fd_owner &operator=(fd_owner const &) = delete;

	~fd_owner()
	{
		if (fd >= 0)
		{
			::close(fd);
		}
	}
};

template <::fast_io::fmt::basic_fixed_string format_literal,
	typename value_type, auto source>
void static_field(benchmark::State &state)
{
	std::array<char, 2048u> verification{};
	::fast_io::obuffer_view memory_output{verification};
	::fast_io::fmt::print<format_literal>(
		memory_output, static_cast<value_type>(*source));
	auto const expected{::fast_io::fmt::concat_std<format_literal>(
		static_cast<value_type>(*source))};
	if (memory_output.size() != expected.size() ||
		std::string_view{memory_output.data(), memory_output.size()} != expected)
	{
		::std::abort();
	}

	fd_owner owner;
	::fast_io::posix_io_observer output{owner.fd};
	for (auto _ : state)
	{
		(void)_;
		::fast_io::fmt::print<format_literal>(
			output, static_cast<value_type>(*source));
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal>
void dynamic_field(benchmark::State &state)
{
	std::array<char, 2048u> verification{};
	::fast_io::obuffer_view memory_output{verification};
	auto const value{static_cast<double>(runtime_double)};
	auto const width{static_cast<unsigned>(runtime_width)};
	auto const precision{static_cast<unsigned>(runtime_precision)};
	::fast_io::fmt::print<format_literal>(
		memory_output, value, width, precision);
	auto const expected{
		::fast_io::fmt::concat_std<format_literal>(value, width, precision)};
	if (memory_output.size() != expected.size() ||
		std::string_view{memory_output.data(), memory_output.size()} != expected)
	{
		::std::abort();
	}

	fd_owner owner;
	::fast_io::posix_io_observer output{owner.fd};
	for (auto _ : state)
	{
		(void)_;
		::fast_io::fmt::print<format_literal>(
			output, static_cast<double>(runtime_double),
			static_cast<unsigned>(runtime_width),
			static_cast<unsigned>(runtime_precision));
	}
}

void internal_float(benchmark::State &state)
{
	static_field<"i = {:020.6f}", float, &runtime_float>(state);
}

void internal_double(benchmark::State &state)
{
	static_field<"i = {:020.6f}", double, &runtime_double>(state);
}

void no_width_double(benchmark::State &state)
{
	static_field<"i = {:.6f}", double, &runtime_double>(state);
}

void dynamic_double(benchmark::State &state)
{
	dynamic_field<"i = {0:0{1}.{2}f}">(state);
}

#define FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK(function_name, benchmark_name) \
	BENCHMARK(function_name)                                                    \
		->Name(benchmark_name)                                                   \
		->MinTime(0.1)                                                          \
		->Repetitions(9)                                                        \
		->ReportAggregatesOnly(true)

FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK(
	internal_float, "runtime-fixed-direct/internal-float");
FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK(
	internal_double, "runtime-fixed-direct/internal-double");
FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK(
	no_width_double, "runtime-fixed-direct/no-width-double");
FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK(
	dynamic_double, "runtime-fixed-direct/dynamic-double");

#undef FAST_IO_RUNTIME_FIXED_DIRECT_BENCHMARK

} // namespace

BENCHMARK_MAIN();
