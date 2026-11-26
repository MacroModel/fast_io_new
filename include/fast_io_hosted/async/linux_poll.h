#pragma once

namespace fast_io::async::details
{

#if defined(__NR__newselect) || (defined(__x86_64__) && defined(__NR_select)) || defined(__NR_pselect6) || defined(__NR_pselect6_time64)
/**
 * @brief Translates the issuer's read and write interests to Linux select bitmaps on kernels without poll.
 * @details Kernel bitmaps extend to the highest descriptor. A descriptor above libc's FD_SETSIZE therefore
 * works on the same path as an ordinary pipe. A bad descriptor wakes the issuer to retry each nonblocking
 * operation individually, which preserves the error on its originating request.
 */
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline int linux_select_descriptors(::pollfd *entries, ::std::size_t count, bool block) noexcept
{
	constexpr auto bits{sizeof(unsigned long) * __CHAR_BIT__};
	int maximum{-1};
	for (::std::size_t i{}; i != count; ++i)
	{
		entries[i].revents = 0;
		if (entries[i].fd > maximum)
		{
			maximum = entries[i].fd;
		}
	}
	if (maximum < 0)
	{
		return 0;
	}
	if (maximum == ::std::numeric_limits<int>::max()) [[unlikely]]
	{
		// nfds is a signed int. Linux cannot allocate this descriptor.
		for (::std::size_t i{}; i != count; ++i)
		{
			if (entries[i].fd == maximum)
			{
				entries[i].revents = POLLNVAL;
			}
		}
		return 1;
	}
	auto const words{static_cast<::std::size_t>(maximum) / bits + 1u};
	::fast_io::details::local_operator_new_array_ptr<unsigned long> storage(words * 2u);
	auto *reads{storage.ptr};
	auto *writes{reads + words};
	for (::std::size_t i{}; i != words * 2u; ++i)
	{
		reads[i] = 0;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		auto const fd{entries[i].fd};
		if (fd >= 0)
		{
			auto const word{static_cast<unsigned>(fd) / bits};
			auto const mask{1ul << (static_cast<unsigned>(fd) % bits)};
			if (entries[i].events & POLLIN)
			{
				reads[word] |= mask;
			}
			if (entries[i].events & POLLOUT)
			{
				writes[word] |= mask;
			}
		}
	}
	// Both time32 and time64 accept an all-zero timeout. Leave enough space for either value-result ABI.
	::std::uint_least64_t zero[2]{};
#if defined(__NR__newselect)
	auto const result{::fast_io::system_call<__NR__newselect, int>(maximum + 1, reads, writes, nullptr, block ? nullptr : zero)};
#elif defined(__x86_64__) && defined(__NR_select)
	auto const result{::fast_io::system_call<__NR_select, int>(maximum + 1, reads, writes, nullptr, block ? nullptr : zero)};
#elif defined(__NR_pselect6)
	auto const result{::fast_io::system_call<__NR_pselect6, int>(maximum + 1, reads, writes, nullptr, block ? nullptr : zero, nullptr)};
#else
	auto const result{::fast_io::system_call<__NR_pselect6_time64, int>(maximum + 1, reads, writes, nullptr, block ? nullptr : zero, nullptr)};
#endif
	if (result <= 0 && result != -EBADF)
	{
		return result;
	}
	int active{};
	for (::std::size_t i{}; i != count; ++i)
	{
		auto &entry{entries[i]};
		if (entry.fd < 0)
		{
			continue;
		}
		auto const word{static_cast<unsigned>(entry.fd) / bits};
		auto const mask{1ul << (static_cast<unsigned>(entry.fd) % bits)};
		if (result == -EBADF)
		{
			// A closed descriptor invalidates select's entire result, including its output sets.
			entry.revents = entry.events;
		}
		else
		{
			entry.revents = static_cast<short>(entry.events & (((reads[word] & mask) ? POLLIN : 0) | ((writes[word] & mask) ? POLLOUT : 0)));
		}
		active += entry.revents != 0;
	}
	return active;
}
#endif

/** @brief Waits for Linux descriptor readiness using the first supported running-kernel ABI. */
inline int linux_poll_descriptors(::pollfd *entries, ::std::size_t count, bool block) noexcept
{
	[[maybe_unused]] int result{-ENOSYS};
#if defined(__NR_poll)
	result = ::fast_io::system_call<__NR_poll, int>(entries, count, block ? -1 : 0);
	if (result != -ENOSYS) [[likely]]
	{
		return result;
	}
#endif
#if defined(__NR_ppoll)
	long zero[2]{};
	result = ::fast_io::system_call<__NR_ppoll, int>(entries, count, block ? nullptr : zero, nullptr, 0u);
	if (result != -ENOSYS) [[likely]]
	{
		return result;
	}
#endif
#if defined(__NR__newselect) || (defined(__x86_64__) && defined(__NR_select)) || defined(__NR_pselect6) || defined(__NR_pselect6_time64)
	return linux_select_descriptors(entries, count, block);
#else
	// A port without an exposed select syscall number can still supply libc's compatibility wrapper.
	result = ::poll(entries, count, block ? -1 : 0);
	return result < 0 ? -errno : result;
#endif
}

} // namespace fast_io::async::details
