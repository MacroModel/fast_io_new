#pragma once

#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
#include "file_cpu_placement.h"
#endif

namespace fast_io::async::details
{

/**
 * @brief Executes opaque synchronous CPOs while their coroutines are suspended.
 * @details Workers publish completions to the issuer, which alone resumes continuations. Workers are reused and created only when all existing workers have pending I/O. For backends with native waits, a kernel notification also wakes the issuer.
 */
class workers
{
	struct thread_node
	{
		thread_node *next;
		workers *pool;
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
		::std::size_t slot;
#endif
		::pthread_t id;
	};
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
	static inline thread_local ::std::size_t local_slot{};
	static inline thread_local file_cpu_placement placement{};
#endif
	using node_allocator = ::fast_io::native_typed_global_allocator<thread_node>;
	::fast_io::posix_pthread_mutex mutex;
	::pthread_cond_t work_available = PTHREAD_COND_INITIALIZER;
	::pthread_cond_t completion_available = PTHREAD_COND_INITIALIZER;
	request_queue incoming, completed;
	thread_node *threads{};
	::std::size_t thread_count{}, outstanding{}, completed_count{};
	bool descriptor_armed{}, token_pending{}, condition_waiting{};
	bool shutting_down{};
	int wake_read{-1}, wake_write{-1};

	/** @brief Opens a nonblocking kernel notification shared by all workers of this issuer. */
	bool open_notification() noexcept
	{
#if defined(FAST_IO_ASYNC_LINUX) && defined(O_CLOEXEC)
#if defined(FAST_IO_ASYNC_EVENTFD) && defined(__NR_eventfd2)
		wake_read = ::fast_io::system_call<__NR_eventfd2, int>(0u, EFD_CLOEXEC | EFD_NONBLOCK);
		if (wake_read >= 0)
		{
			wake_write = wake_read;
#if defined(FAST_IO_ASYNC_SELECT)
			if (wake_read >= FD_SETSIZE)
			{
				close_notification();
				return false;
			}
#endif
			return true;
		}
		if (wake_read != -ENOSYS)
		{
			return false;
		}
#endif
		// The same eight-byte wakeup protocol works on kernels predating eventfd.
		int ends[2];
		if (::fast_io::details::linux_private_pipe(ends, true) < 0)
		{
			return false;
		}
		wake_read = ends[0];
		wake_write = ends[1];
#else
		int ends[2];
		if (::pipe(ends) == -1)
		{
			return false;
		}
		wake_read = ends[0];
		wake_write = ends[1];
		for (int fd : ends)
		{
			if (::fcntl(fd, F_SETFL, O_NONBLOCK) == -1 || ::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
			{
				close_notification();
				return false;
			}
		}
#endif
#if defined(FAST_IO_ASYNC_SELECT)
		if (wake_read >= FD_SETSIZE)
		{
			close_notification();
			return false;
		}
#endif
		return wake_read >= 0;
	}

	/** @brief Closes each owned notification descriptor exactly once. */
	void close_notification() noexcept
	{
		if (wake_read >= 0)
		{
			::fast_io::details::sys_close(wake_read);
			if (wake_write != wake_read)
			{
				::fast_io::details::sys_close(wake_write);
			}
		}
		wake_read = wake_write = -1;
	}

	/**
	 * @brief Publishes or consumes the single eight-byte notification belonging to a queue transition.
	 * @details Both eventfd and the legacy private pipe use the same token. The existing byte request
	 * retries EINTR and selects the raw Linux syscall or the host POSIX primitive at compile time.
	 */
	template <bool signal>
	void transfer_notification() noexcept
	{
		::std::uint_least64_t value{1};
		posix_request request{signal ? wake_write : wake_read, &value, sizeof(value), signal};
		request.perform();
	}

	/** @brief Runs an I/O work item without holding the completion queue lock. */
	void work() noexcept
	{
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
		decltype(work_request::execute) previous{};
#endif
		mutex.lock();
		for (;;)
		{
			while (!shutting_down && incoming.empty())
			{
				::pthread_cond_wait(&work_available, &mutex.mutex);
			}
			if (shutting_down)
			{
				mutex.unlock();
				return;
			}
			auto &request{static_cast<work_request &>(incoming.pop())};
			mutex.unlock();
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
			// An opaque operation may inspect or change its thread affinity. Restore the inherited set
			// before crossing the executor boundary, even when the previous job exited with an error.
			if (request.execute != previous)
			{
				placement.release();
				previous = request.execute;
			}
#endif
			request.execute(request);
			mutex.lock();
			if (completed.empty())
			{
				// Only the wait primitive the issuer actually armed needs a wakeup.
				if (descriptor_armed)
				{
					transfer_notification<true>();
					token_pending = true;
				}
				else if (condition_waiting)
				{
					::pthread_cond_signal(&completion_available);
				}
			}
			completed.push(request);
			++completed_count;
		}
	}

public:
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
	/** @brief Reuses this worker's CPU placement while it services a saturated run of regular-file copies. */
	static void place_file(::std::size_t width) noexcept { placement.select(width, local_slot); }
#endif

	workers() noexcept = default;
	workers(workers const &) = delete;
	workers &operator=(workers const &) = delete;

	/** @brief Stops idle workers and releases the loop's kernel notification. */
	~workers()
	{
		if (threads != nullptr)
		{
			{
				::fast_io::io_lock_guard lock{mutex};
				shutting_down = true;
			}
			::pthread_cond_broadcast(&work_available);
			while (threads != nullptr)
			{
				auto *node{threads};
				threads = node->next;
				::pthread_join(node->id, nullptr);
				node_allocator::deallocate_n(node, 1u);
			}
		}
		::pthread_cond_destroy(&completion_available);
		::pthread_cond_destroy(&work_available);
		close_notification();
	}

	/**
	 * @brief Queues one work item, or leaves it with the caller if workers cannot be started.
	 * @details Keep a single copy of lazy thread and notification setup across the opaque and descriptor
	 * submission paths. GCC otherwise expands both copies into the issuer.
	 */
#if defined(__GNUC__) && !defined(__clang__) && __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	bool push(work_request &request) noexcept
	{
		if (wake_read < 0 && !open_notification())
		{
			return false;
		}
		if (outstanding >= thread_count)
		{
			// A queued writer can unblock an earlier blocking reader. Keep that writer runnable.
			auto *node{::new (static_cast<void *>(node_allocator::allocate(1u))) thread_node{threads, this,
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
				thread_count,
#endif
				{}}};
			auto const status{::pthread_create(&node->id, nullptr, [](void *state) noexcept -> void * {
				auto *worker{static_cast<thread_node *>(state)};
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
				local_slot = worker->slot;
#endif
				worker->pool->work();
				return nullptr; }, node)};
			if (status != 0)
			{
				node_allocator::deallocate_n(node, 1u);
				if (threads == nullptr)
				{
					close_notification();
				}
				return false;
			}
			threads = node;
			++thread_count;
		}

		{
			::fast_io::io_lock_guard lock{mutex};
			++outstanding;
			incoming.push(request);
		}
		::pthread_cond_signal(&work_available);
		return true;
	}

	/** @brief Reports whether any work remains queued, running, or completed but unread. */
	bool busy() const noexcept
	{
		return outstanding != 0;
	}

	/** @brief Arms the kernel notification while holding the same lock that publishes a worker completion. */
	int arm_descriptor() noexcept
	{
		::fast_io::io_lock_guard lock{mutex};
		descriptor_armed = true;
		if (!completed.empty() && !token_pending)
		{
			// Completion may have raced with registration since the issuer's nonblocking queue check.
			transfer_notification<true>();
			token_pending = true;
		}
		return wake_read;
	}

	/** @brief Publishes completed work to the issuer, optionally waiting for the first completion. */
	void reap(request_queue &ready, bool wait) noexcept
	{
		::fast_io::io_lock_guard lock{mutex};
		descriptor_armed = false;
		condition_waiting = wait;
		while (wait && completed.empty())
		{
			::pthread_cond_wait(&completion_available, &mutex.mutex);
		}
		condition_waiting = false;
		if (token_pending)
		{
			// A mixed wait consumes its one notification before this queue can be armed again.
			transfer_notification<false>();
			token_pending = false;
		}
		outstanding -= completed_count;
		completed_count = 0;
		ready.append(completed);
	}
};

} // namespace fast_io::async::details
