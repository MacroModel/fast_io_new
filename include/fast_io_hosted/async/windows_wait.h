#pragma once

namespace fast_io::async::details
{
#if defined(FAST_IO_ASYNC_WIN9X)
// The legacy Windows family exposes a stable C import. Its wait implementation is selected at build time.
extern "C" FAST_IO_DLLIMPORT ::std::uint_least32_t FAST_IO_WINSTDCALL WaitForMultipleObjects(
	::std::uint_least32_t, void *const *, int, ::std::uint_least32_t) noexcept;
#endif

/** @brief Multiplexes operation-owned events and worker handles using the wait API of the targeted Windows family. */
class windows_wait
{
	request_link *pending{};
	static inline constexpr ::std::uint_least32_t timed_out{0x102u};
	static inline constexpr ::std::uint_least32_t wait_limit{64u};

	/** @brief Retires a finished request before its coroutine can access the completion. */
	template <typename RetireEvent>
	static void retire(work_request &request, request_queue &ready, RetireEvent &retire_event) noexcept
	{
		if (request.close_after_wait)
		{
#if defined(FAST_IO_ASYNC_NT)
			::fast_io::win32::nt::NtClose(request.wait_handle);
#else
			::fast_io::win32::CloseHandle(request.wait_handle);
#endif
		}
		else
		{
			retire_event(request);
		}
		ready.push(request);
	}

	/** @brief Waits for one group, optionally consuming every signaled object as a single operation. */
	static ::std::uint_least32_t wait_group(::std::uint_least32_t size, void *const *handles,
		bool all, bool unbounded, bool short_wait) noexcept
	{
#if defined(FAST_IO_ASYNC_NT)
		using namespace ::fast_io::win32::nt;
		::std::uint_least64_t timeout{short_wait ? static_cast<::std::uint_least64_t>(-10000) : 0u};
		return NtWaitForMultipleObjects(size, handles, all ? wait_type::WaitAll : wait_type::WaitAny,
			0, unbounded ? nullptr : &timeout);
#else
		return WaitForMultipleObjects(size, handles, all, unbounded ? UINT32_MAX : static_cast<unsigned>(short_wait));
#endif
	}

	/** @brief Uses optional completion hints to avoid a WaitAll probe on a partially ready group. */
	template <typename CompletionHint>
	static bool all_may_be_ready(work_request *const *requests, ::std::uint_least32_t size,
		CompletionHint &completion_hint) noexcept
	{
		if (size < 4u) return false;
		for (::std::uint_least32_t i{}; i != size; ++i)
		{
			if (!completion_hint(*requests[i])) return false;
		}
		return true;
	}

	/**
	 * @brief Collects completed groups without rebuilding the array for each ready object.
	 * @details Hints may be stale. Only a successful zero-duration WaitAll retires a full group.
	 * A partially ready group uses WaitAny. In a large wait set, every group gets an immediate
	 * probe before the first group can sleep for a bounded interval.
	 */
	template <typename RetireEvent, typename CompletionHint>
#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	void wait_many(request_queue &ready, bool block, RetireEvent &retire_event, CompletionHint &completion_hint) noexcept
	{
		bool allow_short_wait{};
		do
		{
			for (auto **start{&pending}; *start != nullptr;)
			{
				void *handles[wait_limit];
				work_request *requests[wait_limit];
				::std::uint_least32_t size{};
				auto *next{*start};
				do
				{
					auto *request{static_cast<work_request *>(next)};
					handles[size] = request->wait_handle;
					requests[size++] = request;
					next = request->next;
				} while (next != nullptr && size != wait_limit);

				if (all_may_be_ready(requests, size, completion_hint))
				{
					auto const status{wait_group(size, handles, true, false, false)};
					if (status == 0)
					{
						*start = next;
						for (::std::uint_least32_t i{}; i != size; ++i)
						{
							retire(*requests[i], ready, retire_event);
						}
						return;
					}
					if (status != timed_out) [[unlikely]]
					{
						::fast_io::fast_terminate();
					}
				}

				bool const first_group{start == &pending};
				auto const status{wait_group(size, handles, false, block && first_group && next == nullptr,
					allow_short_wait && first_group)};
				if (status < size)
				{
					auto *request{requests[status]};
					(status == 0 ? *start : requests[status - 1u]->next) = request->next;
					retire(*request, ready, retire_event);
					return;
				}
				if (status != timed_out) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				start = &requests[size - 1u]->next;
			}
			allow_short_wait = true;
		} while (block);
	}

protected:
	/** @brief Waits for native readiness and calls the owner before publishing each completion. */
	template <typename RetireEvent, typename CompletionHint>
	void wait_with(request_queue &ready, bool block, RetireEvent retire_event, CompletionHint completion_hint) noexcept
	{
		if (pending == nullptr) [[unlikely]]
		{
			if (block)
			{
				::fast_io::fast_terminate();
			}
			return;
		}
		if (pending->next != nullptr)
		{
			wait_many(ready, block, retire_event, completion_hint);
			return;
		}
		auto &request{*static_cast<work_request *>(pending)};
#if defined(FAST_IO_ASYNC_NT)
		::std::uint_least64_t timeout{};
		auto const status{::fast_io::win32::nt::NtWaitForSingleObject(request.wait_handle, 0, block ? nullptr : &timeout)};
#else
		auto const status{::fast_io::win32::WaitForSingleObject(request.wait_handle, block ? UINT32_MAX : 0u)};
#endif
		if (status == 0)
		{
			pending = nullptr;
			retire(request, ready, retire_event);
		}
		else if (status != timed_out || block) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}

	/** @brief Publishes one waitable kernel handle to the issuer's wait set. */
	void add(work_request &request) noexcept
	{
		request.next = pending;
		pending = ::fast_io::freestanding::addressof(request);
	}

	/** @brief Completes one opaque operation on a worker thread and signals completion by exiting. */
	static ::std::uint_least32_t FAST_IO_WINSTDCALL run_work(void *argument) noexcept
	{
		auto &request{*static_cast<work_request *>(argument)};
		request.execute(request);
		return 0;
	}

public:
	/** @brief Runs blocking I/O with the target Windows family’s thread creation interface. */
	bool push(work_request &request) noexcept
	{
#if defined(FAST_IO_ASYNC_NT)
		using namespace ::fast_io::win32::nt;
		client_id id{};
		auto const status{RtlCreateUserThread(reinterpret_cast<void *>(static_cast<::std::intptr_t>(-1)), nullptr,
											  0, 0, 0, 0, run_work, &request, &request.wait_handle, &id)};
		if (status != 0)
		{
			return false;
		}
#else
		::std::uint_least32_t id{};
		request.wait_handle = ::fast_io::win32::CreateThread(nullptr, 0, run_work, &request, 0, &id);
		if (request.wait_handle == nullptr)
		{
			return false;
		}
#endif
		request.close_after_wait = true;
		add(request);
		return true;
	}

	/** @brief Retires signaled requests, including when the wait set exceeds the 64-object API limit. */
	void wait(request_queue &ready, bool block = true) noexcept
	{
		wait_with(ready, block, [](work_request &) noexcept {}, [](work_request const &) noexcept { return false; });
	}
};

} // namespace fast_io::async::details
