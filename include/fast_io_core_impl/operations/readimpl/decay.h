#pragma once

namespace fast_io::operations::decay
{

// `operations::decay` receives an observer whose public stream-reference policy has already selected value or
// reference transport. A forwarding reference preserves that ABI decision; every named parameter immediately becomes
// the stable lvalue consumed by the borrowed implementation graph. Reconstructing a value here would defeat the
// reference route for a large/non-trivial cursor and would duplicate buffered position state.
template <typename instmtype>
using read_decay_stream_t = ::std::remove_cvref_t<instmtype>;

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
read_some_decay(instmtype &&insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
			   typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	return ::fast_io::details::read_some_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr void read_all_decay(instmtype &&insm,
									 typename read_decay_stream_t<instmtype>::input_char_type *first,
									 typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	::fast_io::details::read_all_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last)
{
	return ::fast_io::details::read_some_bytes_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr void read_all_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last)
{
	::fast_io::details::read_all_bytes_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr io_scatter_status_t
scatter_read_some_decay(instmtype &&insm,
						basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
						::std::size_t n)
{
	return ::fast_io::details::scatter_read_some_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_bytes_decay(instmtype &&insm, io_scatter_t const *pscatters,
																   ::std::size_t n)
{
	return ::fast_io::details::scatter_read_some_bytes_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr void scatter_read_all_decay(instmtype &&insm,
											 basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
											 ::std::size_t n)
{
	::fast_io::details::scatter_read_all_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr void scatter_read_all_bytes_decay(instmtype &&insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	::fast_io::details::scatter_read_all_bytes_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
pread_some_decay(instmtype &&insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
				typename read_decay_stream_t<instmtype>::input_char_type *last,
				 ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pread_some_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr void pread_all_decay(instmtype &&insm,
									  typename read_decay_stream_t<instmtype>::input_char_type *first,
									  typename read_decay_stream_t<instmtype>::input_char_type *last,
									  ::fast_io::intfpos_t off)
{
	::fast_io::details::pread_all_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last,
													 ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pread_some_bytes_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr void pread_all_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last,
											::fast_io::intfpos_t off)
{
	::fast_io::details::pread_all_bytes_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr io_scatter_status_t
scatter_pread_some_decay(instmtype &&insm,
						 basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
						 ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pread_some_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_decay(instmtype &&insm, io_scatter_t const *pscatters,
																	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pread_some_bytes_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr void scatter_pread_all_decay(instmtype &&insm,
											  basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
											  ::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pread_all_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr void scatter_pread_all_bytes_decay(instmtype &&insm, io_scatter_t const *pscatters, ::std::size_t n,
													::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pread_all_bytes_impl(insm, pscatters, n, off);
}

} // namespace fast_io::operations::decay
