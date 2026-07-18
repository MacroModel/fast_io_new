#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include <fast_io_core.h>

namespace
{

inline constexpr ::std::size_t native_maximum{4u};
inline constexpr ::std::size_t some_byte_budget{3u};
inline constexpr ::fast_io::intfpos_t sequential_offset{-1};

inline constexpr bool narrow_scatter_sum_rejects_unrepresentable_elements() noexcept
{
	::fast_io::basic_io_scatter_t<char> oversized[]{{nullptr, 256u}};
	auto const oversized_result{
		::fast_io::details::find_scatter_total_size_overflow_impl<unsigned char>(oversized, 1u)};
	if (oversized_result.position != 0u || oversized_result.total_size != 0u)
	{
		return false;
	}

	::fast_io::basic_io_scatter_t<char> addition_overflow[]{{nullptr, 255u}, {nullptr, 1u}};
	auto const addition_result{
		::fast_io::details::find_scatter_total_size_overflow_impl<unsigned char>(addition_overflow, 2u)};
	if (addition_result.position != 1u || addition_result.total_size != 255u)
	{
		return false;
	}

	::std::array<::fast_io::basic_io_scatter_t<char>, 256u> long_zero_prefix{};
	long_zero_prefix.back().len = 1u;
	auto const count_result{::fast_io::details::find_scatter_total_size_overflow_impl<unsigned char>(
		long_zero_prefix.data(), long_zero_prefix.size())};
	return count_result.position == long_zero_prefix.size() && count_result.total_size == 1u;
}

static_assert(narrow_scatter_sum_rejects_unrepresentable_elements());

enum class call_kind
{
	character_some,
	byte_some,
	character_pread_some,
	byte_pread_some,
	character_all,
	byte_all,
	character_pread_all,
	byte_pread_all
};

struct native_call
{
	call_kind kind;
	::std::size_t count;
	::fast_io::intfpos_t offset;
};

struct source_state
{
	::std::string data;
	::std::size_t cursor{};
	::std::vector<native_call> calls;
};

struct scatter_some_source
{
	using input_char_type = char;
	source_state *state;
};

struct scatter_all_source
{
	using input_char_type = char;
	source_state *state;
};

inline constexpr scatter_some_source input_stream_ref_define(scatter_some_source source) noexcept
{
	return source;
}

inline constexpr scatter_all_source input_stream_ref_define(scatter_all_source source) noexcept
{
	return source;
}

inline constexpr ::std::size_t
scatter_read_maximum_count(::fast_io::io_reserve_type_t<char, scatter_some_source>) noexcept
{
	return native_maximum;
}

inline constexpr ::std::size_t
scatter_read_maximum_count(::fast_io::io_reserve_type_t<char, scatter_all_source>) noexcept
{
	return native_maximum;
}

inline ::std::size_t copy_character_prefix(source_state const &state,
	::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count, ::std::size_t source_offset,
	::std::size_t budget)
{
	assert(source_offset <= state.data.size());
	::std::size_t transferred{};
	for (::std::size_t i{}; i != count; ++i)
	{
		::std::size_t const position{source_offset + transferred};
		if (position == state.data.size() || transferred == budget)
		{
			break;
		}
		::std::size_t const amount{(::std::min)(
			scatters[i].len, (::std::min)(budget - transferred, state.data.size() - position))};
		::std::copy_n(state.data.data() + position, amount, const_cast<char *>(scatters[i].base));
		transferred += amount;
		if (amount != scatters[i].len)
		{
			break;
		}
	}
	return transferred;
}

inline ::std::size_t copy_byte_prefix(source_state const &state, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count, ::std::size_t source_offset, ::std::size_t budget)
{
	assert(source_offset <= state.data.size());
	::std::size_t transferred{};
	for (::std::size_t i{}; i != count; ++i)
	{
		::std::size_t const position{source_offset + transferred};
		if (position == state.data.size() || transferred == budget)
		{
			break;
		}
		::std::size_t const amount{(::std::min)(
			scatters[i].len, (::std::min)(budget - transferred, state.data.size() - position))};
		auto *destination{static_cast<::std::byte *>(const_cast<void *>(scatters[i].base))};
		for (::std::size_t j{}; j != amount; ++j)
		{
			destination[j] = static_cast<::std::byte>(static_cast<unsigned char>(state.data[position + j]));
		}
		transferred += amount;
		if (amount != scatters[i].len)
		{
			break;
		}
	}
	return transferred;
}

inline ::fast_io::io_scatter_status_t scatter_read_some_underflow_define(
	scatter_some_source source, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	source.state->calls.push_back({call_kind::character_some, count, sequential_offset});
	::std::size_t const transferred{
		copy_character_prefix(*source.state, scatters, count, source.state->cursor, some_byte_budget)};
	source.state->cursor += transferred;
	return ::fast_io::scatter_size_to_status(transferred, scatters, count);
}

inline ::fast_io::io_scatter_status_t scatter_read_some_bytes_underflow_define(
	scatter_some_source source, ::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	source.state->calls.push_back({call_kind::byte_some, count, sequential_offset});
	::std::size_t const transferred{
		copy_byte_prefix(*source.state, scatters, count, source.state->cursor, some_byte_budget)};
	source.state->cursor += transferred;
	return ::fast_io::scatter_size_to_status(transferred, scatters, count);
}

inline ::fast_io::io_scatter_status_t scatter_pread_some_underflow_define(
	scatter_some_source source, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count,
	::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	source.state->calls.push_back({call_kind::character_pread_some, count, offset});
	::std::size_t const transferred{copy_character_prefix(
		*source.state, scatters, count, static_cast<::std::size_t>(offset), some_byte_budget)};
	return ::fast_io::scatter_size_to_status(transferred, scatters, count);
}

inline ::fast_io::io_scatter_status_t scatter_pread_some_bytes_underflow_define(
	scatter_some_source source, ::fast_io::io_scatter_t const *scatters, ::std::size_t count,
	::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	source.state->calls.push_back({call_kind::byte_pread_some, count, offset});
	::std::size_t const transferred{
		copy_byte_prefix(*source.state, scatters, count, static_cast<::std::size_t>(offset), some_byte_budget)};
	return ::fast_io::scatter_size_to_status(transferred, scatters, count);
}

// The scalar some-CPOs are deliberately present only to complete a descriptor after the scatter some-CPO reports a
// partial position. Scatter dispatch still selects the scatter CPO first, so the test observes both status conversion
// and the generic all-operation's within-descriptor continuation.
inline char *read_some_underflow_define(scatter_some_source source, char *first, char *last)
{
	::fast_io::basic_io_scatter_t<char> scatter{first, static_cast<::std::size_t>(last - first)};
	::std::size_t const transferred{copy_character_prefix(
		*source.state, __builtin_addressof(scatter), 1u, source.state->cursor, scatter.len)};
	source.state->cursor += transferred;
	return first + transferred;
}

inline ::std::byte *read_some_bytes_underflow_define(
	scatter_some_source source, ::std::byte *first, ::std::byte *last)
{
	::fast_io::io_scatter_t scatter{first, static_cast<::std::size_t>(last - first)};
	::std::size_t const transferred{
		copy_byte_prefix(*source.state, __builtin_addressof(scatter), 1u, source.state->cursor, scatter.len)};
	source.state->cursor += transferred;
	return first + transferred;
}

inline char *pread_some_underflow_define(
	scatter_some_source source, char *first, char *last, ::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	::fast_io::basic_io_scatter_t<char> scatter{first, static_cast<::std::size_t>(last - first)};
	::std::size_t const transferred{copy_character_prefix(
		*source.state, __builtin_addressof(scatter), 1u, static_cast<::std::size_t>(offset), scatter.len)};
	return first + transferred;
}

inline ::std::byte *pread_some_bytes_underflow_define(
	scatter_some_source source, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	::fast_io::io_scatter_t scatter{first, static_cast<::std::size_t>(last - first)};
	::std::size_t const transferred{copy_byte_prefix(
		*source.state, __builtin_addressof(scatter), 1u, static_cast<::std::size_t>(offset), scatter.len)};
	return first + transferred;
}

inline void scatter_read_all_underflow_define(
	scatter_all_source source, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	source.state->calls.push_back({call_kind::character_all, count, sequential_offset});
	::std::size_t const transferred{copy_character_prefix(
		*source.state, scatters, count, source.state->cursor, ::std::numeric_limits<::std::size_t>::max())};
	::std::size_t total{};
	for (::std::size_t i{}; i != count; ++i)
	{
		total += scatters[i].len;
	}
	assert(transferred == total);
	source.state->cursor += transferred;
}

inline void scatter_read_all_bytes_underflow_define(
	scatter_all_source source, ::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	source.state->calls.push_back({call_kind::byte_all, count, sequential_offset});
	::std::size_t const transferred{copy_byte_prefix(
		*source.state, scatters, count, source.state->cursor, ::std::numeric_limits<::std::size_t>::max())};
	::std::size_t total{};
	for (::std::size_t i{}; i != count; ++i)
	{
		total += scatters[i].len;
	}
	assert(transferred == total);
	source.state->cursor += transferred;
}

inline void scatter_pread_all_underflow_define(
	scatter_all_source source, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count,
	::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	source.state->calls.push_back({call_kind::character_pread_all, count, offset});
	::std::size_t const transferred{copy_character_prefix(*source.state, scatters, count,
		static_cast<::std::size_t>(offset), ::std::numeric_limits<::std::size_t>::max())};
	::std::size_t total{};
	for (::std::size_t i{}; i != count; ++i)
	{
		total += scatters[i].len;
	}
	assert(transferred == total);
}

inline void scatter_pread_all_bytes_underflow_define(
	scatter_all_source source, ::fast_io::io_scatter_t const *scatters, ::std::size_t count,
	::fast_io::intfpos_t offset)
{
	assert(offset >= 0);
	source.state->calls.push_back({call_kind::byte_pread_all, count, offset});
	::std::size_t const transferred{copy_byte_prefix(*source.state, scatters, count,
		static_cast<::std::size_t>(offset), ::std::numeric_limits<::std::size_t>::max())};
	::std::size_t total{};
	for (::std::size_t i{}; i != count; ++i)
	{
		total += scatters[i].len;
	}
	assert(transferred == total);
}

inline ::std::array<::fast_io::basic_io_scatter_t<char>, 9u>
make_character_scatters(::std::array<char, 8u> &destination)
{
	// The first legal native batch is all empty. The second batch contains an empty descriptor between nonempty ones,
	// and a third suffix remains. This shape distinguishes descriptor progress from byte progress at both boundaries.
	return {{{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 2u},
		{destination.data() + 2u, 0u},
		{destination.data() + 2u, 3u},
		{destination.data() + 5u, 1u},
		{destination.data() + 6u, 2u}}};
}

inline ::std::array<::fast_io::io_scatter_t, 9u>
make_byte_scatters(::std::array<::std::byte, 8u> &destination)
{
	return {{{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 0u},
		{destination.data(), 2u},
		{destination.data() + 2u, 0u},
		{destination.data() + 2u, 3u},
		{destination.data() + 5u, 1u},
		{destination.data() + 6u, 2u}}};
}

inline void expect_character_payload(::std::array<char, 8u> const &destination)
{
	assert(::std::string(destination.data(), destination.size()) == "abcdefgh");
}

inline void expect_byte_payload(::std::array<::std::byte, 8u> const &destination)
{
	for (::std::size_t i{}; i != destination.size(); ++i)
	{
		assert(::std::to_integer<unsigned char>(destination[i]) == static_cast<unsigned char>("abcdefgh"[i]));
	}
}

inline void expect_calls(source_state const &state, call_kind kind,
	::std::initializer_list<::std::size_t> counts, ::std::initializer_list<::fast_io::intfpos_t> offsets)
{
	assert(counts.size() == offsets.size());
	assert(state.calls.size() == counts.size());
	auto count_iterator{counts.begin()};
	auto offset_iterator{offsets.begin()};
	for (auto const &call : state.calls)
	{
		assert(call.kind == kind);
		assert(call.count == *count_iterator++);
		assert(call.offset == *offset_iterator++);
	}
}

inline void test_scatter_some_continuation()
{
	source_state state{"abcdefgh", {}, {}};
	scatter_some_source source{__builtin_addressof(state)};

	::std::array<char, 8u> characters{};
	auto character_scatters{make_character_scatters(characters)};
	auto const character_status{::fast_io::operations::scatter_read_some(
		source, character_scatters.data(), character_scatters.size())};
	assert(character_status.position == native_maximum);
	assert(character_status.position_in_scatter == 0u);
	assert(state.cursor == 0u);
	expect_calls(state, call_kind::character_some, {4u}, {sequential_offset});

	state.cursor = 0u;
	state.calls.clear();
	characters.fill(char{});
	::fast_io::operations::scatter_read_all(source, character_scatters.data(), character_scatters.size());
	expect_character_payload(characters);
	// The second native result is partial ({2,1}); the scalar continuation finishes that descriptor, after which the
	// final two descriptors are submitted as a third scatter call. The initial {4,0} is descriptor progress, not EOF.
	expect_calls(state, call_kind::character_some, {4u, 4u, 2u},
		{sequential_offset, sequential_offset, sequential_offset});

	state.cursor = 0u;
	state.calls.clear();
	::std::array<::std::byte, 8u> bytes{};
	auto byte_scatters{make_byte_scatters(bytes)};
	auto const byte_status{
		::fast_io::operations::scatter_read_some_bytes(source, byte_scatters.data(), byte_scatters.size())};
	assert(byte_status.position == native_maximum);
	assert(byte_status.position_in_scatter == 0u);
	assert(state.cursor == 0u);
	expect_calls(state, call_kind::byte_some, {4u}, {sequential_offset});

	state.cursor = 0u;
	state.calls.clear();
	bytes.fill(::std::byte{});
	::fast_io::operations::scatter_read_all_bytes(source, byte_scatters.data(), byte_scatters.size());
	expect_byte_payload(bytes);
	expect_calls(state, call_kind::byte_some, {4u, 4u, 2u},
		{sequential_offset, sequential_offset, sequential_offset});
}

inline void test_scatter_pread_some_continuation()
{
	constexpr ::fast_io::intfpos_t initial_offset{5};
	source_state state{"01234abcdefgh", {}, {}};
	scatter_some_source source{__builtin_addressof(state)};

	::std::array<char, 8u> characters{};
	auto character_scatters{make_character_scatters(characters)};
	auto const character_status{::fast_io::operations::scatter_pread_some(
		source, character_scatters.data(), character_scatters.size(), initial_offset)};
	assert(character_status.position == native_maximum);
	assert(character_status.position_in_scatter == 0u);
	expect_calls(state, call_kind::character_pread_some, {4u}, {initial_offset});

	state.calls.clear();
	characters.fill(char{});
	::fast_io::operations::scatter_pread_all(
		source, character_scatters.data(), character_scatters.size(), initial_offset);
	expect_character_payload(characters);
	// Four empty descriptors preserve the initial origin. The partial second call plus its scalar remainder consumes
	// five characters, so the final native prefix starts at 10 rather than at a descriptor-count-derived offset.
	expect_calls(state, call_kind::character_pread_some, {4u, 4u, 2u}, {5, 5, 10});

	state.calls.clear();
	::std::array<::std::byte, 8u> bytes{};
	auto byte_scatters{make_byte_scatters(bytes)};
	auto const byte_status{::fast_io::operations::scatter_pread_some_bytes(
		source, byte_scatters.data(), byte_scatters.size(), initial_offset)};
	assert(byte_status.position == native_maximum);
	assert(byte_status.position_in_scatter == 0u);
	expect_calls(state, call_kind::byte_pread_some, {4u}, {initial_offset});

	state.calls.clear();
	bytes.fill(::std::byte{});
	::fast_io::operations::scatter_pread_all_bytes(
		source, byte_scatters.data(), byte_scatters.size(), initial_offset);
	expect_byte_payload(bytes);
	expect_calls(state, call_kind::byte_pread_some, {4u, 4u, 2u}, {5, 5, 10});
}

inline void test_direct_scatter_all_batching()
{
	source_state state{"abcdefgh", {}, {}};
	scatter_all_source source{__builtin_addressof(state)};

	::std::array<char, 8u> characters{};
	auto character_scatters{make_character_scatters(characters)};
	::fast_io::operations::scatter_read_all(source, character_scatters.data(), character_scatters.size());
	expect_character_payload(characters);
	expect_calls(state, call_kind::character_all, {4u, 4u, 1u},
		{sequential_offset, sequential_offset, sequential_offset});

	state.cursor = 0u;
	state.calls.clear();
	::std::array<::std::byte, 8u> bytes{};
	auto byte_scatters{make_byte_scatters(bytes)};
	::fast_io::operations::scatter_read_all_bytes(source, byte_scatters.data(), byte_scatters.size());
	expect_byte_payload(bytes);
	expect_calls(state, call_kind::byte_all, {4u, 4u, 1u},
		{sequential_offset, sequential_offset, sequential_offset});
}

inline void test_direct_scatter_pread_all_batching()
{
	constexpr ::fast_io::intfpos_t initial_offset{5};
	source_state state{"01234abcdefgh", {}, {}};
	scatter_all_source source{__builtin_addressof(state)};

	::std::array<char, 8u> characters{};
	auto character_scatters{make_character_scatters(characters)};
	::fast_io::operations::scatter_pread_all(
		source, character_scatters.data(), character_scatters.size(), initial_offset);
	expect_character_payload(characters);
	// The zero-only first batch leaves off at 5. The second batch contains six characters, placing the final batch at
	// 11. This proves that positional batching advances by payload length, never by descriptor count.
	expect_calls(state, call_kind::character_pread_all, {4u, 4u, 1u}, {5, 5, 11});

	state.calls.clear();
	::std::array<::std::byte, 8u> bytes{};
	auto byte_scatters{make_byte_scatters(bytes)};
	::fast_io::operations::scatter_pread_all_bytes(
		source, byte_scatters.data(), byte_scatters.size(), initial_offset);
	expect_byte_payload(bytes);
	expect_calls(state, call_kind::byte_pread_all, {4u, 4u, 1u}, {5, 5, 11});
}

} // namespace

int main()
{
	static_assert(::fast_io::scatter_read_maximum_count_stream<char, scatter_some_source>);
	static_assert(::fast_io::scatter_read_maximum_count_stream<char, scatter_all_source>);
	static_assert(
		::fast_io::details::scatter_read_maximum_count_or_unlimited<char, scatter_some_source>() == native_maximum);

	test_scatter_some_continuation();
	test_scatter_pread_some_continuation();
	test_direct_scatter_all_batching();
	test_direct_scatter_pread_all_batching();
}
