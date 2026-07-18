#include <cstddef>
#include <cstdlib>

#include <fast_io_core.h>

namespace seek_transmit_mutex_protocol_test
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
};

struct mutex_proxy
{
	lock_state *state{};

	inline void lock() const noexcept
	{
		require(state != nullptr && !state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		require(state != nullptr && state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct input_state
{
	lock_state lock{};
	::std::size_t flushes{};
	::std::size_t character_seeks{};
	::std::size_t byte_seeks{};
	::std::size_t reads{};
};

struct unlocked_input_ref
{
	using input_char_type = char;
	input_state *state{};
};

struct locked_input_ref
{
	using input_char_type = char;
	input_state *state{};
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(locked_input_ref stream) noexcept
{
	return {__builtin_addressof(stream.state->lock)};
}

inline constexpr unlocked_input_ref input_stream_unlocked_ref_define(locked_input_ref stream) noexcept
{
	return {stream.state};
}

inline void input_stream_buffer_flush_define(unlocked_input_ref stream) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->flushes;
}

inline ::fast_io::intfpos_t input_stream_seek_define(unlocked_input_ref stream, ::fast_io::intfpos_t offset,
													  ::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->character_seeks;
	return offset + 1;
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(unlocked_input_ref stream, ::fast_io::intfpos_t offset,
														::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->byte_seeks;
	return offset + 2;
}

// A locked observer may expose forwarding CPOs for unrelated callers. Seek/flush dispatch must nevertheless unwrap
// it first; reaching one of these poison definitions proves that a direct capability bypassed synchronization.
inline void input_stream_buffer_flush_define(locked_input_ref) noexcept
{
	require(false);
}

inline ::fast_io::intfpos_t input_stream_seek_define(locked_input_ref, ::fast_io::intfpos_t,
													  ::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(locked_input_ref, ::fast_io::intfpos_t,
														::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

inline char *read_some_underflow_define(unlocked_input_ref stream, char *first, char *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->reads;
	return first;
}

inline void read_all_underflow_define(unlocked_input_ref stream, char *, char *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->reads;
}

inline ::std::byte *read_some_bytes_underflow_define(unlocked_input_ref stream, ::std::byte *first,
														  ::std::byte *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->reads;
	return first;
}

inline void read_all_bytes_underflow_define(unlocked_input_ref stream, ::std::byte *, ::std::byte *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->reads;
}

struct output_state
{
	lock_state lock{};
	::std::size_t flushes{};
	::std::size_t character_seeks{};
	::std::size_t byte_seeks{};
	::std::size_t writes{};
};

struct unlocked_output_ref
{
	using output_char_type = char;
	output_state *state{};
};

struct locked_output_ref
{
	using output_char_type = char;
	output_state *state{};
};

inline constexpr mutex_proxy output_stream_mutex_ref_define(locked_output_ref stream) noexcept
{
	return {__builtin_addressof(stream.state->lock)};
}

inline constexpr unlocked_output_ref output_stream_unlocked_ref_define(locked_output_ref stream) noexcept
{
	return {stream.state};
}

inline void output_stream_buffer_flush_define(unlocked_output_ref stream) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->flushes;
}

inline ::fast_io::intfpos_t output_stream_seek_define(unlocked_output_ref stream, ::fast_io::intfpos_t offset,
													   ::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->character_seeks;
	return offset + 3;
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(unlocked_output_ref stream, ::fast_io::intfpos_t offset,
														 ::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->byte_seeks;
	return offset + 4;
}

inline void output_stream_buffer_flush_define(locked_output_ref) noexcept
{
	require(false);
}

inline ::fast_io::intfpos_t output_stream_seek_define(locked_output_ref, ::fast_io::intfpos_t,
													   ::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(locked_output_ref, ::fast_io::intfpos_t,
														 ::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

inline void write_all_overflow_define(unlocked_output_ref stream, char const *, char const *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->writes;
}

inline void write_all_bytes_overflow_define(unlocked_output_ref stream, ::std::byte const *,
														::std::byte const *) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->writes;
}

struct io_state
{
	lock_state lock{};
	::std::size_t flushes{};
	::std::size_t character_seeks{};
	::std::size_t byte_seeks{};
};

struct unlocked_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	io_state *state{};
};

struct locked_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	io_state *state{};
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(locked_io_ref stream) noexcept
{
	return {__builtin_addressof(stream.state->lock)};
}

inline constexpr unlocked_io_ref io_stream_unlocked_ref_define(locked_io_ref stream) noexcept
{
	return {stream.state};
}

inline void io_stream_buffer_flush_define(unlocked_io_ref stream) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->flushes;
}

inline ::fast_io::intfpos_t io_stream_seek_define(unlocked_io_ref stream, ::fast_io::intfpos_t offset,
												  ::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->character_seeks;
	return offset + 5;
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(unlocked_io_ref stream, ::fast_io::intfpos_t offset,
													::fast_io::seekdir) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->byte_seeks;
	return offset + 6;
}

inline void io_stream_buffer_flush_define(locked_io_ref) noexcept
{
	require(false);
}

inline ::fast_io::intfpos_t io_stream_seek_define(locked_io_ref, ::fast_io::intfpos_t,
												  ::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(locked_io_ref, ::fast_io::intfpos_t,
													::fast_io::seekdir) noexcept
{
	require(false);
	return {};
}

struct wrong_flush_result_ref
{
	using input_char_type = char;
};

inline int input_stream_buffer_flush_define(wrong_flush_result_ref) noexcept
{
	return 0;
}

struct wrong_output_flush_result_ref
{
	using output_char_type = char;
};

inline bool output_stream_buffer_flush_define(wrong_output_flush_result_ref) noexcept
{
	return false;
}

struct wrong_io_flush_result_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline ::std::size_t io_stream_buffer_flush_define(wrong_io_flush_result_ref) noexcept
{
	return 0;
}

struct partial_input_ref
{
	using input_char_type = char;
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(partial_input_ref) noexcept
{
	return {};
}

inline void input_stream_buffer_flush_define(partial_input_ref) noexcept
{}

inline ::fast_io::intfpos_t input_stream_seek_define(partial_input_ref, ::fast_io::intfpos_t offset,
													  ::fast_io::seekdir) noexcept
{
	return offset;
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(partial_input_ref, ::fast_io::intfpos_t offset,
														::fast_io::seekdir) noexcept
{
	return offset;
}

struct partial_output_ref
{
	using output_char_type = char;
};

inline constexpr mutex_proxy output_stream_mutex_ref_define(partial_output_ref) noexcept
{
	return {};
}

inline void output_stream_buffer_flush_define(partial_output_ref) noexcept
{}

inline ::fast_io::intfpos_t output_stream_seek_define(partial_output_ref, ::fast_io::intfpos_t offset,
													   ::fast_io::seekdir) noexcept
{
	return offset;
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(partial_output_ref, ::fast_io::intfpos_t offset,
														 ::fast_io::seekdir) noexcept
{
	return offset;
}

struct partial_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(partial_io_ref) noexcept
{
	return {};
}

inline void io_stream_buffer_flush_define(partial_io_ref) noexcept
{}

inline ::fast_io::intfpos_t io_stream_seek_define(partial_io_ref, ::fast_io::intfpos_t offset,
												  ::fast_io::seekdir) noexcept
{
	return offset;
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(partial_io_ref, ::fast_io::intfpos_t offset,
													::fast_io::seekdir) noexcept
{
	return offset;
}

struct split_direction_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(split_direction_io_ref) noexcept
{
	return {};
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(split_direction_io_ref) noexcept
{
	return {};
}

inline void io_stream_buffer_flush_define(split_direction_io_ref) noexcept
{}

inline ::fast_io::intfpos_t io_stream_seek_define(split_direction_io_ref, ::fast_io::intfpos_t offset,
													  ::fast_io::seekdir) noexcept
{
	return offset;
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(split_direction_io_ref, ::fast_io::intfpos_t offset,
														::fast_io::seekdir) noexcept
{
	return offset;
}

struct self_unwrapping_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(self_unwrapping_io_ref) noexcept
{
	return {};
}

inline constexpr self_unwrapping_io_ref io_stream_unlocked_ref_define(self_unwrapping_io_ref stream) noexcept
{
	return stream;
}

struct malformed_io_mutex_proxy
{
	inline int lock() const noexcept
	{
		return 0;
	}

	inline void unlock() const noexcept
	{}
};

struct malformed_io_mutex_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr malformed_io_mutex_proxy io_stream_mutex_ref_define(malformed_io_mutex_ref) noexcept
{
	return {};
}

inline constexpr unlocked_io_ref io_stream_unlocked_ref_define(malformed_io_mutex_ref) noexcept
{
	return {};
}

struct bare_input_ref
{
	using input_char_type = char;
	lock_state *expected_lock{};
	::std::size_t *reads{};
};

inline void observe_bare_input(bare_input_ref stream) noexcept
{
	if (stream.expected_lock != nullptr)
	{
		require(stream.expected_lock->locked);
	}
	++*stream.reads;
}

inline char *read_some_underflow_define(bare_input_ref stream, char *first, char *) noexcept
{
	observe_bare_input(stream);
	return first;
}

inline void read_all_underflow_define(bare_input_ref stream, char *, char *) noexcept
{
	observe_bare_input(stream);
}

inline ::std::byte *read_some_bytes_underflow_define(bare_input_ref stream, ::std::byte *first,
														  ::std::byte *) noexcept
{
	observe_bare_input(stream);
	return first;
}

inline void read_all_bytes_underflow_define(bare_input_ref stream, ::std::byte *, ::std::byte *) noexcept
{
	observe_bare_input(stream);
}

struct bare_output_ref
{
	using output_char_type = char;
};

inline void write_all_overflow_define(bare_output_ref, char const *, char const *) noexcept
{}

inline void write_all_bytes_overflow_define(bare_output_ref, ::std::byte const *, ::std::byte const *) noexcept
{}

template <typename T>
concept input_seek_admitted = requires(T stream) {
	::fast_io::operations::decay::input_stream_seek_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::input_stream_seek_bytes_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::input_stream_buffer_flush_decay(stream);
};

template <typename T>
concept output_seek_admitted = requires(T stream) {
	::fast_io::operations::decay::output_stream_seek_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::output_stream_seek_bytes_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::output_stream_buffer_flush_decay(stream);
};

template <typename T>
concept io_seek_admitted = requires(T stream) {
	::fast_io::operations::decay::io_stream_seek_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::io_stream_seek_bytes_decay(stream, 0, ::fast_io::seekdir::beg);
	::fast_io::operations::decay::io_stream_buffer_flush_decay(stream);
};

template <typename output, typename input>
concept all_transmit_overloads_admitted = requires(output out, input in,
														::fast_io::uintfpos_transmit_reference_wrapper result) {
	::fast_io::operations::decay::transmit_all_decay(out, in, 0);
	::fast_io::operations::decay::transmit_some_decay(out, in, 0);
	::fast_io::operations::decay::transmit_bytes_all_decay(out, in, 0);
	::fast_io::operations::decay::transmit_bytes_some_decay(out, in, 0);
	::fast_io::operations::decay::transmit_until_eof_generic_decay(out, in, result);
	::fast_io::operations::decay::transmit_until_eof_decay(out, in);
	::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay(out, in, result);
	::fast_io::operations::decay::transmit_bytes_until_eof_decay(out, in);
};

using namespace ::fast_io::operations::decay::defines;

static_assert(has_input_stream_buffer_flush_define<unlocked_input_ref>);
static_assert(!has_input_stream_buffer_flush_define<wrong_flush_result_ref>);
static_assert(!has_output_stream_buffer_flush_define<wrong_output_flush_result_ref>);
static_assert(!has_io_stream_buffer_flush_define<wrong_io_flush_result_ref>);
static_assert(has_complete_input_stream_mutex_protocol<locked_input_ref>);
static_assert(has_complete_output_stream_mutex_protocol<locked_output_ref>);
static_assert(has_complete_io_stream_mutex_protocol<locked_io_ref>);
static_assert(has_complete_input_stream_mutex_protocol<locked_io_ref>);
static_assert(has_complete_output_stream_mutex_protocol<locked_io_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<partial_input_ref>);
static_assert(!has_complete_output_stream_mutex_protocol<partial_output_ref>);
static_assert(!has_complete_io_stream_mutex_protocol<partial_io_ref>);
static_assert(!has_complete_io_stream_mutex_protocol<self_unwrapping_io_ref>);
static_assert(!has_complete_io_stream_mutex_protocol<malformed_io_mutex_ref>);

static_assert(input_seek_admitted<locked_input_ref>);
static_assert(output_seek_admitted<locked_output_ref>);
static_assert(io_seek_admitted<locked_io_ref>);
static_assert(!input_seek_admitted<partial_input_ref>);
static_assert(!output_seek_admitted<partial_output_ref>);
static_assert(!io_seek_admitted<partial_io_ref>);
static_assert(!io_seek_admitted<split_direction_io_ref>);
static_assert(!input_stream_buffer_flush_dispatchable<wrong_flush_result_ref>);
static_assert(!output_stream_buffer_flush_dispatchable<wrong_output_flush_result_ref>);
static_assert(!io_stream_buffer_flush_dispatchable<wrong_io_flush_result_ref>);

static_assert(has_complete_transmit_mutex_protocols<locked_output_ref, unlocked_input_ref>);
static_assert(has_complete_transmit_mutex_protocols<unlocked_output_ref, locked_input_ref>);
static_assert(!has_complete_transmit_mutex_protocols<partial_output_ref, unlocked_input_ref>);
static_assert(!has_complete_transmit_mutex_protocols<unlocked_output_ref, partial_input_ref>);
static_assert(all_transmit_overloads_admitted<locked_output_ref, bare_input_ref>);
static_assert(all_transmit_overloads_admitted<bare_output_ref, locked_input_ref>);
static_assert(!all_transmit_overloads_admitted<partial_output_ref, bare_input_ref>);
static_assert(!all_transmit_overloads_admitted<bare_output_ref, partial_input_ref>);

inline void verify_lock_count(lock_state const &state, ::std::size_t expected) noexcept
{
	require(!state.locked);
	require(state.locks == expected);
	require(state.unlocks == expected);
}

inline void test_directional_seek_and_flush() noexcept
{
	input_state input{};
	locked_input_ref locked_input{__builtin_addressof(input)};
	require(::fast_io::operations::decay::input_stream_seek_decay(locked_input, 7, ::fast_io::seekdir::beg) == 8);
	require(::fast_io::operations::decay::input_stream_seek_bytes_decay(locked_input, 7,
																		  ::fast_io::seekdir::beg) == 9);
	::fast_io::operations::decay::input_stream_buffer_flush_decay(locked_input);
	verify_lock_count(input.lock, 3u);
	require(input.flushes == 3u && input.character_seeks == 1u && input.byte_seeks == 1u);

	output_state output{};
	locked_output_ref locked_output{__builtin_addressof(output)};
	require(::fast_io::operations::decay::output_stream_seek_decay(locked_output, 7,
																		 ::fast_io::seekdir::beg) == 10);
	require(::fast_io::operations::decay::output_stream_seek_bytes_decay(locked_output, 7,
																		   ::fast_io::seekdir::beg) == 11);
	::fast_io::operations::decay::output_stream_buffer_flush_decay(locked_output);
	verify_lock_count(output.lock, 3u);
	require(output.flushes == 3u && output.character_seeks == 1u && output.byte_seeks == 1u);

	io_state io{};
	locked_io_ref locked_io{__builtin_addressof(io)};
	require(::fast_io::operations::decay::io_stream_seek_decay(locked_io, 7, ::fast_io::seekdir::beg) == 12);
	require(::fast_io::operations::decay::io_stream_seek_bytes_decay(locked_io, 7,
																	   ::fast_io::seekdir::beg) == 13);
	::fast_io::operations::decay::io_stream_buffer_flush_decay(locked_io);
	verify_lock_count(io.lock, 3u);
	require(io.flushes == 3u && io.character_seeks == 1u && io.byte_seeks == 1u);
}

template <typename output, typename input>
inline void instantiate_every_transmit(output out, input in) noexcept
{
	::fast_io::uintfpos_t transmitted{};
	::fast_io::uintfpos_transmit_reference_wrapper result{__builtin_addressof(transmitted)};
	::fast_io::operations::decay::transmit_all_decay(out, in, 0);
	require(::fast_io::operations::decay::transmit_some_decay(out, in, 0) == 0);
	::fast_io::operations::decay::transmit_bytes_all_decay(out, in, 0);
	require(::fast_io::operations::decay::transmit_bytes_some_decay(out, in, 0) == 0);
	::fast_io::operations::decay::transmit_until_eof_generic_decay(out, in, result);
	require(::fast_io::operations::decay::transmit_until_eof_decay(out, in).transmitted == 0);
	::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay(out, in, result);
	require(::fast_io::operations::decay::transmit_bytes_until_eof_decay(out, in).transmitted == 0);
}

inline void test_transmit_directional_protocols() noexcept
{
	input_state input{};
	instantiate_every_transmit(bare_output_ref{}, locked_input_ref{__builtin_addressof(input)});
	verify_lock_count(input.lock, 8u);
	// Only the four until-eof variants perform a zero-length EOF probe; fixed-count zero transfers perform no read.
	require(input.reads == 4u);

	output_state output{};
	::std::size_t bare_reads{};
	bare_input_ref input_while_output_locked{__builtin_addressof(output.lock), __builtin_addressof(bare_reads)};
	instantiate_every_transmit(locked_output_ref{__builtin_addressof(output)}, input_while_output_locked);
	verify_lock_count(output.lock, 8u);
	require(bare_reads == 4u);
}

} // namespace seek_transmit_mutex_protocol_test

int main()
{
	::seek_transmit_mutex_protocol_test::test_directional_seek_and_flush();
	::seek_transmit_mutex_protocol_test::test_transmit_directional_protocols();
}
