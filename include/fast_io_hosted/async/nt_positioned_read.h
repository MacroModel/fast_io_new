#pragma once

namespace fast_io
{

/** @brief Describes one positioned byte read and its completion length. */
struct positioned_read_request
{
	::std::byte *data{};
	::std::uint_least32_t size{};
	::fast_io::uintfpos_t offset{};
	::std::uint_least32_t transferred{};
};

/** @brief Identifies the transport used by the most recent positioned batch. */
enum class nt_positioned_backend : unsigned char
{
	event,
	completion_port,
	io_ring
};

namespace async::details
{

using nt_read_status = ::std::uint_least32_t;
inline constexpr ::std::size_t nt_positioned_batch_capacity{64u};
inline constexpr nt_read_status nt_read_pending{0x103u}, nt_read_eof{0xc0000011u};

/** @brief Owns a native handle whose teardown cannot change the reported read result. */
struct nt_positioned_handle
{
	void *value{};

	nt_positioned_handle() noexcept = default;
	nt_positioned_handle(nt_positioned_handle const &) = delete;
	nt_positioned_handle &operator=(nt_positioned_handle const &) = delete;

	~nt_positioned_handle()
	{
		if (value != nullptr)
			::fast_io::win32::nt::NtClose(value);
	}
};

/** @brief Commits one completion after the kernel has stopped using its destination. */
inline nt_read_status nt_finish_positioned_read(positioned_read_request &request, nt_read_status status,
	::std::uintptr_t bytes) noexcept
{
	if (status == nt_read_eof)
		return 0u;
	if (status != 0u)
		return status;
	if (request.size < bytes) [[unlikely]]
		::fast_io::fast_terminate();
	request.transferred = static_cast<::std::uint_least32_t>(bytes);
	return 0u;
}

/**
 * @brief Owns completion routing for a privately opened, overlapped NT file.
 * @details A borrowed or duplicated file object cannot satisfy this ownership condition.
 * Native skip-on-success completes cached synchronous returns directly. Only operations
 * that will post a packet enter the pending set. All packets are consumed before return.
 */
class nt_positioned_port
{
	using block = ::fast_io::win32::nt::io_status_block;
	struct packet { void *key, *token; block result; };
	struct association { void *port, *key; };
	using create_fn = nt_read_status (FAST_IO_WINSTDCALL *)(void **, nt_read_status, void *, nt_read_status) noexcept;
	using remove_fn = nt_read_status (FAST_IO_WINSTDCALL *)(void *, void **, void **, block *,
		::std::int_least64_t const *) noexcept;
	using remove_many_fn = nt_read_status (FAST_IO_WINSTDCALL *)(void *, packet *, nt_read_status,
		nt_read_status *, ::std::int_least64_t const *, unsigned char) noexcept;

	nt_positioned_handle port;
	remove_fn remove_one{};
	remove_many_fn remove_many{};
	bool tried{};
	bool skip_success{};

public:
	/** @brief Associates the exclusive file object only after the required NT exports are available. */
	bool open(void *file) noexcept
	{
		using namespace ::fast_io::win32::nt;
		namespace loader = ::fast_io::win32::nt::details;
		if (tried)
			return port.value != nullptr;
		tried = true;
		auto *module{loader::async_native_library()};
		if (module == nullptr)
			return false;
		auto create{loader::async_native_symbol<create_fn>(module, "NtCreateIoCompletion")};
		remove_many = loader::async_native_symbol<remove_many_fn>(module, "NtRemoveIoCompletionEx");
		if (remove_many == nullptr)
			remove_one = loader::async_native_symbol<remove_fn>(module, "NtRemoveIoCompletion");
		if (create == nullptr || (remove_many == nullptr && remove_one == nullptr))
			return false;

		nt_positioned_handle candidate;
		if (create(&candidate.value, 0x100003u, nullptr, 1u) != 0u)
			return false;
		association target{candidate.value, this};
		block result;
		if (NtSetInformationFile(file, &result, &target, sizeof(target),
			file_information_class::FileCompletionInformation) != 0u)
			return false;
		port.value = candidate.value;
		candidate.value = nullptr;

		// This information class is optional on older NT releases.
		nt_read_status flags{3u}; // FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE
		skip_success = NtSetInformationFile(file, &result, &flags, sizeof(flags),
			file_information_class::FileIoCompletionNotificationInformation) == 0u;
		return true;
	}

	/** @brief Issues explicit offsets and retires the accepted native requests in completion order. */
	void read(void *file, ::std::span<positioned_read_request> requests, nt_read_status *failures) const noexcept
	{
		using ::fast_io::win32::nt::NtReadFile;
		block status_blocks[nt_positioned_batch_capacity];
		::std::uint_least64_t pending_slots{};
		nt_read_status pending{};
		for (::std::size_t i{}; i != requests.size(); ++i)
		{
			auto &request{requests[i]};
			if (request.size == 0u)
				continue;
			::std::int_least64_t offset{static_cast<::std::int_least64_t>(request.offset)};
			auto &status_block{status_blocks[i]};
			auto *token{reinterpret_cast<void *>(static_cast<::std::uintptr_t>(i + 1u))};
			auto const status{NtReadFile(file, nullptr, nullptr, token, &status_block, request.data,
				request.size, &offset, nullptr)};
			if (status == nt_read_pending || (status == 0u && !skip_success))
			{
				pending_slots |= ::std::uint_least64_t{1u} << i;
				++pending;
			}
			else
				failures[i] = nt_finish_positioned_read(request, status, status == 0u ? status_block.Information : 0u);
		}

		packet packets[nt_positioned_batch_capacity];
		while (pending != 0u)
		{
			nt_read_status removed{1u}, result;
			if (remove_many != nullptr)
				result = remove_many(port.value, packets, pending, &removed, nullptr, 0u);
			else
				result = remove_one(port.value, &packets[0].key, &packets[0].token, &packets[0].result, nullptr);
			if (result != 0u || removed == 0u || pending < removed) [[unlikely]]
				::fast_io::fast_terminate();
			for (nt_read_status i{}; i != removed; ++i)
			{
				auto const &item{packets[i]};
				auto const slot{reinterpret_cast<::std::uintptr_t>(item.token) - 1u};
				if (item.key != this || requests.size() <= slot) [[unlikely]]
					::fast_io::fast_terminate();
				auto const bit{::std::uint_least64_t{1u} << slot};
				if ((pending_slots & bit) == 0u) [[unlikely]]
					::fast_io::fast_terminate();
				pending_slots ^= bit;
				failures[slot] = nt_finish_positioned_read(requests[slot], item.result.Status, item.result.Information);
			}
			pending -= removed;
		}
	}
};

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(FAST_IO_DISABLE_NT_IORING)
/**
 * @brief Uses the x64 version-3 NT I/O ring ABI verified on Windows build 26100.
 * @details The runtime verifies the exports, layout version, extents and masks before
 * reading the mapped queues. An unsupported layout declines before any read. Publishing
 * the tail transfers ownership of the destinations to the kernel until all CQEs retire.
 */
class nt_positioned_ring
{
	using u32 = ::std::uint_least32_t;
	struct parameters { u32 version, submission, completion, required, advisory; };
	struct mapping
	{
		u32 version, required, advisory, submission, submission_mask, completion, completion_mask;
		::std::byte *send, *receive;
	};
	struct sqe
	{
		u32 opcode, flags;
		::std::uintptr_t token;
		u32 kinds, padding;
		void *file;
		::std::byte *data;
		::std::int_least64_t offset;
		u32 size, io_flags;
		::std::byte reserved[8];
	};
	struct cqe { ::std::uintptr_t token; u32 status, padding; ::std::uintptr_t count; };
	static_assert(sizeof(parameters) == 20u && sizeof(mapping) == 48u);
	static_assert(sizeof(sqe) == 64u && sizeof(cqe) == 24u);

	nt_positioned_handle ring;
	nt_positioned_handle input;
	mapping queues{};
	u32 tail{}, head{};
	bool tried{};

public:
	/**
	 * @brief Requests the known native queue layout and reopens the same file for Ring completions.
	 * @details An empty relative name reopens the file identified by the existing handle. This gives
	 * the Ring a separate file object with no port association, without resolving the original path
	 * again. A filesystem that cannot supply a separate open declines the Ring before submission.
	 */
	bool open(void *source) noexcept
	{
		using namespace ::fast_io::win32::nt;
		if (tried)
			return ring.value != nullptr;
		tried = true;
		auto const &api{::fast_io::win32::nt::details::get_async_native_exports()};
		if (api.build != 26100u || api.create == nullptr || api.query == nullptr || api.submit == nullptr)
			return false;
		constexpr u32 version{300u}, capacity{nt_positioned_batch_capacity};
		u32 capabilities[5]{};
		if (NtQueryIoRingCapabilities(sizeof(capabilities), capabilities) != 0u ||
			capabilities[0] < version || capabilities[3] < capacity || capabilities[4] < 2u * capacity)
			return false;
		parameters request{version, capacity, 2u * capacity, 0u, 0u};
		nt_positioned_handle candidate;
		if (NtCreateIoRing(&candidate.value, sizeof(request), &request, sizeof(queues), &queues) != 0u ||
			queues.version != version || queues.submission != capacity || queues.completion != 2u * capacity ||
			queues.submission_mask != capacity - 1u || queues.completion_mask != 2u * capacity - 1u ||
			queues.send == nullptr || queues.receive == nullptr)
			return false;
		char16_t empty{};
		unicode_string name{0u, sizeof(empty), &empty};
		object_attributes attributes{sizeof(object_attributes), source, &name, 0u, nullptr, nullptr};
		io_status_block result;
		if (NtCreateFile(&input.value, 0x120089u /* FILE_GENERIC_READ */, &attributes, &result, nullptr,
			0u, 7u /* share read, write, delete */, 1u /* FILE_OPEN */,
			0x840u /* FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS */, nullptr, 0u) != 0u)
			return false;
		ring.value = candidate.value;
		candidate.value = nullptr;
		return true;
	}

	/** @brief Publishes one batch with one kernel entry, then retires all completed destinations. */
	void read(::std::span<positioned_read_request> requests, u32 count, nt_read_status *failures) noexcept
	{
		using ::fast_io::win32::nt::NtSubmitIoRing;
		auto *send{reinterpret_cast<u32 *>(queues.send)};
		auto *receive{reinterpret_cast<u32 *>(queues.receive)};
		auto *submissions{reinterpret_cast<sqe *>(queues.send + 16u)};
		auto const *completions{reinterpret_cast<cqe const *>(queues.receive + 8u)};
		::std::uint_least64_t pending{};
		for (::std::size_t i{}; i != requests.size(); ++i)
		{
			auto const &request{requests[i]};
			if (request.size == 0u)
				continue;
			submissions[tail++ & queues.submission_mask] = {1u, 0u, i, 0u, 0u, input.value, request.data,
				static_cast<::std::int_least64_t>(request.offset), request.size, 0u, {}};
			pending |= ::std::uint_least64_t{1u} << i;
		}
		::std::atomic_ref<u32>(send[1]).store(tail, ::std::memory_order_release);
		if (NtSubmitIoRing(ring.value, 0u, count, nullptr) != 0u ||
			::std::atomic_ref<u32>(receive[1]).load(::std::memory_order_acquire) - head != count) [[unlikely]]
			::fast_io::fast_terminate();
		for (; count != 0u; --count)
		{
			auto const &item{completions[head++ & queues.completion_mask]};
			if (requests.size() <= item.token) [[unlikely]]
				::fast_io::fast_terminate();
			auto const bit{::std::uint_least64_t{1u} << item.token};
			if ((pending & bit) == 0u) [[unlikely]]
				::fast_io::fast_terminate();
			pending ^= bit;
			failures[item.token] = nt_finish_positioned_read(requests[item.token], item.status, item.count);
		}
		::std::atomic_ref<u32>(receive[0]).store(head, ::std::memory_order_release);
	}
};
#endif

/** @brief Reads through a private synchronization event when a completion port cannot be opened. */
inline void nt_read_positioned_with_event(void *file, ::std::span<positioned_read_request> requests,
	nt_read_status *failures) noexcept
{
	using namespace ::fast_io::win32::nt;
	nt_positioned_handle event;
	auto const created{NtCreateEvent(&event.value, 0x100002u, nullptr, event_type::SynchronizationEvent, 0)};
	for (::std::size_t i{}; i != requests.size(); ++i)
	{
		auto &request{requests[i]};
		if (request.size == 0u)
			continue;
		auto status{created};
		io_status_block result;
		if (status == 0u)
		{
			::std::int_least64_t offset{static_cast<::std::int_least64_t>(request.offset)};
			status = NtReadFile(file, event.value, nullptr, nullptr, &result, request.data, request.size, &offset, nullptr);
			if (status == nt_read_pending)
			{
				if (NtWaitForSingleObject(event.value, 0, nullptr) != 0u) [[unlikely]]
					::fast_io::fast_terminate();
				status = result.Status;
			}
		}
		failures[i] = nt_finish_positioned_read(request, status, status == 0u ? result.Information : 0u);
	}
}

} // namespace async::details

/**
 * @brief Owns an NT file with exclusive completion routing and independent positioned reads.
 * @details Open a regular file by its path. Requests use non-negative explicit byte offsets on an
 * overlapped file object. The object and its destination ranges are exclusively borrowed until
 * completion. Different objects have independent completion queues. No handle projection permits
 * outside code to change port association, notification flags, or the lifetime of an accepted read.
 */
class nt_positioned_file
{
	using status = ::fast_io::async::details::nt_read_status;
	basic_nt_file<char> file;
	::fast_io::async::details::nt_positioned_port port;
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(FAST_IO_DISABLE_NT_IORING)
	::fast_io::async::details::nt_positioned_ring ring;
#endif
	nt_positioned_backend recent{};

public:
	using input_char_type = char;

	/** @brief Opens its own file object with NT's overlapped random-access mode. */
	template <typename Path>
	explicit nt_positioned_file(Path const &path)
		: file(path, open_mode::in | open_mode::no_block | open_mode::random_access | open_mode::shared_delete)
	{}

	nt_positioned_file(nt_positioned_file const &) = delete;
	nt_positioned_file &operator=(nt_positioned_file const &) = delete;

	/** @brief Exposes the positioned endpoint to annotation binding without projecting a raw handle. */
	friend constexpr nt_positioned_file &input_stream_ref_define(nt_positioned_file &source) noexcept
	{
		return source;
	}

	/** @brief Certifies one native completion observation point for an annotated positioned file. */
	friend constexpr ::std::true_type async_kernel_input_define(io_type_t<nt_positioned_file>) noexcept
	{
		return {};
	}

	/** @brief Reports which transport actually admitted the last nonempty batch. */
	[[nodiscard]] nt_positioned_backend last_backend() const noexcept { return recent; }

private:
	/** @brief Selects a transport from the validated request shape before admitting any buffers. */
	status read(::std::span<positioned_read_request> requests) noexcept
	{
		using namespace ::fast_io::async::details;
		constexpr status invalid_parameter{0xc000000du};
		if (requests.size() > nt_positioned_batch_capacity)
			return invalid_parameter;
		status count{}, largest{};
		for (auto const &request : requests)
		{
			if (request.size == 0u)
				continue;
			if (request.data == nullptr || request.offset > static_cast<::fast_io::uintfpos_t>(INT64_MAX) - request.size)
				return invalid_parameter;
			++count;
			if (largest < request.size)
				largest = request.size;
		}
		for (auto &request : requests)
			request.transferred = 0u;
		if (count == 0u)
			return 0u;
		status failures[nt_positioned_batch_capacity]{};
		auto *handle{file.native_handle()};
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(FAST_IO_DISABLE_NT_IORING)
		// A deep group of page-sized requests amortizes submission. Larger or shallower batches use IOCP.
		if (count >= 32u && largest <= 4096u && ring.open(handle))
		{
			recent = nt_positioned_backend::io_ring;
			ring.read(requests, count, failures);
		}
		else
#endif
		if (port.open(handle))
		{
			recent = nt_positioned_backend::completion_port;
			port.read(handle, requests, failures);
		}
		else
		{
			recent = nt_positioned_backend::event;
			nt_read_positioned_with_event(handle, requests, failures);
		}
		for (::std::size_t i{}; i != requests.size(); ++i)
			if (failures[i] != 0u)
				return failures[i];
		return 0u;
	}

	/** @brief Keeps one native batch alive through the existing async scheduler. */
	struct batch_work : ::fast_io::async::details::work_request
	{
		nt_positioned_file &source;
		::std::span<positioned_read_request> requests;
		::fast_io::async::details::io_context *issuer{};
		status result{};

		batch_work(nt_positioned_file &input, ::std::span<positioned_read_request> spans) noexcept
			: source(input), requests(spans)
		{
			execute = [](work_request &work) noexcept {
				auto &self{static_cast<batch_work &>(work)};
				self.result = self.source.read(self.requests);
			};
		}

		/** @brief Completes an isolated batch inline so cached reads need no extra thread. */
		bool await_ready() noexcept
		{
			using namespace ::fast_io::async::details;
			issuer = platform_context();
			if (issuer != nullptr && !(current_loop->standalone_root && current_loop->ready.empty()))
				return false;
			result = source.read(requests);
			return true;
		}

		/** @brief Lets the existing issuer make progress on other work while this batch joins. */
		bool await_suspend(::std::coroutine_handle<> handle) noexcept
		{
			continuation = handle;
			if (issuer->submit(static_cast<work_request &>(*this)))
				return true;
			result = source.read(requests);
			return false;
		}

		/** @brief Returns the error only after destination ownership has returned to the issuer. */
		status await_resume() const noexcept { return result; }
	};

	/** @brief Joins every admitted request before propagating its first error in request order. */
	friend ::fast_io::async::task<void> async_pread_batch_define(nt_positioned_file &source,
		::std::span<positioned_read_request> requests)
	{
		using namespace ::fast_io::async::details;
		auto const failed{co_await batch_work{source, requests}};
		if (failed != 0u)
			co_await store_native_error{{raise_nt_status, failed}};
	}
};

inline namespace io
{

/**
 * @brief Reads up to 64 independent ranges from the one marked positioned endpoint.
 * @details Each nonempty destination must stay live and disjoint from the other destinations
 * and request metadata until completion. EOF publishes zero bytes, and a final fragment
 * publishes its actual length. An error is delivered after every submitted read has retired.
 * The same file may be used for another batch once this one completes.
 */
template <typename Source>
	requires ::fast_io::async::annotated_operand<Source> &&
		requires(Source &&source, ::std::span<positioned_read_request> requests) {
			{ async_pread_batch_define(::fast_io::async::details::value_operand(static_cast<Source &&>(source)), requests) }
				-> ::std::same_as<::fast_io::async::task<void>>;
		}
[[nodiscard]] inline auto pread_batch(Source &&source, ::std::span<positioned_read_request> requests)
{
	return ::fast_io::async::details::make_io_operation([&] {
		return async_pread_batch_define(::fast_io::async::details::value_operand(static_cast<Source &&>(source)), requests);
	});
}

} // namespace io
} // namespace fast_io
