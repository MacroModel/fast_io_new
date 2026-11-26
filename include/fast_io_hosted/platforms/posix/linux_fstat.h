#pragma once

namespace fast_io::details
{
#if defined(__i386__)
/**
 * @brief Obtains an i386 descriptor's file size using the available Linux stat ABI.
 * @details Linux 2.4 introduced fstat64. Its i386 result spans 96 bytes with the little-endian size at byte 44.
 * The preceding 64-byte fstat structure has a single size word at byte 20. Word arrays keep the kernel layout
 * independent of libc's time and large-file settings, and of the compiler's alignment choice for long long.
 */
inline ::std::size_t linux_i386_file_size(int fd)
{
#if defined(__NR_fstat64)
	::std::uint32_t wide[24];
	auto const status{::fast_io::system_call<__NR_fstat64, int>(fd, wide)};
	if (status != -ENOSYS)
	{
		::fast_io::system_call_throw_error(status);
		if (wide[12] != 0u) [[unlikely]]
		{
			::fast_io::throw_posix_error(EINVAL);
		}
		return wide[11];
	}
#endif
	::std::uint32_t legacy[16];
	::fast_io::system_call_throw_error(::fast_io::system_call<__NR_fstat, int>(fd, legacy));
	return legacy[5];
}
#else
/**
 * @brief Describes the original Linux x86-64 LP64 fstat system-call result.
 * @details The kernel writes 144 bytes. This layout is independent of libc's public struct stat, timestamp
 * representation and feature-test macros. The System V x32 and other processor ABIs supply different layouts.
 */
struct linux_x86_64_stat
{
	::std::uint_least64_t device, inode, links;
	::std::uint_least32_t mode, owner, group, padding;
	::std::uint_least64_t special_device;
	::std::int_least64_t st_size, block_size, blocks;
	::std::uint_least64_t timestamps[6];
	::std::int_least64_t reserved[3];
};
static_assert(sizeof(linux_x86_64_stat) == 144 && offsetof(linux_x86_64_stat, mode) == 24 &&
			  offsetof(linux_x86_64_stat, st_size) == 48);

/** @brief Retrieves kernel metadata without passing a libc-specific stat structure across the syscall boundary. */
inline int linux_fstat(int fd, linux_x86_64_stat &value) noexcept
{
	return ::fast_io::system_call<__NR_fstat, int>(fd, __builtin_addressof(value));
}

/** @brief Tests the file-type bits in the Linux kernel's st_mode value. */
inline constexpr bool linux_stat_is_regular(linux_x86_64_stat const &value) noexcept
{
	return (value.mode & 0170000u) == 0100000u;
}
#endif
} // namespace fast_io::details
