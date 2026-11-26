#pragma once

namespace fast_io::async::details
{

/** @brief Links a suspended operation to exactly one submission or completion queue. */
struct request_link
{
	request_link *next{};
	::std::coroutine_handle<> continuation{};
};

/** @brief Maintains a nonallocating FIFO of frame-owned requests. */
class request_queue
{
	request_link *head{}, *tail{};

public:
	/** @brief Reports whether the queue contains any frame-owned request. */
	bool empty() const noexcept
	{
		return head == nullptr;
	}

	/** @brief Inspects the first request before reserving enough submission slots for its complete operation. */
	request_link &front() const noexcept
	{
		return *head;
	}

	/** @brief Appends a request whose lifetime extends through its eventual completion. */
	void push(request_link &item) noexcept
	{
		item.next = nullptr;
		auto *address{::fast_io::freestanding::addressof(item)};
		if (tail == nullptr)
		{
			head = address;
		}
		else
		{
			tail->next = address;
		}
		tail = address;
	}

	/** @brief Unlinks and returns the first request before its frame can resume. */
	request_link &pop() noexcept
	{
		auto *item{head};
		head = item->next;
		if (head == nullptr)
		{
			tail = nullptr;
		}
		return *item;
	}

	/** @brief Moves all ready requests into the destination without visiting individual nodes. */
	void append(request_queue &source) noexcept
	{
		if (source.empty())
		{
			return;
		}
		if (tail == nullptr)
		{
			head = source.head;
		}
		else
		{
			tail->next = source.head;
		}
		tail = source.tail;
		source.head = source.tail = nullptr;
	}
};

/** @brief Represents synchronous work which can run to completion off the issuer thread. */
struct work_request : request_link
{
	void (*execute)(work_request &) noexcept {};
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
	// Only Windows wait sets retain an operation-specific kernel handle.
	void *wait_handle{};
	bool close_after_wait{};
#endif
};

} // namespace fast_io::async::details
