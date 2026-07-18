#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "../../tests/0002.printscan/scan_concept_support.h"

#ifndef FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS
#define FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS 1000000u
#endif

namespace
{

using namespace ::scan_concept_harness;

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline void fake_observe(void const *address, ::std::size_t size) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	// This is the scan-side equivalent of a fake system-call sink: it forces state to be materialized but performs no
	// memory access.  Reporting it as a separate baseline exposes the common call/barrier cost in every result.
	__asm__ __volatile__("" : : "r"(address), "r"(size) : "memory");
#else
	(void)address;
	(void)size;
#endif
}

inline void consume(::std::size_t const &value) noexcept
{
	fake_observe(__builtin_addressof(value), sizeof(value));
}

// The frozen baseline's ibuffer detector required mutable cursors even for terminal input. A pointer-owning benchmark
// observer models a writable refill buffer and is valid under both that historical restriction and the current
// coherent const-or-mutable cursor protocol. Using one common observer avoids `-fpermissive` and keeps the compared
// machine code attributable to scan dispatch rather than to different compiler language modes.
struct mutable_terminal_input;

struct mutable_terminal_input_ref
{
	using input_char_type = char;
	mutable_terminal_input *input;
};

struct mutable_terminal_input
{
	using input_char_type = char;
	char *begin;
	char *current;
	char *end;
	::std::size_t set_calls{};
};

inline constexpr mutable_terminal_input_ref input_stream_ref_define(mutable_terminal_input &input) noexcept
{
	return {__builtin_addressof(input)};
}

[[maybe_unused]] inline constexpr char *ibuffer_begin(mutable_terminal_input_ref ref) noexcept
{
	return ref.input->begin;
}

inline constexpr char *ibuffer_curr(mutable_terminal_input_ref ref) noexcept
{
	return ref.input->current;
}

inline constexpr char *ibuffer_end(mutable_terminal_input_ref ref) noexcept
{
	return ref.input->end;
}

inline constexpr void ibuffer_set_curr(mutable_terminal_input_ref ref, char *current) noexcept
{
	ref.input->current = current;
	++ref.input->set_calls;
}

inline constexpr bool ibuffer_underflow(mutable_terminal_input_ref) noexcept
{
	return false;
}

inline constexpr bool ibuffer_underflow_never(mutable_terminal_input_ref) noexcept
{
	return true;
}

template <::std::size_t count>
struct fixed_record_pack_proxy
{
	::std::array<fixed_record_target<1u>, count> *targets;
};

template <::std::size_t count>
struct fixed_record_pack_target
{
	::std::array<fixed_record_target<1u>, count> *targets;
};

template <::std::size_t extent>
struct unmarked_fixed_record_target;

template <::std::size_t extent>
struct unmarked_fixed_record_proxy
{
	unmarked_fixed_record_target<extent> *target;
};

template <::std::size_t extent>
struct unmarked_fixed_record_target
{
	::std::array<char, extent> value{};
	::std::size_t calls{};
};

template <::std::size_t extent>
inline constexpr unmarked_fixed_record_proxy<extent>
scan_alias_define(::fast_io::io_alias_t, unmarked_fixed_record_target<extent> &target) noexcept
{
	return {__builtin_addressof(target)};
}

template <::std::size_t extent>
inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, unmarked_fixed_record_proxy<extent>>) noexcept
{
	return extent;
}

template <::std::size_t extent>
inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, unmarked_fixed_record_proxy<extent>>, char const *buffer,
	unmarked_fixed_record_proxy<extent> &proxy) noexcept
{
	for (::std::size_t i{}; i != extent; ++i)
	{
		proxy.target->value[i] = buffer[i];
	}
	++proxy.target->calls;
}

#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
static_assert(::fast_io::precise_reserve_scannable_no_error<
			  char, unmarked_fixed_record_proxy<1u>>);
static_assert(!::fast_io::aggregate_commit_safe_precise_reserve_scannable<
			  char, unmarked_fixed_record_proxy<1u>>);
#endif

template <::std::size_t count>
inline constexpr fixed_record_pack_proxy<count>
scan_alias_define(::fast_io::io_alias_t, fixed_record_pack_target<count> &target) noexcept
{
	return {target.targets};
}

template <::std::size_t count>
inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_record_pack_proxy<count>>) noexcept
{
	return count;
}

template <::std::size_t count>
inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_record_pack_proxy<count>>, char const *buffer,
	fixed_record_pack_proxy<count> &proxy) noexcept
{
	for (::std::size_t i{}; i != count; ++i)
	{
		(*proxy.targets)[i].value[0] = buffer[i];
		++(*proxy.targets)[i].calls;
	}
}

#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
static_assert(::fast_io::operations::decay::defines::has_ibuffer_underflow_never_define<
			  mutable_terminal_input_ref>);
#endif

template <typename function_type>
inline ::std::size_t benchmark_case(char const *name, function_type function)
{
	auto const begin{::std::chrono::steady_clock::now()};
	auto const checksum{function()};
	auto const end{::std::chrono::steady_clock::now()};
	consume(checksum);
	auto const elapsed{::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin).count()};
	auto const nanoseconds_per_operation{
		static_cast<double>(elapsed) / static_cast<double>(FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS)};
	::std::printf("%-28s %12.3f ns/op  checksum=%zu\n", name, nanoseconds_per_operation, checksum);
	return checksum;
}

template <typename preflight_type, typename function_type>
inline void benchmark_verified_case(char const *name, preflight_type preflight, function_type function,
									::std::size_t expected_checksum)
{
	// Protocol/strategy drift is rejected before the clock starts. The checksum comparison occurs after benchmark_case
	// has stopped its timer, so correctness validation contributes neither branches nor work to the measured interval.
	preflight();
	if (benchmark_case(name, function) != expected_checksum)
	{
		__builtin_trap();
	}
}

template <::std::size_t count>
inline ::std::size_t benchmark_precise_pack()
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, count> targets{};
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += scan_fixed_pack(input, targets);
		checksum += static_cast<unsigned char>(targets[count - 1u].value[0]);
		fake_observe(targets.data(), sizeof(targets));
	}
	return checksum;
}

template <::std::size_t count, typename operation_type>
inline void preflight_fixed_target_pack(operation_type operation, ::std::size_t expected_set_calls)
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, count> targets{};
	bool const completed{operation(input, targets)};
	bool targets_valid{true};
	for (::std::size_t i{}; i != count; ++i)
	{
		targets_valid = targets_valid && targets[i].calls == 1u && targets[i].value[0] == storage[i];
	}
	if (!completed || input.current != input.end || input.set_calls != expected_set_calls || !targets_valid)
	{
		__builtin_trap();
	}
}

template <::std::size_t count>
inline void preflight_precise_pack()
{
	preflight_fixed_target_pack<count>(
		[](auto &input, auto &targets) { return scan_fixed_pack(input, targets); }, 1u);
}

template <::std::size_t count, ::std::size_t... indices>
inline bool scan_unmarked_fixed_pack_impl(
	mutable_terminal_input &input, ::std::array<unmarked_fixed_record_target<1u>, count> &targets,
	::std::index_sequence<indices...>)
{
	return ::fast_io::io::scan<true>(input, targets[indices]...);
}

template <::std::size_t count>
inline bool scan_unmarked_fixed_pack(
	mutable_terminal_input &input, ::std::array<unmarked_fixed_record_target<1u>, count> &targets)
{
	return scan_unmarked_fixed_pack_impl(input, targets, ::std::make_index_sequence<count>{});
}

template <::std::size_t count>
inline constexpr unsigned char precise_pack_last_character() noexcept
{
	static_assert(count != 0u);
	return static_cast<unsigned char>('a' + (count - 1u) % 26u);
}

template <::std::size_t count>
inline constexpr ::std::size_t precise_pack_expected_checksum() noexcept
{
	return static_cast<::std::size_t>(FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS) *
		   (1u + precise_pack_last_character<count>());
}

template <::std::size_t count>
inline void preflight_unmarked_precise_pack()
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<unmarked_fixed_record_target<1u>, count> targets{};
	bool const completed{scan_unmarked_fixed_pack(input, targets)};
	bool targets_valid{true};
	for (::std::size_t i{}; i != count; ++i)
	{
		targets_valid = targets_valid && targets[i].calls == 1u && targets[i].value[0] == storage[i];
	}
	if (!completed || input.current != input.end || input.set_calls != count || !targets_valid)
	{
		__builtin_trap();
	}
}

template <::std::size_t count>
inline ::std::size_t benchmark_unmarked_precise_pack()
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<unmarked_fixed_record_target<1u>, count> targets{};
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += scan_unmarked_fixed_pack(input, targets);
		checksum += static_cast<unsigned char>(targets.back().value[0]);
		fake_observe(targets.data(), sizeof(targets));
	}
	return checksum;
}

template <::std::size_t count, ::std::size_t... indices>
inline bool scan_fixed_pack_direct_fold(
	mutable_terminal_input &input, ::std::array<fixed_record_target<1u>, count> &targets,
	::std::index_sequence<indices...>) noexcept
{
	static_assert(count != 0u);
	char *current{input.current};
	auto apply_one = [&]<::std::size_t index>() noexcept {
		fixed_record_proxy<1u> proxy{__builtin_addressof(targets[index])};
		scan_precise_reserve_define(
			::fast_io::io_reserve_type<char, fixed_record_proxy<1u>>, current, proxy);
		++current;
	};
	(apply_one.template operator()<indices>(), ...);
	input.current = current;
	return true;
}

// This is an assembly-control workload, not an alternative public scanner API. It performs the same no-error precise
// CPO calls and one aggregate cursor commit as the explicitly marked library strategy, but omits public aliasing,
// source normalization, and the aggregate bounds check. It is therefore a lower-bound code-shape control; a delta from
// the public path cannot be attributed to parameter forwarding alone.
template <::std::size_t count>
inline ::std::size_t benchmark_precise_pack_direct_fold()
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, count> targets{};
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += scan_fixed_pack_direct_fold(input, targets, ::std::make_index_sequence<count>{});
		checksum += static_cast<unsigned char>(targets[count - 1u].value[0]);
		fake_observe(targets.data(), sizeof(targets));
	}
	return checksum;
}

template <::std::size_t count>
inline void preflight_precise_pack_direct_fold()
{
	preflight_fixed_target_pack<count>(
		[](auto &input, auto &targets) {
			return scan_fixed_pack_direct_fold(input, targets, ::std::make_index_sequence<count>{});
		},
		0u);
}

// A semantic pack keeps an arbitrarily large homogeneous group behind one scanner object. It demonstrates the code
// shape available to a range/pack concept: one public parameter, one precise extent, and a producer-controlled loop.
// The grammar, target mutations, source commit, checksum, and observer barrier match benchmark_precise_pack exactly.
template <::std::size_t count>
inline ::std::size_t benchmark_precise_semantic_pack()
{
	static_assert(count != 0u);
	::std::array<char, count> storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, count> targets{};
	fixed_record_pack_target<count> pack{__builtin_addressof(targets)};
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += ::fast_io::io::scan<true>(input, pack);
		checksum += static_cast<unsigned char>(targets[count - 1u].value[0]);
		fake_observe(targets.data(), sizeof(targets));
	}
	return checksum;
}

template <::std::size_t count>
inline void preflight_precise_semantic_pack()
{
	preflight_fixed_target_pack<count>(
		[](auto &input, auto &targets) {
			fixed_record_pack_target<count> pack{__builtin_addressof(targets)};
			return ::fast_io::io::scan<true>(input, pack);
		},
		1u);
}

[[maybe_unused]] inline ::std::size_t benchmark_precise_scalar()
{
	::std::array<char, 4u> storage{'H', 'E', 'A', 'D'};
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	fixed_record_target<4u> target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += ::fast_io::io::scan<true>(input, target);
		checksum += static_cast<unsigned char>(target.value[3u]);
		fake_observe(__builtin_addressof(target), sizeof(target));
	}
	return checksum;
}

[[maybe_unused]] inline ::std::size_t benchmark_precise_refill()
{
	constexpr ::std::string_view text{"HEAD"};
	bounded_refill_source source;
	fixed_record_target<4u> target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		source.reset(text, 3u);
		checksum += ::fast_io::io::scan<true>(source, target);
		checksum += static_cast<unsigned char>(target.value[3u]);
		fake_observe(__builtin_addressof(target), sizeof(target));
	}
	return checksum;
}

inline constexpr ::std::size_t mixed_prefix_count{64u};
inline constexpr ::std::string_view mixed_prefix_suffix{"tail"};

#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
template <::std::size_t>
using one_byte_fixed_proxy = fixed_record_proxy<1u>;

template <::std::size_t... indices>
inline consteval auto marked_batch_probe(::std::index_sequence<indices...>)
{
	return ::fast_io::details::decay::find_continuous_precise_scan_n<
		char, one_byte_fixed_proxy<indices>...>();
}

template <::std::size_t... indices>
inline consteval auto marked_prefix_context_probe(::std::index_sequence<indices...>)
{
	return ::fast_io::details::decay::find_continuous_precise_scan_n<
		char, one_byte_fixed_proxy<indices>..., literal_proxy<false>>();
}

inline constexpr auto marked_pack_classification{
	marked_batch_probe(::std::make_index_sequence<mixed_prefix_count>{})};
inline constexpr auto mixed_prefix_classification{
	marked_prefix_context_probe(::std::make_index_sequence<mixed_prefix_count>{})};
static_assert(marked_pack_classification.position == mixed_prefix_count &&
			  marked_pack_classification.neededspace == mixed_prefix_count &&
			  marked_pack_classification.aggregate_commit_safe);
static_assert(mixed_prefix_classification.position == mixed_prefix_count &&
			  mixed_prefix_classification.neededspace == mixed_prefix_count &&
			  mixed_prefix_classification.aggregate_commit_safe);
#endif

template <::std::size_t... indices>
inline bool scan_mixed_prefix_terminal_impl(
	mutable_terminal_input &input,
	::std::array<fixed_record_target<1u>, mixed_prefix_count> &prefix_targets,
	literal_target<false> &suffix_target, ::std::index_sequence<indices...>)
{
	return ::fast_io::io::scan<true>(input, prefix_targets[indices]..., suffix_target);
}

inline bool scan_mixed_prefix_terminal(
	mutable_terminal_input &input,
	::std::array<fixed_record_target<1u>, mixed_prefix_count> &prefix_targets,
	literal_target<false> &suffix_target)
{
	return scan_mixed_prefix_terminal_impl(
		input, prefix_targets, suffix_target, ::std::make_index_sequence<mixed_prefix_count>{});
}

inline auto make_mixed_prefix_terminal_storage()
{
	::std::array<char, mixed_prefix_count + mixed_prefix_suffix.size() + 1u> storage{};
	for (::std::size_t i{}; i != mixed_prefix_count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	for (::std::size_t i{}; i != mixed_prefix_suffix.size(); ++i)
	{
		storage[mixed_prefix_count + i] = mixed_prefix_suffix[i];
	}
	storage.back() = '|';
	return storage;
}

inline void preflight_mixed_prefix_terminal()
{
	auto storage{make_mixed_prefix_terminal_storage()};
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, mixed_prefix_count> prefix_targets{};
	literal_target<false> suffix_target;
	bool const completed{scan_mixed_prefix_terminal(input, prefix_targets, suffix_target)};
	bool prefix_valid{true};
	for (::std::size_t i{}; i != mixed_prefix_count; ++i)
	{
		prefix_valid = prefix_valid && prefix_targets[i].calls == 1u &&
					   prefix_targets[i].value[0] == storage[i];
	}
	if (!completed || input.current != input.end || !prefix_valid ||
		!literal_equals(suffix_target, mixed_prefix_suffix) || suffix_target.contiguous_calls != 0u ||
		suffix_target.context_calls != mixed_prefix_suffix.size() + 1u || suffix_target.commits != 1u)
	{
		__builtin_trap();
	}
}

inline constexpr ::std::size_t mixed_prefix_expected_checksum() noexcept
{
	return static_cast<::std::size_t>(FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS) *
		   (1u + precise_pack_last_character<mixed_prefix_count>() + mixed_prefix_suffix.size());
}

[[maybe_unused]] inline ::std::size_t benchmark_mixed_prefix_terminal()
{
	auto storage{make_mixed_prefix_terminal_storage()};
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	::std::array<fixed_record_target<1u>, mixed_prefix_count> prefix_targets{};
	literal_target<false> suffix_target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += scan_mixed_prefix_terminal(input, prefix_targets, suffix_target);
		checksum += static_cast<unsigned char>(prefix_targets.back().value[0]) + suffix_target.size;
		fake_observe(prefix_targets.data(), sizeof(prefix_targets));
		fake_observe(__builtin_addressof(suffix_target), sizeof(suffix_target));
	}
	return checksum;
}

[[maybe_unused]] inline ::std::size_t benchmark_terminal_hybrid()
{
	::std::array<char, 23u> text{
		't', 'e', 'r', 'm', 'i', 'n', 'a', 'l', '-', 'l', 'i', 't', 'e', 'r', 'a', 'l', '|',
		's', 'u', 'f', 'f', 'i', 'x'};
	mutable_terminal_input input{text.data(), text.data(), text.data() + text.size()};
	literal_target<true> target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += ::fast_io::io::scan<true>(input, target);
		checksum += target.size;
		fake_observe(__builtin_addressof(target), sizeof(target));
	}
	return checksum;
}

inline void preflight_terminal_hybrid_dispatch()
{
	::std::array<char, 5u> text{'t', 'e', 's', 't', '|'};
	mutable_terminal_input input{text.data(), text.data(), text.data() + text.size()};
	literal_target<true> target;
	bool const completed{::fast_io::io::scan<true>(input, target)};
	if (!completed || target.contiguous_calls != 1u || target.context_calls != 0u || target.commits != 1u)
	{
		// Strategy names are part of this benchmark's evidence. Fail before any timer starts if concept admission drifts
		// and the nominal terminal/contiguous case silently becomes the refill/context state machine again.
		__builtin_trap();
	}
}

template <bool hybrid>
inline ::std::size_t benchmark_refill_context()
{
	constexpr ::std::string_view text{"refill-boundary-literal|suffix"};
	bounded_refill_source source;
	literal_target<hybrid> target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		source.reset(text, 3u);
		checksum += ::fast_io::io::scan<true>(source, target);
		checksum += target.size;
		fake_observe(__builtin_addressof(target), sizeof(target));
	}
	return checksum;
}

#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
[[maybe_unused]] inline ::std::size_t benchmark_reference_alias()
{
	::std::array<char, 1u> storage{'R'};
	mutable_terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	reference_alias_target target;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		input.current = input.begin;
		checksum += ::fast_io::io::scan<true>(input, target);
		checksum += static_cast<unsigned char>(target.value);
		fake_observe(__builtin_addressof(target), sizeof(target));
	}
	return checksum;
}
#endif

[[maybe_unused]] inline ::std::size_t benchmark_status()
{
	struct benchmark_state
	{
		status_source source;
		status_target target;
	} state;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		state.target.value = false;
		checksum += ::fast_io::io::scan<true>(state.source, state.target);
		checksum += state.target.value;
		fake_observe(__builtin_addressof(state), sizeof(state));
	}
	return checksum + state.source.calls;
}

[[maybe_unused]] inline ::std::size_t benchmark_locked_status()
{
	struct benchmark_state
	{
		locked_status_source source;
		status_target target;
	} state;
	state.source.source.lock_observation = __builtin_addressof(state.source.lock.locked);
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		state.target.value = false;
		checksum += ::fast_io::io::scan<true>(state.source, state.target);
		checksum += state.target.value;
		fake_observe(__builtin_addressof(state), sizeof(state));
	}
	return checksum + state.source.lock.lock_calls + state.source.lock.unlock_calls;
}

template <bool locked>
inline ::std::size_t benchmark_status_pack()
{
	using source_type = ::std::conditional_t<locked, locked_status_source, status_source>;
	struct benchmark_state
	{
		source_type source;
		::std::array<status_target, 9u> targets{};
	} state;
	if constexpr (locked)
	{
		state.source.source.lock_observation = __builtin_addressof(state.source.lock.locked);
	}
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		checksum += scan_status_pack(state.source, state.targets);
		checksum += state.targets.back().value;
		fake_observe(__builtin_addressof(state), sizeof(state));
	}
	if constexpr (locked)
	{
		return checksum + state.source.lock.lock_calls + state.source.lock.unlock_calls;
	}
	else
	{
		return checksum + state.source.calls;
	}
}

[[maybe_unused]] inline ::std::size_t benchmark_mixed_pack()
{
	constexpr ::std::string_view text{"HEADbody|"};
	bounded_refill_source source;
	fixed_record_target<4u> header;
	literal_target<false> body;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		source.reset(text, 3u);
		checksum += ::fast_io::io::scan<true>(source, header, body);
		checksum += static_cast<unsigned char>(header.value[3u]) + body.size;
		fake_observe(__builtin_addressof(body), sizeof(body));
	}
	return checksum;
}

[[maybe_unused]] inline ::std::size_t benchmark_context_pack()
{
	constexpr ::std::string_view text{"left|right|"};
	bounded_refill_source source;
	literal_target<false> left;
	literal_target<false> right;
	::std::size_t checksum{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		source.reset(text, 3u);
		checksum += ::fast_io::io::scan<true>(source, left, right);
		checksum += left.size + right.size;
		fake_observe(__builtin_addressof(right), sizeof(right));
	}
	return checksum;
}

[[maybe_unused]] inline ::std::size_t benchmark_fake_observer()
{
	::std::array<char, 32u> state{};
	for (::std::size_t iteration{}; iteration != FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS; ++iteration)
	{
		fake_observe(state.data(), state.size());
	}
	return FAST_IO_SCAN_CONCEPT_BENCH_ITERATIONS;
}

} // namespace

#if (defined(FAST_IO_SCAN_CONCEPT_BENCH_CONTEXT_ONLY) + defined(FAST_IO_SCAN_CONCEPT_BENCH_HYBRID_ONLY) +          \
	 defined(FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_ONLY) + defined(FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_REFILL_ONLY) +  \
	 defined(FAST_IO_SCAN_CONCEPT_BENCH_TERMINAL_ONLY) + defined(FAST_IO_SCAN_CONCEPT_BENCH_MIXED_PREFIX64_ONLY) + \
	 defined(FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT) + defined(FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT) +      \
	 defined(FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT) +                                                     \
	 defined(FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT)) > 1
#error "select exactly one scan concept benchmark specialization"
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_ONLY)
int main()
{
	benchmark_case("precise scalar (4 bytes)", benchmark_precise_scalar);
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_REFILL_ONLY)
int main()
{
	benchmark_case("precise refill (4 / 3+1)", benchmark_precise_refill);
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_TERMINAL_ONLY)
int main()
{
	preflight_terminal_hybrid_dispatch();
	benchmark_case("terminal hybrid/contiguous", benchmark_terminal_hybrid);
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_MIXED_PREFIX64_ONLY)
int main()
{
	benchmark_verified_case(
		"mixed prefix64->context", preflight_mixed_prefix_terminal,
		benchmark_mixed_prefix_terminal, mixed_prefix_expected_checksum());
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT)
int main()
{
	benchmark_verified_case(
		"unmarked precise pack",
		preflight_unmarked_precise_pack<FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT>,
		benchmark_unmarked_precise_pack<FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT>,
		precise_pack_expected_checksum<FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT>());
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT)
int main()
{
	benchmark_verified_case(
		"precise semantic pack",
		preflight_precise_semantic_pack<FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT>,
		benchmark_precise_semantic_pack<FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT>,
		precise_pack_expected_checksum<FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT>());
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT)
int main()
{
	benchmark_verified_case(
		"precise direct fold",
		preflight_precise_pack_direct_fold<FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT>,
		benchmark_precise_pack_direct_fold<FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT>,
		precise_pack_expected_checksum<FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT>());
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT)
int main()
{
	// A pack-only binary makes code-size and llvm-mca evidence independent of every other template instantiation.
	benchmark_verified_case(
		"precise pack", preflight_precise_pack<FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT>,
		benchmark_precise_pack<FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT>,
		precise_pack_expected_checksum<FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT>());
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_CONTEXT_ONLY)
int main()
{
	// A compile-time-only binary removes the other scanner instantiation from GCC's inlining and layout decisions.
	// Use it with the runtime selector to prove that a delta is not an order or code-placement effect.
	benchmark_case("refill context-only", benchmark_refill_context<false>);
}
#elif defined(FAST_IO_SCAN_CONCEPT_BENCH_HYBRID_ONLY)
int main()
{
	benchmark_case("refill hybrid->context", benchmark_refill_context<true>);
}
#else
int main(int argc, char **argv)
{
#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
	preflight_terminal_hybrid_dispatch();
#endif
	// CPU affinity is intentionally external.  The same binary can be pinned to each permitted physical P-core, while
	// correctness and fuzz builds may run on E-cores, without baking one machine's topology into the benchmark.
	::std::string_view const selected{argc > 1 ? argv[1] : "all"};
	bool ran{};
	auto run = [&]<typename function_type>(::std::string_view key, char const *label, function_type function) {
		if (selected == "all" || selected == key)
		{
			benchmark_case(label, function);
			ran = true;
		}
	};
	auto run_verified = [&]<typename preflight_type, typename function_type>(
							::std::string_view key, char const *label, preflight_type preflight, function_type function,
							::std::size_t expected_checksum) {
		if (selected == "all" || selected == key)
		{
			benchmark_verified_case(label, preflight, function, expected_checksum);
			ran = true;
		}
	};
	// Named cases can run in independent processes.  That mode rules out order, thermal history, and frequency changes
	// when comparing two concept classifications whose runtime state machine should otherwise be identical.
	run("barrier", "fake observer baseline", benchmark_fake_observer);
	run("precise", "precise scalar (4 bytes)", benchmark_precise_scalar);
	run("precise-refill", "precise refill (4 / 3+1)", benchmark_precise_refill);
	run_verified(
		"pack9", "precise pack (9 x 1)", preflight_precise_pack<9u>,
		benchmark_precise_pack<9u>, precise_pack_expected_checksum<9u>());
	// These boundaries distinguish the portable 16-object whole owner, Linux's 32-object owner, and the first bounded
	// large-pack fallback chunks. Keeping them as independently selectable cases makes an ABI threshold accident visible
	// without relying on interpolation between the small pack and the 64-target stress case.
	run_verified(
		"pack16", "precise pack (16 x 1)", preflight_precise_pack<16u>,
		benchmark_precise_pack<16u>, precise_pack_expected_checksum<16u>());
	run_verified(
		"pack17", "precise pack (17 x 1)", preflight_precise_pack<17u>,
		benchmark_precise_pack<17u>, precise_pack_expected_checksum<17u>());
	run_verified(
		"pack32", "precise pack (32 x 1)", preflight_precise_pack<32u>,
		benchmark_precise_pack<32u>, precise_pack_expected_checksum<32u>());
	run_verified(
		"pack33", "precise pack (33 x 1)", preflight_precise_pack<33u>,
		benchmark_precise_pack<33u>, precise_pack_expected_checksum<33u>());
	run_verified(
		"pack64", "precise pack (64 x 1)", preflight_precise_pack<64u>,
		benchmark_precise_pack<64u>, precise_pack_expected_checksum<64u>());
	run_verified(
		"pack256", "precise pack (256 x 1)", preflight_precise_pack<256u>,
		benchmark_precise_pack<256u>, precise_pack_expected_checksum<256u>());
	run_verified(
		"unmarked64", "unmarked pack (64 x 1)", preflight_unmarked_precise_pack<64u>,
		benchmark_unmarked_precise_pack<64u>, precise_pack_expected_checksum<64u>());
	run_verified(
		"unmarked256", "unmarked pack (256 x 1)", preflight_unmarked_precise_pack<256u>,
		benchmark_unmarked_precise_pack<256u>, precise_pack_expected_checksum<256u>());
	run_verified(
		"fold64", "direct-fold control (64 x 1)", preflight_precise_pack_direct_fold<64u>,
		benchmark_precise_pack_direct_fold<64u>, precise_pack_expected_checksum<64u>());
	run_verified(
		"fold256", "direct-fold control (256 x 1)", preflight_precise_pack_direct_fold<256u>,
		benchmark_precise_pack_direct_fold<256u>, precise_pack_expected_checksum<256u>());
	run_verified(
		"semantic64", "semantic pack (64 x 1)", preflight_precise_semantic_pack<64u>,
		benchmark_precise_semantic_pack<64u>, precise_pack_expected_checksum<64u>());
	run_verified(
		"semantic256", "semantic pack (256 x 1)", preflight_precise_semantic_pack<256u>,
		benchmark_precise_semantic_pack<256u>, precise_pack_expected_checksum<256u>());
#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
	// The frozen detector probes this input marker through output_stream_ref by mistake. Its dispatcher consequently
	// cannot prove terminal input and would measure the refill/context path under a false terminal label.
	run("terminal-hybrid", "terminal hybrid/contiguous", benchmark_terminal_hybrid);
#endif
	run("refill-context", "refill context-only", benchmark_refill_context<false>);
	run("refill-hybrid", "refill hybrid->context", benchmark_refill_context<true>);
	run("mixed", "mixed precise->context", benchmark_mixed_pack);
	run_verified(
		"mixed-prefix64-terminal", "mixed prefix64->context", preflight_mixed_prefix_terminal,
		benchmark_mixed_prefix_terminal, mixed_prefix_expected_checksum());
	run("context-pack", "context pack (2 tokens)", benchmark_context_pack);
#if !defined(FAST_IO_SCAN_CONCEPT_BASELINE)
	// The frozen baseline materialized every alias result by value and therefore cannot instantiate the intentionally
	// noncopyable reference protocol. Excluding only this proof case keeps all common dispatch timings comparable while
	// preserving the compile-time failure itself as correctness evidence for the current alias fix.
	run("alias", "noncopyable alias reference", benchmark_reference_alias);
#endif
	run("status", "status CPO", benchmark_status);
	run("locked-status", "mutex + status CPO", benchmark_locked_status);
	run("status-pack", "status pack (9 targets)", benchmark_status_pack<false>);
	run("locked-status-pack", "mutex + status pack (9)", benchmark_status_pack<true>);
	return ran ? 0 : 2;
}
#endif
