#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <fast_io.h>

#include "case_driver.h"
#include "fixture.h"
#include "independent_oracle.h"

#ifndef FAST_IO_STATE_SCAN_INPUT
#define FAST_IO_STATE_SCAN_INPUT 0
#endif

#ifndef FAST_IO_STATE_SCAN_RECEIVER
#define FAST_IO_STATE_SCAN_RECEIVER 0
#endif

namespace fast_io_state_machine_cpo
{

inline constexpr unsigned selected_scan_input{FAST_IO_STATE_SCAN_INPUT};
inline constexpr unsigned selected_scan_receiver{FAST_IO_STATE_SCAN_RECEIVER};
static_assert(selected_scan_input <= 3u);
static_assert(selected_scan_receiver <= 3u);

template <::std::size_t chunk_size>
struct typed_chunk_input;

template <::std::size_t chunk_size>
struct typed_chunk_input_ref
{
	using input_char_type = char;
	typed_chunk_input<chunk_size> *owner{};
};

template <::std::size_t chunk_size>
struct typed_chunk_input
{
	using input_char_type = char;
	char const *data{};
	::std::size_t size{};
	::std::size_t position{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
	char buffer[chunk_size]{};
	char *buffer_current{buffer};
	char *buffer_end{buffer};

	inline constexpr typed_chunk_input(
		char const *source, ::std::size_t source_size) noexcept
		: data(source), size(source_size)
	{}
};

template <::std::size_t chunk_size>
[[nodiscard]] inline typed_chunk_input_ref<chunk_size>
input_stream_ref_define(typed_chunk_input<chunk_size> &input) noexcept
{
	++input.normalizations;
	return {__builtin_addressof(input)};
}

template <::std::size_t chunk_size>
inline char *read_some_underflow_define(
	typed_chunk_input_ref<chunk_size> input, char *first, char *last) noexcept
{
	static_assert(chunk_size == 1u || chunk_size == 3u || chunk_size == 7u);
	++input.owner->primitive_calls;
	auto requested{static_cast<::std::size_t>(last - first)};
	auto const remaining{input.owner->size - input.owner->position};
	if (requested > chunk_size)
	{
		requested = chunk_size;
	}
	if (requested > remaining)
	{
		requested = remaining;
	}
	for (::std::size_t index{}; index != requested; ++index)
	{
		first[index] = input.owner->data[input.owner->position + index];
	}
	input.owner->position += requested;
	return first + requested;
}

template <::std::size_t chunk_size>
[[nodiscard]] inline constexpr char *ibuffer_begin(
	typed_chunk_input_ref<chunk_size> input) noexcept
{
	return input.owner->buffer;
}

template <::std::size_t chunk_size>
[[nodiscard]] inline constexpr char *ibuffer_curr(
	typed_chunk_input_ref<chunk_size> input) noexcept
{
	return input.owner->buffer_current;
}

template <::std::size_t chunk_size>
[[nodiscard]] inline constexpr char *ibuffer_end(
	typed_chunk_input_ref<chunk_size> input) noexcept
{
	return input.owner->buffer_end;
}

template <::std::size_t chunk_size>
inline constexpr void ibuffer_set_curr(
	typed_chunk_input_ref<chunk_size> input, char *position) noexcept
{
	input.owner->buffer_current = position;
}

template <::std::size_t chunk_size>
[[nodiscard]] inline bool ibuffer_underflow(
	typed_chunk_input_ref<chunk_size> input) noexcept
{
	/*
	Let B be the inline window and P the unread physical suffix.  Scan invokes
	underflow only after consuming the previous B, so replacing B with the next
	prefix of P loses no published character.  The typed primitive returns a
	pointer in [begin(B), end(B)]; publishing exactly that half-open range proves
	the ibuffer cursor invariant.  An empty prefix is therefore the sole EOF
	transition, whereas every short nonempty prefix remains ordinary progress.
	*/
	auto const first{input.owner->buffer};
	auto const next{read_some_underflow_define(
		input, first, first + chunk_size)};
	input.owner->buffer_current = first;
	input.owner->buffer_end = next;
	return next != first;
}

struct scan_observation
{
	bool success{};
	bool correct{};
	::std::uint_least64_t signature{};
};

[[nodiscard]] inline constexpr text_record const &selected_scan_text(
	scalar_record const &record) noexcept
{
	if constexpr (selected_scan_receiver == 0u)
	{
		return record.decimal;
	}
	else if constexpr (selected_scan_receiver == 1u)
	{
		return record.hexadecimal;
	}
	else if constexpr (selected_scan_receiver == 2u)
	{
		return record.floating;
	}
	else
	{
		return record.word;
	}
}

template <bool validate, typename input_type>
[[nodiscard]] inline scan_observation scan_receiver_once(
	input_type &input, scalar_record const &record)
{
	/*
	The benchmark's semantic state is the Cartesian product of two independent
	protocols: the input cursor/refill/EOF state and the selected receiver's
	contiguous or context scan state.  No fixture state is allowed to stand in
	for the receiver state, and no receiver is allowed to infer physical EOF
	from a short nonempty chunk.
	*/
	if constexpr (selected_scan_receiver == 0u)
	{
		::std::int_least64_t value{};
		auto const success{::fast_io::io::scan<true>(input, value)};
		return {success,
				!validate || (success && value == record.decimal_value),
				static_cast<::std::uint_least64_t>(value)};
	}
	else if constexpr (selected_scan_receiver == 1u)
	{
		::std::uint_least64_t value{};
		auto const success{::fast_io::io::scan<true>(
			input, ::fast_io::mnp::base_get<16>(value))};
		return {success,
				!validate || (success && value == record.hexadecimal_value), value};
	}
	else if constexpr (selected_scan_receiver == 2u)
	{
		double value{};
		auto const success{::fast_io::io::scan<true>(input, value)};
		return {success,
				!validate ||
					(success && oracle::same_double(value, record.floating_value)),
				oracle::double_bits(value)};
	}
	else
	{
		::std::string value;
		auto const success{::fast_io::io::scan<true>(input, value)};
		compiler_observe_bytes(value.data(), value.size());
		if constexpr (validate)
		{
			auto const correct{success && oracle::equal_bytes(
											  value.data(), value.size(), record.word)};
			return {success, correct, oracle::digest_bytes(UINT64_C(14695981039346656037), value.data(), value.size())};
		}
		else
		{
			return {success, true,
					static_cast<::std::uint_least64_t>(value.size())};
		}
	}
}

template <bool validate>
[[nodiscard]] inline scan_observation scan_selected_once(
	scalar_record const &record)
{
	auto const &text{selected_scan_text(record)};
	if constexpr (selected_scan_input == 0u)
	{
		::fast_io::basic_ibuffer_view<char> input{
			text.bytes.data(), text.bytes.data() + text.size};
		auto result{scan_receiver_once<validate>(input, record)};
		if constexpr (validate)
		{
			/*
			A terminal contiguous source has no hidden physical cursor.  Success at
			EOF therefore entails committing the receiver cursor to the supplied
			half-open range end.
			*/
			result.correct = result.correct &&
							 input.curr_ptr == text.bytes.data() + text.size;
		}
		return result;
	}
	else
	{
		constexpr ::std::size_t chunk_size{
			selected_scan_input == 1u ? 1u
									  : (selected_scan_input == 2u ? 3u : 7u)};
		typed_chunk_input<chunk_size> input{text.bytes.data(), text.size};
		auto result{scan_receiver_once<validate>(input, record)};
		if constexpr (validate)
		{
			/*
			Each nonempty return is only a prefix transition; only a later empty
			return denotes physical EOF.  Full source exhaustion and exactly one
			public normalization jointly verify that the scan state machine did not
			mistake fragmentation for termination or normalize the owner twice.
			*/
			result.correct = result.correct && input.position == text.size &&
							 input.primitive_calls != 0u && input.normalizations == 1u;
		}
		return result;
	}
}

[[nodiscard]] inline constexpr char const *scan_input_name() noexcept
{
	if constexpr (selected_scan_input == 0u)
	{
		return "contiguous";
	}
	else if constexpr (selected_scan_input == 1u)
	{
		return "typed-chunk-1";
	}
	else if constexpr (selected_scan_input == 2u)
	{
		return "typed-chunk-3";
	}
	else
	{
		return "typed-chunk-7";
	}
}

[[nodiscard]] inline constexpr char const *scan_receiver_name() noexcept
{
	if constexpr (selected_scan_receiver == 0u)
	{
		return "int-base10";
	}
	else if constexpr (selected_scan_receiver == 1u)
	{
		return "int-base16";
	}
	else if constexpr (selected_scan_receiver == 2u)
	{
		return "double";
	}
	else
	{
		return "std-string";
	}
}

[[nodiscard]] inline bool validate_scan_corpus(
	scalar_corpus const &corpus, ::std::uint_least64_t &digest)
{
	digest = UINT64_C(14695981039346656037);
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const result{scan_selected_once<true>(corpus[index])};
		if (!result.success || !result.correct)
		{
			::std::fprintf(stderr,
						   "scan preflight failed: input=%s receiver=%s record=%zu\n",
						   scan_input_name(), scan_receiver_name(), index);
			return false;
		}
		digest = (digest ^ result.signature) * UINT64_C(1099511628211);
	}
	return true;
}

} // namespace fast_io_state_machine_cpo

int main(int argc, char **argv)
{
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{80u};
	if (argc > 3 ||
		(argc >= 2 &&
		 !fast_io_state_machine_cpo::parse_unsigned(argv[1], seed)) ||
		(argc == 3 &&
		 (!fast_io_state_machine_cpo::parse_unsigned(
			  argv[2], target_milliseconds) ||
		  !fast_io_state_machine_cpo::valid_target_milliseconds(
			  target_milliseconds))))
	{
		::std::fputs(
			"usage: scan_case [decimal-seed] [target-ms:20..200]\n", stderr);
		return 2;
	}

	fast_io_state_machine_cpo::scalar_corpus corpus;
	fast_io_state_machine_cpo::build_scalar_corpus(corpus, seed);
	::std::uint_least64_t validation_digest{};
	if (!fast_io_state_machine_cpo::oracle::scalar_corpus_is_self_consistent(
			corpus) ||
		!fast_io_state_machine_cpo::validate_scan_corpus(
			corpus, validation_digest))
	{
		return 1;
	}

	auto timed_call = [&](::std::size_t iteration) -> ::std::uint_least64_t {
		auto const result{fast_io_state_machine_cpo::scan_selected_once<false>(
			corpus[iteration &
				   (fast_io_state_machine_cpo::scalar_corpus_size - 1u)])};
		return result.signature ^
			   (result.success ? UINT64_C(0x9e3779b97f4a7c15) : UINT64_C(0));
	};
	auto const measured{fast_io_state_machine_cpo::calibrate_and_measure(
		timed_call, target_milliseconds)};
	auto const seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"scan,%s,%s,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		fast_io_state_machine_cpo::scan_input_name(),
		fast_io_state_machine_cpo::scan_receiver_name(),
		static_cast<unsigned long long>(seed), measured.iterations, seconds,
		nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
}
