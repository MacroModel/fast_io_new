#pragma once

/**
 * @file
 * @brief Transport-independent, bounded, two-way protocol sessions.
 * @details One application operation can produce wire output and then require
 * wire input any number of times. Only the engine knows this dependency graph.
 * Each edge borrows a stable range until the transport commits a prefix.
 */
#include "fast_io_core.h"

namespace fast_io
{

/** @brief Identifies the next externally observable dependency of an operation. */
enum class protocol_edge : unsigned char
{
	complete,
	receive,
	send,
	failed
};

/** @brief Exposes exactly one wire range, or a completed application byte count. */
struct protocol_event
{
	protocol_edge edge{};
	::std::byte *receive{};
	::std::byte const *send{};
	::std::size_t size{};
	::fast_io::error error{};
};

/**
 * @brief Owns a single engine and serializes access to its protocol state.
 * @details External synchronization is required when shared between threads.
 * Abandoning an incomplete operation poisons the session, since the peer may
 * already have observed a partial flight. No destructor performs network I/O.
 */
template <duplex_protocol_engine engine>
class basic_protocol_session
{
	engine provider;
	bool occupied{}, failed{}, transport_eof{};

	/** @brief Transfers an idle engine while invalidating its previous session identity. */
	static engine take(basic_protocol_session &other)
	{
		if (other.occupied || other.failed)
		{
			::fast_io::throw_transcode_stream_error(transcode_stream_errc::invalid_state);
		}
		other.failed = true;
		return ::std::move(other.provider);
	}

public:
	/** @brief Moves an idle session, including the physical EOF state. */
	basic_protocol_session(basic_protocol_session &&other)
		: provider(take(other)), transport_eof(other.transport_eof)
	{}

	/** @brief Takes exclusive ownership of the engine's state. */
	explicit basic_protocol_session(engine value) : provider(::std::move(value))
	{}
	basic_protocol_session(basic_protocol_session const &) = delete;
	basic_protocol_session &operator=(basic_protocol_session const &) = delete;

	/** @brief Queries the initial handshake, including its final undelivered wire bytes, without advancing any operation. */
	[[nodiscard]] bool handshake_pending() const noexcept
		requires requires(engine const &e) {
			{ protocol_handshake_pending_define(e) } noexcept -> ::std::same_as<bool>;
		}
	{
		// A moved-from or poisoned session cannot start further application work.
		return !failed && protocol_handshake_pending_define(provider);
	}

	/** @brief Represents a resumable operation. Its engine and application ranges must outlive it. */
	class operation
	{
		basic_protocol_session &session;
		protocol_request request;
		protocol_result result{protocol_status::retry, 0};
		protocol_event current{};
		bool waiting{}, done{};

		friend basic_protocol_session;

		/** @brief Acquires the exclusive operation slot before exposing any wire bytes. */
		operation(basic_protocol_session &s, protocol_request r) : session(s), request(r)
		{
			if (s.occupied || s.failed)
			{
				::fast_io::throw_transcode_stream_error(transcode_stream_errc::invalid_state);
			}
			s.occupied = true;
		}

		/** @brief Rejects impossible provider edges and prevents a retry against divergent state. */
		[[noreturn]] void violation()
		{
			session.failed = true;
			::fast_io::throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
		}

	public:
		operation(operation const &) = delete;
		operation &operator=(operation const &) = delete;

		/** @brief Cancels a suspended operation without publishing it as complete. */
		~operation()
		{
			if (!done)
			{
				session.failed = true;
				session.occupied = false;
			}
		}

		/**
		 * @brief Returns a stable readiness edge, driving only bounded in-process work.
		 * @details Drain an outgoing flight before reading the peer or completing the
		 * application call. Never repeat an already successful engine operation.
		 */
		protocol_event poll()
		{
			if (done)
			{
				return current;
			}
			if (session.failed)
			{
				violation();
			}
			if (waiting)
			{
				return current;
			}
			// Pessimistic assignment also poisons on unwinding from a provider callback.
			session.failed = true;
			for (;;)
			{
				auto output = protocol_output_window_define(session.provider);
				if (output.size)
				{
					if (!output.data)
					{
						violation();
					}
					current = {protocol_edge::send, nullptr, output.data, output.size};
					waiting = true;
					break;
				}
				if (result.status == protocol_status::failed)
				{
					// Flush any queued fatal alert before exposing the terminal protocol error.
					current = {protocol_edge::failed, nullptr, nullptr, 0, result.error};
					done = true;
					session.occupied = false;
					return current;
				}
				if (result.status == protocol_status::complete)
				{
					if (result.transferred > request.size ||
						(request.action == protocol_action::write && request.size && !result.transferred))
					{
						violation();
					}
					current = {protocol_edge::complete, nullptr, nullptr, result.transferred};
					done = true;
					session.occupied = false;
					break;
				}
				if (result.status == protocol_status::need_input)
				{
					auto input = protocol_input_window_define(session.provider);
					if (session.transport_eof || !input.data || !input.size)
					{
						violation();
					}
					current = {protocol_edge::receive, input.data, nullptr, input.size};
					waiting = true;
					break;
				}
				if (result.status != protocol_status::retry)
				{
					violation();
				}
				result = protocol_step_define(session.provider, request);
			}
			session.failed = false;
			return current;
		}

		/**
		 * @brief Commits one transport completion. Short transfers preserve the suffix.
		 * @details A zero-byte receive signals EOF. A zero-byte send cannot advance
		 * the stream. On would-block retain the edge and wait without calling commit.
		 */
		void commit(::std::size_t count)
		{
			if (done || !waiting || session.failed || count > current.size ||
				(current.edge == protocol_edge::send && !count))
			{
				violation();
			}
			session.failed = true;
			if (current.edge == protocol_edge::receive)
			{
				if (!count)
				{
					session.transport_eof = true;
				}
				protocol_input_commit_define(session.provider, count);
				result = {protocol_status::retry, 0};
			}
			else
			{
				protocol_output_consume_define(session.provider, count);
				if (result.status == protocol_status::need_output)
				{
					result = {protocol_status::retry, 0};
				}
			}
			waiting = false;
			session.failed = false;
		}
	};

	/** @brief Starts an operation whose buffers can be borrowed by a readiness loop or coroutine. */
	[[nodiscard]] operation start(protocol_request request)
	{
		if ((request.action == protocol_action::read && request.size && !request.destination) ||
			(request.action == protocol_action::write && request.size && !request.source))
		{
			::fast_io::throw_transcode_stream_error(transcode_stream_errc::invalid_state);
		}
		return operation{*this, request};
	}
};

namespace details
{
struct protocol_async_access;
}

namespace details
{
/** @brief Raises a reported engine failure at the ordinary blocking stream boundary. */
[[noreturn]] inline void raise_protocol_error(::fast_io::error error)
{
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
	throw error;
#else
	(void)error;
	::fast_io::fast_terminate();
#endif
}
} // namespace details

/** @brief Adapts a protocol session to ordinary blocking fast_io device operations. */
template <typename storage, duplex_protocol_engine engine>
class basic_protocol_file
{
	friend struct details::protocol_async_access;
	storage wire;
	basic_protocol_session<engine> session;

	/** @brief Pumps both wire directions, including output generated during application reads. */
	template <typename on_wire>
	::std::size_t drive(protocol_request request, on_wire observed)
	{
		auto op = session.start(request);
		for (;;)
		{
			auto event = op.poll();
			if (event.edge == protocol_edge::complete)
			{
				return event.size;
			}
			if (event.edge == protocol_edge::failed)
			{
				details::raise_protocol_error(event.error);
			}
			observed(event.edge);
			if (event.edge == protocol_edge::receive)
			{
				auto end = ::fast_io::operations::read_some_bytes(wire.get(), event.receive, event.receive + event.size);
				op.commit(static_cast<::std::size_t>(end - event.receive));
			}
			else
			{
				::fast_io::operations::write_all_bytes(wire.get(), event.send, event.send + event.size);
				// A buffered transport must make its control flight visible to its peer.
				decltype(auto) ref = ::fast_io::operations::output_stream_ref(wire.get());
				if constexpr (::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<
								  ::std::remove_cvref_t<decltype(ref)>>)
				{
					::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(ref);
				}
				op.commit(event.size);
			}
		}
	}

	/** @brief Drives synchronous operations without allocating or updating an observation receipt. */
	::std::size_t drive(protocol_request request)
	{
		return drive(request, [](protocol_edge) noexcept {});
	}

public:
	/** @brief Keeps the original wire expression available to each directional I/O operation. */
	using decorated_stream_type = typename storage::handle_type;
	using engine_type = engine;

	/** @brief Constructs a session over a borrowed or owned full-duplex transport. */
	basic_protocol_file(storage handle, engine provider)
		: wire(::std::move(handle)), session(::std::move(provider))
	{}

	/** @brief Runs the handshake and flushes the last locally generated flight. */
	void handshake()
	{
		drive({protocol_action::handshake});
	}
	/** @brief Sends the authenticated end of local writes and leaves the read side usable. */
	void close_write()
	{
		drive({protocol_action::close_write});
	}
	/** @brief Sends the local close and waits for the authenticated peer close. */
	void shutdown()
	{
		drive({protocol_action::shutdown});
	}
	/** @brief Reads application bytes accepted by the provider or returns zero at its logical end of input. */
	::std::size_t read(::std::byte *data, ::std::size_t size)
	{
		return size ? drive({protocol_action::read, data, nullptr, size}) : 0;
	}
	/** @brief Delivers one prefix and flushes its generated wire output before returning. */
	::std::size_t write(::std::byte const *data, ::std::size_t size)
	{
		return size ? drive({protocol_action::write, nullptr, data, size}) : 0;
	}
};

/** @brief Recognizes an ordered device with both byte input and byte output operations. */
template <typename handle>
concept duplex_protocol_transport = requires(handle &h) {
	::fast_io::operations::input_stream_ref(h);
	::fast_io::operations::output_stream_ref(h);
	requires ::fast_io::operations::decay::defines::bytes_readable<
		::std::remove_cvref_t<decltype(::fast_io::operations::input_stream_ref(h))>>;
	requires ::fast_io::operations::decay::defines::bytes_writable<
		::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(h))>>;
};

/** @brief Borrows an lvalue transport or takes ownership of an rvalue transport. */
template <duplex_protocol_transport handle, duplex_protocol_engine engine>
inline auto make_protocol_file(handle &&wire, engine provider)
{
	auto storage = ::fast_io::details::make_otranscoder_handle_storage(::std::forward<handle>(wire));
	return basic_protocol_file<decltype(storage), engine>{::std::move(storage), ::std::move(provider)};
}

/** @brief Binds both directions of a stateful transform to one shared wire and engine.
 * @details The engine supplies the next read or write dependency for each operation. A TLS handshake,
 * application record and close can all revisit the same wire without rebuilding either direction.
 */
template <duplex_protocol_transport Handle, duplex_protocol_engine Engine>
inline auto make_iotranscoder(Handle &&wire, Engine provider)
{
	return ::fast_io::make_protocol_file(static_cast<Handle &&>(wire), static_cast<Engine &&>(provider));
}

/** @brief Provides a non-owning fast_io character and byte observer over a live session. */
template <typename file>
struct protocol_file_ref
{
	using decorated_stream_type = typename file::decorated_stream_type;
	/** @brief Propagates the engine's plaintext-erasure contract into outer input and output staging. */
	static constexpr bool input_secure_clear{sensitive_transcoder<typename file::engine_type>};
	static constexpr bool output_secure_clear{input_secure_clear};
	using input_char_type = char;
	using output_char_type = char;
	file *owner;
};

template <typename File>
inline constexpr ::std::size_t output_stream_transcode_bytes_define(io_type_t<protocol_file_ref<File>>) noexcept
{
	return details::transcode_preferred_sizes<typename File::engine_type>.input_bytes;
}

template <typename File>
inline constexpr ::std::size_t input_stream_transcode_bytes_define(io_type_t<protocol_file_ref<File>>) noexcept
{
	return details::transcode_preferred_sizes<typename File::engine_type>.output_bytes;
}

template <typename File>
inline constexpr ::std::size_t scatter_fallback_full_output_threshold(io_reserve_type_t<char, protocol_file_ref<File>>) noexcept
{
	return details::output_transcode_bytes<protocol_file_ref<File>>;
}

template <typename File>
inline constexpr ::std::size_t full_output_coalesce_threshold(io_reserve_type_t<char, protocol_file_ref<File>>) noexcept
{
	return details::output_transcode_bytes<protocol_file_ref<File>>;
}

/** @brief An existing protocol observer stays attached to the same session when nested in another adapter. */
template <typename file>
inline constexpr protocol_file_ref<file> io_stream_ref_define(protocol_file_ref<file> ref) noexcept
{
	return ref;
}

/** @brief Obtains the duplex observer without exposing the raw transport to I/O dispatch. */
template <typename storage, typename engine>
inline auto io_stream_ref_define(basic_protocol_file<storage, engine> &file) noexcept
{
	return protocol_file_ref<basic_protocol_file<storage, engine>>{__builtin_addressof(file)};
}

/** @brief Borrows an owned temporary for the duration of the enclosing standard I/O operation. */
template <typename storage, typename engine>
inline auto io_stream_ref_define(basic_protocol_file<storage, engine> &&file) noexcept
{
	return io_stream_ref_define(file);
}

/** @brief Finishes the local write direction without waiting for unrelated peer application data. */
template <typename file>
inline void output_stream_finish_define(protocol_file_ref<file> ref)
{
	ref.owner->close_write();
}

/** @brief Gives an owned temporary responsibility for the checked protocol close on successful output. */
template <typename storage, typename engine>
inline constexpr bool temporary_output_finish_enabled<basic_protocol_file<storage, engine>> = true;

/** @brief Reads an application prefix through the session's authenticated read path. */
template <typename file>
inline ::std::byte *read_some_bytes_underflow_define(protocol_file_ref<file> ref, ::std::byte *first, ::std::byte *last)
{
	return first + ref.owner->read(first, static_cast<::std::size_t>(last - first));
}

/** @brief Writes one application prefix through the session's record layer. */
template <typename file>
inline ::std::byte const *write_some_bytes_overflow_define(protocol_file_ref<file> ref, ::std::byte const *first, ::std::byte const *last)
{
	return first + ref.owner->write(first, static_cast<::std::size_t>(last - first));
}

/** @brief Binds character reads to the same underlying application byte stream. */
template <typename file>
inline char *read_some_underflow_define(protocol_file_ref<file> ref, char *first, char *last)
{
	return first + ref.owner->read(reinterpret_cast<::std::byte *>(first), static_cast<::std::size_t>(last - first));
}

/** @brief Binds formatted output and ordinary character writes to the protocol stream. */
template <typename file>
inline char const *write_some_overflow_define(protocol_file_ref<file> ref, char const *first, char const *last)
{
	return first + ref.owner->write(reinterpret_cast<::std::byte const *>(first), static_cast<::std::size_t>(last - first));
}

} // namespace fast_io
