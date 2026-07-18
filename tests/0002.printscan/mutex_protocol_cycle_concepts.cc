#include <fast_io_core.h>

namespace mutex_protocol_cycle_concepts
{

struct lock_proxy
{
	inline constexpr void lock() noexcept
	{}
	inline constexpr void unlock() noexcept
	{}
};

struct output_terminal
{
	using output_char_type = char;
};

struct output_chain_inner
{
	using output_char_type = char;
};

struct output_chain_outer
{
	using output_char_type = char;
};

inline constexpr lock_proxy output_stream_mutex_ref_define(output_chain_inner) noexcept
{
	return {};
}

inline constexpr output_terminal output_stream_unlocked_ref_define(output_chain_inner) noexcept
{
	return {};
}

inline constexpr lock_proxy output_stream_mutex_ref_define(output_chain_outer) noexcept
{
	return {};
}

inline constexpr output_chain_inner output_stream_unlocked_ref_define(output_chain_outer) noexcept
{
	return {};
}

// Both edges are locally well-formed and preserve the character domain. Only the composed visited-type proof can
// distinguish this graph from the terminating two-edge chain above.
struct output_cycle_a
{
	using output_char_type = char;
};

struct output_cycle_b
{
	using output_char_type = char;
};

inline constexpr lock_proxy output_stream_mutex_ref_define(output_cycle_a) noexcept
{
	return {};
}

inline constexpr output_cycle_b output_stream_unlocked_ref_define(output_cycle_a) noexcept
{
	return {};
}

inline constexpr lock_proxy output_stream_mutex_ref_define(output_cycle_b) noexcept
{
	return {};
}

inline constexpr output_cycle_a output_stream_unlocked_ref_define(output_cycle_b) noexcept
{
	return {};
}

struct directional_cycle_a
{
	using input_char_type = char;
	using output_char_type = char;
};

struct directional_cycle_b
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr lock_proxy input_stream_mutex_ref_define(directional_cycle_a) noexcept
{
	return {};
}

inline constexpr directional_cycle_b input_stream_unlocked_ref_define(directional_cycle_a) noexcept
{
	return {};
}

inline constexpr lock_proxy input_stream_mutex_ref_define(directional_cycle_b) noexcept
{
	return {};
}

inline constexpr directional_cycle_a input_stream_unlocked_ref_define(directional_cycle_b) noexcept
{
	return {};
}

inline constexpr lock_proxy output_stream_mutex_ref_define(directional_cycle_a) noexcept
{
	return {};
}

inline constexpr output_terminal output_stream_unlocked_ref_define(directional_cycle_a) noexcept
{
	return {};
}

// A joint protocol must follow io-specific CPOs. Directional fallbacks may also select these CPOs, but separate input
// and output markers must never be used as evidence for the joint lock invariant.
struct io_cycle_a
{
	using input_char_type = char;
	using output_char_type = char;
};

struct io_cycle_b
{
	using input_char_type = char;
	using output_char_type = char;
};

inline constexpr lock_proxy io_stream_mutex_ref_define(io_cycle_a) noexcept
{
	return {};
}

inline constexpr io_cycle_b io_stream_unlocked_ref_define(io_cycle_a) noexcept
{
	return {};
}

inline constexpr lock_proxy io_stream_mutex_ref_define(io_cycle_b) noexcept
{
	return {};
}

inline constexpr io_cycle_a io_stream_unlocked_ref_define(io_cycle_b) noexcept
{
	return {};
}

using ::fast_io::operations::decay::defines::has_complete_input_stream_mutex_protocol;
using ::fast_io::operations::decay::defines::has_complete_io_stream_mutex_protocol;
using ::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol;
using ::fast_io::operations::decay::defines::has_locally_complete_input_stream_mutex_protocol;
using ::fast_io::operations::decay::defines::has_locally_complete_io_stream_mutex_protocol;
using ::fast_io::operations::decay::defines::has_locally_complete_output_stream_mutex_protocol;

static_assert(has_complete_output_stream_mutex_protocol<output_chain_inner>);
static_assert(has_complete_output_stream_mutex_protocol<output_chain_outer>);

static_assert(has_locally_complete_output_stream_mutex_protocol<output_cycle_a>);
static_assert(has_locally_complete_output_stream_mutex_protocol<output_cycle_b>);
static_assert(!has_complete_output_stream_mutex_protocol<output_cycle_a>);
static_assert(!has_complete_output_stream_mutex_protocol<output_cycle_b>);

static_assert(has_locally_complete_input_stream_mutex_protocol<directional_cycle_a>);
static_assert(has_locally_complete_input_stream_mutex_protocol<directional_cycle_b>);
static_assert(!has_complete_input_stream_mutex_protocol<directional_cycle_a>);
static_assert(!has_complete_input_stream_mutex_protocol<directional_cycle_b>);
static_assert(has_complete_output_stream_mutex_protocol<directional_cycle_a>);
static_assert(!has_complete_io_stream_mutex_protocol<directional_cycle_a>);

static_assert(has_locally_complete_io_stream_mutex_protocol<io_cycle_a>);
static_assert(has_locally_complete_io_stream_mutex_protocol<io_cycle_b>);
static_assert(!has_complete_io_stream_mutex_protocol<io_cycle_a>);
static_assert(!has_complete_io_stream_mutex_protocol<io_cycle_b>);
static_assert(!has_complete_input_stream_mutex_protocol<io_cycle_a>);
static_assert(!has_complete_output_stream_mutex_protocol<io_cycle_a>);

} // namespace mutex_protocol_cycle_concepts

int main()
{}
