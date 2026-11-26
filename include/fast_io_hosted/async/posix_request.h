#pragma once

namespace fast_io::async::details
{

#if defined(FAST_IO_ASYNC_SPLICE)
struct posix_request;

/** @brief Keeps a soft-linked sequence alive until every accepted or canceled SQE reports a completion. */
struct splice_submission
{
	posix_request *entries;
	unsigned count{}, remaining{};
};
#endif

#if defined(FAST_IO_ASYNC_URING)
/** @brief Classifies the implicit write position of one borrowed open file description. */
enum class ring_write_position : unsigned char { unknown, stream, sequential };

/**
 * @brief Checks whether an implicit-offset write may use a ring CQE to advance its range.
 * @details Only ESPIPE establishes a stream. A seekable or unknown device uses write(2) so that a
 * short positive result commits its prefix and advances the shared offset together. The zero
 * displacement preserves the current position, including on an independently shared descriptor.
 */
inline ring_write_position classify_ring_write_position(int fd) noexcept
{
#if defined(__NR_lseek)
	auto const stream{::fast_io::system_call<__NR_lseek, ::std::intptr_t>(fd, 0, SEEK_CUR) == -ESPIPE};
#else
	auto const stream{::lseek(fd, 0, SEEK_CUR) == -1 && errno == ESPIPE};
#endif
	return stream ? ring_write_position::stream : ring_write_position::sequential;
}
#endif

/** @brief Holds a raw POSIX operation and the progress published by its backend. */
struct posix_request : work_request
{
	int fd;
	void const *buffer;
	::std::size_t size;
	bool write;
#if defined(FAST_IO_ASYNC_URING)
	ring_write_position position{ring_write_position::unknown};
#endif
#if defined(FAST_IO_ASYNC_SPLICE)
	int splice_source{-1};
	splice_submission *group{};
#endif
	::std::ptrdiff_t result{};
	posix_request *interest_next{};
	// A type-erased issuer callback records admission once. Workers never touch a coroutine's observation history.
	void (*admission)(posix_request &,bool) noexcept {};
	void admitted_on_issuer(bool wake=true) noexcept
	{
		if (admission) {auto notify{admission};admission=nullptr;notify(*this,wake);}
	}

#if defined(FAST_IO_ASYNC_SPLICE)
	/** @brief Reserves a request slot before a regular-file batch determines its two descriptors and length. */
	constexpr posix_request() noexcept : posix_request(-1, nullptr, 0, true)
	{}
#endif

	/** @brief Records a byte range whose lifetime extends until the completion is consumed. */
	constexpr posix_request(int descriptor, void const *data, ::std::size_t bytes, bool output) noexcept
		: fd(descriptor), buffer(data), size(bytes), write(output)
	{
		execute = [](work_request &item) noexcept { static_cast<posix_request &>(item).perform(); };
	}

	/** @brief Determines whether a readiness retry is safe without changing descriptor flags. */
	bool nonblocking() const noexcept
	{
#if defined(F_GETFL) && defined(O_NONBLOCK)
#if defined(__linux__) && defined(__NR_fcntl)
		auto const flags{::fast_io::system_call<__NR_fcntl, int>(fd, F_GETFL)};
#else
		auto const flags{::fcntl(fd, F_GETFL)};
#endif
		return flags >= 0 && (flags & O_NONBLOCK) != 0;
#else
		return false;
#endif
	}

#if defined(FAST_IO_ASYNC_URING)
	/**
	 * @brief Checks whether an implicit-offset ring operation can report every committed byte.
	 * @details A short buffered ring write can return a positive CQE without advancing a seekable
	 * file description. Retrying its suffix at that position overwrites the accepted prefix. A
	 * zero displacement classifies streams without moving the descriptor. Unknown and seekable
	 * writes use write(2), which commits its count and the shared position in the same operation.
	 */
	bool ring_offset_safe() const noexcept
	{
		return !write || position == ring_write_position::stream ||
			   (position == ring_write_position::unknown && classify_ring_write_position(fd) == ring_write_position::stream);
	}
#endif

	/** @brief Performs one raw request, retrying interruptions that transferred no bytes. */
	void perform() noexcept
	{
		do
		{
#if defined(FAST_IO_ASYNC_SPLICE)
			if (splice_source >= 0)
			{
				result = ::fast_io::system_call<__NR_splice, ::std::ptrdiff_t>(splice_source, nullptr, fd, nullptr, size, 0u);
				continue;
			}
#endif
#if defined(FAST_IO_ASYNC_LINUX)
			result = write ? ::fast_io::system_call<__NR_write, ::std::ptrdiff_t>(fd, buffer, size)
						   : ::fast_io::system_call<__NR_read, ::std::ptrdiff_t>(fd, buffer, size);
#else
			result = write ? ::write(fd, buffer, size) : ::read(fd, const_cast<void *>(buffer), size);
			if (result == -1)
			{
				result = -errno;
			}
#endif
		} while (result == -EINTR);
	}
};

} // namespace fast_io::async::details
