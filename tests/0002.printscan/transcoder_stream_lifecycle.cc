#include <array>
#include <cstddef>
#include <cstdlib>

#include <fast_io_core.h>

namespace transcoder_stream_lifecycle_test
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

inline constexpr ::std::array<::std::size_t, 6u> boundary_sizes{
	0u, 1u, 2u, 3u, 63u, 64u};
inline constexpr ::std::array<::std::size_t, 5u> read_chunk_sizes{
	1u, 2u, 3u, 63u, 64u};
inline constexpr ::std::size_t maximum_logical_size{64u};
inline constexpr ::std::size_t maximum_encoded_size{
	maximum_logical_size * 2u};
inline constexpr char ordinary_characters[]{
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};

struct eol_record
{
	::std::array<char, maximum_logical_size> logical{};
	::std::array<char, maximum_encoded_size> encoded{};
	::std::size_t logical_size{};
	::std::size_t encoded_size{};
};

[[nodiscard]] inline eol_record make_record(::std::size_t logical_size) noexcept
{
	require(logical_size <= maximum_logical_size);
	eol_record record{};
	record.logical_size = logical_size;
	for (::std::size_t index{}; index != logical_size; ++index)
	{
		record.logical[index] = ordinary_characters[index % (sizeof(ordinary_characters) - 1u)];
	}
	if (logical_size != 0u)
	{
		// A leading LF exercises expansion even for the one-unit message.
		record.logical[0] = '\n';
	}
	if (logical_size > 1u)
	{
		// This CR is deliberately logical data. For size two it is also the
		// terminal unmatched CR whose preservation depends on engine finish.
		record.logical[logical_size / 2u] = '\r';
	}
	if (logical_size > 2u)
	{
		// A terminal LF places a complete CRLF expansion at the message boundary.
		record.logical[logical_size - 1u] = '\n';
	}

	for (::std::size_t index{}; index != logical_size; ++index)
	{
		char const character{record.logical[index]};
		if (character == '\n')
		{
			record.encoded[record.encoded_size++] = '\r';
		}
		record.encoded[record.encoded_size++] = character;
	}
	return record;
}

[[nodiscard]] inline ::std::size_t encoded_prefix_size(
	eol_record const &record, ::std::size_t logical_prefix) noexcept
{
	require(logical_prefix <= record.logical_size);
	::std::size_t size{};
	for (::std::size_t index{}; index != logical_prefix; ++index)
	{
		size += record.logical[index] == '\n' ? 2u : 1u;
	}
	return size;
}

inline void require_equal(
	char const *actual, char const *expected, ::std::size_t size) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		require(actual[index] == expected[index]);
	}
}

inline constexpr ::std::array<::std::size_t, 15u> fragmentation_extents{
	0u, 1u, 2u, 3u, 7u, 8u, 15u, 16u,
	17u, 31u, 32u, 33u, 63u, 64u, 65u};
inline constexpr ::std::array<::std::size_t, 6u> isolated_lane_positions{
	0u, 31u, 32u, 63u, 64u, 65u};
inline constexpr ::std::array<::std::size_t, 4u>
	small_code_unit_fragmentation_extents{0u, 1u, 16u, 17u};
inline constexpr ::std::size_t fragmentation_input_capacity{192u};
inline constexpr ::std::size_t fragmentation_output_capacity{512u};
inline constexpr ::std::size_t fragmentation_window_capacity{65u};

template <typename char_type, ::std::size_t capacity>
struct fixed_units
{
	::std::array<char_type, capacity> storage{};
	::std::size_t size{};

	inline void push(char_type value) noexcept
	{
		require(size != capacity);
		storage[size++] = value;
	}
};

template <typename char_type>
[[nodiscard]] inline constexpr char_type ascii_unit(char value) noexcept
{
	return static_cast<char_type>(static_cast<unsigned char>(value));
}

template <typename char_type, ::std::size_t literal_size>
[[nodiscard]] inline fixed_units<char_type, fragmentation_input_capacity>
make_fragmentation_literal(char const (&literal)[literal_size]) noexcept
{
	fixed_units<char_type, fragmentation_input_capacity> result{};
	for (::std::size_t index{}; index + 1u != literal_size; ++index)
	{
		result.push(ascii_unit<char_type>(literal[index]));
	}
	return result;
}

template <typename char_type>
[[nodiscard]] inline fixed_units<char_type, fragmentation_input_capacity>
make_sparse_fragmentation_message() noexcept
{
	fixed_units<char_type, fragmentation_input_capacity> result{};
	for (::std::size_t index{}; index != 131u; ++index)
	{
		result.push(ascii_unit<char_type>(
			ordinary_characters[index % (sizeof(ordinary_characters) - 1u)]));
	}
	result.storage[0u] = ascii_unit<char_type>('\n');
	result.storage[17u] = ascii_unit<char_type>('\n');
	result.storage[64u] = ascii_unit<char_type>('\n');
	result.storage[130u] = ascii_unit<char_type>('\n');
	result.storage[3u] = ascii_unit<char_type>('\r');
	result.storage[62u] = ascii_unit<char_type>('\r');
	result.storage[95u] = ascii_unit<char_type>('\r');
	result.storage[96u] = ascii_unit<char_type>('\n');
	return result;
}

template <typename char_type>
[[nodiscard]] inline fixed_units<char_type, fragmentation_input_capacity>
make_lf_lane_boundary_message() noexcept
{
	fixed_units<char_type, fragmentation_input_capacity> result{};
	for (::std::size_t index{}; index != 132u; ++index)
	{
		result.push(ascii_unit<char_type>(
			ordinary_characters[index % (sizeof(ordinary_characters) - 1u)]));
	}
	// These exact positions exercise both halves and both edges of a 64-byte
	// block, followed by the first two lanes of the next block.
	constexpr ::std::array<::std::size_t, 8u> delimiter_positions{
		0u, 31u, 32u, 63u, 64u, 65u, 127u, 128u};
	for (auto const index : delimiter_positions)
	{
		result.storage[index] = ascii_unit<char_type>('\n');
	}
	return result;
}

template <typename char_type>
[[nodiscard]] inline fixed_units<char_type, fragmentation_input_capacity>
make_cr_lane_boundary_message() noexcept
{
	fixed_units<char_type, fragmentation_input_capacity> result{};
	for (::std::size_t index{}; index != 133u; ++index)
	{
		result.push(ascii_unit<char_type>(
			ordinary_characters[index % (sizeof(ordinary_characters) - 1u)]));
	}
	constexpr ::std::array<::std::size_t, 9u> delimiter_positions{
		0u, 31u, 32u, 63u, 64u, 65u, 127u, 128u, 132u};
	for (auto const index : delimiter_positions)
	{
		result.storage[index] = ascii_unit<char_type>('\r');
	}
	// Selected lookahead units form pairs across lanes 0/1, 32/33, 65/66,
	// and 128/129. Adjacent and terminal CR units independently exercise
	// literal-CR publication and pending state at fragmented source tails.
	constexpr ::std::array<::std::size_t, 4u> lookahead_positions{
		1u, 33u, 66u, 129u};
	for (auto const index : lookahead_positions)
	{
		result.storage[index] = ascii_unit<char_type>('\n');
	}
	return result;
}

template <typename char_type>
[[nodiscard]] inline fixed_units<char_type, fragmentation_input_capacity>
make_source_boundary_message(::std::size_t source_extent) noexcept
{
	// For the zero-extent policy, the progress window is one unit. Otherwise the
	// CR is the last unit of the first nominal source fragment and LF is the first
	// unit of the second. Thus every matrix row contains a CRLF pair whose state
	// transition necessarily crosses two process calls.
	::std::size_t const progress_extent{source_extent == 0u ? 1u : source_extent};
	if (progress_extent > fragmentation_input_capacity - 4u) [[unlikely]]
	{
		::std::abort();
	}
	// Range proof: progress_extent is at least one, and the guard above reserves
	// a four-unit trailer. Therefore progress_extent-1, progress_extent,
	// progress_extent+2, and message_size-1 are all valid storage indices.
	::std::size_t const message_size{progress_extent + 4u};
	fixed_units<char_type, fragmentation_input_capacity> result{};
	for (::std::size_t index{}; index != message_size; ++index)
	{
		result.push(ascii_unit<char_type>(
			ordinary_characters[index % (sizeof(ordinary_characters) - 1u)]));
	}
	result.storage[progress_extent - 1u] = ascii_unit<char_type>('\r');
	result.storage[progress_extent] = ascii_unit<char_type>('\n');
	result.storage[progress_extent + 2u] = ascii_unit<char_type>('\r');
	result.storage[message_size - 1u] = ascii_unit<char_type>('\n');
	if (progress_extent != 1u)
	{
		result.storage[0u] = ascii_unit<char_type>('\n');
	}
	return result;
}

template <typename char_type, ::fast_io::transcoders::eol_scheme from_scheme,
		  ::fast_io::transcoders::eol_scheme to_scheme,
		  ::std::size_t input_capacity>
[[nodiscard]] inline fixed_units<char_type, fragmentation_output_capacity>
reference_eol_transform(
	fixed_units<char_type, input_capacity> const &input) noexcept
{
	fixed_units<char_type, fragmentation_output_capacity> output{};
	for (::std::size_t index{}; index != input.size; ++index)
	{
		char_type const value{input.storage[index]};
		if constexpr (from_scheme == ::fast_io::transcoders::eol_scheme::lf &&
					  to_scheme == ::fast_io::transcoders::eol_scheme::crlf)
		{
			if (value == ascii_unit<char_type>('\n'))
			{
				output.push(ascii_unit<char_type>('\r'));
			}
			output.push(value);
		}
		else if constexpr (
			from_scheme == ::fast_io::transcoders::eol_scheme::crlf &&
			to_scheme == ::fast_io::transcoders::eol_scheme::lf)
		{
			if (value == ascii_unit<char_type>('\r') &&
				index + 1u != input.size &&
				input.storage[index + 1u] == ascii_unit<char_type>('\n'))
			{
				output.push(ascii_unit<char_type>('\n'));
				++index;
			}
			else
			{
				output.push(value);
			}
		}
		else
		{
			static_assert(
				from_scheme == ::fast_io::transcoders::eol_scheme::lf &&
				to_scheme == ::fast_io::transcoders::eol_scheme::cr);
			output.push(value == ascii_unit<char_type>('\n')
							? ascii_unit<char_type>('\r')
							: value);
		}
	}
	return output;
}

template <typename char_type, ::std::size_t left_capacity,
		  ::std::size_t right_capacity>
inline void append_units(fixed_units<char_type, left_capacity> &left,
						 fixed_units<char_type, right_capacity> const &right) noexcept
{
	for (::std::size_t index{}; index != right.size; ++index)
	{
		left.push(right.storage[index]);
	}
}

template <typename char_type, ::std::size_t actual_capacity,
		  ::std::size_t expected_capacity>
inline void require_prefix(
	fixed_units<char_type, actual_capacity> const &actual,
	fixed_units<char_type, expected_capacity> const &expected) noexcept
{
	require(actual.size <= expected.size);
	for (::std::size_t index{}; index != actual.size; ++index)
	{
		require(actual.storage[index] == expected.storage[index]);
	}
}

template <::fast_io::transcoders::eol_scheme from_scheme,
		  ::fast_io::transcoders::eol_scheme to_scheme>
inline void test_isolated_relative_lane_case(
	::std::size_t delimiter_position) noexcept
{
	constexpr ::std::size_t message_size{130u};
	require(delimiter_position < message_size - 1u);
	fixed_units<char, fragmentation_input_capacity> input{};
	for (::std::size_t index{}; index != message_size; ++index)
	{
		input.push(ordinary_characters[index % (sizeof(ordinary_characters) - 1u)]);
	}
	if constexpr (from_scheme == ::fast_io::transcoders::eol_scheme::lf)
	{
		input.storage[delimiter_position] = '\n';
	}
	else
	{
		static_assert(
			from_scheme == ::fast_io::transcoders::eol_scheme::crlf &&
			to_scheme == ::fast_io::transcoders::eol_scheme::lf);
		input.storage[delimiter_position] = '\r';
		input.storage[delimiter_position + 1u] = '\n';
	}
	auto const expected{
		reference_eol_transform<char, from_scheme, to_scheme>(input)};

	char constexpr guard_value{'~'};
	::std::array<char, message_size + 2u> source_window{};
	::std::array<char, fragmentation_output_capacity + 2u> output_window{};
	source_window.fill(guard_value);
	output_window.fill(guard_value);
	for (::std::size_t index{}; index != message_size; ++index)
	{
		source_window[index + 1u] = input.storage[index];
	}
	char const *const from_first{source_window.data() + 1u};
	char const *const from_last{from_first + message_size};
	char *const to_first{output_window.data() + 1u};
	char *const to_last{to_first + fragmentation_output_capacity};
	using engine_type = ::fast_io::transcoders::basic_eol<
		char, from_scheme, to_scheme>;
	engine_type engine{};
	auto const processed{::fast_io::operations::transcode_process(
		engine, from_first, from_last, to_first, to_last)};

	// There is exactly one copy-prefix delimiter in each 130-byte source. With
	// no earlier delimiter to rebase the cursor, positions 0, 31, 32, and 63 are
	// those exact lanes of block zero; after one all-plain 64-byte commit,
	// positions 64 and 65 are lanes zero and one of block one. In the CRLF case,
	// LF is lookahead rather than a copy-prefix delimiter, and position 63 proves
	// that lookahead across the block boundary preserves the same CR lane.
	require(processed.from_next == from_last &&
			processed.to_next == to_first + expected.size &&
			processed.status == ::fast_io::transcode_step_status::need_input);
	require(source_window.front() == guard_value &&
			source_window.back() == guard_value &&
			output_window.front() == guard_value &&
			output_window.back() == guard_value);
	require_equal(to_first, expected.storage.data(), expected.size);

	for (::std::size_t repetition{}; repetition != 2u; ++repetition)
	{
		::std::array<char, 3u> drain_window{};
		drain_window.fill(guard_value);
		char *const drain_first{drain_window.data() + 1u};
		auto const finished{::fast_io::operations::transcode_finish(
			engine, drain_first, drain_first + 1u)};
		require(finished.to_next == drain_first &&
				finished.status == ::fast_io::transcode_drain_status::complete &&
				drain_window.front() == guard_value &&
				drain_window.back() == guard_value);
	}
}

inline void test_isolated_relative_lanes() noexcept
{
	for (auto const delimiter_position : isolated_lane_positions)
	{
		test_isolated_relative_lane_case<
			::fast_io::transcoders::eol_scheme::lf,
			::fast_io::transcoders::eol_scheme::crlf>(delimiter_position);
		test_isolated_relative_lane_case<
			::fast_io::transcoders::eol_scheme::lf,
			::fast_io::transcoders::eol_scheme::cr>(delimiter_position);
		test_isolated_relative_lane_case<
			::fast_io::transcoders::eol_scheme::crlf,
			::fast_io::transcoders::eol_scheme::lf>(delimiter_position);
	}
}

enum class identity_alias
{
	same,
	destination_before,
	destination_after
};

template <typename char_type, identity_alias alias>
inline void test_identity_alias_case() noexcept
{
	constexpr ::std::size_t copy_size{128u};
	char_type constexpr guard_value{ascii_unit<char_type>('~')};
	::std::array<char_type, copy_size + 3u> storage{};
	storage.fill(guard_value);
	constexpr ::std::size_t source_offset{
		alias == identity_alias::destination_before ? 2u : 1u};
	constexpr ::std::size_t destination_offset{
		alias == identity_alias::destination_after ? 2u : 1u};
	char_type *const source{storage.data() + source_offset};
	char_type *const destination{storage.data() + destination_offset};
	::std::array<char_type, copy_size> snapshot{};
	for (::std::size_t index{}; index != copy_size; ++index)
	{
		char_type const value{ascii_unit<char_type>(ordinary_characters[index % (sizeof(ordinary_characters) - 1u)])};
		source[index] = value;
		snapshot[index] = value;
	}

	using engine_type = ::fast_io::transcoders::basic_eol<
		char_type, ::fast_io::transcoders::eol_scheme::lf,
		::fast_io::transcoders::eol_scheme::lf>;
	engine_type engine{};
	auto const processed{::fast_io::operations::transcode_process(
		engine, source, source + copy_size,
		destination, destination + copy_size)};

	// Identity conversion promises the same logical snapshot as memmove, not
	// memcpy: exact aliasing only advances cursors, and either one-unit overlap
	// direction must reproduce all original source units before any are clobbered.
	require(processed.from_next == source + copy_size &&
			processed.to_next == destination + copy_size &&
			processed.status == ::fast_io::transcode_step_status::need_input &&
			storage.front() == guard_value && storage.back() == guard_value);
	for (::std::size_t index{}; index != copy_size; ++index)
	{
		require(destination[index] == snapshot[index]);
	}

	for (::std::size_t repetition{}; repetition != 2u; ++repetition)
	{
		::std::array<char_type, 3u> drain{};
		drain.fill(guard_value);
		char_type *const drain_first{drain.data() + 1u};
		auto const finished{::fast_io::operations::transcode_finish(
			engine, drain_first, drain_first + 1u)};
		require(finished.to_next == drain_first &&
				finished.status == ::fast_io::transcode_drain_status::complete &&
				drain.front() == guard_value && drain.back() == guard_value);
	}
}

template <typename char_type>
inline void test_identity_aliases_for_type() noexcept
{
	test_identity_alias_case<char_type, identity_alias::same>();
	test_identity_alias_case<
		char_type, identity_alias::destination_before>();
	test_identity_alias_case<
		char_type, identity_alias::destination_after>();
}

inline void test_identity_aliases() noexcept
{
	test_identity_aliases_for_type<char>();
	test_identity_aliases_for_type<char16_t>();
}

[[nodiscard]] inline constexpr bool
test_identity_destination_after_constexpr() noexcept
{
	::std::array<char, 9u> storage{
		'~', 'A', 'B', 'C', 'D', 'E', 'F', '?', '~'};
	using engine_type = ::fast_io::transcoders::basic_eol<
		char, ::fast_io::transcoders::eol_scheme::lf,
		::fast_io::transcoders::eol_scheme::lf>;
	engine_type engine{};
	char *const source{storage.data() + 1u};
	char *const destination{storage.data() + 2u};
	auto const processed{engine.process(
		source, source + 6u, destination, destination + 6u)};
	auto const finished{engine.finish(destination + 6u, destination + 6u)};
	return processed.from_next == source + 6u &&
		   processed.to_next == destination + 6u &&
		   processed.status == ::fast_io::transcode_step_status::need_input &&
		   finished.to_next == destination + 6u &&
		   finished.status == ::fast_io::transcode_drain_status::complete &&
		   storage[0u] == '~' && storage[1u] == 'A' && storage[2u] == 'A' &&
		   storage[3u] == 'B' && storage[4u] == 'C' && storage[5u] == 'D' &&
		   storage[6u] == 'E' && storage[7u] == 'F' && storage[8u] == '~';
}

static_assert(test_identity_destination_after_constexpr());

template <typename engine_type, typename char_type, ::std::size_t input_capacity>
[[nodiscard]] inline ::std::size_t process_fragment(
	engine_type &engine, fixed_units<char_type, input_capacity> const &input,
	::std::size_t from_offset, ::std::size_t from_limit,
	::std::size_t destination_capacity,
	fixed_units<char_type, fragmentation_output_capacity> &output) noexcept
{
	require(from_offset <= from_limit && from_limit <= input.size);
	require(destination_capacity <= fragmentation_window_capacity);
	char_type const guard_value{ascii_unit<char_type>('~')};
	::std::array<char_type, fragmentation_window_capacity + 2u> window{};
	for (auto &value : window)
	{
		value = guard_value;
	}
	char_type const *const from_first{input.storage.data() + from_offset};
	char_type const *const from_last{input.storage.data() + from_limit};
	char_type *const to_first{window.data() + 1u};
	char_type *const to_last{to_first + destination_capacity};

	auto const result{::fast_io::operations::transcode_process(
		engine, from_first, from_last, to_first, to_last)};
	require(result.from_next >= from_first && result.from_next <= from_last);
	require(result.to_next >= to_first && result.to_next <= to_last);
	require(window.front() == guard_value &&
			window[destination_capacity + 1u] == guard_value);

	::std::size_t const consumed{
		static_cast<::std::size_t>(result.from_next - from_first)};
	::std::size_t const produced{
		static_cast<::std::size_t>(result.to_next - to_first)};
	for (::std::size_t index{}; index != produced; ++index)
	{
		output.push(to_first[index]);
	}

	// Formal process postcondition: need_input holds exactly when the supplied
	// source fragment was accepted. With both a nonempty source fragment and a
	// writable destination, this one-unit-progress engine must advance at least
	// one cursor; a stationary step is legal only at a zero bound.
	require(result.status == ::fast_io::transcode_step_status::need_input ||
			result.status == ::fast_io::transcode_step_status::need_output);
	bool const accepted_fragment{result.from_next == from_last};
	require((result.status == ::fast_io::transcode_step_status::need_input) ==
			accepted_fragment);
	require(consumed != 0u || produced != 0u || from_first == from_last ||
			destination_capacity == 0u);
	return from_offset + consumed;
}

template <typename engine_type, typename char_type, ::std::size_t input_capacity>
inline void process_fragmented(
	engine_type &engine, fixed_units<char_type, input_capacity> const &input,
	::std::size_t source_extent, ::std::size_t destination_extent,
	fixed_units<char_type, fragmentation_output_capacity> &output) noexcept
{
	if (input.size == 0u)
	{
		::std::size_t const next{process_fragment(
			engine, input, 0u, 0u, destination_extent, output)};
		require(next == 0u);
		return;
	}

	::std::size_t source_offset{};
	::std::size_t steps{};
	::std::size_t const step_budget{input.size * 8u + 64u};
	while (source_offset != input.size)
	{
		require(++steps <= step_budget);
		::std::size_t const progress_source_extent{
			source_extent == 0u ? 1u : source_extent};
		::std::size_t source_limit{source_offset + progress_source_extent};
		if (source_limit > input.size)
		{
			source_limit = input.size;
		}

		if (source_extent == 0u || destination_extent == 0u)
		{
			// A zero extent is a real protocol probe, not a skipped matrix cell.
			// The following one-unit rescue bound supplies the missing resource and
			// makes the liveness proof independent of implementation timing.
			::std::size_t const probe_source_limit{
				source_extent == 0u ? source_offset : source_limit};
			::std::size_t const probe_destination_extent{
				destination_extent == 0u ? 0u : destination_extent};
			::std::size_t const probe_next{process_fragment(
				engine, input, source_offset, probe_source_limit,
				probe_destination_extent, output)};
			require(probe_next == source_offset);
		}

		::std::size_t const progress_destination_extent{
			destination_extent == 0u ? 1u : destination_extent};
		while (source_offset != source_limit)
		{
			require(++steps <= step_budget);
			::std::size_t const old_source_offset{source_offset};
			::std::size_t const old_output_size{output.size};
			source_offset = process_fragment(
				engine, input, source_offset, source_limit,
				progress_destination_extent, output);
			require(source_offset != old_source_offset ||
					output.size != old_output_size);
		}
	}
}

enum class direct_drain_phase
{
	sync_flush,
	finish
};

template <direct_drain_phase phase, typename engine_type, typename char_type>
[[nodiscard]] inline ::fast_io::transcode_drain_status drain_fragment(
	engine_type &engine, ::std::size_t destination_capacity,
	fixed_units<char_type, fragmentation_output_capacity> &output) noexcept
{
	require(destination_capacity <= fragmentation_window_capacity);
	char_type const guard_value{ascii_unit<char_type>('~')};
	::std::array<char_type, fragmentation_window_capacity + 2u> window{};
	for (auto &value : window)
	{
		value = guard_value;
	}
	char_type *const to_first{window.data() + 1u};
	char_type *const to_last{to_first + destination_capacity};
	auto const result{[&] {
		if constexpr (phase == direct_drain_phase::sync_flush)
		{
			return ::fast_io::operations::transcode_sync_flush(
				engine, to_first, to_last);
		}
		else
		{
			return ::fast_io::operations::transcode_finish(
				engine, to_first, to_last);
		}
	}()};
	require(result.to_next >= to_first && result.to_next <= to_last);
	require(window.front() == guard_value &&
			window[destination_capacity + 1u] == guard_value);
	::std::size_t const produced{
		static_cast<::std::size_t>(result.to_next - to_first)};
	for (::std::size_t index{}; index != produced; ++index)
	{
		output.push(to_first[index]);
	}
	// EOL retains at most one code unit and advertises a one-unit minimum. Hence
	// need_output is possible here only for an actual zero-capacity destination;
	// a positive drain window must either commit the pending unit once or report
	// that the phase was already complete.
	require(result.status == ::fast_io::transcode_drain_status::complete ||
			result.status == ::fast_io::transcode_drain_status::need_output);
	if (result.status == ::fast_io::transcode_drain_status::need_output)
	{
		require(destination_capacity == 0u && produced == 0u);
	}
	return result.status;
}

template <direct_drain_phase phase, typename engine_type, typename char_type>
inline void drain_fragmented(
	engine_type &engine, ::std::size_t destination_extent,
	fixed_units<char_type, fragmentation_output_capacity> &output) noexcept
{
	for (::std::size_t step{}; step != 4u; ++step)
	{
		::std::size_t const capacity{
			destination_extent == 0u && step != 0u ? 1u : destination_extent};
		auto const status{drain_fragment<phase>(engine, capacity, output)};
		if (status == ::fast_io::transcode_drain_status::complete)
		{
			return;
		}
	}
	require(false);
}

template <direct_drain_phase phase, typename engine_type, typename char_type>
inline void require_stable_drain(
	engine_type &engine, ::std::size_t destination_extent,
	fixed_units<char_type, fragmentation_output_capacity> &output) noexcept
{
	::std::size_t const old_size{output.size};
	auto const status{
		drain_fragment<phase>(engine, destination_extent, output)};
	require(status == ::fast_io::transcode_drain_status::complete &&
			output.size == old_size);
}

template <typename char_type, ::fast_io::transcoders::eol_scheme from_scheme,
		  ::fast_io::transcoders::eol_scheme to_scheme,
		  ::std::size_t input_capacity>
inline void test_direct_fragmentation_case(
	fixed_units<char_type, input_capacity> const &input,
	::std::size_t source_extent, ::std::size_t destination_extent) noexcept
{
	using engine_type = ::fast_io::transcoders::basic_eol<
		char_type, from_scheme, to_scheme>;
	auto const main_expected{
		reference_eol_transform<char_type, from_scheme, to_scheme>(input)};

	{
		engine_type engine{};
		fixed_units<char_type, fragmentation_output_capacity> actual{};
		process_fragmented(
			engine, input, source_extent, destination_extent, actual);
		require_prefix(actual, main_expected);
		require(main_expected.size - actual.size <= 1u);

		// Sync flush is a nonterminal transaction boundary. It must publish the
		// sole pending unit exactly once, remain idempotent, and leave the engine
		// usable for another independently transformed source segment.
		drain_fragmented<direct_drain_phase::sync_flush>(
			engine, destination_extent, actual);
		require(actual.size == main_expected.size);
		require_prefix(actual, main_expected);
		require_stable_drain<direct_drain_phase::sync_flush>(
			engine, destination_extent, actual);

		auto const suffix{[] {
			if constexpr (
				from_scheme == ::fast_io::transcoders::eol_scheme::lf)
			{
				return make_fragmentation_literal<char_type>("\n");
			}
			else
			{
				// LF cannot look back across sync flush; the trailing CR is left for
				// finish, testing both nonterminal separation and terminal commit.
				return make_fragmentation_literal<char_type>("\n\r");
			}
		}()};
		auto complete_expected{main_expected};
		auto const suffix_expected{
			reference_eol_transform<char_type, from_scheme, to_scheme>(suffix)};
		append_units(complete_expected, suffix_expected);
		process_fragmented(
			engine, suffix, source_extent, destination_extent, actual);
		require_prefix(actual, complete_expected);
		require(complete_expected.size - actual.size <= 1u);
		drain_fragmented<direct_drain_phase::finish>(
			engine, destination_extent, actual);
		require(actual.size == complete_expected.size);
		require_prefix(actual, complete_expected);
		require_stable_drain<direct_drain_phase::finish>(
			engine, 0u, actual);
		require_stable_drain<direct_drain_phase::finish>(
			engine, destination_extent, actual);
	}

	{
		engine_type engine{};
		fixed_units<char_type, fragmentation_output_capacity> actual{};
		process_fragmented(
			engine, input, source_extent, destination_extent, actual);
		require_prefix(actual, main_expected);
		require(main_expected.size - actual.size <= 1u);
		// A direct finish must commit the same pending unit that sync flush would
		// have exposed. Two further finishes prove terminal drain idempotence and
		// reject duplicate publication independently of the first drain capacity.
		drain_fragmented<direct_drain_phase::finish>(
			engine, destination_extent, actual);
		require(actual.size == main_expected.size);
		require_prefix(actual, main_expected);
		require_stable_drain<direct_drain_phase::finish>(
			engine, 0u, actual);
		require_stable_drain<direct_drain_phase::finish>(
			engine, destination_extent, actual);
	}
}

template <typename char_type, ::std::size_t extent_count>
inline void test_direct_engine_fragmentation_matrix(
	::std::array<::std::size_t, extent_count> const &extents,
	bool reduced_corpus) noexcept
{
	auto const empty{make_fragmentation_literal<char_type>("")};
	auto const edge_dense{
		make_fragmentation_literal<char_type>("\n\nA\rB\r\nC\n\n")};
	auto const sparse{make_sparse_fragmentation_message<char_type>()};
	auto const terminal_cr{make_fragmentation_literal<char_type>("Q\r")};
	auto const lf_lane_boundaries{make_lf_lane_boundary_message<char_type>()};
	auto const cr_lane_boundaries{make_cr_lane_boundary_message<char_type>()};
	for (auto const source_extent : extents)
	{
		auto const source_boundary{
			make_source_boundary_message<char_type>(source_extent)};
		for (auto const destination_extent : extents)
		{
			auto const run_directions = [&](auto const &message) {
				test_direct_fragmentation_case<
					char_type, ::fast_io::transcoders::eol_scheme::lf,
					::fast_io::transcoders::eol_scheme::crlf>(
					message, source_extent, destination_extent);
				test_direct_fragmentation_case<
					char_type, ::fast_io::transcoders::eol_scheme::crlf,
					::fast_io::transcoders::eol_scheme::lf>(
					message, source_extent, destination_extent);
				test_direct_fragmentation_case<
					char_type, ::fast_io::transcoders::eol_scheme::lf,
					::fast_io::transcoders::eol_scheme::cr>(
					message, source_extent, destination_extent);
			};
			run_directions(empty);
			run_directions(edge_dense);
			run_directions(source_boundary);
			if (!reduced_corpus)
			{
				run_directions(sparse);
				run_directions(terminal_cr);
				run_directions(lf_lane_boundaries);
				run_directions(cr_lane_boundaries);
			}
		}
	}
}

inline void test_direct_engine_fragmentation() noexcept
{
	// The byte engine receives the complete Cartesian product, including real
	// zero-bound calls. Wider code units reuse the same state-machine proof on a
	// smaller boundary matrix so delimiter semantics cannot accidentally depend
	// on the byte-oriented fast path.
	test_direct_engine_fragmentation_matrix<char>(fragmentation_extents, false);
	test_direct_engine_fragmentation_matrix<char16_t>(
		small_code_unit_fragmentation_extents, true);
	test_direct_engine_fragmentation_matrix<char32_t>(
		small_code_unit_fragmentation_extents, true);
}

template <typename operation>
[[nodiscard]] inline bool rejects_invalid_state(operation &&invoke)
{
#if defined(__cpp_exceptions)
	try
	{
		invoke();
	}
	catch (::fast_io::error const &error)
	{
		return error == ::fast_io::transcode_stream_errc::invalid_state;
	}
	return false;
#else
	// Calling an invalid operation terminates in a no-exception build, so that
	// configuration can retain the positive lifecycle checks only.
	(void)invoke;
	return true;
#endif
}

inline void test_bounded_engine_boundaries() noexcept
{
	{
		::fast_io::transcoders::lf_to_crlf engine{};
		char const source[]{'\n', 'X'};
		char output[3]{};

		// The process invariant permits accepting the LF after emitting only CR:
		// the unresolved LF remains engine state, not an uncommitted source unit.
		auto const split{::fast_io::operations::transcode_process(
			engine, source, source + 1u, output, output + 1u)};
		require(split.from_next == source + 1u &&
				split.to_next == output + 1u && output[0] == '\r' &&
				split.status == ::fast_io::transcode_step_status::need_input);

		// A zero-capacity sync-flush must retain the pending LF and request output;
		// the next bounded drain commits it exactly once without closing the engine.
		auto const blocked_flush{::fast_io::operations::transcode_sync_flush(
			engine, output + 1u, output + 1u)};
		require(blocked_flush.to_next == output + 1u &&
				blocked_flush.status ==
					::fast_io::transcode_drain_status::need_output);
		auto const completed_flush{::fast_io::operations::transcode_sync_flush(
			engine, output + 1u, output + 2u)};
		require(completed_flush.to_next == output + 2u && output[1] == '\n' &&
				completed_flush.status ==
					::fast_io::transcode_drain_status::complete);

		auto const continued{::fast_io::operations::transcode_process(
			engine, source + 1u, source + 2u, output + 2u, output + 3u)};
		require(continued.from_next == source + 2u &&
				continued.to_next == output + 3u && output[2] == 'X');
		auto const finished{::fast_io::operations::transcode_finish(
			engine, output + 3u, output + 3u)};
		require(finished.to_next == output + 3u &&
				finished.status == ::fast_io::transcode_drain_status::complete);
	}

	{
		::fast_io::transcoders::crlf_to_lf engine{};
		char const source[]{'\r', '\n', 'Q', '\r'};
		char output[3]{};

		// A CR at the end of one source range is transactional lookahead state.
		// Supplying LF in the next process call must contract the pair once.
		auto const cr_step{::fast_io::operations::transcode_process(
			engine, source, source + 1u, output, output + 3u)};
		require(cr_step.from_next == source + 1u && cr_step.to_next == output);
		auto const lf_step{::fast_io::operations::transcode_process(
			engine, source + 1u, source + 2u, output, output + 3u)};
		require(lf_step.from_next == source + 2u &&
				lf_step.to_next == output + 1u && output[0] == '\n');

		auto const suffix_step{::fast_io::operations::transcode_process(
			engine, source + 2u, source + 4u, output + 1u, output + 3u)};
		require(suffix_step.from_next == source + 4u &&
				suffix_step.to_next == output + 2u && output[1] == 'Q');

		// Terminal drain cannot discard an unmatched CR. Capacity failure preserves
		// it, and the following finish emits exactly one literal CR.
		auto const blocked_finish{::fast_io::operations::transcode_finish(
			engine, output + 2u, output + 2u)};
		require(blocked_finish.to_next == output + 2u &&
				blocked_finish.status ==
					::fast_io::transcode_drain_status::need_output);
		auto const completed_finish{::fast_io::operations::transcode_finish(
			engine, output + 2u, output + 3u)};
		require(completed_finish.to_next == output + 3u && output[2] == '\r' &&
				completed_finish.status ==
					::fast_io::transcode_drain_status::complete);
		auto const repeated_finish{::fast_io::operations::transcode_finish(
			engine, output + 3u, output + 3u)};
		require(repeated_finish.to_next == output + 3u &&
				repeated_finish.status ==
					::fast_io::transcode_drain_status::complete);
	}

	{
		::fast_io::transcoders::crlf_to_lf engine{};
		char const source[]{'\r', '\n'};
		char output[2]{};
		auto const cr_step{::fast_io::operations::transcode_process(
			engine, source, source + 1u, output, output + 2u)};
		require(cr_step.from_next == source + 1u && cr_step.to_next == output);
		auto const synchronized{::fast_io::operations::transcode_sync_flush(
			engine, output, output + 1u)};
		require(synchronized.to_next == output + 1u && output[0] == '\r' &&
				synchronized.status ==
					::fast_io::transcode_drain_status::complete);

		// Sync-flush is a visible nonterminal boundary: after publishing the CR,
		// a later LF is independent data rather than lookahead for the old range.
		auto const lf_step{::fast_io::operations::transcode_process(
			engine, source + 1u, source + 2u, output + 1u, output + 2u)};
		require(lf_step.from_next == source + 2u &&
				lf_step.to_next == output + 2u && output[1] == '\n');
	}
}

inline void test_output_adapter_matrix()
{
	for (auto const logical_size : boundary_sizes)
	{
		auto const record{make_record(logical_size)};
		::std::array<char, maximum_encoded_size> storage{};
		::fast_io::basic_obuffer_view<char> physical_output{
			storage.data(), storage.data() + record.encoded_size};
		auto output{::fast_io::make_otranscoder(
			physical_output, ::fast_io::transcoders::lf_to_crlf{})};

		::std::size_t const split{(record.logical_size + 1u) / 2u};
		::fast_io::operations::write_all(
			output, record.logical.data(), record.logical.data() + split);
		::fast_io::operations::output_stream_buffer_flush(output);

		// Sync-flush establishes a visibility boundary but leaves the output
		// adapter open. The committed physical prefix must be the independent EOL
		// expansion of exactly the logical prefix written before the flush.
		::std::size_t const physical_prefix{
			encoded_prefix_size(record, split)};
		require(physical_output.size() == physical_prefix);
		require_equal(storage.data(), record.encoded.data(), physical_prefix);

		::fast_io::operations::write_all(
			output, record.logical.data() + split,
			record.logical.data() + record.logical_size);
		::fast_io::operations::output_stream_finish(output);
		::fast_io::operations::output_stream_finish(output);

		// Successful finish is terminal and idempotent: the second call cannot add
		// a duplicate newline suffix or advance the underlying cursor.
		require(physical_output.size() == record.encoded_size);
		require_equal(
			storage.data(), record.encoded.data(), record.encoded_size);
		char const forbidden_write{'!'};
		require(rejects_invalid_state([&] {
			::fast_io::operations::write_all(
				output, __builtin_addressof(forbidden_write),
				__builtin_addressof(forbidden_write) + 1u);
		}));
	}
}

inline void test_input_adapter_matrix()
{
	for (auto const logical_size : boundary_sizes)
	{
		auto const record{make_record(logical_size)};
		for (auto const chunk_size : read_chunk_sizes)
		{
			::fast_io::basic_ibuffer_view<char> physical_input{
				record.encoded.data(),
				record.encoded.data() + record.encoded_size};
			auto input{::fast_io::make_itranscoder(
				physical_input, ::fast_io::transcoders::crlf_to_lf{})};
			::std::array<char, maximum_logical_size + 1u> decoded{};
			::std::size_t decoded_size{};

			for (;;)
			{
				::std::size_t const remaining_capacity{
					decoded.size() - decoded_size};
				require(remaining_capacity != 0u);
				::std::size_t const request{
					chunk_size < remaining_capacity ? chunk_size
													: remaining_capacity};
				char *const first{decoded.data() + decoded_size};
				char *const next{::fast_io::operations::read_some(
					input, first, first + request)};
				require(next >= first && next <= first + request);
				::std::size_t const count{
					static_cast<::std::size_t>(next - first)};
				decoded_size += count;
				if (count == 0u)
				{
					break;
				}
			}

			// Logical EOF is observable only after physical EOF and engine finish.
			// Thus a trailing unmatched CR must already be present before the first
			// empty read, independently of the caller's read chunk size.
			require(decoded_size == record.logical_size);
			require_equal(
				decoded.data(), record.logical.data(), record.logical_size);
			require(physical_input.curr_ptr == physical_input.end_ptr);

			::fast_io::operations::input_stream_drain_and_finish(input);
			::fast_io::operations::input_stream_drain_and_finish(input);
			char stable_eof{};
			require(::fast_io::operations::read_some(
						input, __builtin_addressof(stable_eof),
						__builtin_addressof(stable_eof) + 1u) ==
					__builtin_addressof(stable_eof));
		}
	}
}

inline void test_explicit_input_drain()
{
	for (auto const logical_size : boundary_sizes)
	{
		auto const record{make_record(logical_size)};
		::fast_io::basic_ibuffer_view<char> physical_input{
			record.encoded.data(), record.encoded.data() + record.encoded_size};
		auto input{::fast_io::make_itranscoder(
			physical_input, ::fast_io::transcoders::crlf_to_lf{})};

		if (logical_size != 0u)
		{
			char first_logical{};
			char *const next{::fast_io::operations::read_some(
				input, __builtin_addressof(first_logical),
				__builtin_addressof(first_logical) + 1u)};
			require(next == __builtin_addressof(first_logical) + 1u &&
					first_logical == record.logical[0]);
		}

		// Explicit drain is destructive by contract: any already decoded suffix is
		// discarded while the physical source and terminal engine output are driven
		// to completion. Repetition must neither resume nor reject the finished input.
		::fast_io::operations::input_stream_drain_and_finish(input);
		::fast_io::operations::input_stream_drain_and_finish(input);
		require(physical_input.curr_ptr == physical_input.end_ptr);
		char stable_eof{};
		require(::fast_io::operations::read_some(
					input, __builtin_addressof(stable_eof),
					__builtin_addressof(stable_eof) + 1u) ==
				__builtin_addressof(stable_eof));
	}
}

inline void test_nonterminal_destruction_cancels()
{
	{
		::std::array<char, 4u> storage{};
		::fast_io::basic_obuffer_view<char> physical_output{storage};
		{
			auto output{::fast_io::make_otranscoder(
				physical_output, ::fast_io::transcoders::lf_to_crlf{})};
			char const pending{'P'};
			::fast_io::operations::write_all(
				output, __builtin_addressof(pending),
				__builtin_addressof(pending) + 1u);
			require(physical_output.empty());
		}
		// Owner destruction is cancellation, not an implicit terminal commit.
		require(physical_output.empty());
	}

	{
		char const source[]{'A', '\r'};
		::fast_io::basic_ibuffer_view<char> physical_input{
			source, source + 2u};
		{
			auto input{::fast_io::make_itranscoder(
				physical_input, ::fast_io::transcoders::crlf_to_lf{})};
			(void)input;
		}
		// Construction and destruction are lazy and must not consume or validate
		// unread physical input without explicit read/drain authority.
		require(physical_input.curr_ptr == physical_input.begin_ptr);
	}
}

static_assert(::fast_io::transcoder<::fast_io::transcoders::lf_to_crlf>);
static_assert(::fast_io::transcoder<::fast_io::transcoders::crlf_to_lf>);
static_assert(::fast_io::operations::transcode_min_output_size(
				  ::fast_io::transcode_reserve<::fast_io::transcoders::lf_to_crlf>,
				  ::fast_io::transcode_phase::process) == 1u);

} // namespace transcoder_stream_lifecycle_test

int main()
{
	::transcoder_stream_lifecycle_test::test_isolated_relative_lanes();
	::transcoder_stream_lifecycle_test::test_identity_aliases();
	::transcoder_stream_lifecycle_test::test_direct_engine_fragmentation();
	::transcoder_stream_lifecycle_test::test_bounded_engine_boundaries();
	::transcoder_stream_lifecycle_test::test_output_adapter_matrix();
	::transcoder_stream_lifecycle_test::test_input_adapter_matrix();
	::transcoder_stream_lifecycle_test::test_explicit_input_drain();
	::transcoder_stream_lifecycle_test::test_nonterminal_destruction_cancels();
}
