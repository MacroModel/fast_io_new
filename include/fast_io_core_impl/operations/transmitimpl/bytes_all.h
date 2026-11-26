#pragma once

/*
 * Exact-count byte transmit operation (IO level).
 *
 * This byte-domain counterpart prefers a provider
 * `status_transmit_bytes_all_define` CPO and otherwise copies through the
 * normalized byte read/write primitives until the requested count completes.
 * Character interpretation is deliberately absent.
 * Checked output finish is applied only after the exact-count operation
 * succeeds; a finite transfer makes no terminal claim about its input.
 */

namespace fast_io
{

namespace operations
{

namespace decay
{

/** @brief Borrows stable observers while applying exact-byte mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto) transmit_bytes_all_decay_borrowed(optstmtype &optstm, instmtype &instm,
																  ::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_all_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_all_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock and unwrap the output for the complete logical byte transfer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input only after output mutex handling is complete.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Execute the generic byte loop once both observers are unlocked.
		return ::fast_io::details::transmit_exact_main_impl<true>(optstm, instm, totransmit);
	}
}

/** @brief Owns both normalized observers at the historical byte-value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_all_decay(optstmtype optstm, instmtype instm,
														 ::fast_io::uintfpos_t totransmit)
{
	// A true value signature exposes the target aggregate ABI; recursive work
	// borrows the two owner parameters and cannot create another observer copy.
	return ::fast_io::operations::decay::transmit_bytes_all_decay_borrowed(optstm, instm, totransmit);
}

/** @brief Independently selects value or borrowed byte-stream transport. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_bytes_all_decay_dispatch(optstmtype &optstm, instmtype &instm,
								  ::fast_io::uintfpos_t totransmit)
{
	return ::fast_io::operations::decay::transmit_stream_pair_count_dispatch<
		&::fast_io::operations::decay::transmit_bytes_all_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm, totransmit);
}

} // namespace decay

/**
 * @brief Transfers an exact byte count with checked temporary-output finish.
 *
 * The finite operation preserves the input position immediately after the
 * requested bytes; it intentionally does not validate or consume logical EOF.
 */
template <typename optstmtype, typename instmtype>
#if defined(__HERBCEPTIONS__)
	requires (!::fast_io::details::uses_checked_transmit_all<true, optstmtype, instmtype>)
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_bytes_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	::fast_io::operations::decay::transmit_bytes_all_decay_dispatch(guard.ref(), input_observer, totransmit);
	guard.commit();
}

#if defined(__HERBCEPTIONS__)
/** @brief Transfers an exact byte count using the checked endpoint and terminal-EOF ABI. */
template <typename optstmtype, typename instmtype>
	requires ::fast_io::details::uses_checked_transmit_all<true, optstmtype, instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
transmit_bytes_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit) throws
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	::fast_io::details::transmit_exact_checked_dispatch<true>(guard.ref(), input_observer, totransmit);
	::fast_io::details::finish_checked_transmit<optstmtype>(guard);
}
#endif

} // namespace operations

} // namespace fast_io
