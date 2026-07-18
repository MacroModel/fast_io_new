#include <cstddef>
#include <string_view>

#include <fast_io.h>

#ifndef FAST_IO_RESERVE_SCATTER_TOKEN_COUNT
#define FAST_IO_RESERVE_SCATTER_TOKEN_COUNT 9u
#endif

namespace
{

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_ASM_NOINLINE [[gnu::noinline]]
#else
#define FAST_IO_ASM_NOINLINE
#endif

#if defined(FAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER)
#define FAST_IO_PRODUCER_SPECIFIER FAST_IO_ASM_NOINLINE inline
#else
#define FAST_IO_PRODUCER_SPECIFIER inline constexpr
#endif

struct byte_scatter_sink
{
	using output_char_type = char;
};

inline constexpr byte_scatter_sink output_stream_ref_define(byte_scatter_sink sink) noexcept
{
	return sink;
}

FAST_IO_ASM_NOINLINE inline void write_all_bytes_overflow_define(
	byte_scatter_sink, ::std::byte const *first, ::std::byte const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

FAST_IO_ASM_NOINLINE inline void scatter_write_all_bytes_overflow_define(
	byte_scatter_sink, ::fast_io::io_scatter_t const *scatters, ::std::size_t count) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(scatters), "r"(count) : "memory");
#else
	(void)scatters;
	(void)count;
#endif
}

struct scatter_plan_token
{
	::std::string_view first;
	::std::string_view second;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	return {3u, 0u};
}

FAST_IO_PRODUCER_SPECIFIER ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	scatter_plan_token token) noexcept
{
#if defined(FAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER) && (defined(__GNUC__) || defined(__clang__))
	// Model a producer compiled in another translation unit: the caller may rely on the CPO contract but cannot inspect
	// or scalar-replace its descriptor writes.
	__asm__ __volatile__("" : : "r"(scatters) : "memory");
#endif
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}

#if defined(FAST_IO_RESERVE_SCATTER_NATIVE_BYTES)
FAST_IO_PRODUCER_SPECIFIER ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>, ::fast_io::io_scatter_t *scatters,
	char *reserve, scatter_plan_token token) noexcept
{
#if defined(FAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER) && (defined(__GNUC__) || defined(__clang__))
	__asm__ __volatile__("" : : "r"(scatters) : "memory");
#endif
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}
static_assert(::fast_io::reserve_scatters_bytes_printable<char, scatter_plan_token const &>);
static_assert(::fast_io::reserve_scatters_bytes_printable<char, scatter_plan_token>);
static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char, ::fast_io::parameter<scatter_plan_token const &>>);
#endif

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	// Every descriptor refers to a string literal, so the grouped plan may retain it through the final fake write.
	return {};
}

template <::std::size_t... indices>
inline void print_runtime_plan(
	scatter_plan_token const *tokens, ::std::index_sequence<indices...>)
{
	::fast_io::io_scatter_t scatters[sizeof...(indices) * 3u];
	char reserve{};
	auto current{scatters};
	((current = ::fast_io::details::decay::prrsvsct_byte_common_rsvsc_impl(
					current, __builtin_addressof(reserve), tokens[indices])
					.scatters_pos_ptr),
	 ...);
	scatter_write_all_bytes_overflow_define(
		byte_scatter_sink{}, scatters, static_cast<::std::size_t>(current - scatters));
}

} // namespace

// Keep one stable external symbol for objdump/llvm-mca extraction. Calling the adapter directly excludes unrelated
// whole-pack selection: the body contains exactly producer calls, typed-to-byte fallback conversion when needed, and
// one fake scatter sink. The sink itself performs no copy.
extern "C" FAST_IO_ASM_NOINLINE void fast_io_reserve_scatter_byte_adapter_probe(
	scatter_plan_token const *tokens)
{
	print_runtime_plan(
		tokens, ::std::make_index_sequence<FAST_IO_RESERVE_SCATTER_TOKEN_COUNT>{});
}

int main()
{
	scatter_plan_token tokens[FAST_IO_RESERVE_SCATTER_TOKEN_COUNT];
	for (::std::size_t index{}; index != FAST_IO_RESERVE_SCATTER_TOKEN_COUNT; ++index)
	{
		// Values are populated at run time and cross the noinline boundary. This prevents literal length folding from
		// making the typed-conversion fallback disappear before it can be compared with the exact byte CPO.
		tokens[index] = {(index & 1u) == 0u ? "left" : "L", (index & 1u) == 0u ? "R" : "right"};
	}
	fast_io_reserve_scatter_byte_adapter_probe(tokens);
}
