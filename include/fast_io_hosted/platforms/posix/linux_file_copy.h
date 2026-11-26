#pragma once

#if defined(__NR_splice) && defined(__NR_sendfile) && defined(__NR_fstatfs) && defined(F_SETPIPE_SZ) && __has_include(<sys/statfs.h>)
#include <sys/statfs.h>
#define FAST_IO_LINUX_FILE_COPY_PIPE
#endif

namespace fast_io::details
{
/** @brief Determines whether both descriptors name distinct ordinary files with a positive input size.
 * @details Device and inode identity exclude self-copy through aliases. The size is only an eligibility hint.
 * Pseudo-files with zero reported size, pipes and sockets retain ordinary stream semantics.
 */
inline bool linux_regular_file_pair(int input, int output, linux_x86_64_stat &source,
									linux_x86_64_stat &sink) noexcept
{
	return linux_fstat(input, source) == 0 &&
		   linux_stat_is_regular(source) && source.st_size > 0 &&
		   linux_fstat(output, sink) == 0 && linux_stat_is_regular(sink) &&
		   (source.device != sink.device || source.inode != sink.inode);
}

/** @brief Checks native copy eligibility when the caller has no use for the individual file attributes. */
inline bool linux_regular_file_pair(int input, int output) noexcept
{
	linux_x86_64_stat source, sink;
	return linux_regular_file_pair(input, output, source, sink);
}

/** @brief Returns whether an optional kernel copy method declined this particular descriptor pair. */
inline constexpr bool linux_copy_unsupported(::std::ptrdiff_t code) noexcept
{
	return code == -ENOSYS || code == -EOPNOTSUPP || code == -EXDEV || code == -EINVAL || code == -EBADF || code == -EPERM;
}

/** @brief Retains the committed prefix and a native errno for both synchronous and coroutine consumers. */
struct linux_regular_copy_result
{
	::fast_io::transmit_result progress{};
	int error{};
};

#if defined(FAST_IO_LINUX_FILE_COPY_PIPE)
/** @brief Reports the terminal pipe state independently of the committed byte counter. */
struct linux_pipe_copy_status
{
	int error{};
	bool complete{};
	bool empty{true};
};

/**
 * @brief Forwards a pipe's outstanding bytes after a destination declines splice.
 * @details The input position has already advanced by the bytes held in the pipe. Forward them in
 *          order before obtaining any additional input. Count only completed writes.
 */
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline int linux_copy_pending_file_pipe(int reader, int output, ::std::size_t pending,
									   ::fast_io::uintfpos_transmit_reference_wrapper count) noexcept
{
	::fast_io::details::local_operator_new_array_ptr<::std::byte> buffer(pending);
	while (pending != 0)
	{
		auto const got{::fast_io::system_call<__NR_read, ::std::ptrdiff_t>(reader, buffer.ptr, pending)};
		if (got == -EINTR)
		{
			continue;
		}
		if (got <= 0)
		{
			return got == 0 ? EIO : static_cast<int>(-got);
		}
		for (::std::ptrdiff_t sent{}; sent < got;)
		{
			auto const n{::fast_io::system_call<__NR_write, ::std::ptrdiff_t>(output, buffer.ptr + sent, got - sent)};
			if (n == -EINTR)
			{
				continue;
			}
			if (n <= 0)
			{
				return n == 0 ? EIO : static_cast<int>(-n);
			}
			sent += n;
			transmit_integer_add_define(count, static_cast<::std::size_t>(n));
		}
		pending -= static_cast<::std::size_t>(got);
	}
	return 0;
}

/**
 * @brief Relays an ordinary file through an empty private pipe and preserves both implicit file positions.
 * @details A successful fill is paired with a complete drain before the next fill. An unavailable
 *          system call or filesystem method returns partial progress at the committed byte position.
 *          The caller may continue with another copy method when the pipe is empty and no error occurred.
 */
inline linux_pipe_copy_status linux_copy_file_through_pipe(int input, int output, int const (&ends)[2],
														  unsigned chunk, bool use_sendfile,
														  ::fast_io::uintfpos_transmit_reference_wrapper count) noexcept
{
	linux_pipe_copy_status result{};
	while (chunk != 0)
	{
		auto got{use_sendfile ? ::fast_io::system_call<__NR_sendfile, ::std::ptrdiff_t>(ends[1], input, nullptr, chunk)
							 : ::fast_io::system_call<__NR_splice, ::std::ptrdiff_t>(input, nullptr, ends[1], nullptr, chunk, 0u)};
		if (got == 0)
		{
			result.complete = true;
			break;
		}
		if (got < 0)
		{
			if (got == -EINTR)
			{
				continue;
			}
			if (linux_copy_unsupported(got))
			{
				if (use_sendfile)
				{
					// A declined fill leaves both the pipe and the source position unchanged.
					use_sendfile = false;
					continue;
				}
			}
			else
			{
				result.error = static_cast<int>(-got);
			}
			break;
		}
		while (got != 0)
		{
			auto const n{::fast_io::system_call<__NR_splice, ::std::ptrdiff_t>(ends[0], nullptr, output, nullptr, got, 0u)};
			if (n == -EINTR)
			{
				continue;
			}
			if (n <= 0)
			{
				result.error = n == 0 ? EIO : static_cast<int>(-n);
				if (linux_copy_unsupported(n))
				{
					result.error = linux_copy_pending_file_pipe(ends[0], output, static_cast<::std::size_t>(got), count);
				}
				result.empty = result.error == 0;
				return result;
			}
			got -= n;
			transmit_integer_add_define(count, static_cast<::std::size_t>(n));
		}
	}
	return result;
}
#endif

/** @brief Copies a verified regular-file pair to EOF, retaining offsets through every fallback.
 * @details copy_file_range can offload the copy to the filesystem. sendfile is the next choice on older
 * kernels. If neither backend accepts the pair, the ordinary read/write protocol resumes at the exact prefix.
 * An actual read confirms EOF even on filesystems that answer zero to unsupported kernel-copy requests.
 */
inline linux_regular_copy_result linux_copy_regular_files(int input, int output)
{
	linux_regular_copy_result result{};
	::fast_io::uintfpos_transmit_reference_wrapper count{__builtin_addressof(result.progress.transmitted)};
	auto kernel_copy = [&](auto copy) {
		for (;;)
		{
			auto const n{copy()};
			if (n > 0)
			{
				transmit_integer_add_define(count, static_cast<::std::size_t>(n));
				continue;
			}
			if (n == -EINTR)
			{
				continue;
			}
			if (n < 0 && !linux_copy_unsupported(n))
			{
				result.error = static_cast<int>(-n);
			}
			return n;
		}
	};
	constexpr ::std::size_t limit{0x7ffff000u};
	::std::ptrdiff_t n{-ENOSYS};
#if defined(__NR_copy_file_range)
	n = kernel_copy([&] { return ::fast_io::system_call<__NR_copy_file_range, ::std::ptrdiff_t>(
							  input, nullptr, output, nullptr, limit, 0u); });
#endif
#if defined(__NR_sendfile)
	if (n < 0 && !result.error)
	{
		n = kernel_copy([&] { return ::fast_io::system_call<__NR_sendfile, ::std::ptrdiff_t>(output, input, nullptr, limit); });
	}
#endif
	if (result.error)
	{
		return result;
	}
	// A successful offload normally needs just this one-byte EOF probe and no heap allocation.
	::std::byte first;
	auto copy_block = [&](::std::byte *buffer, ::std::size_t size) {
		::std::ptrdiff_t got;
		do
		{
			got = ::fast_io::system_call<__NR_read, ::std::ptrdiff_t>(input, buffer, size);
		} while (got == -EINTR);
		if (got <= 0)
		{
			result.error = static_cast<int>(-got);
			return false;
		}
		for (::std::ptrdiff_t sent{}; sent < got;)
		{
			auto const done{::fast_io::system_call<__NR_write, ::std::ptrdiff_t>(output, buffer + sent, got - sent)};
			if (done == -EINTR)
			{
				continue;
			}
			if (done <= 0)
			{
				result.error = done == 0 ? EIO : static_cast<int>(-done);
				return false;
			}
			sent += done;
			transmit_integer_add_define(count, static_cast<::std::size_t>(done));
		}
		return true;
	};
	if (n == 0 && !copy_block(&first, 1u))
	{
		return result;
	}
	constexpr auto capacity{::fast_io::details::transmit_buffer_size_cache<1>};
	::fast_io::details::local_operator_new_array_ptr<::std::byte> buffer(capacity);
	while (copy_block(buffer.ptr, capacity))
	{
	}
	return result;
}

/** @brief Transfers a verified native file pair and translates its terminal error once. */
inline ::fast_io::transmit_result linux_copy_regular_files_checked(int input, int output,
																  [[maybe_unused]] linux_x86_64_stat const &source,
																  [[maybe_unused]] linux_x86_64_stat const &sink)
{
	::fast_io::transmit_result progress{};
	::fast_io::uintfpos_transmit_reference_wrapper count{__builtin_addressof(progress.transmitted)};
#if defined(FAST_IO_LINUX_FILE_COPY_PIPE)
	if (source.st_size >= 1024 * 1024 && sink.st_size == 0 && source.device == sink.device)
	{
		// Prefer a bounded private pipe for the first write to a substantial file on ext-family filesystems.
		struct ::statfs filesystem;
		if (::fast_io::system_call<__NR_fstatfs, int>(input, &filesystem) == 0 && filesystem.f_type == 0xef53)
		{
			int ends[2];
			if (linux_private_pipe(ends, false) == 0)
			{
				struct pipe_guard
				{
					int const (&descriptors)[2];
					/** @brief Closes both private descriptors on completion and on exceptional exit. */
					~pipe_guard()
					{
						::fast_io::system_call<__NR_close, int>(descriptors[0]);
						::fast_io::system_call<__NR_close, int>(descriptors[1]);
					}
				} guard{ends};
				constexpr unsigned request{256u * 1024u};
				auto const capacity{linux_descriptor_control(ends[0], F_SETPIPE_SZ, request)};
				if (capacity >= static_cast<int>(request))
				{
					auto const exchanged{linux_copy_file_through_pipe(input, output, ends, request, true, count)};
					if (exchanged.error != 0)
					{
						::fast_io::throw_posix_error(exchanged.error);
					}
					if (exchanged.complete)
					{
						return progress;
					}
				}
			}
		}
	}
#endif
	auto const rest{linux_copy_regular_files(input, output)};
	if (rest.error)
	{
		::fast_io::throw_posix_error(rest.error);
	}
	transmit_integer_add_define(count, rest.progress.transmitted);
	return progress;
}
} // namespace fast_io::details

namespace fast_io
{
/** @brief Supplies the synchronous EOF protocol for ordinary byte-domain file observers on Linux x86-64. */
template <::std::integral InChar, ::std::integral OutChar>
	requires(sizeof(InChar) == 1 && sizeof(OutChar) == 1)
inline transmit_result status_transmit_until_eof_define(basic_posix_io_observer<OutChar> out, basic_posix_io_observer<InChar> in)
{
	details::linux_x86_64_stat source, sink;
	if (!details::linux_regular_file_pair(in.fd, out.fd, source, sink))
	{
		return details::transmit_until_eof_main_impl(out, in);
	}
	return details::linux_copy_regular_files_checked(in.fd, out.fd, source, sink);
}

/**
 * @brief Transfers bytes between two ordinary POSIX file observers without a userspace buffer when available.
 * @details Each observer exposes the descriptor's current byte position, independently of its element type.
 *          Decorators and buffered streams retain their read/write protocol and any completion or finalization effects
 *          unless they explicitly supply an equivalent whole-transfer operation.
 */
template <::std::integral InChar, ::std::integral OutChar>
inline transmit_result status_transmit_bytes_until_eof_define(basic_posix_io_observer<OutChar> out, basic_posix_io_observer<InChar> in)
{
	details::linux_x86_64_stat source, sink;
	if (!details::linux_regular_file_pair(in.fd, out.fd, source, sink))
	{
		return details::transmit_bytes_until_eof_main_impl(out, in);
	}
	return details::linux_copy_regular_files_checked(in.fd, out.fd, source, sink);
}
} // namespace fast_io

#undef FAST_IO_LINUX_FILE_COPY_PIPE
