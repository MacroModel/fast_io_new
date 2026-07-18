#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <fast_io_core.h>

namespace
{

enum class primitive_kind
{
	character_some,
	character_all,
	bytes_some,
	bytes_all,
	character_pwrite_some_leaf,
	character_pwrite_all,
	bytes_pwrite_all
};

struct primitive_call
{
	primitive_kind kind;
	std::size_t count;
	fast_io::intfpos_t offset;
	std::string data;
};

struct sink_state
{
	std::vector<primitive_call> calls;
	std::string output;
};

struct direct_scatter_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct character_pwrite_some_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct wide_byte_pwrite_state
{
	std::vector<fast_io::intfpos_t> offsets;
	std::vector<std::size_t> byte_sizes;
};

struct wide_byte_pwrite_sink
{
	using output_char_type = char16_t;
	wide_byte_pwrite_state *state;
};

inline constexpr wide_byte_pwrite_sink output_stream_ref_define(wide_byte_pwrite_sink sink) noexcept
{
	return sink;
}

inline void pwrite_all_bytes_overflow_define(wide_byte_pwrite_sink sink, std::byte const *first,
											  std::byte const *last, fast_io::intfpos_t offset)
{
	sink.state->offsets.push_back(offset);
	sink.state->byte_sizes.push_back(static_cast<std::size_t>(last - first));
}

inline constexpr character_pwrite_some_sink output_stream_ref_define(character_pwrite_some_sink sink) noexcept
{
	return sink;
}

inline char const *pwrite_some_overflow_define(character_pwrite_some_sink sink, char const *first,
											   char const *last, fast_io::intfpos_t offset)
{
	std::string data(first, last);
	sink.state->output.append(data);
	sink.state->calls.push_back(
		{primitive_kind::character_pwrite_some_leaf, 1u, offset, std::move(data)});
	return last;
}

inline constexpr direct_scatter_sink output_stream_ref_define(direct_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr std::size_t
scatter_write_maximum_count(fast_io::io_reserve_type_t<char, direct_scatter_sink>) noexcept
{
	return 4u;
}

inline std::string gather(fast_io::basic_io_scatter_t<char> const *scatters, std::size_t count)
{
	std::string result;
	for (std::size_t i{}; i != count; ++i)
	{
		result.append(scatters[i].base, scatters[i].len);
	}
	return result;
}

inline std::string gather(fast_io::io_scatter_t const *scatters, std::size_t count)
{
	std::string result;
	for (std::size_t i{}; i != count; ++i)
	{
		result.append(static_cast<char const *>(scatters[i].base), scatters[i].len);
	}
	return result;
}

template <typename scatter_type>
inline void record_call(direct_scatter_sink sink, primitive_kind kind, scatter_type const *scatters,
						std::size_t count, fast_io::intfpos_t offset = 0)
{
	auto data{gather(scatters, count)};
	sink.state->output.append(data);
	sink.state->calls.push_back({kind, count, offset, std::move(data)});
}

inline fast_io::io_scatter_status_t
scatter_write_some_overflow_define(direct_scatter_sink sink,
								   fast_io::basic_io_scatter_t<char> const *scatters,
								   std::size_t count)
{
	record_call(sink, primitive_kind::character_some, scatters, count);
	return {count, 0u};
}

inline void scatter_write_all_overflow_define(direct_scatter_sink sink,
											  fast_io::basic_io_scatter_t<char> const *scatters,
											  std::size_t count)
{
	record_call(sink, primitive_kind::character_all, scatters, count);
}

inline fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	direct_scatter_sink sink, fast_io::io_scatter_t const *scatters, std::size_t count)
{
	record_call(sink, primitive_kind::bytes_some, scatters, count);
	return {count, 0u};
}

inline void scatter_write_all_bytes_overflow_define(direct_scatter_sink sink,
													fast_io::io_scatter_t const *scatters, std::size_t count)
{
	record_call(sink, primitive_kind::bytes_all, scatters, count);
}

inline void scatter_pwrite_all_overflow_define(direct_scatter_sink sink,
											   fast_io::basic_io_scatter_t<char> const *scatters,
											   std::size_t count, fast_io::intfpos_t offset)
{
	record_call(sink, primitive_kind::character_pwrite_all, scatters, count, offset);
}

inline void scatter_pwrite_all_bytes_overflow_define(direct_scatter_sink sink,
													 fast_io::io_scatter_t const *scatters, std::size_t count,
													 fast_io::intfpos_t offset)
{
	record_call(sink, primitive_kind::bytes_pwrite_all, scatters, count, offset);
}

struct lock_state
{
	std::size_t lock_calls{};
	std::size_t unlock_calls{};
	bool locked{};
};

struct counting_mutex_ref
{
	lock_state *state;

	inline void lock() const noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

struct locked_scatter_sink
{
	using output_char_type = char;
	direct_scatter_sink unlocked;
	lock_state *lock;
};

inline constexpr locked_scatter_sink output_stream_ref_define(locked_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr counting_mutex_ref output_stream_mutex_ref_define(locked_scatter_sink sink) noexcept
{
	return {sink.lock};
}

inline constexpr direct_scatter_sink output_stream_unlocked_ref_define(locked_scatter_sink sink) noexcept
{
	return sink.unlocked;
}

inline void assert_some_call(sink_state const &state, primitive_kind kind)
{
	assert(state.calls.size() == 1u);
	assert(state.calls[0].kind == kind);
	assert(state.calls[0].count == 4u);
	assert(state.calls[0].offset == 0);
	assert(state.calls[0].data == "0123");
	assert(state.output == "0123");
}

inline void assert_split_payload(sink_state const &state, primitive_kind kind)
{
	assert(state.calls.size() == 3u);
	assert(state.calls[0].kind == kind);
	assert(state.calls[1].kind == kind);
	assert(state.calls[2].kind == kind);
	assert(state.calls[0].count == 4u);
	assert(state.calls[1].count == 4u);
	assert(state.calls[2].count == 2u);
	assert(state.calls[0].data == "0123");
	assert(state.calls[1].data == "4567");
	assert(state.calls[2].data == "89");
	assert(state.output == "0123456789");
}

inline void assert_split_calls(sink_state const &state, primitive_kind kind)
{
	assert_split_payload(state, kind);
	assert(state.calls[0].offset == 0);
	assert(state.calls[1].offset == 0);
	assert(state.calls[2].offset == 0);
}

inline void assert_positioned_split_calls(sink_state const &state, primitive_kind kind,
										  fast_io::intfpos_t first_offset)
{
	assert_split_payload(state, kind);
	assert(state.calls[0].offset == first_offset);
	assert(state.calls[1].offset == first_offset + 4);
	assert(state.calls[2].offset == first_offset + 8);
}

inline void assert_single_lock(lock_state const &state)
{
	assert(state.lock_calls == 1u);
	assert(state.unlock_calls == 1u);
	assert(!state.locked);
}

} // namespace

int main()
{
	static_assert(fast_io::scatter_write_maximum_count_stream<char, direct_scatter_sink>);
	static_assert(fast_io::details::scatter_write_maximum_count_or_unlimited<char, direct_scatter_sink>() == 4u);

	char const source[]{"0123456789"};
	std::array<fast_io::basic_io_scatter_t<char>, 10u> character_scatters{};
	std::array<fast_io::io_scatter_t, 10u> byte_scatters{};
	for (std::size_t i{}; i != character_scatters.size(); ++i)
	{
		character_scatters[i] = {source + i, 1u};
		byte_scatters[i] = {source + i, 1u};
	}

	sink_state state;
	direct_scatter_sink sink{&state};

	// Empty descriptor lists are completed above the synchronization/device layer. Besides avoiding non-portable
	// zero-iovec native calls, this makes the documented maximum-count proof's lower bound observable in tests.
	auto const empty_character_some{fast_io::operations::scatter_write_some(sink, nullptr, 0u)};
	assert(empty_character_some.position == 0u);
	assert(empty_character_some.position_in_scatter == 0u);
	fast_io::operations::scatter_write_all(sink, nullptr, 0u);
	auto const empty_byte_some{fast_io::operations::scatter_write_some_bytes(sink, nullptr, 0u)};
	assert(empty_byte_some.position == 0u);
	assert(empty_byte_some.position_in_scatter == 0u);
	fast_io::operations::scatter_write_all_bytes(sink, nullptr, 0u);
	fast_io::operations::scatter_pwrite_all(sink, nullptr, 0u, 19);
	fast_io::operations::scatter_pwrite_all_bytes(sink, nullptr, 0u, 19);
	assert(state.calls.empty());

	auto const character_some{
		fast_io::operations::scatter_write_some(sink, character_scatters.data(), character_scatters.size())};
	assert(character_some.position == 4u);
	assert(character_some.position_in_scatter == 0u);
	assert_some_call(state, primitive_kind::character_some);

	state = {};
	fast_io::operations::scatter_write_all(sink, character_scatters.data(), character_scatters.size());
	assert_split_calls(state, primitive_kind::character_all);

	state = {};
	auto const bytes_some{
		fast_io::operations::scatter_write_some_bytes(sink, byte_scatters.data(), byte_scatters.size())};
	assert(bytes_some.position == 4u);
	assert(bytes_some.position_in_scatter == 0u);
	assert_some_call(state, primitive_kind::bytes_some);

	state = {};
	fast_io::operations::scatter_write_all_bytes(sink, byte_scatters.data(), byte_scatters.size());
	assert_split_calls(state, primitive_kind::bytes_all);

	constexpr fast_io::intfpos_t positioned_offset{37};
	state = {};
	auto const character_some_via_all{
		fast_io::operations::scatter_pwrite_some(sink, character_scatters.data(), character_scatters.size(),
												 positioned_offset)};
	assert(character_some_via_all.position == character_scatters.size());
	assert(character_some_via_all.position_in_scatter == 0u);
	assert_positioned_split_calls(state, primitive_kind::character_pwrite_all, positioned_offset);

	state = {};
	fast_io::operations::scatter_pwrite_all(sink, character_scatters.data(), character_scatters.size(),
											positioned_offset);
	assert_positioned_split_calls(state, primitive_kind::character_pwrite_all, positioned_offset);

	state = {};
	auto const bytes_some_via_all{
		fast_io::operations::scatter_pwrite_some_bytes(sink, byte_scatters.data(), byte_scatters.size(),
												   positioned_offset)};
	assert(bytes_some_via_all.position == byte_scatters.size());
	assert(bytes_some_via_all.position_in_scatter == 0u);
	assert_positioned_split_calls(state, primitive_kind::bytes_pwrite_all, positioned_offset);

	state = {};
	fast_io::operations::scatter_pwrite_all_bytes(sink, byte_scatters.data(), byte_scatters.size(),
												  positioned_offset);
	assert_positioned_split_calls(state, primitive_kind::bytes_pwrite_all, positioned_offset);

	state = {};
	character_pwrite_some_sink some_sink{&state};
	std::array<fast_io::basic_io_scatter_t<char>, 3u> leaf_scatters{
		character_scatters[0], character_scatters[1], character_scatters[2]};
	auto const character_leaf_some{fast_io::operations::scatter_pwrite_some(
		some_sink, leaf_scatters.data(), leaf_scatters.size(), positioned_offset)};
	assert(character_leaf_some.position == leaf_scatters.size());
	assert(character_leaf_some.position_in_scatter == 0u);
	assert(state.output == "012");
	assert(state.calls.size() == leaf_scatters.size());
	for (std::size_t i{}; i != state.calls.size(); ++i)
	{
		assert(state.calls[i].kind == primitive_kind::character_pwrite_some_leaf);
		assert(state.calls[i].offset == positioned_offset + static_cast<fast_io::intfpos_t>(i));
		assert(state.calls[i].data == std::string(1u, source[i]));
	}

	// The typed positional API expresses both its origin and descriptor lengths in char16_t units, while this sink
	// exposes only a byte positional primitive. The strategy boundary must therefore scale the initial offset and every
	// later descriptor advance; scaling only the latter would place the first and subsequent writes in different
	// coordinate systems.
	char16_t const wide_source[]{u'a', u'b', u'c'};
	std::array<fast_io::basic_io_scatter_t<char16_t>, 2u> wide_scatters{
		fast_io::basic_io_scatter_t<char16_t>{wide_source, 2u},
		fast_io::basic_io_scatter_t<char16_t>{wide_source + 2u, 1u}};
	wide_byte_pwrite_state wide_state;
	wide_byte_pwrite_sink wide_sink{&wide_state};
	fast_io::operations::scatter_pwrite_all(wide_sink, wide_scatters.data(), wide_scatters.size(), 37);
	assert((wide_state.offsets == std::vector<fast_io::intfpos_t>{74, 78}));
	assert((wide_state.byte_sizes == std::vector<std::size_t>{4u, 2u}));

	lock_state lock;
	locked_scatter_sink locked_sink{sink, &lock};
	state = {};
	fast_io::operations::scatter_write_all(locked_sink, character_scatters.data(), character_scatters.size());
	assert_split_calls(state, primitive_kind::character_all);
	assert_single_lock(lock);

	state = {};
	lock = {};
	fast_io::operations::scatter_pwrite_all_bytes(locked_sink, byte_scatters.data(), byte_scatters.size(),
												  positioned_offset);
	assert_positioned_split_calls(state, primitive_kind::bytes_pwrite_all, positioned_offset);
	assert_single_lock(lock);
}
