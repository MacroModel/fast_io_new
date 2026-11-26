#pragma once

namespace fast_io::details
{
/** @brief Changes or queries a descriptor flag while preserving Linux's negative error convention. */
inline int linux_descriptor_control(int fd, int command, int argument = 0) noexcept
{
#if defined(__NR_fcntl)
	return ::fast_io::system_call<__NR_fcntl, int>(fd, command, argument);
#else
	auto const result{::fcntl(fd, command, argument)};
	return result < 0 ? -errno : result;
#endif
}

/**
 * @brief Creates a private pipe with requested status and descriptor flags on the running kernel.
 * @details Linux 2.6.27 introduced pipe2. Earlier kernels set the two flags after pipe creation.
 * If either change fails both descriptors are closed before the original error is returned.
 */
inline int linux_private_pipe(int (&ends)[2], bool nonblocking) noexcept
{
#if defined(__NR_pipe2)
	int const atomic{::fast_io::system_call<__NR_pipe2, int>(ends, O_CLOEXEC | (nonblocking ? O_NONBLOCK : 0))};
	if (atomic != -ENOSYS) [[likely]]
	{
		return atomic;
	}
#endif
#if defined(__NR_pipe) && (defined(__x86_64__) || defined(__i386__))
	auto const opened{::fast_io::system_call<__NR_pipe, int>(ends)};
	if (opened < 0)
	{
		return opened;
	}
#else
	// Some legacy architectures return the descriptor pair in registers through the libc ABI.
	if (::pipe(ends) < 0)
	{
		return -errno;
	}
#endif
	for (int fd : ends)
	{
		int result{linux_descriptor_control(fd, F_SETFD, FD_CLOEXEC)};
		if (result >= 0 && nonblocking)
		{
			result = linux_descriptor_control(fd, F_SETFL, O_NONBLOCK);
		}
		if (result < 0)
		{
			::fast_io::system_call<__NR_close, int>(ends[0]);
			::fast_io::system_call<__NR_close, int>(ends[1]);
			return result;
		}
	}
	return 0;
}

#if defined(__NR_open)
/**
 * @brief Opens an absolute, working-directory or directory-relative pathname when openat returns ENOSYS.
 * @details A relative directory descriptor is resolved by procfs without changing the process working directory.
 * The legacy kernel's pathname limit also bounds the temporary procfs path.
 */
inline int linux_legacy_openat(int dirfd, char const *path, int flags, mode_t mode) noexcept
{
	if (!path)
	{
		return -EFAULT;
	}
	if (*path == '\0')
	{
		return -ENOENT;
	}
	if (dirfd == AT_FDCWD || *path == '/')
	{
		return ::fast_io::system_call<__NR_open, int>(path, flags, mode);
	}
	auto const valid{linux_descriptor_control(dirfd, F_GETFD)};
	if (valid < 0)
	{
		return valid;
	}
	char joined[4096]{"/proc/self/fd/"};
	char digits[::std::numeric_limits<unsigned>::digits10 + 1];
	char *first{digits + sizeof(digits)};
	auto number{static_cast<unsigned>(dirfd)};
	do
	{
		*--first = static_cast<char>('0' + number % 10u);
		number /= 10u;
	} while (number);
	char *out{joined + sizeof("/proc/self/fd/") - 1};
	while (first != digits + sizeof(digits))
	{
		*out++ = *first++;
	}
	*out++ = '/';
	do
	{
		if (out == joined + sizeof(joined))
		{
			return -ENAMETOOLONG;
		}
		*out++ = *path;
	} while (*path++);
	return ::fast_io::system_call<__NR_open, int>(joined, flags, mode);
}
#endif

/**
 * @brief Restores close-on-exec when a successful old open silently ignored O_CLOEXEC.
 * @details Kernel support is uniform across files. Each thread checks its first flagged open and old kernels
 * keep setting the flag explicitly. Linux could not make this fallback atomic with a concurrent fork and exec.
 */
inline int linux_finish_open(int fd, int flags) noexcept
{
	if (fd >= 0 && (flags & O_CLOEXEC))
	{
#if defined(__SINGLE_THREAD__)
		// A single-thread runtime may predate the target's C++ thread-local storage ABI.
		static bool atomic_cloexec{};
#else
		static thread_local bool atomic_cloexec{};
#endif
		if (!atomic_cloexec) [[unlikely]]
		{
			int result{linux_descriptor_control(fd, F_GETFD)};
			if (result >= 0)
			{
				if (result & FD_CLOEXEC)
				{
					atomic_cloexec = true;
				}
				else
				{
					result = linux_descriptor_control(fd, F_SETFD, result | FD_CLOEXEC);
				}
			}
			if (result < 0)
			{
				::fast_io::system_call<__NR_close, int>(fd);
				return result;
			}
		}
	}
	return fd;
}
} // namespace fast_io::details
