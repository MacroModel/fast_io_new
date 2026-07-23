#pragma once

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#include "defined_types.h"
#endif

namespace fast_io
{

namespace details
{

template <typename T>
inline constexpr bool raw_character_scalar_print_arg{
	::fast_io::details::character_integral<::std::remove_cvref_t<T>>};

template <typename T>
using raw_character_pointer_pointee_t =
	::std::remove_cv_t<::std::remove_pointer_t<::std::remove_cvref_t<T>>>;

template <typename T>
inline constexpr bool raw_character_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> &&
	::fast_io::details::character_integral<raw_character_pointer_pointee_t<T>>};

template <typename T>
inline constexpr bool raw_function_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> &&
	::std::is_function_v<raw_character_pointer_pointee_t<T>>};

template <typename T>
inline constexpr bool raw_non_character_pointer_print_arg{
	::std::is_pointer_v<::std::remove_cvref_t<T>> && (!raw_character_pointer_print_arg<T>) &&
	(!raw_function_pointer_print_arg<T>)};

template <typename T>
inline constexpr bool raw_member_object_pointer_print_arg{
	::std::is_member_object_pointer_v<::std::remove_cvref_t<T>>};

template <typename T>
inline constexpr bool raw_member_function_pointer_print_arg{
	::std::is_member_function_pointer_v<::std::remove_cvref_t<T>>};

template <typename... Args>
inline constexpr bool has_raw_character_scalar_print_arg{(... || raw_character_scalar_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_character_pointer_print_arg{(... || raw_character_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_function_pointer_print_arg{(... || raw_function_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_non_character_pointer_print_arg{(... || raw_non_character_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_member_object_pointer_print_arg{(... || raw_member_object_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_member_function_pointer_print_arg{(... || raw_member_function_pointer_print_arg<Args>)};

template <typename... Args>
inline constexpr bool has_raw_print_arg{has_raw_character_scalar_print_arg<Args...> ||
										has_raw_character_pointer_print_arg<Args...> ||
										has_raw_function_pointer_print_arg<Args...> ||
										has_raw_non_character_pointer_print_arg<Args...> ||
										has_raw_member_object_pointer_print_arg<Args...> ||
										has_raw_member_function_pointer_print_arg<Args...>};

template <bool has_raw_character_scalar>
inline constexpr void print_raw_character_scalar_static_assert() noexcept
{
	static_assert(!has_raw_character_scalar,
				  "fast_io: raw character scalar is ambiguous. Use mnp::chvw(ch) for character text or "
				  "mnp::dec(ch) for its code value.");
}

template <bool has_raw_character_pointer>
inline constexpr void print_raw_character_pointer_static_assert() noexcept
{
	static_assert(!has_raw_character_pointer,
				  "fast_io: raw character pointer is ambiguous. Use mnp::pointervw(ptr) for pointer value or "
				  "mnp::os_c_str(ptr) for OS/C string text.");
}

template <bool has_raw_non_character_pointer>
inline constexpr void print_raw_non_character_pointer_static_assert() noexcept
{
	static_assert(!has_raw_non_character_pointer,
				  "fast_io: raw pointer is not printable directly. Use mnp::pointervw(ptr) for pointer value.");
}

template <bool has_raw_function_pointer>
inline constexpr void print_raw_function_pointer_static_assert() noexcept
{
	static_assert(!has_raw_function_pointer,
				  "fast_io: raw function pointer is not printable directly. Use mnp::funcvw(fn) for function address.");
}

template <bool has_raw_member_object_pointer>
inline constexpr void print_raw_member_object_pointer_static_assert() noexcept
{
	static_assert(!has_raw_member_object_pointer,
				  "fast_io: raw member object pointer is not printable directly. Use mnp::fieldptrvw(ptr) for its "
				  "member-pointer representation.");
}

template <bool has_raw_member_function_pointer>
inline constexpr void print_raw_member_function_pointer_static_assert() noexcept
{
	static_assert(!has_raw_member_function_pointer,
				  "fast_io: raw member function pointer is not printable directly. Use mnp::methodvw(ptr) for its "
				  "member-pointer representation.");
}

template <typename... Args>
inline constexpr void print_raw_static_assert() noexcept
{
	if constexpr (has_raw_character_scalar_print_arg<Args...>)
	{
		print_raw_character_scalar_static_assert<has_raw_character_scalar_print_arg<Args...>>();
	}
	else if constexpr (has_raw_character_pointer_print_arg<Args...>)
	{
		print_raw_character_pointer_static_assert<has_raw_character_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_function_pointer_print_arg<Args...>)
	{
		print_raw_function_pointer_static_assert<has_raw_function_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_non_character_pointer_print_arg<Args...>)
	{
		print_raw_non_character_pointer_static_assert<has_raw_non_character_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_member_object_pointer_print_arg<Args...>)
	{
		print_raw_member_object_pointer_static_assert<has_raw_member_object_pointer_print_arg<Args...>>();
	}
	else if constexpr (has_raw_member_function_pointer_print_arg<Args...>)
	{
		print_raw_member_function_pointer_static_assert<has_raw_member_function_pointer_print_arg<Args...>>();
	}
}

} // namespace details

inline namespace io
{

template <typename T, typename... Args>
// GCC 11--16 leave this facade outlined at -O3. That boundary hides source-expression constants from the core
// __builtin_constant_p gate and adds a public-wrapper call to the unknown-value path. Removing only this marker makes
// every tested constant int/double obuffer wrapper call the runtime formatter; on GCC 11 and 12 it also slows a runtime
// int/double obuffer loop by 10--12%. This is a measured compile-time tradeoff: at sixteen call sites forcing costs
// roughly 1.5--2.8x compilation across GCC 11--16, but runtime text changes by only about -1.5% to +3% because the
// compiler still shares the lower continuation. An explicitly noinline continuation was rejected because it reproduced
// an approximately 12% runtime regression. Clang 17--23 performs that direct-scalar facade decision unaided. A separate
// recursive dynamic-star condition audit proves that this same facade is jointly necessary in the six-edge chain on
// Clang 21--23: deleting it restores a reachable proxy/native formatter graph after the format level has selected the
// active IO record. Clang 16--20 still fail with the complete chain and add 6.8--22.1 KiB of text, so the Clang range
// starts at the first proved endpoint and remains future-open.
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void print(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<false>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for print on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				::fast_io::details::print_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
				// clang-format off
static_assert(type_ok, "some types are not printable for print on default C's stdout");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for print");
static_assert(device_and_type_ok, "some types are not printable for print");
		// clang-format on
#endif
	}
}

template <typename T, typename... Args>
// Keep the line-terminating facade on the same measured GCC policy as print. GCC 11--16 otherwise outline it and lose
// both caller-expression constant discovery and the small unknown-value hot path, whereas Clang 17--23 needs no
// override. The lower continuation remains compiler-shareable, which bounds text growth at larger call-site counts.
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void println(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<true>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for println on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				::fast_io::details::print_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
					static_assert(type_ok, "some types are not printable for print on default C's stdout");
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for println");
static_assert(device_and_type_ok, "some types are not printable for println");
		// clang-format on
#endif
	}
}

template <typename T, typename... Args>
inline constexpr void perr(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization_cold<false>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING) && \
	  !defined(__AVR__)) ||                                                                                            \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for perr on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				::fast_io::details::perr_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
				// clang-format off
static_assert(type_ok, "some types are not printable for perr on native err");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for perr");
static_assert(device_and_type_ok, "some types are not printable for perr");
		// clang-format on
#endif
	}
}

template <typename T, typename... Args>
inline constexpr void perrln(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization_cold<true>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING) && \
	  !defined(__AVR__)) ||                                                                                            \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for perrln on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				::fast_io::details::perr_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
				// clang-format off
static_assert(type_ok, "some types are not printable for perrln on native err");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for perrln");
static_assert(device_and_type_ok, "some types are not printable for perrln");
		// clang-format on
#endif
	}
}

namespace panic_details
{

/// @brief Owns panic's exact historical perr/catch/terminate operation in a dedicated cold continuation.
/// @details This is a semantic fallback boundary, not a promise that an outlined public panic wrapper adds no call
///          frame. Current compilers may retain both functions when their noreturn/cold inline threshold is zero.
template <bool line, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
[[noreturn]] inline constexpr void fallback(Args &&...args) noexcept
{
#ifdef __cpp_exceptions
	try
	{
#endif
		if constexpr (sizeof...(Args) == 0u)
		{
			// A hosted source-free panic still owns one diagnostic print record.
			// The default error sink is normalized once, after which the print
			// level invokes an exact empty-status operation, emits a requested
			// newline, or discards an unobservable non-line record. A freestanding
			// build has no library-owned default error sink and terminates directly.
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
			::fast_io::details::perr_after_source_pre_normalization<line>();
#endif
		}
		else if constexpr (line)
		{
			::fast_io::io::perrln(::std::forward<Args>(args)...);
		}
		else
		{
			::fast_io::io::perr(::std::forward<Args>(args)...);
		}
#ifdef __cpp_exceptions
	}
	catch (...)
	{
	}
#endif
	::fast_io::fast_terminate();
}

} // namespace panic_details

/// @brief Prints an optional diagnostic through perr and terminates.
/// @details The speculative source gate is deliberately ordinary-inline. Current Clang and GCC may outline a noreturn
///          call at an effective zero inline threshold, in which case caller-literal `__builtin_constant_p` evidence is
///          unavailable and the exact cold fallback is selected. Do not force this diagnostic front door inline merely
///          to recover that optional strategy; the structural gate remains here for callers/targets which inline it
///          naturally and for future source representations carrying compile-time storage in their type. Consequently
///          this interface promises neither caller-literal folding nor a single generated call frame on current tools.
///          A source-free call still enters the same cold print-level fallback: ordinary native error output ignores
///          the empty record, while a future default sink with an exact zero-argument status operation may observe it.
template <typename... Args>
[[noreturn]] inline constexpr void panic(Args &&...args) noexcept
{
	if constexpr (sizeof...(Args) == 0u)
	{
		::fast_io::io::panic_details::fallback<false>();
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#ifdef __cpp_exceptions
		try
		{
#endif
			if (::fast_io::details::
					panic_try_compiler_constant_pre_normalization<false>(args...))
			{
				::fast_io::fast_terminate();
			}
#ifdef __cpp_exceptions
		}
		catch (...)
		{
			::fast_io::fast_terminate();
		}
#endif
#endif
		::fast_io::io::panic_details::fallback<false>(
			::std::forward<Args>(args)...);
	}
}

template <typename... Args>
	requires(sizeof...(Args) != 0)
[[noreturn]] inline constexpr void panicln(Args &&...args) noexcept
{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
#ifdef __cpp_exceptions
	try
	{
#endif
		if (::fast_io::details::
				panic_try_compiler_constant_pre_normalization<true>(args...))
		{
			::fast_io::fast_terminate();
		}
#ifdef __cpp_exceptions
	}
	catch (...)
	{
		::fast_io::fast_terminate();
	}
#endif
#endif
	::fast_io::io::panic_details::fallback<true>(
		::std::forward<Args>(args)...);
}

// Allow debug print
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
// With debugging. We output to POSIX fd or Win32 Handle directly instead of C's stdout.
template <typename T, typename... Args>
inline constexpr void debug_print(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<false, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<false>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for debug_print on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				if (::fast_io::details::debug_print_try_default_compiler_constant<false>(
						t, args...))
				{
					return;
				}
				fast_io::details::debug_print_after_source_pre_normalization<false>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
				// clang-format off
static_assert(type_ok, "some types are not printable for debug_print on native out");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for debug_print");
static_assert(device_and_type_ok, "some types are not printable for debug_print on native out");
		// clang-format on
#endif
	}
}

template <typename T, typename... Args>
inline constexpr void debug_println(T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<true, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		decltype(auto) outref = ::fast_io::operations::output_stream_ref(t);
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<true>(
				outref, args...);
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		if constexpr (device_ok)
		{
			if constexpr (::fast_io::details::has_raw_print_arg<Args...>)
			{
				::fast_io::details::print_raw_static_assert<Args...>();
			}
			else
			{
				static_assert(device_and_type_ok,
							  "some types are not printable for debug_println on the provided output stream");
			}
		}
		else
		{
			constexpr bool type_ok{::fast_io::operations::defines::print_freestanding_params_okay<char, T, Args...>};
			if constexpr (type_ok)
			{
				if (::fast_io::details::debug_print_try_default_compiler_constant<true>(
						t, args...))
				{
					return;
				}
				fast_io::details::debug_print_after_source_pre_normalization<true>(
					t, args...);
			}
			else
			{
				if constexpr (::fast_io::details::has_raw_print_arg<T, Args...>)
				{
					::fast_io::details::print_raw_static_assert<T, Args...>();
				}
				else
				{
				// clang-format off
static_assert(type_ok, "some types are not printable for debug_println on native out");
				// clang-format on
				}
			}
		}
#else
		constexpr bool device_ok{::fast_io::operations::defines::has_output_or_io_stream_ref_define<
			::std::remove_reference_t<T> &>};
		// clang-format off
static_assert(device_ok, "freestanding environment must provide IO device for debug_println");
static_assert(device_and_type_ok, "some types are not printable for debug_println on native out");
		// clang-format on
#endif
	}
}

template <typename... Args>
	requires(sizeof...(Args) != 0)
inline constexpr void debug_perr(Args &&...args)
{
	::fast_io::io::perr(::std::forward<Args>(args)...);
}

template <typename... Args>
	requires(sizeof...(Args) != 0)
inline constexpr void debug_perrln(Args &&...args)
{
	::fast_io::io::perrln(::std::forward<Args>(args)...);
}
#endif

template <bool report = false, typename input, typename... Args>
inline constexpr ::std::conditional_t<report, bool, void> scan(input &&in, Args &&...args)
{
	constexpr bool device_error{::fast_io::operations::defines::has_input_or_io_stream_ref_define<input>};
	if constexpr (device_error)
	{
		decltype(auto) inref = ::fast_io::operations::input_stream_ref(in);
		using char_type = typename ::std::remove_cvref_t<decltype(inref)>::input_char_type;
		if constexpr (report)
		{
			return ::fast_io::operations::decay::scan_freestanding_decay_borrowed_input(
				inref,
				::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(args))...);
		}
		else
		{
			if (!::fast_io::operations::decay::scan_freestanding_decay_borrowed_input(
					inref,
					::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(args))...))
			{
				::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
			}
		}
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	__has_include(<stdio.h>)
		return ::fast_io::details::scan_after_io_scan_forward<report>(
			::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(in)),
			::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(args))...);
#else
		// clang-format off
static_assert(device_error, "freestanding environment must provide IO device");
		// clang-format on
#endif
	}
}

} // namespace io

namespace iomnp
{
using namespace ::fast_io::io;
using namespace ::fast_io::mnp;
} // namespace iomnp

} // namespace fast_io
