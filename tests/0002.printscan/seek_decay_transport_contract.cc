#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

#include <fast_io_core.h>

namespace seek_decay_transport_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct inline_cursor
{
	using input_char_type = char;
	::fast_io::intfpos_t position{};
	::std::size_t flushes{};
};

inline constexpr inline_cursor &input_stream_ref_define(inline_cursor &cursor) noexcept
{
	return cursor;
}

inline constexpr ::fast_io::intfpos_t input_stream_seek_define(
	inline_cursor &cursor, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	if (direction == ::fast_io::seekdir::beg)
	{
		cursor.position = offset;
	}
	else
	{
		cursor.position += offset;
	}
	return cursor.position;
}

inline constexpr ::fast_io::intfpos_t input_stream_seek_bytes_define(
	inline_cursor &cursor, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return input_stream_seek_define(cursor, offset, direction);
}

inline constexpr void input_stream_buffer_flush_define(
	inline_cursor &cursor) noexcept
{
	++cursor.flushes;
}

using seek_value_entry = ::fast_io::intfpos_t (*)(
	inline_cursor, ::fast_io::intfpos_t, ::fast_io::seekdir);
using seek_borrowed_entry = ::fast_io::intfpos_t (*)(
	inline_cursor &, ::fast_io::intfpos_t, ::fast_io::seekdir);
using flush_value_entry = void (*)(inline_cursor);
using flush_borrowed_entry = void (*)(inline_cursor &);

// The function types are the portable ABI contract. The historical spelling
// owns a value, whereas recursive algorithms can name the identity-preserving
// entry explicitly. A trivial register-sized cursor receives no value-dispatch
// privilege without the independent ADL substitution proof.
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::input_stream_seek_decay<
		inline_cursor>),
	seek_value_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::input_stream_seek_decay_borrowed<
		inline_cursor>),
	seek_borrowed_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::input_stream_buffer_flush_decay<
		inline_cursor>),
	flush_value_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::input_stream_buffer_flush_decay_borrowed<
		inline_cursor>),
	flush_borrowed_entry>);
static_assert(::std::is_trivially_copyable_v<inline_cursor>);
static_assert(
	!::fast_io::operations::defines::stream_ref_value_transport_safe<
		inline_cursor>);
static_assert(
	!::fast_io::operations::defines::abi_value_input_stream_ref_result<
		inline_cursor &>);

struct nested_state
{
	unsigned depth{};
	unsigned outer_locks{};
	unsigned inner_locks{};
	unsigned seeks{};
	unsigned flushes{};
};

template <unsigned level>
struct nested_lock
{
	nested_state *state{};

	inline void lock() const noexcept
	{
		require(state->depth == level - 1u);
		state->depth = level;
		if constexpr (level == 1u)
		{
			++state->outer_locks;
		}
		else
		{
			++state->inner_locks;
		}
	}

	inline void unlock() const noexcept
	{
		require(state->depth == level);
		state->depth = level - 1u;
	}
};

struct nested_terminal
{
	using input_char_type = char;
	nested_state *state{};
};

struct nested_inner
{
	using input_char_type = char;
	nested_state *state{};
	nested_terminal *terminal{};

	inline explicit constexpr nested_inner(
		nested_state *state_value, nested_terminal *terminal_value) noexcept
		: state(state_value), terminal(terminal_value)
	{}
	nested_inner(nested_inner const &) = delete;
	nested_inner(nested_inner &&) = delete;
};

struct nested_outer
{
	using input_char_type = char;
	nested_state *state{};
	nested_terminal *terminal{};

	inline explicit constexpr nested_outer(
		nested_state *state_value, nested_terminal *terminal_value) noexcept
		: state(state_value), terminal(terminal_value)
	{}
	nested_outer(nested_outer const &) = delete;
	nested_outer(nested_outer &&) = delete;
};

struct nested_source
{
	nested_state state{};
	nested_terminal terminal{__builtin_addressof(state)};
};

inline constexpr nested_outer input_stream_ref_define(
	nested_source &source) noexcept
{
	return nested_outer{__builtin_addressof(source.state),
						__builtin_addressof(source.terminal)};
}

inline constexpr nested_lock<1u> input_stream_mutex_ref_define(
	nested_outer &outer) noexcept
{
	return {outer.state};
}

inline constexpr nested_inner input_stream_unlocked_ref_define(
	nested_outer &outer) noexcept
{
	return nested_inner{outer.state, outer.terminal};
}

inline constexpr nested_lock<2u> input_stream_mutex_ref_define(
	nested_inner &inner) noexcept
{
	return {inner.state};
}

inline constexpr nested_terminal &input_stream_unlocked_ref_define(
	nested_inner &inner) noexcept
{
	return *inner.terminal;
}

inline ::fast_io::intfpos_t input_stream_seek_define(
	nested_terminal &terminal, ::fast_io::intfpos_t offset,
	::fast_io::seekdir) noexcept
{
	require(terminal.state->depth == 2u);
	++terminal.state->seeks;
	return offset;
}

inline void input_stream_buffer_flush_define(
	nested_terminal &terminal) noexcept
{
	require(terminal.state->depth == 2u);
	++terminal.state->flushes;
}

static_assert(
	::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
		nested_outer>);
static_assert(
	::fast_io::operations::decay::defines::input_stream_seek_dispatchable<
		nested_outer>);
static_assert(
	!::fast_io::operations::defines::abi_value_input_stream_ref_result<
		nested_outer &>);
static_assert(requires(nested_outer &outer) {
	::fast_io::operations::decay::input_stream_seek_decay_dispatch(
		outer, 0, ::fast_io::seekdir::beg);
});

inline void test_inline_cursor_transport() noexcept
{
	inline_cursor cursor{};
	require(::fast_io::operations::input_stream_seek(
			cursor, 11, ::fast_io::seekdir::beg) == 11);
	require(cursor.position == 11 && cursor.flushes == 1u);
	require(::fast_io::operations::input_stream_seek_bytes(
			cursor, 7, ::fast_io::seekdir::cur) == 18);
	require(cursor.position == 18 && cursor.flushes == 2u);

	// Direct use of the historical owner intentionally mutates its parameter
	// copy; named observers use `_dispatch` and therefore retain cursor identity.
	require(::fast_io::operations::decay::input_stream_seek_decay(
			cursor, 3, ::fast_io::seekdir::beg) == 3);
	require(cursor.position == 18 && cursor.flushes == 2u);
	require(::fast_io::operations::decay::input_stream_seek_decay_dispatch(
			cursor, 5, ::fast_io::seekdir::beg) == 5);
	require(cursor.position == 5 && cursor.flushes == 3u);
}

inline void test_recursive_mutex_borrow() noexcept
{
	nested_source source{};
	require(::fast_io::operations::input_stream_seek(
			source, 29, ::fast_io::seekdir::beg) == 29);
	require(source.state.depth == 0u);
	require(source.state.outer_locks == 1u &&
			source.state.inner_locks == 1u);
	require(source.state.seeks == 1u && source.state.flushes == 1u);

	::fast_io::operations::input_stream_buffer_flush(source);
	require(source.state.depth == 0u);
	require(source.state.outer_locks == 2u &&
			source.state.inner_locks == 2u);
	require(source.state.flushes == 2u);
}

} // namespace seek_decay_transport_contract

int main()
{
	::seek_decay_transport_contract::test_inline_cursor_transport();
	::seek_decay_transport_contract::test_recursive_mutex_borrow();
}
