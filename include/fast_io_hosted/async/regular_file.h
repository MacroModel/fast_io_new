#pragma once

namespace fast_io::async::details
{
/**
 * @brief Executes an ordinary file pair as one request, retaining the implicit offsets through EOF.
 * @details The calling coroutine owns this request and both observers until completion. A worker may
 * use the pipe only between submission and completion. An isolated root may perform the identical job
 * directly. Neither execution mode changes descriptor flags or uses the input size as an EOF boundary.
 */
class file_transfer_work : public work_request
{
	int input, output;
	unsigned capacity{};
	int ends[2]{-1, -1};
	bool pipe_empty{true};
	::std::size_t roots{};
	::std::uint_least64_t device{};

	/** @brief Restores optional range offload or byte copying at the two currently committed positions. */
	void copy_range(::fast_io::uintfpos_transmit_reference_wrapper count) noexcept
	{
		auto rest{::fast_io::details::linux_copy_regular_files(input, output)};
		transmit_integer_add_define(count, rest.progress.transmitted);
		if (rest.error)
		{
			result.error = {raise_posix_error, static_cast<unsigned>(rest.error)};
		}
		else
		{
			result.complete = true;
		}
	}

	io_context *issuer{};
	transfer_completion result{};
	bool inspect{}, use_pipe{}, empty_output{};

public:
	/** @brief Records the source and destination and defers descriptor inspection and placement until first await. */
	file_transfer_work(int source, int sink) noexcept
		: input(source), output(sink)
	{
		execute = [](work_request &item) noexcept {
			auto &work{static_cast<file_transfer_work &>(item)};
			// Let Linux place the first write to an empty file. A full overwrite batch can reuse
			// stable worker placement. The issuer never changes its own CPU mask.
			workers::place_file(work.empty_output ? 0u : work.roots);
			work.copy();
		};
	}
	file_transfer_work(file_transfer_work const &) = delete;
	file_transfer_work &operator=(file_transfer_work const &) = delete;

	/** @brief Returns the pipe after publication, closing a channel whose pending bytes could not be delivered. */
	~file_transfer_work()
	{
		if (ends[0] >= 0)
		{
			issuer->release_pipe(ends, capacity, pipe_empty);
		}
	}

	/** @brief Copies a verified regular pair until EOF, native failure, or an optional method's refusal. */
	void copy() noexcept
	{
		if (inspect)
		{
			// Other filesystems may implement range cloning or server-side copy.
			struct ::statfs fs;
			use_pipe = ::fast_io::system_call<__NR_fstatfs, int>(input, &fs) == 0 && fs.f_type == 0xef53;
		}
		if (use_pipe && capacity == 0)
		{
			// An empty issuer cache needs no synchronization in the worker. Publish the new pipe at completion.
			capacity = io_context::open_pipe(ends, io_context::splice_chunk);
		}
		auto total{result.progress.transmitted};
		::fast_io::uintfpos_transmit_reference_wrapper count{__builtin_addressof(total)};
		if (use_pipe && capacity != 0)
		{
			bool const send{roots > 1 || empty_output};
			unsigned chunk{capacity};
			if (send)
			{
				auto const limit{roots == 1 ? 256u * 1024u : 512u * 1024u};
				chunk = chunk > limit ? limit : chunk;
			}
			auto const exchanged{::fast_io::details::linux_copy_file_through_pipe(input, output, ends, chunk, send, count)};
			pipe_empty = exchanged.empty;
			result.complete = exchanged.complete;
			if (exchanged.error != 0)
			{
				result.error = {raise_posix_error, static_cast<unsigned>(exchanged.error)};
			}
		}
		if (!result.complete && result.error.raise == nullptr)
		{
			copy_range(count);
		}
		result.progress.transmitted = total;
	}

	/**
	 * @brief Declines ineligible descriptors or executes a proved standalone regular-file transfer on the issuer.
	 * @details Descriptor metadata is inspected only at first await. A nonempty regular source and a distinct
	 * regular destination may use a private pipe. The size selects a strategy and never supplies the EOF result.
	 */
	bool await_ready() noexcept
	{
		issuer = platform_context();
		::fast_io::details::linux_x86_64_stat from, to;
		if (issuer == nullptr || ::fast_io::details::linux_fstat(input, from) != 0 ||
			!::fast_io::details::linux_stat_is_regular(from) || from.st_size <= 0 ||
			::fast_io::details::linux_fstat(output, to) != 0 || !::fast_io::details::linux_stat_is_regular(to) ||
			(from.device == to.device && from.inode == to.inode))
		{
			return true;
		}
		roots = current_loop->root_count;
		device = from.device;
		if (from.st_size >= io_context::splice_chunk)
		{
			empty_output = to.st_size == 0;
			inspect = from.device == to.device && (from.device == 0 || issuer->file_strategy.device != from.device);
			use_pipe = from.device != to.device || issuer->file_strategy.use_pipe;
			if (inspect || use_pipe)
			{
				capacity = issuer->take_pipe(ends);
			}
		}
		if (current_loop->standalone_root && current_loop->ready.empty())
		{
			// This default, outermost wait has a single root and no runnable sibling or external pump.
			copy();
			return true;
		}
		return false;
	}

	/** @brief Transfers continuation ownership to an available worker. A declined submission leaves both cursors untouched. */
	bool await_suspend(::std::coroutine_handle<> handle) noexcept
	{
		continuation = handle;
		return issuer->submit(*this);
	}

	/** @brief Observes the published result only after the worker has left this frame. */
	transfer_completion await_resume() noexcept
	{
		if (inspect && (result.complete || result.error.raise != nullptr))
		{
			issuer->file_strategy = {device, use_pipe};
		}
		return result;
	}
};

} // namespace fast_io::async::details
