#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>
#include <fast_io_dsal/string.h>

#ifndef FAST_IO_PRFCH_PUBLIC_PROBE_SITE
#error "FAST_IO_PRFCH_PUBLIC_PROBE_SITE must be 1 (concat) or 2 (print)."
#endif

#ifndef FAST_IO_PRFCH_PUBLIC_PROBE_PROVED
#error "FAST_IO_PRFCH_PUBLIC_PROBE_PROVED must be 0 or 1."
#endif

#if FAST_IO_PRFCH_PUBLIC_PROBE_SITE != 1 && FAST_IO_PRFCH_PUBLIC_PROBE_SITE != 2
#error "Unsupported FAST_IO_PRFCH_PUBLIC_PROBE_SITE value."
#endif

#if FAST_IO_PRFCH_PUBLIC_PROBE_PROVED != 0 && FAST_IO_PRFCH_PUBLIC_PROBE_PROVED != 1
#error "Unsupported FAST_IO_PRFCH_PUBLIC_PROBE_PROVED value."
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_PRFCH_PUBLIC_PROBE_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_PRFCH_PUBLIC_PROBE_NOINLINE
#endif

namespace fast_io_prfch_public_probe
{

inline constexpr ::std::size_t source_count{32u};

using source_type = ::std::conditional_t<
	FAST_IO_PRFCH_PUBLIC_PROBE_PROVED != 0,
	::fast_io::basic_prfch_cacheable_io_scatter_t<char>,
	::fast_io::basic_io_scatter_t<char>>;

template <::std::size_t... indices>
inline ::fast_io::string concat_public(
	source_type const *sources, ::std::index_sequence<indices...>)
{
	// This call is deliberately the public API. The probe must not force the internal scatter-copy helper's policy
	// template argument: entry aliasing, ABI decay, provenance transport, and the native platform classifier all remain
	// part of the generated code under inspection.
	return ::fast_io::concat_fast_io(sources[indices]...);
}

struct print_output
{
	using output_char_type = char;
	char *begin{};
	char **current{};
	char *end{};
};

inline constexpr print_output output_stream_ref_define(print_output output) noexcept
{
	return output;
}

inline constexpr char *obuffer_begin(print_output output) noexcept
{
	return output.begin;
}

inline constexpr char *obuffer_curr(print_output output) noexcept
{
	return *output.current;
}

inline constexpr char *obuffer_end(print_output output) noexcept
{
	return output.end;
}

inline constexpr void obuffer_set_curr(print_output output, char *current) noexcept
{
	*output.current = current;
}

inline void write_all_overflow_define(print_output output, char const *first, char const *last) noexcept
{
	::std::size_t const size{static_cast<::std::size_t>(last - first)};
	char *const current{*output.current};
	if (static_cast<::std::size_t>(output.end - current) < size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	char *const next{::fast_io::details::non_overlapped_copy_n(first, size, current)};
	*output.current = next;
}

template <::std::size_t... indices>
inline char *print_public(char *destination, ::std::size_t capacity,
						  source_type const *sources, ::std::index_sequence<indices...>)
{
	char *current{destination};
	// A caller-provided put area makes the complete public print materializer observable without a syscall or a
	// benchmark-only output shortcut. Dynamic capacity keeps both the admitted and fallback control flow in the probe.
	::fast_io::print(print_output{destination, __builtin_addressof(current), destination + capacity},
					 sources[indices]...);
	return current;
}

} // namespace fast_io_prfch_public_probe

#if FAST_IO_PRFCH_PUBLIC_PROBE_SITE == 1

extern "C" FAST_IO_PRFCH_PUBLIC_PROBE_NOINLINE ::fast_io::string
fast_io_prfch_public_concat_codegen(
	fast_io_prfch_public_probe::source_type const *sources)
{
	return ::fast_io_prfch_public_probe::concat_public(
		sources, ::std::make_index_sequence<
					 ::fast_io_prfch_public_probe::source_count>{});
}

#else

extern "C" FAST_IO_PRFCH_PUBLIC_PROBE_NOINLINE char *
fast_io_prfch_public_print_codegen(char *destination, ::std::size_t capacity,
								   fast_io_prfch_public_probe::source_type const *sources)
{
	return ::fast_io_prfch_public_probe::print_public(destination, capacity, sources,
													  ::std::make_index_sequence<
														  ::fast_io_prfch_public_probe::source_count>{});
}

#endif

#undef FAST_IO_PRFCH_PUBLIC_PROBE_NOINLINE
