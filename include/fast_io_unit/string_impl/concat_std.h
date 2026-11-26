#pragma once

namespace fast_io
{

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	concat_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char, ::std::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char, ::std::string>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	wconcat_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, wchar_t, ::std::basic_string<wchar_t>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u8concat_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char8_t, ::std::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u16concat_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char16_t, ::std::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u32concat_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char32_t, ::std::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	concatln_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char, ::std::basic_string<char>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char, ::std::basic_string<char>>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	wconcatln_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, wchar_t, ::std::basic_string<wchar_t>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u8concatln_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char8_t, ::std::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u16concatln_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char16_t, ::std::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u32concatln_std(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char32_t, ::std::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

} // namespace fast_io
