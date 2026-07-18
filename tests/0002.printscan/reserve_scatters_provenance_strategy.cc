#include <cassert>
#include <concepts>
#include <cstddef>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

inline char shared_scratch{};

struct static_scratch_plan
{
	char value;
};

inline constexpr ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(::fast_io::io_reserve_type_t<char, static_scratch_plan>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, static_scratch_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve, static_scratch_plan value) noexcept
{
	shared_scratch = value.value;
	*scatters++ = {__builtin_addressof(shared_scratch), 1u};
	return {scatters, reserve};
}

struct dynamic_scratch_plan
{
	char value;
};

struct self_borrowing_plan
{
	char value;
};

struct dynamic_self_borrowing_plan
{
	char value;
};

inline constexpr ::fast_io::reserve_scatters_size_result
	print_reserve_scatters_size(::fast_io::io_reserve_type_t<char, self_borrowing_plan>) noexcept
{
	return {1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(::fast_io::io_reserve_type_t<char, self_borrowing_plan>,
							  ::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
							  self_borrowing_plan &value) noexcept
{
	// This deliberately names producer-owned storage. It is valid for the caller-owned lvalue but would dangle if
	// entry normalization copied the producer and a retained concat plan outlived that copy.
	*scatters++ = {__builtin_addressof(value.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, self_borrowing_plan>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>,
	dynamic_self_borrowing_plan const &) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>) noexcept
{
	return 8u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>, char *iter,
	dynamic_self_borrowing_plan const &value) noexcept
{
	*iter++ = value.value;
	return iter;
}

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>,
	dynamic_self_borrowing_plan &) noexcept
{
	return {1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	dynamic_self_borrowing_plan &value) noexcept
{
	*scatters++ = {__builtin_addressof(value.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, dynamic_self_borrowing_plan>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_scratch_plan>, dynamic_scratch_plan) noexcept
{
	return 1u;
}

inline constexpr ::std::size_t
	print_reserve_static_stack_size(::fast_io::io_reserve_type_t<char, dynamic_scratch_plan>) noexcept
{
	return 8u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_scratch_plan>, char *iter,
	dynamic_scratch_plan value) noexcept
{
	*iter++ = value.value;
	return iter;
}

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, dynamic_scratch_plan>, dynamic_scratch_plan) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, dynamic_scratch_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve, dynamic_scratch_plan value) noexcept
{
	shared_scratch = value.value;
	*scatters++ = {__builtin_addressof(shared_scratch), 1u};
	return {scatters, reserve};
}

static_assert(::fast_io::reserve_scatters_printable<char, static_scratch_plan>);
static_assert(!::fast_io::borrowed_reserve_scatters_source<char, static_scratch_plan>);
static_assert(::fast_io::dynamic_reserve_scatters_printable<char, dynamic_scratch_plan>);
static_assert(!::fast_io::borrowed_reserve_scatters_source<char, dynamic_scratch_plan>);
static_assert(::fast_io::borrowed_reserve_scatters_source<char, ::std::source_location>);
static_assert(::fast_io::copy_stable_borrowed_print_source<char, ::std::source_location>);
static_assert(::fast_io::reserve_scatters_printable<char, self_borrowing_plan &>);
static_assert(::fast_io::borrowed_reserve_scatters_source<char, self_borrowing_plan>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<char, self_borrowing_plan>);

using normalized_self_borrowing_plan =
	decltype(::fast_io::io_print_forward<char>(::std::declval<self_borrowing_plan &>()));
static_assert(::std::same_as<normalized_self_borrowing_plan,
							 ::fast_io::parameter<self_borrowing_plan &>>);
// A parameter copy keeps the same referent, so repeated semantic normalization must not grow a nested wrapper graph.
static_assert(::fast_io::copy_stable_borrowed_print_source<char, normalized_self_borrowing_plan>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_forward<char>(
				  ::std::declval<normalized_self_borrowing_plan &>())),
			  normalized_self_borrowing_plan>);

static_assert(::fast_io::dynamic_reserve_scatters_printable<
			  char, dynamic_self_borrowing_plan &>);
static_assert(::fast_io::borrowed_reserve_scatters_source<
			  char, dynamic_self_borrowing_plan>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<
			  char, dynamic_self_borrowing_plan>);
using normalized_dynamic_self_borrowing_plan = decltype(::fast_io::io_print_forward<char>(::std::declval<dynamic_self_borrowing_plan &>()));
static_assert(::std::same_as<normalized_dynamic_self_borrowing_plan,
							 ::fast_io::parameter<dynamic_self_borrowing_plan &>>);
static_assert(::fast_io::copy_stable_borrowed_print_source<
			  char, normalized_dynamic_self_borrowing_plan>);

struct capture_state
{
	::std::string output;
	::std::size_t scatter_calls{};
	::std::size_t maximum_scatter_count{};
};

struct capture_sink
{
	using output_char_type = char;
	capture_state *state;
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(capture_sink sink, char const *first, char const *last)
{
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(
	capture_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	++sink.state->scatter_calls;
	if (sink.state->maximum_scatter_count < count)
	{
		sink.state->maximum_scatter_count = count;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->output.append(scatters[i].base, scatters[i].len);
	}
}

template <typename... Args>
inline capture_state render(Args &&...args)
{
	capture_state state;
	::fast_io::print(capture_sink{__builtin_addressof(state)}, ::std::forward<Args>(args)...);
	return state;
}

inline void test_static_plan_is_consumed_immediately()
{
	auto state{render(static_scratch_plan{'A'}, static_scratch_plan{'B'})};
	assert(state.output == "AB");
	assert(state.maximum_scatter_count <= 1u);
	assert(::fast_io::concat_std(static_scratch_plan{'A'}, static_scratch_plan{'B'}) == "AB");
}

inline void test_dynamic_plan_uses_contiguous_fallback()
{
	auto state{render(dynamic_scratch_plan{'A'}, dynamic_scratch_plan{'B'})};
	assert(state.output == "AB");
	// An unmarked alternate descriptor plan cannot pre-empt the canonical contiguous dynamic-reserve protocol.
	assert(state.maximum_scatter_count <= 1u);
	assert(::fast_io::concat_std(dynamic_scratch_plan{'A'}, dynamic_scratch_plan{'B'}) == "AB");
}

inline void test_self_borrowing_lvalue_preserves_identity()
{
	self_borrowing_plan first{'A'};
	self_borrowing_plan second{'B'};
	normalized_self_borrowing_plan wrapped{first};

	auto direct_state{render(first, second)};
	assert(direct_state.output == "AB");
	assert(direct_state.scatter_calls == 1u);
	assert(direct_state.maximum_scatter_count == 2u);
	auto wrapped_state{render(wrapped, second)};
	assert(wrapped_state.output == "AB");
	assert(wrapped_state.scatter_calls == 1u);
	assert(wrapped_state.maximum_scatter_count == 2u);
	assert(::fast_io::concat_std(first, second) == "AB");
	assert(::fast_io::concat_std(wrapped, second) == "AB");

	// The transparent adapter must call the owned wrapper in place. Returning a pointer into an adapter-local wrapper
	// copy would make even immediate descriptor consumption invalid as soon as the CPO returned.
	::fast_io::parameter<self_borrowing_plan> owned{{'O'}};
	::fast_io::basic_io_scatter_t<char> scatter;
	char unused_reserve{};
	auto const result{print_reserve_scatters_define(
		::fast_io::io_reserve_type<char, decltype(owned)>, __builtin_addressof(scatter),
		__builtin_addressof(unused_reserve), owned)};
	assert(result.scatters_pos_ptr == __builtin_addressof(scatter) + 1u);
	assert(result.reserve_pos_ptr == __builtin_addressof(unused_reserve));
	assert(scatter.base == __builtin_addressof(owned.reference.value));
	assert(scatter.len == 1u);
}

inline void test_dynamic_self_borrowing_lvalue_preserves_identity()
{
	dynamic_self_borrowing_plan first{'C'};
	dynamic_self_borrowing_plan second{'D'};
	auto state{render(first, second)};
	assert(state.output == "CD");
	assert(state.scatter_calls == 1u);
	assert(state.maximum_scatter_count == 2u);
	// Concat deliberately uses the canonical contiguous dynamic-reserve protocol, but entry normalization must remain
	// valid independent of which destination strategy is selected after the character type becomes known.
	assert(::fast_io::concat_std(first, second) == "CD");
}

} // namespace

int main()
{
	test_static_plan_is_consumed_immediately();
	test_dynamic_plan_uses_contiguous_fallback();
	test_self_borrowing_lvalue_preserves_identity();
	test_dynamic_self_borrowing_lvalue_preserves_identity();
}
