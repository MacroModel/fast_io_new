#pragma once

/**
 * @file
 * @brief Defines normalized bounded transform operations.
 *
 * Decay entry points borrow an already-normalized engine reference. Public
 * entry points normalize an engine exactly once for a complete primitive call.
 * Stream adapters should retain one normalized reference across their loops and
 * use the decay entry points directly.
 */

namespace fast_io
{

namespace operations::decay
{

/** @brief Dispatches one bounded process step on a normalized engine observer. */
template <typename T>
	requires ::fast_io::operations::decay::defines::has_transcode_process_define<T>
inline constexpr auto transcode_process_decay(
	T &&ref, ::fast_io::transcode_from_value_t<T> const *from_first,
	::fast_io::transcode_from_value_t<T> const *from_last,
	::fast_io::transcode_to_value_t<T> *to_first,
	::fast_io::transcode_to_value_t<T> *to_last) noexcept(noexcept(transcode_process_define(ref, from_first, from_last, to_first, to_last)))
{
	return transcode_process_define(
		ref, from_first, from_last, to_first, to_last);
}

/** @brief Dispatches optional sync-flush or reports immediate completion. */
template <typename T>
	requires ::fast_io::operations::decay::defines::has_transcode_endpoint_types<T>
inline constexpr auto transcode_sync_flush_decay(
	T &&ref, ::fast_io::transcode_to_value_t<T> *to_first,
	::fast_io::transcode_to_value_t<T> *to_last)
{
	if constexpr (::fast_io::operations::decay::defines::
					  has_transcode_sync_flush_define<T>)
	{
		// Use the engine's nonterminal drain when it buffers pending output.
		return transcode_sync_flush_define(ref, to_first, to_last);
	}
	else
	{
		// Stateless engines are synchronized without producing additional units.
		// Sync flush is optional. An engine without buffered nonterminal output is
		// already synchronized from the transform protocol's perspective.
		return ::fast_io::basic_transcode_drain_result<
			::fast_io::transcode_to_value_t<T>>{
			to_first, ::fast_io::transcode_drain_status::complete};
	}
}

/** @brief Dispatches one mandatory terminal finish step on an observer. */
template <typename T>
	requires ::fast_io::operations::decay::defines::has_transcode_finish_define<T>
inline constexpr auto transcode_finish_decay(
	T &&ref, ::fast_io::transcode_to_value_t<T> *to_first,
	::fast_io::transcode_to_value_t<T> *to_last) noexcept(noexcept(transcode_finish_define(ref, to_first, to_last)))
{
	return transcode_finish_define(ref, to_first, to_last);
}

/** @brief Queries mandatory minimum destination capacity on an observer type. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_transcode_min_output_size_define<T>
	inline constexpr ::std::size_t transcode_min_output_size_decay(
		::fast_io::transcode_reserve_t<T> reserve,
		::fast_io::transcode_phase phase) noexcept(noexcept(transcode_min_output_size_define(reserve, phase)))
{
	return transcode_min_output_size_define(reserve, phase);
}

/** @brief Queries an optional input-sensitive destination-capacity upper bound. */
template <typename T>
	requires ::fast_io::operations::decay::defines::
		has_transcode_max_output_size_define<T>
	inline constexpr ::std::size_t transcode_max_output_size_decay(
		::fast_io::transcode_reserve_t<T> reserve, ::std::size_t input_units,
		::fast_io::transcode_phase phase) noexcept(noexcept(transcode_max_output_size_define(reserve, input_units, phase)))
{
	return transcode_max_output_size_define(
		reserve, input_units, phase);
}

} // namespace operations::decay

namespace operations
{

/** @brief Names the normalized observer type selected for an engine. */
template <typename T>
using transcode_engine_ref_t = ::std::remove_cvref_t<
	decltype(::fast_io::transcode_ref(::std::declval<T &>()))>;

/** @brief Normalizes an engine once and performs one bounded process step. */
template <typename T>
	requires ::fast_io::transcoder<T>
inline constexpr auto transcode_process(
	T &engine,
	::fast_io::transcode_from_value_t<transcode_engine_ref_t<T>> const *from_first,
	::fast_io::transcode_from_value_t<transcode_engine_ref_t<T>> const *from_last,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_first,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_last)
{
	decltype(auto) ref{::fast_io::transcode_ref(engine)};
	return ::fast_io::operations::decay::transcode_process_decay(
		ref, from_first, from_last, to_first, to_last);
}

/** @brief Normalizes an engine once and performs one nonterminal sync-flush step. */
template <typename T>
	requires ::fast_io::transcoder<T>
inline constexpr auto transcode_sync_flush(
	T &engine,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_first,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_last)
{
	decltype(auto) ref{::fast_io::transcode_ref(engine)};
	return ::fast_io::operations::decay::transcode_sync_flush_decay(
		ref, to_first, to_last);
}

/** @brief Normalizes an engine once and performs one terminal finish step. */
template <typename T>
	requires ::fast_io::transcoder<T>
inline constexpr auto transcode_finish(
	T &engine,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_first,
	::fast_io::transcode_to_value_t<transcode_engine_ref_t<T>> *to_last)
{
	decltype(auto) ref{::fast_io::transcode_ref(engine)};
	return ::fast_io::operations::decay::transcode_finish_decay(
		ref, to_first, to_last);
}

/** @brief Queries the minimum legal destination capacity for a protocol phase. */
template <typename T>
	requires ::fast_io::transcoder<T>
inline constexpr ::std::size_t transcode_min_output_size(
	::fast_io::transcode_reserve_t<T>, ::fast_io::transcode_phase phase)
{
	using ref_type = transcode_engine_ref_t<T>;
	return ::fast_io::operations::decay::transcode_min_output_size_decay(
		::fast_io::transcode_reserve<ref_type>, phase);
}

/** @brief Queries an optional maximum output size for a bounded input count. */
template <typename T>
	requires(::fast_io::transcoder<T> &&
			 ::fast_io::operations::decay::defines::
				 has_transcode_max_output_size_define<transcode_engine_ref_t<T>>)
inline constexpr ::std::size_t transcode_max_output_size(
	::fast_io::transcode_reserve_t<T>, ::std::size_t input_units,
	::fast_io::transcode_phase phase)
{
	// This optional query is an allocation optimization only. Adapter
	// correctness is based exclusively on bounded calls and minimum capacity.
	using ref_type = transcode_engine_ref_t<T>;
	return ::fast_io::operations::decay::transcode_max_output_size_decay(
		::fast_io::transcode_reserve<ref_type>, input_units, phase);
}

} // namespace operations

} // namespace fast_io
