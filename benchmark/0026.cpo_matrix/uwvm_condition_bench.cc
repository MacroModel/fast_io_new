// Compare baseline/candidate with identical -std=c++26 -O3 -march=native flags.
// The synchronous memory sink measures formatter/planner work without syscall noise.
// Usage: executable [iterations] [independent|correlated|off|on] [null|valid]
// Correlated chooses all-on/all-off independently each record; off/on keep a
// stable mask to model the usual unchanged terminal-color setting. Short forms
// i/c/v from earlier runs remain accepted.
// -DUWVM_BENCH_SHARED_PREDICATE=1 uses the same mask!=0 expression for all eight
// conditions, exposing the real shared put_color property to the optimizer.
// -DUWVM_BENCH_TIMESTAMP=1 appends a fixed real iso8601_timestamp, covering the
// mandatory reserve/context path without reading a clock inside the loop.
#ifndef UWVM_BENCH_SHARED_PREDICATE
#define UWVM_BENCH_SHARED_PREDICATE 0
#endif
#ifndef UWVM_BENCH_TIMESTAMP
#define UWVM_BENCH_TIMESTAMP 0
#endif
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#include <uwvm2/uwvm/utils/memory/print.h>

namespace uwvm_condition_bench
{
struct state
{
	char8_t data[4096];
	::std::size_t size{};
	::std::size_t calls{};
};
struct output
{
	using output_char_type = char8_t;
	state *value;
};
inline constexpr output output_stream_ref_define(output out) noexcept
{
	return out;
}
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(::fast_io::io_reserve_type_t<char8_t, output>) noexcept
{
	return {};
}
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(::fast_io::io_reserve_type_t<char8_t, output>) noexcept
{
	return {};
}
inline void append(output out, char8_t const *first, ::std::size_t size)
{
	if (size > sizeof(out.value->data) - out.value->size)
	{
		::std::abort();
	}
	if (size != 0u)
	{
		::std::memcpy(out.value->data + out.value->size, first, size);
	}
	out.value->size += size;
}
inline void write_all_overflow_define(output out, char8_t const *first, char8_t const *last)
{
	++out.value->calls;
	append(out, first, static_cast<::std::size_t>(last - first));
}
inline void scatter_write_all_overflow_define(output out, ::fast_io::basic_io_scatter_t<char8_t> const *scatters, ::std::size_t count)
{
	++out.value->calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		append(out, scatters[index].base, scatters[index].len);
	}
}
inline constexpr bool predicate(unsigned mask, [[maybe_unused]] unsigned bit) noexcept
{
#if UWVM_BENCH_SHARED_PREDICATE
	return mask != 0u;
#else
	return (mask & bit) != 0u;
#endif
}
[[gnu::noinline]] void emit(output out, unsigned mask, ::uwvm2::uwvm::utils::memory::print_memory const &source
#if UWVM_BENCH_TIMESTAMP
							,
							::fast_io::iso8601_timestamp const &timestamp
#endif
)
{
	::fast_io::operations::print_freestanding<false>(out,
													 ::fast_io::mnp::cond(predicate(mask, 1u), u8"\033[31m"), u8"field0: ",
													 ::fast_io::mnp::cond(predicate(mask, 2u), u8"\033[32m"), u8"field1: ",
													 ::fast_io::mnp::cond(predicate(mask, 4u), u8"\033[33m"), u8"field2: ",
													 ::fast_io::mnp::cond(predicate(mask, 8u), u8"\033[34m"), u8"field3: ",
													 ::fast_io::mnp::cond(predicate(mask, 16u), u8"\033[35m"), u8"field4: ",
													 ::fast_io::mnp::cond(predicate(mask, 32u), u8"\033[36m"), u8"field5: ",
													 ::fast_io::mnp::cond(predicate(mask, 64u), u8"\033[37m"), u8"field6: ",
													 ::fast_io::mnp::cond(predicate(mask, 128u), u8"\033[0m"), u8"field7: ", source,
#if UWVM_BENCH_TIMESTAMP
													 timestamp,
#endif
													 u8"\n");
}
} // namespace uwvm_condition_bench
int main(int argc, char **argv)
{
	using namespace uwvm_condition_bench;
	auto const iterations{argc > 1 ? ::std::strtoull(argv[1], nullptr, 10) : 1000000ull};
	if (iterations == 0u)
	{
		::std::fputs("iterations must be positive\n", stderr);
		return 2;
	}
	enum class mask_mode
	{
		independent,
		correlated,
		off,
		on
	};
	mask_mode mode{mask_mode::independent};
	char const *mode_name{"independent"};
	if (argc > 2)
	{
		if (::std::strcmp(argv[2], "off") == 0)
		{
			mode = mask_mode::off;
			mode_name = "off";
		}
		else if (::std::strcmp(argv[2], "on") == 0)
		{
			mode = mask_mode::on;
			mode_name = "on";
		}
		else if (::std::strcmp(argv[2], "c") == 0 || ::std::strcmp(argv[2], "correlated") == 0)
		{
			mode = mask_mode::correlated;
			mode_name = "correlated";
		}
		else if (::std::strcmp(argv[2], "i") != 0 && ::std::strcmp(argv[2], "independent") != 0)
		{
			::std::fputs("mode must be independent, correlated, off, or on\n", stderr);
			return 2;
		}
	}
	auto const valid_memory{argc > 3 && argv[3][0] == 'v'};
	::std::array<::std::byte, 32> bytes{};
	for (::std::size_t index{}; index != bytes.size(); ++index)
	{
		bytes[index] = static_cast<::std::byte>(index);
	}
	auto const source{valid_memory ? ::uwvm2::uwvm::utils::memory::print_memory{bytes.data(), bytes.data() + 8, bytes.data() + bytes.size()} : ::uwvm2::uwvm::utils::memory::print_memory{}};
#if UWVM_BENCH_TIMESTAMP
	constexpr ::fast_io::iso8601_timestamp timestamp{2026, 9u, 5u, 12u, 34u, 56u, 0u, 0};
#endif
	state storage{};
	unsigned random{123456789u};
	unsigned long long checksum{};
	unsigned long long content_sample{};
	auto const begin{::std::chrono::steady_clock::now()};
	for (unsigned long long iteration{}; iteration != iterations; ++iteration)
	{
		random = random * 1664525u + 1013904223u;
		unsigned const mask{mode == mask_mode::off ? 0u : mode == mask_mode::on       ? 255u
													  : mode == mask_mode::correlated ? ((random >> 31u) != 0u ? 255u : 0u)
																					  : random >> 24u};
		storage.size = 0u;
		emit(output{&storage}, mask, source
#if UWVM_BENCH_TIMESTAMP
			 ,
			 timestamp
#endif
		);
		asm volatile("" : : "r"(storage.data) : "memory");
		checksum += storage.size;
		// Sample actual materialized bytes only once per 256 records. Full
		// byte/operation correctness is covered by the separate contract;
		// this inexpensive sample keeps the timing primarily formatter work.
		if ((iteration & 255u) == 0u && storage.size != 0u)
		{
			auto const sample{static_cast<unsigned long long>(storage.data[0]) |
							  (static_cast<unsigned long long>(storage.data[storage.size / 2u]) << 8u) |
							  (static_cast<unsigned long long>(storage.data[storage.size - 1u]) << 16u)};
			content_sample = content_sample * 1099511628211ull + sample;
		}
	}
	auto const elapsed{::std::chrono::duration_cast<::std::chrono::nanoseconds>(::std::chrono::steady_clock::now() - begin).count()};
	::std::printf("%.3f ns/record checksum=%llu calls=%zu sample=%llu mode=%s predicate=%s timestamp=%u\n",
				  static_cast<double>(elapsed) / static_cast<double>(iterations), checksum, storage.calls, content_sample, mode_name,
				  UWVM_BENCH_SHARED_PREDICATE ? "shared" : "independent", static_cast<unsigned>(UWVM_BENCH_TIMESTAMP != 0));
	return 0;
}
