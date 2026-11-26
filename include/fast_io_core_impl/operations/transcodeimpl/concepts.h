#pragma once

/**
 * @file
 * @brief Provides exact CPO recognition for the bounded transform protocol.
 *
 * The generic `transcoder_ref<T>` overloads forward to CPOs on `T&`. A provider
 * that needs a different observer can instead customize `transcode_ref_define`
 * and place the process/drain CPOs on that observer type.
 */

#include "../../concepts/transcode_protocol.h"

namespace fast_io
{

/** @brief Obtains the source unit type advertised by an engine observer. */
template <typename T>
using transcode_from_value_t = typename ::std::remove_cvref_t<T>::from_value_type;

/** @brief Obtains the destination unit type advertised by an engine observer. */
template <typename T>
using transcode_to_value_t = typename ::std::remove_cvref_t<T>::to_value_type;

/** @brief Forwards a bounded process step through the default engine observer. */
template <typename T>
	requires requires(T &engine, typename T::from_value_type const *from,
					  typename T::to_value_type *to) {
		{ transcode_process_define(engine, from, from, to, to) } -> ::std::same_as<::fast_io::basic_transcode_process_result<
			typename T::from_value_type, typename T::to_value_type>>;
	}
inline constexpr auto transcode_process_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::from_value_type const *from_first,
	typename T::from_value_type const *from_last,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_process_define(*ref.ptr, from_first, from_last, to_first, to_last)))
{
	return transcode_process_define(
		*ref.ptr, from_first, from_last, to_first, to_last);
}

/** @brief Forwards a nonterminal sync-flush step through the default observer. */
template <typename T>
	requires requires(typename T::to_value_type *to) {
		{ transcode_sync_flush_define(::std::declval<T &>(), to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			typename T::to_value_type>>;
	}
inline constexpr auto transcode_sync_flush_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_sync_flush_define(*ref.ptr, to_first, to_last)))
{
	return transcode_sync_flush_define(*ref.ptr, to_first, to_last);
}

/** @brief Forwards a terminal finish step through the default engine observer. */
template <typename T>
	requires requires(typename T::to_value_type *to) {
		{ transcode_finish_define(::std::declval<T &>(), to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			typename T::to_value_type>>;
	}
inline constexpr auto transcode_finish_define(
	::fast_io::transcoder_ref<T> ref,
	typename T::to_value_type *to_first,
	typename T::to_value_type *to_last) noexcept(noexcept(transcode_finish_define(*ref.ptr, to_first, to_last)))
{
	return transcode_finish_define(*ref.ptr, to_first, to_last);
}

/** @brief Forwards the minimum-capacity query from an observer to its engine. */
template <typename T>
	requires requires(::fast_io::transcode_phase phase) {
		{ transcode_min_output_size_define(
			::fast_io::transcode_reserve<T>, phase) } -> ::std::same_as<::std::size_t>;
	}
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<::fast_io::transcoder_ref<T>>,
	::fast_io::transcode_phase phase) noexcept(noexcept(transcode_min_output_size_define(::fast_io::transcode_reserve<T>, phase)))
{
	return transcode_min_output_size_define(
		::fast_io::transcode_reserve<T>, phase);
}

template <typename T>
concept transcode_buffer_sizes_provider = requires {
	{ transcode_preferred_buffer_sizes_define(transcode_reserve<T>) } -> ::std::same_as<transcode_buffer_sizes>;
	typename ::std::integral_constant<transcode_buffer_sizes, transcode_preferred_buffer_sizes_define(transcode_reserve<T>)>;
};

/**
 * @brief Certifies that a converter accepts separately delivered prefixes of one logical record.
 * @details Replacing one process range with any ordered partition must preserve the encoded stream
 * and its terminal state. Each returned output prefix can become visible immediately, including if
 * a later field fails. Incomplete input stays in the engine until more arrives or the owner finishes.
 * Splitting a range never implies a flush, an authentication boundary, or a second finish.
 */
template <typename T>
concept prefix_transcoder = requires {
	{ transcode_output_prefixes_define(transcode_reserve<T>) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief A custom observer can publish its own batch sizes, just like its minimum output capacity. */
template <transcode_buffer_sizes_provider T>
inline constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<transcoder_ref<T>>) noexcept
{
	return transcode_preferred_buffer_sizes_define(transcode_reserve<T>);
}

/** @brief Forwards the maximum-output query from an observer to its engine. */
template <typename T>
	requires requires(::std::size_t input_units,
					  ::fast_io::transcode_phase phase) {
		{ transcode_max_output_size_define(
			::fast_io::transcode_reserve<T>, input_units, phase) } -> ::std::same_as<::std::size_t>;
	}
inline constexpr ::std::size_t transcode_max_output_size_define(
	::fast_io::transcode_reserve_t<::fast_io::transcoder_ref<T>>,
	::std::size_t input_units, ::fast_io::transcode_phase phase) noexcept(noexcept(transcode_max_output_size_define(::fast_io::transcode_reserve<T>, input_units, phase)))
{
	return transcode_max_output_size_define(
		::fast_io::transcode_reserve<T>, input_units, phase);
}

namespace operations::decay::defines
{

/** @brief Requires valid and supported source and destination endpoint units. */
template <typename T>
concept has_transcode_endpoint_types = requires {
	typename ::fast_io::transcode_from_value_t<T>;
	typename ::fast_io::transcode_to_value_t<T>;
	requires ::fast_io::transcode_unit<::fast_io::transcode_from_value_t<T>>;
	requires ::fast_io::transcode_unit<::fast_io::transcode_to_value_t<T>>;
};

/** @brief Detects an exact bounded process CPO with the required result type. */
template <typename T>
concept has_transcode_process_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_from_value_t<T> const *from,
			 ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_process_define(ref, from, from, to, to) } -> ::std::same_as<::fast_io::basic_transcode_process_result<
			::fast_io::transcode_from_value_t<T>,
			::fast_io::transcode_to_value_t<T>>>;
	};

/**
 * @brief Recognizes a checked step with the same source and destination cursors as the ordinary step.
 * @details The error channel may differ. On success the driver uses both returned cursors to
 * commit input and output, so their code-unit types and progress protocol must agree exactly.
 */
template <typename T>
concept has_transcode_process_checked_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_from_value_t<T> const *from, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_process_checked_define(ref, from, from, to, to) } -> ::std::same_as<
			::fast_io::basic_transcode_process_result<::fast_io::transcode_from_value_t<T>, ::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Recognizes a checked finish that publishes a bounded prefix in the engine's output units. */
template <typename T>
concept has_transcode_finish_checked_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_finish_checked_define(ref, to, to) } -> ::std::same_as<
			::fast_io::basic_transcode_drain_result<::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects an exact nonterminal sync-flush CPO. */
template <typename T>
concept has_transcode_sync_flush_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_sync_flush_define(ref, to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects the mandatory terminal finish CPO. */
template <typename T>
concept has_transcode_finish_define =
	has_transcode_endpoint_types<T> &&
	requires(T &ref, ::fast_io::transcode_to_value_t<T> *to) {
		{ transcode_finish_define(ref, to, to) } -> ::std::same_as<::fast_io::basic_transcode_drain_result<
			::fast_io::transcode_to_value_t<T>>>;
	};

/** @brief Detects the mandatory minimum-output-capacity query CPO. */
template <typename T>
concept has_transcode_min_output_size_define =
	has_transcode_endpoint_types<T> &&
	requires(::fast_io::transcode_phase phase) {
		{ transcode_min_output_size_define(
			::fast_io::transcode_reserve<::std::remove_cvref_t<T>>, phase) } -> ::std::same_as<::std::size_t>;
	};

/** @brief Detects the optional per-input maximum-output-size query CPO. */
template <typename T>
concept has_transcode_max_output_size_define =
	has_transcode_endpoint_types<T> &&
	requires(::std::size_t input_units,
			 ::fast_io::transcode_phase phase) {
		{ transcode_max_output_size_define(
			::fast_io::transcode_reserve<::std::remove_cvref_t<T>>,
			input_units, phase) } -> ::std::same_as<::std::size_t>;
	};

} // namespace operations::decay::defines

/**
 * @brief Proves that a drain accepts an empty destination and can never emit a trailer.
 * @details The minimum and maximum refer to different obligations. A zero minimum permits an
 * empty writable range. A zero maximum at zero new input bounds the output across every
 * reachable engine state, including partial scalars. Together these facts let an operation
 * run validation without acquiring a child window. The drain still runs and can fail.
 */
template <typename Ref, transcode_phase Phase>
concept outputless_transcode_drain = Phase != transcode_phase::process && requires {
	typename ::std::enable_if_t<transcode_min_output_size_define(transcode_reserve<Ref>, Phase) == 0u>;
	typename ::std::enable_if_t<transcode_max_output_size_define(transcode_reserve<Ref>, 0u, Phase) == 0u>;
};

/**
 * @brief Recognizes publication of all previously committed output through a normalized observer.
 * @details The operation must make committed units visible to the underlying consumer, preserve the
 * logical position and leave the stream open. Repeated publication without intervening output must
 * be idempotent. A device with independently managed capacity can provide this operation separately
 * from refilling a full put area. Existing buffered devices retain their normal flush protocol.
 */
template <typename Ref>
concept output_stream_publishable = requires(Ref &ref) {
	{ output_stream_publish_define(ref) } -> ::std::same_as<void>;
};

/** @brief Requires the complete mandatory bounded transcoder observer protocol. */
template <typename T>
concept transcode_engine_ref =
	::fast_io::operations::decay::defines::has_transcode_process_define<T> &&
	::fast_io::operations::decay::defines::has_transcode_finish_define<T> &&
	::fast_io::operations::decay::defines::has_transcode_min_output_size_define<T>;

/** @brief Requires an engine whose normalized observer satisfies the protocol. */
template <typename T>
concept bounded_transcoder = requires(T &engine) {
	requires ::fast_io::transcode_engine_ref<
		decltype(::fast_io::transcode_ref(engine))>;
};

/**
 * @brief Recognizes a transform independently of its number of steps or its scheduling mechanism.
 * @details A bounded engine consumes and produces prefixes of typed ranges. A duplex engine supplies the
 * next wire direction and commits a prefix before advancing. Both retain state across short completions.
 * The operation binding chooses the capabilities needed by its adapter at compile time.
 */
template <typename T>
concept transcoder = bounded_transcoder<T> || duplex_protocol_engine<T>;

} // namespace fast_io
