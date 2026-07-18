#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#ifndef FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS
#define FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS 100000u
#endif

#if __has_include(<fmt/compile.h>) && __has_include(<fmt/format.h>)
#include <fmt/compile.h>
#include <fmt/format.h>

namespace
{

inline constexpr ::std::size_t iterations{FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS};

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_FMT_BENCH_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
#define FAST_IO_FMT_BENCH_NOINLINE __declspec(noinline)
#else
#define FAST_IO_FMT_BENCH_NOINLINE
#endif

FAST_IO_FMT_BENCH_NOINLINE inline void fake_write(char const *first, char const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

inline void observe_memory(void const *address, ::std::size_t size) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(address), "r"(size) : "memory");
#else
	(void)address;
	(void)size;
#endif
}

[[noreturn]] inline void fail(char const *message)
{
	::std::fprintf(stderr, "fmt_comparison: %s\n", message);
	::std::abort();
}

inline void require(bool condition, char const *message)
{
	if (!condition)
	{
		fail(message);
	}
}

FAST_IO_FMT_BENCH_NOINLINE inline bool runtime_true() noexcept
{
	bool value{true};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : "+r"(value) : : "memory");
#endif
	return value;
}

inline ::std::uint64_t checksum(::std::string_view text) noexcept
{
	::std::uint64_t value{1469598103934665603ULL};
	for (char const ch : text)
	{
		value ^= static_cast<unsigned char>(ch);
		value *= 1099511628211ULL;
	}
	return value;
}

template <typename function_type>
inline ::std::int64_t measure(function_type &&function)
{
	auto const begin{::std::chrono::steady_clock::now()};
	::std::forward<function_type>(function)();
	auto const end{::std::chrono::steady_clock::now()};
	return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count();
}

inline void report(char const *backend, char const *mode, char const *workload,
				   ::std::int64_t elapsed, ::std::string_view verified)
{
	double const nanoseconds_per_operation{
		static_cast<double>(elapsed) / static_cast<double>(iterations)};
	::std::printf("backend=%s mode=%s workload=%s ns/op=%.3f bytes/op=%zu checksum=%llu iterations=%zu\n",
				  backend, mode, workload, nanoseconds_per_operation, verified.size(),
				  static_cast<unsigned long long>(checksum(verified)), iterations);
}

template <typename render_string_type, typename render_buffer_type, typename render_file_type>
inline void run_backend_case(char const *backend, char const *mode, char const *workload,
							::std::string_view expected, render_string_type const &render_string,
							render_buffer_type const &render_buffer, render_file_type const &render_file)
{
	// This one-shot render is outside the timer. It proves that both the checked format-string object and FMT_COMPILE
	// specialization produce the manual string reference before any backend measurement is accepted.
	::std::string const preflight{render_string()};
	require(preflight == expected, "format correctness preflight failed");

	if (::std::strcmp(backend, "fake-only") == 0)
	{
		auto const elapsed{measure([&] {
			for (::std::size_t i{}; i != iterations; ++i)
			{
				fake_write(expected.data(), expected.data() + expected.size());
			}
		})};
		report(backend, mode, workload, elapsed, expected);
		return;
	}

	if (::std::strcmp(backend, "string") == 0)
	{
		::std::string output;
		auto const elapsed{measure([&] {
			for (::std::size_t i{}; i != iterations; ++i)
			{
				output = render_string();
				observe_memory(output.data(), output.size());
			}
		})};
		require(output == expected, "fmt::format produced incorrect bytes");
		report(backend, mode, workload, elapsed, output);
		return;
	}

	if (::std::strcmp(backend, "memory-buffer") == 0)
	{
		::fmt::memory_buffer output;
		auto const elapsed{measure([&] {
			for (::std::size_t i{}; i != iterations; ++i)
			{
				output.clear();
				render_buffer(output);
				observe_memory(output.data(), output.size());
			}
		})};
		::std::string_view const observed{output.data(), output.size()};
		require(observed == expected, "fmt memory-buffer path produced incorrect bytes");
		report(backend, mode, workload, elapsed, observed);
		return;
	}

	if (::std::strcmp(backend, "fake-call") == 0)
	{
		::fmt::memory_buffer output;
		// Resolve the backend before the timer.  A run-time strcmp inside every iteration is not part of fmt's
		// formatting or of the opaque call boundary, and would charge this backend for harness dispatch that the
		// corresponding fast_io fake sink does not execute.
		auto const elapsed{measure([&] {
			for (::std::size_t i{}; i != iterations; ++i)
			{
				output.clear();
				render_buffer(output);
				fake_write(output.data(), output.data() + output.size());
			}
		})};
		::std::string_view const observed{output.data(), output.size()};
		require(observed == expected, "fmt fake-call path produced incorrect bytes");
		report(backend, mode, workload, elapsed, observed);
		return;
	}

	if (::std::strcmp(backend, "dev-null") == 0)
	{
		::std::FILE *file{::std::fopen("/dev/null", "wb")};
		require(file != nullptr, "cannot open /dev/null");
		// Disable libc buffering so every fmt::print call reaches the FILE write boundary during the timed interval.
		// Otherwise small records would measure memcpy into libc's hidden buffer while the corresponding fast_io POSIX
		// case measures a real write syscall, making the advertised syscall-layer comparison invalid.
		require(::std::setvbuf(file, nullptr, _IONBF, 0) == 0, "cannot disable FILE buffering");
		auto const elapsed{measure([&] {
			for (::std::size_t i{}; i != iterations; ++i)
			{
				render_file(file);
			}
		})};
		require(::std::fflush(file) == 0, "fflush failed");
		require(::std::fclose(file) == 0, "fclose failed");
		report(backend, mode, workload, elapsed, expected);
		return;
	}

	fail("unknown backend");
}

template <typename... args_type>
inline void run_checked(char const *backend, char const *workload, ::std::string_view expected,
						::fmt::format_string<args_type const &...> format, args_type const &...args)
{
	// The format_string constructor has already performed the C++20 compile-time check. Keep the ordinary typed API in
	// the timed closure: fmt::format/format_to/print still build their run-time argument view and execute the regular
	// parsing/formatting layer on every call. Hoisting `make_format_args` would benchmark a prebuilt vformat request and
	// incorrectly remove work that an actual fmt::print call owns. The checked type spells the exact const-reference
	// categories passed below; otherwise GCC/Clang must reconcile an immediate `format_string<T...>` constructor with a
	// second, conflicting deduction of `T` from `T const&`, and a valid literal can fail before the benchmark is formed.
	run_backend_case(
		backend, "checked", workload, expected,
		[&] { return ::fmt::format(format, args...); },
		[&](::fmt::memory_buffer &output) {
			::fmt::format_to(::std::back_inserter(output), format, args...);
		},
		[&](::std::FILE *file) { ::fmt::print(file, format, args...); });
}

template <typename format_type, typename... args_type>
inline void run_compiled(char const *backend, char const *workload, ::std::string_view expected,
						 format_type const &format, args_type const &...args)
{
	// Keeping the FMT_COMPILE object in the direct overload set is essential: converting it to string_view here would
	// erase the compiled formatting program and accidentally benchmark the checked/runtime path a second time.
	run_backend_case(
		backend, "compile", workload, expected,
		[&] { return ::fmt::format(format, args...); },
		[&](::fmt::memory_buffer &output) {
			::fmt::format_to(::std::back_inserter(output), format, args...);
		},
		[&](::std::FILE *file) { ::fmt::print(file, format, args...); });
}

enum class width_placement
{
	left,
	middle,
	right
};

template <::std::size_t width, width_placement placement = width_placement::left>
inline ::std::string make_width()
{
	constexpr ::std::string_view child{"@leaf"};
	constexpr ::std::size_t padding{width - child.size()};
	if constexpr (placement == width_placement::left)
	{
		return ::std::string{child} + ::std::string(padding, '~');
	}
	else if constexpr (placement == width_placement::middle)
	{
		constexpr ::std::size_t left_padding{padding >> 1u};
		return ::std::string(left_padding, '~') + ::std::string{child} +
			   ::std::string(padding - left_padding, '~');
	}
	else
	{
		return ::std::string(padding, '~') + ::std::string{child};
	}
}

template <::std::size_t width, width_placement placement = width_placement::left>
inline void run_width(char const *backend, char const *mode, char const *workload)
{
	::std::string const expected{make_width<width, placement>()};
	constexpr ::std::string_view child{"@leaf"};
	// The integer argument is formatting metadata (field width), never a formatted value. Thus this still isolates
	// string padding/composition and does not benchmark an integer-to-text algorithm.
	if (::std::strcmp(mode, "checked") == 0)
	{
		if constexpr (placement == width_placement::left)
		{
			run_checked(backend, workload, expected, "{:~<{}}", child, width);
		}
		else if constexpr (placement == width_placement::middle)
		{
			run_checked(backend, workload, expected, "{:~^{}}", child, width);
		}
		else
		{
			run_checked(backend, workload, expected, "{:~>{}}", child, width);
		}
		return;
	}
	if (::std::strcmp(mode, "compile") == 0)
	{
		if constexpr (placement == width_placement::left)
		{
			run_compiled(backend, workload, expected, FMT_COMPILE("{:~<{}}"), child, width);
		}
		else if constexpr (placement == width_placement::middle)
		{
			run_compiled(backend, workload, expected, FMT_COMPILE("{:~^{}}"), child, width);
		}
		else
		{
			run_compiled(backend, workload, expected, FMT_COMPILE("{:~>{}}"), child, width);
		}
		return;
	}
	fail("unknown mode");
}

inline void dispatch(char const *backend, char const *mode, char const *workload)
{
	if (::std::strcmp(workload, "leaf") == 0)
	{
		constexpr ::std::string_view value{"plain-leaf"};
		if (::std::strcmp(mode, "checked") == 0)
		{
			run_checked(backend, workload, value, "{}", value);
			return;
		}
		if (::std::strcmp(mode, "compile") == 0)
		{
			run_compiled(backend, workload, value, FMT_COMPILE("{}"), value);
			return;
		}
	}
	if (::std::strcmp(workload, "pack9") == 0)
	{
		constexpr ::std::string_view expected{"abcdefghijklmnopqrstu"};
		constexpr ::std::string_view a{"a"}, b{"bc"}, c{"def"}, d{"ghij"}, e{"k"}, f{"lm"},
			g{"nop"}, h{"qrst"}, i{"u"};
		if (::std::strcmp(mode, "checked") == 0)
		{
			run_checked(backend, workload, expected, "{}{}{}{}{}{}{}{}{}", a, b, c, d, e, f, g, h, i);
			return;
		}
		if (::std::strcmp(mode, "compile") == 0)
		{
			run_compiled(backend, workload, expected, FMT_COMPILE("{}{}{}{}{}{}{}{}{}"),
						 a, b, c, d, e, f, g, h, i);
			return;
		}
	}
	if (::std::strcmp(workload, "cond-pack") == 0)
	{
		constexpr ::std::string_view left{"left"}, right{"right"};
		::std::string_view const selected{runtime_true() ? left : right};
		constexpr ::std::string_view expected{"<left=selected>"};
		constexpr ::std::string_view payload{"selected"};
		if (::std::strcmp(mode, "checked") == 0)
		{
			run_checked(backend, workload, expected, "<{}={}>", selected, payload);
			return;
		}
		if (::std::strcmp(mode, "compile") == 0)
		{
			run_compiled(backend, workload, expected, FMT_COMPILE("<{}={}>"), selected, payload);
			return;
		}
	}
	if (::std::strcmp(workload, "width255") == 0)
	{
		run_width<255u>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width256") == 0)
	{
		run_width<256u>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width257") == 0)
	{
		run_width<257u>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width-middle257") == 0)
	{
		run_width<257u, width_placement::middle>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width-right257") == 0)
	{
		run_width<257u, width_placement::right>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width4095") == 0)
	{
		run_width<4095u>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width4096") == 0)
	{
		run_width<4096u>(backend, mode, workload);
		return;
	}
	if (::std::strcmp(workload, "width4097") == 0)
	{
		run_width<4097u>(backend, mode, workload);
		return;
	}
	fail("unknown mode or workload");
}

inline void usage(char const *program)
{
	::std::printf(
		"usage: %s BACKEND MODE WORKLOAD\n\n"
		"backends: fake-only fake-call memory-buffer string dev-null\n"
		"modes: checked compile\n"
		"workloads: leaf pack9 cond-pack width255 width256 width257 width-middle257 width-right257 "
		"width4095 width4096 width4097\n",
		program);
}

} // namespace

int main(int argc, char **argv)
{
	static_assert(iterations != 0u);
	if (argc == 2 && ::std::strcmp(argv[1], "--list") == 0)
	{
		usage(argv[0]);
		return 0;
	}
	if (argc != 4)
	{
		usage(argv[0]);
		return 2;
	}
	dispatch(argv[1], argv[2], argv[3]);
}

#else

int main()
{
	::std::fputs("fmt_comparison: fmt/compile.h and fmt/format.h are not available\n", stderr);
	return 77;
}

#endif
