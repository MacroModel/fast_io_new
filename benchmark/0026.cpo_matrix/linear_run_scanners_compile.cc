#include <fast_io.h>

#ifndef FAST_IO_COMPILE_PACK
#define FAST_IO_COMPILE_PACK 1024
#endif

namespace linear_run_scanners_compile
{
template <::std::size_t index>
struct field
{};

template <::std::size_t index>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char, field<index>>) noexcept
{
	return 1u + index % 7u;
}

template <::std::size_t index>
inline constexpr char *print_reserve_define(::fast_io::io_reserve_type_t<char, field<index>>, char *first, field<index>) noexcept
{
	constexpr ::std::size_t size{1u + index % 7u};
	for (::std::size_t offset{}; offset != size; ++offset)
	{
		first[offset] = 'x';
	}
	return first + size;
}

struct context
{};

struct context_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(context, char *first, char *) noexcept
	{
		return {first, true};
	}
};

inline constexpr ::fast_io::io_type_t<context_state> print_context_type(::fast_io::io_reserve_type_t<char, context>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_context_static_buffer_size(::fast_io::io_reserve_type_t<char, context>) noexcept
{
	return 32u;
}

template <::std::size_t... index>
inline consteval bool verify(::std::index_sequence<index...>) noexcept
{
	constexpr auto scatter_scan{
		::fast_io::details::decay::find_continuous_scatters_n<char, field<index>..., context>()};
	constexpr auto context_scan{
		::fast_io::details::decay::find_context_capture_run_n<char, field<index>..., context>()};
	::std::size_t size{};
	for (::std::size_t position{}; position != sizeof...(index); ++position)
	{
		size += 1u + position % 7u;
	}
	return scatter_scan.position == sizeof...(index) && scatter_scan.neededspace == size &&
		   !scatter_scan.lastisreserve && context_scan.position == sizeof...(index) + 1u &&
		   context_scan.has_context && context_scan.context_buffer_size == 32u &&
		   context_scan.max_static_reserve_burst_size == size;
}

static_assert(verify(::std::make_index_sequence<FAST_IO_COMPILE_PACK>{}));
} // namespace linear_run_scanners_compile

int main()
{}
