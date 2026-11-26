#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <cerrno>
#include <unistd.h>
#include <sys/syscall.h>

namespace fast_io
{

/**
 * @brief Issues a Linux syscall through the generic libc entry and restores the raw kernel error convention.
 * @details Architecture-specific entry points return negative errno directly. libc's syscall sets errno and
 * returns -1 instead. Normalize it here so ENOSYS triggers the same runtime fallback on every architecture.
 */
template <::std::size_t syscall_number, ::std::signed_integral return_value_type, typename... Args>
	requires(::std::is_trivially_copyable_v<Args> && ...)
inline return_value_type system_call(Args... args) noexcept
{
	long ret{::syscall(syscall_number, args...)};
	if (ret == -1) [[unlikely]]
	{
		return static_cast<return_value_type>(-errno);
	}
	return static_cast<return_value_type>(ret);
}

/** @brief Rejects an exact inline syscall only when a caller instantiates it on this architecture. */
template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
[[__gnu__::__always_inline__]]
inline return_value_type inline_syscall(auto, auto) noexcept
{
	static_assert(sizeof(return_value_type) == 0, "force inline for generic syscall is not supported");
}

template <::std::size_t syscall_number>
[[noreturn]]
inline void system_call_no_return(auto p1) noexcept
{
	::syscall(syscall_number, p1);
	__builtin_unreachable();
}

template <::std::integral I>
[[noreturn]]
inline void fast_exit(I ret) noexcept
{
	system_call_no_return<__NR_exit>(ret);
}
} // namespace fast_io
