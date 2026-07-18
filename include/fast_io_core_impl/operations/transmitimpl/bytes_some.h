#pragma once

namespace fast_io
{

namespace details
{

template <typename optstmtype, typename instmtype>
inline constexpr ::fast_io::uintfpos_t transmit_bytes_some_main_impl(optstmtype &optstm, instmtype &instm,
																	 ::fast_io::uintfpos_t needtransmit)
{
	/*
	A dummy placeholder implementation
	*/
	constexpr ::std::size_t bfsz{::fast_io::details::transmit_buffer_size_cache<1>};
	::fast_io::details::local_operator_new_array_ptr<::std::byte> newptr(bfsz);
	::std::byte *buffer_start{newptr.ptr};
	::fast_io::uintfpos_t totransmit{needtransmit};
	while (totransmit)
	{
		::std::size_t this_round{bfsz};
		if (totransmit < this_round)
		{
			this_round = static_cast<::std::size_t>(totransmit);
		}
		::std::byte *iter{
			::fast_io::operations::decay::read_some_bytes_decay(instm, buffer_start, buffer_start + this_round)};
		if (iter == buffer_start)
		{
			break;
		}
		::fast_io::operations::decay::write_all_bytes_decay(optstm, buffer_start, iter);
		totransmit -= static_cast<::std::size_t>(iter - buffer_start);
	}
	return needtransmit - totransmit;
}

} // namespace details

namespace operations
{

namespace decay
{

template <typename optstmtype, typename instmtype>
	requires(::fast_io::operations::decay::defines::has_complete_transmit_mutex_protocols<optstmtype, instmtype>)
inline constexpr decltype(auto) transmit_bytes_some_decay(optstmtype &&optstm, instmtype &&instm,
														  ::fast_io::uintfpos_t totransmit)
{
	using output_observer_type = ::std::remove_cvref_t<optstmtype>;
	using input_observer_type = ::std::remove_cvref_t<instmtype>;
#if 0
	if constexpr(::fast_io::status_output_stream<optstmtype>)
	{
		return status_transmit_bytes_some_define(optstm,instm,totransmit);
	}
	else if constexpr(::fast_io::status_input_stream<instmtype>)
	{
		return status_transmit_bytes_some_define(optstm,instm,totransmit);
	}
	else
#endif
	if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
					  output_observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		decltype(auto) unlocked_output{
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm)};
		return ::fast_io::operations::decay::transmit_bytes_some_decay(unlocked_output, instm, totransmit);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<
						   input_observer_type>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		decltype(auto) unlocked_input{::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm)};
		return ::fast_io::operations::decay::transmit_bytes_some_decay(optstm, unlocked_input, totransmit);
	}
	else
	{
		return ::fast_io::details::transmit_bytes_some_main_impl(optstm, instm, totransmit);
	}
}

} // namespace decay

template <typename optstmtype, typename instmtype>
inline constexpr decltype(auto) transmit_bytes_some(optstmtype &&optstm, instmtype &&instm,
													::fast_io::uintfpos_t totransmit)
{
	decltype(auto) output_observer{::fast_io::operations::output_stream_ref(optstm)};
	decltype(auto) input_observer{::fast_io::operations::input_stream_ref(instm)};
	return ::fast_io::operations::decay::transmit_bytes_some_decay(output_observer, input_observer, totransmit);
}

} // namespace operations

} // namespace fast_io
