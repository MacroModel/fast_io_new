#pragma once

/** @file @brief Direction-changing transform steps and their provider-owned wire windows. */
namespace fast_io
{
/**
 * @brief Certifies that a decoder may combine output from already available input into a larger public window.
 * @details For a fixed input and initial state, processing the available units into successive legal
 * windows must yield the same byte stream, carry and terminal result as processing into a larger
 * window. An error cannot retract an earlier publishable prefix. The consumer must honor record
 * authentication boundaries and return available output before waiting for further physical input.
 * This contract permits either synchronous or asynchronous consumers to choose their window size.
 */
template <typename Engine>
concept batchable_transcode_input = requires {
	{ transcode_input_batch_define(transcode_reserve<Engine>) } noexcept -> ::std::same_as<::std::true_type>;
};

/**
 * @brief Allows adjacent public windows from the same completed conversion to be delivered together.
 * @details The engine has already accepted the entire private output range. Consuming any prefix
 * requires no further engine call, authentication, input or finalization. This permits a transfer
 * to retain the original public window boundaries for cursor recovery while grouping physical writes.
 */
template <typename Engine>
concept coalescible_transcode_result = requires {
	{ transcode_verified_window_define(transcode_reserve<Engine>) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief Selects the application operation whose arguments stay live until completion. */
enum class protocol_action : unsigned char
{
	handshake,
	read,
	write,
	close_write,
	shutdown
};

/** @brief Carries one application range. Writes must stay immutable throughout the operation. */
struct protocol_request
{
	protocol_action action{};
	::std::byte *destination{};
	::std::byte const *source{};
	::std::size_t size{};
};

/** @brief Reports an engine result before outstanding wire output has been drained. */
enum class protocol_status : unsigned char
{
	complete,
	need_input,
	need_output,
	retry,
	failed
};

/** @brief Counts application bytes only. A completed nonempty read of zero is authenticated EOF for TLS. */
struct protocol_result
{
	protocol_status status{};
	::std::size_t transferred{};
	::fast_io::error error{};
};

/** @brief Describes a contiguous provider-owned wire buffer. */
template <typename byte_type>
struct basic_protocol_window
{
	byte_type *data{};
	::std::size_t size{};
};

/**
 * @brief Describes a stateful protocol engine independently of files, schedulers and cryptography.
 * @details A successful step commits at most one application result. The output
 * window exposes queued bytes even after success or a request for peer input.
 * Consuming output or filling input advances exactly the supplied prefix.
 * Committing zero input means physical EOF. The engine must then complete or
 * fail without demanding another transport read. Retry requires internal progress.
 */
template <typename engine>
concept duplex_protocol_engine = requires(engine &e, protocol_request request, ::std::size_t n) {
	{ protocol_step_define(e, request) } -> ::std::same_as<protocol_result>;
	{ protocol_input_window_define(e) } -> ::std::same_as<basic_protocol_window<::std::byte>>;
	{ protocol_output_window_define(e) } -> ::std::same_as<basic_protocol_window<::std::byte const>>;
	{ protocol_input_commit_define(e, n) } -> ::std::same_as<void>;
	{ protocol_output_consume_define(e, n) } -> ::std::same_as<void>;
};
} // namespace fast_io
