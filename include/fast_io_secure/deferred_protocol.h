#pragma once

/** @file @brief Captured protocol parameters whose live engine is constructed at the first I/O step. */
#include "../fast_io_protocol.h"
#include "error.h"
#include <optional>
#include <variant>

namespace fast_io::secure::details
{
/** @brief Returns provider setup success or an exact failure without requiring an exception ABI from the provider. */
template <typename Engine>
struct protocol_open_result
{
	::std::optional<Engine> engine;
	errc error{errc::provider};
};

/**
 * @brief Retains independently owned parameters until an operation actually needs its protocol engine.
 * @details The initial handshake query and wire-window probes are side-effect free. A successful
 * first step replaces the captured parameters exactly once. A failed attempt releases the
 * parameters, including a captured PSK, and leaves a terminal status for subsequent polls.
 */
template <typename Parameters>
class deferred_protocol_engine
{
	using engine = typename Parameters::engine_type;
	static_assert(::std::is_nothrow_move_constructible_v<Parameters> && ::std::is_nothrow_move_constructible_v<engine>);
	::std::variant<Parameters, engine, ::fast_io::error> state;

	/** @brief Installs a poison state before opening so that even allocation failure cannot retry a partly initialized exchange. */
	protocol_result first_step(protocol_request request)
	{
		if (auto *failure{::std::get_if<2>(&state)})
			return {protocol_status::failed, 0, *failure};

		{
			auto parameters{::std::get<0>(::std::move(state))};
			constexpr auto domain{domain_define(::fast_io::error_type<errc>)};
			state.template emplace<2>(::fast_io::error{domain,static_cast<::std::size_t>(errc::provider)});
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			try
			{
#endif
				auto opened{parameters.open()};
				if (!opened.engine)
				{
					auto error{::fast_io::error{domain,static_cast<::std::size_t>(opened.error)}};
					state.template emplace<2>(error);
					return {protocol_status::failed,0,error};
				}
				state.template emplace<1>(::std::move(*opened.engine));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			}
			catch (::fast_io::error error)
			{
				state.template emplace<2>(error);
				return {protocol_status::failed, 0, error};
			}
#endif
			// Release retained context references and erase captured raw keys before the live exchange begins.
		}
		return protocol_step_define(::std::get<1>(state), request);
	}

public:
	/** @brief Captures a descriptor without opening a wire, creating an engine, or allocating record storage. */
	explicit deferred_protocol_engine(Parameters parameters) noexcept : state{::std::in_place_index<0>, ::std::move(parameters)} {}

	/** @brief Preserves the underlying engine's compile-time record and staging limits. */
	friend constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<deferred_protocol_engine>) noexcept
	{
		return ::fast_io::details::transcode_preferred_sizes<engine>;
	}
	/** @brief Preserves the engine's plaintext-erasure contract for outer buffering layers. */
	friend constexpr auto transcode_sensitive_define(io_type_t<deferred_protocol_engine>) noexcept
	{
		return ::std::bool_constant<::fast_io::sensitive_transcoder<engine>>{};
	}
	/** @brief Reports the initial handshake as pending before the first step and queries the live protocol thereafter. */
	friend bool protocol_handshake_pending_define(deferred_protocol_engine const &d) noexcept
	{
		if (auto *live{::std::get_if<1>(&d.state)}) return protocol_handshake_pending_define(*live);
		return d.state.index() == 0;
	}
	/** @brief Exposes incoming storage only after the first protocol step has requested transport input. */
	friend basic_protocol_window<::std::byte> protocol_input_window_define(deferred_protocol_engine &d)
	{
		if (auto *live{::std::get_if<1>(&d.state)}) return protocol_input_window_define(*live);
		return {};
	}
	/** @brief Commits a prefix of a range previously lent by the live engine. */
	friend void protocol_input_commit_define(deferred_protocol_engine &d, ::std::size_t count)
	{
		if (auto *live{::std::get_if<1>(&d.state)}) { protocol_input_commit_define(*live,count); return; }
		::fast_io::throw_transcode_stream_error(transcode_stream_errc::invalid_state);
	}
	/** @brief Leaves an unstarted descriptor idle when a driver first probes for an outgoing flight. */
	friend basic_protocol_window<::std::byte const> protocol_output_window_define(deferred_protocol_engine &d)
	{
		if (auto *live{::std::get_if<1>(&d.state)}) return protocol_output_window_define(*live);
		return {};
	}
	/** @brief Releases only a prefix of ciphertext borrowed from the live engine. */
	friend void protocol_output_consume_define(deferred_protocol_engine &d, ::std::size_t count)
	{
		if (auto *live{::std::get_if<1>(&d.state)}) { protocol_output_consume_define(*live,count); return; }
		::fast_io::throw_transcode_stream_error(transcode_stream_errc::invalid_state);
	}
	/** @brief Opens the captured configuration once and delegates every subsequent bounded protocol step directly. */
	friend protocol_result protocol_step_define(deferred_protocol_engine &d, protocol_request request)
	{
		if (auto *live{::std::get_if<1>(&d.state)}) return protocol_step_define(*live,request);
		return d.first_step(request);
	}
};
} // namespace fast_io::secure::details
