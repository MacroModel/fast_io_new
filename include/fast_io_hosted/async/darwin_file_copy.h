#pragma once

namespace fast_io::async::details
{

/**
 * @brief Relays a pair of regular Darwin files through one ordinary POSIX copy request in a batch.
 * @details Retaining the file positions and temporary buffer in one request avoids one scheduler
 * boundary per read and write. A distinct regular pair can finish independently of stream readiness.
 * Both borrowed descriptors remain live until publication of the committed count or native failure.
 */
class darwin_transfer_work : public work_request
{
	int input;
	int output;
	::std::size_t capacity{::fast_io::details::transmit_buffer_size_cache<1u>};
	io_context *issuer{};
	transfer_completion result{};

	/** @brief Reads blocks, completes each short write and records only the bytes accepted by the output. */
	void copy() noexcept
	{
		::fast_io::details::local_operator_new_array_ptr<::std::byte> buffer(capacity);
		::fast_io::uintfpos_transmit_reference_wrapper count{::fast_io::freestanding::addressof(result.progress.transmitted)};
		for (;;)
		{
			posix_request read{input, buffer.ptr, capacity, false};
			read.perform();
			if (read.result <= 0)
			{
				if (read.result == 0)
				{
					result.complete = true;
				}
				else
				{
					result.error = {raise_posix_error, static_cast<unsigned>(-read.result)};
				}
				return;
			}
			auto remaining{static_cast<::std::size_t>(read.result)};
			for (auto *first{buffer.ptr}; remaining != 0;)
			{
				posix_request write{output, first, remaining, true};
				write.perform();
				if (write.result <= 0)
				{
					result.error = {raise_posix_error, write.result == 0 ? static_cast<unsigned>(EIO)
						: static_cast<unsigned>(-write.result)};
					return;
				}
				auto const written{static_cast<::std::size_t>(write.result)};
				transmit_integer_add_define(count, written);
				first += written;
				remaining -= written;
			}
		}
	}

public:
	/** @brief Records two borrowed handles and defers all filesystem work until the first await. */
	darwin_transfer_work(int source, int sink) noexcept : input(source), output(sink)
	{
		execute = [](work_request &item) noexcept { static_cast<darwin_transfer_work &>(item).copy(); };
	}

	/** @brief Selects a self-contained copy or leaves both cursors to the generic device operations. */
	bool await_ready() noexcept
	{
		issuer = platform_context();
		struct ::stat source, sink;
		if (issuer == nullptr || current_loop->root_count < 2u ||
			::fstat(input, &source) != 0 || !S_ISREG(source.st_mode) ||
			::fstat(output, &sink) != 0 || !S_ISREG(sink.st_mode) ||
			(source.st_dev == sink.st_dev && source.st_ino == sink.st_ino))
		{
			return true;
		}
		auto const input_flags{::fcntl(input, F_GETFL)};
		auto const output_flags{::fcntl(output, F_GETFL)};
		// Nonblocking descriptions keep their existing readiness and retry protocol.
		if (input_flags < 0 || output_flags < 0 || ((input_flags | output_flags) & O_NONBLOCK) != 0)
		{
			return true;
		}
#if !defined(FAST_IO_BUFFER_SIZE)
		constexpr ::off_t large_source{8u * 1024u * 1024u};
		if (source.st_size >= large_source)
		{
			// Amortize native request overhead only when the observed source has a substantial unread range.
			auto const position{::lseek(input, 0, SEEK_CUR)};
			if (position >= 0 && source.st_size - position >= large_source)
			{
				capacity = 1024u * 1024u;
			}
		}
#endif
		return false;
	}

	/** @brief Publishes the whole transfer as one worker item. A refused submission preserves the cursors. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		return issuer->submit(static_cast<work_request &>(*this));
	}

	/** @brief Gives the caller the committed prefix after the corresponding completion. */
	transfer_completion await_resume() const noexcept
	{
		return result;
	}
};

} // namespace fast_io::async::details

namespace fast_io
{

/** @brief Offers one buffered file request for two raw byte observers on Darwin. */
template <::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1u && sizeof(OutputChar) == 1u)
inline auto async_transmit_until_eof_request_define(basic_posix_io_observer<InputChar> source,
												   basic_posix_io_observer<OutputChar> sink) noexcept
{
	return ::fast_io::async::details::darwin_transfer_work{source.fd, sink.fd};
}

} // namespace fast_io
