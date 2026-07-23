#pragma once

namespace fast_io
{
namespace details
{

template <typename element_type>
inline constexpr element_type scatter_scalar_empty_anchor{};

template <typename element_type>
struct basic_scatter_scalar_range
{
	element_type const *first;
	element_type const *last;
};

/**
 * @brief Converts one scatter descriptor to the pointer pair required by a scalar output primitive.
 *
 * @details A zero-length scatter is allowed to carry a null base because it denotes no readable object.  C++ pointer
 *          arithmetic and subtraction, however, are defined only within an array object; consequently `base + 0` and
 *          a later `last - first` are not valid when both pointers are null.  A null empty descriptor is therefore
 *          represented by two pointers to a stable inline anchor.  A non-null empty base is deliberately preserved so
 *          observers that distinguish descriptor provenance keep seeing the original address.  Positive extents retain
 *          the normal scatter precondition that `base` begins a valid array range.
 *
 *          This normalization changes neither the byte sequence nor the strategy's exception boundary: callers retain
 *          their existing one scalar-range dispatch per descriptor, including an empty descriptor, and only the
 *          otherwise-invalid null pointer representation is replaced.
 *
 *          The empty arm is marked unlikely for optimizer cost modelling, not as a semantic precondition.  Apple Clang
 *          otherwise stops inlining a cold scatter loop even when every descriptor length is compile-time nonzero;
 *          the annotation restores instruction-for-instruction baseline code after constant propagation.  GCC keeps
 *          the same generic cold-loop layout with the annotation, while run-time empty descriptors remain supported.
 */
template <typename element_type>
inline constexpr basic_scatter_scalar_range<element_type>
scatter_to_scalar_range(element_type const *base, ::std::size_t len) noexcept
{
	if (len == 0u) [[unlikely]]
	{
		if (base == nullptr)
		{
			base = __builtin_addressof(scatter_scalar_empty_anchor<element_type>);
		}
		return {base, base};
	}
	return {base, base + len};
}

template <typename outstmtype>
inline constexpr typename outstmtype::output_char_type const *
pwrite_some_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
					  typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t);

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
																::std::byte const *last, ::fast_io::intfpos_t off);

template <typename outstmtype>
inline constexpr void pwrite_all_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
										   typename outstmtype::output_char_type const *last, ::fast_io::intfpos_t off);

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last,
												 ::fast_io::intfpos_t);

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
															   ::std::byte const *last);

template <typename outstmtype>
inline constexpr void write_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr typename outstmtype::output_char_type const *
write_some_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
					 typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		return write_some_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		::std::size_t len{static_cast<::std::size_t>(last - first)};
		basic_io_scatter_t<char_type> sc{first, len};
		return ::fast_io::scatter_status_one_size(
				   scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len) +
			   first;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		write_all_overflow_define(outsm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		// Scatter `all` returns void: successful return proves that its sole descriptor was consumed in full.
		basic_io_scatter_t<char_type> sc{first, static_cast<::std::size_t>(last - first)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		if constexpr (sizeof(typename outstmtype::output_char_type) == 1)
		{
			::std::byte const *firstptr{reinterpret_cast<::std::byte const *>(first)};
			::std::byte const *ptr{
				write_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last))};
			return ptr - firstptr + first;
		}
		else
		{
			::std::byte const *firstptr{reinterpret_cast<::std::byte const *>(first)};
			::std::byte const *ptr{
				write_some_bytes_cold_impl(outsm, firstptr, reinterpret_cast<::std::byte const *>(last))};
			::std::size_t diff{static_cast<::std::size_t>(ptr - firstptr)};
			::std::size_t v{diff / sizeof(char_type)};
			::std::size_t const partial{diff % sizeof(char_type)};
			if (partial != 0)
			{
				// A typed some-operation may report only complete elements. Complete the partially emitted element before
				// returning its successor; writing `partial` bytes here would leave the element truncated unless its size
				// happened to be exactly twice the observed prefix.
				write_all_bytes_cold_impl(outsm, ptr, ptr + (sizeof(char_type) - partial));
				++v;
			}
			return first + v;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto current_position{::fast_io::operations::decay::output_stream_seek_decay(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pwrite_some_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_decay(outsm, ret - first + current_position, ::fast_io::seekdir::beg);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, 0, ::fast_io::seekdir::cur)};
		::std::byte const *const first_bytes{reinterpret_cast<::std::byte const *>(first)};
		::std::byte const *const last_bytes{reinterpret_cast<::std::byte const *>(last)};
		::std::byte const *written{
			::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first_bytes, last_bytes, current_position)};
		::std::ptrdiff_t const byte_difference{written - first_bytes};
		::std::size_t const complete_characters{
			static_cast<::std::size_t>(byte_difference) / sizeof(char_type)};
		::std::size_t const partial_bytes{
			static_cast<::std::size_t>(byte_difference) % sizeof(char_type)};
		::std::size_t character_progress{complete_characters};
		if (partial_bytes != 0u)
		{
			::std::size_t const remaining_bytes{sizeof(char_type) - partial_bytes};
			auto const continuation_position{
				::fast_io::fposoffadd_nonegative(current_position, byte_difference)};
			::fast_io::details::pwrite_all_bytes_cold_impl(
				outsm, written, written + remaining_bytes, continuation_position);
			++character_progress;
		}
		// The selected concepts prove an exact byte seek result and at least one byte-positional output primitive.
		// The primitive progress contract keeps `written` in [first_bytes,last_bytes]. Passing the queried byte
		// position directly therefore preserves an unaligned current position; rounding through a character offset
		// would target preceding storage. Completing a partial character makes `character_progress` a typed cursor,
		// and the checked conversion below advances the sequential position by exactly that emitted prefix.
		auto const byte_progress{::fast_io::details::scatter_fpos_mul<char_type>(
			::fast_io::fposoffadd_nonegative(0, character_progress))};
		::fast_io::operations::decay::output_stream_seek_bytes_decay(
			outsm, byte_progress, ::fast_io::seekdir::cur);
		return first + character_progress;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::std::byte const *write_some_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first,
															   ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		return write_some_bytes_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		::std::size_t len{static_cast<::std::size_t>(last - first)};
		io_scatter_t sc{first, len};
		return ::fast_io::scatter_status_one_size(
				   scatter_write_some_bytes_overflow_define(outsm, __builtin_addressof(sc), 1), len) +
			   first;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		auto const result{write_some_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
													 reinterpret_cast<char_type_const_ptr>(last))};
		return reinterpret_cast<::std::byte const *>(result);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		::std::size_t len{static_cast<::std::size_t>(last - first)};
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_const_ptr>(first), len};
		return ::fast_io::scatter_status_one_size(
				   scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len) +
			   first;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		write_all_bytes_overflow_define(outsm, first, last);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{first, static_cast<::std::size_t>(last - first)};
		scatter_write_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		write_all_overflow_define(outsm, reinterpret_cast<char_type_const_ptr>(first),
								  reinterpret_cast<char_type_const_ptr>(last));
		return last;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		basic_io_scatter_t<char_type> sc{reinterpret_cast<char_type_const_ptr>(first),
										 static_cast<::std::size_t>(last - first)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
		return last;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, ret - first, ::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay(outsm, 0, ::fast_io::seekdir::cur)};
		auto ret{::fast_io::details::pwrite_some_bytes_cold_impl(outsm, first, last, current_position)};
		::fast_io::operations::decay::output_stream_seek_decay(outsm, ret - first, ::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void write_all_cold_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
										  typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype>)
	{
		write_all_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>)
	{
		basic_io_scatter_t<char_type> sc{first, static_cast<::std::size_t>(last - first)};
		scatter_write_all_overflow_define(outsm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			while ((first = write_some_overflow_define(outsm, first, last)) != last)
			{
				char_type *curr{obuffer_curr(outsm)};
				char_type *ed{obuffer_end(outsm)};
				::std::ptrdiff_t bfddiff{ed - curr};
				::std::ptrdiff_t itdiff{last - first};
				if (itdiff < bfddiff)
				{
					obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
					return;
				}
			}
		}
		else
		{
			while ((first = write_some_overflow_define(outsm, first, last)) != last)
				;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype>)
	{
		// The one-descriptor adapter must consume progress returned by the typed scatter customization. It cannot call
		// the byte customization (which the stream need not provide), and unlike an `all` primitive the result is not a
		// completion proof, so retry until the descriptor is complete.
		while (first != last)
		{
			::std::size_t const len{static_cast<::std::size_t>(last - first)};
			basic_io_scatter_t<char_type> sc{first, len};
			first += ::fast_io::scatter_status_one_size(
				scatter_write_some_overflow_define(outsm, __builtin_addressof(sc), 1), len);
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<
							outstmtype>))
	{
		write_all_bytes_cold_impl(outsm, reinterpret_cast<::std::byte const *>(first),
								  reinterpret_cast<::std::byte const *>(last));
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_cold_impl(outsm, first, last, current_position);
		::fast_io::operations::decay::output_stream_seek_decay(outsm, last - first, ::fast_io::seekdir::cur);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto firstbptr{reinterpret_cast<::std::byte const *>(first)};
		auto lastbptr{reinterpret_cast<::std::byte const *>(last)};
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_bytes_cold_impl(outsm, firstbptr, lastbptr, current_position);
		::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, lastbptr - firstbptr,
																	 ::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void write_all_bytes_cold_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outstmtype>)
	{
		write_all_bytes_overflow_define(outsm, first, last);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outstmtype>)
	{
		io_scatter_t sc{first, static_cast<::std::size_t>(last - first)};
		scatter_write_all_bytes_overflow_define(outsm, __builtin_addressof(sc), 1);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			while ((first = write_some_bytes_overflow_define(outsm, first, last)) != last)
			{
				char_type *curr{obuffer_curr(outsm)};
				char_type *ed{obuffer_end(outsm)};
				::std::ptrdiff_t bfddiff{ed - curr};
				::std::ptrdiff_t itdiff{last - first};
				if (itdiff < bfddiff)
				{
					obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
					return;
				}
			}
		}
		else
		{
			while ((first = write_some_bytes_overflow_define(outsm, first, last)) != last)
				;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outstmtype>)
	{
		while (first != last)
		{
			::std::size_t const len{static_cast<::std::size_t>(last - first)};
			io_scatter_t sc{first, len};
			first += ::fast_io::scatter_status_one_size(
				scatter_write_some_bytes_overflow_define(outsm, __builtin_addressof(sc), 1), len);
		}
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   (::fast_io::operations::decay::defines::has_write_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outstmtype>))
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		char_type_const_ptr firstcptr{reinterpret_cast<char_type_const_ptr>(first)};
		char_type_const_ptr lastcptr{reinterpret_cast<char_type_const_ptr>(last)};
		::fast_io::details::write_all_cold_impl(outsm, firstcptr, lastcptr);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_seek_bytes_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
	{
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_bytes_cold_impl(outsm, first, last, current_position);
		::fast_io::operations::decay::output_stream_seek_bytes_decay(outsm, last - first, ::fast_io::seekdir::cur);
	}
	else if constexpr (sizeof(char_type) == 1 &&
					   ::fast_io::operations::decay::defines::has_output_or_io_stream_seek_define<outstmtype> &&
					   (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>))
	{
		using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char_type const *;
		char_type_const_ptr firstcptr{reinterpret_cast<char_type_const_ptr>(first)};
		char_type_const_ptr lastcptr{reinterpret_cast<char_type_const_ptr>(last)};
		auto const current_position{
			::fast_io::operations::decay::output_stream_seek_decay(outsm, 0, ::fast_io::seekdir::cur)};
		::fast_io::details::pwrite_all_cold_impl(outsm, firstcptr, lastcptr, current_position);
		::fast_io::operations::decay::output_stream_seek_decay(outsm, lastcptr - firstcptr, ::fast_io::seekdir::cur);
	}
}

template <typename outstmtype>
inline constexpr typename outstmtype::output_char_type const *
write_some_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
				typename outstmtype::output_char_type const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			// The complete protocol proves guard storage, character identity, and strict type progress. Keeping the
			// recursive primitive inside this guard makes all fallback retries part of one logical, atomic write.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// Name the unlocked result exactly once. `decltype(auto)` owns a prvalue proxy for this call but preserves an
			// lvalue result as a reference, so recursive fallback neither dangles nor introduces an ABI-visible copy.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_some_impl(unlocked, first, last);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::ptrdiff_t bfddiff{ed - curr};
			::std::ptrdiff_t itdiff{last - first};
			if (itdiff < bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
				return last;
			}
		}
		return ::fast_io::details::write_some_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_impl(outstmtype &outsm, typename outstmtype::output_char_type const *first,
									 typename outstmtype::output_char_type const *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_all_impl(unlocked, first, last);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		using char_type = typename outstmtype::output_char_type;
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype>)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::ptrdiff_t bfddiff{ed - curr};
			::std::ptrdiff_t itdiff{last - first};
			if (itdiff < bfddiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				obuffer_set_curr(outsm, non_overlapped_copy_n(first, static_cast<::std::size_t>(itdiff), curr));
				return;
			}
		}
		::fast_io::details::write_all_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_impl(outstmtype &outsm, ::std::byte const *first,
														  ::std::byte const *last)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_some_bytes_impl(unlocked, first, last);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::ptrdiff_t bfddiff{ed - curr};
			::std::ptrdiff_t itdiff{last - first};
			if (itdiff < bfddiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				obuffer_set_curr(outsm, non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(first),
															  static_cast<::std::size_t>(itdiff), curr));
				return last;
			}
		}
		return ::fast_io::details::write_some_bytes_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_bytes_impl(outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::write_all_bytes_impl(unlocked, first, last);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		using char_type = typename outstmtype::output_char_type;
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstmtype> &&
					  sizeof(char_type) == 1)
		{
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			::std::ptrdiff_t bfddiff{ed - curr};
			::std::ptrdiff_t itdiff{last - first};
			if (itdiff < bfddiff)
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[likely]]
#endif
			{
				using char_type_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
					[[__gnu__::__may_alias__]]
#endif
					= char_type const *;
				obuffer_set_curr(outsm, non_overlapped_copy_n(reinterpret_cast<char_type_const_ptr>(first),
															  static_cast<::std::size_t>(itdiff), curr));
				return;
			}
		}
		::fast_io::details::write_all_bytes_cold_impl(outsm, first, last);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
char_put_cold_impl(outstmtype &outstm,
				   typename outstmtype::output_char_type ch)
{
	if constexpr (::fast_io::operations::decay::defines::has_output_stream_char_put_overflow_define<outstmtype>)
	{
		output_stream_char_put_overflow_define(outstm, ch);
	}
	else
	{
		::fast_io::details::write_all_impl(outstm, __builtin_addressof(ch), __builtin_addressof(ch) + 1);
	}
}

template <typename outstm>
inline constexpr void
char_put_impl(outstm &outsm, typename outstm::output_char_type ch)
{
	// `outsm` is already a normalized stream reference.  Reading the character type directly is essential after mutex
	// unwrapping: the unlocked reference is an internal leaf protocol and need not advertise another public
	// `output_stream_ref_define`.  Re-normalizing it here made the otherwise complete mutex protocol ill-formed.
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstm>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstm>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::char_put_impl(unlocked, ch);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstm>,
				"an output mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else
	{
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outstm>)
		{
			using char_type = typename outstm::output_char_type;
			char_type *curr{obuffer_curr(outsm)};
			char_type *ed{obuffer_end(outsm)};
			bool condition;
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_is_line_buffering_define<outstm>)
			{
				condition = curr < ed;
			}
			else
			{
				condition = curr != ed;
			}
			if (condition)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif

			{
				*curr = ch;
				obuffer_set_curr(outsm, curr + 1);
				return;
			}
		}
		::fast_io::details::char_put_cold_impl(outsm, ch);
	}
}

} // namespace details

} // namespace fast_io
