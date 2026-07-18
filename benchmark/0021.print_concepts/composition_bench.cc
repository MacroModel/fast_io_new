#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <fast_io_dsal/string.h>
#include <fast_io.h>
#include <fast_io_device.h>
#include <fast_io_legacy.h>

#ifndef FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS
#define FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS 100000u
#endif

namespace
{

inline constexpr ::std::size_t iterations{FAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS};

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_BENCH_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
#define FAST_IO_BENCH_NOINLINE __declspec(noinline)
#else
#define FAST_IO_BENCH_NOINLINE
#endif

// These sinks deliberately expose only the operation being measured.  The noinline compiler barrier makes the
// boundary indistinguishable from an opaque system-call wrapper without adding a memory copy or a checksum update.
// Consequently fake-write measures contiguous-call construction/dispatch, while fake-scatter additionally exercises
// descriptor planning and the native scatter concept.  Correctness is established separately before the timer starts.
struct fake_write_sink
{
	using output_char_type = char;
};

inline constexpr fake_write_sink output_stream_ref_define(fake_write_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, fake_write_sink>) noexcept
{
	// This backend is an opaque compiler barrier with call overhead but no kernel transition or payload copy. It is the
	// positive destination-cost control for single-pass producers: repeated calls are exactly what this benchmark is
	// intended to measure. Real POSIX/Windows file observers do not inherit the marker from structural write support;
	// their run-time scatter/syscall plans therefore retain priority.
	return {};
}

FAST_IO_BENCH_NOINLINE inline void
write_all_overflow_define(fake_write_sink, char const *first, char const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
// These deliberately large, non-trivial observers expose accidental framework copies without changing the formatted
// bytes. Their customization returns one owned normalized proxy: exactly one counted copy per public operation is
// therefore the device contract, while every additional copy is introduced by a recursive library transport boundary.
// Separate correctness tests cover the stronger stable-reference contract, which the frozen public-entry concept could
// not even name because it accessed a member directly on `decltype(ref)` without removing reference qualification.
// Fifteen machine words also keep the object outside every
// modeled register-aggregate envelope (SysV, Microsoft x64, AAPCS, RISC-V, LoongArch, and their conservative
// fallbacks). The benchmark does not claim that size alone establishes identity: the non-trivial copy operation is the
// semantic evidence that this owned proxy must be borrowed after its single public-entry normalization.
struct large_fake_state
{
	::std::size_t copies{};
};

struct large_fake_sink
{
	using output_char_type = char;
	large_fake_state *state{};
	::std::array<::std::uintptr_t, 15u> payload{};

	constexpr large_fake_sink() noexcept = default;
	constexpr explicit large_fake_sink(large_fake_state *state_pointer) noexcept : state{state_pointer} {}

	FAST_IO_BENCH_NOINLINE large_fake_sink(large_fake_sink const &other) noexcept
		: state{other.state}, payload{other.payload}
	{
		++state->copies;
	}

	FAST_IO_BENCH_NOINLINE large_fake_sink(large_fake_sink &&other) noexcept
		: state{other.state}, payload{other.payload}
	{
		++state->copies;
	}
};

inline large_fake_sink output_stream_ref_define(large_fake_sink &sink) noexcept
{
	return sink;
}

FAST_IO_BENCH_NOINLINE inline void
write_all_overflow_define(large_fake_sink &, char const *first, char const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

struct large_obuffer_state
{
	char *begin{};
	char *current{};
	char *end{};
	::std::size_t copies{};
};

struct large_obuffer_sink
{
	using output_char_type = char;
	large_obuffer_state *state{};
	::std::array<::std::uintptr_t, 15u> payload{};

	constexpr large_obuffer_sink() noexcept = default;
	constexpr explicit large_obuffer_sink(large_obuffer_state *state_pointer) noexcept : state{state_pointer} {}

	FAST_IO_BENCH_NOINLINE large_obuffer_sink(large_obuffer_sink const &other) noexcept
		: state{other.state}, payload{other.payload}
	{
		++state->copies;
	}

	FAST_IO_BENCH_NOINLINE large_obuffer_sink(large_obuffer_sink &&other) noexcept
		: state{other.state}, payload{other.payload}
	{
		++state->copies;
	}
};

inline large_obuffer_sink output_stream_ref_define(large_obuffer_sink &sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr char *obuffer_begin(large_obuffer_sink &sink) noexcept
{
	return sink.state->begin;
}

inline constexpr char *obuffer_curr(large_obuffer_sink &sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(large_obuffer_sink &sink) noexcept
{
	return sink.state->end;
}

inline constexpr void obuffer_set_curr(large_obuffer_sink &sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void write_all_overflow_define(large_obuffer_sink &, char const *, char const *)
{
	::std::abort();
}
#endif

struct fake_scatter_sink
{
	using output_char_type = char;
};

inline constexpr fake_scatter_sink output_stream_ref_define(fake_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::size_t
scatter_write_maximum_count(::fast_io::io_reserve_type_t<char, fake_scatter_sink>) noexcept
{
	// Linux writev has a finite descriptor limit.  A finite value also exercises the library's batching proof instead
	// of accidentally benchmarking an unrealistic unlimited backend.
	return 1024u;
}

FAST_IO_BENCH_NOINLINE inline void
write_all_overflow_define(fake_scatter_sink, char const *first, char const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

FAST_IO_BENCH_NOINLINE inline void
scatter_write_all_overflow_define(fake_scatter_sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
								  ::std::size_t count) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(scatters), "r"(count) : "memory");
#else
	(void)scatters;
	(void)count;
#endif
}

// The capture sink is never timed.  Supporting both scalar and scatter operations lets it validate exactly the same
// printable graph regardless of which plan a particular revision chooses.
struct capture_sink
{
	using output_char_type = char;
	::std::string *output;
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

// A pointer-based reference keeps the put cursor in shared state.  This is intentionally not basic_obuffer_view: the
// latter is a terminal view, whereas this sink models the reusable output-object protocol that print strategies see in
// buffered files.  The extra scalar overflow operation is unreachable with the capacity selected by the benchmark,
// but makes the object a complete output stream for generic newline dispatch.
struct reusable_obuffer_state
{
	char *begin;
	char *current;
	char *end;
};

struct reusable_obuffer_sink
{
	using output_char_type = char;
	reusable_obuffer_state *state;
};

inline constexpr reusable_obuffer_sink output_stream_ref_define(reusable_obuffer_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr char *obuffer_begin(reusable_obuffer_sink sink) noexcept
{
	return sink.state->begin;
}

inline constexpr char *obuffer_curr(reusable_obuffer_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(reusable_obuffer_sink sink) noexcept
{
	return sink.state->end;
}

inline constexpr void obuffer_set_curr(reusable_obuffer_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void write_all_overflow_define(reusable_obuffer_sink, char const *, char const *)
{
	::std::abort();
}

inline void write_all_overflow_define(capture_sink sink, char const *first, char const *last)
{
	sink.output->append(first, last);
}

inline void scatter_write_all_overflow_define(capture_sink sink,
								  ::fast_io::basic_io_scatter_t<char> const *scatters,
								  ::std::size_t count)
{
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.output->append(scatters[i].base, scatters[i].len);
	}
}

// A nonnumeric reserve-printable leaf keeps this benchmark focused on formatting composition.  Its internal shift
// gives mnp::internal a real insertion point without involving integer or floating-point conversion algorithms.
struct text_token
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, text_token>) noexcept
{
	return 5u;
}

inline constexpr char *
print_reserve_define(::fast_io::io_reserve_type_t<char, text_token>, char *iter, text_token) noexcept
{
	*iter++ = '@';
	*iter++ = 'l';
	*iter++ = 'e';
	*iter++ = 'a';
	*iter++ = 'f';
	return iter;
}

#if defined(FAST_IO_PRINT_CONCEPT_BASELINE)
// The frozen baseline's sized-range implementation accidentally preserves the iterator's reference in its internal
// reserve tag even though its public element capability is value-shaped. These adapters make that historical header
// instantiable without changing the producer or its generated instructions: both immediately delegate to the same
// five-byte value protocol above. Current headers must not see the adapters, so the benchmark still proves that modern
// range normalization removes this reference-tag leak rather than relying on application-provided compatibility CPOs.
inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, text_token &>) noexcept
{
	return print_reserve_size(::fast_io::io_reserve_type<char, text_token>);
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, text_token &>, char *iter, text_token token) noexcept
{
	return print_reserve_define(::fast_io::io_reserve_type<char, text_token>, iter, token);
}
#endif

inline constexpr ::std::size_t
print_define_internal_shift(::fast_io::io_reserve_type_t<char, text_token>, text_token) noexcept
{
	return 1u;
}

// These protocol fixtures contain text only. Their producer kernels are intentionally elementary so the measured
// differences remain attributable to concept admission, state/descriptor planning, and destination composition.
struct dynamic_text_token
{
	::std::string_view text;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_text_token>, dynamic_text_token token) noexcept
{
	return token.text.size();
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_text_token>, char *destination,
	dynamic_text_token token) noexcept
{
	for (char ch : token.text)
	{
		*destination++ = ch;
	}
	return destination;
}

inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, dynamic_text_token>) noexcept
{
	return 64u;
}

struct precise_text_token
{
	::std::string_view text;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_text_token>, precise_text_token token) noexcept
{
	// The deliberately loose ordinary bound keeps the exact protocol observable: a strategy that ignores precise
	// sizing still emits the same characters but reserves eight unnecessary bytes per leaf.
	return token.text.size() + 8u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_text_token>, char *destination,
	precise_text_token token) noexcept
{
	for (char ch : token.text)
	{
		*destination++ = ch;
	}
	return destination;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_text_token>, precise_text_token token) noexcept
{
	return token.text.size();
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_text_token>, char *destination,
	[[maybe_unused]] ::std::size_t precise_size, precise_text_token token) noexcept
{
	return print_reserve_define(
		::fast_io::io_reserve_type<char, precise_text_token>, destination, token);
}

struct scatter_plan_token
{
	::std::string_view first;
	::std::string_view second;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	return {3u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	scatter_plan_token token) noexcept
{
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	// Every descriptor names either a string literal or the caller-owned string-view payload. No scratch is reused by
	// later producers, so a semantic pack may retain all nine plans until its final scatter write.
	return {};
}

struct context_text_token
{
	::std::string_view text;
};

struct context_text_state
{
	::std::size_t offset{};

	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		context_text_token token, char *first, char *last) noexcept
	{
		auto current{first};
		while (current != last && offset != token.text.size())
		{
			*current++ = token.text[offset++];
		}
		return {current, offset == token.text.size()};
	}
};

inline constexpr ::fast_io::io_type_t<context_text_state> print_context_type(
	::fast_io::io_reserve_type_t<char, context_text_token>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char, context_text_token>) noexcept
{
	// A deliberately short window forces multi-step state reuse without making the payload large.
	return 4u;
}

struct staged_text_token
{
	char first;
	char second;
};

struct staged_text_state
{
	char first{};
	char second{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, staged_text_token>) noexcept
{
	return 2u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, staged_text_token>, char *destination,
	staged_text_token token) noexcept
{
	*destination++ = token.first;
	*destination++ = token.second;
	return destination;
}

inline constexpr ::fast_io::io_type_t<staged_text_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, staged_text_token>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, staged_text_token>) noexcept
{
	return 4u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, staged_text_token>, staged_text_token const &) noexcept
{
	return true;
}

inline constexpr staged_text_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, staged_text_token>, staged_text_token const &token) noexcept
{
	return {token.first, token.second};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, staged_text_token>, char *destination,
	staged_text_token const &, staged_text_state const &state) noexcept
{
	*destination++ = state.first;
	*destination++ = state.second;
	return destination;
}

static_assert(::fast_io::dynamic_reserve_with_possible_static_stack_size<char, dynamic_text_token>);
static_assert(::fast_io::precise_reserve_printable<char, precise_text_token>);
static_assert(::fast_io::reserve_scatters_printable<char, scatter_plan_token>);
static_assert(::fast_io::context_printable_with_static_buffer_size<char, context_text_token>);
static_assert(::fast_io::staged_printable<char, staged_text_token>);

[[noreturn]] inline void fail(char const *message)
{
	::std::fprintf(stderr, "composition_bench: %s\n", message);
	::std::abort();
}

inline void require(bool condition, char const *message)
{
	if (!condition)
	{
		fail(message);
	}
}

FAST_IO_BENCH_NOINLINE inline bool runtime_true() noexcept
{
	bool value{true};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : "+r"(value) : : "memory");
#endif
	return value;
}

inline void observe_memory(void const *address, ::std::size_t size) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	// This zero-instruction compiler barrier prevents repeated reusable-buffer or concat results from being collapsed.
	// Unlike the fake syscall, it remains inline and therefore contributes no call instruction to memory-write cases.
	__asm__ __volatile__("" : : "r"(address), "r"(size) : "memory");
#else
	(void)address;
	(void)size;
#endif
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

template <bool line, typename output, typename printable>
inline void invoke_print(output &&out, printable const &value)
{
	if constexpr (line)
	{
		::fast_io::println(::std::forward<output>(out), value);
	}
	else
	{
		::fast_io::print(::std::forward<output>(out), value);
	}
}

template <bool line, typename printable>
inline ::std::string verify_print(printable const &value, ::std::string_view expected)
{
	::std::string observed;
	capture_sink sink{__builtin_addressof(observed)};
	invoke_print<line>(sink, value);
	::std::string complete_expected{expected};
	if constexpr (line)
	{
		complete_expected.push_back('\n');
	}
	require(observed == complete_expected, "print/println correctness preflight failed");
	return complete_expected;
}

template <typename function_type>
inline ::std::int64_t measure(function_type &&function)
{
	auto const begin{::std::chrono::steady_clock::now()};
	::std::forward<function_type>(function)();
	auto const end{::std::chrono::steady_clock::now()};
	return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count();
}

inline void report(char const *backend, char const *operation, char const *workload,
				   ::std::int64_t elapsed, ::std::string_view verified)
{
	double const nanoseconds_per_operation{
		static_cast<double>(elapsed) / static_cast<double>(iterations)};
	::std::printf("backend=%s operation=%s workload=%s ns/op=%.3f bytes/op=%zu checksum=%llu iterations=%zu\n",
				  backend, operation, workload, nanoseconds_per_operation, verified.size(),
				  static_cast<unsigned long long>(checksum(verified)), iterations);
}

inline void report_observer_transport(char const *backend, char const *operation, char const *workload,
									 ::std::int64_t elapsed, ::std::string_view verified,
									 ::std::size_t copies)
{
	double const nanoseconds_per_operation{
		static_cast<double>(elapsed) / static_cast<double>(iterations)};
	double const copies_per_operation{
		static_cast<double>(copies) / static_cast<double>(iterations)};
	::std::printf(
		"backend=%s operation=%s workload=%s ns/op=%.3f bytes/op=%zu checksum=%llu "
		"copies/op=%.3f iterations=%zu\n",
		backend, operation, workload, nanoseconds_per_operation, verified.size(),
		static_cast<unsigned long long>(checksum(verified)), copies_per_operation, iterations);
}

template <bool line, typename printable>
inline void benchmark_fake_only(char const *operation, char const *workload, printable const &value,
								 ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			write_all_overflow_define(fake_write_sink{}, verified.data(), verified.data() + verified.size());
		}
	})};
	report("fake-only", operation, workload, elapsed, verified);
}

template <bool line, typename printable>
inline void benchmark_fake_write(char const *operation, char const *workload, printable const &value,
									 ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	fake_write_sink sink;
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			invoke_print<line>(sink, value);
		}
	})};
	report("fake-write", operation, workload, elapsed, verified);
}

#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
template <bool line, typename printable>
inline void benchmark_large_fake_observer(char const *operation, char const *workload,
									  printable const &value, ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	large_fake_state state{};
	large_fake_sink sink{__builtin_addressof(state)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			invoke_print<line>(sink, value);
		}
	})};
	report_observer_transport(
		"large-fake-observer", operation, workload, elapsed, verified, state.copies);
}
#endif

template <bool line, typename printable>
inline void benchmark_fake_scatter(char const *operation, char const *workload, printable const &value,
								   ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	fake_scatter_sink sink;
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			invoke_print<line>(sink, value);
		}
	})};
	report("fake-scatter", operation, workload, elapsed, verified);
}

template <bool line, typename printable>
inline void benchmark_obuffer(char const *operation, char const *workload, printable const &value,
							  ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	::std::vector<char> storage(verified.size() + 64u);
	reusable_obuffer_state state{storage.data(), storage.data(), storage.data() + storage.size()};
	reusable_obuffer_sink sink{__builtin_addressof(state)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			state.current = state.begin;
			invoke_print<line>(sink, value);
			observe_memory(state.begin, static_cast<::std::size_t>(state.current - state.begin));
		}
	})};
	::std::size_t const observed_size{static_cast<::std::size_t>(state.current - state.begin)};
	require(observed_size == verified.size(), "reusable obuffer produced the wrong byte count");
	require(::std::string_view{state.begin, observed_size} == verified,
			"reusable obuffer produced incorrect bytes");
	report("obuffer", operation, workload, elapsed, verified);
}

#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
template <bool line, typename printable>
inline void benchmark_large_obuffer_observer(char const *operation, char const *workload,
										 printable const &value, ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	::std::vector<char> storage(verified.size() + 64u);
	large_obuffer_state state{storage.data(), storage.data(), storage.data() + storage.size(), 0u};
	large_obuffer_sink sink{__builtin_addressof(state)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			state.current = state.begin;
			invoke_print<line>(sink, value);
			observe_memory(state.begin, static_cast<::std::size_t>(state.current - state.begin));
		}
	})};
	::std::size_t const observed_size{static_cast<::std::size_t>(state.current - state.begin)};
	require(observed_size == verified.size(), "large obuffer observer produced the wrong byte count");
	require(::std::string_view{state.begin, observed_size} == verified,
			"large obuffer observer produced incorrect bytes");
	report_observer_transport(
		"large-obuffer-observer", operation, workload, elapsed, verified, state.copies);
}
#endif

template <bool line, typename string_type, typename string_sink_type, typename printable>
inline void benchmark_reusable_string(char const *backend, char const *operation, char const *workload,
									 printable const &value, ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	string_type output;
	// Reserve before timing because this case represents a long-lived string output object.  concat_std/concat_fast_io
	// remain the allocation-owning cases; mixing allocation growth into both would obscure the output-reference policy.
	output.reserve(verified.size() + 64u);
	string_sink_type sink{__builtin_addressof(output)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			output.clear();
			invoke_print<line>(sink, value);
			observe_memory(output.data(), output.size());
		}
	})};
	::std::string_view const observed{output.data(), output.size()};
	require(observed == verified, "reusable string output reference produced incorrect bytes");
	report(backend, operation, workload, elapsed, observed);
}

template <bool line, typename output, typename printable>
inline void benchmark_native_output(char const *backend, char const *operation, char const *workload,
								   output &out, printable const &value, ::std::string_view expected)
{
	::std::string const verified{verify_print<line>(value, expected)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			invoke_print<line>(out, value);
		}
	})};
	// Buffered C and streambuf backends may retain the tail of the final operation.  Flushing after the clock preserves
	// their steady-state buffering cost while still surfacing deferred device errors before a result is reported.
	auto normalized{::fast_io::operations::output_stream_ref(out)};
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<
					  decltype(normalized)>)
	{
		::fast_io::operations::decay::output_stream_buffer_flush_decay(normalized);
	}
	report(backend, operation, workload, elapsed, verified);
}

template <bool current_context_strategy_required, bool line, typename printable>
inline void dispatch_print_backend(char const *backend, char const *operation, char const *workload,
								  printable const &value, ::std::string_view expected)
{
	if (::std::strcmp(backend, "fake-only") == 0)
	{
		benchmark_fake_only<line>(operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(backend, "fake-write") == 0)
	{
		benchmark_fake_write<line>(operation, workload, value, expected);
		return;
	}
#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
	if (::std::strcmp(backend, "large-fake-observer") == 0)
	{
		benchmark_large_fake_observer<line>(operation, workload, value, expected);
		return;
	}
#endif
	if (::std::strcmp(backend, "fake-scatter") == 0)
	{
		benchmark_fake_scatter<line>(operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(backend, "obuffer") == 0)
	{
	#if defined(FAST_IO_PRINT_CONCEPT_BASELINE)
		if constexpr (current_context_strategy_required)
		{
			// The frozen baseline cannot form context printing for this reusable put-area observer: its selected context
			// branch unconditionally names a flush CPO that the observer does not provide. Keep that compile-time
			// regression visible as an unsupported tuple instead of preventing every otherwise comparable backend from
			// being instantiated in the common harness.
			fail("baseline does not support context3 on the reusable obuffer backend");
		}
		else
	#endif
		{
			benchmark_obuffer<line>(operation, workload, value, expected);
		}
		return;
	}
#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
	if (::std::strcmp(backend, "large-obuffer-observer") == 0)
	{
		benchmark_large_obuffer_observer<line>(operation, workload, value, expected);
		return;
	}
#endif
	if (::std::strcmp(backend, "ostring-std") == 0)
	{
		benchmark_reusable_string<line, ::std::string, ::fast_io::ostring_ref_std>(
			backend, operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(backend, "ostring-fast") == 0)
	{
		benchmark_reusable_string<line, ::fast_io::string, ::fast_io::ostring_ref_fast_io>(
			backend, operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(backend, "dev-null-owner") == 0)
	{
		::fast_io::posix_file file{"/dev/null", ::fast_io::open_mode::out};
		benchmark_native_output<line>(backend, operation, workload, file, value, expected);
		return;
	}
	if (::std::strcmp(backend, "dev-null-observer") == 0)
	{
		::fast_io::posix_file file{"/dev/null", ::fast_io::open_mode::out};
		::fast_io::posix_io_observer observer{file.native_handle()};
		benchmark_native_output<line>(backend, operation, workload, observer, value, expected);
		return;
	}
	if (::std::strcmp(backend, "c-unlocked-owner") == 0)
	{
		::fast_io::c_file_unlocked file{"/dev/null", ::fast_io::open_mode::out};
		benchmark_native_output<line>(backend, operation, workload, file, value, expected);
		return;
	}
	if (::std::strcmp(backend, "c-unlocked-observer") == 0)
	{
		::fast_io::c_file_unlocked file{"/dev/null", ::fast_io::open_mode::out};
		::fast_io::c_io_observer_unlocked observer{file.native_handle()};
		benchmark_native_output<line>(backend, operation, workload, observer, value, expected);
		return;
	}
	if (::std::strcmp(backend, "c-owner") == 0)
	{
		::fast_io::c_file file{"/dev/null", ::fast_io::open_mode::out};
		benchmark_native_output<line>(backend, operation, workload, file, value, expected);
		return;
	}
	if (::std::strcmp(backend, "c-observer") == 0)
	{
		::fast_io::c_file file{"/dev/null", ::fast_io::open_mode::out};
		::fast_io::c_io_observer observer{file.native_handle()};
		benchmark_native_output<line>(backend, operation, workload, observer, value, expected);
		return;
	}
	if (::std::strcmp(backend, "obuf-owner") == 0)
	{
		::fast_io::obuf_file file{"/dev/null"};
		benchmark_native_output<line>(backend, operation, workload, file, value, expected);
		return;
	}
	if (::std::strcmp(backend, "obuf-ref") == 0)
	{
		::fast_io::obuf_file file{"/dev/null"};
		auto normalized{::fast_io::operations::output_stream_ref(file)};
		static_assert(!::std::is_same_v<::std::remove_cvref_t<decltype(normalized)>, ::fast_io::obuf_file>);
		// The owner case normalizes on every API entry; this case hoists precisely that normalization out of the timed
		// loop.  Both retain the same buffer and native handle, so their delta isolates owner-to-reference composition.
		benchmark_native_output<line>(backend, operation, workload, normalized, value, expected);
		return;
	}
	if (::std::strcmp(backend, "filebuf-owner") == 0)
	{
		::fast_io::filebuf_file file{"/dev/null", ::fast_io::open_mode::out};
		benchmark_native_output<line>(backend, operation, workload, file, value, expected);
		return;
	}
	if (::std::strcmp(backend, "filebuf-observer") == 0)
	{
		::fast_io::filebuf_file file{"/dev/null", ::fast_io::open_mode::out};
		::fast_io::filebuf_io_observer observer{file.native_handle()};
		benchmark_native_output<line>(backend, operation, workload, observer, value, expected);
		return;
	}
	fail("unknown or incompatible print backend");
}

// Keep each concat specialization outside the workload dispatcher's global inlining budget. The complete timed loop
// remains inside this function, so the attribute adds no call to an individual iteration. This isolation is important
// for high-arity formatting graphs: with GCC 15, merely adding an unrelated template specialization otherwise changed
// the generated sixteen-leaf loop by several KiB and could masquerade as a library strategy effect.
template <bool line, typename printable>
FAST_IO_BENCH_NOINLINE inline void benchmark_concat_std(
	char const *operation, char const *workload, printable const &value, ::std::string_view expected)
{
	::std::string complete_expected{expected};
	if constexpr (line)
	{
		complete_expected.push_back('\n');
	}
	::std::string observed;
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			if constexpr (line)
			{
				observed = ::fast_io::concatln_std(value);
			}
			else
			{
				observed = ::fast_io::concat_std(value);
			}
			observe_memory(observed.data(), observed.size());
		}
	})};
	require(observed == complete_expected, "concat_std/concatln_std produced incorrect bytes");
	report("std-string", operation, workload, elapsed, observed);
}

template <bool line, typename printable>
FAST_IO_BENCH_NOINLINE inline void benchmark_concat_fast(
	char const *operation, char const *workload, printable const &value, ::std::string_view expected)
{
	::std::string complete_expected{expected};
	if constexpr (line)
	{
		complete_expected.push_back('\n');
	}
	::fast_io::string observed;
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			if constexpr (line)
			{
				observed = ::fast_io::concatln_fast_io(value);
			}
			else
			{
				observed = ::fast_io::concat_fast_io(value);
			}
			observe_memory(observed.data(), observed.size());
		}
	})};
	::std::string_view const observed_view{observed.data(), observed.size()};
	require(observed_view == complete_expected, "concat_fast_io/concatln_fast_io produced incorrect bytes");
	report("fast-string", operation, workload, elapsed, observed_view);
}

template <bool current_context_strategy_required = false, typename printable>
inline void dispatch_operation(char const *backend, char const *operation, char const *workload,
								  printable const &value, ::std::string_view expected)
{
	if (::std::strcmp(operation, "print") == 0)
	{
		dispatch_print_backend<current_context_strategy_required, false>(
			backend, operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(operation, "println") == 0)
	{
		dispatch_print_backend<current_context_strategy_required, true>(
			backend, operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(operation, "concat") == 0 && ::std::strcmp(backend, "std-string") == 0)
	{
		benchmark_concat_std<false>(operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(operation, "concatln") == 0 && ::std::strcmp(backend, "std-string") == 0)
	{
		benchmark_concat_std<true>(operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(operation, "concat") == 0 && ::std::strcmp(backend, "fast-string") == 0)
	{
		benchmark_concat_fast<false>(operation, workload, value, expected);
		return;
	}
	if (::std::strcmp(operation, "concatln") == 0 && ::std::strcmp(backend, "fast-string") == 0)
	{
		benchmark_concat_fast<true>(operation, workload, value, expected);
		return;
	}
	fail("unknown or incompatible operation/backend pair");
}

enum class width_placement
{
	left,
	middle,
	right,
	internal
};

inline ::std::string make_width(::std::size_t width, width_placement placement, char fill)
{
	constexpr ::std::string_view child{"@leaf"};
	if (width <= child.size())
	{
		return ::std::string{child};
	}
	::std::size_t const padding{width - child.size()};
	switch (placement)
	{
	case width_placement::left:
		return ::std::string{child} + ::std::string(padding, fill);
	case width_placement::middle:
	{
		::std::size_t const left_padding{padding >> 1u};
		return ::std::string(left_padding, fill) + ::std::string{child} +
			   ::std::string(padding - left_padding, fill);
	}
	case width_placement::right:
		return ::std::string(padding, fill) + ::std::string{child};
	case width_placement::internal:
		return ::std::string{"@"} + ::std::string(padding, fill) + "leaf";
	}
	fail("invalid width placement");
}

template <::std::size_t count>
inline ::std::string make_range_reference(::std::array<::std::string_view, count> const &values,
										  ::std::string_view separator)
{
	::std::string result;
	for (::std::size_t i{}; i != count; ++i)
	{
		if (i != 0u)
		{
			result.append(separator);
		}
		result.append(values[i]);
	}
	return result;
}

template <::std::size_t count>
inline ::std::string make_fixed_range_reference(::std::string_view separator)
{
	::std::string result;
	for (::std::size_t i{}; i != count; ++i)
	{
		if (i != 0u)
		{
			result.append(separator);
		}
		result.append("@leaf");
	}
	return result;
}

template <::std::size_t count>
inline void run_range_workload(char const *backend, char const *operation, char const *workload)
{
	constexpr ::std::array<::std::string_view, 4u> choices{"a", "bc", "def", "ghij"};
	::std::array<::std::string_view, count> values{};
	for (::std::size_t i{}; i != count; ++i)
	{
		values[i] = choices[i % choices.size()];
	}
	constexpr ::std::string_view separator{"::"};
	auto value{::fast_io::mnp::rgvw(values, separator)};
	::std::string const expected{make_range_reference(values, separator)};
	dispatch_operation(backend, operation, workload, value, expected);
}

template <::std::size_t count>
inline void run_fixed_range_workload(char const *backend, char const *operation, char const *workload)
{
	::std::array<text_token, count> values{};
	constexpr ::std::string_view separator{"::"};
	auto value{::fast_io::mnp::rgvw(values, separator)};
	::std::string const expected{make_fixed_range_reference<count>(separator)};
	dispatch_operation(backend, operation, workload, value, expected);
}

template <bool line, bool framed, typename output, typename range_type>
inline void invoke_range_record(output &&out, range_type const &range)
{
	using namespace ::std::literals;
	if constexpr (framed)
	{
		if constexpr (line)
		{
			::fast_io::println(::std::forward<output>(out), "pre["sv, range, "]post"sv);
		}
		else
		{
			::fast_io::print(::std::forward<output>(out), "pre["sv, range, "]post"sv);
		}
	}
	else
	{
		if constexpr (line)
		{
			::fast_io::println(::std::forward<output>(out), ""sv, range, ""sv);
		}
		else
		{
			::fast_io::print(::std::forward<output>(out), ""sv, range, ""sv);
		}
	}
}

/// @brief Measures the ordinary multi-argument fast entry around a put-area-preferred range.
/// @details The adjacent form adds two empty scatters and therefore emits exactly the same bytes as `rgvwN`; any delta
///          is strategy overhead, not payload work. The framed form represents the common record-building case. Both
///          deliberately remain ordinary arguments rather than a semantic pack: this isolates whether mixed static
///          scatters make the legacy dynamic-scatter admission premeasure a range that could otherwise traverse once
///          into a real put area. The reusable obuffer is the unmarked structural control; the reusable fast_io string
///          exercises the explicit deferred-commit marker. Short put areas, append-only adapters, and direct-write
///          sinks have different whole-record trade-offs and remain outside these two fixtures.
template <bool line, bool framed, typename range_type>
inline void benchmark_obuffer_range_record(char const *operation, char const *workload,
										   range_type const &range, ::std::string_view expected)
{
	::std::string verified;
	capture_sink capture{__builtin_addressof(verified)};
	invoke_range_record<line, framed>(capture, range);
	::std::string complete_expected{expected};
	if constexpr (line)
	{
		complete_expected.push_back('\n');
	}
	require(verified == complete_expected, "range-record correctness preflight failed");

	::std::vector<char> storage(verified.size() + 64u);
	reusable_obuffer_state state{storage.data(), storage.data(), storage.data() + storage.size()};
	reusable_obuffer_sink sink{__builtin_addressof(state)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			state.current = state.begin;
			invoke_range_record<line, framed>(sink, range);
			observe_memory(state.begin, static_cast<::std::size_t>(state.current - state.begin));
		}
	})};
	::std::size_t const observed_size{static_cast<::std::size_t>(state.current - state.begin)};
	require(::std::string_view(state.begin, observed_size) == verified,
			"range-record reusable obuffer produced incorrect bytes");
	report("obuffer", operation, workload, elapsed, verified);
}

template <bool line, bool framed, typename range_type>
inline void benchmark_fast_string_range_record(char const *operation, char const *workload,
										   range_type const &range, ::std::string_view expected)
{
	::std::string verified;
	capture_sink capture{__builtin_addressof(verified)};
	invoke_range_record<line, framed>(capture, range);
	::std::string complete_expected{expected};
	if constexpr (line)
	{
		complete_expected.push_back('\n');
	}
	require(verified == complete_expected, "range-record fast-string correctness preflight failed");

	::fast_io::string output;
	output.reserve(verified.size() + 64u);
	::fast_io::ostring_ref_fast_io sink{__builtin_addressof(output)};
	auto const elapsed{measure([&] {
		for (::std::size_t i{}; i != iterations; ++i)
		{
			output.clear();
			invoke_range_record<line, framed>(sink, range);
			observe_memory(output.data(), output.size());
		}
	})};
	::std::string_view const observed{output.data(), output.size()};
	require(observed == verified, "range-record fast-string output produced incorrect bytes");
	report("ostring-fast", operation, workload, elapsed, observed);
}

template <::std::size_t count, bool framed>
inline void run_range_record_workload(char const *backend, char const *operation, char const *workload)
{
	bool const use_obuffer{::std::strcmp(backend, "obuffer") == 0};
	bool const use_fast_string{::std::strcmp(backend, "ostring-fast") == 0};
	if (!use_obuffer && !use_fast_string)
	{
		fail("range-record workload requires obuffer or ostring-fast");
	}
	constexpr ::std::array<::std::string_view, 4u> choices{"a", "bc", "def", "ghij"};
	::std::array<::std::string_view, count> values{};
	for (::std::size_t i{}; i != count; ++i)
	{
		values[i] = choices[i % choices.size()];
	}
	auto range{::fast_io::mnp::rgvw(values, "::")};
	::std::string expected{make_range_reference(values, "::")};
	if constexpr (framed)
	{
		expected = "pre[" + expected + "]post";
	}
	if (::std::strcmp(operation, "print") == 0)
	{
		if (use_obuffer)
		{
			benchmark_obuffer_range_record<false, framed>(operation, workload, range, expected);
		}
		else
		{
			benchmark_fast_string_range_record<false, framed>(operation, workload, range, expected);
		}
		return;
	}
	if (::std::strcmp(operation, "println") == 0)
	{
		if (use_obuffer)
		{
			benchmark_obuffer_range_record<true, framed>(operation, workload, range, expected);
		}
		else
		{
			benchmark_fast_string_range_record<true, framed>(operation, workload, range, expected);
		}
		return;
	}
	fail("range-record workload supports only print/println");
}

template <::std::size_t width, width_placement placement = width_placement::left>
inline void run_width_workload(char const *backend, char const *operation, char const *workload)
{
	::std::string const expected{make_width(width, placement, '~')};
	if constexpr (placement == width_placement::left)
	{
		auto value{::fast_io::mnp::left(text_token{}, width, '~')};
		dispatch_operation(backend, operation, workload, value, expected);
	}
	else if constexpr (placement == width_placement::middle)
	{
		auto value{::fast_io::mnp::middle(text_token{}, width, '~')};
		dispatch_operation(backend, operation, workload, value, expected);
	}
	else if constexpr (placement == width_placement::right)
	{
		auto value{::fast_io::mnp::right(text_token{}, width, '~')};
		dispatch_operation(backend, operation, workload, value, expected);
	}
	else
	{
		// Internal placement is materially different from ordinary alignment: the child advertises a one-character
		// insertion point, so padding splits "@leaf" into "@" and "leaf". Keeping it as a named workload prevents a
		// left/right-only matrix from hiding regressions in the semantic internal-shift capability.
		auto value{::fast_io::mnp::internal(text_token{}, width, '~')};
		dispatch_operation(backend, operation, workload, value, expected);
	}
}

inline void run_mixed_workload(char const *backend, char const *operation, char const *workload)
{
	constexpr ::std::array<::std::string_view, 16u> values{
		"a", "bc", "def", "ghij", "k", "lm", "nop", "qrst",
		"u", "vw", "xyz", "ABCD", "E", "FG", "HIJ", "KLMN"};
	auto range{::fast_io::mnp::rgvw(values, "::")};
	auto left{::fast_io::mnp::left(text_token{}, 257u, '.')};
	auto middle{::fast_io::mnp::middle(text_token{}, 4097u, ':')};
	auto right{::fast_io::mnp::right(text_token{}, 257u, '+')};
	auto internal{::fast_io::mnp::internal(text_token{}, 4097u, '-')};
	auto selected{::fast_io::mnp::cond(runtime_true(), ::fast_io::mnp::pack("L=", left),
											 ::fast_io::mnp::pack("R=", right))};
	auto optional{::fast_io::mnp::cond(runtime_true(), ::fast_io::mnp::pack("|M=", middle))};
	auto value{::fast_io::mnp::pack("record:{", selected, optional, "|I=", internal, "|range=", range, "}")};

	::std::string expected{"record:{L="};
	expected += make_width(257u, width_placement::left, '.');
	expected += "|M=";
	expected += make_width(4097u, width_placement::middle, ':');
	expected += "|I=";
	expected += make_width(4097u, width_placement::internal, '-');
	expected += "|range=";
	expected += make_range_reference(values, "::");
	expected.push_back('}');
	dispatch_operation(backend, operation, workload, value, expected);
}

inline void run_dynamic_pack_workload(char const *backend, char const *operation, char const *workload)
{
	auto value{::fast_io::mnp::pack(
		dynamic_text_token{"a"}, dynamic_text_token{"bc"}, dynamic_text_token{"def"},
		dynamic_text_token{"ghij"}, dynamic_text_token{"k"}, dynamic_text_token{"lm"},
		dynamic_text_token{"nop"}, dynamic_text_token{"qrst"}, dynamic_text_token{"u"})};
	dispatch_operation(backend, operation, workload, value, "abcdefghijklmnopqrstu");
}

template <::std::size_t count, ::std::size_t... indexes>
inline void run_precise_pack_workload_impl(
	char const *backend, char const *operation, char const *workload,
	::std::index_sequence<indexes...>)
{
	constexpr ::std::array<::std::string_view, 4u> choices{"a", "bc", "def", "ghij"};
	auto value{::fast_io::mnp::pack(precise_text_token{choices[indexes % choices.size()]}...)};
	::std::string expected;
	for (::std::size_t i{}; i != count; ++i)
	{
		expected.append(choices[i % choices.size()]);
	}
	dispatch_operation(backend, operation, workload, value, expected);
}

/// @brief Benchmarks the flat 2/4/8/12/16 exact-resize cost boundary without conversion algorithms.
/// @details Public pack normalization exposes `count` ordinary text leaves to concat phase 1. The portable std::string
///          destination can therefore use one exact logical resize, while fast_io::string is the native put-area
///          control. Eight and twelve leaves distinguish gradual per-leaf work from a suspected high-arity compiler
///          cliff, while the noinline timed-loop boundary prevents unrelated specializations from changing GCC's
///          global inlining decision. Producer loops and bytes are identical in both cases; only destination strategy
///          differs.
template <::std::size_t count>
inline void run_precise_pack_workload(char const *backend, char const *operation, char const *workload)
{
	run_precise_pack_workload_impl<count>(
		backend, operation, workload, ::std::make_index_sequence<count>{});
}

inline void run_scatter_plan_pack_workload(char const *backend, char const *operation, char const *workload)
{
	auto value{::fast_io::mnp::pack(
		scatter_plan_token{"a", "A"}, scatter_plan_token{"b", "B"},
		scatter_plan_token{"c", "C"}, scatter_plan_token{"d", "D"},
		scatter_plan_token{"e", "E"}, scatter_plan_token{"f", "F"},
		scatter_plan_token{"g", "G"}, scatter_plan_token{"h", "H"},
		scatter_plan_token{"i", "I"})};
	dispatch_operation(backend, operation, workload, value,
		"a|Ab|Bc|Cd|De|Ef|Fg|Gh|Hi|I");
}

inline void run_context_pack_workload(char const *backend, char const *operation, char const *workload)
{
	auto value{::fast_io::mnp::pack(
		context_text_token{"context"}, context_text_token{"-state"}, context_text_token{"-windows"})};
	dispatch_operation<true>(backend, operation, workload, value, "context-state-windows");
}

inline void run_staged_pack_workload(char const *backend, char const *operation, char const *workload)
{
	auto value{::fast_io::mnp::pack(
		staged_text_token{'a', 'A'}, staged_text_token{'b', 'B'}, staged_text_token{'c', 'C'},
		staged_text_token{'d', 'D'}, staged_text_token{'e', 'E'}, staged_text_token{'f', 'F'},
		staged_text_token{'g', 'G'}, staged_text_token{'h', 'H'}, staged_text_token{'i', 'I'})};
	dispatch_operation(backend, operation, workload, value, "aAbBcCdDeEfFgGhHiI");
}

inline void dispatch_workload(char const *backend, char const *operation, char const *workload)
{
	using namespace ::std::literals;
	if (::std::strcmp(workload, "leaf") == 0)
	{
		constexpr ::std::string_view value{"plain-leaf"};
		dispatch_operation(backend, operation, workload, value, value);
		return;
	}
	if (::std::strcmp(workload, "pack9") == 0)
	{
		auto value{::fast_io::mnp::pack("a"sv, "bc"sv, "def"sv, "ghij"sv, "k"sv,
										  "lm"sv, "nop"sv, "qrst"sv, "u"sv)};
		dispatch_operation(backend, operation, workload, value, "abcdefghijklmnopqrstu"sv);
		return;
	}
	if (::std::strcmp(workload, "cond-pack") == 0)
	{
		auto left{::fast_io::mnp::pack("left"sv, "="sv, "selected"sv)};
		auto right{::fast_io::mnp::pack("right"sv, "="sv, "unused"sv)};
		auto selected{::fast_io::mnp::cond(runtime_true(), left, right)};
		auto value{::fast_io::mnp::pack("<"sv, ::fast_io::mnp::pack(selected), ">"sv)};
		dispatch_operation(backend, operation, workload, value, "<left=selected>"sv);
		return;
	}
	if (::std::strcmp(workload, "width255") == 0)
	{
		run_width_workload<255u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width256") == 0)
	{
		run_width_workload<256u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width257") == 0)
	{
		run_width_workload<257u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width-middle257") == 0)
	{
		run_width_workload<257u, width_placement::middle>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width-right257") == 0)
	{
		run_width_workload<257u, width_placement::right>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width-internal257") == 0)
	{
		run_width_workload<257u, width_placement::internal>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width4095") == 0)
	{
		run_width_workload<4095u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width4096") == 0)
	{
		run_width_workload<4096u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "width4097") == 0)
	{
		run_width_workload<4097u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw1") == 0)
	{
		run_range_workload<1u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw16") == 0)
	{
		run_range_workload<16u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw128") == 0)
	{
		run_range_workload<128u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw512") == 0)
	{
		// This pure string-view range produces roughly 2.3 KiB, crossing concat's former 2-KiB inline staging
		// boundary without changing element or separator protocols.
		run_range_workload<512u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw-fixed1") == 0)
	{
		run_fixed_range_workload<1u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw-fixed16") == 0)
	{
		run_fixed_range_workload<16u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw-fixed128") == 0)
	{
		run_fixed_range_workload<128u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw16-adjacent") == 0)
	{
		run_range_record_workload<16u, false>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw128-adjacent") == 0)
	{
		run_range_record_workload<128u, false>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw16-framed") == 0)
	{
		run_range_record_workload<16u, true>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "rgvw128-framed") == 0)
	{
		run_range_record_workload<128u, true>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "mixed") == 0)
	{
		run_mixed_workload(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "dynamic9") == 0)
	{
		run_dynamic_pack_workload(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "precise2") == 0)
	{
		run_precise_pack_workload<2u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "precise4") == 0)
	{
		run_precise_pack_workload<4u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "precise8") == 0)
	{
		run_precise_pack_workload<8u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "precise12") == 0)
	{
		run_precise_pack_workload<12u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "precise16") == 0)
	{
		run_precise_pack_workload<16u>(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "reserve-scatter9") == 0)
	{
		run_scatter_plan_pack_workload(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "context3") == 0)
	{
		run_context_pack_workload(backend, operation, workload);
		return;
	}
	if (::std::strcmp(workload, "staged9") == 0)
	{
		run_staged_pack_workload(backend, operation, workload);
		return;
	}
	fail("unknown workload");
}

inline void print_usage(char const *program)
{
	::std::printf(
		"usage: %s BACKEND OPERATION WORKLOAD\n\n"
		"backends (print/println):\n"
		"  fake-only fake-write fake-scatter obuffer\n"
#if !defined(FAST_IO_PRINT_CONCEPT_BASELINE)
		"  large-fake-observer large-obuffer-observer\n"
#endif
		"  ostring-std ostring-fast\n"
		"  dev-null-owner dev-null-observer obuf-owner obuf-ref\n"
		"  c-owner c-observer c-unlocked-owner c-unlocked-observer\n"
		"  filebuf-owner filebuf-observer\n"
		"backends (concat/concatln):\n"
		"  std-string fast-string\n"
		"operations:\n"
		"  print println concat concatln\n"
		"workloads:\n"
		"  leaf pack9 cond-pack width255 width256 width257 width-middle257 width-right257\n"
		"  width-internal257 width4095 width4096 width4097\n"
		"  rgvw1 rgvw16 rgvw128 rgvw512 rgvw-fixed1 rgvw-fixed16 rgvw-fixed128\n"
		"  rgvw16-adjacent rgvw128-adjacent rgvw16-framed rgvw128-framed\n"
		"  mixed dynamic9 precise2 precise4 precise8 precise12 precise16 reserve-scatter9 context3 staged9\n",
		program);
}

} // namespace

int main(int argc, char **argv)
{
	static_assert(iterations != 0u);
	if (argc == 2 && ::std::strcmp(argv[1], "--list") == 0)
	{
		print_usage(argv[0]);
		return 0;
	}
	if (argc != 4)
	{
		print_usage(argv[0]);
		return 2;
	}
	dispatch_workload(argv[1], argv[2], argv[3]);
}
