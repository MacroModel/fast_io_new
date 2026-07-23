#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>

#include <fast_io_core.h>

namespace seek_positional_current_offset
{

struct operation_state
{
	::fast_io::intfpos_t current{};
	::std::array<::fast_io::intfpos_t, 256u> offsets{};
	::std::size_t calls{};
	::std::size_t maximum_progress{::std::numeric_limits<::std::size_t>::max()};
	::std::array<::std::byte, 1024u> source{};
};

inline void reset(operation_state &state, ::fast_io::intfpos_t position) noexcept
{
	state.current = position;
	state.calls = 0u;
	state.maximum_progress = ::std::numeric_limits<::std::size_t>::max();
}

inline void record(operation_state &state, ::fast_io::intfpos_t offset) noexcept
{
	assert(state.calls < state.offsets.size());
	state.offsets[state.calls++] = offset;
}

inline ::fast_io::intfpos_t seek(operation_state &state, ::fast_io::intfpos_t offset,
								 ::fast_io::seekdir direction) noexcept
{
	if (direction == ::fast_io::seekdir::beg)
	{
		state.current = offset;
	}
	else if (direction == ::fast_io::seekdir::cur)
	{
		state.current += offset;
	}
	else
	{
		assert(false);
	}
	return state.current;
}

struct wide_byte_output
{
	using output_char_type = char16_t;
	operation_state *state{};
};

inline constexpr wide_byte_output output_stream_ref_define(wide_byte_output output) noexcept
{
	return output;
}

inline ::fast_io::intfpos_t output_stream_seek_bytes_define(
	wide_byte_output output, ::fast_io::intfpos_t offset, ::fast_io::seekdir direction) noexcept
{
	return seek(*output.state, offset, direction);
}

inline ::std::byte const *pwrite_some_bytes_overflow_define(
	wide_byte_output output, ::std::byte const *first, ::std::byte const *last,
	::fast_io::intfpos_t offset) noexcept
{
	record(*output.state, offset);
	auto const size{static_cast<::std::size_t>(last - first)};
	auto const progress{size < output.state->maximum_progress ? size : output.state->maximum_progress};
	return first + progress;
}

struct character_output
{
	using output_char_type = char;
	operation_state *state{};
};

inline constexpr character_output output_stream_ref_define(character_output output) noexcept
{
	return output;
}

inline ::fast_io::intfpos_t output_stream_seek_define(
	character_output output, ::fast_io::intfpos_t offset, ::fast_io::seekdir direction) noexcept
{
	return seek(*output.state, offset, direction);
}

inline char const *pwrite_some_overflow_define(character_output output, char const *, char const *last,
											   ::fast_io::intfpos_t offset) noexcept
{
	record(*output.state, offset);
	return last;
}

struct wide_byte_input
{
	using input_char_type = char16_t;
	operation_state *state{};
};

inline constexpr wide_byte_input input_stream_ref_define(wide_byte_input input) noexcept
{
	return input;
}

inline ::fast_io::intfpos_t input_stream_seek_bytes_define(
	wide_byte_input input, ::fast_io::intfpos_t offset, ::fast_io::seekdir direction) noexcept
{
	return seek(*input.state, offset, direction);
}

inline ::std::byte *pread_some_bytes_underflow_define(
	wide_byte_input input, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t offset) noexcept
{
	record(*input.state, offset);
	auto const size{static_cast<::std::size_t>(last - first)};
	auto const progress{size < input.state->maximum_progress ? size : input.state->maximum_progress};
	assert(offset >= 0);
	auto const unsigned_offset{static_cast<::std::size_t>(offset)};
	assert(unsigned_offset <= input.state->source.size());
	assert(progress <= input.state->source.size() - unsigned_offset);
	::std::memcpy(first, input.state->source.data() + unsigned_offset, progress);
	return first + progress;
}

struct character_input
{
	using input_char_type = char;
	operation_state *state{};
};

inline constexpr character_input input_stream_ref_define(character_input input) noexcept
{
	return input;
}

inline ::fast_io::intfpos_t input_stream_seek_define(
	character_input input, ::fast_io::intfpos_t offset, ::fast_io::seekdir direction) noexcept
{
	return seek(*input.state, offset, direction);
}

inline char *pread_some_underflow_define(character_input input, char *first, char *last,
										 ::fast_io::intfpos_t offset) noexcept
{
	record(*input.state, offset);
	auto const size{static_cast<::std::size_t>(last - first)};
	assert(offset >= 0);
	auto const unsigned_offset{static_cast<::std::size_t>(offset)};
	assert(unsigned_offset <= input.state->source.size());
	assert(size <= input.state->source.size() - unsigned_offset);
	::std::memcpy(first, input.state->source.data() + unsigned_offset, size);
	return last;
}

inline void require_single_call(operation_state const &state, ::fast_io::intfpos_t expected_offset)
{
	assert(state.calls == 1u);
	assert(state.offsets[0] == expected_offset);
}

inline void require_two_calls(operation_state const &state, ::fast_io::intfpos_t first_offset,
							  ::fast_io::intfpos_t second_offset)
{
	assert(state.calls == 2u);
	assert(state.offsets[0] == first_offset);
	assert(state.offsets[1] == second_offset);
}

inline void require_linear_calls(operation_state const &state, ::std::size_t count,
								 ::fast_io::intfpos_t first_offset, ::fast_io::intfpos_t stride)
{
	assert(state.calls == count);
	for (::std::size_t i{}; i != count; ++i)
	{
		assert(state.offsets[i] ==
			   first_offset + static_cast<::fast_io::intfpos_t>(i) * stride);
	}
}

} // namespace seek_positional_current_offset

int main()
{
	using namespace seek_positional_current_offset;

	operation_state state{};
	for (::std::size_t i{}; i != state.source.size(); ++i)
	{
		state.source[i] = static_cast<::std::byte>(i);
	}

	wide_byte_output wide_output{__builtin_addressof(state)};
	char16_t wide_text[]{u'A', u'B'};
	::fast_io::basic_io_scatter_t<char16_t> wide_output_scatter{wide_text, 2u};
	::std::byte const *const wide_output_bytes{reinterpret_cast<::std::byte const *>(wide_text)};
	::fast_io::io_scatter_t wide_output_byte_scatter{wide_output_bytes, sizeof(wide_text)};

	// A byte-only backend permits an unaligned current position. Typed synthesis must retain that exact byte origin.
	reset(state, 3);
	auto const *wide_end{::fast_io::operations::write_some(wide_output, wide_text, wide_text + 1)};
	assert(wide_end == wide_text + 1);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	// A byte primitive may stop within one character. The adapter must finish that representation at the adjacent
	// byte offset before returning typed progress, without rounding the original unaligned position.
	reset(state, 3);
	state.maximum_progress = 1u;
	wide_end = ::fast_io::operations::write_some(wide_output, wide_text, wide_text + 1);
	assert(wide_end == wide_text + 1);
	require_two_calls(state, 3, 4);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	reset(state, 3);
	auto status{::fast_io::operations::scatter_write_some(wide_output, &wide_output_scatter, 1u)};
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_text)));

	reset(state, 3);
	state.maximum_progress = 1u;
	status = ::fast_io::operations::scatter_write_some(wide_output, &wide_output_scatter, 1u);
	assert(status.position == 0u && status.position_in_scatter == 1u);
	require_two_calls(state, 3, 4);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	reset(state, 3);
	::fast_io::operations::scatter_write_all(wide_output, &wide_output_scatter, 1u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_text)));

	// Crossing the bounded typed-to-byte descriptor workspace must preserve the byte offset between chunks.
	constexpr ::std::size_t output_conversion_count{
		::fast_io::details::scatter_byte_conversion_stack_capacity + 1u};
	::std::array<char16_t, output_conversion_count> wide_chunk_text{};
	::std::array<::fast_io::basic_io_scatter_t<char16_t>, output_conversion_count> wide_chunk_scatters{};
	for (::std::size_t i{}; i != output_conversion_count; ++i)
	{
		wide_chunk_scatters[i] = {wide_chunk_text.data() + i, 1u};
	}
	reset(state, 3);
	::fast_io::operations::scatter_write_all(
		wide_output, wide_chunk_scatters.data(), wide_chunk_scatters.size());
	require_linear_calls(state, output_conversion_count, 3,
						 static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));
	assert(state.current ==
		   3 + static_cast<::fast_io::intfpos_t>(output_conversion_count * sizeof(char16_t)));

	reset(state, 3);
	status = ::fast_io::operations::scatter_write_some_bytes(wide_output, &wide_output_byte_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_text)));

	reset(state, 3);
	::fast_io::operations::scatter_write_all_bytes(wide_output, &wide_output_byte_scatter, 1u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_text)));

	character_output char_output{__builtin_addressof(state)};
	char text[]{'a', 'b', 'c'};
	::fast_io::basic_io_scatter_t<char> char_output_scatter{text, sizeof(text)};
	::fast_io::io_scatter_t char_output_byte_scatter{text, sizeof(text)};

	reset(state, 7);
	status = ::fast_io::operations::scatter_write_some(char_output, &char_output_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	::fast_io::operations::scatter_write_all(char_output, &char_output_scatter, 1u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	status = ::fast_io::operations::scatter_write_some_bytes(char_output, &char_output_byte_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	::fast_io::operations::scatter_write_all_bytes(char_output, &char_output_byte_scatter, 1u);
	require_single_call(state, 7);
	assert(state.current == 10);

	// A byte-to-typed some adaptation may expose only its bounded prefix; all adaptation must then consume both chunks.
	::std::array<char, output_conversion_count> character_chunk_text{};
	::std::array<::fast_io::io_scatter_t, output_conversion_count> character_chunk_byte_scatters{};
	for (::std::size_t i{}; i != output_conversion_count; ++i)
	{
		character_chunk_byte_scatters[i] = {character_chunk_text.data() + i, 1u};
	}
	reset(state, 7);
	status = ::fast_io::operations::scatter_write_some_bytes(
		char_output, character_chunk_byte_scatters.data(), character_chunk_byte_scatters.size());
	assert(status.position == ::fast_io::details::scatter_byte_conversion_stack_capacity);
	assert(status.position_in_scatter == 0u);
	require_linear_calls(state, ::fast_io::details::scatter_byte_conversion_stack_capacity, 7, 1);
	assert(state.current ==
		   7 + static_cast<::fast_io::intfpos_t>(::fast_io::details::scatter_byte_conversion_stack_capacity));

	reset(state, 7);
	::fast_io::operations::scatter_write_all_bytes(
		char_output, character_chunk_byte_scatters.data(), character_chunk_byte_scatters.size());
	require_linear_calls(state, output_conversion_count, 7, 1);
	assert(state.current == 7 + static_cast<::fast_io::intfpos_t>(output_conversion_count));

	wide_byte_input wide_input{__builtin_addressof(state)};
	char16_t wide_destination[2]{};
	::fast_io::basic_io_scatter_t<char16_t> wide_input_scatter{wide_destination, 2u};
	::fast_io::io_scatter_t wide_input_byte_scatter{wide_destination, sizeof(wide_destination)};

	reset(state, 3);
	auto *wide_read_end{::fast_io::operations::read_some(wide_input, wide_destination, wide_destination + 1)};
	assert(wide_read_end == wide_destination + 1);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	reset(state, 3);
	state.maximum_progress = 1u;
	wide_read_end = ::fast_io::operations::read_some(wide_input, wide_destination, wide_destination + 1);
	assert(wide_read_end == wide_destination + 1);
	require_two_calls(state, 3, 4);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	reset(state, 3);
	status = ::fast_io::operations::scatter_read_some(wide_input, &wide_input_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_destination)));

	reset(state, 3);
	state.maximum_progress = 1u;
	status = ::fast_io::operations::scatter_read_some(wide_input, &wide_input_scatter, 1u);
	assert(status.position == 0u && status.position_in_scatter == 1u);
	require_two_calls(state, 3, 4);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));

	reset(state, 3);
	::fast_io::operations::scatter_read_all(wide_input, &wide_input_scatter, 1u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_destination)));

	constexpr ::std::size_t input_conversion_count{
		::fast_io::details::scatter_read_byte_conversion_stack_capacity + 1u};
	static_assert(input_conversion_count == output_conversion_count);
	::std::array<char16_t, input_conversion_count> wide_chunk_destination{};
	::std::array<::fast_io::basic_io_scatter_t<char16_t>, input_conversion_count> wide_input_chunk_scatters{};
	for (::std::size_t i{}; i != input_conversion_count; ++i)
	{
		wide_input_chunk_scatters[i] = {wide_chunk_destination.data() + i, 1u};
	}
	reset(state, 3);
	::fast_io::operations::scatter_read_all(
		wide_input, wide_input_chunk_scatters.data(), wide_input_chunk_scatters.size());
	require_linear_calls(state, input_conversion_count, 3,
						 static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));
	assert(state.current ==
		   3 + static_cast<::fast_io::intfpos_t>(input_conversion_count * sizeof(char16_t)));

	reset(state, 3);
	status = ::fast_io::operations::scatter_read_some_bytes(wide_input, &wide_input_byte_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_destination)));

	reset(state, 3);
	::fast_io::operations::scatter_read_all_bytes(wide_input, &wide_input_byte_scatter, 1u);
	require_single_call(state, 3);
	assert(state.current == 3 + static_cast<::fast_io::intfpos_t>(sizeof(wide_destination)));

	character_input char_input{__builtin_addressof(state)};
	char destination[3]{};
	::fast_io::basic_io_scatter_t<char> char_input_scatter{destination, sizeof(destination)};
	::fast_io::io_scatter_t char_input_byte_scatter{destination, sizeof(destination)};

	reset(state, 7);
	status = ::fast_io::operations::scatter_read_some(char_input, &char_input_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	::fast_io::operations::scatter_read_all(char_input, &char_input_scatter, 1u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	status = ::fast_io::operations::scatter_read_some_bytes(char_input, &char_input_byte_scatter, 1u);
	assert(status.position == 1u && status.position_in_scatter == 0u);
	require_single_call(state, 7);
	assert(state.current == 10);

	reset(state, 7);
	::fast_io::operations::scatter_read_all_bytes(char_input, &char_input_byte_scatter, 1u);
	require_single_call(state, 7);
	assert(state.current == 10);

	::std::array<char, input_conversion_count> character_chunk_destination{};
	::std::array<::fast_io::io_scatter_t, input_conversion_count> character_input_chunk_scatters{};
	for (::std::size_t i{}; i != input_conversion_count; ++i)
	{
		character_input_chunk_scatters[i] = {character_chunk_destination.data() + i, 1u};
	}
	reset(state, 7);
	status = ::fast_io::operations::scatter_read_some_bytes(
		char_input, character_input_chunk_scatters.data(), character_input_chunk_scatters.size());
	assert(status.position == ::fast_io::details::scatter_read_byte_conversion_stack_capacity);
	assert(status.position_in_scatter == 0u);
	require_linear_calls(state, ::fast_io::details::scatter_read_byte_conversion_stack_capacity, 7, 1);
	assert(state.current ==
		   7 + static_cast<::fast_io::intfpos_t>(::fast_io::details::scatter_read_byte_conversion_stack_capacity));

	reset(state, 7);
	::fast_io::operations::scatter_read_all_bytes(
		char_input, character_input_chunk_scatters.data(), character_input_chunk_scatters.size());
	require_linear_calls(state, input_conversion_count, 7, 1);
	assert(state.current == 7 + static_cast<::fast_io::intfpos_t>(input_conversion_count));
}
