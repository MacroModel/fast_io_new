#include <cstddef>
#include <cstdlib>

#include <fast_io_core.h>

namespace seek_transmit_observer_borrow
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct lock_state
{
	bool locked{};
	::std::size_t locks{};
	::std::size_t unlocks{};

	inline void lock() noexcept
	{
		require(!locked);
		locked = true;
		++locks;
	}

	inline void unlock() noexcept
	{
		require(locked);
		locked = false;
		++unlocks;
	}
};

struct mutex_proxy
{
	lock_state *state{};

	inline void lock() const noexcept
	{
		state->lock();
	}

	inline void unlock() const noexcept
	{
		state->unlock();
	}
};

struct borrowed_output_source;

struct borrowed_locked_output
{
	using output_char_type = char;
	borrowed_output_source *source{};
	unsigned char abi_large[64]{};

	borrowed_locked_output() = default;
	inline explicit borrowed_locked_output(borrowed_output_source *value) noexcept : source(value)
	{}
	borrowed_locked_output(borrowed_locked_output const &other) noexcept;
};

struct borrowed_unlocked_output
{
	using output_char_type = char;
	borrowed_output_source *source{};
	unsigned char abi_large[64]{};

	borrowed_unlocked_output() = default;
	inline explicit borrowed_unlocked_output(borrowed_output_source *value) noexcept : source(value)
	{}
	borrowed_unlocked_output(borrowed_unlocked_output const &other) noexcept;
};

struct borrowed_output_source
{
	lock_state lock{};
	::std::size_t observer_copies{};
	::std::size_t seeks{};
	::std::size_t flushes{};
	borrowed_locked_output locked{this};
	borrowed_unlocked_output unlocked{this};
};

inline borrowed_locked_output::borrowed_locked_output(borrowed_locked_output const &other) noexcept
	: source(other.source)
{
	++source->observer_copies;
}

inline borrowed_unlocked_output::borrowed_unlocked_output(borrowed_unlocked_output const &other) noexcept
	: source(other.source)
{
	++source->observer_copies;
}

inline constexpr borrowed_locked_output &output_stream_ref_define(borrowed_output_source &source) noexcept
{
	return source.locked;
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(borrowed_locked_output &observer) noexcept
{
	return {__builtin_addressof(observer.source->lock)};
}

inline constexpr borrowed_unlocked_output &output_stream_unlocked_ref_define(borrowed_locked_output &observer) noexcept
{
	return observer.source->unlocked;
}

inline ::fast_io::intfpos_t output_stream_seek_define(borrowed_unlocked_output &observer,
													  ::fast_io::intfpos_t offset, ::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(borrowed_unlocked_output &observer,
															::fast_io::intfpos_t offset,
															::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline void output_stream_buffer_flush_define(borrowed_unlocked_output &observer) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->flushes;
}

inline void write_all_overflow_define(borrowed_unlocked_output &observer, char const *, char const *) noexcept
{
	require(observer.source->lock.locked);
}

inline void write_all_bytes_overflow_define(borrowed_unlocked_output &observer, ::std::byte const *,
											::std::byte const *) noexcept
{
	require(observer.source->lock.locked);
}

struct borrowed_input_source;

struct borrowed_locked_input
{
	using input_char_type = char;
	borrowed_input_source *source{};
	unsigned char abi_large[64]{};

	borrowed_locked_input() = default;
	inline explicit borrowed_locked_input(borrowed_input_source *value) noexcept : source(value)
	{}
	borrowed_locked_input(borrowed_locked_input const &other) noexcept;
};

struct borrowed_unlocked_input
{
	using input_char_type = char;
	borrowed_input_source *source{};
	unsigned char abi_large[64]{};

	borrowed_unlocked_input() = default;
	inline explicit borrowed_unlocked_input(borrowed_input_source *value) noexcept : source(value)
	{}
	borrowed_unlocked_input(borrowed_unlocked_input const &other) noexcept;
};

struct borrowed_input_source
{
	lock_state lock{};
	::std::size_t observer_copies{};
	::std::size_t seeks{};
	::std::size_t flushes{};
	borrowed_locked_input locked{this};
	borrowed_unlocked_input unlocked{this};
};

inline borrowed_locked_input::borrowed_locked_input(borrowed_locked_input const &other) noexcept
	: source(other.source)
{
	++source->observer_copies;
}

inline borrowed_unlocked_input::borrowed_unlocked_input(borrowed_unlocked_input const &other) noexcept
	: source(other.source)
{
	++source->observer_copies;
}

inline constexpr borrowed_locked_input &input_stream_ref_define(borrowed_input_source &source) noexcept
{
	return source.locked;
}

inline constexpr mutex_proxy input_stream_mutex_ref_define(borrowed_locked_input &observer) noexcept
{
	return {__builtin_addressof(observer.source->lock)};
}

inline constexpr borrowed_unlocked_input &input_stream_unlocked_ref_define(borrowed_locked_input &observer) noexcept
{
	return observer.source->unlocked;
}

inline ::fast_io::intfpos_t input_stream_seek_define(borrowed_unlocked_input &observer,
													 ::fast_io::intfpos_t offset, ::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(borrowed_unlocked_input &observer,
														   ::fast_io::intfpos_t offset,
														   ::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline void input_stream_buffer_flush_define(borrowed_unlocked_input &observer) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->flushes;
}

inline char *read_some_underflow_define(borrowed_unlocked_input &observer, char *first, char *) noexcept
{
	require(observer.source->lock.locked);
	return first;
}

inline void read_all_underflow_define(borrowed_unlocked_input &observer, char *, char *) noexcept
{
	require(observer.source->lock.locked);
}

inline ::std::byte *read_some_bytes_underflow_define(borrowed_unlocked_input &observer, ::std::byte *first,
													 ::std::byte *) noexcept
{
	require(observer.source->lock.locked);
	return first;
}

inline void read_all_bytes_underflow_define(borrowed_unlocked_input &observer, ::std::byte *, ::std::byte *) noexcept
{
	require(observer.source->lock.locked);
}

struct move_only_input_source
{
	lock_state lock{};
	::std::size_t seeks{};
};

struct move_only_locked_input
{
	using input_char_type = char;
	move_only_input_source *source{};

	move_only_locked_input() = default;
	inline explicit constexpr move_only_locked_input(move_only_input_source *value) noexcept : source(value)
	{}
	move_only_locked_input(move_only_locked_input const &) = delete;
	move_only_locked_input &operator=(move_only_locked_input const &) = delete;
	move_only_locked_input(move_only_locked_input &&) = default;
};

struct move_only_unlocked_input
{
	using input_char_type = char;
	move_only_input_source *source{};

	move_only_unlocked_input() = default;
	inline explicit constexpr move_only_unlocked_input(move_only_input_source *value) noexcept : source(value)
	{}
	move_only_unlocked_input(move_only_unlocked_input const &) = delete;
	move_only_unlocked_input &operator=(move_only_unlocked_input const &) = delete;
	move_only_unlocked_input(move_only_unlocked_input &&) = default;
};

inline constexpr move_only_locked_input input_stream_ref_define(move_only_input_source &source) noexcept
{
	return move_only_locked_input{__builtin_addressof(source)};
}

inline constexpr mutex_proxy input_stream_mutex_ref_define(move_only_locked_input &observer) noexcept
{
	return {__builtin_addressof(observer.source->lock)};
}

inline constexpr move_only_unlocked_input input_stream_unlocked_ref_define(move_only_locked_input &observer) noexcept
{
	return move_only_unlocked_input{observer.source};
}

inline ::fast_io::intfpos_t input_stream_seek_define(move_only_unlocked_input &observer,
													 ::fast_io::intfpos_t offset, ::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

struct borrowed_io_source;

struct borrowed_locked_io
{
	using input_char_type = char;
	using output_char_type = char;
	borrowed_io_source *source{};
	unsigned char abi_large[64]{};

	borrowed_locked_io() = default;
	inline explicit borrowed_locked_io(borrowed_io_source *value) noexcept : source(value)
	{}
	borrowed_locked_io(borrowed_locked_io const &other) noexcept;
};

struct borrowed_unlocked_io
{
	using input_char_type = char;
	using output_char_type = char;
	borrowed_io_source *source{};
	unsigned char abi_large[64]{};

	borrowed_unlocked_io() = default;
	inline explicit borrowed_unlocked_io(borrowed_io_source *value) noexcept : source(value)
	{}
	borrowed_unlocked_io(borrowed_unlocked_io const &other) noexcept;
};

struct borrowed_io_source
{
	lock_state lock{};
	::std::size_t observer_copies{};
	::std::size_t seeks{};
	::std::size_t flushes{};
	borrowed_locked_io locked{this};
	borrowed_unlocked_io unlocked{this};
};

inline borrowed_locked_io::borrowed_locked_io(borrowed_locked_io const &other) noexcept : source(other.source)
{
	++source->observer_copies;
}

inline borrowed_unlocked_io::borrowed_unlocked_io(borrowed_unlocked_io const &other) noexcept : source(other.source)
{
	++source->observer_copies;
}

inline constexpr borrowed_locked_io &io_stream_ref_define(borrowed_io_source &source) noexcept
{
	return source.locked;
}

inline constexpr mutex_proxy io_stream_mutex_ref_define(borrowed_locked_io &observer) noexcept
{
	return {__builtin_addressof(observer.source->lock)};
}

inline constexpr borrowed_unlocked_io &io_stream_unlocked_ref_define(borrowed_locked_io &observer) noexcept
{
	return observer.source->unlocked;
}

inline ::fast_io::intfpos_t io_stream_seek_define(borrowed_unlocked_io &observer, ::fast_io::intfpos_t offset,
												  ::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(borrowed_unlocked_io &observer,
														::fast_io::intfpos_t offset,
														::fast_io::seekdir) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->seeks;
	return offset;
}

inline void io_stream_buffer_flush_define(borrowed_unlocked_io &observer) noexcept
{
	require(observer.source->lock.locked);
	++observer.source->flushes;
}

struct capabilityless_locked_io
{
	using input_char_type = char;
	using output_char_type = char;
};

struct capabilityless_unlocked_io
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(capabilityless_locked_io &) noexcept
{
	return {};
}

inline constexpr capabilityless_unlocked_io io_stream_unlocked_ref_define(capabilityless_locked_io &) noexcept
{
	return {};
}

struct directional_terminal_inside_io
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(directional_terminal_inside_io &) noexcept
{
	return {};
}

inline constexpr capabilityless_unlocked_io
input_stream_unlocked_ref_define(directional_terminal_inside_io &) noexcept
{
	return {};
}

inline constexpr ::fast_io::intfpos_t io_stream_seek_define(directional_terminal_inside_io &,
															::fast_io::intfpos_t offset, ::fast_io::seekdir) noexcept
{
	return offset;
}

struct outer_io_over_directional_terminal
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(outer_io_over_directional_terminal &) noexcept
{
	return {};
}

inline constexpr directional_terminal_inside_io
io_stream_unlocked_ref_define(outer_io_over_directional_terminal &) noexcept
{
	return {};
}

static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
			  borrowed_locked_output>);
static_assert(::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
			  borrowed_locked_input>);
static_assert(::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
			  move_only_locked_input>);
static_assert(::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<borrowed_locked_io>);
static_assert(::fast_io::operations::decay::defines::input_stream_seek_dispatchable<move_only_locked_input>);
static_assert(!::fast_io::operations::decay::defines::input_stream_seek_bytes_dispatchable<move_only_locked_input>);
static_assert(!::fast_io::operations::decay::defines::input_stream_buffer_flush_dispatchable<move_only_locked_input>);

// Mutex completeness and operation support are independent proofs. The wrapper below has a finite, character-
// preserving io lock chain, yet its terminal has no seek/flush CPO. Every operation-specific concept must stay false.
static_assert(::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::input_stream_seek_dispatchable<capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::output_stream_seek_dispatchable<capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::io_stream_seek_dispatchable<capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::input_stream_buffer_flush_dispatchable<
			  capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<
			  capabilityless_locked_io>);
static_assert(!::fast_io::operations::decay::defines::io_stream_buffer_flush_dispatchable<
			  capabilityless_locked_io>);
static_assert(::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol<
			  outer_io_over_directional_terminal>);
static_assert(!::fast_io::operations::decay::defines::io_stream_seek_dispatchable<
			  outer_io_over_directional_terminal>);

inline void test_public_seek_owns_or_borrows_once() noexcept
{
	borrowed_output_source output{};
	require(::fast_io::operations::output_stream_seek(output, 17, ::fast_io::seekdir::beg) == 17);
	require(::fast_io::operations::output_stream_seek_bytes(output, 19, ::fast_io::seekdir::beg) == 19);
	::fast_io::operations::output_stream_buffer_flush(output);
	require(output.observer_copies == 0);
	require(output.seeks == 2 && output.flushes == 3);
	require(output.lock.locks == 3 && output.lock.unlocks == 3 && !output.lock.locked);

	borrowed_input_source input{};
	require(::fast_io::operations::input_stream_seek(input, 23, ::fast_io::seekdir::beg) == 23);
	require(::fast_io::operations::input_stream_seek_bytes(input, 29, ::fast_io::seekdir::beg) == 29);
	::fast_io::operations::input_stream_buffer_flush(input);
	require(input.observer_copies == 0);
	require(input.seeks == 2 && input.flushes == 3);
	require(input.lock.locks == 3 && input.lock.unlocks == 3 && !input.lock.locked);

	move_only_input_source move_only{};
	require(::fast_io::operations::input_stream_seek(move_only, 31, ::fast_io::seekdir::beg) == 31);
	require(move_only.seeks == 1);
	require(move_only.lock.locks == 1 && move_only.lock.unlocks == 1 && !move_only.lock.locked);

	borrowed_io_source io{};
	require(::fast_io::operations::io_stream_seek(io, 37, ::fast_io::seekdir::beg) == 37);
	require(::fast_io::operations::io_stream_seek_bytes(io, 41, ::fast_io::seekdir::beg) == 41);
	::fast_io::operations::io_stream_buffer_flush(io);
	require(io.observer_copies == 0);
	require(io.seeks == 2 && io.flushes == 3);
	require(io.lock.locks == 3 && io.lock.unlocks == 3 && !io.lock.locked);
}

inline void test_public_transmit_borrows_normalized_observers() noexcept
{
	borrowed_output_source output{};
	borrowed_input_source input{};
	::fast_io::operations::transmit_all(output, input, 0);
	require(::fast_io::operations::transmit_some(output, input, 0) == 0);
	::fast_io::operations::transmit_bytes_all(output, input, 0);
	require(::fast_io::operations::transmit_bytes_some(output, input, 0) == 0);

	// Four public calls normalize each observer once. Since both CPOs return large non-trivial lvalues, ABI-aware
	// normalization preserves those references; transmit recursion and terminal selection must not copy them later.
	require(output.observer_copies == 0 && input.observer_copies == 0);
	require(output.lock.locks == 4 && output.lock.unlocks == 4 && !output.lock.locked);
	require(input.lock.locks == 4 && input.lock.unlocks == 4 && !input.lock.locked);
}

} // namespace seek_transmit_observer_borrow

int main()
{
	::seek_transmit_observer_borrow::test_public_seek_owns_or_borrows_once();
	::seek_transmit_observer_borrow::test_public_transmit_borrows_normalized_observers();
}
