#pragma once

/*
 * Element transmit-until-EOF operation (IO level).
 *
 * This operation drains an input observer into an equal-width output observer,
 * tracks total progress through the extensible transmit-integer protocol, and
 * prefers provider whole-operation CPOs where available. Generic execution
 * composes normalized read/write primitives under one synchronization plan.
 * Logical EOF completes an input transcoder's terminal drain naturally; after
 * that success the output guard checked-finishes an eligible temporary owner.
 */

namespace fast_io
{

namespace details
{

/** @brief Transfers elements to logical EOF and accumulates generic progress. */
template <typename optstmtype, typename instmtype, typename T>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr void transmit_until_eof_generic_main_impl(optstmtype &optstm, instmtype &instm, T &resultint)
{
	::fast_io::details::transmit_copy_available<false>(optstm, instm, resultint);
}

/** @brief Transfers elements to EOF and returns the standard result type. */
template <typename optstmtype, typename instmtype>
inline constexpr ::fast_io::transmit_result transmit_until_eof_main_impl(optstmtype &optstm, instmtype &instm)
{
	::fast_io::uintfpos_t transmitted{};
	uintfpos_transmit_reference_wrapper wrapper{__builtin_addressof(transmitted)};
	if constexpr (::fast_io::details::transmit_direct_terminal_window<false, optstmtype, instmtype>)
	{
		if (::fast_io::details::transmit_try_direct_terminal_window<false>(optstm, instm, transmitted) ==
			transmit_terminal_window_result::complete) [[likely]]
		{
			return {transmitted};
		}
		::fast_io::details::transmit_terminal_window_miss<
			::fast_io::details::transmit_until_eof_generic_main_impl<optstmtype, instmtype,
				uintfpos_transmit_reference_wrapper>>(optstm, instm, wrapper);
	}
	else
	{
		::fast_io::details::transmit_until_eof_generic_main_impl(optstm, instm, wrapper);
	}
	return {transmitted};
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Borrows both observers and one accumulator through EOF mutex recursion. */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
inline constexpr decltype(auto) transmit_until_eof_generic_decay_borrowed(optstmtype &optstm, instmtype &instm,
																		  T &resultint)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock output across the complete EOF-driven transfer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(unlocked_output, instm,
																					   resultint);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock input only after output has reached an unlocked observer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(optstm, unlocked_input,
																					   resultint);
	}
	else
	{
		// Run the generic-result EOF loop on unlocked observers.
		return ::fast_io::details::transmit_until_eof_generic_main_impl(optstm, instm, resultint);
	}
}

/**
 * @brief Owns streams and the generic accumulator at the historical value boundary.
 * @details Recursive execution borrows all three named owners, so the progress
 *          adapter denotes one state throughout the complete transfer.
 */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
inline constexpr decltype(auto) transmit_until_eof_generic_decay(optstmtype optstm, instmtype instm, T resultint)
{
	return ::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed(optstm, instm, resultint);
}

/** @brief Selects each stream ABI independently while preserving accumulator identity. */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_until_eof_generic_decay_dispatch(optstmtype &optstm, instmtype &instm,
										  T &resultint)
{
	return ::fast_io::operations::decay::transmit_stream_pair_accumulator_dispatch<
		&::fast_io::operations::decay::transmit_until_eof_generic_decay_borrowed<optstmtype, instmtype, T>>(
		optstm, instm, resultint);
}

/** @brief Borrows stable observers through standard-result EOF mutex recursion. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_until_eof_decay_borrowed(optstmtype &optstm, instmtype &instm)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Hold the output lock through the complete logical input stream.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(unlocked_output, instm);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Hold the input lock after output mutex recursion is resolved.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(optstm, unlocked_input);
	}
	else if constexpr (::fast_io::details::has_transmit_eof_status<false, optstmtype, instmtype>)
	{
		// Both normalized and unlocked observers jointly prove the specialized EOF protocol.
		return status_transmit_until_eof_define(optstm, instm);
	}

	else
	{
		// Execute standard-result EOF transfer on the unlocked observers.
		return ::fast_io::details::transmit_until_eof_main_impl(optstm, instm);
	}
}

/** @brief Owns both observers at the historical standard-result value boundary. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_until_eof_decay(optstmtype optstm, instmtype instm)
{
	return ::fast_io::operations::decay::transmit_until_eof_decay_borrowed(optstm, instm);
}

/** @brief Independently selects value or borrowed transport for each named observer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr decltype(auto)
transmit_until_eof_decay_dispatch(optstmtype &optstm, instmtype &instm)
{
	return ::fast_io::operations::decay::transmit_stream_pair_dispatch<
		&::fast_io::operations::decay::transmit_until_eof_decay_borrowed<optstmtype, instmtype>>(
		optstm, instm);
}

} // namespace decay

/** @brief Transfers elements to EOF with a caller-selected progress accumulator. */
template <typename optstmtype, typename instmtype, typename T>
inline constexpr decltype(auto) transmit_until_eof_generic(optstmtype &&optstm, instmtype &&instm, T resultint)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::uses_checked_transmit_eof<false, false, optstmtype, instmtype>)
#endif
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
#if defined(__HERBCEPTIONS__)
	if constexpr (details::uses_checked_transmit_eof<false, false, optstmtype, instmtype>)
	{
		if constexpr (details::uses_checked_transmit_eof_loop<false, false, optstmtype, instmtype>)
			details::transmit_checked_eof<false>(guard.ref(), input_observer, resultint);
		else
			decay::transmit_until_eof_generic_decay_dispatch(guard.ref(), input_observer, resultint);
		details::finish_checked_transmit<optstmtype>(guard);
	}
	else
#endif
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_until_eof_generic_decay_dispatch(
			output_observer, input_observer, resultint);
	});
}

/** @brief Transfers elements to EOF and checked-finishes an eligible output. */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_until_eof(optstmtype &&optstm, instmtype &&instm)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::uses_checked_transmit_eof<false, true, optstmtype, instmtype>)
#endif
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
#if defined(__HERBCEPTIONS__)
	if constexpr (details::uses_checked_transmit_eof<false, true, optstmtype, instmtype>)
	{
		::fast_io::transmit_result result;
		if constexpr (details::uses_checked_transmit_eof_loop<false, true, optstmtype, instmtype>)
		{
			::fast_io::uintfpos_transmit_reference_wrapper progress{__builtin_addressof(result.transmitted)};
			details::transmit_checked_eof<false>(guard.ref(), input_observer, progress);
		}
		else
			result = decay::transmit_until_eof_decay_dispatch(guard.ref(), input_observer);
		details::finish_checked_transmit<optstmtype>(guard);
		return result;
	}
	else
#endif
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_until_eof_decay_dispatch(output_observer, input_observer);
	});
}

} // namespace operations

} // namespace fast_io
