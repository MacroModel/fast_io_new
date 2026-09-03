#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>

#include "case_driver.h"
#include "common_outputs.h"
#include "independent_oracle.h"

#ifndef FAST_IO_CPO_MATRIX_OUTPUT
#define FAST_IO_CPO_MATRIX_OUTPUT 0
#endif

namespace
{

using namespace fast_io_cpo_matrix;

inline constexpr unsigned selected_output{FAST_IO_CPO_MATRIX_OUTPUT};
static_assert(selected_output <= 1u);

template <typename Output, typename Pack>
inline void invoke_print(Output &output, Pack const &pack)
{
	::std::apply(
		[&output](auto const &...arguments) {
			::fast_io::operations::print_freestanding<selected_line>(
				output, arguments...);
		},
		pack);
}

[[nodiscard]] bool validate_all(
	corpus_type const &corpus, source_pack_corpus const &packs,
	::std::uint_least64_t &digest) noexcept
{
	digest = UINT64_C(14695981039346656037);
	::std::array<char, oracle::maximum_output_capacity> storage{};
	ring_output_state ring;
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto const expected{oracle::make_expected(corpus[record_index])};
		char const *actual{};
		::std::size_t actual_size{};
		if constexpr (selected_output == 0u)
		{
			::fast_io::basic_obuffer_view<char> output{
				storage.data(), storage.data() + storage.size()};
			invoke_print(output, packs[record_index]);
			actual = storage.data();
			actual_size = output.size();
		}
		else
		{
			ring.reset();
			auto output{make_ring_output(ring)};
			invoke_print(output, packs[record_index]);
			actual = ring.storage.data();
			actual_size = static_cast<::std::size_t>(ring.total_written);
		}
		auto const comparison{oracle::compare(actual, actual_size, expected)};
		if (!comparison.equal)
		{
			::std::fprintf(
				stderr,
				"print preflight mismatch: record=%zu byte=%zu actual=%zu expected=%zu\n",
				record_index, comparison.mismatch, actual_size, expected.size);
			return false;
		}
		digest = oracle::digest_bytes(digest, actual, actual_size);
	}
	return true;
}

[[nodiscard]] constexpr char const *output_name() noexcept
{
	return selected_output == 0u ? "obuffer" : "write-all-ring";
}

} // namespace

int main(int argc, char **argv)
{
	fast_io_cpo_matrix::process_deadline_guard process_deadline;
	if (!process_deadline.armed())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{150u};
	if (argc > 3 ||
		(argc >= 2 && !fast_io_cpo_matrix::parse_unsigned(argv[1], seed)) ||
		(argc == 3 &&
		 (!fast_io_cpo_matrix::parse_unsigned(argv[2], target_milliseconds) ||
		  target_milliseconds < 20u || target_milliseconds > 300u)))
	{
		::std::fputs("usage: print_case [decimal-seed] [target-ms:20..300]\n", stderr);
		return 2;
	}

	fast_io_cpo_matrix::corpus_type corpus;
	fast_io_cpo_matrix::build_corpus(corpus, seed);
	fast_io_cpo_matrix::source_pack_corpus packs;
	fast_io_cpo_matrix::build_source_packs(corpus, packs);
	::std::uint_least64_t validation_digest{};
	if (!validate_all(corpus, packs, validation_digest))
	{
		return 1;
	}

	alignas(64)::std::array<
		char, fast_io_cpo_matrix::oracle::maximum_output_capacity>
		output_storage{};
	fast_io_cpo_matrix::ring_output_state ring;
	auto timed_call = [&](::std::size_t iteration) -> ::std::size_t {
		auto const record_index{iteration &
								(fast_io_cpo_matrix::corpus_size - 1u)};
		if constexpr (selected_output == 0u)
		{
			::fast_io::basic_obuffer_view<char> output{
				output_storage.data(), output_storage.data() + output_storage.size()};
			invoke_print(output, packs[record_index]);
			auto const size{output.size()};
			fast_io_cpo_matrix::compiler_observe_bytes(
				output_storage.data(), size);
			return size;
		}
		else
		{
			auto output{fast_io_cpo_matrix::make_ring_output(ring)};
			auto const before{ring.total_written};
			invoke_print(output, packs[record_index]);
			fast_io_cpo_matrix::compiler_observe_bytes(
				ring.storage.data(), ring.storage.size());
			return static_cast<::std::size_t>(ring.total_written - before);
		}
	};
	auto reset_timed_state = [&]() noexcept {
		output_storage.fill(char{});
		ring.reset();
	};
	auto const measured{fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset_timed_state, target_milliseconds)};
	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	auto timed_digest{fast_io_cpo_matrix::oracle::digest_bytes(
		UINT64_C(14695981039346656037), output_storage.data(),
		output_storage.size())};
	if constexpr (selected_output == 1u)
	{
		timed_digest = fast_io_cpo_matrix::oracle::digest_bytes(
			UINT64_C(14695981039346656037), ring.storage.data(),
			ring.storage.size());
	}
	::std::printf(
		"print,%s,%zu,%u,%s,%llu,%zu,%.9f,%.3f,%llu,%llu,%llu\n",
		fast_io_cpo_matrix::source_family_name(),
		fast_io_cpo_matrix::selected_pack_count,
		fast_io_cpo_matrix::selected_line ? 1u : 0u, output_name(),
		static_cast<unsigned long long>(seed), measured.iterations,
		elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest),
		static_cast<unsigned long long>(timed_digest));
}
