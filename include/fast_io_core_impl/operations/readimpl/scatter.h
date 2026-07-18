#pragma once

namespace fast_io
{


namespace operations::decay
{
template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last);

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_decay(instmtype &&insm, ::std::byte *first, ::std::byte *last,
														  ::fast_io::intfpos_t);

} // namespace operations::decay

namespace details
{

// Scatter decomposition is an internal control-flow choice. All descriptor segments must therefore use one borrowed
// observer: N descriptors may require N scalar calls, but they must not create N proxy owners or change observer
// identity between calls. Native scatter CPOs still receive the ordinary named lvalue required by their protocol.
template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t scatter_read_some_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters,
																	   ::std::size_t n);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void scatter_read_all_bytes_cold_impl(instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_read_all_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
						   ::std::size_t n);

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr io_scatter_status_t
scatter_read_some_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
							::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		// "Some" performs one native attempt. Admit only the stream's legal descriptor prefix and leave the returned
		// status in the caller's descriptor coordinate system; a completed bounded prefix is therefore continuation,
		// not an implicit request to consume the suffix in this operation.
		::std::size_t const count{
			::fast_io::details::scatter_read_maximum_count_clamp<char_type, instmtype>(n)};
		return scatter_read_some_underflow_define(insm, pscatters, count);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		for (::std::size_t i{}; i != n; ++i)
		{
			auto [basec, len] = pscatters[i];
			char_type *base{const_cast<char_type *>(basec)};
			auto ed{base + len};
			auto written{::fast_io::details::read_some_impl(insm, base, ed)};
			::std::size_t sz{static_cast<::std::size_t>(written - base)};
			if (sz != len)
			{
				return {i, sz};
			}
		}
		return {n, 0};
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype> ||
					   ::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		scatter_read_all_cold_impl(insm, pscatters, n);
		return {n, 0};
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		if constexpr (sizeof(char_type) == 1)
		{
			using scattermayalias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= io_scatter_t const *;
			return ::fast_io::details::scatter_read_some_bytes_cold_impl(
				insm, reinterpret_cast<scattermayalias_ptr>(pscatters), n);
		}
		else
		{
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basefd, len] = pscatters[i];
				char_type *basef{const_cast<char_type *>(basefd)};
				auto edf{basef + len};
				::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basef))};
				::std::byte *ed{reinterpret_cast<::std::byte *>(edf)};
				auto readed{::fast_io::details::read_some_bytes_impl(insm, base, ed)};
				::std::size_t diff{static_cast<::std::size_t>(readed - base)};
				::std::size_t md{diff % sizeof(char_type)};
				::std::size_t sz{diff / sizeof(char_type)};
				if (md)
				{
					::std::size_t dfd{sizeof(char_type) - md};
					::fast_io::details::read_all_bytes_impl(insm, readed, readed + dfd);
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
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		auto ret{scatter_pread_some_cold_impl(insm, pscatters, n, 0)};
		::fast_io::operations::decay::input_stream_seek_decay(insm, fposoffadd_scatters(0, pscatters, ret),
															  ::fast_io::seekdir::cur);
		return ret;
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		auto ret{scatter_pread_some_cold_impl(insm, pscatters, n, 0)};
		::fast_io::operations::decay::input_stream_seek_bytes_decay(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, ret)),
			::fast_io::seekdir::cur);
		return ret;
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t
scatter_read_some_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
					   ::std::size_t n)
{
	if (n == 0u)
	{
		return {};
	}
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			// Character preservation proves that descriptors formed for the locked stream retain their element type
			// after unwrapping. The guard surrounds recursive dispatch, so descriptor fallback cannot relock per segment.
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::scatter_read_some_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		auto curr{ibuffer_curr(insm)};
		auto ed{ibuffer_end(insm)};

		::std::size_t buffptrdiff{static_cast<::std::size_t>(ed - curr)};

		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [basec, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				char_type *base{const_cast<char_type *>(basec)};
				::fast_io::details::non_overlapped_copy_n(curr, len, base);
				curr += len;
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		ibuffer_set_curr(insm, curr);
		if (i != e)
#if __has_cpp_attribute(unlikely)
			[[unlikely]]
#endif
		{
			auto ret{::fast_io::details::scatter_read_some_cold_impl(insm, i, static_cast<::std::size_t>(e - i))};
			ret.position += static_cast<::std::size_t>(i - pscatters);
			return ret;
		}
		return {n, 0};
	}
	else
	{
		return scatter_read_some_cold_impl(insm, pscatters, n);
	}
}

template <typename instmtype>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void
scatter_read_all_cold_impl(instmtype &insm, basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
						   ::std::size_t n)
{
	using char_type = typename instmtype::input_char_type;
	if constexpr (::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<instmtype>)
	{
		constexpr ::std::size_t maximum{
			::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, instmtype>()};
		// Every all-CPO either completes its admitted prefix or reports EOF by its normal failure mechanism. Splitting
		// only after a completed descriptor batch preserves order and observable destination contents. SIZE_MAX makes
		// the loop inactive for an unbounded stream; a finite policy is nonzero, so n decreases monotonically.
		while (maximum < n)
		{
			scatter_read_all_underflow_define(insm, pscatters, maximum);
			pscatters += maximum;
			n -= maximum;
		}
		scatter_read_all_underflow_define(insm, pscatters, n);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_all_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			char_type *base{const_cast<char_type *>(basec)};
			::fast_io::details::read_all_impl(insm, base, base + len);
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<instmtype>)
	{
		for (;;)
		{
			auto ret{::fast_io::details::scatter_read_some_impl(insm, pscatters, n)};
			::std::size_t retpos{ret.position};
			if (retpos == n)
			{
				return;
			}
			::std::size_t pisc{ret.position_in_scatter};
			if (retpos == 0u && pisc == 0u)
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
			if (pisc)
			{
				auto pi = pscatters[ret.position];
				char_type *base{const_cast<char_type *>(pi.base)};
				::fast_io::details::read_all_impl(insm, base + pisc, base + pi.len);
				++retpos;
			}
			pscatters += retpos;
			n -= retpos;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_read_some_underflow_define<instmtype>)
	{
		for (auto i{pscatters}, e{pscatters + n}; i != e; ++i)
		{
			auto [basec, len] = *i;
			char_type *base{const_cast<char_type *>(basec)};
			::fast_io::details::read_all_impl(insm, base, base + len);
		}
	}
	else if constexpr ((::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<instmtype>))
	{
		if constexpr (sizeof(char_type) == 1)
		{
			using scattermayalias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
				[[__gnu__::__may_alias__]]
#endif
				= io_scatter_t *;
			::fast_io::details::scatter_read_all_bytes_cold_impl(insm, reinterpret_cast<scattermayalias_ptr>(pscatters),
																 n);
		}
		else
		{
			for (::std::size_t i{}; i != n; ++i)
			{
				auto [basef, len] = pscatters[i];
				auto edf{basef + len};
				::std::byte *base{reinterpret_cast<::std::byte *>(const_cast<void *>(basef))};
				::std::byte *ed{reinterpret_cast<::std::byte *>(edf)};
				::fast_io::details::read_all_bytes_impl(insm, base, ed);
			}
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_operations<instmtype>))
	{
		scatter_pread_all_cold_impl(insm, pscatters, n, 0);
		::fast_io::operations::decay::input_stream_seek_decay(
			insm, ::fast_io::fposoffadd_scatters(0, pscatters, {n, 0}), ::fast_io::seekdir::cur);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_seek_bytes_define<instmtype> &&
					   (::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<instmtype>))
	{
		scatter_pread_all_cold_impl(insm, pscatters, n, 0);
		::fast_io::operations::decay::input_stream_seek_bytes_decay(
			insm, ::fast_io::details::scatter_fpos_mul<char_type>(::fast_io::fposoffadd_scatters(0, pscatters, {n, 0})),
			::fast_io::seekdir::cur);
	}
}

template <typename instmtype>
inline constexpr void scatter_read_all_impl(instmtype &insm,
											basic_io_scatter_t<typename instmtype::input_char_type> const *pscatters,
											::std::size_t n)
{
	if (n == 0u)
	{
		return;
	}
	if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<instmtype>)
	{
		if constexpr (::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>)
		{
			::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
				::fast_io::operations::decay::input_stream_mutex_ref_decay(insm)};
			decltype(auto) unlocked = ::fast_io::operations::decay::input_stream_unlocked_ref_decay(insm);
			return ::fast_io::details::scatter_read_all_impl(unlocked, pscatters, n);
		}
		else
		{
			static_assert(
				::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol<instmtype>,
				"an input mutex marker requires a complete, character-preserving, type-progressing unlocked protocol");
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<instmtype>)
	{
		using char_type = typename instmtype::input_char_type;
		auto curr{ibuffer_curr(insm)};
		auto ed{ibuffer_end(insm)};

		::std::size_t buffptrdiff{static_cast<::std::size_t>(ed - curr)};

		auto i{pscatters}, e{pscatters + n};
		for (; i != e; ++i)
		{
			auto [basec, len] = *i;
			if (len <= buffptrdiff)
#if __has_cpp_attribute(likely)
				[[likely]]
#endif
			{
				char_type *base{const_cast<char_type *>(basec)};
				::fast_io::details::non_overlapped_copy_n(curr, len, base);
				curr += len;
				buffptrdiff -= len;
			}
			else
			{
				break;
			}
		}
		ibuffer_set_curr(insm, curr);
		if (i != e)
#if __has_cpp_attribute(unlikely)
			[[unlikely]]
#endif
		{
			return ::fast_io::details::scatter_read_all_cold_impl(insm, i, static_cast<::std::size_t>(e - i));
		}
	}
	else
	{
		return ::fast_io::details::scatter_read_all_cold_impl(insm, pscatters, n);
	}
}

} // namespace details

} // namespace fast_io
