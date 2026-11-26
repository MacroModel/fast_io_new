#pragma once

#if !defined(__cplusplus)
#error "You must be using a C++ compiler"
#endif

#include <version>
#include <type_traits>
#include <concepts>
#include <limits>
#include <cstdint>
#include <cstddef>
#include <new>
#include <initializer_list>
#include <bit>
#include <compare>
// The core supplies the classic search, comparison and copy algorithms used by the string implementation.
#include "../fast_io_core.h"
#include "impl/misc/push_warnings.h"
#include "impl/misc/push_macros.h"
#include "impl/freestanding.h"
#include "impl/common.h"
#include "impl/string_view.h"
#include "impl/cstring_view.h"
#include "impl/string.h"

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))

namespace fast_io
{

template <::std::integral chartype, typename allocator = ::fast_io::native_global_allocator>
using basic_string = ::fast_io::containers::basic_string<chartype, allocator>;
using string = ::fast_io::containers::basic_string<char, ::fast_io::native_global_allocator>;
using wstring = ::fast_io::containers::basic_string<wchar_t, ::fast_io::native_global_allocator>;
using u8string = ::fast_io::containers::basic_string<char8_t, ::fast_io::native_global_allocator>;
using u16string = ::fast_io::containers::basic_string<char16_t, ::fast_io::native_global_allocator>;
using u32string = ::fast_io::containers::basic_string<char32_t, ::fast_io::native_global_allocator>;

template <::std::integral chartype, typename allocator = ::fast_io::native_global_allocator>
using basic_ostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<chartype, allocator>;
using ostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<char, ::fast_io::native_global_allocator>;
using wostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<wchar_t, ::fast_io::native_global_allocator>;
using u8ostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<char8_t, ::fast_io::native_global_allocator>;
using u16ostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<char16_t, ::fast_io::native_global_allocator>;
using u32ostring_ref_fast_io = ::fast_io::containers::basic_ostring_ref_fast_io<char32_t, ::fast_io::native_global_allocator>;

namespace tlc
{

template <::std::integral chartype, typename allocator = ::fast_io::native_thread_local_allocator>
using basic_string = ::fast_io::containers::basic_string<chartype, allocator>;
using string = ::fast_io::containers::basic_string<char, ::fast_io::native_thread_local_allocator>;
using wstring = ::fast_io::containers::basic_string<wchar_t, ::fast_io::native_thread_local_allocator>;
using u8string = ::fast_io::containers::basic_string<char8_t, ::fast_io::native_thread_local_allocator>;
using u16string = ::fast_io::containers::basic_string<char16_t, ::fast_io::native_thread_local_allocator>;
using u32string = ::fast_io::containers::basic_string<char32_t, ::fast_io::native_thread_local_allocator>;

template <::std::integral chartype, typename allocator = ::fast_io::native_thread_local_allocator>
using basic_ostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<chartype, allocator>;
using ostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<char, ::fast_io::native_thread_local_allocator>;
using wostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<wchar_t, ::fast_io::native_thread_local_allocator>;
using u8ostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<char8_t, ::fast_io::native_thread_local_allocator>;
using u16ostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<char16_t, ::fast_io::native_thread_local_allocator>;
using u32ostring_ref_fast_io_tlc = ::fast_io::containers::basic_ostring_ref_fast_io<char32_t, ::fast_io::native_thread_local_allocator>;

} // namespace tlc

// Public concat wrappers must preserve the caller's original value categories until the checked concat front door.
// That front door performs aliasing and character-dependent forwarding exactly once, then proves the destination it
// may actually select. Pre-normalizing named `args` here both erased rvalue categories and made `basic_general_concat`
// repeat status forwarding on the resulting object; probing a dummy stream also rejected valid destination-specific
// print definitions. Direct perfect forwarding therefore aligns diagnostics, strategy admission, and CPO execution.
template <::std::integral char_type, typename... Args>
constexpr inline auto
basic_concat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char_type, ::fast_io::basic_string<char_type>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<
		false, char_type, ::fast_io::basic_string<char_type>>(::std::forward<Args>(args)...);
}

template <typename... Args>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
constexpr inline auto concat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char, ::fast_io::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char, ::fast_io::string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto wconcat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, wchar_t, ::fast_io::wstring, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, wchar_t, ::fast_io::wstring>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u8concat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char8_t, ::fast_io::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char8_t, ::fast_io::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u16concat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char16_t, ::fast_io::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char16_t, ::fast_io::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u32concat_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char32_t, ::fast_io::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char32_t, ::fast_io::u32string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto concatln_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char, ::fast_io::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char, ::fast_io::string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto wconcatln_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, wchar_t, ::fast_io::wstring, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, wchar_t, ::fast_io::wstring>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u8concatln_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char8_t, ::fast_io::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char8_t, ::fast_io::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u16concatln_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char16_t, ::fast_io::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char16_t, ::fast_io::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u32concatln_fast_io(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char32_t, ::fast_io::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char32_t, ::fast_io::u32string>(
		::std::forward<Args>(args)...);
}

namespace tlc
{

template <::std::integral char_type, typename... Args>
constexpr inline auto
basic_concat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char_type, ::fast_io::tlc::basic_string<char_type>, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<
		false, char_type, ::fast_io::tlc::basic_string<char_type>>(::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto concat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char, ::fast_io::tlc::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char, ::fast_io::tlc::string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto wconcat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, wchar_t, ::fast_io::tlc::wstring, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, wchar_t, ::fast_io::tlc::wstring>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u8concat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char8_t, ::fast_io::tlc::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char8_t, ::fast_io::tlc::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u16concat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char16_t, ::fast_io::tlc::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char16_t, ::fast_io::tlc::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u32concat_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<false, char32_t, ::fast_io::tlc::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char32_t, ::fast_io::tlc::u32string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto concatln_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char, ::fast_io::tlc::string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char, ::fast_io::tlc::string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto wconcatln_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, wchar_t, ::fast_io::tlc::wstring, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, wchar_t, ::fast_io::tlc::wstring>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u8concatln_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char8_t, ::fast_io::tlc::u8string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char8_t, ::fast_io::tlc::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u16concatln_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char16_t, ::fast_io::tlc::u16string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char16_t, ::fast_io::tlc::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
constexpr inline auto u32concatln_fast_io_tlc(Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::concat_range_has_hc<true, char32_t, ::fast_io::tlc::u32string, Args...>)
#endif
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char32_t, ::fast_io::tlc::u32string>(
		::std::forward<Args>(args)...);
}

} // namespace tlc

} // namespace fast_io

#endif

#include "impl/misc/pop_macros.h"
#include "impl/misc/pop_warnings.h"
