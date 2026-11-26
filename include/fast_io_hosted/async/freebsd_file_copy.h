#pragma once

namespace fast_io::async::details
{

/**
 * @brief Copies eligible FreeBSD byte streams with one range operation at a time.
 * @details Null offsets let the kernel advance both original file positions. The caller lends its current
 * positions for the duration of the request, as for an ordinary sequential transfer. A nonzero request
 * returning zero establishes EOF. An unsupported pair or a range that needs regular write semantics
 * returns an incomplete result so the generic transfer resumes with the already committed byte count.
 * The syscall number comes from the build headers and the request uses the existing libc syscall entry point.
 */
class freebsd_transfer_work : public work_request
{
	int input;
	int output;
	io_context *issuer{};
	transfer_completion result{};

	/** @brief Completes kernel copies and preserves any prefix before fallback or error delivery. */
	void copy() noexcept
	{
		struct ::stat sink_status;
		if (::fstat(output, &sink_status) != 0 || !S_ISREG(sink_status.st_mode))
		{
			return;
		}
		auto const input_flags{::fcntl(input, F_GETFL)};
		auto const output_flags{::fcntl(output, F_GETFL)};
		// Preserve special open-description behavior through the ordinary device protocol.
		if (input_flags < 0 || output_flags < 0 || (input_flags & (O_DIRECT | O_NONBLOCK)) != 0 ||
			(output_flags & (O_APPEND | O_DIRECT | O_NONBLOCK | O_SYNC | O_DSYNC)) != 0)
		{
			return;
		}
		auto source_position{::lseek(input, 0, SEEK_CUR)};
		auto sink_position{::lseek(output, 0, SEEK_CUR)};
		if (source_position < 0 || sink_position < 0)
		{
			return;
		}
		::fast_io::uintfpos_transmit_reference_wrapper count{::fast_io::freestanding::addressof(result.progress.transmitted)};
		constexpr auto last_offset{::std::numeric_limits<::off_t>::max()};
		// The public syscall trampoline returns int, even when the kernel operation returns ssize_t.
		constexpr auto maximum{static_cast<::std::uintmax_t>(::std::numeric_limits<int>::max())};
		// Keep at least one existing byte outside the kernel's range to prevent its eager tail truncation.
		auto const sink_limit{sink_position < sink_status.st_size ? sink_status.st_size - 1 : last_offset};
		for (;;)
		{
			auto const source_room{static_cast<::std::uintmax_t>(last_offset - source_position)};
			auto const sink_room{static_cast<::std::uintmax_t>(sink_limit - sink_position)};
			auto const room{source_room < sink_room ? source_room : sink_room};
			auto const size{static_cast<::std::size_t>(room < maximum ? room : maximum)};
			if (size == 0)
			{
				return;
			}
			// The FreeBSD kernel validates the pair, including regular-file and append-mode requirements.
			auto const copied{static_cast<::ssize_t>(::syscall(SYS_copy_file_range, input, static_cast<::off_t *>(nullptr),
																	output, static_cast<::off_t *>(nullptr), size, ::std::uintptr_t{}))};
			if (copied > 0)
			{
				source_position += static_cast<::off_t>(copied);
				sink_position += static_cast<::off_t>(copied);
				transmit_integer_add_define(count, static_cast<::std::size_t>(copied));
				continue;
			}
			if (copied == 0)
			{
				result.complete = true;
				return;
			}
			auto const failure{errno};
			if (failure == EINTR)
			{
				continue;
			}
			// On terminal filesystem errors FreeBSD can still publish a prefix through the output cursor.
			auto const position{::lseek(output, 0, SEEK_CUR)};
			if (position > sink_position)
			{
				transmit_integer_add_define(count, static_cast<::std::size_t>(position - sink_position));
			}
			if (failure != ENOSYS && failure != EBADF && failure != EINVAL && failure != EISDIR && failure != EXDEV &&
				failure != EOPNOTSUPP && failure != ENOTCAPABLE)
			{
				result.error = {raise_posix_error, static_cast<unsigned>(failure)};
			}
			return;
		}
	}

public:
	/** @brief Borrows both endpoint handles without advancing either cursor. */
	freebsd_transfer_work(int source, int sink) noexcept : input(source), output(sink)
	{
		execute = [](work_request &item) noexcept { static_cast<freebsd_transfer_work &>(item).copy(); };
	}

	/** @brief Runs sole outermost jobs on the issuer and offers concurrent jobs to its worker queue. */
	bool await_ready() noexcept
	{
		issuer = platform_context();
		if (issuer == nullptr)
		{
			return true;
		}
		if (current_loop->standalone_root && current_loop->ready.empty())
		{
			// The outermost waiter can make progress without a worker handoff.
			copy();
			return true;
		}
		return current_loop->root_count < 2u;
	}

	/** @brief Publishes one worker request for this transfer. A declined submission consumes no data. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		return issuer->submit(static_cast<work_request &>(*this));
	}

	/** @brief Returns the committed prefix after the issuer observes worker completion. */
	transfer_completion await_resume() const noexcept
	{
		return result;
	}
};

} // namespace fast_io::async::details

namespace fast_io
{

/** @brief Offers a frame-owned range copy for byte-domain native POSIX observers on FreeBSD. */
template <::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1u && sizeof(OutputChar) == 1u)
inline auto async_transmit_until_eof_request_define(basic_posix_io_observer<InputChar> source,
												   basic_posix_io_observer<OutputChar> sink) noexcept
{
	return ::fast_io::async::details::freebsd_transfer_work{source.fd, sink.fd};
}

} // namespace fast_io
