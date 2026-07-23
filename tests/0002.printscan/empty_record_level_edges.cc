#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>

#include <fast_io.h>

namespace
{

enum class mutex_event : unsigned char
{
	outer_lock,
	inner_lock,
	empty_status,
	inner_unlock,
	outer_unlock
};

struct mutex_state
{
	::std::array<mutex_event, 8u> events{};
	::std::size_t event_count{};
	unsigned lock_depth{};

	inline void push(mutex_event event) noexcept
	{
		assert(event_count != events.size());
		events[event_count++] = event;
	}
};

struct outer_mutex_ref
{
	mutex_state *state{};

	inline void lock() const noexcept
	{
		assert(state->lock_depth == 0u);
		state->push(mutex_event::outer_lock);
		++state->lock_depth;
	}

	inline void unlock() const noexcept
	{
		assert(state->lock_depth == 1u);
		--state->lock_depth;
		state->push(mutex_event::outer_unlock);
	}
};

struct inner_mutex_ref
{
	mutex_state *state{};

	inline void lock() const noexcept
	{
		assert(state->lock_depth == 1u);
		state->push(mutex_event::inner_lock);
		++state->lock_depth;
	}

	inline void unlock() const noexcept
	{
		assert(state->lock_depth == 2u);
		--state->lock_depth;
		state->push(mutex_event::inner_unlock);
	}
};

template <bool observes_empty>
struct terminal_sink
{
	using output_char_type = char;
	mutex_state *state{};
};

template <bool observes_empty>
struct inner_locked_sink
{
	using output_char_type = char;
	mutex_state *state{};
};

template <bool observes_empty>
struct outer_locked_sink
{
	using output_char_type = char;
	mutex_state *state{};
};

template <bool observes_empty>
inline constexpr outer_locked_sink<observes_empty> &output_stream_ref_define(
	outer_locked_sink<observes_empty> &sink) noexcept
{
	return sink;
}

template <bool observes_empty>
inline constexpr outer_mutex_ref output_stream_mutex_ref_define(
	outer_locked_sink<observes_empty> &sink) noexcept
{
	return {sink.state};
}

template <bool observes_empty>
inline constexpr inner_locked_sink<observes_empty>
output_stream_unlocked_ref_define(
	outer_locked_sink<observes_empty> &sink) noexcept
{
	return {sink.state};
}

template <bool observes_empty>
inline constexpr inner_mutex_ref output_stream_mutex_ref_define(
	inner_locked_sink<observes_empty> &sink) noexcept
{
	return {sink.state};
}

template <bool observes_empty>
inline constexpr terminal_sink<observes_empty>
output_stream_unlocked_ref_define(
	inner_locked_sink<observes_empty> &sink) noexcept
{
	return {sink.state};
}

template <bool line>
	requires(!line)
inline void status_print_define(terminal_sink<true> &sink) noexcept
{
	assert(sink.state->lock_depth == 2u);
	sink.state->push(mutex_event::empty_status);
}

using observable_outer = outer_locked_sink<true>;
using observable_inner = inner_locked_sink<true>;
using silent_outer = outer_locked_sink<false>;
using silent_inner = inner_locked_sink<false>;

static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<observable_outer>);
static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<observable_inner>);
static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<silent_outer>);
static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<silent_inner>);
static_assert(
	::fast_io::operations::decay::defines::
		empty_print_observable<observable_outer>);
static_assert(
	!::fast_io::operations::decay::defines::
		empty_print_observable<silent_outer>);

struct zero_physical_state
{
	::std::size_t empty_status_calls{};
	::std::size_t leaf_materializations{};
	::std::size_t plan_materializations{};
	::std::size_t scatter_calls{};
	::std::size_t bytes{};
};

struct zero_physical_sink
{
	using output_char_type = char;
	zero_physical_state *state{};
};

inline constexpr zero_physical_sink &output_stream_ref_define(
	zero_physical_sink &sink) noexcept
{
	return sink;
}

template <bool line>
	requires(!line)
inline void status_print_define(zero_physical_sink &sink) noexcept
{
	++sink.state->empty_status_calls;
}

inline void scatter_write_all_overflow_define(
	zero_physical_sink &sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->bytes += scatters[i].len;
	}
}

struct zero_byte_leaf
{
	zero_physical_state *state{};
	char storage{};
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, zero_byte_leaf>,
	zero_byte_leaf const &leaf) noexcept
{
	++leaf.state->leaf_materializations;
	return {__builtin_addressof(leaf.storage), 0u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, zero_byte_leaf>) noexcept
{
	return {};
}

struct zero_plan_source
{
	zero_physical_state *state{};
	char storage{};
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, zero_plan_source>,
	zero_plan_source const &source) noexcept
{
	++source.state->plan_materializations;
	return {__builtin_addressof(source.storage), 0u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, zero_plan_source>) noexcept
{
	return {};
}

static_assert(::fast_io::scatter_printable_for<char, zero_byte_leaf const &>);
static_assert(::fast_io::scatter_printable_for<char, zero_plan_source const &>);

inline void test_observable_two_mutex_empty_record()
{
	mutex_state state;
	observable_outer sink{__builtin_addressof(state)};
	::fast_io::io::print(sink);

	constexpr ::std::array expected{
		mutex_event::outer_lock, mutex_event::inner_lock,
		mutex_event::empty_status, mutex_event::inner_unlock,
		mutex_event::outer_unlock};
	assert(state.event_count == expected.size());
	for (::std::size_t i{}; i != expected.size(); ++i)
	{
		assert(state.events[i] == expected[i]);
	}
	assert(state.lock_depth == 0u);
}

inline void test_silent_two_mutex_empty_record()
{
	mutex_state state;
	silent_outer sink{__builtin_addressof(state)};
	::fast_io::io::print(sink);
	assert(state.event_count == 0u);
	assert(state.lock_depth == 0u);
}

inline void test_zero_byte_active_records()
{
	zero_physical_state state;
	zero_physical_sink sink{__builtin_addressof(state)};
	auto logically_empty_record{::fast_io::mnp::pack()};
	::fast_io::io::print(sink, logically_empty_record);
	assert(state.empty_status_calls == 1u);
	assert(state.leaf_materializations == 0u);
	assert(state.plan_materializations == 0u);
	assert(state.scatter_calls == 0u);
	state.empty_status_calls = 0u;

	// A representation with zero bytes still owns one source expression. Its
	// scatter CPO must run, while the logically-empty completion CPO must not.
	// The enclosing pack deliberately activates the structural empty-record
	// proof that could otherwise confuse physical size with semantic arity.
	zero_byte_leaf leaf{__builtin_addressof(state)};
	auto leaf_record{::fast_io::mnp::pack(leaf)};
	::fast_io::io::print(sink, leaf_record);
	assert(state.empty_status_calls == 0u);
	assert(state.leaf_materializations == 1u);
	assert(state.scatter_calls == 1u);
	assert(state.bytes == 0u);

	// A bound compiled plan remains one active printable object even when its
	// final descriptor has length zero. Deferred normalization proves that the
	// physical plan ran instead of being collapsed into an empty record.
	constexpr auto plan{
		::fast_io::make_scatter_plan<char>(::fast_io::mnp::scatter_dynamic<0>)};
	zero_plan_source source{__builtin_addressof(state)};
	auto bound{plan(source)};
	auto plan_record{::fast_io::mnp::pack(bound)};
	::fast_io::io::print(sink, plan_record);
	assert(state.empty_status_calls == 0u);
	assert(state.plan_materializations == 1u);
	assert(state.bytes == 0u);
}

} // namespace

int main()
{
	test_observable_two_mutex_empty_record();
	test_silent_two_mutex_empty_record();
	test_zero_byte_active_records();
}
