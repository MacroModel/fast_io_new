#pragma once

namespace fast_io
{

namespace details
{

template <typename optstmtype, typename instmtype>
	requires(sizeof(typename optstmtype::output_char_type) == sizeof(typename instmtype::input_char_type))
inline constexpr void transmit_all_main_impl(optstmtype &optstm, instmtype &instm,
											 ::fast_io::uintfpos_t totransmit)
{
	/*
	A dummy placeholder implementation
	*/
	using input_char_type = typename instmtype::input_char_type;
	using output_char_type = typename optstmtype::output_char_type;
	constexpr ::std::size_t bfsz{::fast_io::details::transmit_buffer_size_cache<sizeof(input_char_type)>};
	::fast_io::details::local_operator_new_array_ptr<input_char_type> newptr(bfsz);
	input_char_type *buffer_start{newptr.ptr};
	while (totransmit)
	{
		::std::size_t this_round{bfsz};
		if (totransmit < this_round)
		{
			this_round = static_cast<::std::size_t>(totransmit);
		}
		auto iter{buffer_start + this_round};
		::fast_io::operations::decay::read_all_decay(instm, buffer_start, iter);
		if constexpr (::std::same_as<output_char_type, input_char_type>)
		{
			::fast_io::operations::decay::write_all_decay(optstm, buffer_start, iter);
		}
		else
		{
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
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_all_decay(optstm, unlocked_input, totransmit);
	}
	else
	{
		return ::fast_io::details::transmit_all_main_impl(optstm, instm, totransmit);
	}
}

} // namespace decay

template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_all(optstmtype &&optstm, instmtype &&instm, ::fast_io::uintfpos_t totransmit)
{
	// Normalize each source exactly once. A prvalue result is owned by its local; an ABI-selected stable reference is
	// preserved. Every subsequent transmit layer receives a named lvalue and therefore cannot introduce an observer copy.
	decltype(auto) output_observer{::fast_io::operations::output_stream_ref(optstm)};
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	return ::fast_io::operations::decay::transmit_all_decay(output_observer, input_observer, totransmit);
}

} // namespace operations

} // namespace fast_io
