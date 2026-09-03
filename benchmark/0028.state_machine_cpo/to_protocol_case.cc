#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../0026.cpo_matrix/case_driver.h"
#include "../0026.cpo_matrix/common_sources.h"

#ifndef FAST_IO_TO_PROTOCOL_FRONTDOOR
#define FAST_IO_TO_PROTOCOL_FRONTDOOR 0
#endif

#ifndef FAST_IO_TO_PROTOCOL_TARGET
#define FAST_IO_TO_PROTOCOL_TARGET 0
#endif

#ifndef FAST_IO_TO_PROTOCOL_MODE
#define FAST_IO_TO_PROTOCOL_MODE 0
#endif

namespace fast_io_to_protocol
{

/*
One translation unit denotes one protocol cell. The macro axes are therefore
part of the experiment's type identity: a compiler cannot share instantiations,
inliner budgets, or dead protocol branches across cells. Runtime validation is
performed before calibration and the measured closure calls only the selected
public conversion front door.
*/

inline constexpr unsigned selected_frontdoor{FAST_IO_TO_PROTOCOL_FRONTDOOR};
inline constexpr unsigned selected_target{FAST_IO_TO_PROTOCOL_TARGET};
inline constexpr unsigned selected_mode{FAST_IO_TO_PROTOCOL_MODE};
static_assert(selected_frontdoor <= 1u);
static_assert(selected_target <= 1u);
static_assert(selected_mode <= 1u);
static_assert(selected_mode == 0u ||
				  ::fast_io_cpo_matrix::selected_source_family ==
					  ::fast_io_cpo_matrix::source_family::fixed_reserve,
			  "literal mode uses source id zero because its arguments are spelled at the call site");

inline constexpr ::std::uint_least64_t fnv_offset{
	UINT64_C(14695981039346656037)};
inline constexpr ::std::uint_least64_t fnv_prime{UINT64_C(1099511628211)};
inline constexpr ::std::array<::std::size_t, 8u> decimal_sizes{
	1u, 2u, 3u, 7u, 8u, 15u, 17u, 18u};

struct context_target
{
	::std::uint_least64_t value{};
	::std::uint_least64_t digest{};
	::std::size_t size{};
	::std::size_t context_calls{};
	::std::size_t nonempty_context_calls{};
	::std::size_t empty_context_calls{};
	::std::size_t eof_calls{};
};

struct context_target_ref
{
	context_target *destination{};
};

struct context_state
{
	::std::uint_least64_t value{};
	::std::uint_least64_t digest{fnv_offset};
	::std::size_t size{};
	::std::size_t context_calls{};
	::std::size_t nonempty_context_calls{};
	::std::size_t empty_context_calls{};
	::std::size_t eof_calls{};
};

inline constexpr context_target_ref scan_alias_define(
	::fast_io::io_alias_t, context_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::io_type_t<context_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, context_target_ref>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, context_target_ref>,
	context_state &state, char const *first, char const *last,
	context_target_ref) noexcept
{
	++state.context_calls;
	if (first == last)
	{
		++state.empty_context_calls;
	}
	else
	{
		++state.nonempty_context_calls;
	}
	for (auto current{first}; current != last; ++current)
	{
		auto const character{static_cast<unsigned char>(*current)};
		if (character < static_cast<unsigned char>('0') ||
			character > static_cast<unsigned char>('9'))
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		auto const digit{static_cast<::std::uint_least64_t>(
			character - static_cast<unsigned char>('0'))};
		if (state.value >
			((::std::numeric_limits<::std::uint_least64_t>::max)() -
			 digit) /
				10u)
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		state.value = state.value * 10u + digit;
		state.digest = (state.digest ^ character) * fnv_prime;
		++state.size;
	}
	// Every source fragment is a nonterminal transition. Only the single EOF
	// CPO below may publish the accumulated value into the destination object.
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, context_target_ref>,
	context_state &state, context_target_ref target_ref) noexcept
{
	++state.eof_calls;
	target_ref.destination->value = state.value;
	target_ref.destination->digest = state.digest;
	target_ref.destination->size = state.size;
	target_ref.destination->context_calls = state.context_calls;
	target_ref.destination->nonempty_context_calls =
		state.nonempty_context_calls;
	target_ref.destination->empty_context_calls = state.empty_context_calls;
	target_ref.destination->eof_calls = state.eof_calls;
	return state.size == 0u ? ::fast_io::parse_code::end_of_file
							: ::fast_io::parse_code::ok;
}

[[nodiscard]] inline constexpr ::std::true_type scan_context_result_in_range(
	::fast_io::io_reserve_type_t<char, context_target_ref>) noexcept
{
	/*
	For every state and input span the ordinary transition returns either the
	current invalid byte or `last`. Both cursors are in the supplied closed range;
	the marker therefore removes only redundant range validation and changes no
	parse or target semantics.
	*/
	return {};
}

using builtin_target_type = ::std::uint_least64_t;
using builtin_target_ref = decltype(::fast_io::scan_alias_define(
	::fast_io::io_alias, ::std::declval<builtin_target_type &>()));
static_assert(::fast_io::contiguous_scannable<char, builtin_target_ref>);
static_assert(::fast_io::context_scannable<char, context_target_ref>);

struct expected_record
{
	bool valid{};
	::std::uint_least64_t value{};
	::std::uint_least64_t digest{fnv_offset};
	::std::size_t size{};
	::std::size_t nonempty_fragments{};
};

struct observation
{
	::std::uint_least64_t value{};
	::std::uint_least64_t digest{};
	::std::size_t size{};
	::std::size_t context_calls{};
	::std::size_t nonempty_context_calls{};
	::std::size_t empty_context_calls{};
	::std::size_t eof_calls{};
};

[[nodiscard]] inline constexpr observation make_builtin_observation(
	::std::uint_least64_t value) noexcept
{
	observation result{};
	result.value = value;
	return result;
}

[[nodiscard]] inline constexpr ::std::uint_least64_t observation_signature(
	observation const &result) noexcept
{
	/*
	A public conversion may legally coalesce adjacent printable fragments or
	split one fragment before invoking the target context CPO. Consequently the
	primitive-call counters diagnose the selected implementation schedule but are
	not part of the conversion's semantic observation. Keeping them out of the
	timed checksum and validation digest makes old/new results comparable whenever
	they publish the same value, byte digest, size, and unique EOF transition.
	*/
	auto signature{result.value ^ result.digest};
	signature ^= static_cast<::std::uint_least64_t>(result.size) << 1u;
	signature ^= static_cast<::std::uint_least64_t>(result.eof_calls) << 49u;
	return signature;
}

inline void build_decimal_corpus(
	::fast_io_cpo_matrix::corpus_type &corpus,
	::std::uint_least64_t seed) noexcept
{
	auto random{seed ^ UINT64_C(0x6a09e667f3bcc909)};
	auto const size_offset{static_cast<::std::size_t>(seed) & 7u};
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto &record{corpus[record_index]};
		for (auto &token : record.tokens)
		{
			token.size = 0u;
		}
		::std::array<char, 18u> digits{};
		auto const total_size{
			decimal_sizes[(record_index + size_offset) % decimal_sizes.size()]};
		for (::std::size_t index{}; index != total_size; ++index)
		{
			random = ::fast_io_cpo_matrix::xorshift64(
				random + UINT64_C(0xbb67ae8584caa73b));
			auto const digit{index == 0u ? 1u + random % 9u : random % 10u};
			digits[index] = static_cast<char>(
				static_cast<unsigned char>('0') +
				static_cast<unsigned char>(digit));
		}

		/*
		The total spelling never exceeds eighteen decimal digits, so the built-in
		uint64 target is valid for every pack. Pack 32 intentionally contains empty
		suffix fragments: increasing argument count must not require inventing a
		thirty-two-digit value outside the target domain.
		*/
		auto const base_size{
			total_size / ::fast_io_cpo_matrix::selected_pack_count};
		auto const remainder{
			total_size % ::fast_io_cpo_matrix::selected_pack_count};
		::std::size_t digit_index{};
		for (::std::size_t token_index{};
			 token_index != ::fast_io_cpo_matrix::selected_pack_count;
			 ++token_index)
		{
			auto &token{record.tokens[token_index]};
			token.size = base_size + (token_index < remainder ? 1u : 0u);
			for (::std::size_t index{}; index != token.size; ++index)
			{
				token.bytes[index] = digits[digit_index++];
			}
		}
	}
}

[[nodiscard]] inline constexpr expected_record make_expected(
	::fast_io_cpo_matrix::corpus_record const &record) noexcept
{
	expected_record expected{};
	expected.valid = true;
	for (::std::size_t token_index{};
		 token_index != ::fast_io_cpo_matrix::selected_pack_count;
		 ++token_index)
	{
		auto const &token{record.tokens[token_index]};
		if (token.size != 0u)
		{
			++expected.nonempty_fragments;
		}
		for (::std::size_t index{}; index != token.size; ++index)
		{
			auto const character{static_cast<unsigned char>(token.bytes[index])};
			if (character < static_cast<unsigned char>('0') ||
				character > static_cast<unsigned char>('9'))
			{
				expected.valid = false;
				return expected;
			}
			auto const digit{static_cast<::std::uint_least64_t>(
				character - static_cast<unsigned char>('0'))};
			if (expected.value >
				((::std::numeric_limits<::std::uint_least64_t>::max)() -
				 digit) /
					10u)
			{
				expected.valid = false;
				return expected;
			}
			expected.value = expected.value * 10u + digit;
			expected.digest = (expected.digest ^ character) * fnv_prime;
			++expected.size;
		}
	}
	return expected;
}

template <typename pack_type>
[[nodiscard]] inline observation invoke_runtime_once(pack_type const &pack)
{
	return ::std::apply(
		[](auto const &...arguments) -> observation {
			if constexpr (selected_target == 0u)
			{
				builtin_target_type value{};
				if constexpr (selected_frontdoor == 0u)
				{
					value = ::fast_io::to<builtin_target_type>(arguments...);
				}
				else
				{
					::fast_io::inplace_to(value, arguments...);
				}
				return make_builtin_observation(value);
			}
			else
			{
				context_target target{};
				if constexpr (selected_frontdoor == 0u)
				{
					target = ::fast_io::to<context_target>(arguments...);
				}
				else
				{
					::fast_io::inplace_to(target, arguments...);
				}
				return {target.value, target.digest, target.size,
						target.context_calls, target.nonempty_context_calls,
						target.empty_context_calls, target.eof_calls};
			}
		},
		pack);
}

inline constexpr ::std::uint_least64_t literal_expected{
	UINT64_C(314159265358979323)};

[[nodiscard]] inline observation invoke_literal_once()
{
	static_assert(selected_mode == 0u || selected_target == 0u,
				  "literal/compiler-constant cells require the built-in integer target");
	builtin_target_type value{};
	if constexpr (::fast_io_cpo_matrix::selected_pack_count == 1u)
	{
		if constexpr (selected_frontdoor == 0u)
		{
			value = ::fast_io::to<builtin_target_type>("314159265358979323");
		}
		else
		{
			::fast_io::inplace_to(value, "314159265358979323");
		}
	}
	else if constexpr (::fast_io_cpo_matrix::selected_pack_count == 2u)
	{
		if constexpr (selected_frontdoor == 0u)
		{
			value = ::fast_io::to<builtin_target_type>(
				"314159265", "358979323");
		}
		else
		{
			::fast_io::inplace_to(value, "314159265", "358979323");
		}
	}
	else if constexpr (::fast_io_cpo_matrix::selected_pack_count == 8u)
	{
		if constexpr (selected_frontdoor == 0u)
		{
			value = ::fast_io::to<builtin_target_type>(
				"314", "15", "92", "65", "35", "89", "79", "323");
		}
		else
		{
			::fast_io::inplace_to(
				value, "314", "15", "92", "65", "35", "89", "79", "323");
		}
	}
	else
	{
		if constexpr (selected_frontdoor == 0u)
		{
			value = ::fast_io::to<builtin_target_type>(
				"3", "1", "4", "1", "5", "9", "2", "6", "5", "3", "5",
				"8", "9", "7", "9", "3", "2", "3", "", "", "", "",
				"", "", "", "", "", "", "", "", "", "");
		}
		else
		{
			::fast_io::inplace_to(
				value, "3", "1", "4", "1", "5", "9", "2", "6", "5", "3",
				"5", "8", "9", "7", "9", "3", "2", "3", "", "", "", "",
				"", "", "", "", "", "", "", "", "", "");
		}
	}
	return make_builtin_observation(value);
}

[[nodiscard]] inline consteval bool selected_source_contract() noexcept
{
	using namespace ::fast_io_cpo_matrix;
	if constexpr (selected_mode == 1u)
	{
		// Literal cells spell every source directly at the public call site and do
		// not claim that a run-time source family entered the constant graph.
		return selected_target == 0u;
	}
	else if constexpr (selected_source_family == source_family::fixed_reserve)
	{
		return ::fast_io::reserve_printable<char, fixed_reserve_source> &&
			   ::std::same_as<decltype(print_eager_materialization_safe(
								  ::fast_io::io_reserve_type<char, fixed_reserve_source>)),
							  ::std::true_type>;
	}
	else if constexpr (selected_source_family == source_family::dynamic_reserve)
	{
		return ::fast_io::dynamic_reserve_printable<char, dynamic_reserve_source>;
	}
	else if constexpr (selected_source_family == source_family::precise_preferred)
	{
		return ::fast_io::precise_reserve_printable<char, precise_preferred_source> &&
			   ::std::same_as<decltype(print_precise_reserve_size_cached(
								  ::fast_io::io_reserve_type<char, precise_preferred_source>)),
							  ::std::true_type> &&
			   ::std::same_as<decltype(print_concat_fresh_precise_resize_preferred(
								  ::fast_io::io_reserve_type<char, precise_preferred_source>)),
							  ::std::true_type>;
	}
	else if constexpr (selected_source_family == source_family::stable_scatter)
	{
		return ::fast_io::scatter_printable<char, stable_scatter_source> &&
			   ::std::same_as<decltype(print_borrowed_scatter_source(
								  ::fast_io::io_reserve_type<char, stable_scatter_source>)),
							  ::std::true_type> &&
			   ::std::same_as<decltype(print_scatter_output_state_independent(
								  ::fast_io::io_reserve_type<char, stable_scatter_source>)),
							  ::std::true_type> &&
			   ::std::same_as<decltype(print_scatter_direct_print_equivalent(
								  ::fast_io::io_reserve_type<char, stable_scatter_source>)),
							  ::std::true_type>;
	}
	else if constexpr (selected_source_family == source_family::mixed_proven)
	{
		using bounded_query_result = decltype(single_pass_bounded_materialization_preferred(
			::fast_io::io_reserve_type<char, bounded_dynamic_source>));
		using bounded_size_result = decltype(single_pass_bounded_materialization_size(
			::fast_io::io_reserve_type<char, bounded_dynamic_source>,
			::std::declval<bounded_dynamic_source const &>(),
			::std::declval<::std::size_t>()));
		return source_family_for_index<0u> == source_family::fixed_reserve &&
			   source_family_for_index<1u> == source_family::bounded_dynamic &&
			   source_family_for_index<2u> == source_family::precise_preferred &&
			   source_family_for_index<3u> == source_family::stable_scatter &&
			   source_family_for_index<4u> == source_family::alias &&
			   ::std::same_as<bounded_query_result, ::std::true_type> &&
			   ::std::same_as<bounded_size_result, ::std::size_t> &&
			   ::fast_io::alias_printable<alias_source>;
	}
	else
	{
		return false;
	}
}

static_assert(selected_source_contract());
static_assert(::std::tuple_size_v<::fast_io_cpo_matrix::selected_source_pack> ==
			  ::fast_io_cpo_matrix::selected_pack_count);

[[nodiscard]] inline bool validate_runtime_corpus(
	::fast_io_cpo_matrix::corpus_type const &corpus,
	::fast_io_cpo_matrix::source_pack_corpus const &packs,
	::std::uint_least64_t &validation_digest)
{
	validation_digest = fnv_offset;
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const expected{make_expected(corpus[index])};
		auto const actual{invoke_runtime_once(packs[index])};
		/*
		The public protocol specifies the concatenated character sequence and the
		terminal publication transition, not a one-to-one relation between source
		objects and context invocations. Empty fragments may be elided, adjacent
		fragments may be coalesced, and a fragment may be split. The counters remain
		in failure diagnostics, while only value/digest/size and exactly one EOF are
		authoritative correctness observations.
		*/
		bool target_correct{actual.value == expected.value};
		if constexpr (selected_target != 0u)
		{
			target_correct = target_correct &&
						 actual.digest == expected.digest &&
						 actual.size == expected.size && actual.eof_calls == 1u;
		}
		if (!expected.valid || expected.size == 0u || !target_correct)
		{
			::std::fprintf(
				stderr,
				"to-protocol preflight failed: record=%zu size=%zu fragments=%zu calls=%zu nonempty=%zu empty=%zu eof=%zu\n",
				index, expected.size, expected.nonempty_fragments,
				actual.context_calls, actual.nonempty_context_calls,
				actual.empty_context_calls, actual.eof_calls);
			return false;
		}
		validation_digest =
			(validation_digest ^ observation_signature(actual)) * fnv_prime;
	}
	return true;
}

[[nodiscard]] inline constexpr char const *mode_name() noexcept
{
	return selected_mode == 0u ? "runtime"
							  : "literal-constant-call-lower-bound";
}

[[nodiscard]] inline constexpr char const *source_name() noexcept
{
	if constexpr (selected_mode == 1u)
	{
		return "literal";
	}
	else if constexpr (
		::fast_io_cpo_matrix::selected_source_family ==
		::fast_io_cpo_matrix::source_family::mixed_proven)
	{
		return "mixed-proof";
	}
	else
	{
		return ::fast_io_cpo_matrix::source_family_name();
	}
}

[[nodiscard]] inline constexpr char const *frontdoor_name() noexcept
{
	return selected_frontdoor == 0u ? "to" : "inplace-to";
}

[[nodiscard]] inline constexpr char const *target_name() noexcept
{
	return selected_target == 0u ? "builtin-u64" : "context-u64";
}

[[nodiscard]] inline int run_selected(
	::std::uint_least64_t seed,
	::std::uint_least64_t target_milliseconds)
{
	::fast_io_cpo_matrix::measurement measured{};
	::std::uint_least64_t validation_digest{fnv_offset};
	if constexpr (selected_mode == 0u)
	{
		::fast_io_cpo_matrix::corpus_type corpus{};
		build_decimal_corpus(corpus, seed);
		::fast_io_cpo_matrix::source_pack_corpus packs{};
		::fast_io_cpo_matrix::build_source_packs(corpus, packs);
		if (!validate_runtime_corpus(corpus, packs, validation_digest))
		{
			return 1;
		}
		auto timed_call = [&](::std::size_t iteration) {
			auto const &pack{packs[iteration &
								   (::fast_io_cpo_matrix::corpus_size - 1u)]};
			return observation_signature(invoke_runtime_once(pack));
		};
		auto reset = []() noexcept {};
		measured = ::fast_io_cpo_matrix::calibrate_and_measure(
			timed_call, reset, target_milliseconds);
	}
	else
	{
		/*
		Literal arguments expose a compiler constant-replacement/call lower bound.
		The compiler may erase most or all conversion work, so this timed closure is
		principally a compile/assembly/size control and is never evidence for a
		runtime fallback path.
		*/
		auto const preflight{invoke_literal_once()};
		if (preflight.value != literal_expected)
		{
			::std::fputs("literal to-protocol preflight failed\n", stderr);
			return 1;
		}
		validation_digest =
			(validation_digest ^ observation_signature(preflight)) * fnv_prime;
		auto timed_call = [](::std::size_t) {
			return observation_signature(invoke_literal_once());
		};
		auto reset = []() noexcept {};
		measured = ::fast_io_cpo_matrix::calibrate_and_measure(
			timed_call, reset, target_milliseconds);
	}

	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"to-protocol,%s,%s,%zu,%s,%s,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		mode_name(), source_name(),
		::fast_io_cpo_matrix::selected_pack_count, frontdoor_name(),
		target_name(), static_cast<unsigned long long>(seed),
		measured.iterations, elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
	return 0;
}

} // namespace fast_io_to_protocol

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
		(argc >= 2 &&
		 !::fast_io_cpo_matrix::parse_unsigned(argv[1], seed)) ||
		(argc == 3 &&
		 (!::fast_io_cpo_matrix::parse_unsigned(
			  argv[2], target_milliseconds) ||
		  target_milliseconds < 20u || target_milliseconds > 80u)))
	{
		::std::fputs(
			"usage: to_protocol_case [decimal-seed] [target-ms:20..80]\n",
			stderr);
		return 2;
	}
	return ::fast_io_to_protocol::run_selected(seed, target_milliseconds);
}
