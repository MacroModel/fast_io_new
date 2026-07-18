#pragma once

namespace fast_io::operations::decay
{

// `operations::decay` is below the public stream-normalization boundary. Its observer argument is therefore a
// forwarding reference only to preserve whether the already-normalized object is owned or borrowed; every named
// parameter is an lvalue and the implementation graph borrows it. This separation is important across ABIs: the
// normalization policy may deliberately return a small trivial observer by value on a register-friendly psABI, or
// retain a large/non-trivial observer by reference, without reopening either choice at every fallback edge.
template <typename outstmtype>
using write_decay_stream_t = ::std::remove_cvref_t<outstmtype>;

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
write_some_decay(outstmtype &&outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
				 typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	return ::fast_io::details::write_some_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr void write_all_decay(outstmtype &&outsm,
									  typename write_decay_stream_t<outstmtype>::output_char_type const *first,
									  typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	::fast_io::details::write_all_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_decay(outstmtype &&outsm, ::std::byte const *first,
														   ::std::byte const *last)
{
	return ::fast_io::details::write_some_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr void write_all_bytes_decay(outstmtype &&outsm, ::std::byte const *first, ::std::byte const *last)
{
	::fast_io::details::write_all_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t
scatter_write_some_decay(outstmtype &&outsm,
						 basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
						 ::std::size_t n)
{
	return ::fast_io::details::scatter_write_some_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes_decay(outstmtype &&outsm,
																	io_scatter_t const *pscatters,
																	::std::size_t n)
{
	return ::fast_io::details::scatter_write_some_bytes_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr void
scatter_write_all_decay(outstmtype &&outsm,
						basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
						::std::size_t n)
{
	::fast_io::details::scatter_write_all_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr void scatter_write_all_bytes_decay(outstmtype &&outsm, io_scatter_t const *pscatters,
													::std::size_t n)
{
	::fast_io::details::scatter_write_all_bytes_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
pwrite_some_decay(outstmtype &&outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
				  typename write_decay_stream_t<outstmtype>::output_char_type const *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pwrite_some_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr void pwrite_all_decay(outstmtype &&outsm,
									   typename write_decay_stream_t<outstmtype>::output_char_type const *first,
									   typename write_decay_stream_t<outstmtype>::output_char_type const *last,
									   ::fast_io::intfpos_t off)
{
	::fast_io::details::pwrite_all_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_decay(outstmtype &&outsm, ::std::byte const *first,
															::std::byte const *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pwrite_some_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_decay(outstmtype &&outsm, ::std::byte const *first, ::std::byte const *last,
											 ::fast_io::intfpos_t off)
{
	::fast_io::details::pwrite_all_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t
scatter_pwrite_some_decay(
	outstmtype &&outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pwrite_some_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_decay(outstmtype &&outsm,
																	 io_scatter_t const *pscatters,
																	 ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr void
scatter_pwrite_all_decay(
	outstmtype &&outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pwrite_all_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_decay(outstmtype &&outsm, io_scatter_t const *pscatters,
													 ::std::size_t n,
													 ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pwrite_all_bytes_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void char_put_decay(outstmtype &&outstm,
									 typename write_decay_stream_t<outstmtype>::output_char_type ch)
{
	::fast_io::details::char_put_impl(outstm, ch);
}

} // namespace fast_io::operations::decay
