#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

struct buffered_state
{
	std::array<char, 256u> storage{};
	char *current{storage.data()};
	std::size_t scatter_calls{};
	std::size_t overflow_calls{};
};

// This deliberately over-advertises every direct output shape.  A put area is
// still conclusive evidence that static payload pointers must not bypass the
// destination-owned buffer.
struct spoofed_buffered_sink
{
	using output_char_type = char;
	buffered_state *state{};
};

struct buffered_adapter
{
	buffered_state *state{};
};

inline constexpr spoofed_buffered_sink output_stream_ref_define(
	spoofed_buffered_sink sink) noexcept
{
	return sink;
}

inline constexpr spoofed_buffered_sink output_stream_ref_define(
	buffered_adapter sink) noexcept
{
	return {sink.state};
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, spoofed_buffered_sink>) noexcept
{
	return {};
}

inline constexpr char *obuffer_begin(spoofed_buffered_sink sink) noexcept
{
	return sink.state->storage.data();
}

inline constexpr char *obuffer_curr(spoofed_buffered_sink sink) noexcept
{
	return sink.state->current;
}

inline constexpr char *obuffer_end(spoofed_buffered_sink sink) noexcept
{
	return sink.state->storage.data() + sink.state->storage.size();
}

inline constexpr void obuffer_set_curr(
	spoofed_buffered_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void obuffer_overflow(spoofed_buffered_sink, char) noexcept
{
	::fast_io::fast_terminate();
}

inline constexpr bool obuffer_overflow_never(spoofed_buffered_sink) noexcept
{
	return true;
}

inline void append(
	buffered_state &state, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		*state.current++ = *first;
	}
}

inline void write_all_overflow_define(
	spoofed_buffered_sink sink, char const *first, char const *last) noexcept
{
	++sink.state->overflow_calls;
	append(*sink.state, first, last);
}

inline void scatter_write_all_overflow_define(
	spoofed_buffered_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		append(*sink.state, scatters[index].base,
			   scatters[index].base + scatters[index].len);
	}
}

inline void scatter_write_all_bytes_overflow_define(
	spoofed_buffered_sink sink, ::fast_io::io_scatter_t const *scatters,
	std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		auto const first{static_cast<char const *>(scatters[index].base)};
		append(*sink.state, first, first + scatters[index].len);
	}
}

static_assert(::fast_io::synchronous_direct_scatter_output<
			  char, spoofed_buffered_sink>);
static_assert(::fast_io::operations::decay::defines::
				  has_obuffer_basic_operations<spoofed_buffered_sink>);
static_assert(!::fast_io::details::decay::
				  print_output_retains_static_scatter<spoofed_buffered_sink>);

inline void reset(buffered_state &state) noexcept
{
	state.current = state.storage.data();
	state.scatter_calls = 0u;
	state.overflow_calls = 0u;
}

inline std::string_view view(buffered_state const &state) noexcept
{
	return {state.storage.data(),
			static_cast<std::size_t>(state.current - state.storage.data())};
}

template <typename output_type>
void test_output(output_type output)
{
	auto &state{*output.state};
	::fast_io::operations::print_freestanding<false>(output,
													 "pre:", 2, "/", ::fast_io::mnp::boolalpha(true), "/", 1.25,
													 ":post");
	assert(view(state) == "pre:2/true/1.25:post");
	assert(state.scatter_calls == 0u);
	assert(state.overflow_calls == 0u);

	reset(state);
	::fast_io::fmt::print<"pre:{}/{}/{}:post">(
		output, 2, ::fast_io::mnp::boolalpha(true), 1.25);
	assert(view(state) == "pre:2/true/1.25:post");
	assert(state.scatter_calls == 0u);
	assert(state.overflow_calls == 0u);

	reset(state);
	::fast_io::fmt::printf<"pre:%d/%s/%f:post">(
		output, 2, "true", 1.25);
	assert(view(state) == "pre:2/true/1.250000:post");
	assert(state.scatter_calls == 0u);
	assert(state.overflow_calls == 0u);
}

} // namespace

int main()
{
	buffered_state direct{};
	test_output(spoofed_buffered_sink{__builtin_addressof(direct)});

	buffered_state adapted{};
	test_output(buffered_adapter{__builtin_addressof(adapted)});
}
