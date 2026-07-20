#include <benchmark/benchmark.h>

#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>

namespace
{

float volatile runtime_float{12345.6789f};
double volatile runtime_double{12345.6789012345};
unsigned volatile runtime_width{20u};
unsigned volatile runtime_precision{6u};

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type>
void validate(value_type value)
{
	std::array<char, 2048u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::fmt::print<format_literal>(output, value);
	auto const reference{::fast_io::fmt::concat_std<format_literal>(value)};
	if (output.size() != reference.size() ||
		std::string_view{output.data(), output.size()} != reference)
	{
		std::abort();
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type, auto source>
void static_field(benchmark::State &state)
{
	validate<format_literal>(static_cast<value_type>(*source));
	alignas(64) std::array<char, 2048u> storage{};
	for (auto _ : state)
	{
		(void)_;
		::fast_io::obuffer_view output{storage};
		::fast_io::fmt::print<format_literal>(
			output, static_cast<value_type>(*source));
		benchmark::DoNotOptimize(output.size());
		asm volatile("" : : "m"(storage) : "memory");
		benchmark::ClobberMemory();
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type, auto source>
void static_printf_field(benchmark::State &state)
{
	auto const value{static_cast<value_type>(*source)};
	std::array<char, 2048u> validation_storage{};
	::fast_io::obuffer_view validation_output{validation_storage};
	::fast_io::fmt::printf<format_literal>(validation_output, value);
	auto const reference{::fast_io::fmt::concatf_std<format_literal>(value)};
	if (validation_output.size() != reference.size() ||
		std::string_view{validation_output.data(), validation_output.size()} !=
			reference)
	{
		std::abort();
	}
	alignas(64) std::array<char, 2048u> storage{};
	for (auto _ : state)
	{
		(void)_;
		::fast_io::obuffer_view output{storage};
		::fast_io::fmt::printf<format_literal>(
			output, static_cast<value_type>(*source));
		benchmark::DoNotOptimize(output.size());
		asm volatile("" : : "m"(storage) : "memory");
		benchmark::ClobberMemory();
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal>
void dynamic_fixed_field(benchmark::State &state)
{
	auto const value{static_cast<double>(runtime_double)};
	auto const width{static_cast<unsigned>(runtime_width)};
	auto const precision{static_cast<unsigned>(runtime_precision)};
	auto const reference{::fast_io::fmt::concat_std<format_literal>(
		value, width, precision)};
	alignas(64) std::array<char, 2048u> storage{};
	{
		::fast_io::obuffer_view output{storage};
		::fast_io::fmt::print<format_literal>(
			output, value, width, precision);
		if (output.size() != reference.size() ||
			std::string_view{output.data(), output.size()} != reference)
		{
			std::abort();
		}
	}
	for (auto _ : state)
	{
		(void)_;
		::fast_io::obuffer_view output{storage};
		::fast_io::fmt::print<format_literal>(
			output, static_cast<double>(runtime_double),
			static_cast<unsigned>(runtime_width),
			static_cast<unsigned>(runtime_precision));
		benchmark::DoNotOptimize(output.size());
		asm volatile("" : : "m"(storage) : "memory");
		benchmark::ClobberMemory();
	}
}

void dynamic_fixed(benchmark::State &state)
{
	dynamic_fixed_field<"{0:{1}.{2}f}">(state);
}

void dynamic_internal_fixed(benchmark::State &state)
{
	dynamic_fixed_field<"{0:+0{1}.{2}f}">(state);
}

void static_internal_float(benchmark::State &state)
{
	static_field<"{:+020.6f}", float, &runtime_float>(state);
}

void static_internal_double(benchmark::State &state)
{
	static_field<"{:+020.6f}", double, &runtime_double>(state);
}

void static_right_float(benchmark::State &state)
{
	static_field<"{:>20.6f}", float, &runtime_float>(state);
}

void static_right_double(benchmark::State &state)
{
	static_field<"{:>20.6f}", double, &runtime_double>(state);
}

void static_left_double(benchmark::State &state)
{
	static_field<"{:<20.6f}", double, &runtime_double>(state);
}

void static_center_double(benchmark::State &state)
{
	static_field<"{:*^20.6f}", double, &runtime_double>(state);
}

void static_insufficient_width_double(benchmark::State &state)
{
	static_field<"{:+5.6f}", double, &runtime_double>(state);
}

void static_internal_hex_double(benchmark::State &state)
{
	static_field<"{:+#020.6a}", double, &runtime_double>(state);
}

void static_no_width_double(benchmark::State &state)
{
	static_field<"{:+.6f}", double, &runtime_double>(state);
}

void static_no_width_float(benchmark::State &state)
{
	static_field<"{:+.6f}", float, &runtime_float>(state);
}

void static_printf_internal_double(benchmark::State &state)
{
	static_printf_field<"%+020.6f", double, &runtime_double>(state);
}

#define FAST_IO_STATIC_FIELD_BENCHMARK(function_name, benchmark_name) \
	BENCHMARK(function_name)                                          \
		->Name(benchmark_name)                                        \
		->MinTime(0.1)                                                \
		->Repetitions(9)                                              \
		->ReportAggregatesOnly(true)

FAST_IO_STATIC_FIELD_BENCHMARK(
	static_internal_float, "static_fixed/internal_float");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_internal_double, "static_fixed/internal_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_right_float, "static_fixed/right_float");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_right_double, "static_fixed/right_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_left_double, "static_fixed/left_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_center_double, "static_fixed/center_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_insufficient_width_double,
	"static_fixed/insufficient_width_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_internal_hex_double, "static_fixed/internal_hex_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_no_width_double, "static_fixed/no_width_double");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_no_width_float, "static_fixed/no_width_float");
FAST_IO_STATIC_FIELD_BENCHMARK(
	static_printf_internal_double, "printf_static_fixed/internal_double");

BENCHMARK(dynamic_fixed)
	->Name("dynamic_fixed/right_double")
	->MinTime(0.1)
	->Repetitions(9)
	->ReportAggregatesOnly(true);

BENCHMARK(dynamic_internal_fixed)
	->Name("dynamic_fixed/internal_double")
	->MinTime(0.1)
	->Repetitions(9)
	->ReportAggregatesOnly(true);

#undef FAST_IO_STATIC_FIELD_BENCHMARK

} // namespace

BENCHMARK_MAIN();
