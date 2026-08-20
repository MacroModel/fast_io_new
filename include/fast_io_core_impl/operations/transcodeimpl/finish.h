#pragma once

/**
 * @file
 * @brief Defines explicit terminal operations for transcoder stream adapters.
 *
 * Output finish commits trailers/tags and closes the output direction. Input
 * drain-and-finish is intentionally named differently: it discards unread
 * decoded data while consuming physical EOF to force terminal validation.
 * Ordinary scan/read never invokes that destructive input operation.
 */

namespace fast_io::operations::decay::defines
{

/** @brief Detects terminal output-finish support on a normalized observer. */
template <typename T>
concept has_output_stream_finish_define = requires(T ref) {
	{ output_stream_finish_define(ref) } -> ::std::same_as<void>;
};

/** @brief Detects explicit destructive input drain-and-finish support. */
template <typename T>
concept has_input_stream_drain_and_finish_define = requires(T ref) {
	{ input_stream_drain_and_finish_define(ref) } -> ::std::same_as<void>;
};

} // namespace fast_io::operations::decay::defines

namespace fast_io::operations::decay
{

/** @brief Dispatches terminal output finish on an already normalized observer. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_output_stream_finish_define<::std::remove_cvref_t<T>>
	inline void output_stream_finish_decay(T &&ref)
{
	output_stream_finish_define(ref);
}

/** @brief Dispatches explicit input drain-and-finish on a normalized observer. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_input_stream_drain_and_finish_define<::std::remove_cvref_t<T>>
	inline void input_stream_drain_and_finish_decay(T &&ref)
{
	input_stream_drain_and_finish_define(ref);
}

} // namespace fast_io::operations::decay

namespace fast_io::operations
{

/** @brief Normalizes a stream once and terminally finishes its output direction. */
template <typename T>
inline void output_stream_finish(T &&stream)
{
	// Normalize once and dispatch on the stable borrowed observer. Lifetime-
	// based automatic finish is owned by public output-operation guards instead.
	decltype(auto) ref{
		::fast_io::operations::output_stream_ref(stream)};
	::fast_io::operations::decay::output_stream_finish_decay(ref);
}

/**
 * @brief Consumes physical EOF and terminally validates an input transcoder.
 *
 * Unlike ordinary finite reads, this operation is explicitly destructive: it
 * discards all remaining decoded output in order to drive the engine to EOF.
 */
template <typename T>
inline void input_stream_drain_and_finish(T &&stream)
{
	// This explicit API is the caller's assertion that the remainder of the
	// logical message may be discarded in exchange for complete validation.
	decltype(auto) ref{
		::fast_io::operations::input_stream_ref(stream)};
	::fast_io::operations::decay::input_stream_drain_and_finish_decay(ref);
}

} // namespace fast_io::operations

namespace fast_io
{

using ::fast_io::operations::output_stream_finish;
using ::fast_io::operations::input_stream_drain_and_finish;

/** @brief Delegates explicit drain-and-finish to an input adapter owner. */
template <typename owner>
inline void input_stream_drain_and_finish_define(
	::fast_io::basic_itranscoder_ref<owner> ref)
{
	ref.ptr->drain_and_finish();
}

/** @brief Finishes only the input child of a duplex transcoder owner. */
template <typename owner>
inline void input_stream_drain_and_finish_define(
	::fast_io::basic_iotranscoder_ref<owner> ref)
{
	ref.ptr->drain_and_finish_input();
}

} // namespace fast_io
