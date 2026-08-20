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
	// Generic EOF transfer uses ordinary byte writes so the payload remains
	// opaque even when the input stream has a wider character type.
	using input_char_type = typename instmtype::input_char_type;
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(
		::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>);
	input_char_type *buffer_start{newptr.ptr};
	input_char_type *buffer_end{newptr.ptr + newptr.size};
	for (input_char_type *iter;
		 (iter = ::fast_io::operations::decay::read_some_decay(instm, buffer_start, buffer_end)) != buffer_start;)
	{
		// Forward each nonempty input block and accumulate its byte extent.
		auto bufferstartpbyte{reinterpret_cast<::std::byte const *>(buffer_start)};
		auto iterpbyte{reinterpret_cast<::std::byte const *>(iter)};
		::std::size_t off{static_cast<::std::size_t>(iterpbyte - bufferstartpbyte)};
		::fast_io::operations::decay::write_all_bytes_decay(optstm, bufferstartpbyte, iterpbyte);
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
