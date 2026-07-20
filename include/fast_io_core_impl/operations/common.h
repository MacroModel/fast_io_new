#pragma once

namespace fast_io
{

template <typename T>
inline constexpr io_scatter_status_t scatter_size_to_status(::std::size_t sz, basic_io_scatter_t<T> const *base,
															::std::size_t len) noexcept
{
	// Given the backend invariant sz <= sum(base[0..len).len), a scatter "some" result is a prefix decomposition:
	// position counts descriptors consumed in full, and position_in_scatter counts elements consumed from the first
	// incomplete descriptor. Subtracting complete lengths proves that the returned pair denotes exactly sz elements,
	// without changing the descriptor-relative coordinate system expected by retrying all-operations.
	::std::size_t total{sz};
	for (::std::size_t i{}; i != len; ++i)
	{
		::std::size_t blen{base[i].len};
		if (total < blen) [[unlikely]]
		{
			return {i, total};
		}
		total -= blen;
	}
	return {len, 0};
}

namespace details
{

template <::std::integral char_type, typename stream_type>
inline constexpr ::std::size_t scatter_read_maximum_count_clamp(::std::size_t count) noexcept
{
	// The optional limit is normalized to SIZE_MAX, so one min operation covers both bounded and unbounded streams.
	// A recognized policy is nonzero by concept; therefore every nonempty request still admits a nonempty descriptor
	// prefix, including a prefix whose descriptors all happen to have zero payload length.
	constexpr ::std::size_t maximum{
		::fast_io::details::scatter_read_maximum_count_or_unlimited<char_type, stream_type>()};
	return count < maximum ? count : maximum;
}

template <::std::integral char_type, typename stream_type>
inline constexpr ::std::size_t scatter_write_maximum_count_clamp(::std::size_t count) noexcept
{
	// Absence of a stream limit is represented by SIZE_MAX. Therefore min(count, maximum) is valid even for the
	// sentinel itself and needs no separate unlimited branch. A declared limit is concept-checked as nonzero, which
	// also supplies the progress invariant used by the batching loops in the scatter all-operations.
	constexpr ::std::size_t maximum{
		::fast_io::details::scatter_write_maximum_count_or_unlimited<char_type, stream_type>()};
	return count < maximum ? count : maximum;
}

} // namespace details

namespace details
{
template <typename T>
struct basic_scatter_total_size_overflow_result
{
	T total_size{};
	::std::size_t position{};
};
} // namespace details
using scatter_total_size_overflow_result = ::fast_io::details::basic_scatter_total_size_overflow_result<::std::size_t>;

namespace details
{

template <::std::unsigned_integral U, typename T>
inline constexpr ::fast_io::details::basic_scatter_total_size_overflow_result<U>
find_scatter_total_size_overflow_impl(basic_io_scatter_t<T> const *base, ::std::size_t len) noexcept
{
	// Descriptor count is an address-space quantity and must remain size_t even when the accumulated payload/offset is
	// intentionally narrower. Coupling len to U would truncate the iteration bound before any overflow check and could
	// falsely certify an unvisited suffix as irrelevant to positional advancement.
	constexpr U mx{::std::numeric_limits<U>::max()};
	U total{};
	auto i{base}, e{base + len};
	for (; i != e; ++i)
	{
		// Descriptor lengths are size_t, while positional accumulation may deliberately use a narrower unsigned type.
		// Prove representability before conversion; otherwise truncating the descriptor would make an oversized element
		// look small. Once converted, total <= mx is the loop invariant, so comparing length with mx - total cannot
		// underflow and is equivalent to testing whether total + length is representable.
		if constexpr (::std::numeric_limits<U>::digits < ::std::numeric_limits<::std::size_t>::digits)
		{
			if (i->len > static_cast<::std::size_t>(mx)) [[unlikely]]
			{
				break;
			}
		}
		U const element_length{static_cast<U>(i->len)};
		if (mx - total < element_length) [[unlikely]]
		{
			break;
		}
		total += element_length;
	}
	return {total, static_cast<::std::size_t>(i - base)};
}

} // namespace details

template <typename T>
inline constexpr scatter_total_size_overflow_result find_scatter_total_size_overflow(basic_io_scatter_t<T> const *base,
																					 ::std::size_t len) noexcept
{
	if (__builtin_is_constant_evaluated())
	{
		return ::fast_io::details::find_scatter_total_size_overflow_impl<::std::size_t>(base, len);
	}
	else
	{
		using io_scatter_alias_ptr

#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= io_scatter_t const *;
		return ::fast_io::details::find_scatter_total_size_overflow_impl<::std::size_t>(
			reinterpret_cast<io_scatter_alias_ptr>(base), len);
	}
}

namespace details
{
inline constexpr ::std::size_t scatter_status_one_size_impl(::std::size_t position, ::std::size_t position_in_scatter,
															::std::size_t n) noexcept
{
	if (position)
	{
		return n;
	}
	return position_in_scatter;
}
} // namespace details

inline constexpr ::std::size_t scatter_status_one_size(io_scatter_status_t status, ::std::size_t n) noexcept
{
	return ::fast_io::details::scatter_status_one_size_impl(status.position, status.position_in_scatter, n);
}

template <::std::integral dftype>
inline constexpr ::fast_io::intfpos_t fposoffadd_nonegative(::fast_io::intfpos_t off, dftype df) noexcept
{
	FAST_IO_ASSUME(0 <= df);

	constexpr ::fast_io::intfpos_t mxv{::std::numeric_limits<::fast_io::intfpos_t>::max()};
	constexpr ::fast_io::uintfpos_t umxv{static_cast<::fast_io::uintfpos_t>(mxv)};
	if constexpr (mxv < ::std::numeric_limits<dftype>::max())
	{
		if (mxv < df)
		{
			return mxv;
		}
	}
	::fast_io::intfpos_t mx{static_cast<::fast_io::intfpos_t>(umxv - static_cast<::fast_io::uintfpos_t>(df))};
	if (mx < off)
	{
		return mxv;
	}
	else
	{
		return off + static_cast<::fast_io::intfpos_t>(df);
	}
}

template <::std::integral dftype>
inline constexpr ::fast_io::intfpos_t fposoffadd(::fast_io::intfpos_t off, dftype df) noexcept
{
	constexpr ::fast_io::intfpos_t mxv{::std::numeric_limits<::fast_io::intfpos_t>::max()};
	constexpr ::fast_io::uintfpos_t umxv{static_cast<::fast_io::uintfpos_t>(mxv)};
	if constexpr (mxv < ::std::numeric_limits<dftype>::max())
	{
		if (mxv < df)
		{
			return mxv;
		}
	}
	if constexpr (::std::signed_integral<dftype>)
	{
		if (df < 0)
		{
			constexpr ::fast_io::intfpos_t mnv{::std::numeric_limits<::fast_io::intfpos_t>::min()};
			if constexpr (::std::numeric_limits<dftype>::min() < mxv)
			{
				if (df < mxv)
				{
					return mnv;
				}
			}
			auto ddf{mnv - df};
			if (off < ddf)
			{
				return mnv;
			}
			else
			{
				return off + df;
			}
		}
	}
	::fast_io::intfpos_t mx{static_cast<::fast_io::intfpos_t>(umxv - static_cast<::fast_io::uintfpos_t>(df))};
	if (mx < off)
	{
		return mxv;
	}
	else
	{
		return off + df;
	}
}

namespace details
{

template <typename T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr ::fast_io::intfpos_t
fposoffadd_scatters_impl(::fast_io::intfpos_t off, basic_io_scatter_t<T> const *base, ::std::size_t position,
						 ::std::size_t position_in_scatter) noexcept
{
	auto res{::fast_io::details::find_scatter_total_size_overflow_impl<::fast_io::uintfpos_t>(base, position)};
	constexpr ::fast_io::intfpos_t mxv{::std::numeric_limits<::fast_io::intfpos_t>::max()};
	if (res.position != position)
	{
		return mxv;
	}
	return fposoffadd_nonegative(fposoffadd_nonegative(off, res.total_size), position_in_scatter);
}
} // namespace details

template <typename T>
inline constexpr ::fast_io::intfpos_t fposoffadd_scatters(::fast_io::intfpos_t off, basic_io_scatter_t<T> const *base,
														  io_scatter_status_t status) noexcept
{
	if (__builtin_is_constant_evaluated())
	{
		return ::fast_io::details::fposoffadd_scatters_impl(off, base, status.position, status.position_in_scatter);
	}
	else
	{
		using io_scatter_alias_ptr

#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= io_scatter_t const *;
		return ::fast_io::details::fposoffadd_scatters_impl(off, reinterpret_cast<io_scatter_alias_ptr>(base),
															status.position, status.position_in_scatter);
	}
}

namespace details
{

template <::std::integral char_type>
inline constexpr ::fast_io::intfpos_t scatter_fpos_mul(::fast_io::intfpos_t ofd) noexcept
{
	constexpr ::fast_io::intfpos_t mx{::std::numeric_limits<::fast_io::intfpos_t>::max()};
	constexpr ::fast_io::intfpos_t ofs{mx / sizeof(char_type)};
	if (ofd > ofs)
	{
		return mx;
	}
	else
	{
		return ofd * static_cast<intfpos_t>(sizeof(char_type));
	}
}

inline constexpr ::fast_io::intfpos_t adjust_instm_offset(::std::ptrdiff_t remainspace,
														  ::fast_io::intfpos_t requested) noexcept
{
	FAST_IO_ASSUME(remainspace >= 0);
	constexpr auto ptrdfmn{::std::numeric_limits<::fast_io::intfpos_t>::min()};
	if (requested < ptrdfmn + remainspace)
	{
		return ptrdfmn;
	}
	return requested - remainspace;
}

} // namespace details

} // namespace fast_io
