#pragma once

namespace fast_io::async::details
{

/** @brief Waits for BSD read and write filters while retaining the original descriptor flags. */
class kqueue_wait
{
	int fd{-1};
	bool probed{};
	posix_request *interests{};

	/** @brief Chooses the native kevent filter type, which differs between BSD implementations. */
	static constexpr auto filter(bool writing) noexcept
	{
		return writing ? EVFILT_WRITE : EVFILT_READ;
	}

	/** @brief Publishes a level-triggered filter change before exposing the associated request. */
	bool change(int stream, bool writing, unsigned flags) noexcept
	{
		struct ::kevent event{};
		EV_SET(&event, static_cast<::std::uintptr_t>(stream), filter(writing), flags, 0, 0, 0);
		int status;
		do
		{
			status = ::kevent(fd, &event, 1, nullptr, 0, nullptr);
		} while (status == -1 && errno == EINTR);
		return status == 0;
	}

public:
	kqueue_wait() noexcept = default;
	kqueue_wait(kqueue_wait const &) = delete;
	kqueue_wait &operator=(kqueue_wait const &) = delete;

	/** @brief Releases the private event queue when its issuer exits. */
	~kqueue_wait()
	{
		if (fd >= 0)
		{
			::close(fd);
		}
	}

	/** @brief Probes the OS once so that an unavailable queue consistently selects the POSIX fallback. */
	bool available() noexcept
	{
		if (!probed)
		{
			probed = true;
			fd = ::kqueue();
			if (fd >= 0 && ::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
			{
				::close(fd);
				fd = -1;
			}
		}
		return fd >= 0;
	}

	/** @brief Registers a direction after its first nonblocking operation returned would-block. */
	bool push(posix_request &request) noexcept
	{
		if (!change(request.fd, request.write, EV_ADD | EV_ENABLE))
		{
			request.result = -errno;
			return false;
		}
		request.interest_next = interests;
		interests = ::fast_io::freestanding::addressof(request);
		return true;
	}

	/** @brief Reports whether stream readiness can wake this issuer. */
	bool busy() const noexcept
	{
		return interests != nullptr;
	}

	/** @brief Blocks for either a stream filter or a completed worker, then retries the matching operations. */
	void wait(request_queue &ready, int wake_fd, bool block = true) noexcept
	{
		if (wake_fd >= 0)
		{
			// Polling the kqueue descriptor combines the two completion sources without a second kevent filter.
			::pollfd descriptors[2]{{fd, POLLIN, 0}, {wake_fd, POLLIN, 0}};
			int status;
			do
			{
				status = ::poll(descriptors, 2, block ? -1 : 0);
			} while (status < 0 && errno == EINTR);
			if (status < 0) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			if (descriptors[0].revents == 0)
			{
				return;
			}
		}
		struct ::kevent events[16];
		int count;
		do
		{
			::timespec const zero{};
			count = ::kevent(fd, nullptr, 0, events, 16, block ? nullptr : &zero);
		} while (count < 0 && errno == EINTR);
		if (count < 0) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		for (int i{}; i != count; ++i)
		{
			auto const stream{static_cast<int>(events[i].ident)};
			bool const writing{events[i].filter == EVFILT_WRITE};
			bool still_waiting{};
			auto **link{&interests};
			while (auto *request = *link)
			{
				if (request->fd == stream && request->write == writing)
				{
					request->perform();
					if (request->result != -EAGAIN && request->result != -EWOULDBLOCK)
					{
						*link = request->interest_next;
						ready.push(*request);
						continue;
					}
					still_waiting = true;
				}
				link = &request->interest_next;
			}
			if (!still_waiting)
			{
				change(stream, writing, EV_DELETE);
			}
		}
	}
};

} // namespace fast_io::async::details
