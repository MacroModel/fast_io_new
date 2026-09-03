#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>

#include <fast_io_dsal/string.h>
#include <fast_io_unit/string.h>

#include "case_driver.h"
#include "independent_oracle.h"

#ifndef FAST_IO_CPO_MATRIX_RESULT
#define FAST_IO_CPO_MATRIX_RESULT 0
#endif

namespace
{

using namespace fast_io_cpo_matrix;

inline constexpr unsigned selected_result{FAST_IO_CPO_MATRIX_RESULT};
static_assert(selected_result <= 1u);

template <typename Pack>
#if defined(FAST_IO_CPO_MATRIX_NOINLINE_PROBE)
/*
The assembly probe deliberately keeps the public concat front door as one
symbol.  Normal timing builds leave the marker absent, so this diagnostic
boundary cannot perturb their inliner budget or hot path.
*/
[[gnu::noinline]]
#endif
[[nodiscard]] inline auto invoke_concat(Pack const &pack)
{
	return ::std::apply(
		[](auto const &...arguments) {
			if constexpr (selected_result == 0u)
			{
				if constexpr (selected_line)
				{
					return ::fast_io::concatln_std(arguments...);
				}
				else
				{
					return ::fast_io::concat_std(arguments...);
				}
			}
			else
			{
				if constexpr (selected_line)
				{
					return ::fast_io::concatln_fast_io(arguments...);
				}
				else
				{
					return ::fast_io::concat_fast_io(arguments...);
				}
			}
		},
		pack);
}

[[nodiscard]] bool validate_all(
	corpus_type const &corpus, source_pack_corpus const &packs,
	::std::uint_least64_t &digest) noexcept
{
	digest = UINT64_C(14695981039346656037);
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto const expected{oracle::make_expected(corpus[record_index])};
		auto result{invoke_concat(packs[record_index])};
		auto const comparison{
			oracle::compare(result.data(), result.size(), expected)};
		if (!comparison.equal)
		{
			::std::fprintf(
				stderr,
				"concat preflight mismatch: record=%zu byte=%zu actual=%zu expected=%zu\n",
				record_index, comparison.mismatch,
				static_cast<::std::size_t>(result.size()), expected.size);
			return false;
		}
		digest = oracle::digest_bytes(digest, result.data(), result.size());
	}
	return true;
}

[[nodiscard]] constexpr char const *result_name() noexcept
{
	return selected_result == 0u ? "std-string" : "fast-io-string";
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
		::std::fputs("usage: concat_case [decimal-seed] [target-ms:20..300]\n", stderr);
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

	auto timed_call = [&](::std::size_t iteration) -> ::std::size_t {
		auto const record_index{iteration &
								(fast_io_cpo_matrix::corpus_size - 1u)};
		auto result{invoke_concat(packs[record_index])};
		fast_io_cpo_matrix::compiler_observe_bytes(result.data(), result.size());
		return result.size();
	};
	auto reset_timed_state = []() noexcept {};
	auto const measured{fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset_timed_state, target_milliseconds)};
	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"concat,%s,%zu,%u,%s,%llu,%zu,%.9f,%.3f,%llu,%llu,0\n",
		fast_io_cpo_matrix::source_family_name(),
		fast_io_cpo_matrix::selected_pack_count,
		fast_io_cpo_matrix::selected_line ? 1u : 0u, result_name(),
		static_cast<unsigned long long>(seed), measured.iterations,
		elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
}
