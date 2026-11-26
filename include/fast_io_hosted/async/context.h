#pragma once

namespace fast_io::async::details
{

/** @brief Owns all completion sources for one issuer thread. */
class io_context
{
#if defined(FAST_IO_ASYNC_POLL)
	poll_wait fallback;
#endif
#if defined(FAST_IO_ASYNC_URING)
	uring ring;
#endif
#if defined(FAST_IO_ASYNC_EPOLL)
	epoll readiness;
#endif
#if defined(FAST_IO_ASYNC_SELECT)
	select_wait readiness_select;
#endif
#if defined(FAST_IO_ASYNC_KQUEUE)
	kqueue_wait kqueue;
#endif
#if defined(FAST_IO_ASYNC_NT)
	nt_wait nt;
#elif defined(FAST_IO_ASYNC_WIN9X)
	windows_wait nt;
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
	workers blocking;
#endif

#if defined(FAST_IO_ASYNC_LINUX)
	/** @brief Waits for any of the active Linux completion descriptors. */
	void wait_descriptors([[maybe_unused]] request_queue &ready) noexcept
	{
		::pollfd descriptors[3];
		unsigned count{};
#if defined(FAST_IO_ASYNC_URING)
		if (ring.busy())
		{
			descriptors[count++] = {ring.descriptor(), POLLIN, 0};
		}
#endif
#if defined(FAST_IO_ASYNC_EPOLL)
		if (readiness.busy())
		{
			descriptors[count++] = {readiness.descriptor(), POLLIN, 0};
		}
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
		if (blocking.busy())
		{
			if (count == 0
#if defined(FAST_IO_ASYNC_POLL)
				&& !fallback.busy()
#endif
			)
			{
				// All outstanding work uses this completion queue. No descriptor needs to be polled.
				blocking.reap(ready, true);
				return;
			}
			descriptors[count++] = {blocking.arm_descriptor(), POLLIN, 0};
		}
#endif
#if defined(FAST_IO_ASYNC_POLL)
		if (fallback.busy())
		{
			fallback.wait(descriptors, count, ready);
			return;
		}
#endif
		if (count == 0) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}

		int result;
		do
		{
			result = linux_poll_descriptors(descriptors, count, true);
		} while (result == -EINTR);
		if (result < 0) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
#endif

#if defined(FAST_IO_ASYNC_SPLICE) || defined(FAST_IO_ASYNC_FILE_WORKER)
	struct pipe_cache
	{
		struct entry
		{
			int ends[2];
			unsigned capacity;
		} pairs[8];
		unsigned size{};
		/** @brief Closes every idle pipe when its issuing thread exits. */
		~pipe_cache()
		{
			while (size != 0)
			{
				auto &ends{pairs[--size].ends};
				::fast_io::system_call<__NR_close, int>(ends[0]);
				::fast_io::system_call<__NR_close, int>(ends[1]);
			}
		}
	} idle_pipes;
#endif

public:
#if defined(FAST_IO_ASYNC_URING)
	/** @brief Probes kernel completion support before acquiring resources for an optional direct transfer. */
	bool has_ring() noexcept
	{
		return ring.available();
	}

	/** @brief Starts a sealed prefix before a lone issuer spends its next activation formatting.
	 * @details The SQ head confirms admission. A batch of peers can share its normal ring enter.
	 */
	void launch_prefix() noexcept
	{
		if (current_loop->root_count == 1 && current_loop->ready.empty() && ring.busy()) ring.enter(false);
	}
#endif
#if defined(FAST_IO_ASYNC_SPLICE) || defined(FAST_IO_ASYNC_FILE_WORKER)
#if defined(FAST_IO_ASYNC_FILE_WORKER)
	/** @brief Remembers the last filesystem strategy observed by this issuer's workers. */
	struct
	{
		::std::uint_least64_t device{};
		bool use_pipe{};
	} file_strategy;
#endif
	static inline constexpr unsigned splice_chunk{1024u * 1024u};

	/** @brief Takes an idle pipe on the issuer, where the cache has a single owner. */
	unsigned take_pipe(int (&ends)[2]) noexcept
	{
		if (idle_pipes.size != 0)
		{
			auto const &pair{idle_pipes.pairs[--idle_pipes.size]};
			ends[0] = pair.ends[0];
			ends[1] = pair.ends[1];
			return pair.capacity;
		}
		return 0;
	}

	/** @brief Creates a private pipe on the executing thread after the filesystem selects that method. */
	static unsigned open_pipe(int (&ends)[2], unsigned requested) noexcept
	{
		if (::fast_io::details::linux_private_pipe(ends, false) < 0)
		{
			return 0;
		}
		// A cgroup or pipe quota can refuse the larger capacity. Query the size the kernel actually granted.
		auto control = [&](int command, unsigned argument) noexcept {
#if defined(__NR_fcntl)
			return ::fast_io::system_call<__NR_fcntl, int>(ends[1], command, argument);
#else
			return ::fcntl(ends[1], command, static_cast<int>(argument));
#endif
		};
		auto capacity{control(F_SETPIPE_SZ, requested)};
#ifdef F_GETPIPE_SZ
		if (capacity <= 0)
		{
			capacity = control(F_GETPIPE_SZ, 0);
		}
#endif
		// On a kernel without a capacity query the transfer still handles any short completion.
		return capacity > 0 ? static_cast<unsigned>(capacity) : requested;
	}

#if defined(FAST_IO_ASYNC_SPLICE)
	/** @brief Leases the pipe needed by an issuer that submits linked ring operations. */
	unsigned acquire_pipe(int (&ends)[2]) noexcept
	{
		auto capacity{take_pipe(ends)};
		return capacity ? capacity : open_pipe(ends, splice_chunk);
	}
#endif

	/** @brief Reuses only proven-empty pipes and bounds the number of idle kernel descriptors. */
	void release_pipe(int const (&ends)[2], unsigned capacity, bool empty) noexcept
	{
		if (empty && idle_pipes.size != sizeof(idle_pipes.pairs) / sizeof(*idle_pipes.pairs))
		{
			auto &pair{idle_pipes.pairs[idle_pipes.size++]};
			pair.ends[0] = ends[0];
			pair.ends[1] = ends[1];
			pair.capacity = capacity;
		}
		else
		{
			::fast_io::system_call<__NR_close, int>(ends[0]);
			::fast_io::system_call<__NR_close, int>(ends[1]);
		}
	}

#endif
#if defined(FAST_IO_ASYNC_SPLICE)
	/** @brief Enqueues the sequence as a unit and returns ownership after its last CQE. */
	bool submit_batch(splice_submission &group, ::std::coroutine_handle<> handle) noexcept
	{
		auto &first{*group.entries};
		if (!ring.available())
		{
			first.result = -EOPNOTSUPP;
			return false;
		}
		first.continuation = handle;
		group.remaining = group.count;
		ring.push(first);
		return true;
	}
#endif
	/** @brief Submits opaque I/O to a worker when the target has a usable thread runtime. */
	bool submit([[maybe_unused]] work_request &request) noexcept
	{
#if defined(FAST_IO_ASYNC_WORKERS)
		return blocking.push(request);
#elif (defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)) && !defined(__SINGLE_THREAD__)
		return nt.push(request);
#else
		return false;
#endif
	}

#if defined(FAST_IO_ASYNC_POSIX)
	/** @brief Arms readiness for a descriptor already known to be nonblocking. */
	bool wait_ready([[maybe_unused]] posix_request &request) noexcept
	{
#if defined(FAST_IO_ASYNC_EPOLL)
		if (readiness.push(request))
		{
			return true;
		}
#endif
#if defined(FAST_IO_ASYNC_KQUEUE)
		if (kqueue.available())
		{
			return kqueue.push(request);
		}
#endif
#if defined(FAST_IO_ASYNC_SELECT)
		return readiness_select.push(request);
#elif defined(FAST_IO_ASYNC_POLL)
		return fallback.push(request);
#else
		return false;
#endif
	}

	/**
	 * @brief Performs a readiness-safe retry or dispatches blocking descriptors to an available worker.
	 * @details Initial submission and completion retries use the same fallback. On GCC this boundary keeps
	 * the readiness and worker dispatch code out of each copy of the ring completion loop.
	 */
#if defined(__GNUC__) && !defined(__clang__) && __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	bool submit_without_ring(posix_request &request) noexcept
	{
		if (request.nonblocking())
		{
			// Try immediately. Only a would-block result needs a readiness wait.
			request.perform();
			request.admitted_on_issuer();
			return (request.result == -EAGAIN || request.result == -EWOULDBLOCK) && wait_ready(request);
		}
#if defined(FAST_IO_ASYNC_KQUEUE)
		if (current_loop->standalone_root && current_loop->ready.empty())
		{
			// The outermost consumer already waits for its sole root. Keep a blocking syscall on its issuer.
			request.perform();
			return false;
		}
#endif
		if (submit(static_cast<work_request &>(request)))
		{
			return true;
		}
		request.perform();
		return false;
	}

	/** @brief Chooses completion I/O, nonblocking readiness, or safe blocking execution. */
	bool submit(posix_request &request) noexcept
	{
#if defined(FAST_IO_ASYNC_URING)
		if (ring.available() && request.ring_offset_safe()) [[likely]]
		{
			ring.push(request);
			return true;
		}
#endif
		return submit_without_ring(request);
	}
#endif

#if defined(FAST_IO_ASYNC_NT)
	/** @brief Identifies a sole outermost NT consumer before selecting its completion source. */
	bool submit(nt_request &request) noexcept
	{
		return nt.push(request, current_loop->standalone_root && current_loop->ready.empty());
	}
#endif

	/** @brief Collects at least one completion and resumes it on the original issuer. */
	void step(request_queue &ready)
	{
		do
		{
#if defined(FAST_IO_ASYNC_URING)
			if (ring.busy())
			{
				ring.reap([&](posix_request &request) noexcept {
#if defined(FAST_IO_ASYNC_SPLICE)
					if (request.group != nullptr)
					{
						// The entire linked sequence has retired before the coroutine interprets partial progress.
						ready.push(request);
						return;
					}
#endif
					if ((request.result == -EAGAIN || request.result == -EWOULDBLOCK) &&
						request.nonblocking() && wait_ready(request))
					{
						return;
					}
					if ((request.result == -EINTR || request.result == -EOPNOTSUPP || request.result == -ENOSYS) &&
						submit_without_ring(request))
					{
						return;
					}
					ready.push(request);
				});
			}
#endif
#if defined(FAST_IO_ASYNC_EPOLL)
			if (readiness.busy())
			{
				bool const wait_in_epoll{ready.empty() && !fallback.busy()
#if defined(FAST_IO_ASYNC_URING)
										 && !ring.busy()
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
										 && !blocking.busy()
#endif
				};
				// A single completion source can sleep and collect events in the same syscall.
				if (readiness.reap(ready, fallback, wait_in_epoll) && wait_in_epoll)
				{
					continue;
				}
			}
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
			if (blocking.busy())
			{
				blocking.reap(ready, false);
			}
#endif
			if (!ready.empty())
			{
#if defined(FAST_IO_ASYNC_URING)
				if (ring.busy())
				{
					ring.enter(false);
				}
#endif
				// Poll every armed readiness source before resuming tasks that may yield repeatedly.
#if defined(FAST_IO_ASYNC_POLL)
				if (fallback.busy())
				{
					fallback.wait(nullptr, 0, ready, false);
				}
#endif
#if defined(FAST_IO_ASYNC_KQUEUE)
				if (kqueue.busy())
				{
					kqueue.wait(ready, -1, false);
				}
#endif
#if defined(FAST_IO_ASYNC_SELECT)
				if (readiness_select.busy())
				{
					readiness_select.wait(ready, -1, false);
				}
#endif
#if defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)
				nt.wait(ready, false);
#endif
				break;
			}

#if defined(FAST_IO_ASYNC_URING)
			if (ring.busy())
			{
				bool const wait_in_ring{
#if defined(FAST_IO_ASYNC_POLL)
					!fallback.busy() &&
#endif
#if defined(FAST_IO_ASYNC_EPOLL)
					!readiness.busy() &&
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
					!blocking.busy() &&
#endif
					true};
				ring.enter(wait_in_ring);
				if (wait_in_ring)
				{
					continue;
				}
			}
#endif
#if defined(FAST_IO_ASYNC_SELECT) || defined(FAST_IO_ASYNC_KQUEUE)
#if defined(FAST_IO_ASYNC_WORKERS)
			int const wake_fd{blocking.busy() ? blocking.arm_descriptor() : -1};
#else
			constexpr int wake_fd{-1};
#endif
#if defined(FAST_IO_ASYNC_KQUEUE)
			if (kqueue.busy())
			{
				kqueue.wait(ready, wake_fd);
				continue;
			}
#endif
#if defined(FAST_IO_ASYNC_SELECT)
			readiness_select.wait(ready, wake_fd);
#endif
#elif defined(FAST_IO_ASYNC_LINUX)
			wait_descriptors(ready);
#elif defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)
			nt.wait(ready);
#elif defined(FAST_IO_ASYNC_WORKERS)
			if (!blocking.busy()) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			blocking.reap(ready, true);
#else
			// An unfinished root must be waiting for a completion from this loop.
			::fast_io::fast_terminate();
#endif
		} while (ready.empty());
	}
};

#if defined(FAST_IO_ASYNC_POSIX) || defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)
/**
 * @brief Polls the native completion sources of an attached portable issuer.
 * @details GCC can recognize this callback inside drive() and clone the entire Linux polling loop there.
 * Keeping the indirect call boundary on the measured compiler family shares the same wait code across roots.
 */
#if defined(__linux__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15 && __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void step_platform(void *context, request_queue &ready)
{
	static_cast<io_context *>(context)->step(ready);
}

/** @brief Retrieves the active native issuer, allowing an application-owned event loop to operate independently. */
inline io_context *platform_context() noexcept
{
	return current_loop != nullptr && current_loop->step == step_platform ? static_cast<io_context *>(current_loop->context) : nullptr;
}

/** @brief Reuses native queues on the issuing thread across successive outermost waits. */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline io_context &thread_context() noexcept
{
#if defined(__SINGLE_THREAD__)
	static io_context context;
#else
	static thread_local io_context context;
#endif
	return context;
}

/** @brief Adds OS completions to the portable issuer without changing its cooperative ready queue. */
inline void attach(io_context &context, scheduler &loop) noexcept
{
	loop.context = ::fast_io::freestanding::addressof(context);
	loop.step = step_platform;
	loop.submit_work = [](void *state, work_request &request) noexcept { return static_cast<io_context *>(state)->submit(request); };
}

/** @brief Shares the native issuer's root scheduler across outer and reentrant callers. */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void run_platform_in(io_context &context, ::std::coroutine_handle<> const *roots, ::std::size_t size, ::std::coroutine_handle<> const *entries = nullptr)
{
	scheduler loop{};
	attach(context, loop);
	if (size == 1 && current_loop == nullptr)
	{
		// The root has no siblings. Nested children and multi-root batches still use normal worker submission.
		loop.standalone_root = roots[0];
	}
	drive(loop, roots, size, entries);
}

/** @brief Reuses native queues between waits and isolates wait sets during reentrant calls. */
inline void run_platform(::std::coroutine_handle<> const *roots, ::std::size_t size, ::std::coroutine_handle<> const *entries = nullptr)
{
	if (current_loop == nullptr)
	{
		run_platform_in(thread_context(), roots, size, entries);
		return;
	}
	io_context nested;
	run_platform_in(nested, roots, size, entries);
}

/** @brief Activates the OS issuer on demand, including when the first marked operation is created inside a root. */
inline void enable_platform() noexcept
{
	if (hosted_driver.load(::std::memory_order_relaxed) == nullptr)
	{
		hosted_driver.store(run_platform, ::std::memory_order_relaxed);
	}
	if (current_loop != nullptr && current_loop->step == nullptr)
	{
		attach(thread_context(), *current_loop);
	}
}
#endif

} // namespace fast_io::async::details

#if defined(FAST_IO_ASYNC_POSIX) || defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)
namespace fast_io
{

/** @brief Identifies the issuing environment supplied by a hosted inclusion at the device-marking boundary. */
template <typename T>
inline consteval auto async_scheduler_define(io_type_t<T>) noexcept
{
	return ::fast_io::async::details::enable_platform;
}

} // namespace fast_io
#endif
