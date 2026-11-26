#pragma once

/*
 * Exact-count element transmit operation (IO level).
 *
 * `transmit_all` moves the requested number of equal-width input/output
 * elements, preferring a provider `status_transmit_all_define` operation and
 * otherwise using the normalized read/write matrix until complete. It treats
 * payload units opaquely and adds no formatting or scanning semantics.
 * The public output is guarded for success-only finish of opted-in temporaries;
 * the input is never implicitly drained beyond the requested element count.
 */

namespace fast_io
{

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying exact-count mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto) transmit_all_decay_borrowed(optstmtype &optstm, instmtype &instm,
															::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_all_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_all_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock and unwrap the output for the complete logical transfer.
		// Protocol admission proves that recursive unwrapping preserves the output character domain and changes type;
		// the guard therefore protects the complete logical transmit rather than only one primitive write.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_all_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input only when output needs no mutex recursion.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_all_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Both observers are ready for the generic exact-count transfer loop.
		return ::fast_io::details::transmit_exact_main_impl<false>(optstm, instm, totransmit);
	}
}

/**
 * @brief Owns both normalized observers at the historical value-decay boundary.
 * @details The parameters, not forwarding references, preserve the aggregate ABI
 *          selected for an explicit owner call; recursion subsequently borrows them.
 */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_all_decay(optstmtype optstm, instmtype instm,
												   ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_all_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed transport for each named observer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_all_decay_dispatch(optstmtype &optstm, instmtype &instm,
							::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_all_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/**
 * @brief Transfers an exact element count with checked temporary-output finish.
 *
 * Input normalization is lifetime-only and never drains past `totransmit`;
 * output commit occurs only after the entire transfer succeeds.
 */
template <typename optstmtype, typename instmtype>
#if defined(__HERBCEPTIONS__)
	requires (!::fast_io::details::uses_checked_transmit_all<false, optstmtype, instmtype>)
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	::fast_io::operations::decay::transmit_all_decay_dispatch(guard.ref(), input_observer, totransmit);
	guard.commit();
}

#if defined(__HERBCEPTIONS__)
/** @brief Transfers an exact element count using the checked endpoint and terminal-EOF ABI. */
template <typename optstmtype, typename instmtype>
	requires ::fast_io::details::uses_checked_transmit_all<false, optstmtype, instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
transmit_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit) throws
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	::fast_io::details::transmit_exact_checked_dispatch<false>(guard.ref(), input_observer, totransmit);
	::fast_io::details::finish_checked_transmit<optstmtype>(guard);
}
#endif

} // namespace operations

} // namespace fast_io
