#pragma once

namespace fast_io::async::details
{

/**
 * @brief Owns one worker request for a pair of synchronous NT byte streams.
 * @details The immutable open modes prove that successive reads and writes use the same implicit
 * cursors as the ordinary sequential operations. The coroutine retains both endpoints until the
 * worker exits. Overlapped or opaque handles decline without consuming input and continue through
 * the existing per-operation event path. The request counts the bytes committed even on failure.
 */
class nt_transfer_work : public work_request
{
	void *input;
	void *output;
	io_context *issuer{};
	transfer_completion result{};

	/** @brief Completes short NT reads and writes inside one worker, then publishes EOF or the failing status. */
	void copy() noexcept
	{
		constexpr auto capacity{::fast_io::details::transmit_buffer_size_cache<1u>};
		::fast_io::details::local_operator_new_array_ptr<::std::byte> buffer(capacity);
		::fast_io::uintfpos_transmit_reference_wrapper count{::fast_io::freestanding::addressof(result.progress.transmitted)};
		for (;;)
		{
			nt_awaiter read{input, buffer.ptr, capacity, false, nt_open_mode::synchronous};
			read.perform_blocking();
			auto [received, error]{read.await_resume()};
			if (error.raise != nullptr || received > capacity) [[unlikely]]
			{
				result.error = error.raise == nullptr ? native_error{raise_invalid_progress, 0u} : error;
				return;
			}
			if (received == 0)
			{
				result.complete = true;
				return;
			}
			for (auto *first{buffer.ptr}; received != 0;)
			{
				nt_awaiter write{output, first, received, true, nt_open_mode::synchronous};
				write.perform_blocking();
				auto [sent, failure]{write.await_resume()};
				if (failure.raise != nullptr || sent == 0 || received < sent) [[unlikely]]
				{
					result.error = failure.raise == nullptr ? native_error{raise_invalid_progress, 0u} : failure;
					return;
				}
				transmit_integer_add_define(count, sent);
				first += sent;
				received -= sent;
			}
		}
	}

public:
	/** @brief Captures borrowed handles without issuing I/O at task construction. */
	nt_transfer_work(void *source, void *sink) noexcept
		: input(source), output(sink)
	{
		execute = [](work_request &item) noexcept { static_cast<nt_transfer_work &>(item).copy(); };
	}

	/** @brief Rejects unknown completion modes and runs a proved standalone transfer on the issuer. */
	bool await_ready() noexcept
	{
		bool input_synchronous{}, output_synchronous{};
		if (!query_nt_synchronous_mode(input, input_synchronous) || !input_synchronous ||
			!query_nt_synchronous_mode(output, output_synchronous) || !output_synchronous)
		{
			return true;
		}
		issuer = platform_context();
		if (issuer == nullptr || (current_loop->standalone_root && current_loop->ready.empty()))
		{
			copy();
			return true;
		}
		return false;
	}

	/** @brief Submits the entire copy once. A failed worker submission leaves the stream cursors unchanged. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		return issuer->submit(static_cast<work_request &>(*this));
	}

	/** @brief Returns the committed prefix after the waiting thread has observed worker completion. */
	transfer_completion await_resume() const noexcept
	{
		return result;
	}
};

} // namespace fast_io::async::details

namespace fast_io
{

/** @brief Offers a single frame-owned copy request for raw NT byte observers. */
template <nt_family InputFamily, nt_family OutputFamily, ::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1u && sizeof(OutputChar) == 1u)
inline auto async_transmit_until_eof_request_define(basic_nt_family_io_observer<InputFamily, InputChar> source,
												   basic_nt_family_io_observer<OutputFamily, OutputChar> sink) noexcept
{
	return ::fast_io::async::details::nt_transfer_work{source.native_handle(), sink.native_handle()};
}

} // namespace fast_io
