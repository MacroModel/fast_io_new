#include <fast_io.h>

#if defined(__unix__) || defined(__APPLE__)

#include <cstddef>
#include <new>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

struct panic_empty_state
{
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t status_calls{};
	bool locked{};
};

struct panic_empty_sink
{
	using output_char_type = char;
	panic_empty_state *state{};
};

struct panic_empty_unlocked_sink
{
	using output_char_type = char;
	panic_empty_state *state{};
};

struct panic_empty_lock
{
	panic_empty_state *state{};

	inline void lock() noexcept
	{
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		state->locked = false;
		++state->unlocks;
	}
};

inline constexpr panic_empty_sink output_stream_ref_define(
	panic_empty_sink sink) noexcept
{
	return sink;
}

inline constexpr panic_empty_lock output_stream_mutex_ref_define(
	panic_empty_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr panic_empty_unlocked_sink output_stream_unlocked_ref_define(
	panic_empty_sink sink) noexcept
{
	return {sink.state};
}

template <bool line>
	requires(!line)
inline void status_print_define(panic_empty_unlocked_sink sink) noexcept
{
	if (!sink.state->locked)
	{
		::fast_io::fast_terminate();
	}
	++sink.state->status_calls;
}

} // namespace

int main()
{
	void *const storage{::mmap(nullptr, sizeof(panic_empty_state),
							   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)};
	if (storage == MAP_FAILED)
	{
		return 1;
	}
	auto *state{::new (storage) panic_empty_state{}};

	::rlimit no_core{};
	if (::setrlimit(RLIMIT_CORE, __builtin_addressof(no_core)) != 0)
	{
		return 2;
	}

	::pid_t const child{::fork()};
	if (child == -1)
	{
		return 3;
	}
	if (child == 0)
	{
		// The sink is the device argument, so this is a zero-source panic record.
		// Panic must enter the ordinary print level before terminating.
		::fast_io::io::panic(panic_empty_sink{state});
	}

	int child_status{};
	if (::waitpid(child, __builtin_addressof(child_status), 0) != child)
	{
		return 4;
	}
	bool const contract_holds{
		WIFSIGNALED(child_status) && state->locks == 1u &&
		state->unlocks == 1u && state->status_calls == 1u && !state->locked};
	state->~panic_empty_state();
	if (::munmap(state, sizeof(panic_empty_state)) != 0)
	{
		return 5;
	}
	return contract_holds ? 0 : 6;
}

#else

int main()
{}

#endif
