#pragma once

namespace fast_io
{

namespace details
{
template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_cold_impl(outstmtype &outsm,
																		 io_scatter_t const *pscatters, ::std::size_t n,
																		 ::fast_io::intfpos_t off);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_pwrite_all_bytes_cold_impl(outstmtype &outsm, io_scatter_t const *pscatters,
												 ::std::size_t n, ::fast_io::intfpos_t off);

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_pwrite_all_cold_impl(outstmtype &outsm,
							 basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							 ::std::size_t n, ::fast_io::intfpos_t off);

// Keep the typed and byte positional helpers distinct. basic_io_scatter_t<char_type>::len is measured in char_type
// elements, whereas io_scatter_t::len is measured in bytes. Their layouts can be reinterpreted only when
// sizeof(char_type) == 1; otherwise the typed helper must own unit conversion and positional-offset accounting.

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_pwrite_some_cold_impl(outstmtype &outsm,
							  basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							  ::std::size_t n, ::fast_io::intfpos_t off)
{
	using char_type = typename outstmtype::output_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		// Like non-positional scatter "some", this is one native attempt over one legal prefix. Passing off here is
		// essential: positional output must neither depend on nor mutate the stream's current file position.
		::std::size_t const count{
			::fast_io::details::scatter_write_maximum_count_clamp<char_type, outstmtype>(n)};
		return scatter_pwrite_some_overflow_define(outsm, pscatters, count, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		// The offset for descriptor i equals the initial offset plus all preceding, fully written descriptor lengths.
		// On a partial write we return immediately without advancing off; the status carries the exact partial progress
		// and lets the enclosing all-operation derive the retry offset once, avoiding double advancement.
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [base, len] = pscatters[i];
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			auto written{::fast_io::details::pwrite_some_impl(outsm, range.first, range.last, off)};
			::std::size_t sz{static_cast<::std::size_t>(written - range.first)};
			if (sz != len)
			{
				return {i, sz};
			}
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype> ||
					   ::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		// Do not dispatch typed descriptors to scatter_pwrite_all_bytes_cold_impl: for wide character types that would
		// reinterpret character counts as byte counts. The typed helper preserves both descriptor and offset units.
		::fast_io::details::scatter_pwrite_all_cold_impl(outsm, pscatters, n, off);
		return {n, 0};
	}
	else
#if 0
		if constexpr ((::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
#endif
	/*
	 * The implementation of synthesizing pwrite through write+seek is missing
	 */
	{
		if constexpr (sizeof(char_type) == 1)
		{
			using scattermayalias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= io_scatter_t const *;
			return ::fast_io::details::scatter_pwrite_some_bytes_cold_impl(
				outsm, reinterpret_cast<scattermayalias_const_ptr>(pscatters), n, off);
		}
		else
		{
			// Crossing from typed positional scatters to byte primitives changes the coordinate unit as well as the
			// pointer type. Convert the initial character offset once; every later increment below is already in bytes.
			off = ::fast_io::details::scatter_fpos_mul<char_type>(off);
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basef, len] = pscatters[i];
				auto const range{::fast_io::details::scatter_to_scalar_range(basef, len)};
				::std::byte const *base{reinterpret_cast<::std::byte const *>(range.first)};
				::std::byte const *ed{reinterpret_cast<::std::byte const *>(range.last)};
				auto written{::fast_io::details::pwrite_some_bytes_impl(outsm, base, ed, off)};
				::std::size_t diff{static_cast<::std::size_t>(written - base)};
				off = ::fast_io::fposoffadd_nonegative(off, diff);
				::std::size_t md{diff % sizeof(char_type)};
				::std::size_t sz{diff / sizeof(char_type)};
				if (md)
				{
					::std::size_t dfd{sizeof(char_type) - md};
					::fast_io::details::pwrite_all_bytes_impl(outsm, written, written + dfd, off);
					off = ::fast_io::fposoffadd_nonegative(off, dfd);
					++sz;
				}
				if (sz != len)
				{
					return {i, sz};
				}
			}
			return {n, 0};
		}
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t
scatter_pwrite_some_impl(outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
						 ::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// Empty positional output neither touches the stream nor interprets the supplied offset.
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// Locking changes only the observer used for dispatch; the caller's positional coordinate is forwarded unchanged.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_some_impl(unlocked, pscatters, n, off);
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
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay(outsm);
		}
		return ::fast_io::details::scatter_pwrite_some_cold_impl(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_pwrite_all_cold_impl(outstmtype &outsm,
							 basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
							 ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<outstmtype>)
	{
		using char_type = typename outstmtype::output_char_type;
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, outstmtype>()};
		// Every overflow all-call completes its consecutive batch. fposoffadd_scatters therefore advances off by
		// exactly that batch before the descriptor pointer moves, so each byte/character is written at the same logical
		// position as in one unbounded call. SIZE_MAX disables batching; a finite maximum is nonzero by concept, hence
		// both n and the unprocessed descriptor suffix decrease monotonically.
		while (maximum < n)
		{
			scatter_pwrite_all_overflow_define(outsm, pscatters, maximum, off);
			off = ::fast_io::fposoffadd_scatters(off, pscatters, {maximum, 0u});
			pscatters += maximum;
			n -= maximum;
		}
		scatter_pwrite_all_overflow_define(outsm, pscatters, n, off);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<outstmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_pwrite_some_impl(outsm, pscatters, n, off)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			off = ::fast_io::fposoffadd_scatters(off, pscatters, ret);
			::std::size_t pisc{ret.position_in_scatter};
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				::fast_io::details::pwrite_all_impl(outsm, pi.base + pisc, pi.base + pi.len, off);
				off = ::fast_io::fposoffadd_nonegative(off, pi.len - pisc);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<outstmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [base, len] = *i;
			auto const range{::fast_io::details::scatter_to_scalar_range(base, len)};
			::fast_io::details::pwrite_all_impl(outsm, range.first, range.last, off);
			off = ::fast_io::fposoffadd_nonegative(off, len);
		}
	}
	else
#if 0
if constexpr ((::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<
							outstmtype> ||
						::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<outstmtype> ||
						::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<
							outstmtype>))
#endif
	/*
	 * The implementation of synthesizing pwrite through write+seek is missing
	 */
	{
		using char_type = typename outstmtype::output_char_type;
		if constexpr (sizeof(char_type) == 1)
		{
			using scattermayalias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= io_scatter_t const *;
			::fast_io::details::scatter_pwrite_all_bytes_cold_impl(
				outsm, reinterpret_cast<scattermayalias_const_ptr>(pscatters), n, off);
		}
		else
		{
			// The byte fallback consumes byte extents, so its positional origin must use the same unit.
			off = ::fast_io::details::scatter_fpos_mul<char_type>(off);
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basef, len] = pscatters[i];
				auto const range{::fast_io::details::scatter_to_scalar_range(basef, len)};
				::std::byte const *base{reinterpret_cast<::std::byte const *>(range.first)};
				::std::byte const *ed{reinterpret_cast<::std::byte const *>(range.last)};
				::fast_io::details::pwrite_all_bytes_impl(outsm, base, ed, off);
				// The selected fallback is a byte protocol: both its extent and its file position are measured in bytes.
				// `len` belongs to the typed descriptor and counts characters, so advancing by `len` would overlap every
				// descriptor after the first whenever sizeof(char_type) != 1. Checked multiplication makes the unit
				// conversion explicit and preserves the saturating offset-add contract.
				::std::size_t const byte_len{
					::fast_io::details::intrinsics::mul_or_overflow_die(len, sizeof(char_type))};
				off = ::fast_io::fposoffadd_nonegative(off, byte_len);
			}
		}
	}
}

template <typename outstmtype>
inline constexpr void
scatter_pwrite_all_impl(outstmtype &outsm, basic_io_scatter_t<typename outstmtype::output_char_type> const *pscatters,
						::std::size_t n, ::fast_io::intfpos_t off)
{
	if (n == 0u)
	{
		// A zero-descriptor positional all-operation is complete without a lock, flush, or native syscall.
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outstmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<outstmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::output_stream_mutex_ref_decay(outsm)};
			// The unlocked observer is an implementation detail of synchronization, not a new positional origin.
			decltype(auto) unlocked = ::fast_io::operations::decay::output_stream_unlocked_ref_decay(outsm);
			return ::fast_io::details::scatter_pwrite_all_impl(unlocked, pscatters, n, off);
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
		if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_buffer_flush_define<outstmtype>)
		{
			::fast_io::operations::decay::output_stream_buffer_flush_decay(outsm);
		}
		return ::fast_io::details::scatter_pwrite_all_cold_impl(outsm, pscatters, n, off);
	}
}

} // namespace details

} // namespace fast_io
