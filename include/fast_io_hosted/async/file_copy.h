#pragma once

#if defined(FAST_IO_ASYNC_FILE_COPY) && !defined(FAST_IO_ASYNC_FILE_WORKER)
namespace fast_io::async::details
{
/** @brief Suspends once for the complete old-kernel copy when both file descriptions permit it. */
inline task<transfer_completion> copy_without_ring(int input, int output) FAST_IO_ASYNC_THROWS
{
	::fast_io::async::transfer_completion transfer{};
	if (::fast_io::details::linux_regular_file_pair(input, output))
	{
		// One suspended job owns all fallback syscalls, including any short reads and writes.
		auto result{co_await ::fast_io::async::details::invoke_blocking([input, output] {
			return ::fast_io::details::linux_copy_regular_files(input, output);
		})};
		transfer.progress = result.progress;
		transfer.complete = !result.error;
		if (result.error)
		{
			transfer.error = {::fast_io::async::details::raise_posix_error, static_cast<::std::uint_least32_t>(result.error)};
		}
	}
	co_return transfer;
}
} // namespace fast_io::async::details
#endif

namespace fast_io
{
#if defined(FAST_IO_ASYNC_FILE_WORKER)
/** @brief Gives the transfer coroutine a lazy, frame-owned request for a pair of byte-domain Linux descriptors. */
template <::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1 && sizeof(OutputChar) == 1)
inline auto async_transmit_until_eof_request_define(basic_posix_io_observer<InputChar> source,
												   basic_posix_io_observer<OutputChar> sink) noexcept
{
	return ::fast_io::async::details::file_transfer_work{source.fd, sink.fd};
}

/** @brief Preserves the independently awaitable task interface for direct callers of the original customization. */
template <::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1 && sizeof(OutputChar) == 1)
inline ::fast_io::async::task<::fast_io::async::transfer_completion>
async_transmit_until_eof_define(basic_posix_io_observer<InputChar> source, basic_posix_io_observer<OutputChar> sink) FAST_IO_ASYNC_THROWS
{
	co_return co_await async_transmit_until_eof_request_define(source, sink);
}
#else
/** @brief Selects the available kernel transfer without allocating a forwarding coroutine frame. */
template <::std::integral InputChar, ::std::integral OutputChar>
	requires(sizeof(InputChar) == 1 && sizeof(OutputChar) == 1)
inline auto async_transmit_until_eof_define(basic_posix_io_observer<InputChar> source,
											basic_posix_io_observer<OutputChar> sink) FAST_IO_ASYNC_THROWS
{
#if defined(FAST_IO_ASYNC_SPLICE) && defined(FAST_IO_ASYNC_FILE_COPY)
	if (auto *issuer = ::fast_io::async::details::platform_context(); issuer && issuer->has_ring())
	{
		return ::fast_io::async::details::splice_files(source.fd, sink.fd);
	}
	return ::fast_io::async::details::copy_without_ring(source.fd, sink.fd);
#elif defined(FAST_IO_ASYNC_SPLICE)
	return ::fast_io::async::details::splice_files(source.fd, sink.fd);
#else
	return ::fast_io::async::details::copy_without_ring(source.fd, sink.fd);
#endif
}

#endif
} // namespace fast_io
