#pragma once

namespace fast_io::async::details
{

/** @brief Multiplexes nonblocking POSIX streams on targets providing the BSD select interface. */
class select_wait
{
	posix_request *interests{};

public:
	/** @brief Waits only on descriptors representable by the target's fd_set. */
	bool push(posix_request &request) noexcept
	{
		if (request.fd < 0 || request.fd >= FD_SETSIZE)
		{
			return false;
		}
		request.interest_next = interests;
		interests = ::fast_io::freestanding::addressof(request);
		return true;
	}

	/** @brief Reports whether a readiness retry is pending. */
	bool busy() const noexcept
	{
		return interests != nullptr;
	}

	/** @brief Waits for stream readiness, rebuilding value-result sets after every interrupt. */
	void wait(request_queue &ready, int wake_fd, bool block = true) noexcept
	{
		::fd_set reads, writes;
		int result;
		do
		{
			FD_ZERO(&reads);
			FD_ZERO(&writes);
			int maximum{wake_fd};
			if (wake_fd >= 0)
			{
				FD_SET(wake_fd, &reads);
			}
			for (auto *request{interests}; request != nullptr; request = request->interest_next)
			{
				FD_SET(request->fd, request->write ? &writes : &reads);
				if (request->fd > maximum)
				{
					maximum = request->fd;
				}
			}
			if (maximum < 0) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			// A runnable coroutine may depend on this request. Probe without delaying the ready batch.
			::timeval zero{};
			result = ::select(maximum + 1, &reads, &writes, nullptr, block ? nullptr : &zero);
		} while (result == -1 && errno == EINTR);

		int const failure{result < 0 ? errno : 0};
		auto **link{::fast_io::freestanding::addressof(interests)};
		while (auto *request = *link)
		{
			if (failure != 0 || FD_ISSET(request->fd, request->write ? &writes : &reads))
			{
				if (failure == 0)
				{
					request->perform();
				}
				else
				{
					request->result = -failure;
				}
				if (request->result != -EAGAIN && request->result != -EWOULDBLOCK)
				{
					*link = request->interest_next;
					ready.push(*request);
					continue;
				}
			}
			link = ::fast_io::freestanding::addressof(request->interest_next);
		}
	}
};

} // namespace fast_io::async::details
