#include <array>
#include <cstddef>
#include <cstdlib>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_INPUT_MUTEX_PROTOCOL_RESTORE_FLOATING_MACRO
#endif
#include <fast_io.h>
#if defined(FAST_IO_INPUT_MUTEX_PROTOCOL_RESTORE_FLOATING_MACRO)
#undef FAST_IO_DISABLE_FLOATING_POINT
#undef FAST_IO_INPUT_MUTEX_PROTOCOL_RESTORE_FLOATING_MACRO
#endif

namespace input_mutex_protocol_test
{

inline void test_require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct lock_state
{
	bool locked{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
};

struct mutex_proxy
{
	lock_state *state{};

	inline void lock() const noexcept
	{
		test_require(!state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		test_require(state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

struct operation_state
{
	lock_state lock{};
	::std::size_t scan_calls{};
	::std::size_t read_calls{};
};

struct unlocked_operation_ref
{
	using input_char_type = char;
	operation_state *state{};
};

struct locked_operation_ref
{
	using input_char_type = char;
	operation_state *state{};
};

struct operation_source
{
	operation_state state{};
};

inline constexpr locked_operation_ref input_stream_ref_define(operation_source &source) noexcept
{
	return {__builtin_addressof(source.state)};
}

inline constexpr mutex_proxy input_stream_mutex_ref_define(locked_operation_ref source) noexcept
{
	return {__builtin_addressof(source.state->lock)};
}

inline constexpr unlocked_operation_ref input_stream_unlocked_ref_define(locked_operation_ref source) noexcept
{
	return {source.state};
}

struct scan_target
{
	bool value{};
};

struct scan_proxy
{
	scan_target *target{};
};

inline constexpr scan_proxy scan_alias_define(::fast_io::io_alias_t, scan_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline bool status_scan_define(unlocked_operation_ref source, scan_proxy proxy) noexcept
{
	// The observation is part of this test's proof: the high-level status CPO must execute inside, rather than beside,
	// the stream-level critical section.
	test_require(source.state->lock.locked);
	++source.state->scan_calls;
	proxy.target->value = true;
	return true;
}

inline void read_all_underflow_define(unlocked_operation_ref source, char *first, char *last) noexcept
{
	// Primitive read dispatch must share the same protocol and must not reacquire while recursively unwrapping.
	test_require(source.state->lock.locked);
	++source.state->read_calls;
	for (; first != last; ++first)
	{
		*first = 'r';
	}
}

// The following deliberately incomplete protocols isolate every conjunct of the complete protocol. They are compile-
// time fixtures only: none is ever passed to an operation body, because recognition must reject it first.
struct marker_only_ref
{
	using input_char_type = char;
	lock_state *state{};
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(marker_only_ref source) noexcept
{
	return {source.state};
}

struct wide_unlocked_ref
{
	using input_char_type = wchar_t;
};

struct wrong_character_ref
{
	using input_char_type = char;
	lock_state *state{};
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(wrong_character_ref source) noexcept
{
	return {source.state};
}

inline constexpr wide_unlocked_ref input_stream_unlocked_ref_define(wrong_character_ref) noexcept
{
	return {};
}

struct self_unlocked_ref
{
	using input_char_type = char;
	lock_state *state{};
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(self_unlocked_ref source) noexcept
{
	return {source.state};
}

inline constexpr self_unlocked_ref input_stream_unlocked_ref_define(self_unlocked_ref source) noexcept
{
	return source;
}

struct malformed_lock_proxy
{
	inline int lock() const noexcept
	{
		return 0;
	}
	inline void unlock() const noexcept
	{}
};

struct malformed_lock_ref
{
	using input_char_type = char;
};

inline constexpr malformed_lock_proxy input_stream_mutex_ref_define(malformed_lock_ref) noexcept
{
	return {};
}

inline constexpr unlocked_operation_ref input_stream_unlocked_ref_define(malformed_lock_ref) noexcept
{
	return {};
}

struct malformed_unlock_proxy
{
	inline void lock() const noexcept
	{}
	inline int unlock() const noexcept
	{
		return 0;
	}
};

struct malformed_unlock_ref
{
	using input_char_type = char;
};

inline constexpr malformed_unlock_proxy input_stream_mutex_ref_define(malformed_unlock_ref) noexcept
{
	return {};
}

inline constexpr unlocked_operation_ref input_stream_unlocked_ref_define(malformed_unlock_ref) noexcept
{
	return {};
}

struct noncopyable_mutex_proxy
{
	noncopyable_mutex_proxy() = default;
	noncopyable_mutex_proxy(noncopyable_mutex_proxy const &) = delete;
	noncopyable_mutex_proxy &operator=(noncopyable_mutex_proxy const &) = delete;

	inline void lock() const noexcept
	{}
	inline void unlock() const noexcept
	{}
};

struct nonstorable_mutex_state
{
	noncopyable_mutex_proxy mutex{};
};

struct nonstorable_mutex_ref
{
	using input_char_type = char;
	nonstorable_mutex_state *state{};
};

inline constexpr noncopyable_mutex_proxy &input_stream_mutex_ref_define(nonstorable_mutex_ref source) noexcept
{
	return source.state->mutex;
}

inline constexpr unlocked_operation_ref input_stream_unlocked_ref_define(nonstorable_mutex_ref) noexcept
{
	return {};
}

struct borrowed_noncopyable_unlocked_ref
{
	using input_char_type = char;

	borrowed_noncopyable_unlocked_ref() = default;
	borrowed_noncopyable_unlocked_ref(borrowed_noncopyable_unlocked_ref const &) = delete;
	borrowed_noncopyable_unlocked_ref &operator=(borrowed_noncopyable_unlocked_ref const &) = delete;
};

struct borrowed_unlocked_state
{
	lock_state lock{};
	borrowed_noncopyable_unlocked_ref unlocked{};
};

struct borrowed_unlocked_locked_ref
{
	using input_char_type = char;
	borrowed_unlocked_state *state{};
};

inline constexpr mutex_proxy input_stream_mutex_ref_define(borrowed_unlocked_locked_ref source) noexcept
{
	return {__builtin_addressof(source.state->lock)};
}

inline constexpr borrowed_noncopyable_unlocked_ref &
input_stream_unlocked_ref_define(borrowed_unlocked_locked_ref source) noexcept
{
	return source.state->unlocked;
}

struct io_unlocked_ref
{
	using input_char_type = char;
};

struct io_locked_ref
{
	using input_char_type = char;
	lock_state *state{};
};

inline constexpr mutex_proxy io_stream_mutex_ref_define(io_locked_ref source) noexcept
{
	return {source.state};
}

inline constexpr io_unlocked_ref io_stream_unlocked_ref_define(io_locked_ref) noexcept
{
	return {};
}

using ::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol;

static_assert(has_complete_input_stream_mutex_protocol<locked_operation_ref>);
static_assert(has_complete_input_stream_mutex_protocol<io_locked_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<marker_only_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<wrong_character_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<self_unlocked_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<malformed_lock_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<malformed_unlock_ref>);
static_assert(!has_complete_input_stream_mutex_protocol<nonstorable_mutex_ref>);
// A mutable lvalue projection already has storage owned by the locked object. Requiring copy construction here would
// reject the exact identity-preserving route used by mutex recursion; only prvalue/xvalue projections need ownership.
static_assert(has_complete_input_stream_mutex_protocol<borrowed_unlocked_locked_ref>);

inline void require_one_completed_lock(lock_state const &state)
{
	test_require(!state.locked);
	test_require(state.lock_calls == 1u);
	test_require(state.unlock_calls == 1u);
}

} // namespace input_mutex_protocol_test

int main()
{
	using namespace ::input_mutex_protocol_test;

	operation_source scan_source;
	scan_target target;
	test_require(::fast_io::io::scan<true>(scan_source, target));
	test_require(target.value);
	test_require(scan_source.state.scan_calls == 1u);
	require_one_completed_lock(scan_source.state.lock);

	operation_source read_source;
	::std::array<char, 7u> storage{};
	::fast_io::operations::decay::read_all_decay(
		input_stream_ref_define(read_source), storage.data(), storage.data() + storage.size());
	for (char ch : storage)
	{
		test_require(ch == 'r');
	}
	test_require(read_source.state.read_calls == 1u);
	require_one_completed_lock(read_source.state.lock);
}
