#include <cassert>
#include <cstddef>
#include <utility>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_SCAN_OBSERVER_TEST_RESTORE_FLOATING_POINT
#endif
#include <fast_io.h>
#if defined(FAST_IO_SCAN_OBSERVER_TEST_RESTORE_FLOATING_POINT)
#undef FAST_IO_SCAN_OBSERVER_TEST_RESTORE_FLOATING_POINT
#undef FAST_IO_DISABLE_FLOATING_POINT
#endif

namespace scan_observer_single_owner
{

struct source_state
{
	char buffer[2]{'A', 'B'};
	char *current{buffer};
	char *end{buffer + 2};
};

struct move_only_input_observer
{
	using input_char_type = char;
	source_state *state{};

	inline explicit constexpr move_only_input_observer(source_state *value) noexcept : state(value) {}
	move_only_input_observer(move_only_input_observer const &) = delete;
	move_only_input_observer &operator=(move_only_input_observer const &) = delete;
	inline constexpr move_only_input_observer(move_only_input_observer &&other) noexcept
		: state(::std::exchange(other.state, nullptr))
	{}
	move_only_input_observer &operator=(move_only_input_observer &&) = delete;
};

struct source
{
	source_state *state{};
};

inline constexpr move_only_input_observer input_stream_ref_define(source &&value) noexcept
{
	return move_only_input_observer{value.state};
}

struct borrowed_source
{
	move_only_input_observer observer;
};

inline constexpr move_only_input_observer &input_stream_ref_define(
	borrowed_source &value) noexcept
{
	return value.observer;
}

inline constexpr char *ibuffer_begin(move_only_input_observer &observer) noexcept
{
	return observer.state->buffer;
}

inline constexpr char *ibuffer_curr(move_only_input_observer &observer) noexcept
{
	return observer.state->current;
}

inline constexpr char *ibuffer_end(move_only_input_observer &observer) noexcept
{
	return observer.state->end;
}

inline constexpr void ibuffer_set_curr(move_only_input_observer &observer, char *current) noexcept
{
	observer.state->current = current;
}

inline constexpr bool ibuffer_underflow(move_only_input_observer &) noexcept
{
	return false;
}

struct target
{
	char value{};
};

struct target_proxy
{
	target *value{};
};

inline constexpr target_proxy scan_alias_define(
	::fast_io::io_alias_t, target &value) noexcept
{
	return {__builtin_addressof(value)};
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, target_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, target_proxy>, char const *first,
	target_proxy &proxy) noexcept
{
	proxy.value->value = *first;
}

static_assert(::fast_io::operations::decay::defines::has_ibuffer_basic_operations<
	move_only_input_observer>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<source>);

} // namespace scan_observer_single_owner

int main()
{
	using namespace ::scan_observer_single_owner;
	source_state state;
	target first;
	target second;
	target_proxy first_proxy{__builtin_addressof(first)};
	target_proxy second_proxy{__builtin_addressof(second)};

	// The stream-ref prvalue creates exactly one move-only owner at the public decay boundary. Both the contiguous
	// controller and its scalar fallback receive that owner by reference; scanner-proxy value policy is independent.
	assert(::fast_io::operations::decay::scan_freestanding_decay(
		::fast_io::operations::input_stream_ref(source{__builtin_addressof(state)}),
		first_proxy, second_proxy));
	assert(first.value == 'A');
	assert(second.value == 'B');
	assert(state.current == state.end);

	// The legacy/public wrapper must preserve a stable lvalue CPO result. Its observer is deliberately noncopyable, so
	// either a raw `decltype(ref)::input_char_type` lookup or a second by-value scan boundary makes this call ill-formed.
	source_state borrowed_state;
	borrowed_source borrowed{move_only_input_observer{__builtin_addressof(borrowed_state)}};
	target borrowed_first;
	target borrowed_second;
	assert(::fast_io::io::scan<true>(
		borrowed, borrowed_first, borrowed_second));
	assert(borrowed_first.value == 'A');
	assert(borrowed_second.value == 'B');
	assert(borrowed_state.current == borrowed_state.end);
}
