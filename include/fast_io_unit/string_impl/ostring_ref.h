#pragma once

namespace fast_io
{

/// @brief Opts standard strings into retained-scatter composition when they are range lvalue elements.
/// @details fast_io's standard-string alias is a direct `{data(), size()}` view, so the characters have exactly the
///          lifetime of the source string and advancing a stable range iterator cannot overwrite an earlier element's
///          storage. The range strategy independently requires an lvalue iterator reference; this marker therefore
///          does not admit temporary strings returned by transform views.
template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// A user-defined traits or allocator type contributes an associated namespace to ADL. That namespace may replace
	// the ordinary alias/forwarding protocol with scratch-backed or stateful semantics, so structural contiguity alone
	// cannot prove retained lifetime or repeatability. The standard specialization has no user-owned associated namespace.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// String aliasing reads only this object's data pointer and size; it never consults a destination put cursor.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// fast_io's standard-string print vocabulary is exactly its direct data/size alias; it has no hidden element hook.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type strlike_buffered_print_preferred(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// The standard default-allocator string owns reusable contiguous storage and supplies amortized append/growth.
	// Custom traits or allocators remain unmarked because their associated namespaces and cost models are extensible.
	return {};
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
	requires(::std::same_as<traits_type, ::std::char_traits<char_type>> &&
			 ::std::same_as<allocator_type, ::std::allocator<char_type>>)
inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	// On implementations where fast_io exposes the standard string's real put area, raw writes cannot relocate that
	// allocation until reserve/overflow is invoked and publishing an in-area end pointer has no independent I/O effect.
	// Restricting the proof to the standard traits/allocator also excludes user-associated ADL output hooks.
	return {};
}

#if defined(_GLIBCXX_STRING_VIEW) || defined(_LIBCPP_STRING_VIEW) || defined(_STRING_VIEW_)
/// @brief Opts standard string views into retained-scatter composition when stored as range lvalue elements.
/// @details Their alias points directly at the view's `[data(), data()+size())` range. The source-side marker is kept
///          here, beside the standard string integration, instead of inferring lifetime merely from a scatter-shaped
///          alias; that separation prevents unrelated scratch-producing aliases from receiving the same permission.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// Custom traits add an ADL namespace and may replace the view's otherwise trivial print protocol.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// A string_view scatter is exactly its stored pointer/length pair and is independent of every output object.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// The view's complete print semantics are the characters in its stored pointer/length pair.
	return {};
}
#endif

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr auto
strlike_construct_define(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
						 char_type const *first, char_type const *last)
{
	return ::std::basic_string<char_type, traits_type, allocator_type>(first, last);
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr auto strlike_construct_single_character_define(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>, char_type ch)
{
	return ::std::basic_string<char_type, traits_type, allocator_type>(1, ch);
}

/// @brief Establishes an exact standard-string extent before exposing writable characters.
/// @details `reserve(n); data()` is intentionally insufficient: capacity does not create live characters, and portable
///          C++20 does not permit an adapter to write beyond `size()`. `resize(n)` first makes every character in the
///          requested range part of the string's observable value; the returned mutable `data()` pointer may then be
///          used by an independently proved exact-size formatter. Value initialization is the cost of this fully
///          standard fallback. Implementations with a real `buffer_strlike` protocol retain that stronger strategy and
///          are not forced through this CPO.
template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr char_type *strlike_precise_resize_and_get_begin(
	io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
	::std::basic_string<char_type, traits_type, allocator_type> &str, ::std::size_t n)
{
	str.resize(n);
	return str.data();
}

#if __cpp_lib_string_resize_and_overwrite >= 202110L
/// @brief Opts the exact default standard string into callback-owned logical-size publication.
/// @details The specialization intentionally names `std::basic_string<char_type>` with no traits or allocator template
///          parameters: that is exactly standard traits plus the default allocator. Custom traits and allocators carry
///          user-associated namespaces and independent allocation/cost behavior, neither of which was part of the
///          evidence for concat's direct-write policy. The marker supplies only destination lifetime semantics; concat
///          separately proves that its concrete source writer cannot throw after this CPO has allocated storage.
template <::std::integral char_type>
inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	io_strlike_type_t<char_type, ::std::basic_string<char_type>>) noexcept
{
	return {};
}

/// @brief Executes one exact overwrite operation on a default standard string.
/// @details `resize_and_overwrite` may allocate and therefore remains potentially throwing. Its callback receives a
///          contiguous writable extent, returns the logical size to publish, and must not retain the pointer. Forwarding
///          the exact operation category lets the standard implementation own its callback in the normal way; concat's
///          operation object is copyable, but this adapter does not impose that stronger restriction on the protocol.
template <::std::integral char_type, typename operation>
	requires requires(::std::basic_string<char_type> &str, ::std::size_t n, operation &&op) {
		str.resize_and_overwrite(n, static_cast<operation &&>(op));
	}
inline constexpr void strlike_exact_resize_and_overwrite(
	io_strlike_type_t<char_type, ::std::basic_string<char_type>>,
	::std::basic_string<char_type> &str, ::std::size_t n, operation &&op) noexcept(noexcept(str.resize_and_overwrite(n, static_cast<operation &&>(op))))
{
	str.resize_and_overwrite(n, static_cast<operation &&>(op));
}
#endif

#if (defined(__GLIBCXX__) && !defined(_LIBCPP_VERSION) && !defined(_GLIBCXX_USE_CXX11_ABI)) || \
	(defined(_LIBCPP_VERSION) &&                                                               \
	 !((_LIBCPP_VERSION < 20 && !defined(_LIBCPP_HAS_NO_ASAN) || _LIBCPP_HAS_ASAN) && defined(_LIBCPP_INSTRUMENTED_WITH_ASAN)))

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr char_type *
strlike_begin(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
			  ::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return str.data();
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr char_type *
strlike_curr(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
			 ::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return str.data() + str.size();
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr char_type *
strlike_end(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
			::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return str.data() + str.capacity();
}
#if __cpp_lib_string_resize_and_overwrite >= 202110L
namespace details
{

template <::std::integral char_type, typename size_type>
struct empty_string_set_ptr
{
	::std::size_t realsize{};
	inline constexpr ::std::size_t operator()(char_type const *, size_type) noexcept
	{
		return realsize;
	}
};

} // namespace details

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr ::std::size_t
strlike_sso_size(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>) noexcept
{
	return ::fast_io::details::string_hack::local_capacity<
		::std::basic_string<char_type, traits_type, allocator_type>>();
}
#endif

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_set_curr(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
				 ::std::basic_string<char_type, traits_type, allocator_type> &str, char_type *p)
{
#if (__cpp_lib_string_resize_and_overwrite >= 202110L || __cpp_constexpr_dynamic_alloc >= 201907L)
	if (__builtin_is_constant_evaluated())
	{
		auto old_ptr{str.data()};
		::std::size_t const sz{static_cast<::std::size_t>(p - str.data())};
#if __cpp_lib_string_resize_and_overwrite >= 202110L

		str.resize_and_overwrite(
			sz, ::fast_io::details::empty_string_set_ptr<
					char_type, typename ::std::basic_string<char_type, traits_type, allocator_type>::size_type>{sz});
#else
		auto curr_ptr{str.data() + str.size()};
		if (p < curr_ptr)
		{
			str.resize(sz);
		}
		else if (curr_ptr < p)
		{
			::std::size_t const oldsz{str.size()};
			::std::size_t const diff{static_cast<::std::size_t>(p - curr_ptr)};
			::fast_io::details::local_operator_new_array_ptr<char_type> buffer(diff);
			for (::std::size_t i{}; i != diff; ++i)
			{
				buffer[i] = curr_ptr[i];
			}
			str.append(diff, 0);
			auto newp{str.data() + oldsz};
			for (::std::size_t i{}; i != diff; ++i)
			{
				newp[i] = buffer[i];
			}
		}
#endif
		if (old_ptr != str.data())
		{
			::fast_io::fast_terminate();
		}
	}
	else
#endif
	{
		::fast_io::details::string_hack::set_end_ptr(str, p);
		traits_type::assign(*p, char_type());
	}
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_reserve(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
				::std::basic_string<char_type, traits_type, allocator_type> &str, ::std::size_t n)
{
	str.reserve(n);
}
#endif
template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_append(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
			   ::std::basic_string<char_type, traits_type, allocator_type> &str, char_type const *first,
			   char_type const *last)
{
	str.append(first, last);
}

template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr void
strlike_push_back(io_strlike_type_t<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>,
				  ::std::basic_string<char_type, traits_type, allocator_type> &str, char_type ch)
{
	str.push_back(ch);
}
template <::std::integral char_type, typename traits_type, typename allocator_type>
inline constexpr io_strlike_reference_wrapper<char_type, ::std::basic_string<char_type, traits_type, allocator_type>>
io_strlike_ref(io_alias_t, ::std::basic_string<char_type, traits_type, allocator_type> &str) noexcept
{
	return {__builtin_addressof(str)};
}

template <::std::integral CharT, typename Traits = ::std::char_traits<CharT>,
		  typename Allocator = ::std::allocator<CharT>>
using basic_ostring_ref_std = io_strlike_reference_wrapper<CharT, ::std::basic_string<CharT, Traits, Allocator>>;
using ostring_ref_std = basic_ostring_ref_std<char>;
using wostring_ref_std = basic_ostring_ref_std<wchar_t>;
using u8ostring_ref_std = basic_ostring_ref_std<char8_t>;
using u16ostring_ref_std = basic_ostring_ref_std<char16_t>;
using u32ostring_ref_std = basic_ostring_ref_std<char32_t>;

template <::std::integral CharT, typename Traits = ::std::char_traits<CharT>,
		  typename Allocator = ::std::allocator<CharT>>
using basic_ostring_ref [[deprecated("Please use basic_ostring_ref_std or basic_ostring_ref_fast_io instead.")]] = ::fast_io::basic_ostring_ref_std<CharT, Traits, Allocator>;
using ostring_ref [[deprecated("Please use ostring_ref_std or ostring_ref_fast_io instead.")]] = ::fast_io::ostring_ref_std;
using wostring_ref [[deprecated("Please use wostring_ref_std or wostring_ref_fast_io instead.")]] = ::fast_io::wostring_ref_std;
using u8ostring_ref [[deprecated("Please use u8ostring_ref_std or u8ostring_ref_fast_io instead.")]] = ::fast_io::u8ostring_ref_std;
using u16ostring_ref [[deprecated("Please use u16ostring_ref_std or u16ostring_ref_fast_io instead.")]] = ::fast_io::u16ostring_ref_std;
using u32ostring_ref [[deprecated("Please use u32ostring_ref_std or u32ostring_ref_fast_io instead.")]] = ::fast_io::u32ostring_ref_std;

} // namespace fast_io
