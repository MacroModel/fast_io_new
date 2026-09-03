#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <fast_io.h>

#include "case_driver.h"
#include "fixture.h"
#include "independent_oracle.h"

#ifndef FAST_IO_STATE_TRANSCODER_MODE
#define FAST_IO_STATE_TRANSCODER_MODE 0
#endif

namespace fast_io_state_machine_cpo
{

inline constexpr unsigned selected_transcoder_mode{
	FAST_IO_STATE_TRANSCODER_MODE};
static_assert(selected_transcoder_mode <= 1u);

struct transcode_observation
{
	bool valid{};
	::std::size_t output_size{};
	::std::size_t logical_units{};
};

[[nodiscard]] inline transcode_observation run_adapter_pipeline(
	transcode_record const &record,
	::std::array<char, maximum_encoded_size> &output_storage)
{
	::fast_io::basic_ibuffer_view<char> physical_input{
		record.encoded.data(), record.encoded.data() + record.encoded_size};
	::fast_io::basic_obuffer_view<char> physical_output{
		output_storage.data(), output_storage.data() + output_storage.size()};
	auto input{::fast_io::make_itranscoder(
		physical_input, ::fast_io::transcoders::crlf_to_lf{})};
	auto output{::fast_io::make_otranscoder(
		physical_output, ::fast_io::transcoders::lf_to_crlf{})};

	/*
	This is a real streaming composition rather than a direct engine call:
	physical CRLF input is decoded by the input adapter, each published logical
	unit is transferred through the ordinary typed transmit CPO, and the output
	adapter re-encodes it while the transfer is still in progress.

	The concrete EOL engines are transformations, not cryptographic engines.
	Moreover, the current input adapter publishes in `streaming_unverified` mode;
	terminal success validates only the engine protocol and cannot be cited as
	authenticated-before-publication behavior for a future AEAD transcoder.

	The lvalue output adapter is not a temporary message owner, so transmit does
	not terminally commit it.  Explicit finish is therefore part of the measured
	operation and is the only point that may commit an engine trailer or terminal
	state.  Input transmit reaches logical EOF and consequently drives engine
	finish; the following drain-and-finish is an idempotent assertion that no
	unvalidated suffix remains.
	*/
	auto const transmitted{
		::fast_io::operations::transmit_until_eof(output, input)};
	::fast_io::operations::output_stream_finish(output);
	::fast_io::operations::input_stream_drain_and_finish(input);
	return {true, physical_output.size(),
			static_cast<::std::size_t>(transmitted.transmitted)};
}

[[nodiscard]] inline transcode_observation run_staged_control(
	transcode_record const &record,
	::std::array<char, maximum_encoded_size> &output_storage) noexcept
{
	::std::array<char, maximum_logical_size> intermediate{};
	::std::size_t logical_size{};
	for (::std::size_t index{}; index != record.encoded_size; ++index)
	{
		auto const character{record.encoded[index]};
		if (character == '\r')
		{
			if (index + 1u == record.encoded_size ||
				record.encoded[index + 1u] != '\n')
			{
				return {false, 0u, 0u};
			}
			++index;
			intermediate[logical_size++] = '\n';
		}
		else
		{
			intermediate[logical_size++] = character;
		}
	}

	/*
	The compiler barrier forces the control to materialize the complete decoded
	message before the encoding pass.  It performs no semantic validation and
	therefore remains a staged baseline rather than an oracle inside timing.
	*/
	compiler_observe_bytes(intermediate.data(), logical_size);
	::std::size_t output_size{};
	for (::std::size_t index{}; index != logical_size; ++index)
	{
		auto const character{intermediate[index]};
		if (character == '\n')
		{
			output_storage[output_size++] = '\r';
		}
		output_storage[output_size++] = character;
	}
	return {true, output_size, logical_size};
}

[[nodiscard]] inline constexpr char const *transcoder_mode_name() noexcept
{
	return selected_transcoder_mode == 0u
			   ? "adapter-streaming"
			   : "staged-control";
}

[[nodiscard]] inline bool validate_transcoder_corpus(
	transcode_corpus const &corpus, ::std::uint_least64_t &digest)
{
	digest = UINT64_C(14695981039346656037);
	::std::array<char, maximum_encoded_size> streaming_output{};
	::std::array<char, maximum_encoded_size> staged_output{};
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const &record{corpus[index]};
		if (!oracle::transcode_record_is_canonical(record))
		{
			::std::fprintf(
				stderr, "transcoder fixture is noncanonical: record=%zu\n", index);
			return false;
		}
		auto const streaming{run_adapter_pipeline(record, streaming_output)};
		auto const staged{run_staged_control(record, staged_output)};
		bool const equivalent{
			streaming.valid && staged.valid &&
			streaming.logical_units == record.logical_size &&
			staged.logical_units == record.logical_size &&
			streaming.output_size == record.encoded_size &&
			staged.output_size == record.encoded_size &&
			oracle::equal_bytes(streaming_output.data(), streaming.output_size,
								record.encoded.data(), record.encoded_size) &&
			oracle::equal_bytes(staged_output.data(), staged.output_size,
								record.encoded.data(), record.encoded_size) &&
			oracle::equal_bytes(streaming_output.data(), streaming.output_size,
								staged_output.data(), staged.output_size)};
		if (!equivalent)
		{
			::std::fprintf(stderr,
						   "transcoder preflight failed: record=%zu logical=%zu encoded=%zu\n",
						   index, record.logical_size, record.encoded_size);
			return false;
		}
		digest = oracle::digest_bytes(
			digest, streaming_output.data(), streaming.output_size);
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
			"usage: transcoder_case [decimal-seed] [target-ms:20..200]\n",
			stderr);
		return 2;
	}

	fast_io_state_machine_cpo::transcode_corpus corpus;
	fast_io_state_machine_cpo::build_transcode_corpus(corpus, seed);
	::std::uint_least64_t validation_digest{};
	if (!fast_io_state_machine_cpo::validate_transcoder_corpus(
			corpus, validation_digest))
	{
		return 1;
	}

	::std::array<char, fast_io_state_machine_cpo::maximum_encoded_size>
		output_storage{};
	auto timed_call = [&](::std::size_t iteration) -> ::std::uint_least64_t {
		auto const &record{corpus[iteration &
								  (fast_io_state_machine_cpo::transcode_corpus_size - 1u)]};
		fast_io_state_machine_cpo::transcode_observation result;
		if constexpr (
			fast_io_state_machine_cpo::selected_transcoder_mode == 0u)
		{
			result = fast_io_state_machine_cpo::run_adapter_pipeline(
				record, output_storage);
		}
		else
		{
			result = fast_io_state_machine_cpo::run_staged_control(
				record, output_storage);
		}
		fast_io_state_machine_cpo::compiler_observe_bytes(
			output_storage.data(), result.output_size);
		return static_cast<::std::uint_least64_t>(result.output_size) ^
			   (static_cast<::std::uint_least64_t>(result.logical_units) << 32u) ^
			   (result.valid ? UINT64_C(0x9e3779b97f4a7c15) : UINT64_C(0));
	};
	auto const measured{fast_io_state_machine_cpo::calibrate_and_measure(
		timed_call, target_milliseconds)};
	auto const seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"transcoder,%s,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		fast_io_state_machine_cpo::transcoder_mode_name(),
		static_cast<unsigned long long>(seed), measured.iterations, seconds,
		nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
}
