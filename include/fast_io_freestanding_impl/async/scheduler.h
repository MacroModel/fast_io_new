#pragma once

namespace fast_io::async::details
{

/** @brief Collects frame-owned continuations for one execution on the calling thread.
 * @details A board or OS provider may supply a completion pump. The default freestanding
 * loop needs only this queue and never requires a clock, thread, interrupt or system call.
 */
struct scheduler
{
	request_queue ready;
	// Only a top-level, one-root hosted wait may complete its own synchronous leaf on the issuer.
	::std::coroutine_handle<> standalone_root{};
	// The issuer exposes the number of roots submitted together to its execution policy.
	::std::size_t root_count{};
	// A submission-only ring enter is useful when an actual observer is waiting for admission.
	::std::size_t admission_waiters{};
	// A phase observer may attach to an independently submitted root without resuming it a second time.
	::std::coroutine_handle<> const *roots{};
	void *context{};
	void (*step)(void *, request_queue &){};
	bool (*submit_work)(void *, work_request &) noexcept {};
};

#if ((__STDC_HOSTED__ == 1 && !defined(_LIBCPP_FREESTANDING) && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES)) && !defined(__SINGLE_THREAD__) && !defined(__MSDOS__) && !defined(__AVR__)
inline thread_local scheduler *current_loop{};
#elif defined(__pdp11__) && defined(__GXX_WEAK__) && !__GXX_WEAK__ && !defined(__ELF__)
// GNU PDP-11 a.out emits a strong definition for a writable inline variable in every translation unit.
// A linker common declaration coalesces into one zero-initialized, word-aligned address. It emits no instructions.
static_assert(sizeof(scheduler *) == 2);
extern scheduler *current_loop __asm__("__fast_io_async_current_loop");
__asm__(".comm __fast_io_async_current_loop,2");
#else
inline scheduler *current_loop{};
#endif

/** @brief Checks batch membership only when a caller explicitly observes an intermediate phase. */
inline bool submitted_root(::std::coroutine_handle<> handle) noexcept
{
	if (current_loop)
		for (::std::size_t i{}; i != current_loop->root_count; ++i)
			if (current_loop->roots[i] == handle) return true;
	return false;
}

/** @brief Restores the enclosing issuer after a nested wait. */
struct scheduler_scope
{
	scheduler *previous{current_loop};

	/** @brief Installs the active issuer for cooperative yields and backend submissions. */
	explicit scheduler_scope(scheduler &loop) noexcept
	{
		current_loop = ::fast_io::freestanding::addressof(loop);
	}
	scheduler_scope(scheduler_scope const &) = delete;
	scheduler_scope &operator=(scheduler_scope const &) = delete;

	/** @brief Returns submissions to the previously active loop. */
	~scheduler_scope()
	{
		current_loop = previous;
	}
};

/** @brief Validates every handle in a batch before resuming the first root. */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void start_roots(::std::coroutine_handle<> const *roots, ::std::size_t size, ::std::coroutine_handle<> const *entries = nullptr)
{
	for (::std::size_t index{}; index != size; ++index)
	{
		if (!roots[index]) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		for (::std::size_t prior{}; prior != index; ++prior)
		{
			if (roots[prior] == roots[index]) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		if (!roots[index].done())
		{
			(entries ? entries[index] : roots[index]).resume();
		}
	}
}

/** @brief Starts distinct roots and pumps completions until all have reached final suspend. */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void drive(scheduler &loop, ::std::coroutine_handle<> const *roots, ::std::size_t size, ::std::coroutine_handle<> const *entries = nullptr)
{
	loop.root_count = size;
	loop.roots = roots;
	scheduler_scope scope{loop};
	if (size == 1)
	{
		// A lone root that finishes on its issuer never enters the completion pump.
		auto root{*roots};
		if (!root) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if (!root.done())
		{
			(entries ? entries[0] : root).resume();
		}
		if (root.done())
		{
			return;
		}
	}
	else
	{
		start_roots(roots, size, entries);
	}
	for (::std::size_t completed{}; completed != size;)
	{
		if (roots[completed].done())
		{
			++completed;
			continue;
		}
		if (loop.step != nullptr)
		{
			loop.step(loop.context, loop.ready);
		}
		else if (loop.ready.empty()) [[unlikely]]
		{
			// A board-specific external completion needs its event pump supplied to run_with().
			::fast_io::fast_terminate();
		}
		// New yields go into the next batch, allowing external completions to make progress.
		request_queue batch;
		batch.append(loop.ready);
		while (!batch.empty())
		{
			batch.pop().continuation.resume();
		}
	}
}

#if defined(FAST_IO_ASYNC_HOSTED_CONFIG)
/** @brief Allows the hosted package to supply its issuer across translation-unit and include boundaries. */
using driver_function = void (*)(::std::coroutine_handle<> const *, ::std::size_t, ::std::coroutine_handle<> const *);
inline constinit ::std::atomic<driver_function> hosted_driver{};
#endif

/** @brief Drives standalone waits with the configured issuer or the cooperative freestanding loop. */
inline void run_roots(::std::coroutine_handle<> const *roots, ::std::size_t size, ::std::coroutine_handle<> const *entries = nullptr)
{
#if defined(FAST_IO_ASYNC_HOSTED_CONFIG)
	if (auto driver = hosted_driver.load(::std::memory_order_relaxed))
	{
		driver(roots, size, entries);
		return;
	}
#endif
	scheduler loop{};
#if defined(FAST_IO_ASYNC_HOSTED_CONFIG)
	if (size == 1 && current_loop == nullptr)
	{
		// A device may install its hosted driver inside this first root, after run_roots has begun.
		loop.standalone_root = roots[0];
	}
#endif
	drive(loop, roots, size, entries);
}

/** @brief Places a suspended frame at the end of its issuer's ready queue. */
struct yield_awaiter : request_link
{
	/** @brief Always visits await_suspend to discover the current issuer. */
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	/** @brief Queues a continuation or continues inline when there is no active fast_io issuer. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		if (current_loop == nullptr)
		{
			return false;
		}
		continuation = handle;
		current_loop->ready.push(*this);
		return true;
	}

	/** @brief Delivers control to the yielding coroutine. */
	constexpr void await_resume() const noexcept
	{}
};

/** @brief Gives the issuer another turn between bounded producer or parser steps. */
[[nodiscard]] inline constexpr yield_awaiter yield() noexcept
{
	return {};
}

} // namespace fast_io::async::details
