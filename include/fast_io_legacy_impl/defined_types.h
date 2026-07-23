#pragma once

#if __has_include(<stdio.h>)
#include "c/impl.h"
#endif

namespace fast_io
{
#if !defined(__AVR__)

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	native_io_observer
	in() noexcept
{
	return native_stdin();
}

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	native_io_observer
	out() noexcept
{
	return native_stdout();
}

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	native_io_observer
	err() noexcept
{
	return native_stderr();
}

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	decltype(auto)
	u8in() noexcept
{
	return native_stdin<char8_t>();
}

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	decltype(auto)
	u8out() noexcept
{
	return native_stdout<char8_t>();
}

inline
#if defined(__WINE__) || !defined(_WIN32)
	constexpr
#endif
	decltype(auto)
	u8err() noexcept
{
	return native_stderr<char8_t>();
}

using in_buf_type = basic_ibuf<native_io_observer>;
using out_buf_type = basic_obuf<native_io_observer>;

using u8in_buf_type = basic_ibuf<u8native_io_observer>;
using u8out_buf_type = basic_obuf<u8native_io_observer>;

using in_buf_type_lockable = basic_io_lockable<in_buf_type>;
using out_buf_type_lockable = basic_io_lockable<out_buf_type>;

using u8in_buf_type_lockable = basic_io_lockable<u8in_buf_type>;
using u8out_buf_type_lockable = basic_io_lockable<u8out_buf_type>;

#endif

/// @brief Authorizes exact compiler-constant writes into fast_io's unlocked C-stream put area.
/// @details The unlocked observer exposes the C implementation's live buffer cursor directly; its `obuffer_set_curr`
///          customization publishes only the advanced pointer.  A standard C observer reaches this type only after the
///          public print operation has acquired the stream mutex, while an explicitly unlocked observer already makes
///          synchronization the caller's responsibility.  In both cases an exact reserve define followed by one cursor
///          publication is equivalent to the ordinary in-buffer write and need not stage a proxy across the lock.
template <::std::integral char_type>
inline constexpr ::std::true_type print_compiler_constant_obuffer_materialization_safe(
	::fast_io::io_reserve_type_t<
		char_type, ::fast_io::basic_c_io_observer_unlocked<char_type>>) noexcept
{
	return {};
}

namespace details
{

/// @brief Sends default-output source expressions through the shared pre-normalization constant gate.
/// @details `print_after_io_print_forward` is the compatibility boundary for callers which already own normalized
///          values. The public default-output front door still has the original source expressions and must not erase
///          compiler-constant evidence before core print has made its optional strategy decision. Materialization CPOs
///          admitted here are pure by the stronger source marker; a C stdout mutex is still acquired by the historical
///          dispatcher after that local value transformation, and an unlocked status-print owner disables the strategy.
template <bool line, typename... Args>
// Tested GCC 13-16 do not inline this bridge at -O3 even when the public facade is
// inlined. Keeping it outlined loses __builtin_constant_p evidence and turns
// default-output constant integer/floating probes into calls to the runtime print
// instance; the measured forced chain is also smaller in aggregate. The positive
// GCC policy remains open until a newer compiler measures a reversal; Clang remains ordinary.
#if defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void print_after_source_pre_normalization(Args &&...args)
{
#if __has_include(<stdio.h>)
	auto output{c_stdout()};
#else
	auto output{out()};
#endif
	decltype(auto) outref{::fast_io::operations::output_stream_ref(output)};
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization<line>(
			outref, args...);
}

template <bool line, typename... Args>
inline constexpr void print_after_io_print_forward(Args... args)
{
#if __has_include(<stdio.h>)
	::fast_io::operations::decay::print_freestanding_decay<line>(c_stdout(), args...);
#else
	::fast_io::operations::decay::print_freestanding_decay<line>(out(), args...);
#endif
}

template <bool line, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void perr_after_io_print_forward(Args... args)
{
#if defined(__AVR__)
	::fast_io::operations::decay::print_freestanding_decay<line>(c_stderr(), args...);
#else
	::fast_io::operations::decay::print_freestanding_decay<line>(err(), args...);
#endif
}

/// @brief Sends native-error source expressions through the constant gate while retaining the cold unknown path.
/// @details The arguments are deliberately presented as this helper's named lvalues. That is the public legacy
///          normalization boundary: ref-qualified alias and status-forwarding CPOs must observe the same category for
///          a caller temporary that they observed before the compiler-constant strategy existed.
template <bool line, typename... Args>
inline constexpr void perr_after_source_pre_normalization(Args &&...args)
{
#if defined(__AVR__)
	auto output{c_stderr()};
#else
	auto output{err()};
#endif
	decltype(auto) outref{::fast_io::operations::output_stream_ref(output)};
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization_cold<line>(
			outref, args...);
}

/// @brief Tries panic's constant arm using the same device-first classification as public perr/perrln.
/// @details Every source is observed through this helper's named parameter, matching the normalization category in the
///          subsequent public perr frame. Type-only strategy admission and the optimizer query both precede output
///          normalization. Therefore a false result has touched neither the output object nor a source-normalization
///          CPO, and the cold fallback remains the sole owner of those observable operations. Only a proven true arm
///          obtains the output reference and emits through it exactly once.
template <bool line, typename T, typename... Args>
inline constexpr bool panic_try_compiler_constant_pre_normalization(
	T &&t, Args &&...args)
{
	constexpr bool device_and_type_ok{
		::fast_io::operations::defines::print_freestanding_okay_for_line<
			line, T, Args...>};
	if constexpr (device_and_type_ok)
	{
		using output_type = ::std::remove_cvref_t<decltype(
			::fast_io::operations::output_stream_ref(
				::std::declval<::std::remove_reference_t<T> &>()))>;
		using char_type = typename output_type::output_char_type;
		if constexpr (
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_cold_codegen_supported<
					output_type, Args &...>())
		{
			if constexpr (
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_available<
						line, output_type, Args &...>())
			{
				if (::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_gate<char_type>(
							args...))
				{
					decltype(auto) outref{
						::fast_io::operations::output_stream_ref(t)};
					::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_true_emit_after_lock<line>(
							outref, args...);
					return true;
				}
			}
		}
		return false;
	}
	else
	{
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING) && \
	  !defined(__AVR__)) ||                                                                                            \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES))
		constexpr bool device_ok{
			::fast_io::operations::defines::
				has_output_or_io_stream_ref_define<
					::std::remove_reference_t<T> &>};
		constexpr bool type_ok{
			::fast_io::operations::defines::print_freestanding_params_okay<
				char, T, Args...>};
		if constexpr (!device_ok && type_ok)
		{
#if defined(__AVR__)
			using output_owner = decltype(c_stderr());
#else
			using output_owner = decltype(err());
#endif
			using output_type = ::std::remove_cvref_t<decltype(
				::fast_io::operations::output_stream_ref(
					::std::declval<output_owner &>()))>;
			using char_type = typename output_type::output_char_type;
			if constexpr (
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_cold_codegen_supported<
						output_type, T &, Args &...>())
			{
				if constexpr (
					::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_available<
							line, output_type, T &, Args &...>())
				{
					if (::fast_io::operations::decay::
							print_compiler_constant_pre_normalization_gate<char_type>(
								t, args...))
					{
#if defined(__AVR__)
						auto output{c_stderr()};
#else
						auto output{err()};
#endif
						decltype(auto) outref{
							::fast_io::operations::output_stream_ref(output)};
						::fast_io::operations::decay::
							print_compiler_constant_pre_normalization_true_emit_after_lock<line>(
								outref, t, args...);
						return true;
					}
				}
			}
		}
#endif
		return false;
	}
}

template <bool line, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr void debug_print_after_io_print_forward(Args... args)
{
#if defined(__AVR__)
	::fast_io::operations::decay::print_freestanding_decay<line>(c_stdout(), args...);
#else
	::fast_io::operations::decay::print_freestanding_decay<line>(out(), args...);
#endif
}

/// @brief Attempts only a cold-level-proved compiler-constant arm for the native debug sink.
/// @details The diagnostic code-generation predicate is evaluated before the ordinary availability proof. This nested
///          ordering makes replacement formation impossible for a rejected cold record; a false result then leaves the
///          established debug continuation as the sole owner of aliasing, status, and output normalization. GCC 12/13
///          require this one forced boundary to propagate all four line/non-line, constant/runtime roots into
///          .text.unlikely. A 35-case A/B audit found no instruction, call-graph, proxy, planner, native-writer, or total
///          text-size change on GCC 11--17; therefore the attribute is restricted to the only releases with a measured
///          cold-layout effect and is deliberately absent from the public debug facades.
template <bool line, typename... Args>
#if defined(__GNUC__) && !defined(__clang__) && 12 <= __GNUC__ && __GNUC__ <= 13
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr bool debug_print_try_default_compiler_constant(Args &...args)
{
#if defined(__AVR__)
	using output_owner = decltype(c_stdout());
#else
	using output_owner = decltype(out());
#endif
	using output_type = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(
			::std::declval<output_owner &>()))>;
	using char_type = typename output_type::output_char_type;
	if constexpr (
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_cold_codegen_supported<
				output_type, decltype((args))...>())
	{
		if constexpr (
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_available<
					line, output_type, decltype((args))...>())
		{
			if (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_gate<char_type>(args...))
			{
#if defined(__AVR__)
				auto output{c_stdout()};
#else
				auto output{out()};
#endif
				decltype(auto) outref{
					::fast_io::operations::output_stream_ref(output)};
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_true_emit_after_lock<line>(
						outref, args...);
				return true;
			}
		}
	}
	return false;
}

/// @brief Applies the default debug sink's cold-level proof before the historical source-normalization continuation.
/// @details This mirrors `debug_print_after_io_print_forward`'s AVR/native sink choice. Its named arguments preserve the
///          legacy alias/status category. On a compiler/version whose cold proof rejects a value-query record, the
///          discarded branch cannot form the replacement type or name the optimizer query; the exact historical helper
///          remains the sole continuation. An admitted true arm obtains the sink only after the query and emits once
///          through the same output-level mutex protocol used by print and panic.
template <bool line, typename... Args>
inline constexpr void debug_print_after_source_pre_normalization(Args &&...args)
{
#if defined(__AVR__)
	using output_owner = decltype(c_stdout());
#else
	using output_owner = decltype(out());
#endif
	using output_type = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(
			::std::declval<output_owner &>()))>;
	using char_type = typename output_type::output_char_type;
	if constexpr (
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_cold_codegen_supported<
				output_type, decltype((args))...>())
	{
		if constexpr (
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_available<
					line, output_type, decltype((args))...>())
		{
			if (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_gate<char_type>(args...))
			{
#if defined(__AVR__)
				auto output{c_stdout()};
#else
				auto output{out()};
#endif
				decltype(auto) outref{
					::fast_io::operations::output_stream_ref(output)};
				::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_true_emit_after_lock<line>(
						outref, args...);
				return;
			}
		}
	}
	::fast_io::details::debug_print_after_io_print_forward<line>(
		::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(args))...);
}

/// @brief Dispatches the already-normalized default-stdin scanner pack without materializing it again.
/// @details `io_scan_alias` and `io_scan_forward` may deliberately produce a noncopyable lvalue proxy. Taking this
///          boundary by value used to copy that proxy before the real scan dispatcher, even though explicit-input
///          scanning retained the same proxy by reference. Forwarding references preserve both stable references and
///          owned prvalues for the duration of this complete call; the downstream dispatcher names each target as an
///          lvalue while scanning, so no scanner CPO observes an accidental rvalue category.
template <bool report, typename... Args>
inline constexpr ::std::conditional_t<report, bool, void> scan_after_io_scan_forward(Args &&...args)
{
#if __has_include(<stdio.h>)
	if constexpr (report)
	{
		return ::fast_io::operations::decay::scan_freestanding_decay(
			c_stdin(), ::std::forward<Args>(args)...);
	}
	else
	{
		if (!::fast_io::operations::decay::scan_freestanding_decay(
				c_stdin(), ::std::forward<Args>(args)...))
		{
			::fast_io::throw_parse_code(fast_io::parse_code::end_of_file);
		}
	}
#endif
}

} // namespace details

} // namespace fast_io
