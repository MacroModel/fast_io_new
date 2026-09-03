#include <fast_io.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_PADDING_BENCH_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_PADDING_BENCH_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_PADDING_BENCH_NOINLINE
#endif

inline constexpr auto decimal_flags{
	::fast_io::details::base_scan_mani_flags_cache<
		10u, true, false, false, false>};
using decimal_manipulator =
	::fast_io::manipulators::scalar_manip_t<
		decimal_flags, ::std::uint_least64_t &>;

FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t parse_integer_ordinary(
	char const *first, ::std::size_t semantic_size) noexcept
{
	::std::uint_least64_t value{};
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, decimal_manipulator>, first,
		first + semantic_size, decimal_manipulator{value})};
	return value ^
		   (static_cast<::std::uint_least64_t>(result.iter - first) << 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t parse_integer_padded(
	char const *first, ::std::size_t semantic_size) noexcept
{
	::std::uint_least64_t value{};
	auto const result{scan_contiguous_padding_define(
		::fast_io::io_reserve_type<char, decimal_manipulator>, first,
		first + semantic_size, 64u - semantic_size,
		decimal_manipulator{value})};
	return value ^
		   (static_cast<::std::uint_least64_t>(result.iter - first) << 48u) ^
		   (static_cast<::std::uint_least64_t>(result.code) << 56u);
}

FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t parse_integer_public_ordinary(
	char const *first, ::std::size_t semantic_size)
{
	::std::uint_least64_t value{};
	::fast_io::basic_ibuffer_view<char> input{
		first, first + semantic_size};
	auto const success{::fast_io::io::scan<true>(input, value)};
	return value ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 63u);
}

FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t parse_integer_public_padded(
	char const *first, ::std::size_t semantic_size)
{
	::std::uint_least64_t value{};
	::fast_io::basic_padded_ibuffer_view<char> input{
		first, first + semantic_size, 64u - semantic_size};
	auto const success{::fast_io::io::scan<true>(input, value)};
	return value ^
		   (static_cast<::std::uint_least64_t>(
				input.curr_ptr - first)
			<< 48u) ^
		   (static_cast<::std::uint_least64_t>(success) << 63u);
}

template <::std::size_t simd_size>
FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t scan_float_tail_ordinary(
	char const *first, ::std::size_t semantic_size) noexcept
{
	auto const *const last{first + semantic_size};
	bool tail_nonzero{};
	auto current{
		::fast_io::details::scan_decfloat_skip_after_exact_limit_simd<
			simd_size>(first, last, tail_nonzero)};
	char8_t digit{};
	for (; current != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(
			   *current, digit);
		 ++current)
	{
		if (!tail_nonzero && digit != 0u)
		{
			tail_nonzero = true;
		}
	}
	return static_cast<::std::uint_least64_t>(current - first) |
		   (static_cast<::std::uint_least64_t>(tail_nonzero) << 63u);
}

template <::std::size_t simd_size>
FAST_IO_PADDING_BENCH_NOINLINE
::std::uint_least64_t scan_float_tail_padded(
	char const *first, ::std::size_t semantic_size) noexcept
{
	bool tail_nonzero{};
	auto current{
		::fast_io::details::
			scan_decfloat_skip_after_exact_limit_padding_simd<
				simd_size>(
				first, first + semantic_size, tail_nonzero,
				64u - semantic_size)};
	auto const *const last{first + semantic_size};
	char8_t digit{};
	for (; current != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(
			   *current, digit);
		 ++current)
	{
		if (!tail_nonzero && digit != 0u)
		{
			tail_nonzero = true;
		}
	}
	return static_cast<::std::uint_least64_t>(current - first) |
		   (static_cast<::std::uint_least64_t>(tail_nonzero) << 63u);
}

inline volatile ::std::uint_least64_t benchmark_sink{};

[[nodiscard]] ::std::size_t parse_size(char const *text) noexcept
{
	char *end{};
	auto const value{::std::strtoull(
		text, __builtin_addressof(end), 0)};
	if (end == text || *end != '\0')
	{
		return 0u;
	}
	return static_cast<::std::size_t>(value);
}

template <typename function_type>
[[nodiscard]] double measure(
	function_type function, ::std::vector<char> const &storage,
	::std::size_t semantic_size, ::std::size_t iterations)
{
	constexpr ::std::size_t stride{64u};
	auto accumulator{static_cast<::std::uint_least64_t>(0)};
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t iteration{}; iteration != iterations; ++iteration)
	{
		auto const block{iteration & 4095u};
		accumulator ^= function(
			storage.data() + block * stride, semantic_size);
	}
	auto const finish{::std::chrono::steady_clock::now()};
	benchmark_sink = benchmark_sink ^ accumulator;
	auto const elapsed{
		::std::chrono::duration_cast<::std::chrono::nanoseconds>(
			finish - start)
			.count()};
	return static_cast<double>(elapsed) /
		   static_cast<double>(iterations);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		::std::fprintf(
			stderr,
			"usage: %s MODE SEMANTIC_SIZE ITERATIONS\n",
			argv[0]);
		return 2;
	}
	auto const semantic_size{parse_size(argv[2])};
	auto const iterations{parse_size(argv[3])};
	if (semantic_size == 0u || semantic_size >= 64u ||
		iterations == 0u)
	{
		return 2;
	}

	constexpr ::std::size_t stride{64u};
	::std::vector<char> storage(4096u * stride);
	::std::uint_least64_t random{
		UINT64_C(0x9e3779b97f4a7c15) ^ semantic_size};
	for (::std::size_t block{}; block != 4096u; ++block)
	{
		auto *const destination{storage.data() + block * stride};
		for (::std::size_t index{}; index != stride; ++index)
		{
			random ^= random << 7u;
			random ^= random >> 9u;
			random ^= random << 8u;
			destination[index] =
				static_cast<char>('1' + random % 9u);
		}
		destination[0] = static_cast<char>('1' + block % 8u);
	}

	double nanoseconds{};
	if (::std::strcmp(argv[1], "integer-ordinary") == 0)
	{
		nanoseconds = measure(
			parse_integer_ordinary, storage, semantic_size, iterations);
	}
	else if (::std::strcmp(argv[1], "integer-padded") == 0)
	{
		nanoseconds = measure(
			parse_integer_padded, storage, semantic_size, iterations);
	}
	else if (
		::std::strcmp(argv[1], "integer-public-ordinary") == 0)
	{
		nanoseconds = measure(
			parse_integer_public_ordinary, storage, semantic_size,
			iterations);
	}
	else if (
		::std::strcmp(argv[1], "integer-public-padded") == 0)
	{
		nanoseconds = measure(
			parse_integer_public_padded, storage, semantic_size,
			iterations);
	}
#if defined(__AVX2__)
	else if (::std::strcmp(argv[1], "float-tail-ordinary") == 0)
	{
		nanoseconds = measure(
			scan_float_tail_ordinary<32u>, storage, semantic_size,
			iterations);
	}
	else if (::std::strcmp(argv[1], "float-tail-padded") == 0)
	{
		nanoseconds = measure(
			scan_float_tail_padded<32u>, storage, semantic_size,
			iterations);
	}
#elif defined(__SSE2__)
	else if (::std::strcmp(argv[1], "float-tail-ordinary") == 0)
	{
		nanoseconds = measure(
			scan_float_tail_ordinary<16u>, storage, semantic_size,
			iterations);
	}
	else if (::std::strcmp(argv[1], "float-tail-padded") == 0)
	{
		nanoseconds = measure(
			scan_float_tail_padded<16u>, storage, semantic_size,
			iterations);
	}
#endif
	else
	{
		return 2;
	}
	::std::printf(
		"{\"mode\":\"%s\",\"semantic_size\":%zu,"
		"\"iterations\":%zu,\"ns\":%.6f,\"sink\":%llu}\n",
		argv[1], semantic_size, iterations, nanoseconds,
		static_cast<unsigned long long>(benchmark_sink));
}
