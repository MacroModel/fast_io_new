#include <benchmark/benchmark.h>

#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

namespace
{

float volatile runtime_float{12345.6789f};

[[nodiscard]] inline double finite_binary32_to_binary64(float value) noexcept
{
	auto const source{::std::bit_cast<::std::uint_least32_t>(value)};
	auto const mantissa{source & 0x7fffffu};
	auto const exponent{(source >> 23u) & 0xffu};
	::std::uint_least64_t destination{
		static_cast<::std::uint_least64_t>(source >> 31u) << 63u};
	if (exponent)
	{
		destination |= static_cast<::std::uint_least64_t>(exponent + 896u) << 52u;
		destination |= static_cast<::std::uint_least64_t>(mantissa) << 29u;
	}
	else if (mantissa)
	{
		auto const leading{static_cast<::std::uint_least32_t>(
			::std::bit_width(mantissa) - 1u)};
		destination |= static_cast<::std::uint_least64_t>(leading + 874u) << 52u;
		destination |= static_cast<::std::uint_least64_t>(
			mantissa ^ (static_cast<::std::uint_least32_t>(1u) << leading))
			<< (52u - leading);
	}
	return ::std::bit_cast<double>(destination);
}

template <::fast_io::fmt::basic_fixed_string format_literal, bool promote>
void precision_field(benchmark::State &state)
{
	auto const value{static_cast<float>(runtime_float)};
	auto const reference{::fast_io::fmt::concat_std<format_literal>(value)};
	alignas(64) ::std::array<char, 2048u> storage{};
	{
		::fast_io::obuffer_view output{storage};
		if constexpr (promote)
		{
			::fast_io::fmt::print<format_literal>(
				output, finite_binary32_to_binary64(value));
		}
		else
		{
			::fast_io::fmt::print<format_literal>(output, value);
		}
		if (output.size() != reference.size() ||
			::std::string_view{output.data(), output.size()} != reference)
		{
			::std::abort();
		}
	}
	for (auto _ : state)
	{
		(void)_;
		::fast_io::obuffer_view output{storage};
		auto const current{static_cast<float>(runtime_float)};
		if constexpr (promote)
		{
			::fast_io::fmt::print<format_literal>(
				output, finite_binary32_to_binary64(current));
		}
		else
		{
			::fast_io::fmt::print<format_literal>(output, current);
		}
		benchmark::DoNotOptimize(output.size());
		asm volatile("" : : "m"(storage) : "memory");
		benchmark::ClobberMemory();
	}
}

void baseline_internal(benchmark::State &state)
{
	precision_field<"{:+020.6f}", false>(state);
}

void promoted_internal(benchmark::State &state)
{
	precision_field<"{:+020.6f}", true>(state);
}

void baseline_no_width(benchmark::State &state)
{
	precision_field<"{:+.6f}", false>(state);
}

void promoted_no_width(benchmark::State &state)
{
	precision_field<"{:+.6f}", true>(state);
}

#define FAST_IO_BINARY32_PROMOTION_BENCHMARK(function_name, benchmark_name) \
	BENCHMARK(function_name)                                                  \
		->Name(benchmark_name)                                                \
		->MinTime(0.2)                                                       \
		->Repetitions(11)                                                    \
		->ReportAggregatesOnly(true)

FAST_IO_BINARY32_PROMOTION_BENCHMARK(
	baseline_internal, "binary32_precision/baseline_internal");
FAST_IO_BINARY32_PROMOTION_BENCHMARK(
	promoted_internal, "binary32_precision/promoted_internal");
FAST_IO_BINARY32_PROMOTION_BENCHMARK(
	baseline_no_width, "binary32_precision/baseline_no_width");
FAST_IO_BINARY32_PROMOTION_BENCHMARK(
	promoted_no_width, "binary32_precision/promoted_no_width");

#undef FAST_IO_BINARY32_PROMOTION_BENCHMARK

} // namespace

BENCHMARK_MAIN();
