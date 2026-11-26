#pragma once

namespace fast_io::async::details
{

#if defined(FAST_IO_ASYNC_POSIX)
/** @brief Raises a POSIX completion error at the consumer's exception boundary. */
[[noreturn]] inline void raise_posix_error(::std::uint_least32_t code) FAST_IO_ASYNC_THROWS
{
#if defined(__HERBCEPTIONS__)
	throw throws static_cast<::std::errc>(code);
#else
	::fast_io::throw_posix_error(static_cast<int>(code));
#endif
}

/** @brief Suspends the issuer for one native sequential read or write. */
struct posix_awaiter : posix_request
{
	using posix_request::posix_request;

#if defined(FAST_IO_ASYNC_URING)
	/** @brief Borrows the write position established by the enclosing operation. */
	constexpr posix_awaiter(int descriptor, void const *data, ::std::size_t bytes, ring_write_position proof) noexcept
		: posix_request(descriptor, data, bytes, true)
	{
		position = proof;
	}
#endif

	/** @brief Completes an empty range without submitting it. */
	constexpr bool await_ready() const noexcept
	{
		return size == 0;
	}

	/** @brief Submits the prepared request and retains the continuation that consumes its byte count. */
	bool submit_request(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		if (auto *context = platform_context())
		{
			return context->submit(*this);
		}
		// A foreign coroutine may consume the task without installing a fast_io loop.
		perform();
		return false;
	}

	/** @brief Delivers ordinary admission and completion to the coroutine awaiting this request. */
	template <typename Promise>
	bool await_suspend(::std::coroutine_handle<Promise> handle) noexcept
	{
		if constexpr (requires(Promise &p) { p.history; p.observers; })
			admission=[](posix_request &r,bool wake) noexcept {
				auto h{::std::coroutine_handle<Promise>::from_address(r.continuation.address())};
				native_admitted(h.promise(),r.write,wake);
			};
		return submit_request(handle);
	}

	/** @brief Returns byte progress and retains errno without throwing inside the coroutine. */
	byte_completion await_resume() noexcept
	{
		// A blocking worker can report the syscall only when its completion reaches this issuer.
		if (size) admitted_on_issuer(false);
		if (result < 0) [[unlikely]]
		{
			return {0u, {raise_posix_error, static_cast<::std::uint_least32_t>(-result)}};
		}
		return {static_cast<::std::size_t>(result), {}};
	}
};

/** @brief Relays a prefix request's admission to its public operation on the issuer thread.
 * @details The writer resumes through the ordinary device continuation. Its parent keeps the promise
 * and borrowed bytes alive through every short write. Only a delegated request carries this pointer.
 */
struct prefix_posix_awaiter : posix_awaiter
{
	task<void>::promise_type *owner;
	bool run_ahead;

	/** @brief Wakes admission subscribers on the record even while its writer is blocked on the descriptor. */
	bool await_suspend(::std::coroutine_handle<> writer) noexcept
	{
		admission = [](posix_request &r, bool wake) noexcept {
			native_admitted(*static_cast<prefix_posix_awaiter &>(r).owner, r.write, wake);
		};
		auto suspended{submit_request(writer)};
#if defined(FAST_IO_ASYNC_URING)
		// Only a genuine wait on the first prefix competes with work remaining on this issuer.
		if (suspended && run_ahead) platform_context()->launch_prefix();
#endif
		return suspended;
	}
};

/** @brief Binds the actual POSIX admission receipt to the record whose first bytes this writer submits. */
inline prefix_posix_awaiter async_forward_admission_define(posix_awaiter request, task<void>::promise_type &owner, bool run_ahead) noexcept
{
	return {request, ::fast_io::freestanding::addressof(owner), run_ahead};
}

#if defined(FAST_IO_ASYNC_URING)
/** @brief Retains one descriptor's write-position classification until the transfer releases it. */
struct posix_output_sequence
{
	int fd;
	ring_write_position position{ring_write_position::unknown};
};

/** @brief Classifies the first nonempty ring write and reuses its result across subsequent completions. */
inline posix_awaiter async_write_some_define(posix_output_sequence &out, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	if (out.position == ring_write_position::unknown && bytes != 0)
	{
		auto *issuer{platform_context()};
		out.position = issuer != nullptr && issuer->has_ring() ? classify_ring_write_position(out.fd) : ring_write_position::sequential;
	}
	return {out.fd, data, bytes, out.position};
}
#endif
#endif

#if defined(FAST_IO_ASYNC_NT)
/** @brief Presents one native NT operation to the coroutine completion protocol. */
struct nt_awaiter : nt_request
{
	using nt_request::nt_request;

	/** @brief Completes empty ranges without touching the handle. */
	constexpr bool await_ready() const noexcept
	{
		return size == 0;
	}

	/** @brief Submits the native event or performs the operation at a foreign coroutine boundary. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		if (auto *context = platform_context())
		{
			return context->submit(*this);
		}
		perform_blocking();
		return false;
	}

	/** @brief Converts the final IO_STATUS_BLOCK to byte progress, EOF, or a native error value. */
	byte_completion await_resume() const noexcept
	{
		auto const result{status == 0x103u ? block.Status : status};
		if (result == 0)
		{
			return {block.Information, {}};
		}
		if (!write && (result == 0xc0000011u || result == 0xc000014bu))
		{
			return {0u, {}};
		}
		return {0u, {raise_nt_status, result}};
	}
};

/**
 * @brief Borrows a live NT file for one multi-request operation.
 * @details The mode describes the file object, so its synchronous category
 * remains valid while the handle stays open. Only successful classification
 * is reused. Opaque handles retain per-request classification and worker
 * fallback. No state is indexed by a handle value across borrows.
 */
struct nt_io_sequence
{
	void *file;
	nt_open_mode mode{nt_open_mode::unqueried};

	/** @brief Obtains the mode only when the first nonempty request can use it. */
	nt_open_mode classify(::std::size_t bytes) noexcept
	{
		if (mode == nt_open_mode::unqueried && bytes != 0u) [[unlikely]]
			return classify_first();
		return mode;
	}

private:
	/** @brief Keeps the first native mode query out of subsequent coroutine admissions. */
#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	nt_open_mode classify_first() noexcept
	{
		auto const claim{classify_nt_open_mode(file)};
		if (claim != nt_open_mode::opaque)
			mode = claim;
		return claim;
	}
};

/** @brief Retains the read buffer until this sequenced operation completes. */
inline nt_awaiter async_read_some_define(nt_io_sequence &input, ::std::byte *data, ::std::size_t bytes) noexcept
{
	return {input.file, data, bytes, false, input.classify(bytes)};
}

/** @brief Retains the write buffer until this sequenced operation completes. */
inline nt_awaiter async_write_some_define(nt_io_sequence &output, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	return {output.file, data, bytes, true, output.classify(bytes)};
}
#endif

} // namespace fast_io::async::details

#if defined(FAST_IO_ASYNC_POSIX)
namespace fast_io
{

/** @brief Advertises the raw POSIX input as a distinct kernel transport for file-selected ADL binding. */
template <posix_family Family, ::std::integral Char>
inline constexpr auto async_input_levels_define(io_type_t<basic_posix_family_io_observer<Family,Char>>) noexcept
{
	using file = basic_posix_family_io_observer<Family,Char>;
	return async::file_capabilities<file,true,async::file_points<async::point_kind::transport>,true>{};
}

/** @brief Advertises the raw POSIX output as a distinct kernel transport for file-selected ADL binding. */
template <posix_family Family, ::std::integral Char>
inline constexpr auto async_output_levels_define(io_type_t<basic_posix_family_io_observer<Family,Char>>) noexcept
{
	using file = basic_posix_family_io_observer<Family,Char>;
	return async::file_capabilities<file,false,async::file_points<async::point_kind::transport>,true>{};
}

/** @brief A POSIX descriptor accepts already encoded physical bytes and reports completion through its native request. */
template <posix_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_kernel_output_define(io_type_t<basic_posix_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

/** @brief Declares a physical kernel input stage for the raw positive/EOF descriptor protocol. */
template <posix_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_kernel_input_define(io_type_t<basic_posix_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

#if defined(FAST_IO_ASYNC_URING)
/** @brief Borrows the current POSIX output description for repeated writes within one transmit. */
template <posix_family Family, ::std::integral Char>
inline constexpr auto async_output_sequence_define(basic_posix_family_io_observer<Family, Char> out) noexcept
{
	return ::fast_io::async::details::posix_output_sequence{out.native_handle()};
}
#endif

/** @brief Adapts raw POSIX input to a non-throwing native completion request. */
template <posix_family Family, ::std::integral Char>
inline constexpr auto async_read_some_define(basic_posix_family_io_observer<Family, Char> input, ::std::byte *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::posix_awaiter{input.native_handle(), data, bytes, false};
}

/** @brief Adapts raw POSIX output to a non-throwing native completion request. */
template <posix_family Family, ::std::integral Char>
inline constexpr auto async_write_some_define(basic_posix_family_io_observer<Family, Char> output, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::posix_awaiter{output.native_handle(), data, bytes, true};
}

#if defined(FAST_IO_ASYNC_FILE_WORKER)
#if defined(__NR_fcntl) && defined(F_GETFL) && defined(O_APPEND) && defined(O_DIRECT)
namespace details
{
template <typename T>
inline constexpr bool plain_posix_tie_leaf = false;
template <posix_family Family, ::std::integral Char>
inline constexpr bool plain_posix_tie_leaf<basic_posix_family_io_observer<Family, Char>> = true;
template <posix_family Family, ::std::integral Char>
inline constexpr bool plain_posix_tie_leaf<basic_posix_family_file<Family, Char>> = true;
template <typename Selection, typename Operand>
inline constexpr bool plain_posix_tie_leaf<::fast_io::async::basic_async<Selection, Operand>> =
	plain_posix_tie_leaf<::std::remove_cvref_t<Operand>>;
}

/** @brief The original POSIX owner, observer, or its built-in annotation has no empty tie work. */
template <typename T>
	requires details::plain_posix_tie_leaf<T>
inline constexpr ::std::true_type async_empty_output_tie_define(io_type_t<T>) noexcept
{
	return {};
}

/** @brief Linux workers and the ring return write completions to the submitting event loop. */
template <posix_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_output_prefix_on_issuer_define(io_type_t<basic_posix_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

/**
 * @brief Permits incremental formatting into an ordinary sequential ext-family or tmpfs file.
 * @details A descriptor can also designate a message socket or a pseudo-file with command-sized writes.
 * The first full sealed window probes the live descriptor once per operation. Failed or unavailable
 * probes retain whole-record staging. Append and direct I/O preserve their original write boundaries.
 */
template <posix_family Family, ::std::integral Char>
inline bool async_output_accepts_prefixes_define(basic_posix_family_io_observer<Family, Char> out) noexcept
{
	::fast_io::details::linux_x86_64_stat file;
	if (::fast_io::details::linux_fstat(out.fd, file) || !::fast_io::details::linux_stat_is_regular(file)) return false;
	auto const flags{::fast_io::system_call<__NR_fcntl, int>(out.fd, F_GETFL)};
	if (flags < 0 || (flags & (O_APPEND | O_DIRECT)) != 0) return false;
	struct ::statfs fs;
	return ::fast_io::system_call<__NR_fstatfs, int>(out.fd, &fs) == 0 && (fs.f_type == 0xef53 || fs.f_type == 0x01021994);
}
#endif

#if defined(__NR_fcntl) && defined(F_GETFL) && defined(F_GET_SEALS) && defined(F_SEAL_SEAL) && \
	defined(O_ACCMODE) && defined(O_WRONLY) && defined(O_RDWR) && defined(O_APPEND) && defined(O_DIRECT) && \
	defined(O_SYNC) && defined(O_DSYNC)
/**
 * @brief Allows coalesced transfer writes to an anonymous tmpfs file with immutable harmless seals.
 * @details A grow seal can reject a crossing write before the writable prefix. Checking the fixed
 * seal set keeps that result, append, direct I/O and per-write synchronization on the ordinary route.
 */
template <posix_family Family, ::std::integral Char>
inline bool async_output_can_coalesce_transmit_define(basic_posix_family_io_observer<Family, Char> out) noexcept
{
	::fast_io::details::linux_x86_64_stat file;
	if (::fast_io::details::linux_fstat(out.fd, file) || !::fast_io::details::linux_stat_is_regular(file) ||
		file.links != 0)
		return false;
	auto const flags{::fast_io::system_call<__NR_fcntl, int>(out.fd, F_GETFL)};
	if (flags < 0 || ((flags & O_ACCMODE) != O_WRONLY && (flags & O_ACCMODE) != O_RDWR) ||
		(flags & (O_APPEND | O_DIRECT | O_SYNC | O_DSYNC)) != 0)
		return false;
	if (::fast_io::system_call<__NR_fcntl, int>(out.fd, F_GET_SEALS) != F_SEAL_SEAL)
		return false;
	struct ::statfs fs;
	return ::fast_io::system_call<__NR_fstatfs, int>(out.fd, &fs) == 0 && fs.f_type == 0x01021994;
}

/** @brief Writes an eligible coalesced transfer window and reports the accepted prefix. */
template <posix_family Family, ::std::integral Char>
inline ::fast_io::async::byte_completion async_write_transmit_window_define(
	basic_posix_family_io_observer<Family, Char> out, ::std::byte const *first, ::std::size_t bytes) noexcept
{
	// The issuer has just checked the concrete open file description.
	::fast_io::async::byte_completion done{};
	while (done.transferred != bytes)
	{
		::fast_io::async::details::posix_request write{out.fd, first + done.transferred, bytes - done.transferred, true};
		write.perform();
		if (write.result <= 0) [[unlikely]]
		{
			done.error = {::fast_io::async::details::raise_posix_error, static_cast<unsigned>(write.result ? -write.result : EIO)};
			break;
		}
		done.transferred += static_cast<::std::size_t>(write.result);
	}
	return done;
}
#endif
/**
 * @brief Writes an isolated regular file's contiguous range using its shared file position.
 * @details Descriptor numbers can be reused between calls. Inspect the current descriptor each time.
 * A stream, an unknown file type or unavailable metadata returns to normal asynchronous submission.
 * A regular file uses write(2) through short completions, preserving the bytes accepted before failure.
 */
template <posix_family Family, ::std::integral Char>
inline bool async_write_range_on_issuer_define(basic_posix_family_io_observer<Family, Char> out,
											  Char const *first, Char const *last, ::fast_io::async::native_error &failure) noexcept
{
	if (first == last)
	{
		return true;
	}
	::fast_io::details::linux_x86_64_stat info;
	if (::fast_io::details::linux_fstat(out.fd, info) || !::fast_io::details::linux_stat_is_regular(info))
	{
		return false;
	}
	auto data{reinterpret_cast<::std::byte const *>(first)};
	auto remaining{static_cast<::std::size_t>(last - first) * sizeof(Char)};
	do
	{
		::fast_io::async::details::posix_request write{out.fd, data, remaining, true};
		write.perform();
		if (write.result <= 0) [[unlikely]]
		{
			failure = {::fast_io::async::details::raise_posix_error, static_cast<unsigned>(write.result ? -write.result : EIO)};
			break;
		}
		data += write.result;
		remaining -= static_cast<::std::size_t>(write.result);
	} while (remaining);
	return true;
}

/** @brief Commits a staged record with the ordinary sequential-file range semantics. */
template <posix_family Family, ::std::integral Char>
inline bool async_write_record_on_issuer_define(basic_posix_family_io_observer<Family, Char> out,
	Char const *first, Char const *last, ::fast_io::async::native_error &failure) noexcept
{
	return async_write_range_on_issuer_define(out, first, last, failure);
}

#endif

} // namespace fast_io
#endif

#if defined(FAST_IO_ASYNC_NT)
namespace fast_io
{

/** @brief Requires an NT buffer miss to retain its range through a possible STATUS_PENDING. */
template <nt_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_native_buffer_completion_define(
	io_type_t<basic_nt_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

/** @brief Preserves the same completion requirement through a Win32 handle observer. */
template <win32_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_native_buffer_completion_define(
	io_type_t<basic_win32_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

/** @brief Preserves the NT completion requirement through a borrowed CRT descriptor. */
template <posix_family Family, ::std::integral Char>
inline constexpr ::std::true_type async_native_buffer_completion_define(
	io_type_t<basic_posix_family_io_observer<Family, Char>>) noexcept
{
	return {};
}

/** @brief Submits a sequential NT read without crossing the Win32 file API. */
template <nt_family Family, ::std::integral Char>
inline auto async_read_some_define(basic_nt_family_io_observer<Family, Char> input, ::std::byte *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::nt_awaiter{input.native_handle(), data, bytes, false};
}

/** @brief Submits a sequential NT write without crossing the Win32 file API. */
template <nt_family Family, ::std::integral Char>
inline auto async_write_some_define(basic_nt_family_io_observer<Family, Char> output, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::nt_awaiter{output.native_handle(), data, bytes, true};
}

/** @brief Uses the native NT completion protocol for a Win32 file observer on the NT family. */
template <win32_family Family, ::std::integral Char>
inline auto async_read_some_define(basic_win32_family_io_observer<Family, Char> stream, ::std::byte *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::nt_awaiter{stream.native_handle(), data, bytes, false};
}

/** @brief Borrows the NT handle behind a CRT file descriptor for a native read operation. */
template <posix_family Family, ::std::integral Char>
inline auto async_read_some_define(basic_posix_family_io_observer<Family, Char> stream, ::std::byte *data, ::std::size_t bytes) noexcept
{
	return async_read_some_define(static_cast<basic_nt_family_io_observer<nt_family::nt, Char>>(stream), data, bytes);
}

/** @brief Uses the native NT completion protocol for a Win32 file observer on the NT family. */
template <win32_family Family, ::std::integral Char>
inline auto async_write_some_define(basic_win32_family_io_observer<Family, Char> stream, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	return ::fast_io::async::details::nt_awaiter{stream.native_handle(), data, bytes, true};
}

/** @brief Borrows the NT handle behind a CRT file descriptor for a native write operation. */
template <posix_family Family, ::std::integral Char>
inline auto async_write_some_define(basic_posix_family_io_observer<Family, Char> stream, ::std::byte const *data, ::std::size_t bytes) noexcept
{
	return async_write_some_define(static_cast<basic_nt_family_io_observer<nt_family::nt, Char>>(stream), data, bytes);
}

/** @brief Borrows this NT handle for a single asynchronous input sequence. */
template <nt_family Family, ::std::integral Char>
inline constexpr auto async_input_sequence_define(basic_nt_family_io_observer<Family, Char> stream) noexcept
{
	return ::fast_io::async::details::nt_io_sequence{stream.native_handle()};
}

/** @brief Borrows this NT handle for a single asynchronous output sequence. */
template <nt_family Family, ::std::integral Char>
inline constexpr auto async_output_sequence_define(basic_nt_family_io_observer<Family, Char> stream) noexcept
{
	return ::fast_io::async::details::nt_io_sequence{stream.native_handle()};
}

/** @brief Borrows this WIN32 handle for a single asynchronous input sequence. */
template <win32_family Family, ::std::integral Char>
inline constexpr auto async_input_sequence_define(basic_win32_family_io_observer<Family, Char> stream) noexcept
{
	return ::fast_io::async::details::nt_io_sequence{stream.native_handle()};
}

/** @brief Borrows this WIN32 handle for a single asynchronous output sequence. */
template <win32_family Family, ::std::integral Char>
inline constexpr auto async_output_sequence_define(basic_win32_family_io_observer<Family, Char> stream) noexcept
{
	return ::fast_io::async::details::nt_io_sequence{stream.native_handle()};
}

} // namespace fast_io
#endif
