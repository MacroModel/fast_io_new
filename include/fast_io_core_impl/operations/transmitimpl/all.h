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

namespace details
{

/** @brief Copies an exact element count through a bounded temporary buffer. */
template <typename optstmtype, typename instmtype>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr void transmit_all_main_impl(optstmtype &optstm, instmtype &instm,
											 ::fast_io::uintfpos_t totransmit)
{
	// Generic transfer intentionally stages through a fixed-size local buffer;
	// provider-specific zero-copy strategies belong to status transmit CPOs.
	using input_char_type = typename instmtype::input_char_type;
	using output_char_type = typename optstmtype::output_char_type;
	constexpr ::std::size_t bfsz{::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>};
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(bfsz);
	input_char_type *buffer_start{newptr.ptr};
	while (totransmit)
	{
		// Move one bounded block while preserving the exact remaining count.
		::std::size_t this_round{bfsz};
		if (totransmit < this_round)
		{
			// Limit the final iteration to the exact remaining element count.
			this_round = static_cast<::std::size_t>(totransmit);
		}
		auto iter{buffer_start + this_round};
		::fast_io::operations::decay::read_all_decay(instm, buffer_start, iter);
		if constexpr (::std::same_as<output_char_type, input_char_type>)
		{
			// Matching character types can reuse the input buffer directly.
			::fast_io::operations::decay::write_all_decay(optstm, buffer_start, iter);
		}
		else
		{
			// Equal-width distinct character types require an alias-safe view.
			using output_char_type_may_alias_const_ptrtp
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= output_char_type const *;
			::fast_io::operations::decay::write_all_decay(
				optstm, reinterpret_cast<output_char_type_may_alias_const_ptrtp>(buffer_start),
				reinterpret_cast<output_char_type_may_alias_const_ptrtp>(iter));
		}
		totransmit -= this_round;
	}
}

} // namespace details

namespace operations
{

namespace decay
{

/** @brief Applies mutex recursion before executing exact-count element transfer. */
template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_all_decay(optstmtype &&optstm, instmtype &&instm,
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
		return ::fast_io::operations::decay::transmit_all_decay(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		// Lock and unwrap input only when output needs no mutex recursion.
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_all_decay(optstm, unlocked_input, totransmit);
	}
	else
	{
		// Both observers are ready for the generic exact-count transfer loop.
		return ::fast_io::details::transmit_all_main_impl(optstm, instm, totransmit);
	}
}

} // namespace decay

/**
 * @brief Transfers an exact element count with checked temporary-output finish.
 *
 * Input normalization is lifetime-only and never drains past `totransmit`;
 * output commit occurs only after the entire transfer succeeds.
 */
template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	::fast_io::operations::basic_output_operation_guard<optstmtype &&> guard{optstm};
	return ::fast_io::operations::output_operation_guard_invoke(guard, [&](auto &output_observer) -> decltype(auto) {
		return ::fast_io::operations::decay::transmit_all_decay(output_observer, input_observer, totransmit);
	});
}

} // namespace operations

} // namespace fast_io
