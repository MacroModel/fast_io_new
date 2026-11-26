#pragma once

namespace fast_io
{

template <typename... Args>
[[deprecated("use ::fast_io::concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	concat(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char, ::std::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char, ::std::string>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
template <typename... Args>
[[deprecated("use ::fast_io::wconcat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	wconcat(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, wchar_t, ::std::basic_string<wchar_t>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[deprecated("use ::fast_io::u8concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u8concat(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char8_t, ::std::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u16concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u16concat(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char16_t, ::std::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u32concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u32concat(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char32_t, ::std::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	concatln(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char, ::std::basic_string<char>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char, ::std::basic_string<char>>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS

template <typename... Args>
[[deprecated("use ::fast_io::wconcatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	wconcatln(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, wchar_t, ::std::basic_string<wchar_t>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[deprecated("use ::fast_io::u8concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u8concatln(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char8_t, ::std::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u16concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u16concatln(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char16_t, ::std::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u32concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	auto
	u32concatln(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char32_t, ::std::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

} // namespace fast_io
