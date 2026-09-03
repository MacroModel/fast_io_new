#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/time.h>
#endif

#include <fast_io_core.h>

#ifndef FAST_IO_OLD_NEW_TRANSMIT_KIND
#define FAST_IO_OLD_NEW_TRANSMIT_KIND 3
#endif

#ifndef FAST_IO_OLD_NEW_TRANSMIT_CHUNK
#define FAST_IO_OLD_NEW_TRANSMIT_CHUNK 23
#endif

#ifndef FAST_IO_OLD_NEW_TRANSMIT_OUTPUT
#define FAST_IO_OLD_NEW_TRANSMIT_OUTPUT 0
#endif

namespace
{

inline constexpr unsigned selected_kind{FAST_IO_OLD_NEW_TRANSMIT_KIND};
inline constexpr ::std::size_t selected_chunk_size{
	FAST_IO_OLD_NEW_TRANSMIT_CHUNK};
inline constexpr unsigned selected_output{FAST_IO_OLD_NEW_TRANSMIT_OUTPUT};
inline constexpr ::std::size_t storage_size{4096u};
static_assert(selected_kind <= 5u);
static_assert(selected_chunk_size != 0u && selected_chunk_size <= storage_size);
static_assert(selected_output <= 1u);

#if __has_cpp_attribute(__gnu__::__noinline__)
#define FAST_IO_OLD_NEW_NOINLINE [[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
#define FAST_IO_OLD_NEW_NOINLINE [[msvc::noinline]]
#else
#define FAST_IO_OLD_NEW_NOINLINE
#endif

struct input_state
{
	::std::array<::std::byte, storage_size> storage{};
	::std::size_t position{};
	::std::size_t limit{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

struct output_state
{
	::std::array<::std::byte, storage_size> storage{};
	::std::size_t position{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
};

struct input_ref
{
	using input_char_type = char;
	input_state *state{};
};

struct output_ref
{
	using output_char_type = char;
	output_state *state{};
};

inline input_ref input_stream_ref_define(input_state &state) noexcept
{
	++state.normalizations;
	return {__builtin_addressof(state)};
}

/* The fixed-obuffer cells bypass raw-output normalization by construction. */
[[maybe_unused]] inline output_ref
output_stream_ref_define(output_state &state) noexcept
{
	++state.normalizations;
	return {__builtin_addressof(state)};
}

/*
Each executable selects exactly one transfer grammar.  The remaining ADL
primitives intentionally stay available in this shared source so that every
old/new cell differs only by macros and include root, not by fixture shape.
*/
[[maybe_unused]] inline char *read_some_underflow_define(
	input_ref input, char *first, char *last) noexcept
{
	++input.state->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.state->limit - input.state->position};
	if (count > selected_chunk_size)
	{
		count = selected_chunk_size;
	}
	if (count > remaining)
	{
		count = remaining;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = static_cast<char>(::std::to_integer<unsigned char>(
			input.state->storage[input.state->position + index]));
	}
	input.state->position += count;
	return first + count;
}

[[maybe_unused]] inline void read_all_underflow_define(input_ref input, char *first, char *last) noexcept
{
	++input.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	if (input.state->limit - input.state->position < count)
	{
		::std::abort();
	}
	/* Exact-read semantics are independent of the partial-read chunk policy.
	   One successful primitive publishes the complete requested interval. */
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = static_cast<char>(::std::to_integer<unsigned char>(
			input.state->storage[input.state->position + index]));
	}
	input.state->position += count;
}

[[maybe_unused]] inline ::std::byte *read_some_bytes_underflow_define(
	input_ref input, ::std::byte *first, ::std::byte *last) noexcept
{
	++input.state->primitive_calls;
	auto count{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.state->limit - input.state->position};
	if (count > selected_chunk_size)
	{
		count = selected_chunk_size;
	}
	if (count > remaining)
	{
		count = remaining;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = input.state->storage[input.state->position + index];
	}
	input.state->position += count;
	return first + count;
}

[[maybe_unused]] inline void read_all_bytes_underflow_define(
	input_ref input, ::std::byte *first, ::std::byte *last) noexcept
{
	++input.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	if (input.state->limit - input.state->position < count)
	{
		::std::abort();
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		first[index] = input.state->storage[input.state->position + index];
	}
	input.state->position += count;
}

[[maybe_unused]] inline void write_all_overflow_define(
	output_ref output, char const *first, char const *last) noexcept
{
	++output.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	if (storage_size - output.state->position < count)
	{
		::std::abort();
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		output.state->storage[output.state->position + index] =
			static_cast<::std::byte>(static_cast<unsigned char>(first[index]));
	}
	output.state->position += count;
}

[[maybe_unused]] inline void write_all_bytes_overflow_define(
	output_ref output, ::std::byte const *first, ::std::byte const *last) noexcept
{
	++output.state->primitive_calls;
	auto const count{static_cast<::std::size_t>(last - first)};
	if (storage_size - output.state->position < count)
	{
		::std::abort();
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		output.state->storage[output.state->position + index] = first[index];
	}
	output.state->position += count;
}

[[nodiscard]] ::std::size_t parse_size(char const *text) noexcept
{
	if (text == nullptr || *text == '\0')
	{
		return 0u;
	}
	::std::size_t value{};
	for (; *text != '\0'; ++text)
	{
		auto const digit{static_cast<unsigned char>(*text) - static_cast<unsigned>('0')};
		if (9u < digit || value > (::std::numeric_limits<::std::size_t>::max() - digit) / 10u)
		{
			return 0u;
		}
		value = value * 10u + digit;
	}
	return value;
}

template <typename output_type>
inline ::fast_io::uintfpos_t invoke_selected_transmit(
	output_type &output, input_state &input, ::fast_io::uintfpos_t requested)
{
	::fast_io::uintfpos_t reported{requested};
#if FAST_IO_OLD_NEW_TRANSMIT_KIND == 0
	::fast_io::operations::transmit_all(output, input, requested);
#elif FAST_IO_OLD_NEW_TRANSMIT_KIND == 1
	reported = ::fast_io::operations::transmit_some(output, input, requested);
#elif FAST_IO_OLD_NEW_TRANSMIT_KIND == 2
	::fast_io::operations::transmit_bytes_all(output, input, requested);
#elif FAST_IO_OLD_NEW_TRANSMIT_KIND == 3
	reported = ::fast_io::operations::transmit_bytes_some(output, input, requested);
#elif FAST_IO_OLD_NEW_TRANSMIT_KIND == 4
	reported = ::fast_io::operations::transmit_until_eof(output, input).transmitted;
#else
	reported =
		::fast_io::operations::transmit_bytes_until_eof(output, input).transmitted;
#endif
	return reported;
}

FAST_IO_OLD_NEW_NOINLINE ::std::uint_least64_t transmit_once(
	input_state &input, output_state &output, ::fast_io::uintfpos_t available,
	::fast_io::uintfpos_t requested)
{
	input.position = 0u;
	input.limit = static_cast<::std::size_t>(available);
	output.position = 0u;
	::fast_io::uintfpos_t reported{};
#if FAST_IO_OLD_NEW_TRANSMIT_OUTPUT == 0
	reported = invoke_selected_transmit(output, input, requested);
#else
	auto first{reinterpret_cast<char *>(output.storage.data())};
	::fast_io::basic_obuffer_view<char> output_view{
		first, first + output.storage.size()};
	reported = invoke_selected_transmit(output_view, input, requested);
	output.position = output_view.size();
#endif
#if defined(__GNUC__) || defined(__clang__)
	/*
	The entire produced byte range escapes through the opaque memory operand.
	This proves that the compiler must retain every staged read and write without
	adding a digest loop to the timed transfer itself.
	*/
	__asm__ __volatile__("" : : "m"(output.storage) : "memory");
#endif
	return static_cast<::std::uint_least64_t>(reported) ^
		   (static_cast<::std::uint_least64_t>(output.position) << 17u);
}

[[nodiscard]] bool validate_once(
	input_state &input, output_state &output, ::std::size_t available,
	::std::size_t requested) noexcept
{
	auto const before_input_calls{input.primitive_calls};
	auto const before_output_calls{output.primitive_calls};
	auto const reported{transmit_once(input, output, available, requested)};
	/*
	For exact transfer, AVAILABLE >= REQUESTED is a precondition.  A bounded
	`some` transfer publishes min(AVAILABLE, REQUESTED), whereas EOF transfer
	drains AVAILABLE and ignores the count bound.  Fragmentation never changes
	those logical extents: every nonempty short read is progress, not EOF.
	*/
	auto const expected_size{
		selected_kind == 1u || selected_kind == 3u
			? (available < requested ? available : requested)
			: (selected_kind >= 4u ? available : requested)};
	auto const expected_report{static_cast<::std::uint_least64_t>(expected_size) ^
							   (static_cast<::std::uint_least64_t>(expected_size) << 17u)};
	if (reported != expected_report || input.position != expected_size ||
		output.position != expected_size)
	{
		::std::fprintf(
			stderr,
			"transmit extent mismatch: kind=%u output=%u available=%zu requested=%zu packed=%llu expected-packed=%llu input=%zu output-size=%zu expected-size=%zu\n",
			selected_kind, selected_output, available, requested,
			static_cast<unsigned long long>(reported),
			static_cast<unsigned long long>(expected_report), input.position,
			output.position, expected_size);
		return false;
	}
	for (::std::size_t index{}; index != expected_size; ++index)
	{
		if (input.storage[index] != output.storage[index])
		{
			return false;
		}
	}
	/*
	Exact/some zero-count calls know their closed interval in advance and permit
	no primitive data-plane call.  EOF transfer has no count parameter: even an
	empty logical input requires exactly one read returning no progress.  For a
	nonempty EOF transfer, ceil(size/chunk) progress reads plus that terminal read
	prove that short chunks were not mistaken for EOF.  A fixed obuffer performs
	no output primitive; its cursor is the publication witness instead.
	*/
	::std::size_t const progress_reads{
		expected_size == 0u ? 0u
							: (expected_size + selected_chunk_size - 1u) /
								  selected_chunk_size};
	::std::size_t expected_input_delta{};
	if (selected_kind >= 4u)
	{
		expected_input_delta = progress_reads + 1u;
	}
	else if (selected_kind == 1u || selected_kind == 3u)
	{
		/* A bounded loop probes EOF only when availability ends before its bound. */
		expected_input_delta =
			progress_reads + static_cast<::std::size_t>(available < requested);
	}
	else
	{
		expected_input_delta = expected_size == 0u ? 0u : 1u;
	}
	::std::size_t const expected_output_delta{
		selected_output == 0u
			? ((selected_kind == 1u || selected_kind == 3u || selected_kind >= 4u)
				   ? progress_reads
				   : (expected_size == 0u ? 0u : 1u))
			: 0u};
	auto const actual_input_delta{input.primitive_calls - before_input_calls};
	auto const actual_output_delta{output.primitive_calls - before_output_calls};
	if (actual_input_delta != expected_input_delta ||
		actual_output_delta != expected_output_delta)
	{
		::std::fprintf(
			stderr,
			"transmit primitive mismatch: kind=%u output=%u available=%zu requested=%zu reads=%zu expected-reads=%zu writes=%zu expected-writes=%zu\n",
			selected_kind, selected_output, available, requested,
			actual_input_delta, expected_input_delta, actual_output_delta,
			expected_output_delta);
		return false;
	}
	return true;
}

[[nodiscard]] bool arm_process_deadline() noexcept
{
#if defined(__unix__) || defined(__APPLE__)
	/*
	A process-local real-time deadline bounds fixture validation and the requested
	iteration loop without adding a watchdog thread to the single-task M4 run.
	The timer is deliberately stricter than one second; expiration invalidates
	the cell instead of allowing thermal or scheduler drift into later samples.
	*/
	::itimerval timer{};
	timer.it_value.tv_usec = 800000;
	return ::setitimer(ITIMER_REAL, __builtin_addressof(timer), nullptr) == 0;
#else
	return true;
#endif
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 4)
	{
		::std::fprintf(
			stderr,
			"usage: transmit_matrix AVAILABLE [REQUESTED] ITERATIONS\n");
		return 2;
	}
	auto const available{parse_size(argv[1])};
	auto const requested{argc == 4 ? parse_size(argv[2]) : available};
	auto const iterations{parse_size(argv[argc - 1])};
	if (storage_size < available || storage_size < requested ||
		iterations == 0u ||
		(selected_kind != 1u && selected_kind != 3u && selected_kind < 4u &&
		 available < requested))
	{
		return 2;
	}
	if (!arm_process_deadline())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	input_state input{};
	output_state output{};
	auto random{UINT64_C(0x9e3779b97f4a7c15)};
	for (auto &value : input.storage)
	{
		random ^= random << 7u;
		random ^= random >> 9u;
		value = static_cast<::std::byte>(random);
	}
	if (!validate_once(input, output, available, requested))
	{
		return 3;
	}

	::std::uint_least64_t checksum{};
	auto const start{::std::chrono::steady_clock::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		checksum += transmit_once(input, output, available, requested) + index;
	}
	auto const end{::std::chrono::steady_clock::now()};
	auto const elapsed{
		::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - start).count()};
	::std::printf("kind=%u,chunk=%zu,output=%u,available=%zu,requested=%zu,iterations=%zu,ns=%lld,checksum=%llu,reads=%zu,writes=%zu,normalizations=%zu\n",
				  selected_kind, selected_chunk_size, selected_output, available,
				  requested, iterations, static_cast<long long>(elapsed),
				  static_cast<unsigned long long>(checksum), input.primitive_calls,
				  output.primitive_calls, input.normalizations + output.normalizations);
}
