#pragma once

namespace fast_io::async::details
{

/**
 * @brief Owns a small single-issuer io_uring using the Linux UAPI.
 * @details The caller submits only sequential operations with the shared open-file offset.
 * Release/acquire orders the mapped entries against the kernel's head and tail updates.
 * Requests and their buffers remain owned by coroutine frames until their CQEs are consumed.
 */
class uring
{
	static inline constexpr unsigned queue_depth{64};
	int fd{-1};
	void *rings{};
	::std::size_t rings_size{};
	::io_uring_sqe *entries{};
	::std::size_t entries_size{};
	unsigned *sq_head{}, *sq_tail{}, *sq_indices{};
	unsigned *cq_head{}, *cq_tail{};
	::io_uring_cqe *completions{};
	unsigned sq_mask{}, cq_mask{}, tail{}, in_flight{};
	bool initialized{}, disabled{}, has_admission{};
	request_queue pending{}, rejected{};

	/** @brief Projects a validated Linux ring offset into its mapping. */
	template <typename T>
	static T *at(void *base, unsigned offset) noexcept
	{
		return reinterpret_cast<T *>(static_cast<::std::byte *>(base) + offset);
	}

	/** @brief Maps a queue region with the same lifetime as the ring descriptor. */
	static void *map(int descriptor, ::std::size_t size, ::std::int_least64_t offset) noexcept
	{
#if defined(__NR_mmap) && !defined(__NR_mmap2) && !defined(__s390__) && !defined(__s390x__)
		auto const result{::fast_io::system_call<__NR_mmap, ::std::intptr_t>(nullptr, size, PROT_READ | PROT_WRITE,
																			 MAP_SHARED, descriptor, static_cast<::std::uintptr_t>(offset))};
		return static_cast<::std::uintptr_t>(result) >= static_cast<::std::uintptr_t>(-4095) ? nullptr : reinterpret_cast<void *>(static_cast<::std::uintptr_t>(result));
#else
		// libc supplies the architecture-specific mmap2 unit or legacy argument block.
		auto const mapping{::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
								  descriptor, static_cast<::off_t>(offset))};
		return mapping == MAP_FAILED ? nullptr : mapping;
#endif
	}

	/** @brief Releases partially or fully initialized ring resources. */
	void close() noexcept
	{
		if (entries != nullptr)
		{
			::fast_io::details::sys_munmap_nothrow(entries, entries_size);
			entries = nullptr;
		}
		if (rings != nullptr)
		{
			::fast_io::details::sys_munmap_nothrow(rings, rings_size);
			rings = nullptr;
		}
		if (fd != -1)
		{
			::fast_io::system_call<__NR_close, int>(fd);
			fd = -1;
		}
	}

	/** @brief Fills one SQE for a bounded read or write at the current file offset. */
	void stage(posix_request &request, [[maybe_unused]] bool link_next = false, [[maybe_unused]] bool pipe_drain = false) noexcept
	{
		auto const index{tail++ & sq_mask};
		::io_uring_sqe entry{};
		entry.opcode = request.write ? IORING_OP_WRITE : IORING_OP_READ;
#if defined(FAST_IO_ASYNC_SPLICE)
		if (request.splice_source >= 0)
		{
			// The two implicit file positions advance by the actual CQE byte count.
			entry.opcode = IORING_OP_SPLICE;
			if (link_next)
			{
				entry.flags = IOSQE_IO_LINK;
			}
			if (pipe_drain)
			{
				// At EOF the pipe can stay empty. A linked drain must never wait for an unissued producer.
				entry.splice_flags = SPLICE_F_NONBLOCK;
			}
			entry.splice_fd_in = request.splice_source;
			entry.splice_off_in = static_cast<::std::uint_least64_t>(-1);
		}
		else
#endif
		{
			entry.addr = reinterpret_cast<::std::uintptr_t>(request.buffer);
		}
		entry.fd = request.fd;
		entry.off = static_cast<::std::uint_least64_t>(-1);
		// Bound the length to the signed completion domain.
		entry.len = static_cast<unsigned>(request.size > 0x7fff'ffffu ? 0x7fff'ffffu : request.size);
		entry.user_data = reinterpret_cast<::std::uintptr_t>(::fast_io::freestanding::addressof(request));
		entries[index] = entry;
		has_admission = has_admission || request.admission!=nullptr;
		if (sq_indices != nullptr)
		{
			sq_indices[index] = index;
		}
		++in_flight;
	}

	/** @brief Reserves the complete linked sequence before publishing any of its SQEs. */
	bool stage_if_space(posix_request &request) noexcept
	{
#if defined(FAST_IO_ASYNC_SPLICE)
		if (auto *group = request.group)
		{
			if (group->count > (sq_mask + 1u) - in_flight)
			{
				return false;
			}
			for (unsigned i{}; i != group->count; ++i)
			{
				// Every fill is followed by its own nonblocking drain. A short operation cancels all successors.
				stage(group->entries[i], i + 1u != group->count, (i & 1u) != 0);
			}
			return true;
		}
#endif
		if (in_flight <= sq_mask)
		{
			stage(request);
			return true;
		}
		return false;
	}

	/** @brief Withdraws unpublished SQEs after an unavailable enter syscall while retaining all accepted buffers. */
	void disable_submissions() noexcept
	{
		disabled = true;
		auto const consumed{::std::atomic_ref<unsigned>(*sq_head).load(::std::memory_order_acquire)};
		// This issuer never uses SQPOLL. The kernel cannot start consuming more entries between enter calls.
		for (auto index{consumed}; index != tail; ++index)
		{
			auto &request{*reinterpret_cast<posix_request *>(static_cast<::std::uintptr_t>(entries[index & sq_mask].user_data))};
			rejected.push(request);
		}
		in_flight -= tail - consumed;
		tail = consumed;
		::std::atomic_ref<unsigned>(*sq_tail).store(tail, ::std::memory_order_release);
		while (!pending.empty())
		{
			auto &request{static_cast<posix_request &>(pending.pop())};
			rejected.push(request);
#if defined(FAST_IO_ASYNC_SPLICE)
			if (auto *group = request.group)
			{
				// The head is already queued. Give each unpublished successor its own cancellation result.
				for (unsigned i{1}; i != group->count; ++i)
				{
					rejected.push(group->entries[i]);
				}
			}
#endif
		}
	}

	/** @brief Publishes a request only after every SQE sharing its coroutine frame is complete. */
	template <typename Complete>
	static void finish(posix_request &request, int status, Complete &complete) noexcept
	{
		request.result = status;
#if defined(FAST_IO_ASYNC_SPLICE)
		if (auto *group = request.group)
		{
			if (--group->remaining == 0)
			{
				complete(*group->entries);
			}
			return;
		}
#endif
		complete(request);
	}

public:
	uring() noexcept = default;
	uring(uring const &) = delete;
	uring &operator=(uring const &) = delete;

	/** @brief Releases the mappings after all submitted requests have completed. */
	~uring()
	{
		close();
	}

	/** @brief Initializes the ring once, returning false if the running kernel cannot be used. */
	bool available() noexcept
	{
		if (initialized) [[likely]]
		{
			return fd != -1 && !disabled;
		}
		initialized = true;
		::io_uring_params params{};
#ifdef IORING_SETUP_NO_SQARRAY
		// Recent kernels can use the SQE array directly.
		params.flags = IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_NO_SQARRAY;
		fd = ::fast_io::system_call<__NR_io_uring_setup, int>(queue_depth, &params);
		if (fd == -EINVAL)
#endif
		{
			// The older UAPI includes a separate array of SQE indices.
			params = {};
			fd = ::fast_io::system_call<__NR_io_uring_setup, int>(queue_depth, &params);
		}
		if (fd < 0)
		{
			fd = -1;
			return false;
		}
		constexpr unsigned required_features{IORING_FEAT_SINGLE_MMAP | IORING_FEAT_NODROP | IORING_FEAT_RW_CUR_POS};
		if ((params.features & required_features) != required_features)
		{
			close();
			return false;
		}

		auto const sq_size{params.sq_off.array + params.sq_entries * sizeof(unsigned)};
		auto const cq_size{params.cq_off.cqes + params.cq_entries * sizeof(::io_uring_cqe)};
		rings_size = sq_size > cq_size ? sq_size : cq_size;
		entries_size = params.sq_entries * sizeof(::io_uring_sqe);
		rings = map(fd, rings_size, IORING_OFF_SQ_RING);
		entries = static_cast<::io_uring_sqe *>(map(fd, entries_size, IORING_OFF_SQES));
		if (rings == nullptr || entries == nullptr)
		{
			// Release a successful first mapping even if the second mapping failed.
			close();
			return false;
		}

		sq_head = at<unsigned>(rings, params.sq_off.head);
		sq_tail = at<unsigned>(rings, params.sq_off.tail);
#ifdef IORING_SETUP_NO_SQARRAY
		if ((params.flags & IORING_SETUP_NO_SQARRAY) == 0)
#endif
		{
			sq_indices = at<unsigned>(rings, params.sq_off.array);
		}
		cq_head = at<unsigned>(rings, params.cq_off.head);
		cq_tail = at<unsigned>(rings, params.cq_off.tail);
		completions = at<::io_uring_cqe>(rings, params.cq_off.cqes);
		sq_mask = *at<unsigned>(rings, params.sq_off.ring_mask);
		cq_mask = *at<unsigned>(rings, params.cq_off.ring_mask);
		return true;
	}

	/** @brief Queues a request for the next batch submission. */
	void push(posix_request &request) noexcept
	{
		// Bounding outstanding work by the SQ size also bounds CQ occupancy.
		if (!pending.empty() || !stage_if_space(request))
		{
			pending.push(request);
		}
	}

	/** @brief Reports whether the scheduler still owns kernel requests or queued submissions. */
	bool busy() const noexcept
	{
		return in_flight != 0 || !rejected.empty();
	}

	/** @brief Returns the descriptor which becomes readable when a CQE is available. */
	int descriptor() const noexcept
	{
		return fd;
	}

	/** @brief Submits one batch and optionally waits for the first completion in the same syscall. */
	void enter(bool wait) noexcept
	{
		if (disabled)
		{
			if (wait && rejected.empty() && in_flight != 0)
			{
				// Previously accepted I/O may still own buffers. Wait for its CQEs with the ring descriptor.
				::pollfd event{fd, POLLIN, 0};
				int status;
				do
				{
					status = linux_poll_descriptors(&event, 1, true);
				} while (status == -EINTR);
				if (status < 0) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
			}
			return;
		}
		for (;;)
		{
			// A signal may interrupt enter after the kernel consumed some entries.
			auto const submitted{::std::atomic_ref<unsigned>(*sq_head).load(::std::memory_order_acquire)};
			auto const count{tail - submitted};
			if (count == 0 && !wait)
			{
				return;
			}
			::std::atomic_ref<unsigned>(*sq_tail).store(tail, ::std::memory_order_release);
			unsigned minimum{static_cast<unsigned>(wait)};
#if defined(FAST_IO_ASYNC_SPLICE)
			// Wait for a whole new chain only when it is the issuer's sole possible continuation.
			// Linux returns immediately on partial submission. Subsequent enters then wait for one CQE.
			if (wait && count > 1 && in_flight == count)
			{
				auto const &first{entries[submitted & sq_mask]};
				auto *request{reinterpret_cast<posix_request *>(static_cast<::std::uintptr_t>(first.user_data))};
				if (request->group != nullptr && request->group->count == count)
				{
					minimum = count;
				}
			}
#endif
			// A subscribed issuer first admits SQEs without waiting for a CQE. The actual SQ head is the receipt.
			bool const admit{has_admission && count && current_loop && current_loop->admission_waiters};
			if (admit) minimum=0;
			auto const result{::fast_io::system_call<__NR_io_uring_enter, int>(
				fd, count, minimum, wait && !admit ? IORING_ENTER_GETEVENTS : 0u, nullptr, 0u)};
			if (has_admission && count)
			{
				// A later subscriber can depend on an already accepted, still backpressured request.
				// Record the consumed SQEs even when no waiter required a submission-only enter.
				auto const accepted{::std::atomic_ref<unsigned>(*sq_head).load(::std::memory_order_acquire)};
				for (auto pos{submitted};pos!=accepted;++pos)
				{
					auto &entry{entries[pos & sq_mask]};
					auto *request{reinterpret_cast<posix_request *>(static_cast<::std::uintptr_t>(entry.user_data))};
					request->admitted_on_issuer(admit);
				}
				has_admission=accepted!=tail;
			}
			if (result >= 0)
			{
				return;
			}
			if (result == -EBUSY || result == -EAGAIN)
			{
				// The next pass drains CQ pressure before retrying submission.
				return;
			}
			if (result != -EINTR)
			{
				disable_submissions();
				return;
			}
		}
	}

	/** @brief Retires a batch of CQEs before any completion can resume user code. */
	template <typename Complete>
	void reap(Complete complete) noexcept
	{
		while (!rejected.empty())
		{
			finish(static_cast<posix_request &>(rejected.pop()), -EOPNOTSUPP, complete);
		}
		auto const head{::std::atomic_ref<unsigned>(*cq_head).load(::std::memory_order_relaxed)};
		auto const end{::std::atomic_ref<unsigned>(*cq_tail).load(::std::memory_order_acquire)};
		if (head == end)
		{
			return;
		}
		for (auto index{head}; index != end; ++index)
		{
			auto const &cqe{completions[index & cq_mask]};
			auto &request{*reinterpret_cast<posix_request *>(static_cast<::std::uintptr_t>(cqe.user_data))};
			finish(request, cqe.res, complete);
		}
		in_flight -= end - head;
		::std::atomic_ref<unsigned>(*cq_head).store(end, ::std::memory_order_release);
		while (!pending.empty() && stage_if_space(static_cast<posix_request &>(pending.front())))
		{
			// Reuse SQ slots before returning control to the continuations.
			pending.pop();
		}
	}
};

} // namespace fast_io::async::details
