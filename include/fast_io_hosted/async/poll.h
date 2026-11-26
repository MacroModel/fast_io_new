#pragma once

namespace fast_io::async::details
{

/** @brief Multiplexes nonblocking Linux descriptors if the running kernel declines epoll registration. */
class poll_wait
{
	posix_request *interests{};
	::std::size_t size{};

public:
	/** @brief Retains the request until poll reports a retryable direction or a terminal error. */
	bool push(posix_request &request) noexcept
	{
		request.interest_next = interests;
		interests = ::fast_io::freestanding::addressof(request);
		++size;
		return true;
	}

	/** @brief Reports whether ordinary descriptors must participate in the next issuer wait. */
	bool busy() const noexcept
	{
		return interests != nullptr;
	}

	/** @brief Waits for native completion sources and fallback descriptors using one syscall. */
	void wait(::pollfd const *extra, unsigned count, request_queue &ready, bool block = true) noexcept
	{
		if (SIZE_MAX - count < size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		::fast_io::details::local_operator_new_array_ptr<::pollfd> entries(size + count);
		auto *cursor{entries.ptr};
		for (unsigned i{}; i != count; ++i)
		{
			*cursor++ = extra[i];
		}
		for (auto *item{interests}; item != nullptr; item = item->interest_next)
		{
			*cursor++ = {item->fd, static_cast<short>(item->write ? POLLOUT : POLLIN), 0};
		}
		int status;
		do
		{
			status = linux_poll_descriptors(entries.ptr, entries.size, block);
		} while (status == -EINTR);
		if (status < 0) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		cursor = entries.ptr + count;
		auto **link{&interests};
		while (auto *item = *link)
		{
			if ((cursor++)->revents != 0)
			{
				item->perform();
				if (item->result != -EAGAIN && item->result != -EWOULDBLOCK)
				{
					*link = item->interest_next;
					--size;
					ready.push(*item);
					continue;
				}
			}
			link = &item->interest_next;
		}
	}
};

} // namespace fast_io::async::details
