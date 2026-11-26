#pragma once

namespace fast_io
{
namespace details
{
inline ::fast_io::intfpos_t posix_seek_impl(int fd, ::fast_io::intfpos_t offset, seekdir s)
{
#if defined(__MSDOS__)
	if constexpr (sizeof(off_t) < sizeof(::fast_io::intfpos_t))
	{
		if (offset < static_cast<::fast_io::intfpos_t>(::std::numeric_limits<off_t>::min()) ||
			offset > static_cast<::fast_io::intfpos_t>(::std::numeric_limits<off_t>::max()))
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	auto ret{noexcept_call(::lseek, fd, static_cast<off_t>(offset), static_cast<int>(s))};
	if (ret == -1)
	{
		throw_posix_error();
	}
	return static_cast<::fast_io::intfpos_t>(static_cast<my_make_unsigned_t<off_t>>(ret));
#elif defined(__linux__) && (defined(__NR_llseek) || defined(__NR__llseek))
	// The split-offset Linux ABI returns the full position through an output pointer.
	// Its availability depends on the kernel ABI, including when libc uses a 64-bit off_t.
#if defined(__NR_llseek)
	constexpr auto number{__NR_llseek};
#else
	constexpr auto number{__NR__llseek};
#endif
	::std::uint_least64_t result{};
	auto const bits{static_cast<::std::uint_least64_t>(offset)};
	auto const ret{system_call<number, int>(fd, static_cast<::std::uint_least32_t>(bits >> 32u),
		static_cast<::std::uint_least32_t>(bits), __builtin_addressof(result), static_cast<int>(s))};
	system_call_throw_error(ret);
	return static_cast<::fast_io::intfpos_t>(result);
#elif defined(__linux__) && defined(__NR_lseek) && defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ >= 8
	auto const ret{system_call<__NR_lseek, long>(fd, static_cast<long>(offset), static_cast<int>(s))};
	system_call_throw_error(ret);
	return static_cast<::fast_io::intfpos_t>(ret);
#elif (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__)) && !defined(__CYGWIN__)
	if constexpr (sizeof(::std::int_least64_t) < sizeof(::fast_io::intfpos_t))
	{
		if (offset < static_cast<::fast_io::intfpos_t>(::std::numeric_limits<::std::int_least64_t>::min()) ||
			offset > static_cast<::fast_io::intfpos_t>(::std::numeric_limits<::std::int_least64_t>::max()))
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	auto ret(::fast_io::noexcept_call(::_lseeki64, fd, static_cast<::std::int_least64_t>(offset), static_cast<int>(s)));
	if (ret == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	return static_cast<::fast_io::intfpos_t>(static_cast<::std::uint_least64_t>(ret));

#else
	if constexpr (sizeof(off_t) < sizeof(::fast_io::intfpos_t))
	{
		if (offset < static_cast<::fast_io::intfpos_t>(::std::numeric_limits<off_t>::min()) ||
			offset > static_cast<::fast_io::intfpos_t>(::std::numeric_limits<off_t>::max()))
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	auto ret(::fast_io::noexcept_call(::lseek, fd, static_cast<off_t>(offset), static_cast<int>(s)));
	if (ret == -1) [[unlikely]]
	{
		throw_posix_error();
	}
	return static_cast<::fast_io::intfpos_t>(static_cast<::std::uint_least64_t>(ret));
#endif
}
} // namespace details

template <::fast_io::posix_family family, ::std::integral char_type>
inline ::fast_io::intfpos_t io_stream_seek_bytes_define(::fast_io::basic_posix_family_io_observer<family, char_type> bpiob,
														::fast_io::intfpos_t off, ::fast_io::seekdir sdir)
{
	return ::fast_io::details::posix_seek_impl(bpiob.fd, off, sdir);
}

} // namespace fast_io
