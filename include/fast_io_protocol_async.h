#pragma once
/**
 * @file
 * @brief Connects resumable protocol edges to the existing fast_io coroutine scheduler.
 * @details Include this header to select composed reads/writes in print and
 * transmit. No executor or thread is created by a protocol engine. A lower
 * transport must implement asynchronous byte requests and make successful
 * writes visible to its peer. Buffered transports with a distinct flush
 * operation retain the blocking facade until they provide a composed flush.
 */
#include "fast_io_protocol.h"
#include "fast_io_freestanding.h"
#if defined(__cpp_impl_coroutine) && __has_include(<coroutine>)

namespace fast_io::details
{
/** @brief Grants the coroutine driver private access without forwarding a plaintext device to a raw descriptor. */
struct protocol_async_access
{
	/** @brief Borrows the two directional wire endpoints for the lifetime of a single operation. */
	template <typename file>
	static decltype(auto) wire(file &f) noexcept
	{
		return f.wire.get();
	}
	/** @brief Starts the same exclusive, resumable operation used by the synchronous facade. */
	template <typename file>
	static auto start(file &f, protocol_request r)
	{
		return f.session.start(r);
	}
	/** @brief Reads the provider's handshake state without issuing a wire request or exposing application data. */
	template <typename File>
	static bool handshake_pending(File const &file) noexcept
		requires requires { file.session.handshake_pending(); }
	{
		return file.session.handshake_pending();
	}
	/** @brief Reuses the blocking driver when a surrounding asynchronous buffer delegates a write on its worker. */
	template <typename file, typename observed>
	static ::std::size_t write(file &f, ::std::byte const *p, ::std::size_t n, observed observer)
	{
		return f.drive({protocol_action::write, nullptr, p, n}, observer);
	}
};

template <typename storage>
using protocol_input_ref = ::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(::std::declval<storage &>().get()))>;
template <typename storage>
using protocol_output_ref = ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(::std::declval<storage &>().get()))>;

/** @brief Requires a wire completion to publish its prefix, including the last handshake or alert byte. */
template <typename storage>
concept asynchronous_protocol_transport = ::fast_io::async::awaitable_input<protocol_input_ref<storage>> &&
										  ::fast_io::async::awaitable_output<protocol_output_ref<storage>> &&
										  !::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<protocol_output_ref<storage>>;

/** @brief Records physical participation in the issuer's history without retiring a per-record point early. */
struct protocol_activity_receipt
{
	protocol_action action;
	bool used;
	bool await_ready() const noexcept
	{
		return false;
	}
	template <typename promise>
	bool await_suspend(::std::coroutine_handle<promise> handle) const noexcept
	{
		if (action == protocol_action::read || action == protocol_action::write)
		{
			auto index = static_cast<unsigned>(action == protocol_action::write);
			::fast_io::async::details::file_activity activity{};
			activity.tracked[index] = true;
			activity.reached[index] = used ? 1u : 0u;
			handle.promise().history.received(activity);
		}
		return false;
	}
	void await_resume() const noexcept
	{}
};

/** @brief Drives every transport edge in a single coroutine with one outstanding request at a time. */
template <typename storage, typename engine>
	requires asynchronous_protocol_transport<storage>
inline ::fast_io::async::task<::fast_io::async::byte_completion> drive_protocol_async(
	basic_protocol_file<storage, engine> &file, protocol_request request)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	using namespace ::fast_io::async;
	constexpr auto enable = ::fast_io::async::details::stream_scheduler<typename storage::handle_type>();
	if constexpr (!::std::same_as<::std::remove_cv_t<decltype(enable)>, ::std::nullptr_t>)
	{
		if (enable)
		{
			enable();
		}
	}
	auto &wire = protocol_async_access::wire(file);
	decltype(auto) in = ::fast_io::operations::input_stream_ref(wire);
	decltype(auto) out = ::fast_io::operations::output_stream_ref(wire);
	auto op = protocol_async_access::start(file, request);
	bool used{};
	for (;;)
	{
		auto event = op.poll();
		if (event.edge == protocol_edge::complete)
		{
			co_await protocol_activity_receipt{request.action, used};
			co_return byte_completion{event.size, {}};
		}
		if (event.edge == protocol_edge::failed)
		{
			co_await protocol_activity_receipt{request.action, used};
#if defined(__HERBCEPTIONS__)
			throw throws ::std::errc::protocol_error;
#else
			::fast_io::details::raise_protocol_error(event.error);
#endif
		}
		used = true;
		byte_completion result;
		if (event.edge == protocol_edge::receive)
		{
			result = co_await async_read_some_define(in, event.receive, event.size);
		}
		else
		{
			result = co_await async_write_some_define(out, event.send, event.size);
		}
		if (result.error.raise)
		{
			co_await protocol_activity_receipt{request.action, used};
			co_await ::fast_io::async::details::store_native_error{result.error};
			co_return byte_completion{};
		}
		op.commit(result.transferred);
	}
}
} // namespace fast_io::details

namespace fast_io
{
/** @brief Local, ordered entry points for a protocol with a distinct initial handshake and application phase. */
enum class protocol_stage : unsigned { handshake, application_data };

/** @brief Advertises two local starts only when the engine can distinguish settled negotiation from pending wire bytes.
 * @details The child has its own nearest marker. A TLS read may send authentication traffic before reading a record.
 * The first application start orders entry into the application operation after delivery of the handshake flight.
 */
template <typename Storage, typename Engine, bool Input>
	requires requires(Engine const &engine) {
		{ protocol_handshake_pending_define(engine) } noexcept -> ::std::same_as<bool>;
	}
inline constexpr auto async_local_points_define(io_type_t<protocol_file_ref<basic_protocol_file<Storage, Engine>>>,
	::std::bool_constant<Input>) noexcept
{
	return async::ordered_start_points<protocol_stage,2,async::point_kind::decorator>{};
}

/** @brief Adds this protocol to the actual wire description, including every nested transform and buffer. */
template <typename storage, typename engine>
inline constexpr auto async_layer_kind_define(io_type_t<protocol_file_ref<basic_protocol_file<storage, engine>>>) noexcept
{
	return async::point_kind::decorator;
}

/** @brief Identifies an observable child without granting permission to bypass the protocol. */
template <typename storage, typename engine>
inline constexpr auto async_stage_child_define(io_type_t<protocol_file_ref<basic_protocol_file<storage, engine>>>) noexcept
{
	return ::std::type_identity<typename storage::handle_type>{};
}

/** @brief Advances the same protocol session as the synchronous facade using the operation's wire selection.
 * @details A TLS read can send control traffic and a write can require a receive. Each completed wire
 * request belongs to the original operation. The provider alone authorizes plaintext, authenticates
 * the peer, rejects truncation and controls close alerts. A partial transfer advances only its prefix.
 */
template <typename Path, ::std::size_t Depth, typename Storage, typename Engine>
inline async::task<::std::size_t> drive_selected_protocol(protocol_file_ref<basic_protocol_file<Storage, Engine>> ref,
	protocol_request request, async::details::file_activity &activity)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	namespace op = ::fast_io::async::details;
	using child = typename Path::child_selection;
	constexpr auto depth{Depth + Path::local_count};
	auto &file{*ref.owner};
	auto &wire{details::protocol_async_access::wire(file)};
	decltype(auto) in{operations::input_stream_ref(wire)};
	decltype(auto) out{operations::output_stream_ref(wire)};
	using direction = op::file_activity::direction;
	bool const output{activity.logical == direction::inherit ? request.action != protocol_action::read :
		activity.logical == direction::output};
	auto const side{output ? async::point_side::output : async::point_side::input};
	constexpr bool stages{requires { details::protocol_async_access::handshake_pending(file); } &&
		(Path::local_selection::selects(0) || Path::local_selection::selects(1))};
	bool const application{request.action == protocol_action::write || request.action == protocol_action::read};
	bool negotiate{};
	if constexpr (stages) negotiate = application && details::protocol_async_access::handshake_pending(file);
	for (;;)
	{
		// Acquiring the exclusive slot precedes a private stop. A waiting observer cannot reuse this session.
		auto session{details::protocol_async_access::start(file, negotiate ? protocol_request{protocol_action::handshake} : request)};
		if constexpr (stages)
		{
			if (application)
			{
				if constexpr (Path::local_selection::selects(0))
					co_await op::layer_start{{side,Depth},negotiate};
				if constexpr (Path::local_selection::selects(1))
					if (!negotiate) co_await op::layer_start{{side,Depth + 1u},request.size != 0};
			}
		}
		for (;;)
		{
			if constexpr (Path::local_selection::selects(0) || Path::local_selection::selects(1))
			{
				bool const selected{!stages || (negotiate ? Path::local_selection::selects(0) : Path::local_selection::selects(1))};
				if (selected && op::has_layer_peer()) co_await op::yield();
			}
			auto event{session.poll()};
			if (event.edge == protocol_edge::complete)
			{
				if (!negotiate) co_return event.size;
				negotiate = false;
				break;
			}
			if (event.edge == protocol_edge::failed)
			{
#if defined(__HERBCEPTIONS__)
				throw throws ::std::errc::protocol_error;
#else
				details::raise_protocol_error(event.error);
#endif
			}
			op::file_activity physical{};
			physical.logical = output ? direction::output : direction::input;
			auto transfer{event.size};
			if (event.edge == protocol_edge::receive)
				transfer = co_await op::read_layer<child, depth>(in, event.receive, transfer, physical);
			else
			{
				co_await op::write_layer<child, depth, op::layer_action::write>(out, event.send, transfer, physical);
				// A buffered control flight must become visible before the handshake waits for its peer.
				if constexpr (operations::decay::defines::output_stream_buffer_flush_dispatchable<::std::remove_cvref_t<decltype(out)>>)
					co_await op::write_layer<child, depth, op::layer_action::publish>(out, nullptr, 0, physical);
			}
			op::visit_layer(activity, output, physical.reached[0] < physical.reached[1] ? physical.reached[1] : physical.reached[0]);
			session.commit(transfer);
		}
	}
}

/** @brief Exposes an authenticated input prefix to the operation driving this decorated path. */
template <typename Path, ::std::size_t Depth, typename Storage, typename Engine>
inline auto async_layer_read_define(io_type_t<Path>, ::std::integral_constant<::std::size_t, Depth>,
	protocol_file_ref<basic_protocol_file<Storage, Engine>> ref, ::std::byte *data, ::std::size_t size,
	async::details::file_activity &activity)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return drive_selected_protocol<Path, Depth>(ref, {protocol_action::read, data, nullptr, size}, activity);
}

/** @brief Drives a selected application write or authenticated local close, preserving wire visibility. */
template <typename Path, ::std::size_t Depth, async::details::layer_action Action, typename Storage, typename Engine>
inline async::task<void> async_layer_write_define(io_type_t<Path>, ::std::integral_constant<::std::size_t, Depth>,
	::std::integral_constant<async::details::layer_action, Action>, protocol_file_ref<basic_protocol_file<Storage, Engine>> ref,
	::std::byte const *data, ::std::size_t size, async::details::file_activity &activity)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	if constexpr (Action == async::details::layer_action::finish)
		co_await drive_selected_protocol<Path, Depth>(ref, {protocol_action::close_write}, activity);
	else if constexpr (Action == async::details::layer_action::write)
		while (size)
		{
			auto count{co_await drive_selected_protocol<Path, Depth>(ref, {protocol_action::write, nullptr, data, size}, activity)};
			if (!count || count > size) async::details::raise_invalid_progress(0);
			data += count;
			size -= count;
		}
}

/** @brief Records an actual wire delegation when an outer buffer or transcoder runs its synchronous overflow. */
template <typename storage, typename engine>
inline void async_measured_write_define(protocol_file_ref<basic_protocol_file<storage, engine>> ref,
										char const *first, char const *last, async::details::file_activity &activity, ::std::size_t depth)
{
	auto observed = [&](protocol_edge) noexcept {
		if (activity.reached[1] <= depth)
		{
			activity.reached[1] = depth + 1;
		}
	};
	while (first != last)
	{
		first += details::protocol_async_access::write(*ref.owner, reinterpret_cast<::std::byte const *>(first),
													   static_cast<::std::size_t>(last - first), observed);
	}
}

/** @brief Reads through the full protocol, suspending on either direction selected by the engine. */
template <typename storage, typename engine>
	requires details::asynchronous_protocol_transport<storage>
inline auto async_read_some_define(protocol_file_ref<basic_protocol_file<storage, engine>> ref, ::std::byte *p, ::std::size_t n)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return details::drive_protocol_async(*ref.owner, {protocol_action::read, p, nullptr, n});
}

/** @brief Writes a stable coroutine-owned application range and awaits every necessary control flight. */
template <typename storage, typename engine>
	requires details::asynchronous_protocol_transport<storage>
inline auto async_write_some_define(protocol_file_ref<basic_protocol_file<storage, engine>> ref, ::std::byte const *p, ::std::size_t n)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return details::drive_protocol_async(*ref.owner, {protocol_action::write, nullptr, p, n});
}

namespace async
{
/** @brief Negotiates a full session without blocking the calling event loop. */
template <typename storage, typename engine>
	requires ::fast_io::details::asynchronous_protocol_transport<storage>
inline auto handshake(basic_protocol_file<storage, engine> &s)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return ::fast_io::details::drive_protocol_async(s, {protocol_action::handshake});
}

/** @brief Awaits the local protocol close while retaining the remote read direction. */
template <typename storage, typename engine>
	requires ::fast_io::details::asynchronous_protocol_transport<storage>
inline auto close_write(basic_protocol_file<storage, engine> &s)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return ::fast_io::details::drive_protocol_async(s, {protocol_action::close_write});
}

/** @brief Awaits both authenticated directions of an orderly protocol shutdown. */
template <typename storage, typename engine>
	requires ::fast_io::details::asynchronous_protocol_transport<storage>
inline auto shutdown(basic_protocol_file<storage, engine> &s)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	return ::fast_io::details::drive_protocol_async(s, {protocol_action::shutdown});
}
} // namespace async
} // namespace fast_io
#endif
