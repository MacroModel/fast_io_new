#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#ifndef FAST_IO_STATE_TRANSCODER_DIRECTION_MODE
#define FAST_IO_STATE_TRANSCODER_DIRECTION_MODE 0
#endif

#ifndef FAST_IO_STATE_TRANSCODER_DIRECTION_CHUNK
#define FAST_IO_STATE_TRANSCODER_DIRECTION_CHUNK 6
#endif

#ifndef FAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY
#define FAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY 0
#endif

#ifndef FAST_IO_STATE_TRANSCODER_DIRECTION_OLD
#define FAST_IO_STATE_TRANSCODER_DIRECTION_OLD 0
#endif

/*
The policy macro is part of the experiment identity.  Rejecting an inherited
FAST_IO_BUFFER_SIZE prevents a row labelled `default-cold` from silently using
a tuned allocation policy; conversely the sensitivity row is exactly 64 bytes,
not merely any nondefault build supplied by the environment.
*/
#if FAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY == 0
#ifdef FAST_IO_BUFFER_SIZE
#error "default-cold transcoder direction cells require FAST_IO_BUFFER_SIZE to be absent"
#endif
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY == 1
#ifndef FAST_IO_BUFFER_SIZE
#error "buffer64-cold transcoder direction cells require FAST_IO_BUFFER_SIZE=64"
#elif FAST_IO_BUFFER_SIZE != 64
#error "buffer64-cold transcoder direction cells require exactly FAST_IO_BUFFER_SIZE=64"
#endif
#else
#error "unsupported transcoder direction buffer policy"
#endif

#include <fast_io.h>

#if FAST_IO_STATE_TRANSCODER_DIRECTION_OLD
/*
The official tree does not expose its experimental EOL transcoder from the
umbrella header.  This include is intentionally old-only: current adapter and
CPO rows must use the current public aggregation surface.
*/
#include <fast_io_freestanding_impl/transcoders/eol.h>
#endif

#include "../0026.cpo_matrix/case_driver.h"
#include "fixture.h"
#include "independent_oracle.h"

namespace fast_io_state_machine_cpo::transcoder_direction
{

inline constexpr unsigned selected_mode{
	FAST_IO_STATE_TRANSCODER_DIRECTION_MODE};
inline constexpr unsigned selected_chunk{
	FAST_IO_STATE_TRANSCODER_DIRECTION_CHUNK};
inline constexpr unsigned selected_buffer_policy{
	FAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY};
inline constexpr bool selected_old_tree{
	FAST_IO_STATE_TRANSCODER_DIRECTION_OLD != 0};

static_assert(selected_mode <= 7u);
static_assert(selected_chunk <= 6u);
static_assert(selected_buffer_policy <= 1u);
static_assert(!selected_old_tree || selected_mode == 0u || selected_mode == 2u,
			  "the official tree is comparable only through direct-member engine rows");
static_assert(!selected_old_tree || selected_buffer_policy == 0u,
			  "adapter buffer policy is inapplicable to official engine-only rows");

inline constexpr ::std::array<::std::size_t, 7u> chunk_sizes{
	1u, 2u, 3u, 63u, 64u, 65u, maximum_encoded_size + 1u};
inline constexpr ::std::size_t selected_chunk_capacity{
	chunk_sizes[selected_chunk]};
inline constexpr bool selected_chunk_is_bulk{selected_chunk == 6u};
inline constexpr ::std::size_t direction_corpus_size{16u};
inline constexpr ::std::size_t direction_storage_size{
	maximum_encoded_size + 1u};

using direction_corpus =
	::std::array<transcode_record, direction_corpus_size>;

struct transcode_observation
{
	bool valid{};
	::std::size_t output_size{};
	::std::size_t logical_units{};
};

struct engine_process_step
{
	char const *from_next{};
	char *to_next{};
	bool source_complete{};
};

struct engine_finish_step
{
	char *to_next{};
	bool complete{};
};

[[nodiscard]] inline constexpr ::std::size_t bounded_advance(
	::std::size_t position, ::std::size_t end,
	::std::size_t extent) noexcept
{
	auto const remaining{end - position};
	return position + (remaining < extent ? remaining : extent);
}

inline constexpr void copy_units(
	char *destination, char const *source, ::std::size_t count) noexcept
{
	for (::std::size_t index{}; index != count; ++index)
	{
		destination[index] = source[index];
	}
}

inline void build_direction_corpus(
	direction_corpus &corpus, ::std::uint_least64_t seed) noexcept
{
	/*
	The sizes surround both the small protocol windows and the 63/64/65
	allocation boundary.  Sixteen records retain a power-of-two timed selector
	without sacrificing the 127/128/129 and 255/256 suffix boundaries.
	*/
	static constexpr ::std::array<::std::size_t, direction_corpus_size>
		logical_sizes{0u, 1u, 2u, 3u, 31u, 62u, 63u, 64u,
					  65u, 66u, 127u, 128u, 129u, 255u, 256u, 383u};
	static constexpr char alphabet[]{
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \t"};
	auto random{seed ^ UINT64_C(0x6a09e667f3bcc909)};
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto &record{corpus[record_index]};
		record.logical_size = logical_sizes[record_index];
		record.encoded_size = 0u;
		for (::std::size_t index{}; index != record.logical_size; ++index)
		{
			random = xorshift64(random + UINT64_C(0xbb67ae8584caa73b));
			/*
			For a finite physical window, placing CR at its final slot forces the
			following LF into the next source transition.  The fixture therefore
			exercises retained lookahead state by construction, while remaining a
			canonical CRLF encoding with no raw carriage return ambiguity.
			*/
			bool const split_crlf{
				!selected_chunk_is_bulk &&
				record.encoded_size % selected_chunk_capacity ==
					selected_chunk_capacity - 1u};
			bool const edge_newline{
				record.logical_size != 0u &&
				(index == 0u || index + 1u == record.logical_size)};
			char const character{
				split_crlf || edge_newline || random % 13u == 0u
					? '\n'
					: alphabet[static_cast<::std::size_t>(random) %
							   (sizeof(alphabet) - 1u)]};
			record.logical[index] = character;
			if (character == '\n')
			{
				record.encoded[record.encoded_size++] = '\r';
			}
			record.encoded[record.encoded_size++] = character;
		}
	}
}

template <bool use_public_cpo, typename engine_type>
[[nodiscard]] inline engine_process_step invoke_engine_process(
	engine_type &engine, char const *from_first, char const *from_last,
	char *to_first, char *to_last) noexcept
{
#if FAST_IO_STATE_TRANSCODER_DIRECTION_OLD
	static_assert(!use_public_cpo,
				  "the official EOL engine predates the current bounded CPO");
	/*
	Old exposes one member returning only cursor pairs.  Source completion is
	therefore derived from the exact supplied range.  No adapter or current-CPO
	claim is attached to this compatibility path.
	*/
	auto [from_next, to_next]{
		engine.transcode(from_first, from_last, to_first, to_last)};
	return {from_next, to_next, from_next == from_last};
#else
	if constexpr (use_public_cpo)
	{
		auto const result{::fast_io::operations::transcode_process(
			engine, from_first, from_last, to_first, to_last)};
		return {result.from_next, result.to_next,
				result.status == ::fast_io::transcode_step_status::need_input};
	}
	else
	{
		auto const result{
			engine.process(from_first, from_last, to_first, to_last)};
		return {result.from_next, result.to_next,
				result.status == ::fast_io::transcode_step_status::need_input};
	}
#endif
}

template <bool use_public_cpo, typename engine_type>
[[nodiscard]] inline engine_finish_step invoke_engine_finish(
	engine_type &engine, char *to_first, char *to_last) noexcept
{
#if FAST_IO_STATE_TRANSCODER_DIRECTION_OLD
	(void)engine;
	(void)to_last;
	/*
	The comparable old member rows supply the proven expansion bound on every
	process call and use canonical CRLF input, so no unresolved terminal unit is
	permitted.  Old has no separate finish operation to time or emulate.
	*/
	return {to_first, true};
#else
	if constexpr (use_public_cpo)
	{
		auto const result{::fast_io::operations::transcode_finish(
			engine, to_first, to_last)};
		return {result.to_next,
				result.status == ::fast_io::transcode_drain_status::complete};
	}
	else
	{
		auto const result{engine.finish(to_first, to_last)};
		return {result.to_next,
				result.status == ::fast_io::transcode_drain_status::complete};
	}
#endif
}

template <bool decode, bool use_public_cpo>
[[nodiscard]] inline transcode_observation run_engine(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage) noexcept
{
	using engine_type = ::std::conditional_t<
		decode, ::fast_io::transcoders::crlf_to_lf,
		::fast_io::transcoders::lf_to_crlf>;
	engine_type engine{};
	char const *const source_begin{
		decode ? record.encoded.data() : record.logical.data()};
	::std::size_t const source_size{
		decode ? record.encoded_size : record.logical_size};
	char const *source_current{source_begin};
	char *output_current{output_storage.data()};
	char *const output_end{output_storage.data() + output_storage.size()};
	::std::size_t const source_window{
		selected_chunk_is_bulk ? source_size : selected_chunk_capacity};
	/*
	Capacity invariant: CRLF contraction emits at most one unit per supplied
	source unit, while LF expansion emits at most two.  Deriving the destination
	window from this semantic bound keeps the old member control within its
	contract even for chunk=1; an old small-capacity defect is not smuggled into
	the dispatch comparison as a nominal workload difference.
	*/
	::std::size_t const destination_window{
		selected_chunk_is_bulk
			? output_storage.size()
			: selected_chunk_capacity * (decode ? 1u : 2u)};
	while (source_current != source_begin + source_size)
	{
		auto const remaining_source{static_cast<::std::size_t>(
			(source_begin + source_size) - source_current)};
		auto const supplied_source{
			remaining_source < source_window ? remaining_source : source_window};
		char const *const source_last{source_current + supplied_source};
		while (source_current != source_last)
		{
			if (output_current == output_end)
			{
				return {false, 0u, 0u};
			}
			auto const remaining_output{static_cast<::std::size_t>(
				output_end - output_current)};
			auto const supplied_output{
				remaining_output < destination_window ? remaining_output
													  : destination_window};
			char *const destination_last{output_current + supplied_output};
			char const *const old_source{source_current};
			char *const old_output{output_current};
			auto const step{invoke_engine_process<use_public_cpo>(
				engine, source_current, source_last, output_current,
				destination_last)};
			if (step.from_next < source_current || step.from_next > source_last ||
				step.to_next < output_current || step.to_next > destination_last)
			{
				return {false, 0u, 0u};
			}
			source_current = step.from_next;
			output_current = step.to_next;
			if (step.source_complete != (source_current == source_last) ||
				(source_current == old_source && output_current == old_output))
			{
				return {false, 0u, 0u};
			}
		}
	}

	for (;;)
	{
		/*
		The extra storage unit guarantees the engine's mandatory minimum finish
		capacity even when process output exactly reaches the semantic maximum.
		Finish may emit only inside this supplied range and must either complete or
		make positive progress before requesting another range.
		*/
		auto const remaining_output{
			static_cast<::std::size_t>(output_end - output_current)};
		if (remaining_output == 0u)
		{
			return {false, 0u, 0u};
		}
		auto const supplied_output{
			remaining_output < destination_window ? remaining_output
												  : destination_window};
		char *const destination_last{output_current + supplied_output};
		auto const finish{invoke_engine_finish<use_public_cpo>(
			engine, output_current, destination_last)};
		if (finish.to_next < output_current ||
			finish.to_next > destination_last)
		{
			return {false, 0u, 0u};
		}
		bool const made_progress{finish.to_next != output_current};
		output_current = finish.to_next;
		if (finish.complete)
		{
			break;
		}
		if (!made_progress)
		{
			return {false, 0u, 0u};
		}
	}
	return {true,
			static_cast<::std::size_t>(output_current - output_storage.data()),
			record.logical_size};
}

#if !FAST_IO_STATE_TRANSCODER_DIRECTION_OLD &&      \
	FAST_IO_STATE_TRANSCODER_DIRECTION_MODE >= 4 && \
	FAST_IO_STATE_TRANSCODER_DIRECTION_MODE <= 6

template <::std::size_t maximum_chunk>
struct bounded_input;

template <::std::size_t maximum_chunk>
struct bounded_input_ref
{
	using input_char_type = char;
	bounded_input<maximum_chunk> *owner{};
};

template <::std::size_t maximum_chunk>
struct bounded_input
{
	using input_char_type = char;
	char const *data{};
	::std::size_t size{};
	::std::size_t position{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

template <::std::size_t maximum_chunk>
[[nodiscard]] inline bounded_input_ref<maximum_chunk>
input_stream_ref_define(bounded_input<maximum_chunk> &input) noexcept
{
	++input.normalizations;
	return {__builtin_addressof(input)};
}

template <::std::size_t maximum_chunk>
inline char *read_some_underflow_define(
	bounded_input_ref<maximum_chunk> input, char *first, char *last) noexcept
{
	/*
	Formal source invariant: position is monotone in [0,size], the returned
	pointer is in [first,last], and its advance equals the physical cursor
	advance.  A short nonempty prefix is progress; only zero advance at
	position==size denotes physical EOF.
	*/
	static_assert(maximum_chunk != 0u);
	++input.owner->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.owner->size - input.owner->position};
	if (count > maximum_chunk)
	{
		count = maximum_chunk;
	}
	if (count > remaining)
	{
		count = remaining;
	}
	copy_units(first, input.owner->data + input.owner->position, count);
	input.owner->position += count;
	return first + count;
}

template <::std::size_t maximum_chunk>
struct bounded_output;

template <::std::size_t maximum_chunk>
struct bounded_output_ref
{
	using output_char_type = char;
	bounded_output<maximum_chunk> *owner{};
};

template <::std::size_t maximum_chunk>
struct bounded_output
{
	using output_char_type = char;
	char *data{};
	::std::size_t capacity{};
	::std::size_t position{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

template <::std::size_t maximum_chunk>
[[nodiscard]] inline bounded_output_ref<maximum_chunk>
output_stream_ref_define(bounded_output<maximum_chunk> &output) noexcept
{
	++output.normalizations;
	return {__builtin_addressof(output)};
}

template <::std::size_t maximum_chunk>
inline char const *write_some_overflow_define(
	bounded_output_ref<maximum_chunk> output, char const *first,
	char const *last) noexcept
{
	/*
	Formal sink invariant: storage[0,position) is exactly the concatenation of
	all accepted prefixes, position never exceeds capacity, and every nonempty
	request advances by 1..maximum_chunk.  The abort branch converts impossible
	capacity exhaustion into a bounded failure instead of a write-all livelock.
	*/
	static_assert(maximum_chunk != 0u);
	++output.owner->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{output.owner->capacity - output.owner->position};
	if (count > maximum_chunk)
	{
		count = maximum_chunk;
	}
	if (count > remaining)
	{
		count = remaining;
	}
	if (count == 0u && first != last)
	{
		/* A full sink would make generic write-all retry forever. */
		::std::abort();
	}
	copy_units(output.owner->data + output.owner->position, first, count);
	output.owner->position += count;
	return first + count;
}

#if FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 4

[[nodiscard]] inline transcode_observation run_input_adapter(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage)
{
	bounded_input<selected_chunk_capacity> physical_input{
		record.encoded.data(), record.encoded_size, 0u, 0u, 0u};
	auto input{::fast_io::make_itranscoder(
		physical_input, ::fast_io::transcoders::crlf_to_lf{})};
	::std::size_t output_size{};
	for (;;)
	{
		auto const remaining{output_storage.size() - output_size};
		auto const request{
			remaining < selected_chunk_capacity ? remaining
												: selected_chunk_capacity};
		if (request == 0u)
		{
			return {false, 0u, 0u};
		}
		char *const first{output_storage.data() + output_size};
		char *const next{::fast_io::operations::read_some(
			input, first, first + request)};
		if (next < first || next > first + request)
		{
			return {false, 0u, 0u};
		}
		auto const count{static_cast<::std::size_t>(next - first)};
		output_size += count;
		if (count == 0u)
		{
			break;
		}
	}
	::fast_io::operations::input_stream_drain_and_finish(input);
	return {physical_input.position == record.encoded_size, output_size,
			record.logical_size};
}

#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 5

[[nodiscard]] inline transcode_observation run_output_adapter(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage)
{
	bounded_output<selected_chunk_capacity> physical_output{
		output_storage.data(), output_storage.size(), 0u, 0u, 0u};
	auto output{::fast_io::make_otranscoder(
		physical_output, ::fast_io::transcoders::lf_to_crlf{})};
	::std::size_t position{};
	while (position != record.logical_size)
	{
		auto const next{bounded_advance(
			position, record.logical_size, selected_chunk_capacity)};
		::fast_io::operations::write_all(
			output, record.logical.data() + position,
			record.logical.data() + next);
		position = next;
	}
	::fast_io::operations::output_stream_finish(output);
	return {true, physical_output.position, record.logical_size};
}

#else

[[nodiscard]] inline transcode_observation run_full_transmit(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage)
{
	bounded_input<selected_chunk_capacity> physical_input{
		record.encoded.data(), record.encoded_size, 0u, 0u, 0u};
	bounded_output<selected_chunk_capacity> physical_output{
		output_storage.data(), output_storage.size(), 0u, 0u, 0u};
	auto input{::fast_io::make_itranscoder(
		physical_input, ::fast_io::transcoders::crlf_to_lf{})};
	auto output{::fast_io::make_otranscoder(
		physical_output, ::fast_io::transcoders::lf_to_crlf{})};
	/*
	The measured state machine is the product
	  physical input x decoder x generic transmit x encoder x physical output.
	Logical EOF terminally finishes the input engine.  Because the output owner is
	an lvalue, transmit does not own its message boundary; explicit finish is the
	unique terminal commit.  The following input drain is an idempotent assertion
	that no unvalidated physical suffix survives the transfer.
	*/
	auto const transmitted{
		::fast_io::operations::transmit_until_eof(output, input)};
	::fast_io::operations::output_stream_finish(output);
	::fast_io::operations::input_stream_drain_and_finish(input);
	return {physical_input.position == record.encoded_size,
			physical_output.position,
			static_cast<::std::size_t>(transmitted.transmitted)};
}

#endif

#endif

#if !FAST_IO_STATE_TRANSCODER_DIRECTION_OLD && \
	FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 7

[[nodiscard]] inline transcode_observation run_staged_control(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage) noexcept
{
	::std::array<char, maximum_logical_size + 1u> intermediate{};
	::std::size_t logical_size{};
	bool pending_cr{};
	::std::size_t source_position{};
	while (source_position != record.encoded_size)
	{
		auto const source_end{bounded_advance(
			source_position, record.encoded_size, selected_chunk_capacity)};
		for (; source_position != source_end; ++source_position)
		{
			auto const character{record.encoded[source_position]};
			if (pending_cr)
			{
				if (character == '\n')
				{
					intermediate[logical_size++] = '\n';
					pending_cr = false;
					continue;
				}
				intermediate[logical_size++] = '\r';
				pending_cr = false;
			}
			if (character == '\r')
			{
				pending_cr = true;
			}
			else
			{
				intermediate[logical_size++] = character;
			}
		}
	}
	if (pending_cr)
	{
		intermediate[logical_size++] = '\r';
	}

	/*
	This barrier establishes the staged control's defining lifetime boundary:
	the complete decoded interval must exist before encoding begins.  It performs
	no byte walk, so exact correctness remains exclusively in the untimed oracle.
	*/
	::fast_io_cpo_matrix::compiler_observe_bytes(
		intermediate.data(), logical_size);
	::std::size_t output_size{};
	::std::size_t logical_position{};
	while (logical_position != logical_size)
	{
		auto const logical_end{bounded_advance(
			logical_position, logical_size, selected_chunk_capacity)};
		for (; logical_position != logical_end; ++logical_position)
		{
			auto const character{intermediate[logical_position]};
			if (character == '\n')
			{
				output_storage[output_size++] = '\r';
			}
			output_storage[output_size++] = character;
		}
	}
	return {true, output_size, logical_size};
}

#endif

[[nodiscard]] inline transcode_observation run_selected_once(
	transcode_record const &record,
	::std::array<char, direction_storage_size> &output_storage)
{
#if FAST_IO_STATE_TRANSCODER_DIRECTION_OLD
#if FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 0
	return run_engine<true, false>(record, output_storage);
#else
	return run_engine<false, false>(record, output_storage);
#endif
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 0
	return run_engine<true, false>(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 1
	return run_engine<true, true>(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 2
	return run_engine<false, false>(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 3
	return run_engine<false, true>(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 4
	return run_input_adapter(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 5
	return run_output_adapter(record, output_storage);
#elif FAST_IO_STATE_TRANSCODER_DIRECTION_MODE == 6
	return run_full_transmit(record, output_storage);
#else
	return run_staged_control(record, output_storage);
#endif
}

[[nodiscard]] inline constexpr bool selected_output_is_logical() noexcept
{
	return selected_mode == 0u || selected_mode == 1u || selected_mode == 4u;
}

[[nodiscard]] inline bool validate_corpus(
	direction_corpus const &corpus, ::std::uint_least64_t &digest)
{
	digest = UINT64_C(14695981039346656037);
	::std::array<char, direction_storage_size> output_storage{};
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const &record{corpus[index]};
		if (!oracle::transcode_record_is_canonical(record))
		{
			::std::fprintf(
				stderr, "direction fixture is noncanonical: record=%zu\n", index);
			return false;
		}
		auto const result{run_selected_once(record, output_storage)};
		char const *const expected{selected_output_is_logical()
									   ? record.logical.data()
									   : record.encoded.data()};
		auto const expected_size{selected_output_is_logical()
									 ? record.logical_size
									 : record.encoded_size};
		bool const correct{
			result.valid && result.logical_units == record.logical_size &&
			result.output_size == expected_size &&
			oracle::equal_bytes(output_storage.data(), result.output_size,
								expected, expected_size)};
		if (!correct)
		{
			::std::fprintf(
				stderr,
				"transcoder direction preflight failed: record=%zu logical=%zu encoded=%zu output=%zu units=%zu\n",
				index, record.logical_size, record.encoded_size,
				result.output_size, result.logical_units);
			return false;
		}
		digest = oracle::digest_bytes(
			digest, output_storage.data(), result.output_size);
		digest = (digest ^ static_cast<::std::uint_least64_t>(result.output_size)) *
				 UINT64_C(1099511628211);
	}
	return true;
}

[[nodiscard]] inline constexpr ::std::uint_least64_t observation_signature(
	transcode_observation const &result) noexcept
{
	/*
	Primitive-call schedules are intentionally excluded: legal adapters may
	coalesce or split the same message.  The timed checksum observes only terminal
	success and semantic extents; the preflight digest proves every output byte.
	*/
	auto signature{static_cast<::std::uint_least64_t>(result.output_size)};
	signature ^= static_cast<::std::uint_least64_t>(result.logical_units) << 32u;
	if (result.valid)
	{
		signature ^= UINT64_C(0x9e3779b97f4a7c15);
	}
	return signature;
}

[[nodiscard]] inline constexpr char const *mode_name() noexcept
{
	if constexpr (selected_mode == 0u)
	{
		return "engine-decode-member";
	}
	else if constexpr (selected_mode == 1u)
	{
		return "engine-decode-cpo";
	}
	else if constexpr (selected_mode == 2u)
	{
		return "engine-encode-member";
	}
	else if constexpr (selected_mode == 3u)
	{
		return "engine-encode-cpo";
	}
	else if constexpr (selected_mode == 4u)
	{
		return "input-adapter";
	}
	else if constexpr (selected_mode == 5u)
	{
		return "output-adapter";
	}
	else if constexpr (selected_mode == 6u)
	{
		return "full-transmit";
	}
	else
	{
		return "staged-control";
	}
}

[[nodiscard]] inline constexpr char const *chunk_name() noexcept
{
	if constexpr (selected_chunk == 0u)
	{
		return "1";
	}
	else if constexpr (selected_chunk == 1u)
	{
		return "2";
	}
	else if constexpr (selected_chunk == 2u)
	{
		return "3";
	}
	else if constexpr (selected_chunk == 3u)
	{
		return "63";
	}
	else if constexpr (selected_chunk == 4u)
	{
		return "64";
	}
	else if constexpr (selected_chunk == 5u)
	{
		return "65";
	}
	else
	{
		return "bulk";
	}
}

[[nodiscard]] inline constexpr char const *allocation_policy_name() noexcept
{
	if constexpr (selected_mode <= 3u)
	{
		return selected_buffer_policy == 0u
				   ? "engine-no-adapter-allocation-default-config"
				   : "engine-no-adapter-allocation-buffer64-inert";
	}
	else if constexpr (selected_mode == 7u)
	{
		return selected_buffer_policy == 0u
				   ? "staged-stack-only-default-config"
				   : "staged-stack-only-buffer64-inert";
	}
	else if constexpr (selected_mode == 6u)
	{
		return selected_buffer_policy == 0u
				   ? "adapter-and-transmit-reconstructed-default-cold"
				   : "adapter-and-transmit-reconstructed-buffer64-cold";
	}
	else
	{
		return selected_buffer_policy == 0u
				   ? "adapter-reconstructed-default-cold"
				   : "adapter-reconstructed-buffer64-cold";
	}
}

[[nodiscard]] inline constexpr char const *comparison_scope_name() noexcept
{
	if constexpr (selected_mode == 0u || selected_mode == 2u)
	{
		return "old-new-engine-member-comparable";
	}
	else if constexpr (selected_mode == 1u || selected_mode == 3u)
	{
		return "new-native-cpo-no-old-cpo";
	}
	else if constexpr (selected_mode == 7u)
	{
		return "new-only-staged-control";
	}
	else
	{
		return "new-only-adapter-stack";
	}
}

[[nodiscard]] inline constexpr char const *tree_name() noexcept
{
	return selected_old_tree ? "official-old-engine" : "new";
}

int run_selected(
	::std::uint_least64_t seed,
	::std::uint_least64_t target_milliseconds)
{
	direction_corpus corpus{};
	build_direction_corpus(corpus, seed);
	::std::uint_least64_t validation_digest{};
	if (!validate_corpus(corpus, validation_digest))
	{
		return 1;
	}

	::std::array<char, direction_storage_size> output_storage{};
	auto timed_call = [&](::std::size_t iteration) -> ::std::uint_least64_t {
		auto const &record{corpus[iteration & (direction_corpus_size - 1u)]};
		auto const result{run_selected_once(record, output_storage)};
		::fast_io_cpo_matrix::compiler_observe_bytes(
			output_storage.data(), result.output_size);
		return observation_signature(result);
	};
	auto reset = []() noexcept {
		/* Every invocation owns fresh engines/adapters; no cross-sample state exists. */
	};
	auto const measured{::fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset, target_milliseconds)};
	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"transcoder-direction,%s,%s,%s,%s,%s,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		tree_name(), mode_name(), chunk_name(), allocation_policy_name(),
		comparison_scope_name(), static_cast<unsigned long long>(seed),
		measured.iterations, elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
	return 0;
}

} // namespace fast_io_state_machine_cpo::transcoder_direction

int main(int argc, char **argv)
{
	::fast_io_cpo_matrix::process_deadline_guard process_deadline;
	if (!process_deadline.armed())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{80u};
	if (argc > 3 ||
		(argc >= 2 && !::fast_io_cpo_matrix::parse_unsigned(argv[1], seed)) ||
		(argc == 3 &&
		 (!::fast_io_cpo_matrix::parse_unsigned(
			  argv[2], target_milliseconds) ||
		  target_milliseconds < 20u || target_milliseconds > 80u)))
	{
		::std::fputs(
			"usage: transcoder_direction_case [decimal-seed] [target-ms:20..80]\n",
			stderr);
		return 2;
	}
	return ::fast_io_state_machine_cpo::transcoder_direction::run_selected(
		seed, target_milliseconds);
}
