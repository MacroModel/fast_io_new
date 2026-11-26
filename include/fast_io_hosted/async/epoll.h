#pragma once

namespace fast_io::async::details
{

/** @brief Waits for descriptor readiness without changing its shared O_NONBLOCK flag. */
class epoll
{
	int fd{-1};
	posix_request *interests{};

	/** @brief Forms the readiness mask for the operation's direction. */
	static constexpr unsigned mask(posix_request const &request) noexcept
	{
		return request.write ? EPOLLOUT : EPOLLIN;
	}

	/** @brief Merges the interest in both directions on one descriptor. */
	unsigned mask_for(int descriptor) const noexcept
	{
		unsigned events{};
		for (auto *request{interests}; request != nullptr; request = request->interest_next)
		{
			if (request->fd == descriptor)
			{
				events |= mask(*request);
			}
		}
		return events;
	}

	/** @brief Moves armed retries to poll when the kernel cannot service the epoll wait ABI. */
	void retire(poll_wait &fallback) noexcept
	{
		::fast_io::system_call<__NR_close, int>(fd);
		fd = -ENOSYS;
		while (auto *request = interests)
		{
			interests = request->interest_next;
			fallback.push(*request);
		}
	}

public:
	epoll() noexcept = default;
	epoll(epoll const &) = delete;
	epoll &operator=(epoll const &) = delete;

	/** @brief Releases the private epoll descriptor. */
	~epoll()
	{
		if (fd >= 0)
		{
			::fast_io::system_call<__NR_close, int>(fd);
		}
	}

	/** @brief Registers a retry after a nonblocking read or write reported EAGAIN. */
	bool push(posix_request &request) noexcept
	{
		if (fd == -1)
		{
#if defined(__NR_epoll_create1)
			fd = ::fast_io::system_call<__NR_epoll_create1, int>(EPOLL_CLOEXEC);
#endif
#if defined(__NR_epoll_create)
#if defined(__NR_epoll_create1)
			if (fd == -ENOSYS)
#endif
			{
				// Before Linux 2.6.27 the size hint had to be positive and close-on-exec was separate.
				fd = ::fast_io::system_call<__NR_epoll_create, int>(1);
				if (fd >= 0 &&
#if defined(O_CLOEXEC)
					::fast_io::details::linux_descriptor_control(fd, F_SETFD, FD_CLOEXEC) < 0)
#else
					::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
#endif
				{
					::fast_io::system_call<__NR_close, int>(fd);
					fd = -1;
				}
			}
#endif
			if (fd < 0)
			{
				if (fd != -ENOSYS)
				{
					// Resource exhaustion can clear before the next registration.
					fd = -1;
				}
				return false;
			}
		}
		else if (fd < 0)
		{
			return false;
		}
		auto const old_mask{mask_for(request.fd)};
		auto const new_mask{old_mask | mask(request)};
		if (old_mask != new_mask)
		{
			::epoll_event event{};
			event.events = new_mask;
			event.data.fd = request.fd;
			auto const result{::fast_io::system_call<__NR_epoll_ctl, int>(
				fd, old_mask == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, request.fd, &event)};
			if (result < 0)
			{
				return false;
			}
		}
		request.interest_next = interests;
		interests = ::fast_io::freestanding::addressof(request);
		return true;
	}

	/** @brief Returns whether the loop is waiting for readiness. */
	bool busy() const noexcept
	{
		return interests != nullptr;
	}

	/** @brief Returns the descriptor for combining epoll with other completion sources. */
	int descriptor() const noexcept
	{
		return fd;
	}

	/**
	 * @brief Retries ready operations and retires completed interests in one traversal per event.
	 * @details A blocking wait is valid when epoll is the issuer's only completion source. Otherwise
	 * the issuer combines the epoll descriptor with its other sources and polls this queue separately.
	 */
	bool reap(request_queue &ready, poll_wait &fallback, bool wait) noexcept
	{
		::epoll_event events[16];
		int count;
		do
		{
#if defined(__NR_epoll_wait)
			count = ::fast_io::system_call<__NR_epoll_wait, int>(fd, events, 16, wait ? -1 : 0);
#if defined(__NR_epoll_pwait)
			if (count == -ENOSYS) [[unlikely]]
			{
				count = ::fast_io::system_call<__NR_epoll_pwait, int>(fd, events, 16, wait ? -1 : 0, nullptr, 0u);
			}
#endif
#else
			count = ::fast_io::system_call<__NR_epoll_pwait, int>(fd, events, 16, wait ? -1 : 0, nullptr, 0u);
#endif
		} while (wait && count == -EINTR);
		if (count < 0 && count != -EINTR) [[unlikely]]
		{
			// A sandbox can allow registration while withholding both wait syscalls.
			retire(fallback);
			return false;
		}
		for (int index{}; index < count; ++index)
		{
			auto const descriptor{events[index].data.fd};
			auto const reported{events[index].events};
			unsigned old_mask{}, new_mask{};
			auto **link{::fast_io::freestanding::addressof(interests)};
			while (auto *request = *link)
			{
				if (request->fd == descriptor)
				{
					auto const interest{mask(*request)};
					old_mask |= interest;
					if ((reported & (interest | EPOLLERR | EPOLLHUP)) != 0)
					{
						request->perform();
						if (request->result != -EAGAIN && request->result != -EWOULDBLOCK)
						{
							// Remove the interest before making the coroutine runnable.
							*link = request->interest_next;
							ready.push(*request);
							continue;
						}
					}
					new_mask |= interest;
				}
				link = ::fast_io::freestanding::addressof(request->interest_next);
			}
			if (old_mask != new_mask)
			{
				::epoll_event event{};
				event.events = new_mask;
				event.data.fd = descriptor;
				::fast_io::system_call<__NR_epoll_ctl, int>(
					fd, new_mask == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD, descriptor, &event);
			}
		}
		return true;
	}
};

} // namespace fast_io::async::details
