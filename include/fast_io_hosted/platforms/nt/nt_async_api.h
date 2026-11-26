#pragma once

namespace fast_io::win32::nt
{

/** @brief Identifies Windows NT generations that can share an asynchronous I/O implementation. */
enum class async_generation : unsigned char
{
	legacy, /**< Native NT releases before version 5.0 or an unavailable version query. */
	nt5,    /**< NT 5.x and Windows Vista (NT 6.0). */
	nt7,    /**< Windows 7 through Windows 8.1 (NT 6.1 through NT 6.3). */
	nt10    /**< Windows 10 and later, including systems with optional I/O ring exports. */
};

/** @brief Classifies a native OS version independently of optional symbols and build numbers. */
inline constexpr async_generation classify_async_generation(::std::uint_least32_t major,
															::std::uint_least32_t minor) noexcept
{
	if (major >= 10)
	{
		return async_generation::nt10;
	}
	if (major > 6 || (major == 6 && minor != 0))
	{
		return async_generation::nt7;
	}
	return major >= 5 ? async_generation::nt5 : async_generation::legacy;
}

namespace details
{

namespace native_async_loader
{
// These loader entry points precede the modern I/O exports. C linkage binds the native ntdll symbols.
extern "C" FAST_IO_DLLIMPORT ::std::uint_least32_t FAST_IO_WINSTDCALL LdrGetDllHandle(
	char16_t const *, ::std::uint_least32_t const *, unicode_string const *, void **) noexcept;
extern "C" FAST_IO_DLLIMPORT ::std::uint_least32_t FAST_IO_WINSTDCALL LdrGetProcedureAddress(
	void *, ansi_string const *, ::std::uint_least32_t, void **) noexcept;
}

/** @brief Describes the base RTL_OSVERSIONINFOW buffer accepted by RtlGetVersion. */
struct async_version_info
{
	::std::uint_least32_t size{sizeof(async_version_info)};
	::std::uint_least32_t major{}, minor{}, build{}, platform{};
	char16_t service_pack[128]{};
};

/** @brief Borrows the native loader's existing ntdll mapping for the lifetime of the process. */
inline void *async_native_library() noexcept
{
	unicode_string name{18u, 20u, const_cast<char16_t *>(u"ntdll.dll")};
	void *module{};
	if (native_async_loader::LdrGetDllHandle(nullptr, nullptr, &name, &module) != 0)
	{
		return nullptr;
	}
	return module;
}

/** @brief Resolves a counted export name without introducing a portable executable import for that export. */
template <typename Function, ::std::size_t N>
inline Function async_native_symbol(void *module, char const (&name)[N]) noexcept
{
	static_assert(N <= UINT16_MAX);
	ansi_string symbol{static_cast<::std::uint_least16_t>(N - 1), static_cast<::std::uint_least16_t>(N),
					   const_cast<char *>(name)};
	void *address{};
	if (native_async_loader::LdrGetProcedureAddress(module, &symbol, 0, &address) != 0)
	{
		return nullptr;
	}
	return reinterpret_cast<Function>(address);
}

using nt_ring_status = ::std::uint_least32_t;
using query_io_ring_fn = nt_ring_status(FAST_IO_WINSTDCALL *)(::std::size_t, void *) noexcept;
using create_io_ring_fn = nt_ring_status(FAST_IO_WINSTDCALL *)(void **, ::std::uint_least32_t, void *,
															   ::std::uint_least32_t, void *) noexcept;
using submit_io_ring_fn = nt_ring_status(FAST_IO_WINSTDCALL *)(void *, ::std::uint_least32_t, ::std::uint_least32_t,
															  ::std::int_least64_t const *) noexcept;
using set_io_ring_fn = nt_ring_status(FAST_IO_WINSTDCALL *)(void *, ::std::uint_least32_t, ::std::uint_least32_t,
														   void *) noexcept;

/** @brief Stores the native release information and the exact set of optional NT exports. */
struct async_native_exports
{
	::fast_io::win32::nt::async_generation generation{};
	::std::uint_least32_t build{};
	query_io_ring_fn query{};
	create_io_ring_fn create{};
	submit_io_ring_fn submit{};
	set_io_ring_fn set_information{};
};

/** @brief Resolves NT release metadata and modern functions once per process. */
inline async_native_exports resolve_async_native_exports() noexcept
{
	async_native_exports exports{};
	auto *module{async_native_library()};
	if (module == nullptr)
	{
		return exports;
	}
	using version_fn = nt_ring_status(FAST_IO_WINSTDCALL *)(async_version_info *) noexcept;
	auto version{async_native_symbol<version_fn>(module, "RtlGetVersion")};
	async_version_info info{};
	if (version == nullptr || version(&info) != 0)
	{
		return exports;
	}
	exports.generation = classify_async_generation(info.major, info.minor);
	exports.build = info.build;
	if (exports.generation == async_generation::nt10)
	{
		// NT 10 also covers the original Windows 10 release, whose ntdll has no I/O ring exports.
		exports.query = async_native_symbol<query_io_ring_fn>(module, "NtQueryIoRingCapabilities");
		exports.create = async_native_symbol<create_io_ring_fn>(module, "NtCreateIoRing");
		exports.submit = async_native_symbol<submit_io_ring_fn>(module, "NtSubmitIoRing");
		exports.set_information = async_native_symbol<set_io_ring_fn>(module, "NtSetInformationIoRing");
	}
	return exports;
}

/** @brief Shares one immutable export table without entering the loader on each request. */
inline async_native_exports const &get_async_native_exports() noexcept
{
	static auto const exports{resolve_async_native_exports()};
	return exports;
}

inline constexpr nt_ring_status status_not_implemented{0xc0000002u};

} // namespace details

/** @brief Returns the running NT generation without applying the executable's compatibility manifest. */
inline async_generation native_async_generation() noexcept
{
	return details::get_async_native_exports().generation;
}

/**
 * @brief Queries the kernel's native I/O ring capabilities.
 * @param size Size in bytes of the writable native capability buffer.
 * @param capabilities Receives native fields according to the running kernel ABI.
 * @return An NTSTATUS, including STATUS_NOT_IMPLEMENTED on releases without the export.
 */
inline ::std::uint_least32_t NtQueryIoRingCapabilities(::std::size_t size, void *capabilities) noexcept
{
	auto query{details::get_async_native_exports().query};
	return query == nullptr ? details::status_not_implemented : query(size, capabilities);
}

/**
 * @brief Creates a native NT I/O ring without adding a loader dependency on the Windows 11 export.
 * @details Use the running kernel's native parameter layouts. The output handle is closed with NtClose.
 *          Perform capability negotiation before interpreting any returned memory mapping.
 * @return An NTSTATUS, including STATUS_NOT_IMPLEMENTED on releases without the export.
 */
inline ::std::uint_least32_t NtCreateIoRing(void **handle, ::std::uint_least32_t input_size, void *input,
										  ::std::uint_least32_t output_size, void *output) noexcept
{
	auto create{details::get_async_native_exports().create};
	return create == nullptr ? details::status_not_implemented : create(handle, input_size, input, output_size, output);
}

/**
 * @brief Submits published native queue entries and optionally waits for completions.
 * @details Submission status and per-operation completion status are separate. The caller must keep every
 *          submitted range alive until its completion has been retired.
 */
inline ::std::uint_least32_t NtSubmitIoRing(void *handle, ::std::uint_least32_t flags, ::std::uint_least32_t wait_count,
										  ::std::int_least64_t const *timeout) noexcept
{
	auto submit{details::get_async_native_exports().submit};
	return submit == nullptr ? details::status_not_implemented : submit(handle, flags, wait_count, timeout);
}

/**
 * @brief Updates ring information using the kernel's native information class and buffer ABI.
 * @details The caller supplies writable storage with the size and alignment required by the native class.
 */
inline ::std::uint_least32_t NtSetInformationIoRing(void *handle, ::std::uint_least32_t information_class,
												 ::std::uint_least32_t size, void *information) noexcept
{
	auto set{details::get_async_native_exports().set_information};
	return set == nullptr ? details::status_not_implemented : set(handle, information_class, size, information);
}

} // namespace fast_io::win32::nt
