#pragma once


namespace fast_io
{

namespace operations
{

template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *write_some(outstmtype &&outstm, char_type const *first, char_type const *last)
{
	// Normalize once at the public boundary. `decltype(auto)` materializes a value result exactly once and preserves a
	// reference result exactly; the entire primitive fallback graph then borrows this named observer.
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::write_some_impl(outsm, first, last);
}

template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all(outstmtype &&outstm, char_type const *first, char_type const *last)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::write_all_impl(outsm, first, last);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte const *write_some_bytes(outstmtype &&outstm, ::std::byte const *first,
													 ::std::byte const *last)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::write_some_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void write_all_bytes(outstmtype &&outstm, ::std::byte const *first, ::std::byte const *last)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::write_all_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes(outstmtype &&outstm, io_scatter_t const *pscatter,
															  ::std::size_t len)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_write_some_bytes_impl(outsm, pscatter, len);
}

template <typename outstmtype>
inline constexpr void scatter_write_all_bytes(outstmtype &&outstm, io_scatter_t const *pscatter, ::std::size_t len)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	::fast_io::details::scatter_write_all_bytes_impl(outsm, pscatter, len);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_write_some(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_write_some_impl(outsm, pscatter, len);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_write_all(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_write_all_impl(outsm, pscatter, len);
}

template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *pwrite_some(outstmtype &&outstm, char_type const *first, char_type const *last,
											  ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::pwrite_some_impl(outsm, first, last, off);
}

template <typename outstmtype, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all(outstmtype &&outstm, char_type const *first, char_type const *last,
								 ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::pwrite_all_impl(outsm, first, last, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::byte const *pwrite_some_bytes(outstmtype &&outstm, ::std::byte const *first,
													  ::std::byte const *last, ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::pwrite_some_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void pwrite_all_bytes(outstmtype &&outstm, ::std::byte const *first, ::std::byte const *last,
									   ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::pwrite_all_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes(outstmtype &&outstm, io_scatter_t const *pscatter,
															   ::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, pscatter, len, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pwrite_all_bytes(outstmtype &&outstm, io_scatter_t const *pscatter, ::std::size_t len,
											   ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	::fast_io::details::scatter_pwrite_all_bytes_impl(outsm, pscatter, len, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_pwrite_some_impl(outsm, pscatter, len, off);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scatter_pwrite_all(
	outstmtype &&outstm,
	basic_io_scatter_t<typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type> const
		*pscatter,
	::std::size_t len, ::fast_io::intfpos_t off)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	return ::fast_io::details::scatter_pwrite_all_impl(outsm, pscatter, len, off);
}

/**
 * @brief Writes a single character to the output stream.
 * @tparam outstmtype The type of the output stream to write to, which should satisfy the output_stream concept.
 * @param outstm The output stream to write to.
 * @param ch The character to write to the output stream.
 * @note This function is marked constexpr, allowing its invocation in constant expressions.
 */
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void char_put(outstmtype &&outstm,
							   typename ::std::remove_cvref_t<
								   decltype(::fast_io::operations::output_stream_ref(outstm))>::output_char_type ch)
{
	decltype(auto) outsm = ::fast_io::operations::output_stream_ref(outstm);
	::fast_io::details::char_put_impl(outsm, ch);
}

} // namespace operations

} // namespace fast_io
