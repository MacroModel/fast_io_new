#include <cassert>
#include <cstddef>
#include <string_view>

#include "scan_concept_support.h"

namespace scan_strategy_edges
{

struct contiguous_target;

struct contiguous_proxy
{
	contiguous_target *target;
};

struct contiguous_target
{
	char const *reported{};
	::fast_io::parse_code code{::fast_io::parse_code::ok};
};

inline constexpr contiguous_proxy scan_alias_define(::fast_io::io_alias_t, contiguous_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, contiguous_proxy>, char const *first, char const *,
	contiguous_proxy &proxy) noexcept
{
	return {proxy.target->reported == nullptr ? first : proxy.target->reported, proxy.target->code};
}

struct dual_source;

struct dual_ref
{
	using input_char_type = char;
	dual_source *source;
};

struct dual_unlocked_ref
{
	using input_char_type = char;
	dual_source *source;
};

struct dual_mutex
{
	dual_source *source;
	void lock() const noexcept;
	void unlock() const noexcept;
};

struct dual_source
{
	bool locked{};
	bool outer_status_called{};
	bool unlocked_status_called{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
};

inline void dual_mutex::lock() const noexcept
{
	assert(!source->locked);
	source->locked = true;
	++source->lock_calls;
}

inline void dual_mutex::unlock() const noexcept
{
	assert(source->locked);
	source->locked = false;
	++source->unlock_calls;
}

inline constexpr dual_ref input_stream_ref_define(dual_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline constexpr dual_mutex input_stream_mutex_ref_define(dual_ref source) noexcept
{
	return {source.source};
}

inline constexpr dual_unlocked_ref input_stream_unlocked_ref_define(dual_ref source) noexcept
{
	return {source.source};
}

inline bool status_scan_define(
	dual_ref source, ::scan_concept_harness::status_proxy &proxy) noexcept
{
	// This shortcut deliberately exists on the lockable observer. Correct routing must never call it without first
	// descending through the mutex/unlocked protocol.
	source.source->outer_status_called = true;
	proxy.target->value = true;
	return true;
}

inline bool status_scan_define(
	dual_unlocked_ref source, ::scan_concept_harness::status_proxy &proxy) noexcept
{
	assert(source.source->locked);
	source.source->unlocked_status_called = true;
	proxy.target->value = true;
	return true;
}

struct io_only_source
{
	bool called{};
};

struct io_only_ref
{
	using input_char_type = char;
	io_only_source *source;
};

inline constexpr io_only_ref io_stream_ref_define(io_only_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline bool status_scan_define(
	io_only_ref source, ::scan_concept_harness::status_proxy &proxy) noexcept
{
	source.source->called = true;
	proxy.target->value = true;
	return true;
}

struct throwing_precise_target;

struct throwing_precise_proxy
{
	throwing_precise_target *target;
};

struct throwing_precise_target
{
	bool should_throw{};
	::std::size_t calls{};
};

inline constexpr throwing_precise_proxy
scan_alias_define(::fast_io::io_alias_t, throwing_precise_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct cursor_observing_target;

struct cursor_observing_proxy
{
	cursor_observing_target *target;
};

struct cursor_observing_target
{
	char const *const *published_cursor{};
	char const *expected{};
	bool observed_scalar_schedule{};
};

inline constexpr cursor_observing_proxy scan_alias_define(
	::fast_io::io_alias_t, cursor_observing_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, cursor_observing_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, cursor_observing_proxy>, char const *,
	cursor_observing_proxy &proxy) noexcept
{
	proxy.target->observed_scalar_schedule =
		*proxy.target->published_cursor == proxy.target->expected;
}

// One observer type deliberately supports both modes. Presence of `ibuffer_underflow_never` is therefore only a
// query capability; its false result must keep a hybrid on context dispatch and reject a contiguous-only scanner.
struct runtime_terminal_input
{
	using input_char_type = char;
	char const *begin{};
	char const *current{};
	char const *end{};
	bool terminal{};
	::std::size_t set_calls{};
};

struct runtime_terminal_input_ref
{
	using input_char_type = char;
	runtime_terminal_input *input;
};

inline constexpr runtime_terminal_input_ref input_stream_ref_define(runtime_terminal_input &input) noexcept
{
	return {__builtin_addressof(input)};
}

inline constexpr char const *ibuffer_begin(runtime_terminal_input_ref ref) noexcept
{
	return ref.input->begin;
}
inline constexpr char const *ibuffer_curr(runtime_terminal_input_ref ref) noexcept
{
	return ref.input->current;
}
inline constexpr char const *ibuffer_end(runtime_terminal_input_ref ref) noexcept
{
	return ref.input->end;
}
inline constexpr void ibuffer_set_curr(runtime_terminal_input_ref ref, char const *current) noexcept
{
	ref.input->current = current;
	++ref.input->set_calls;
}
inline constexpr bool ibuffer_underflow(runtime_terminal_input_ref) noexcept
{
	return false;
}
inline constexpr bool ibuffer_underflow_never(runtime_terminal_input_ref ref) noexcept
{
	return ref.input->terminal;
}

struct empty_contiguous_target
{
	bool called{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, ::fast_io::parameter<empty_contiguous_target &>>,
	char const *first, char const *last, ::fast_io::parameter<empty_contiguous_target &> target) noexcept
{
	target.reference.called = true;
	return {last == first ? first : last, ::fast_io::parse_code::ok};
}

inline constexpr ::std::size_t
	scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, throwing_precise_proxy>) noexcept
{
	return 1u;
}

inline void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_precise_proxy>, char const *, throwing_precise_proxy &proxy)
{
	++proxy.target->calls;
	if (proxy.target->should_throw)
	{
#if defined(__cpp_exceptions)
		throw 1;
#else
		::fast_io::fast_terminate();
#endif
	}
}

struct iterative_only
{};

inline constexpr void scan_iterative_init_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &) noexcept
{}

inline constexpr ::fast_io::parse_result<char const *> scan_iterative_next_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &, char const *first,
	char const *) noexcept
{
	return {first, ::fast_io::parse_code::end_of_file};
}

inline constexpr ::fast_io::parse_code scan_iterative_eof_define(
	::fast_io::io_reserve_type_t<char, iterative_only>, iterative_only &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

static_assert(::fast_io::iterative_scannable<char, iterative_only>);
static_assert(!::fast_io::precise_reserve_scannable<char, iterative_only>);
static_assert(!::fast_io::contiguous_scannable<char, iterative_only>);
static_assert(!::fast_io::context_scannable<char, iterative_only>);

struct individually_valid_huge_precise
{};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, individually_valid_huge_precise>) noexcept
{
	return static_cast<::std::size_t>(PTRDIFF_MAX) - 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, individually_valid_huge_precise>, char const *,
	individually_valid_huge_precise &) noexcept
{}

static_assert(::fast_io::precise_reserve_scannable_no_error<char, individually_valid_huge_precise>);
static_assert(!::fast_io::aggregate_commit_safe_precise_reserve_scannable<
			  char, cursor_observing_proxy>);
static_assert(::fast_io::terminal_contiguous_context_scannable<
			  char, ::scan_concept_harness::literal_proxy<true>>);
inline constexpr auto throwing_batch_probe{
	::fast_io::details::decay::find_continuous_precise_scan_n<
		char, throwing_precise_proxy, throwing_precise_proxy, throwing_precise_proxy>()};
// Throwing exact-void scanners may share the availability proof because the unmarked body retains scalar cursor
// publications. They must never acquire the stronger apply-all-then-commit schedule from that optimization alone.
static_assert(throwing_batch_probe.position == 3u && throwing_batch_probe.neededspace == 3u);
static_assert(!throwing_batch_probe.aggregate_commit_safe);
inline constexpr auto marked_prefix_context_probe{
	::fast_io::details::decay::find_continuous_precise_scan_n<
		char, ::scan_concept_harness::fixed_record_proxy<1u>,
		::scan_concept_harness::fixed_record_proxy<1u>,
		::scan_concept_harness::literal_proxy<false>>()};
// A non-precise suffix terminates the run; its default policy bit is not part of the marked prefix and must not turn
// that prefix into the scalar-commit strategy.
static_assert(marked_prefix_context_probe.position == 2u && marked_prefix_context_probe.neededspace == 2u);
static_assert(marked_prefix_context_probe.aggregate_commit_safe);
inline constexpr auto overflowing_batch_probe{
	::fast_io::details::decay::find_continuous_precise_scan_n<
		char, individually_valid_huge_precise, individually_valid_huge_precise>()};
// Each scanner owns a valid pointer-domain extent, but the pair does not. Batch discovery must retain a scalar prefix
// instead of evaluating a fatal checked addition; ordinary scalar dispatch can then process each protocol separately.
static_assert(overflowing_batch_probe.position == 1u);
static_assert(
	overflowing_batch_probe.neededspace == static_cast<::std::size_t>(PTRDIFF_MAX) - 1u);

} // namespace scan_strategy_edges

namespace
{

inline bool throws_parse_error(auto &&operation, ::fast_io::parse_code expected)
{
#if defined(__cpp_exceptions)
	try
	{
		operation();
	}
	catch (::fast_io::error const &error)
	{
		return error.domain == ::fast_io::parse_domain_value &&
			   error.code == static_cast<::std::size_t>(static_cast<char8_t>(expected));
	}
	return false;
#else
	(void)operation;
	(void)expected;
	return true;
#endif
}

inline void test_checked_iterators_and_progress()
{
	char storage[]{'L', 'x', 'R'};
	::fast_io::basic_ibuffer_view<char> escaped_input(storage + 1u, storage + 2u);
	::scan_strategy_edges::contiguous_target escaped{storage + 3u};
	assert(throws_parse_error(
		[&] { (void)::fast_io::io::scan<true>(escaped_input, escaped); }, ::fast_io::parse_code::invalid));
	assert(escaped_input.curr_ptr == storage + 1u);

	::fast_io::basic_ibuffer_view<char> partial_input(storage + 1u, storage + 2u);
	::scan_strategy_edges::contiguous_target partial{storage + 1u, ::fast_io::parse_code::partial};
	assert(!::fast_io::io::scan<true>(partial_input, partial));
	assert(partial_input.curr_ptr == storage + 1u);

	::fast_io::basic_ibuffer_view<char> context_input(storage + 1u, storage + 2u);
	::scan_concept_harness::escaped_context_target escaped_context;
	escaped_context.reported_iterator = storage + 3u;
	assert(throws_parse_error(
		[&] { (void)::fast_io::io::scan<true>(context_input, escaped_context); },
		::fast_io::parse_code::invalid));
	assert(context_input.curr_ptr == storage + 1u);

	::fast_io::basic_ibuffer_view<char> stalled_input(storage + 1u, storage + 2u);
	::scan_concept_harness::stalled_context_target stalled;
	assert(throws_parse_error(
		[&] { (void)::fast_io::io::scan<true>(stalled_input, stalled); }, ::fast_io::parse_code::invalid));
	assert(stalled_input.curr_ptr == storage + 1u);
}

inline void test_lock_and_reference_routing()
{
	::scan_strategy_edges::dual_source source;
	::scan_concept_harness::status_target target;
	assert(::fast_io::io::scan<true>(source, target));
	assert(target.value && source.unlocked_status_called && !source.outer_status_called);
	assert(source.lock_calls == 1u && source.unlock_calls == 1u && !source.locked);

	::scan_strategy_edges::io_only_source io_source;
	::scan_concept_harness::status_target io_target;
	assert(::fast_io::io::scan<true>(io_source, io_target));
	assert(io_source.called && io_target.value);
}

inline void test_throwing_precise_batch_cursor()
{
#if defined(__cpp_exceptions)
	char storage[]{'A', 'B', 'C'};
	::fast_io::basic_ibuffer_view<char> input(storage, storage + 3u);
	::scan_strategy_edges::throwing_precise_target first;
	::scan_strategy_edges::throwing_precise_target second{true};
	::scan_strategy_edges::throwing_precise_target third;
	bool threw{};
	try
	{
		(void)::fast_io::io::scan<true>(input, first, second, third);
	}
	catch (int)
	{
		threw = true;
	}
	assert(threw);
	assert(input.curr_ptr == storage + 2u);
	assert(first.calls == 1u && second.calls == 1u && third.calls == 0u);
#endif
}

inline void test_precise_batch_cursor_schedule()
{
	char storage[]{'A', 'B', 'C'};
	::scan_strategy_edges::runtime_terminal_input input{
		storage, storage, storage + 3u, true};
	::scan_strategy_edges::cursor_observing_target first{
		__builtin_addressof(input.current), storage + 1u};
	::scan_strategy_edges::cursor_observing_target second{
		__builtin_addressof(input.current), storage + 2u};
	::scan_strategy_edges::cursor_observing_target third{
		__builtin_addressof(input.current), storage + 3u};
	assert(::fast_io::io::scan<true>(input, first, second, third));
	assert(first.observed_scalar_schedule && second.observed_scalar_schedule &&
		   third.observed_scalar_schedule);
	assert(input.current == input.end && input.set_calls == 3u);

	input.current = input.begin;
	input.set_calls = 0u;
	::scan_concept_harness::fixed_record_target<1u> marked_first;
	::scan_concept_harness::fixed_record_target<1u> marked_second;
	::scan_concept_harness::fixed_record_target<1u> marked_third;
	assert(::fast_io::io::scan<true>(input, marked_first, marked_second, marked_third));
	// All three scanner types opt into delayed commit, so the same availability proof publishes one aggregate cursor.
	assert(input.current == input.end && input.set_calls == 1u);
}

inline void test_terminal_query_and_empty_contiguous_semantics()
{
	char text[]{'x', '|'};
	::scan_strategy_edges::runtime_terminal_input dynamic_input{
		text, text, text + 2u, false};
	::scan_concept_harness::literal_target<true> hybrid;
	assert(::fast_io::io::scan<true>(dynamic_input, hybrid));
	assert(hybrid.contiguous_calls == 0u && hybrid.context_calls != 0u);

#if defined(__cpp_exceptions)
	dynamic_input.current = dynamic_input.begin;
	::scan_strategy_edges::contiguous_target contiguous;
	assert(throws_parse_error(
		[&] { (void)::fast_io::io::scan<true>(dynamic_input, contiguous); },
		::fast_io::parse_code::invalid));
	assert(dynamic_input.current == dynamic_input.begin);
#endif

	char sentinel{};
	::fast_io::basic_ibuffer_view<char> empty_input(
		__builtin_addressof(sentinel), __builtin_addressof(sentinel));
	::scan_strategy_edges::empty_contiguous_target empty;
	assert(::fast_io::io::scan<true>(empty_input, empty));
	assert(empty.called && empty_input.curr_ptr == empty_input.end_ptr);

	// The null pair is the canonical default empty-view representation. Its successful zero-width commit must retain
	// the cursor without evaluating either null-pointer subtraction or `nullptr + 0`.
	::scan_strategy_edges::runtime_terminal_input null_input{
		nullptr, nullptr, nullptr, true};
	::scan_strategy_edges::empty_contiguous_target null_empty;
	assert(::fast_io::io::scan<true>(null_input, null_empty));
	assert(null_empty.called && null_input.current == nullptr && null_input.set_calls == 1u);
}

inline void test_nonreporting_eof()
{
#if defined(__cpp_exceptions)
	::std::string_view empty;
	::fast_io::basic_ibuffer_view<char> input(empty);
	::scan_concept_harness::fixed_record_target<1u> target;
	assert(throws_parse_error(
		[&] { ::fast_io::io::scan<false>(input, target); }, ::fast_io::parse_code::end_of_file));
	assert(target.calls == 0u);
#endif
}

} // namespace

int main()
{
	test_checked_iterators_and_progress();
	test_lock_and_reference_routing();
	test_throwing_precise_batch_cursor();
	test_precise_batch_cursor_schedule();
	test_terminal_query_and_empty_contiguous_semantics();
	test_nonreporting_eof();
}
