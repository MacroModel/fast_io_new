#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct buffered_state
{
	char *begin{};
	char *current{};
	char *end{};
	::std::size_t commits{};
	::std::size_t overflow_calls{};
};

struct buffered_sink
{
	using output_char_type = char;
	buffered_state *state;
};

inline constexpr buffered_sink output_stream_ref_define(buffered_sink sink) noexcept
{
	return sink;
}

inline constexpr char *obuffer_begin(buffered_sink sink) noexcept
{
	return sink.state->begin;
}

inline constexpr char *obuffer_curr(buffered_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(buffered_sink sink) noexcept
{
	return sink.state->end;
}

inline constexpr void obuffer_set_curr(buffered_sink sink, char *current) noexcept
{
	++sink.state->commits;
	sink.state->current = current;
}

inline void write_all_overflow_define(buffered_sink sink, char const *, char const *) noexcept
{
	++sink.state->overflow_calls;
	// Every run in this test has a put area large enough for its complete payload. Reaching the overflow primitive would
	// mean the grouped scatter drain failed to preserve the ordinary exact-fit buffered contract.
	::fast_io::fast_terminate();
}

struct producer_trace
{
	::std::array<::std::size_t, 8u> calls{};
	::std::array<::std::size_t, 16u> order{};
	::std::size_t order_size{};
};

struct retained_token
{
	char left;
	char right;
	producer_trace *trace;
	::std::size_t id;

	// A nontrivial lvalue is transported through `parameter<T&>`. The positive test therefore proves that the retained
	// run unwraps the one entry-owned transport instead of copying a producer whose descriptors point into itself.
	inline ~retained_token()
	{}
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, retained_token>) noexcept
{
	// The producer deliberately consumes only three descriptors and one reserve character. Spare capacity proves that
	// the next component receives a slice beginning at the validated actual cursor, not at an assumed full capacity.
	return {4u, 2u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, retained_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve, retained_token &token) noexcept
{
	++token.trace->calls[token.id];
	token.trace->order[token.trace->order_size++] = token.id;
	*scatters++ = {__builtin_addressof(token.left), 1u};
	*reserve = '|';
	*scatters++ = {reserve, 1u};
	*scatters++ = {__builtin_addressof(token.right), 1u};
	return {scatters, reserve + 1u};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, retained_token>) noexcept
{
	// The character descriptors name the caller-owned token, while the delimiter names the component's caller-owned
	// reserve slice. Neither is invalidated when a later producer is invoked.
	return {};
}

inline char shared_scratch{};

struct unretained_consuming_token
{
	char value;
	::std::size_t *calls;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, unretained_consuming_token>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, unretained_consuming_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	unretained_consuming_token token) noexcept
{
	++*token.calls;
	shared_scratch = token.value;
	*scatters++ = {__builtin_addressof(shared_scratch), 1u};
	return {scatters, reserve};
}

struct potentially_throwing_retained_token
{
	char value;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, potentially_throwing_retained_token>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, potentially_throwing_retained_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	potentially_throwing_retained_token const &token)
{
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, potentially_throwing_retained_token>) noexcept
{
	return {};
}

struct oversized_retained_token
{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, oversized_retained_token>) noexcept
{
	constexpr ::std::size_t over_local_budget{
		(4u * 1024u) / sizeof(::fast_io::basic_io_scatter_t<char>) + 1u};
	return {over_local_budget, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, oversized_retained_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	oversized_retained_token) noexcept
{
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, oversized_retained_token>) noexcept
{
	return {};
}

static_assert(::fast_io::reserve_scatters_printable<char, retained_token>);
static_assert(::fast_io::borrowed_reserve_scatters_source<char, retained_token>);
static_assert(::fast_io::details::decay::print_retained_buffered_reserve_scatters_capability_v<
			  char, retained_token>);
static_assert(::fast_io::details::decay::print_retained_buffered_reserve_scatters_lifetime_v<
			  char, retained_token>);
static_assert(::fast_io::details::decay::print_retained_buffered_reserve_scatters_nothrow_v<
			  char, retained_token>);
static_assert(::fast_io::details::decay::print_retained_buffered_reserve_scatters_run_selected<
	false, char, buffered_sink, retained_token, retained_token>());
static_assert(!::fast_io::details::decay::print_retained_buffered_reserve_scatters_run_selected<
	false, char, buffered_sink, retained_token>());

static_assert(::fast_io::reserve_scatters_printable<char, unretained_consuming_token>);
static_assert(!::fast_io::borrowed_reserve_scatters_source<char, unretained_consuming_token>);
static_assert(!::fast_io::details::decay::print_retained_buffered_reserve_scatters_run_selected<
	false, char, buffered_sink, unretained_consuming_token, unretained_consuming_token>());

static_assert(::fast_io::borrowed_reserve_scatters_source<char, potentially_throwing_retained_token>);
static_assert(!::fast_io::details::decay::print_retained_buffered_reserve_scatters_nothrow_v<
			  char, potentially_throwing_retained_token>);
static_assert(!::fast_io::details::decay::print_retained_buffered_reserve_scatters_run_selected<
	false, char, buffered_sink, potentially_throwing_retained_token,
	potentially_throwing_retained_token>());

static_assert(::fast_io::reserve_scatters_printable<char, oversized_retained_token>);
static_assert(!::fast_io::details::decay::print_retained_buffered_reserve_scatters_run_selected<
	false, char, buffered_sink, oversized_retained_token, oversized_retained_token>());

template <::std::size_t size>
struct fixed_buffer
{
	::std::array<char, size> storage{};
	buffered_state state{storage.data(), storage.data(), storage.data() + storage.size()};

	[[nodiscard]] inline ::std::string_view view() const noexcept
	{
		return {storage.data(), static_cast<::std::size_t>(state.current - state.begin)};
	}
};

inline void test_adjacent_retained_run_is_materialized_once()
{
	producer_trace trace{};
	retained_token first{'A', 'a', __builtin_addressof(trace), 0u};
	retained_token second{'B', 'b', __builtin_addressof(trace), 1u};
	retained_token third{'C', 'c', __builtin_addressof(trace), 2u};
	fixed_buffer<32u> output;
	::fast_io::print(buffered_sink{__builtin_addressof(output.state)}, first, second, third);
	assert(output.view() == "A|aB|bC|c");
	assert(output.state.commits == 1u);
	assert(output.state.overflow_calls == 0u);
	assert(trace.calls[0] == 1u && trace.calls[1] == 1u && trace.calls[2] == 1u);
	assert(trace.order_size == 3u);
	assert(trace.order[0] == 0u && trace.order[1] == 1u && trace.order[2] == 2u);
}

inline void test_semantic_pack_reaches_the_same_buffered_run()
{
	producer_trace trace{};
	auto value{::fast_io::mnp::pack(
		retained_token{'D', 'd', __builtin_addressof(trace), 0u},
		retained_token{'E', 'e', __builtin_addressof(trace), 1u},
		retained_token{'F', 'f', __builtin_addressof(trace), 2u})};
	fixed_buffer<32u> output;
	::fast_io::print(buffered_sink{__builtin_addressof(output.state)}, value);
	assert(output.view() == "D|dE|eF|f");
	assert(output.state.commits == 1u);
	assert(trace.calls[0] == 1u && trace.calls[1] == 1u && trace.calls[2] == 1u);
}

inline void test_exact_fit_line_uses_one_commit()
{
	producer_trace trace{};
	retained_token first{'G', 'g', __builtin_addressof(trace), 0u};
	retained_token second{'H', 'h', __builtin_addressof(trace), 1u};
	// Two three-character tokens plus the newline consume the entire put area exactly.
	fixed_buffer<7u> output;
	::fast_io::println(buffered_sink{__builtin_addressof(output.state)}, first, second);
	assert(output.view() == "G|gH|h\n");
	assert(output.state.current == output.state.end);
	assert(output.state.commits == 1u);
	assert(trace.calls[0] == 1u && trace.calls[1] == 1u);
}

inline void test_unretained_consuming_sources_remain_immediate()
{
	::std::size_t calls{};
	fixed_buffer<8u> output;
	::fast_io::print(buffered_sink{__builtin_addressof(output.state)},
		unretained_consuming_token{'A', __builtin_addressof(calls)},
		unretained_consuming_token{'B', __builtin_addressof(calls)});
	assert(output.view() == "AB");
	assert(calls == 2u);
	assert(output.state.commits == 2u);
}

inline void test_potentially_throwing_producers_keep_the_old_boundary()
{
	fixed_buffer<8u> output;
	::fast_io::print(buffered_sink{__builtin_addressof(output.state)},
		potentially_throwing_retained_token{'I'}, potentially_throwing_retained_token{'J'});
	assert(output.view() == "IJ");
	assert(output.state.commits == 2u);
}

inline void test_retained_prefix_then_unretained_tail()
{
	producer_trace trace{};
	::std::size_t tail_calls{};
	retained_token first{'K', 'k', __builtin_addressof(trace), 0u};
	retained_token second{'L', 'l', __builtin_addressof(trace), 1u};
	fixed_buffer<16u> output;
	::fast_io::print(buffered_sink{__builtin_addressof(output.state)}, first, second,
		unretained_consuming_token{'M', __builtin_addressof(tail_calls)});
	assert(output.view() == "K|kL|lM");
	assert(output.state.commits == 2u);
	assert(trace.calls[0] == 1u && trace.calls[1] == 1u);
	assert(tail_calls == 1u);
}

} // namespace

int main()
{
	test_adjacent_retained_run_is_materialized_once();
	test_semantic_pack_reaches_the_same_buffered_run();
	test_exact_fit_line_uses_one_commit();
	test_unretained_consuming_sources_remain_immediate();
	test_potentially_throwing_producers_keep_the_old_boundary();
	test_retained_prefix_then_unretained_tail();
}
