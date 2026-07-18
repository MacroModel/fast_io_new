#include <array>
#include <cstddef>
#include <cstdlib>
#include <list>

#include <fast_io_core.h>

namespace primitive_mutex_protocol_test
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

enum class primitive_kind : ::std::size_t
{
	read_some,
	read_all,
	read_some_bytes,
	read_all_bytes,
	scatter_read_some,
	scatter_read_all,
	scatter_read_some_bytes,
	scatter_read_all_bytes,
	pread_some,
	pread_all,
	pread_some_bytes,
	pread_all_bytes,
	scatter_pread_some,
	scatter_pread_all,
	scatter_pread_some_bytes,
	scatter_pread_all_bytes,
	write_some,
	write_all,
	char_put,
	write_some_bytes,
	write_all_bytes,
	scatter_write_some,
	scatter_write_all,
	scatter_write_some_bytes,
	scatter_write_all_bytes,
	pwrite_some,
	pwrite_all,
	pwrite_some_bytes,
	pwrite_all_bytes,
	scatter_pwrite_some,
	scatter_pwrite_all,
	scatter_pwrite_some_bytes,
	scatter_pwrite_all_bytes,
	count
};

inline constexpr ::std::size_t primitive_count{static_cast<::std::size_t>(primitive_kind::count)};

struct lock_state
{
	bool locked{};
	::std::size_t locks{};
	::std::size_t unlocks{};
};

struct operation_state
{
	lock_state lock{};
	::std::array<::std::size_t, primitive_count> calls{};
};

struct mutex_proxy
{
	lock_state *state{};

	inline void lock() const noexcept
	{
		require(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		require(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct unlocked_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	operation_state *state{};
};

struct locked_io_ref
{
	using input_char_type = char;
	using output_char_type = char;
	operation_state *state{};
};

inline constexpr locked_io_ref input_stream_ref_define(locked_io_ref stream) noexcept
{
	return stream;
}

inline constexpr locked_io_ref output_stream_ref_define(locked_io_ref stream) noexcept
{
	return stream;
}

inline constexpr mutex_proxy io_stream_mutex_ref_define(locked_io_ref stream) noexcept
{
	return {__builtin_addressof(stream.state->lock)};
}

inline constexpr unlocked_io_ref io_stream_unlocked_ref_define(locked_io_ref stream) noexcept
{
	return {stream.state};
}

// Range admission is intentionally evaluated on the locked observer before the iterator dispatcher can unwrap it.
// This declaration supplies that type-level capability; execution must still take the mutex branch, so reaching this
// body would prove that range dispatch bypassed synchronization.
inline void write_all_overflow_define(locked_io_ref, char const *, char const *) noexcept
{
	require(false);
}

inline void observe(unlocked_io_ref stream, primitive_kind kind) noexcept
{
	require(stream.state->lock.locked);
	++stream.state->calls[static_cast<::std::size_t>(kind)];
}

inline char *read_some_underflow_define(unlocked_io_ref stream, char *, char *last) noexcept
{
	observe(stream, primitive_kind::read_some);
	return last;
}

inline void read_all_underflow_define(unlocked_io_ref stream, char *, char *) noexcept
{
	observe(stream, primitive_kind::read_all);
}

inline ::std::byte *read_some_bytes_underflow_define(unlocked_io_ref stream, ::std::byte *,
													 ::std::byte *last) noexcept
{
	observe(stream, primitive_kind::read_some_bytes);
	return last;
}

inline void read_all_bytes_underflow_define(unlocked_io_ref stream, ::std::byte *, ::std::byte *) noexcept
{
	observe(stream, primitive_kind::read_all_bytes);
}

inline ::fast_io::io_scatter_status_t scatter_read_some_underflow_define(
	unlocked_io_ref stream, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t count) noexcept
{
	observe(stream, primitive_kind::scatter_read_some);
	return {count, 0u};
}

inline void scatter_read_all_underflow_define(unlocked_io_ref stream,
											 ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t) noexcept
{
	observe(stream, primitive_kind::scatter_read_all);
}

inline ::fast_io::io_scatter_status_t scatter_read_some_bytes_underflow_define(
	unlocked_io_ref stream, ::fast_io::io_scatter_t const *, ::std::size_t count) noexcept
{
	observe(stream, primitive_kind::scatter_read_some_bytes);
	return {count, 0u};
}

inline void scatter_read_all_bytes_underflow_define(unlocked_io_ref stream, ::fast_io::io_scatter_t const *,
												   ::std::size_t) noexcept
{
	observe(stream, primitive_kind::scatter_read_all_bytes);
}

inline char *pread_some_underflow_define(unlocked_io_ref stream, char *, char *last,
											::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pread_some);
	return last;
}

inline void pread_all_underflow_define(unlocked_io_ref stream, char *, char *, ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pread_all);
}

inline ::std::byte *pread_some_bytes_underflow_define(unlocked_io_ref stream, ::std::byte *, ::std::byte *last,
													  ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pread_some_bytes);
	return last;
}

inline void pread_all_bytes_underflow_define(unlocked_io_ref stream, ::std::byte *, ::std::byte *,
												::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pread_all_bytes);
}

inline ::fast_io::io_scatter_status_t scatter_pread_some_underflow_define(
	unlocked_io_ref stream, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t count,
	::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pread_some);
	return {count, 0u};
}

inline void scatter_pread_all_underflow_define(unlocked_io_ref stream,
											  ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t,
											  ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pread_all);
}

inline ::fast_io::io_scatter_status_t scatter_pread_some_bytes_underflow_define(
	unlocked_io_ref stream, ::fast_io::io_scatter_t const *, ::std::size_t count,
	::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pread_some_bytes);
	return {count, 0u};
}

inline void scatter_pread_all_bytes_underflow_define(unlocked_io_ref stream, ::fast_io::io_scatter_t const *,
													::std::size_t, ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pread_all_bytes);
}

inline char const *write_some_overflow_define(unlocked_io_ref stream, char const *, char const *last) noexcept
{
	observe(stream, primitive_kind::write_some);
	return last;
}

inline void write_all_overflow_define(unlocked_io_ref stream, char const *, char const *) noexcept
{
	observe(stream, primitive_kind::write_all);
}

inline void output_stream_char_put_overflow_define(unlocked_io_ref stream, char) noexcept
{
	observe(stream, primitive_kind::char_put);
}

inline ::std::byte const *write_some_bytes_overflow_define(unlocked_io_ref stream, ::std::byte const *,
													   ::std::byte const *last) noexcept
{
	observe(stream, primitive_kind::write_some_bytes);
	return last;
}

inline void write_all_bytes_overflow_define(unlocked_io_ref stream, ::std::byte const *,
											   ::std::byte const *) noexcept
{
	observe(stream, primitive_kind::write_all_bytes);
}

inline ::fast_io::io_scatter_status_t scatter_write_some_overflow_define(
	unlocked_io_ref stream, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t count) noexcept
{
	observe(stream, primitive_kind::scatter_write_some);
	return {count, 0u};
}

inline void scatter_write_all_overflow_define(unlocked_io_ref stream,
											  ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t) noexcept
{
	observe(stream, primitive_kind::scatter_write_all);
}

inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	unlocked_io_ref stream, ::fast_io::io_scatter_t const *, ::std::size_t count) noexcept
{
	observe(stream, primitive_kind::scatter_write_some_bytes);
	return {count, 0u};
}

inline void scatter_write_all_bytes_overflow_define(unlocked_io_ref stream, ::fast_io::io_scatter_t const *,
													::std::size_t) noexcept
{
	observe(stream, primitive_kind::scatter_write_all_bytes);
}

inline char const *pwrite_some_overflow_define(unlocked_io_ref stream, char const *, char const *last,
											  ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pwrite_some);
	return last;
}

inline void pwrite_all_overflow_define(unlocked_io_ref stream, char const *, char const *,
										  ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pwrite_all);
}

inline ::std::byte const *pwrite_some_bytes_overflow_define(unlocked_io_ref stream, ::std::byte const *,
														::std::byte const *last,
														::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pwrite_some_bytes);
	return last;
}

inline void pwrite_all_bytes_overflow_define(unlocked_io_ref stream, ::std::byte const *, ::std::byte const *,
												 ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::pwrite_all_bytes);
}

inline ::fast_io::io_scatter_status_t scatter_pwrite_some_overflow_define(
	unlocked_io_ref stream, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t count,
	::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pwrite_some);
	return {count, 0u};
}

inline void scatter_pwrite_all_overflow_define(unlocked_io_ref stream,
											   ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t,
											   ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pwrite_all);
}

inline ::fast_io::io_scatter_status_t scatter_pwrite_some_bytes_overflow_define(
	unlocked_io_ref stream, ::fast_io::io_scatter_t const *, ::std::size_t count,
	::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pwrite_some_bytes);
	return {count, 0u};
}

inline void scatter_pwrite_all_bytes_overflow_define(unlocked_io_ref stream, ::fast_io::io_scatter_t const *,
													 ::std::size_t, ::fast_io::intfpos_t) noexcept
{
	observe(stream, primitive_kind::scatter_pwrite_all_bytes);
}

// These fixtures all advertise a mutex marker. Their negative results therefore prove failure of the protocol
// evidence itself, rather than the vacuous absence of synchronization customization.
struct marker_only_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(marker_only_ref) noexcept
{
	return {};
}

struct self_unwrapping_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(self_unwrapping_ref) noexcept
{
	return {};
}

inline constexpr self_unwrapping_ref io_stream_unlocked_ref_define(self_unwrapping_ref stream) noexcept
{
	return stream;
}

struct wide_unlocked_ref
{
	using input_char_type = wchar_t;
	using output_char_type = wchar_t;
};

struct character_mismatch_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(character_mismatch_ref) noexcept
{
	return {};
}

inline constexpr wide_unlocked_ref io_stream_unlocked_ref_define(character_mismatch_ref) noexcept
{
	return {};
}

struct malformed_mutex_proxy
{
	inline int lock() const noexcept
	{
		return 0;
	}

	inline void unlock() const noexcept
	{}
};

struct malformed_mutex_ref
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr malformed_mutex_proxy io_stream_mutex_ref_define(malformed_mutex_ref) noexcept
{
	return {};
}

inline constexpr unlocked_io_ref io_stream_unlocked_ref_define(malformed_mutex_ref) noexcept
{
	return {};
}

using namespace ::fast_io::operations::decay::defines;

static_assert(has_complete_input_stream_mutex_protocol<locked_io_ref>);
static_assert(has_complete_output_stream_mutex_protocol<locked_io_ref>);
static_assert(writable<locked_io_ref>);
static_assert(readable<unlocked_io_ref> && bytes_readable<unlocked_io_ref> && preadable<unlocked_io_ref> &&
			  bytes_preadable<unlocked_io_ref>);
static_assert(writable<unlocked_io_ref> && bytes_writable<unlocked_io_ref> && pwritable<unlocked_io_ref> &&
			  bytes_pwritable<unlocked_io_ref>);

static_assert(has_input_or_io_stream_mutex_ref_define<marker_only_ref> &&
			  has_output_or_io_stream_mutex_ref_define<marker_only_ref>);
static_assert(has_input_or_io_stream_mutex_ref_define<self_unwrapping_ref> &&
			  has_output_or_io_stream_mutex_ref_define<self_unwrapping_ref>);
static_assert(has_input_or_io_stream_mutex_ref_define<character_mismatch_ref> &&
			  has_output_or_io_stream_mutex_ref_define<character_mismatch_ref>);
static_assert(has_input_or_io_stream_mutex_ref_define<malformed_mutex_ref> &&
			  has_output_or_io_stream_mutex_ref_define<malformed_mutex_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<marker_only_ref>);
static_assert(!has_complete_output_stream_mutex_protocol<marker_only_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<self_unwrapping_ref>);
static_assert(!has_complete_output_stream_mutex_protocol<self_unwrapping_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<character_mismatch_ref>);
static_assert(!has_complete_output_stream_mutex_protocol<character_mismatch_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<malformed_mutex_ref>);
static_assert(!has_complete_output_stream_mutex_protocol<malformed_mutex_ref>);

template <typename function_type>
inline void expect_one_guard(operation_state &state, primitive_kind kind, ::std::size_t primitive_calls,
							 function_type &&function)
{
	::std::size_t const index{static_cast<::std::size_t>(kind)};
	::std::size_t const calls_before{state.calls[index]};
	::std::size_t const locks_before{state.lock.locks};
	::std::size_t const unlocks_before{state.lock.unlocks};
	function();
	require(!state.lock.locked);
	require(state.lock.locks == locks_before + 1u);
	require(state.lock.unlocks == unlocks_before + 1u);
	require(state.calls[index] == calls_before + primitive_calls);
}

template <typename function_type>
inline void expect_one_guard(operation_state &state, primitive_kind kind, function_type &&function)
{
	expect_one_guard(state, kind, 1u, static_cast<function_type &&>(function));
}

} // namespace primitive_mutex_protocol_test

int main()
{
	using namespace ::primitive_mutex_protocol_test;

	operation_state state{};
	locked_io_ref stream{__builtin_addressof(state)};
	::std::array<char, 2u> characters{'a', 'b'};
	::std::array<::std::byte, 2u> bytes{};
	::std::array<::fast_io::basic_io_scatter_t<char>, 1u> character_scatters{
		::fast_io::basic_io_scatter_t<char>{characters.data(), characters.size()}};
	::std::array<::fast_io::io_scatter_t, 1u> byte_scatters{
		::fast_io::io_scatter_t{bytes.data(), bytes.size()}};

	expect_one_guard(state, primitive_kind::read_some, [&] {
		require(::fast_io::operations::read_some(stream, characters.data(), characters.data() + characters.size()) ==
				characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::read_all, [&] {
		::fast_io::operations::read_all(stream, characters.data(), characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::read_some_bytes, [&] {
		require(::fast_io::operations::read_some_bytes(stream, bytes.data(), bytes.data() + bytes.size()) ==
				bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::read_all_bytes, [&] {
		::fast_io::operations::read_all_bytes(stream, bytes.data(), bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::scatter_read_some, [&] {
		auto const result{::fast_io::operations::scatter_read_some(stream, character_scatters.data(),
															  character_scatters.size())};
		require(result.position == character_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_read_all, [&] {
		::fast_io::operations::scatter_read_all(stream, character_scatters.data(), character_scatters.size());
	});
	expect_one_guard(state, primitive_kind::scatter_read_some_bytes, [&] {
		auto const result{::fast_io::operations::scatter_read_some_bytes(stream, byte_scatters.data(),
																byte_scatters.size())};
		require(result.position == byte_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_read_all_bytes, [&] {
		::fast_io::operations::scatter_read_all_bytes(stream, byte_scatters.data(), byte_scatters.size());
	});

	constexpr ::fast_io::intfpos_t offset{17};
	expect_one_guard(state, primitive_kind::pread_some, [&] {
		require(::fast_io::operations::pread_some(stream, characters.data(), characters.data() + characters.size(),
													 offset) == characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::pread_all, [&] {
		::fast_io::operations::pread_all(stream, characters.data(), characters.data() + characters.size(), offset);
	});
	expect_one_guard(state, primitive_kind::pread_some_bytes, [&] {
		require(::fast_io::operations::pread_some_bytes(stream, bytes.data(), bytes.data() + bytes.size(), offset) ==
				bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::pread_all_bytes, [&] {
		::fast_io::operations::pread_all_bytes(stream, bytes.data(), bytes.data() + bytes.size(), offset);
	});
	expect_one_guard(state, primitive_kind::scatter_pread_some, [&] {
		auto const result{::fast_io::operations::scatter_pread_some(
			stream, character_scatters.data(), character_scatters.size(), offset)};
		require(result.position == character_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_pread_all, [&] {
		::fast_io::operations::scatter_pread_all(stream, character_scatters.data(), character_scatters.size(), offset);
	});
	expect_one_guard(state, primitive_kind::scatter_pread_some_bytes, [&] {
		auto const result{::fast_io::operations::scatter_pread_some_bytes(
			stream, byte_scatters.data(), byte_scatters.size(), offset)};
		require(result.position == byte_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_pread_all_bytes, [&] {
		::fast_io::operations::scatter_pread_all_bytes(stream, byte_scatters.data(), byte_scatters.size(), offset);
	});

	expect_one_guard(state, primitive_kind::write_some, [&] {
		require(::fast_io::operations::write_some(stream, characters.data(), characters.data() + characters.size()) ==
				characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::write_all, [&] {
		::fast_io::operations::write_all(stream, characters.data(), characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::char_put,
					 [&] { ::fast_io::operations::char_put(stream, 'c'); });
	expect_one_guard(state, primitive_kind::write_some_bytes, [&] {
		require(::fast_io::operations::write_some_bytes(stream, bytes.data(), bytes.data() + bytes.size()) ==
				bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::write_all_bytes, [&] {
		::fast_io::operations::write_all_bytes(stream, bytes.data(), bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::scatter_write_some, [&] {
		auto const result{::fast_io::operations::scatter_write_some(stream, character_scatters.data(),
															   character_scatters.size())};
		require(result.position == character_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_write_all, [&] {
		::fast_io::operations::scatter_write_all(stream, character_scatters.data(), character_scatters.size());
	});
	expect_one_guard(state, primitive_kind::scatter_write_some_bytes, [&] {
		auto const result{::fast_io::operations::scatter_write_some_bytes(stream, byte_scatters.data(),
																 byte_scatters.size())};
		require(result.position == byte_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_write_all_bytes, [&] {
		::fast_io::operations::scatter_write_all_bytes(stream, byte_scatters.data(), byte_scatters.size());
	});

	expect_one_guard(state, primitive_kind::pwrite_some, [&] {
		require(::fast_io::operations::pwrite_some(stream, characters.data(), characters.data() + characters.size(),
													  offset) == characters.data() + characters.size());
	});
	expect_one_guard(state, primitive_kind::pwrite_all, [&] {
		::fast_io::operations::pwrite_all(stream, characters.data(), characters.data() + characters.size(), offset);
	});
	expect_one_guard(state, primitive_kind::pwrite_some_bytes, [&] {
		require(::fast_io::operations::pwrite_some_bytes(stream, bytes.data(), bytes.data() + bytes.size(), offset) ==
				bytes.data() + bytes.size());
	});
	expect_one_guard(state, primitive_kind::pwrite_all_bytes, [&] {
		::fast_io::operations::pwrite_all_bytes(stream, bytes.data(), bytes.data() + bytes.size(), offset);
	});
	expect_one_guard(state, primitive_kind::scatter_pwrite_some, [&] {
		auto const result{::fast_io::operations::scatter_pwrite_some(
			stream, character_scatters.data(), character_scatters.size(), offset)};
		require(result.position == character_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_pwrite_all, [&] {
		::fast_io::operations::scatter_pwrite_all(stream, character_scatters.data(), character_scatters.size(), offset);
	});
	expect_one_guard(state, primitive_kind::scatter_pwrite_some_bytes, [&] {
		auto const result{::fast_io::operations::scatter_pwrite_some_bytes(
			stream, byte_scatters.data(), byte_scatters.size(), offset)};
		require(result.position == byte_scatters.size() && result.position_in_scatter == 0u);
	});
	expect_one_guard(state, primitive_kind::scatter_pwrite_all_bytes, [&] {
		::fast_io::operations::scatter_pwrite_all_bytes(stream, byte_scatters.data(), byte_scatters.size(), offset);
	});

	// A noncontiguous range takes the iterator dispatcher. Two leaf writes are expected, but the outer range operation
	// owns one guard; locking each recursive scalar write would change the observed count from one to two.
	::std::list<char> range{'x', 'y'};
	expect_one_guard(state, primitive_kind::write_all, 2u,
					 [&] { ::fast_io::operations::write_all_range(stream, range); });
}
