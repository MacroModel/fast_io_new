#pragma once

namespace fast_io::async::details
{

/** @brief Raises an NT completion status through the selected C++ error channel. */
[[noreturn]] inline void raise_nt_status(::std::uint_least32_t status) FAST_IO_ASYNC_THROWS
{
#if defined(__HERBCEPTIONS__)
	throw throws static_cast<::std::nt_errc>(status);
#else
	::fast_io::throw_nt_error(status);
#endif
}

/** @brief Reports whether the current handle has a known implicit, synchronous file position. */
inline bool query_nt_synchronous_mode(void *stream, bool &synchronous) noexcept
{
	using namespace ::fast_io::win32::nt;
	io_status_block info{};
	::std::uint_least32_t mode{};
	auto const result{NtQueryInformationFile(stream, &info, &mode, sizeof(mode), file_information_class::FileModeInformation)};
	synchronous = result == 0 && (mode & 0x30u) != 0;
	return result == 0;
}

/** @brief Classifies the stable completion mode of one live file object. */
enum class nt_open_mode : unsigned char
{
	unqueried,
	opaque,
	synchronous,
	overlapped
};

/** @brief Tests the file object's synchronous category without acting on a query failure. */
inline nt_open_mode classify_nt_open_mode(void *file) noexcept
{
	bool synchronous;
	if (!query_nt_synchronous_mode(file, synchronous))
		return nt_open_mode::opaque;
	return synchronous ? nt_open_mode::synchronous : nt_open_mode::overlapped;
}

/**
 * @brief Holds the event and IO_STATUS_BLOCK of a sequential native NT operation.
 * @details A private event distinguishes this request from other activity on the same handle. The public stream
 * cursor is preserved by passing the same implicit file offset as the synchronous NT stream CPO.
 */
struct nt_request : work_request
{
	void *stream;
	void const *buffer;
	::std::uint_least32_t size;
	bool write;
	::fast_io::win32::nt::io_status_block block{};
	void *event{};
	::std::uint_least32_t status{};
	nt_open_mode mode;

	/** @brief Records a stable range and caps each native submission to an ULONG byte count. */
	constexpr nt_request(void *handle, void const *first, ::std::size_t length, bool output,
						 nt_open_mode known_mode = nt_open_mode::unqueried) noexcept
		: stream(handle), buffer(first), size(static_cast<::std::uint_least32_t>(
											 length > UINT32_MAX ? UINT32_MAX : length)),
		  write(output), mode(known_mode)
	{
		execute = [](work_request &item) noexcept { static_cast<nt_request &>(item).perform_blocking(); };
	}

	nt_request(nt_request const &) = delete;

	/** @brief Closes the private event after the issuer has observed completion. */
	~nt_request()
	{
		if (event != nullptr)
		{
			::fast_io::win32::nt::NtClose(event);
		}
	}

	/**
	 * @brief Issues native I/O with an idle issuer event or creates a private event when needed.
	 * @details A synchronization event is consumed by the request's only waiter. The status block
	 * and buffer stay with the request until that wait succeeds or the I/O completes inline.
	 */
	void issue(void *idle_event = nullptr) noexcept
	{
		using namespace ::fast_io::win32::nt;
		if (mode != nt_open_mode::synchronous)
		{
			event = idle_event;
			if (event == nullptr)
			{
				status = NtCreateEvent(&event, 0x100002u, nullptr, event_type::SynchronizationEvent, 0);
				if (status != 0) [[unlikely]]
				{
					return;
				}
			}
		}
		// A pending submission may leave the status block untouched until the kernel finishes.
		block.Status = 0x103u;
		status = write ? NtWriteFile(stream, event, nullptr, nullptr, &block, buffer, size, nullptr, nullptr)
					   : NtReadFile(stream, event, nullptr, nullptr, &block, const_cast<void *>(buffer), size, nullptr, nullptr);
	}

	/** @brief Completes an operation issued on a synchronous or otherwise opaque handle. */
	void perform_blocking() noexcept
	{
		issue();
		if (status == 0x103u)
		{
			// Native synchronous opens carry SYNCHRONIZE. Opaque handles retain a private event.
			auto const waited{::fast_io::win32::nt::NtWaitForSingleObject(event == nullptr ? stream : event, 0, nullptr)};
			if (waited != 0) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			status = block.Status;
		}
	}

	/** @brief Checks the immutable open mode before issuing potentially blocking I/O on the loop. */
	bool asynchronous_handle() noexcept
	{
		if (mode == nt_open_mode::unqueried)
			mode = classify_nt_open_mode(stream);
		return mode == nt_open_mode::overlapped;
	}

	/** @brief Supplies a scheduling hint even before the kernel signals the request event. */
	bool may_have_completed() const noexcept
	{
		// The event wait still arbitrates completion and ownership. Read only the native status word here.
		return *static_cast<::std::uint_least32_t const volatile *>(&block.Status) != 0x103u;
	}
};

/** @brief Adds native asynchronous file requests to the Windows wait set. */
class nt_wait : public windows_wait
{
	static inline constexpr unsigned event_capacity{64};
	void *idle_events[event_capacity];
	unsigned idle_count{};

	/** @brief Leases one reset native event exclusively to the next overlapped request. */
	void *take_event() noexcept
	{
		return idle_count == 0 ? nullptr : idle_events[--idle_count];
	}

	/** @brief Returns a consumed event to its sole issuer or closes it if the cache is full. */
	void recycle_event(nt_request &request) noexcept
	{
		if (idle_count != event_capacity)
		{
			idle_events[idle_count++] = request.event;
		}
		else
		{
			::fast_io::win32::nt::NtClose(request.event);
		}
		request.event = nullptr;
	}

public:
	nt_wait() noexcept = default;
	nt_wait(nt_wait const &) = delete;
	nt_wait &operator=(nt_wait const &) = delete;

	/** @brief Releases only idle events when an isolated or thread-local issuer is destroyed. */
	~nt_wait()
	{
		while (idle_count != 0)
		{
			::fast_io::win32::nt::NtClose(idle_events[--idle_count]);
		}
	}

	using windows_wait::push;

	/** @brief Reclaims consumed synchronization events before resuming their requests. */
	void wait(request_queue &ready, bool block = true) noexcept
	{
		wait_with(ready, block, [this](work_request &work) noexcept {
			// The base wait set closes worker handles itself. All other entries are native requests.
			recycle_event(static_cast<nt_request &>(work));
		}, [](work_request const &work) noexcept {
			return !work.close_after_wait && static_cast<nt_request const &>(work).may_have_completed();
		});
	}

	/**
	 * @brief Submits a native request while retaining its sequential file-position contract.
	 * @details A recognized synchronous handle can complete inline during an isolated outermost wait.
	 * A batch or application-supplied pump retains the worker path for blocking requests. An overlapped
	 * handle still owns a private event until the kernel has completed the request.
	 */
	bool push(nt_request &request, [[maybe_unused]] bool isolated) noexcept
	{
		if (request.asynchronous_handle())
		{
			request.issue(take_event());
			if (request.status != 0x103u)
			{
				// Inline completion did not consume the signal through a wait.
				if (request.event != nullptr && idle_count != event_capacity &&
					::fast_io::win32::nt::NtResetEvent(request.event, nullptr) == 0)
				{
					recycle_event(request);
				}
				return false;
			}
			// The awaiter owns this event. The wait set borrows it through completion.
			request.wait_handle = request.event;
			add(request);
			return true;
		}
#if !defined(__SINGLE_THREAD__)
		if (!(isolated && request.mode == nt_open_mode::synchronous) && push(static_cast<work_request &>(request)))
		{
			return true;
		}
#endif
		request.perform_blocking();
		return false;
	}
};

} // namespace fast_io::async::details
