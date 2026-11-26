#pragma once

namespace fast_io::async::details
{
/** @brief Writes a complete range and retains native failure values for the consuming coroutine. */
template <typename Output>
inline task<native_error> drain_splice_bytes(Output &output, ::std::byte const *data, ::std::size_t size) FAST_IO_ASYNC_THROWS
{
	while (size != 0)
	{
		auto const [count, error]{co_await async_write_some_define(output, data, size)};
		if (error.raise != nullptr) [[unlikely]]
		{
			co_return error;
		}
		if (count == 0 || size < count) [[unlikely]]
		{
			co_return native_error{raise_invalid_progress, 0u};
		}
		data += count;
		size -= count;
	}
	co_return native_error{};
}



/**
 * @brief Owns a transfer's private pipe and the storage needed by its linked requests.
 * @details Whole-file workers need only the pipe descriptors. A linked submission borrows the same
 * request storage for each successive batch. Resumption after its final CQE ends the kernel's borrow.
 */
struct splice_pipe
{
	io_context *owner;
	int ends[2]{-1, -1};
	unsigned capacity{};
	bool empty{};

	using request_allocator = ::fast_io::native_typed_global_allocator<posix_request>;
	static inline constexpr unsigned request_count{8};
	posix_request *requests{};

	/** @brief Associates a newly leased pipe with the issuing context that owns its cache. */
	explicit constexpr splice_pipe(io_context &issuer) noexcept : owner{__builtin_addressof(issuer)}
	{}

	splice_pipe(splice_pipe const &) = delete;
	splice_pipe &operator=(splice_pipe const &) = delete;

	/** @brief Leases storage once per transfer, when its first linked submission is constructed. */
	posix_request *request_storage() noexcept
	{
		if (requests == nullptr)
		{
			requests = request_allocator::allocate(request_count);
		}
		return requests;
	}

	/** @brief Releases the slots and returns a proven-empty pipe after every borrowing SQE has completed. */
	~splice_pipe()
	{
		if (requests != nullptr)
		{
			static_assert(::std::is_trivially_destructible_v<posix_request>);
			request_allocator::deallocate_n(requests, request_count);
		}
		if (ends[0] >= 0)
		{
			owner->release_pipe(ends, capacity, empty);
		}
	}
};

/** @brief Returns a splice request whose two descriptors use their shared current positions. */
struct splice_awaiter : posix_awaiter
{
	/** @brief Selects a kernel-to-kernel transfer instead of a userspace byte range. */
	constexpr splice_awaiter(int input, int output, ::std::size_t bytes) noexcept
		: posix_awaiter(output, nullptr, bytes, true)
	{
		splice_source = input;
	}
};

/** @brief Carries the bytes committed by complete pairs and the two results of the last executed pair. */
struct splice_completion
{
	::std::size_t committed{};
	byte_completion read{}, written{};
};

#if defined(FAST_IO_ASYNC_WORKERS)
/**
 * @brief Owns one worker submission for a regular-file transfer at the two current file positions.
 * @details A private pipe bounds the active fill/drain window. A null pipe offers the complete range
 * to the filesystem in one call. The issuer alone borrows or returns pipes
 * and resumes the coroutine. A worker can report bytes still in its pipe to the ordinary drain code.
 */
struct regular_file_work : work_request
{
	io_context &issuer;
	int input, output;
	splice_pipe *pipe;
	::fast_io::transmit_result &progress;
	splice_completion partial{};
	bool submitted{}, complete{};
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
	::std::size_t roots{current_loop->root_count};
#endif

	/** @brief Selects whole-range offload with a null pipe or a fill/drain loop with an owned pipe. */
	regular_file_work(io_context &context, int source, int sink, splice_pipe *channel, ::fast_io::transmit_result &count) noexcept
		: issuer(context), input(source), output(sink), pipe(channel), progress(count)
	{
		execute = [](work_request &item) noexcept { static_cast<regular_file_work &>(item).copy(); };
	}

	/** @brief Copies a nonempty eligible input until a zero read, an unsupported operation or a native error. */
	void copy() noexcept
	{
		// Only the issuing coroutine reads the count after publication. Keep the live value and
		// immutable descriptor numbers in registers across the syscall's memory clobber.
		auto total{progress.transmitted};
		::fast_io::uintfpos_transmit_reference_wrapper count{__builtin_addressof(total)};
		auto const source{input}, sink{output};
#if defined(FAST_IO_ASYNC_FILE_PLACEMENT)
		// Both worker strategies inherit the same placement. Select it once before dispatch.
		workers::place_file(roots);
#endif
#if defined(__NR_copy_file_range)
		if (pipe == nullptr)
		{
			// Older and virtual filesystems can report an initial zero without actually reaching EOF.
			bool advanced{};
			for (;;)
			{
				auto n{::fast_io::system_call<__NR_copy_file_range, ::std::ptrdiff_t>(
					source, nullptr, sink, nullptr, 0x7ffff000u, 0u)};
				if (n > 0)
				{
					transmit_integer_add_define(count, static_cast<::std::size_t>(n));
					advanced = true;
					continue;
				}
				if (n == -EINTR) { continue; }
				complete = n == 0 && advanced;
				if (n < 0 && n != -ENOSYS && n != -EOPNOTSUPP && n != -EXDEV && n != -EINVAL && n != -EBADF && n != -EPERM)
				{
					partial.read.error = {raise_posix_error, static_cast<::std::uint_least32_t>(-n)};
				}
				progress.transmitted = total;
				return;
			}
		}
#endif
		auto const read_end{pipe->ends[0]}, write_end{pipe->ends[1]};
		auto capacity{pipe->capacity};
		auto move = [](int from, int to, ::std::size_t bytes) noexcept {
			::std::ptrdiff_t n;
			do
			{
				n = ::fast_io::system_call<__NR_splice, ::std::ptrdiff_t>(from, nullptr, to, nullptr, bytes, 0u);
			} while (n == -EINTR);
			return n;
		};
		for (;;)
		{
			auto n{move(source, write_end, capacity)};
			if (n <= 0)
			{
				if (n < 0) { partial.read.error = {raise_posix_error, static_cast<::std::uint_least32_t>(-n)}; }
				complete = n == 0;
				pipe->empty = true;
				progress.transmitted = total;
				return;
			}
			while (n)
			{
				auto sent{move(read_end, sink, static_cast<::std::size_t>(n))};
				if (sent <= 0)
				{
					// Only the undelivered suffix is still in the pipe. Completed writes are already counted.
					partial.read.transferred = static_cast<::std::size_t>(n);
					partial.written.error = sent < 0 ? native_error{raise_posix_error, static_cast<::std::uint_least32_t>(-sent)}
													: native_error{raise_invalid_progress, 0u};
					progress.transmitted = total;
					return;
				}
				transmit_integer_add_define(count, static_cast<::std::size_t>(sent));
				n -= sent;
			}
		}
	}

	/** @brief Gives the issuer a chance to keep processing unrelated descriptors while the filesystem runs. */
	constexpr bool await_ready() const noexcept { return false; }

	/** @brief Leaves a declined worker submission untouched for the native ring. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		submitted = issuer.submit(*this);
		return submitted;
	}

	/** @brief The issuer acquires all published progress when it removes this item from the completion queue. */
	constexpr void await_resume() const noexcept {}
};
#endif


/**
 * @brief Chains bounded fill/drain pairs through one private pipe and one pair of implicit file positions.
 * @details Linux soft links cancel every later SQE after a short or failed splice. Completed prefix pairs are
 * fully committed. Only the first incomplete pair can leave pages in the pipe. Four pairs use at most eight
 * SQ slots, allowing eight concurrent transfers to make progress on the 64-entry issuer.
 */
class splice_batch
{
	static inline constexpr unsigned max_pairs{splice_pipe::request_count / 2u};
	posix_request *requests;
	splice_submission submission;
	io_context *context;

	/**
	 * @brief Constructs one active SQE record and binds its descriptors and expected length.
	 * @details Clang otherwise expands this initialization up to eight times inside the coroutine's
	 * resume routine. Keeping one copy bounds code size on a path that already enters the kernel.
	 */
#if defined(__clang__) && __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	void set(int input, int output, unsigned length) noexcept
	{
		// Only active SQEs need live request objects. The previous batch has received its final CQE.
		auto *slot{requests + submission.count++};
		auto &entry{*::new (static_cast<void *>(slot)) posix_request(output, nullptr, length, true)};
		entry.splice_source = input;
		entry.group = &submission;
		entry.result = -ECANCELED;
	}

	/** @brief Decodes a signed CQE without raising until the operation's result is consumed. */
	static constexpr byte_completion completion(::std::ptrdiff_t value) noexcept
	{
		if (value < 0) [[unlikely]]
		{
			return {0, {raise_posix_error, static_cast<::std::uint_least32_t>(-value)}};
		}
		return {static_cast<::std::size_t>(value), {}};
	}

public:
	/** @brief Describes as much of a positive size hint as the bounded chain and actual pipe capacity allow. */
	splice_batch(io_context &issuer, int input, int output, splice_pipe &pipe, ::std::uint_least64_t hint) noexcept
		: requests{pipe.request_storage()}, submission{requests}, context(&issuer)
	{
		for (unsigned i{}; i != max_pairs && hint != 0; ++i)
		{
			auto const length{hint < pipe.capacity ? static_cast<unsigned>(hint) : pipe.capacity};
			set(input, pipe.ends[1], length);
			set(pipe.ends[0], output, length);
			hint -= length;
		}
	}

	// Each leased entry points back into this awaiter until the last CQE.
	splice_batch(splice_batch const &) = delete;
	splice_batch &operator=(splice_batch const &) = delete;

	/** @brief Requires actual kernel progress before reporting a byte count. */
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	/** @brief Suspends once for all requests in this bounded sequence. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		return context->submit_batch(submission, handle);
	}

	/** @brief Locates the first incomplete pair after every kernel reference to the frame has expired. */
	splice_completion await_resume() const noexcept
	{
		::std::size_t committed{};
		for (unsigned i{};; i += 2u)
		{
			auto const &read{requests[i]}, &written{requests[i + 1u]};
			if (i + 2u == submission.count || read.result != static_cast<::std::ptrdiff_t>(read.size) ||
				written.result != static_cast<::std::ptrdiff_t>(written.size))
			{
				return {committed, completion(read.result), completion(written.result)};
			}
			committed += written.size;
		}
	}
};

/** @brief Determines whether an optional splice operation can fall back to ordinary stream I/O. */
inline constexpr bool unsupported_splice(native_error error) noexcept
{
	return error.code == EINVAL || error.code == ENOSYS || error.code == EOPNOTSUPP ||
		   error.code == EXDEV || error.code == EPERM;
}

/**
 * @brief Returns a regular source's size hint after excluding aliased or unsuitable destinations.
 * @details The transfer proves EOF with a read. The size only helps choose a submission shape and never
 * terminates the copy, including if another writer extends the source during a transfer.
 */
inline ::std::uint_least64_t splice_size_hint(int input, int output, bool &same_device) noexcept
{
#if defined(__x86_64__) && !defined(__ILP32__) && defined(__NR_fstat)
	// The original LP64 status ABI supplies identity and size in 144 bytes. A restricted descriptor may
	// still permit statx, so a rejected system call falls through to the independent metadata interface.
	::fast_io::details::linux_x86_64_stat from, to;
	if (::fast_io::details::linux_fstat(input, from) == 0 && ::fast_io::details::linux_fstat(output, to) == 0)
	{
		same_device = from.device == to.device;
		return ::fast_io::details::linux_stat_is_regular(from) && from.st_size > 0 &&
			   ::fast_io::details::linux_stat_is_regular(to) && (!same_device || from.inode != to.inode)
				   ? static_cast<::std::uint_least64_t>(from.st_size)
				   : 0;
	}
#endif
	using stat_type = ::fast_io::details::linux_struct_statx;
	static_assert(sizeof(stat_type) == 256u);
	auto regular = [](int fd, stat_type &info) noexcept {
		using mask_type = ::fast_io::linux_statx_mask;
		constexpr auto mask{static_cast<unsigned>(mask_type::statx_ino | mask_type::statx_size | mask_type::statx_type)};
		constexpr unsigned empty_path{0x1000};
		return ::fast_io::system_call<__NR_statx, int>(fd, "", empty_path, mask, &info) == 0 &&
			   (info.stx_mask & mask) == mask && S_ISREG(info.stx_mode);
	};
	stat_type source, sink;
	if (regular(input, source) && source.stx_size != 0 && regular(output, sink))
	{
		same_device = source.stx_dev_major == sink.stx_dev_major && source.stx_dev_minor == sink.stx_dev_minor;
		if (!same_device || source.stx_ino != sink.stx_ino)
		{
			return source.stx_size;
		}
	}
	return 0;
}

/**
 * @brief Chooses whole-range offload, a worker-owned fill/drain loop or linked kernel completions.
 * @details Direction, short completions and implicit offsets are preserved. When the destination declines splice
 * after a successful source read, the private pipe is drained with ordinary native I/O before the generic loop
 * resumes. Each operation owns its own pipe, so concurrent transfers never mix pages.
 */
inline task<transfer_completion> splice_files(int input, int output) FAST_IO_ASYNC_THROWS
{
	transfer_completion result{};
	auto *context{platform_context()};
	if (context == nullptr || !context->has_ring())
	{
		co_return result;
	}
	bool same_device{};
	auto hint{splice_size_hint(input, output, same_device)};
	if (hint == 0)
	{
		co_return result;
	}
#if defined(FAST_IO_ASYNC_WORKERS)
	bool use_pipe_worker{same_device && static_cast<bool>(current_loop->standalone_root)};
#if defined(__NR_copy_file_range)
	// A small regular file fits a short kernel copy. One worker can prove EOF before handing it back.
	if (same_device && (!use_pipe_worker || hint <= 64u * 1024u))
	{
		// The kernel owns both current positions throughout the whole-range worker submission.
		regular_file_work work{*context, input, output, nullptr, result.progress};
		co_await work;
		if (work.complete || work.partial.read.error.raise != nullptr)
		{
			result.complete = work.complete;
			result.error = work.partial.read.error;
			co_return result;
		}
		// A declined method can follow a positive prefix. The next backend always uses the live offsets.
	}
#endif
#endif
	splice_pipe pipe{*context};
	pipe.capacity = context->acquire_pipe(pipe.ends);
	if (pipe.capacity == 0)
	{
		co_return result;
	}
	::fast_io::uintfpos_transmit_reference_wrapper progress{::fast_io::freestanding::addressof(result.progress.transmitted)};
	splice_completion completion{};
#if defined(FAST_IO_ASYNC_WORKERS)
	bool have_completion{};
	if (use_pipe_worker && hint >= pipe.capacity)
	{
		regular_file_work work{*context, input, output, &pipe, result.progress};
		co_await work;
		if (work.complete)
		{
			result.complete = true;
			co_return result;
		}
		have_completion = work.submitted;
		completion = work.partial;
	}
#endif
	for (;;)
	{
#if defined(FAST_IO_ASYNC_WORKERS)
		if (have_completion)
		{
			// A worker may have consumed an input suffix that the ring still needs to deliver.
			have_completion = false;
		}
		else
#endif
		if (hint == 0)
		{
			completion = {};
			// Probe a predicted EOF without submitting a drain for an empty pipe. A concurrent append
			// is observable and goes through the same checked drain as every other successful read.
			completion.read = co_await splice_awaiter{input, pipe.ends[1], pipe.capacity};
			hint = ::std::numeric_limits<::std::uint_least64_t>::max();
		}
		else
		{
			completion = co_await splice_batch{*context, input, output, pipe, hint};
		}
		auto const [committed, received, written]{completion};
		transmit_integer_add_define(progress, committed);
		hint = hint > committed ? hint - committed : 0;
		if (received.error.raise != nullptr)
		{
			if (!unsupported_splice(received.error))
			{
				result.error = received.error;
			}
			co_return result;
		}
		if (received.transferred == 0)
		{
			pipe.empty = true;
			result.complete = true;
			co_return result;
		}
		hint = hint > received.transferred ? hint - received.transferred : 0;
		if (written.transferred > received.transferred) [[unlikely]]
		{
			result.error = {raise_invalid_progress, 0u};
			co_return result;
		}
		transmit_integer_add_define(progress, written.transferred);
		auto remaining{received.transferred - written.transferred};
		auto drain_error{written.error};
		if (drain_error.code == ECANCELED || drain_error.code == EAGAIN || drain_error.code == EINTR)
		{
			// A short fill may cancel the linked SQE. Only bytes actually held in the pipe remain to be written.
			drain_error = {};
		}
		while (remaining != 0)
		{
			auto sent{byte_completion{0, drain_error}};
			if (sent.error.raise == nullptr)
			{
				sent = co_await splice_awaiter{pipe.ends[0], output, remaining};
			}
			if (sent.error.raise != nullptr)
			{
				if (!unsupported_splice(sent.error))
				{
					result.error = sent.error;
					co_return result;
				}
				// Input has advanced. Forward every byte still in the pipe before switching strategies.
				::fast_io::details::local_operator_new_array_ptr<::std::byte> buffer(remaining);
				::fast_io::posix_io_observer sink{output};
				while (remaining != 0)
				{
					auto drained{co_await posix_awaiter{pipe.ends[0], buffer.ptr, remaining, false}};
					if (drained.error.raise != nullptr || drained.transferred == 0) [[unlikely]]
					{
						result.error = drained.error.raise != nullptr ? drained.error : native_error{raise_invalid_progress, 0u};
						co_return result;
					}
					result.error = co_await drain_splice_bytes(sink, buffer.ptr, drained.transferred);
					if (result.error.raise != nullptr)
					{
						co_return result;
					}
					transmit_integer_add_define(progress, drained.transferred);
					remaining -= drained.transferred;
				}
				co_return result;
			}
			if (sent.transferred == 0 || sent.transferred > remaining) [[unlikely]]
			{
				result.error = {raise_invalid_progress, 0u};
				co_return result;
			}
			remaining -= sent.transferred;
			transmit_integer_add_define(progress, sent.transferred);
		}
	}
}

} // namespace fast_io::async::details
