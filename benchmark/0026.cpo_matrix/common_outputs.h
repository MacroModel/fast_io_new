#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common_sources.h"

namespace fast_io_cpo_matrix
{

inline constexpr ::std::size_t ring_capacity{64u * 1024u};

struct ring_output_state
{
	alignas(64)::std::array<char, ring_capacity> storage{};
	::std::size_t cursor{};
	::std::uint_least64_t total_written{};

	inline void reset() noexcept
	{
		cursor = 0u;
		total_written = 0u;
	}

	inline void append(char const *first, char const *last) noexcept
	{
		for (; first != last; ++first)
		{
			storage[cursor] = *first;
			if (++cursor == storage.size())
			{
				cursor = 0u;
			}
			++total_written;
		}
	}
};

struct ring_output_ref
{
	using output_char_type = char;
	ring_output_state *state{};
};

/*
This customization returns a trivial observer whose state is owned by main for
the complete benchmark.  The observer intentionally exposes only typed
write-all: it must not satisfy an output-buffer or scatter-output concept by
accident, because those capabilities have different dispatcher precedence and
will receive independent matrix cells.
*/
inline constexpr ring_output_ref output_stream_ref_define(
	ring_output_ref output) noexcept
{
	return output;
}

/*
Every successful call consumes the complete pointer range before returning.
The cyclic destination prevents unbounded allocation during calibration, and
`total_written` provides a monotonic publication point independent of ring
wrap.  Source bytes remain live for the duration of this synchronous call, so
the output never retains a borrowed source pointer.
*/
inline void write_all_overflow_define(
	ring_output_ref output, char const *first, char const *last) noexcept
{
	output.state->append(first, last);
}

[[nodiscard]] inline constexpr ring_output_ref make_ring_output(
	ring_output_state &state) noexcept
{
	return {__builtin_addressof(state)};
}

} // namespace fast_io_cpo_matrix
