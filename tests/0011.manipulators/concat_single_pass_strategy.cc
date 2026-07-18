#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io_dsal/string.h>
#include <fast_io.h>

namespace
{

struct retained_text_plan
{
	::std::string_view first;
	::std::string_view second;
	::std::size_t *define_count;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, retained_text_plan>) noexcept
{
	// Capacity deliberately exceeds actual use so the concat plan must honor returned cursors rather than copying the
	// complete uninitialized capacity.
	return {5u, 3u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, retained_text_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	retained_text_plan token) noexcept
{
	++*token.define_count;
	reserve[0] = '<';
	reserve[1] = '>';
	*scatters++ = {reserve, 1u};
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {token.second.data(), token.second.size()};
	*scatters++ = {reserve + 1u, 1u};
	return {scatters, reserve + 2u};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, retained_text_plan>) noexcept
{
	// Input views outlive concat and reserve-backed descriptors name storage owned by the enclosing retained plan.
	return {};
}

struct unretained_scratch_plan
{
	char value;
	::std::size_t *define_count;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, unretained_scratch_plan>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, unretained_scratch_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	unretained_scratch_plan token) noexcept
{
	static char shared_scratch;
	++*token.define_count;
	shared_scratch = token.value;
	*scatters++ = {__builtin_addressof(shared_scratch), 1u};
	return {scatters, reserve};
}

struct context_text_token
{
	::std::string_view text;
	::std::size_t *starts;
	::std::size_t *emitted;
};

struct context_text_state
{
	::std::size_t offset{};
	bool started{};

	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		context_text_token token, char *first, char *last) noexcept
	{
		if (!started)
		{
			started = true;
			++*token.starts;
		}
		auto current{first};
		while (current != last && offset != token.text.size())
		{
			*current++ = token.text[offset++];
			++*token.emitted;
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
	// This is a refill-window bound only. The test output is intentionally longer than one window.
	return 2u;
}

struct status_forward_text
{
	char value;
	::std::size_t *forward_count;
};

inline status_forward_text status_io_print_forward(
	::fast_io::io_alias_type_t<char>, status_forward_text token) noexcept
{
	++*token.forward_count;
	return token;
}

struct counted_dynamic_text
{
	char value;
	::std::size_t *size_count;
	::std::size_t *define_count;
};

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, counted_dynamic_text>, counted_dynamic_text const &token) noexcept
{
	++*token.size_count;
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, counted_dynamic_text>, char *destination,
	counted_dynamic_text const &token) noexcept
{
	++*token.define_count;
	*destination = token.value;
	return destination + 1u;
}

struct counted_scatter_text
{
	char value;
	::std::size_t *scatter_count;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, counted_scatter_text>, counted_scatter_text const &token) noexcept
{
	++*token.scatter_count;
	return {__builtin_addressof(token.value), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, counted_scatter_text>) noexcept
{
	// The descriptor names the normalized owner's member, and observing it again produces the same one-byte sequence.
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, status_forward_text>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, status_forward_text>, char *destination,
	status_forward_text token) noexcept
{
	*destination = token.value;
	return destination + 1u;
}

struct descriptor_byte_overflow_plan
{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, descriptor_byte_overflow_plan>) noexcept
{
	return {SIZE_MAX / sizeof(::fast_io::basic_io_scatter_t<char>) + 1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, descriptor_byte_overflow_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	descriptor_byte_overflow_plan) noexcept
{
	return {scatters, reserve};
}

using retained_transport = decltype(::fast_io::io_print_forward<char>(::std::declval<retained_text_plan &>()));

static_assert(::fast_io::reserve_scatters_printable<char, retained_text_plan>);
static_assert(::fast_io::borrowed_reserve_scatters_source<char, retained_text_plan>);
static_assert(::fast_io::details::decay::concat_retained_reserve_scatters_run_v<
			  char, retained_transport, retained_transport>);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, retained_text_plan>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, retained_text_plan>::value);
static_assert(!::fast_io::borrowed_reserve_scatters_source<char, unretained_scratch_plan>);
static_assert(!::fast_io::details::decay::concat_retained_reserve_scatters_run_v<
			  char, unretained_scratch_plan, unretained_scratch_plan>);
// Allocation-byte representability is a base protocol invariant now, so this impossible plan is rejected before
// concat's retained-run policy is considered rather than being admitted and then filtered by a second calculation.
static_assert(!::fast_io::reserve_scatters_printable<char, descriptor_byte_overflow_plan>);
static_assert(!::fast_io::details::decay::concat_retained_reserve_scatters_run_v<
			  char, descriptor_byte_overflow_plan>);
static_assert(::fast_io::details::decay::basic_general_concat_context_staging_preferred_destination<
			  char, ::fast_io::string>);
static_assert(::fast_io::details::decay::basic_general_concat_context_staging_preferred_destination<
			  char, ::fast_io::tlc::string>);

inline constexpr ::fast_io::basic_io_scatter_t<char> cursor_probe[4]{};
static_assert(::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
	cursor_probe, cursor_probe + 2u, cursor_probe));
static_assert(::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
	cursor_probe, cursor_probe + 2u, cursor_probe + 2u));
static_assert(!::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
	cursor_probe, cursor_probe + 2u, cursor_probe + 3u));

void test_retained_pack_is_single_pass()
{
	::std::size_t define_count{};
	auto value{::fast_io::mnp::pack(
		retained_text_plan{"a", "A", __builtin_addressof(define_count)},
		retained_text_plan{"b", "B", __builtin_addressof(define_count)},
		retained_text_plan{"c", "C", __builtin_addressof(define_count)},
		retained_text_plan{"d", "D", __builtin_addressof(define_count)},
		retained_text_plan{"e", "E", __builtin_addressof(define_count)},
		retained_text_plan{"f", "F", __builtin_addressof(define_count)},
		retained_text_plan{"g", "G", __builtin_addressof(define_count)},
		retained_text_plan{"h", "H", __builtin_addressof(define_count)},
		retained_text_plan{"i", "I", __builtin_addressof(define_count)})};
	auto output{::fast_io::concat_fast_io(value)};
	assert(output == "<aA><bB><cC><dD><eE><fF><gG><hH><iI>");
	assert(define_count == 9u);
}

void test_unretained_scratch_is_consumed_immediately()
{
	::std::size_t define_count{};
	auto output{::fast_io::concat_fast_io(
		unretained_scratch_plan{'A', __builtin_addressof(define_count)},
		unretained_scratch_plan{'B', __builtin_addressof(define_count)})};
	// If concat retained these unmarked descriptors, both would observe the second invocation's shared scratch byte.
	assert(output == "AB");
	assert(define_count == 2u);
}

template <typename concat_function>
void test_context_staging_is_single_pass(concat_function concat)
{
	::std::size_t starts{};
	::std::size_t emitted{};
	auto value{::fast_io::mnp::pack(
		context_text_token{"context", __builtin_addressof(starts), __builtin_addressof(emitted)},
		context_text_token{"-state", __builtin_addressof(starts), __builtin_addressof(emitted)},
		context_text_token{"-windows", __builtin_addressof(starts), __builtin_addressof(emitted)})};
	auto output{concat(value)};
	assert(output == "context-state-windows");
	assert(starts == 3u);
	assert(emitted == ::std::string_view{"context-state-windows"}.size());
}

void test_public_wrapper_forwards_status_once()
{
	::std::size_t forward_count{};
	status_forward_text token{'Q', __builtin_addressof(forward_count)};
	auto output{::fast_io::concat_fast_io(token)};
	assert(output == "Q");
	assert(forward_count == 1u);
}

void test_mixed_plan_queries_each_object_once()
{
	::std::size_t size_count{};
	::std::size_t define_count{};
	::std::size_t scatter_count{};
	auto output{::fast_io::concat_fast_io(
		counted_dynamic_text{'D', __builtin_addressof(size_count), __builtin_addressof(define_count)},
		counted_scatter_text{'S', __builtin_addressof(scatter_count)})};
	assert(output == "DS");
	// Planning caches both the object-dependent reserve bound and retained descriptor. Neither CPO is replayed after
	// destination allocation, and the reserve producer itself is invoked exactly once during emission.
	assert(size_count == 1u);
	assert(define_count == 1u);
	assert(scatter_count == 1u);
}

} // namespace

int main()
{
	test_retained_pack_is_single_pass();
	test_unretained_scratch_is_consumed_immediately();
	test_context_staging_is_single_pass([](auto &value) {
		return ::fast_io::concat_fast_io(value);
	});
	test_context_staging_is_single_pass([](auto &value) {
		return ::fast_io::tlc::concat_fast_io_tlc(value);
	});
	test_public_wrapper_forwards_status_once();
	test_mixed_plan_queries_each_object_once();
}
