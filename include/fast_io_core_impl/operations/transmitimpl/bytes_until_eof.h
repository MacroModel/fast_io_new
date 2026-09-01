#pragma once

/*
 * Byte transmit-until-EOF operation (IO level).
 *
 * This byte-domain counterpart drains an input to an output while reporting
 * total byte progress, preferring provider status CPOs and otherwise composing
 * normalized byte read/write operations. It does not decode, transcode, format,
 * or scan the transferred payload.
 * Reaching logical input EOF naturally drives an input transcoder's finish;
 * the guarded temporary output is then checked-finished before return.
 */

namespace fast_io
{

namespace details
{

/** @brief Transfers bytes to logical EOF and accumulates progress generically. */
template <typename optstmtype, typename instmtype, typename T>
inline constexpr void transmit_bytes_until_eof_generic_main_impl(optstmtype &optstm, instmtype &instm, T &resultint)
{
	/*
	A byte transmit operation is closed over the byte CPO domain on both sides.
	Using input_char_type here would silently require a typed read, reject valid
	byte-only observers, and round progress to sizeof(input_char_type).  A byte
	buffer plus read_some_bytes/write_all_bytes instead proves that every returned
	cursor denotes the exact opaque byte interval [buffer_start, iter), including
	odd lengths for wide-character observers.
	*/
	::fast_io::details::local_operator_new_array_ptr<::std::byte> newptr(
		::fast_io::details::transmit_buffer_size_cache<1>);
	::std::byte *buffer_start{newptr.ptr};
	::std::byte *buffer_end{newptr.ptr + newptr.size};
	for (::std::byte *iter;
		 (iter = ::fast_io::operations::decay::read_some_bytes_decay(instm, buffer_start, buffer_end)) != buffer_start;)
	{
		// Pointer subtraction is now already expressed in the operation's byte unit.
		::std::size_t off{static_cast<::std::size_t>(iter - buffer_start)};
		::fast_io::operations::decay::write_all_bytes_decay(optstm, buffer_start, iter);
		transmit_integer_add_define(resultint, off);
	}
}

/** @brief Transfers bytes to logical EOF and returns the standard result type. */
template <typename optstmtype, typename instmtype>
inline constexpr ::fast_io::transmit_result transmit_bytes_until_eof_main_impl(optstmtype &optstm, instmtype &instm)
{
	::fast_io::uintfpos_t transmitted{};
	uintfpos_transmit_reference_wrapper wrapper{__builtin_addressof(transmitted)};
	::fast_io::details::transmit_bytes_until_eof_generic_main_impl(optstm, instm, wrapper);
	return {transmitted};
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Applies mutex recursion to generic-result byte EOF transfer. */
template <typename optstmtype, typename instmtype, typename T>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype> &&
			 ::fast_io::details::transmit_integer_wrapper<::std::remove_cvref_t<T>>)
inline constexpr decltype(auto) transmit_bytes_until_eof_generic_decay(optstmtype &&optstm, instmtype &&instm,
																	   T &&resultint)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_until_eof_generic_define(
			optstm,instm,resultint);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock output across every read/write iteration through logical EOF.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay(unlocked_output, instm, resultint);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock input only after output has reached its unlocked observer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay(optstm, unlocked_input, resultint);
	}
	else
	{
		// Run the generic byte EOF loop once both observers are unlocked.
		return ::fast_io::details::transmit_bytes_until_eof_generic_main_impl(optstm, instm, resultint);
	}
}

/** @brief Applies mutex recursion to standard-result byte EOF transfer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_until_eof_decay(optstmtype &&optstm, instmtype &&instm)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_until_eof_define(optstm,instm);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_until_eof_define(optstm,instm);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		// Lock output for the complete EOF-driven transfer.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_until_eof_decay(unlocked_output, instm);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input after output mutex recursion is complete.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_until_eof_decay(optstm, unlocked_input);
	}
	else
	{
		// Execute the standard-result byte EOF loop on unlocked observers.
		return ::fast_io::details::transmit_bytes_until_eof_main_impl(optstm, instm);
	}
}

} // namespace decay

/** @brief Transfers bytes to EOF with a caller-selected progress accumulator. */
template <typename optstmtype, typename instmtype, typename T>
inline constexpr decltype(auto) transmit_bytes_until_eof_generic(optstmtype &&optstm, instmtype &&instm, T resultint)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_bytes_until_eof_generic_decay(
			output_observer, input_observer, resultint);
	});
}

/** @brief Transfers bytes to EOF and checked-finishes an eligible output. */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_bytes_until_eof(optstmtype &&optstm, instmtype &&instm)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_bytes_until_eof_decay(output_observer, input_observer);
	});
}

} // namespace operations

} // namespace fast_io
