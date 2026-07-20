#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

struct sink_state
{
	std::string output;
	std::size_t write_calls{};
	std::size_t scatter_calls{};
};

struct direct_sink
{
	using output_char_type = char;
	sink_state *state;
};

inline constexpr direct_sink output_stream_ref_define(direct_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(direct_sink sink, char const *first,
									  char const *last)
{
	++sink.state->write_calls;
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(
	direct_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count)
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(scatters[index].base, scatters[index].len);
	}
}

inline constexpr std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return 16u;
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

struct byte_sink
{
	using output_char_type = char;
	sink_state *state;
};

inline constexpr byte_sink output_stream_ref_define(byte_sink sink) noexcept
{
	return sink;
}

inline void write_all_bytes_overflow_define(
	byte_sink sink, std::byte const *first, std::byte const *last)
{
	++sink.state->write_calls;
	sink.state->output.append(reinterpret_cast<char const *>(first),
							  reinterpret_cast<char const *>(last));
}

inline void scatter_write_all_bytes_overflow_define(
	byte_sink sink, ::fast_io::io_scatter_t const *scatters,
	std::size_t count)
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(
			reinterpret_cast<char const *>(scatters[index].base),
			scatters[index].len);
	}
}

inline constexpr std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, byte_sink>) noexcept
{
	return 16u;
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, byte_sink>) noexcept
{
	return {};
}

struct buffered_state
{
	char storage[64]{};
	char *current{storage};
	std::size_t write_calls{};
	std::size_t scatter_calls{};
};

struct buffered_sink
{
	using output_char_type = char;
	buffered_state *state;
};

inline constexpr buffered_sink output_stream_ref_define(buffered_sink sink) noexcept
{
	return sink;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline constexpr char *obuffer_begin(buffered_sink sink) noexcept
{
	return sink.state->storage;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline constexpr char *obuffer_curr(buffered_sink sink) noexcept
{
	return sink.state->current;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline constexpr char *obuffer_end(buffered_sink sink) noexcept
{
	return sink.state->storage + sizeof(sink.state->storage);
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline constexpr void obuffer_set_curr(buffered_sink sink, char *current) noexcept
{
	sink.state->current = current;
}

inline void write_all_overflow_define(buffered_sink sink, char const *first,
									  char const *last)
{
	++sink.state->write_calls;
	std::copy(first, last, sink.state->current);
	sink.state->current += last - first;
}

inline void scatter_write_all_overflow_define(
	buffered_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	std::size_t count)
{
	++sink.state->scatter_calls;
	for (std::size_t index{}; index != count; ++index)
	{
		std::copy_n(scatters[index].base, scatters[index].len,
					sink.state->current);
		sink.state->current += scatters[index].len;
	}
}

inline constexpr std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, buffered_sink>) noexcept
{
	// A put area is independently cheaper than copying through this unbuffered-output policy.
	return 16u;
}

inline constexpr std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, buffered_sink>) noexcept
{
	// This deliberately over-advertises the marker to prove that the put-area gate still wins.
	return {};
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
double runtime_double(double value) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : "+x"(value));
#endif
	return value;
}

} // namespace

int main()
{
	double const value{runtime_double(3.2)};
	char const small_left[]{"a"};
	char const small_right[]{"b"};
	::fast_io::basic_io_scatter_t<char> const small_left_scatter{
		small_left, sizeof(small_left) - 1u};
	::fast_io::basic_io_scatter_t<char> const small_right_scatter{
		small_right, sizeof(small_right) - 1u};

	sink_state raw_small_state;
	::fast_io::print(direct_sink{&raw_small_state}, small_left_scatter, value,
					 small_right_scatter);
	assert(raw_small_state.output == "a3.2b");
	assert(raw_small_state.write_calls == 1u);
	assert(raw_small_state.scatter_calls == 0u);

	sink_state format_small_state;
	::fast_io::fmt::print<"{}{}{}">(direct_sink{&format_small_state},
									small_left_scatter, value, small_right_scatter);
	assert(format_small_state.output == "a3.2b");
	assert(format_small_state.write_calls == 1u);
	assert(format_small_state.scatter_calls == 0u);

	sink_state byte_small_state;
	::fast_io::fmt::print<"{}{}{}">(byte_sink{&byte_small_state},
									small_left_scatter, value, small_right_scatter);
	assert(byte_small_state.output == "a3.2b");
	assert(byte_small_state.write_calls == 1u);
	assert(byte_small_state.scatter_calls == 0u);

	sink_state dynamic_precision_state;
	std::size_t const precision{static_cast<std::size_t>(runtime_double(2.0))};
	::fast_io::fmt::print<"{0}:{0:.{1}f}">(direct_sink{&dynamic_precision_state},
										   value, precision);
	assert(dynamic_precision_state.output == "3.2:3.20");
	assert(dynamic_precision_state.write_calls == 1u);
	assert(dynamic_precision_state.scatter_calls == 0u);

	char const large_left[]{"0123456789"};
	char const large_right[]{"abcdefghij"};
	::fast_io::basic_io_scatter_t<char> const large_left_scatter{
		large_left, sizeof(large_left) - 1u};
	::fast_io::basic_io_scatter_t<char> const large_right_scatter{
		large_right, sizeof(large_right) - 1u};
	sink_state large_state;
	::fast_io::print(direct_sink{&large_state}, large_left_scatter, value,
					 large_right_scatter);
	assert(large_state.output == "01234567893.2abcdefghij");
	assert(large_state.write_calls == 0u);
	assert(large_state.scatter_calls == 1u);

	buffered_state buffered;
	::fast_io::fmt::print<"{}{}{}">(buffered_sink{&buffered}, small_left_scatter,
									value, small_right_scatter);
	assert(std::string(buffered.storage, buffered.current) == "a3.2b");
	assert(buffered.write_calls == 0u);
	assert(buffered.scatter_calls == 0u);
}
