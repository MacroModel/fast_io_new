#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <fast_io_dsal/string.h>
#include <fast_io_unit/string.h>

#include "byte_oracle.h"
#include "case_driver.h"

#ifndef FAST_IO_ORDERED_STAGING_SOURCE
#define FAST_IO_ORDERED_STAGING_SOURCE 0
#endif

#ifndef FAST_IO_ORDERED_STAGING_PACK
#define FAST_IO_ORDERED_STAGING_PACK 8
#endif

#ifndef FAST_IO_ORDERED_STAGING_RESULT
#define FAST_IO_ORDERED_STAGING_RESULT 0
#endif

#ifndef FAST_IO_ORDERED_STAGING_LINE
#define FAST_IO_ORDERED_STAGING_LINE 0
#endif

#ifndef FAST_IO_ORDERED_STAGING_TOPOLOGY
#define FAST_IO_ORDERED_STAGING_TOPOLOGY 0
#endif

#ifndef FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD
#define FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD 2049
#endif

namespace fast_io_concat_ordered_staging_bench
{

enum class source_schedule : unsigned char
{
	mixed_unretained,
	mixed_borrowed,
	precise
};

enum class barrier_topology : unsigned char
{
	repeated,
	early,
	middle,
	late
};

enum class result_kind : unsigned char
{
	standard_string,
	fast_io_string
};

inline constexpr source_schedule selected_source{
	static_cast<source_schedule>(FAST_IO_ORDERED_STAGING_SOURCE)};
inline constexpr ::std::size_t selected_pack{FAST_IO_ORDERED_STAGING_PACK};
inline constexpr result_kind selected_result{
	static_cast<result_kind>(FAST_IO_ORDERED_STAGING_RESULT)};
inline constexpr bool selected_line{FAST_IO_ORDERED_STAGING_LINE != 0};
inline constexpr barrier_topology selected_topology{
	static_cast<barrier_topology>(FAST_IO_ORDERED_STAGING_TOPOLOGY)};

static_assert(FAST_IO_ORDERED_STAGING_SOURCE >= 0 &&
			  FAST_IO_ORDERED_STAGING_SOURCE <= 2);
static_assert(
	selected_pack == 7u || selected_pack == 8u || selected_pack == 9u ||
	selected_pack == 32u);
static_assert(FAST_IO_ORDERED_STAGING_RESULT == 0 ||
			  FAST_IO_ORDERED_STAGING_RESULT == 1);
static_assert(FAST_IO_ORDERED_STAGING_LINE == 0 ||
			  FAST_IO_ORDERED_STAGING_LINE == 1);
static_assert(FAST_IO_ORDERED_STAGING_TOPOLOGY >= 0 &&
			  FAST_IO_ORDERED_STAGING_TOPOLOGY <= 3);
static_assert(selected_source == source_schedule::mixed_unretained ||
				  selected_topology == barrier_topology::repeated,
			  "barrier position is meaningful only for an unretained mixed schedule");

inline constexpr ::std::size_t corpus_size{64u};
inline constexpr ::std::size_t small_leaf_size{23u};
// Each independently compiled batch derives its static reserve bound from the largest admitted profile. Keeping
// the default at 2049 preserves the original code-generation experiment; a small profile in a larger-bound batch
// is not a single-variable comparison with the default binary because its fixed-source CPO contract has changed.
inline constexpr ::std::size_t maximum_total_payload{
	FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD};
static_assert(maximum_total_payload >= 2049u && maximum_total_payload <= 8193u);
inline constexpr ::std::size_t maximum_leaf_payload{
	(maximum_total_payload + selected_pack - 1u) / selected_pack};

struct token_view
{
	char const *data{};
	::std::size_t size{};
};

struct corpus_record
{
	::std::array<char, maximum_total_payload> bytes{};
	::std::array<::std::size_t, selected_pack + 1u> offsets{};
};

using corpus_type = ::std::array<corpus_record, corpus_size>;

[[nodiscard]] inline constexpr ::std::uint_least64_t xorshift64(
	::std::uint_least64_t value) noexcept
{
	value ^= value << 7u;
	value ^= value >> 9u;
	value ^= value << 8u;
	return value;
}

/// @brief Builds one immutable byte interval and partitions it among the public concat arguments.
/// @details Every partition has either floor(total/N) or ceil(total/N) bytes; rotating the remainder changes leaf
///          boundaries without changing the complete expected interval. This keeps the static reserve bound tight at
///          ceil(maximum_total_payload/N), unlike a maximum-per-leaf fixture whose aggregate bound would measure
///          artificial over-reserve. The default batch retains maximum_total_payload == 2049.
///          The bytes and boundaries are owned by the record for every synchronous old/new invocation.
inline void build_corpus(
	corpus_type &corpus, ::std::size_t total_payload,
	::std::uint_least64_t seed) noexcept
{
	static constexpr char alphabet[]{
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"};
	auto random{seed ^ UINT64_C(0x9e3779b97f4a7c15)};
	auto const quotient{total_payload / selected_pack};
	auto const remainder{total_payload % selected_pack};
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto &record{corpus[record_index]};
		record.offsets[0u] = 0u;
		auto const remainder_origin{
			(record_index + static_cast<::std::size_t>(seed)) % selected_pack};
		for (::std::size_t leaf{}; leaf != selected_pack; ++leaf)
		{
			auto const rotated{(leaf + selected_pack - remainder_origin) %
							   selected_pack};
			auto const leaf_size{quotient +
								 static_cast<::std::size_t>(rotated < remainder)};
			record.offsets[leaf + 1u] = record.offsets[leaf] + leaf_size;
		}
		for (::std::size_t index{}; index != total_payload; ++index)
		{
			random = xorshift64(random + UINT64_C(0xd1b54a32d192ed03));
			record.bytes[index] = alphabet[static_cast<::std::size_t>(random) %
										   (sizeof(alphabet) - 1u)];
		}
	}
}

[[nodiscard]] inline constexpr token_view token_at(
	corpus_record const &record, ::std::size_t index) noexcept
{
	auto const first{record.offsets[index]};
	auto const last{record.offsets[index + 1u]};
	return {record.bytes.data() + first, last - first};
}

inline constexpr char *copy_token(
	char *destination, token_view source) noexcept
{
	for (::std::size_t index{}; index != source.size; ++index)
	{
		destination[index] = source.data[index];
	}
	return destination + source.size;
}

struct fixed_source
{
	token_view view{};
};

/// @brief Supplies the same tight type-level upper bound to every fixed leaf in this compile-time pack.
/// @details `maximum_leaf_payload` is derived from the largest admitted total and the balanced partition invariant.
///          The returned cursor, not the bound, remains the authoritative logical extent for every smaller profile.
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_source>) noexcept
{
	return maximum_leaf_payload;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_source>, char *destination,
	fixed_source source) noexcept
{
	return copy_token(destination, source.view);
}

[[nodiscard]] inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, fixed_source>) noexcept
{
	// The writer copies an immutable record slice and has no externally observable state.
	return {};
}

struct dynamic_source
{
	token_view view{};
};

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_source>,
	dynamic_source source) noexcept
{
	return source.view.size + 4u;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_source>, char *destination,
	dynamic_source source) noexcept
{
	return copy_token(destination, source.view);
}

struct precise_source
{
	token_view view{};
};

/// @brief Provides byte-equivalent conservative and exact protocols as the precise negative control.
/// @details A destination may select either complete pair, but it must not mix or replay the pairs. The explicit slack
///          keeps the ordinary dynamic spelling non-exact while the precise size remains the record partition extent.
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_source>,
	precise_source source) noexcept
{
	return source.view.size + 7u;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_source>, char *destination,
	precise_source source) noexcept
{
	return copy_token(destination, source.view);
}

[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_source>,
	precise_source source) noexcept
{
	return source.view.size;
}

[[nodiscard]] inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_source>, char *destination,
	::std::size_t precise_size, precise_source source) noexcept
{
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		destination[index] = source.view.data[index];
	}
	return destination + precise_size;
}

struct unretained_scatter_source
{
	token_view view{};
};

/// @brief Publishes a synchronous scatter without granting descriptor-retention semantics.
/// @details The fixture storage happens to outlive the call, but pointer shape cannot prove that a generic producer is
///          stable across later CPO invocations. This type therefore models the formal barrier which the ordered
///          staging policy must consume before it evaluates the following leaf.
[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char>
print_scatter_define(
	::fast_io::io_reserve_type_t<char, unretained_scatter_source>,
	unretained_scatter_source source) noexcept
{
	return {source.view.data, source.view.size};
}

struct borrowed_scatter_source
{
	token_view view{};
};

[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char>
print_scatter_define(
	::fast_io::io_reserve_type_t<char, borrowed_scatter_source>,
	borrowed_scatter_source source) noexcept
{
	return {source.view.data, source.view.size};
}

[[nodiscard]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, borrowed_scatter_source>) noexcept
{
	/* The returned interval belongs to immutable corpus storage whose lifetime contains the complete concat call. */
	return {};
}

struct alias_source
{
	token_view view{};
};

[[nodiscard]] inline constexpr fixed_source print_alias_define(
	::fast_io::io_alias_t, alias_source source) noexcept
{
	return {source.view};
}

static_assert(::fast_io::reserve_printable<char, fixed_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, dynamic_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, precise_source>);
static_assert(::fast_io::precise_reserve_printable<char, precise_source>);
static_assert(::fast_io::scatter_printable<char, unretained_scatter_source>);
static_assert(::fast_io::scatter_printable<char, borrowed_scatter_source>);
static_assert(::fast_io::alias_printable<alias_source>);

enum class leaf_kind : unsigned char
{
	fixed,
	dynamic,
	precise,
	unretained_scatter,
	borrowed_scatter,
	alias
};

[[nodiscard]] consteval ::std::size_t selected_barrier_index() noexcept
{
	if constexpr (selected_topology == barrier_topology::early)
	{
		return 0u;
	}
	else if constexpr (selected_topology == barrier_topology::middle)
	{
		return selected_pack / 2u;
	}
	else
	{
		return selected_pack - 1u;
	}
}

template <::std::size_t index>
inline constexpr leaf_kind leaf_kind_for_index{[]() consteval {
	if constexpr (selected_source == source_schedule::precise)
	{
		return leaf_kind::precise;
	}
	else if constexpr (selected_source == source_schedule::mixed_borrowed)
	{
		constexpr auto slot{index % 5u};
		if constexpr (slot == 0u)
		{
			return leaf_kind::fixed;
		}
		else if constexpr (slot == 1u)
		{
			return leaf_kind::dynamic;
		}
		else if constexpr (slot == 2u)
		{
			return leaf_kind::precise;
		}
		else if constexpr (slot == 3u)
		{
			return leaf_kind::borrowed_scatter;
		}
		else
		{
			return leaf_kind::alias;
		}
	}
	else if constexpr (selected_topology == barrier_topology::repeated)
	{
		constexpr auto slot{index % 5u};
		if constexpr (slot == 0u)
		{
			return leaf_kind::fixed;
		}
		else if constexpr (slot == 1u)
		{
			return leaf_kind::dynamic;
		}
		else if constexpr (slot == 2u)
		{
			return leaf_kind::precise;
		}
		else if constexpr (slot == 3u)
		{
			return leaf_kind::unretained_scatter;
		}
		else
		{
			return leaf_kind::alias;
		}
	}
	else if constexpr (index == selected_barrier_index())
	{
		return leaf_kind::unretained_scatter;
	}
	else
	{
		/* A single-position topology rotates only destination-neutral non-scatter leaves around its barrier. */
		constexpr auto slot{index % 4u};
		if constexpr (slot == 0u)
		{
			return leaf_kind::fixed;
		}
		else if constexpr (slot == 1u)
		{
			return leaf_kind::dynamic;
		}
		else if constexpr (slot == 2u)
		{
			return leaf_kind::precise;
		}
		else
		{
			return leaf_kind::alias;
		}
	}
}()};

template <leaf_kind kind>
struct source_type;

template <>
struct source_type<leaf_kind::fixed>
{
	using type = fixed_source;
};

template <>
struct source_type<leaf_kind::dynamic>
{
	using type = dynamic_source;
};

template <>
struct source_type<leaf_kind::precise>
{
	using type = precise_source;
};

template <>
struct source_type<leaf_kind::unretained_scatter>
{
	using type = unretained_scatter_source;
};

template <>
struct source_type<leaf_kind::borrowed_scatter>
{
	using type = borrowed_scatter_source;
};

template <>
struct source_type<leaf_kind::alias>
{
	using type = alias_source;
};

template <::std::size_t index>
using source_type_for_index =
	typename source_type<leaf_kind_for_index<index>>::type;

template <::std::size_t index>
[[nodiscard]] inline constexpr source_type_for_index<index> make_source(
	corpus_record const &record) noexcept
{
	return {token_at(record, index)};
}

template <::std::size_t... indices>
[[nodiscard]] inline constexpr auto make_source_pack_impl(
	corpus_record const &record, ::std::index_sequence<indices...>) noexcept
{
	return ::std::tuple<source_type_for_index<indices>...>{
		make_source<indices>(record)...};
}

[[nodiscard]] inline constexpr auto make_source_pack(
	corpus_record const &record) noexcept
{
	return make_source_pack_impl(
		record, ::std::make_index_sequence<selected_pack>{});
}

using selected_source_pack =
	decltype(make_source_pack(::std::declval<corpus_record const &>()));
using source_pack_corpus = ::std::array<selected_source_pack, corpus_size>;

inline void build_source_packs(
	corpus_type const &corpus, source_pack_corpus &packs) noexcept
{
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		packs[index] = make_source_pack(corpus[index]);
	}
}

template <typename pack_type>
[[nodiscard]] inline auto invoke_concat(pack_type const &pack)
{
	return ::std::apply(
		[](auto const &...arguments) {
			if constexpr (selected_result == result_kind::standard_string)
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

/// @brief Validates every produced byte against the original unspecialized record interval.
/// @details The expected data is read directly from corpus storage; the optional newline is checked separately.
///          Neither path calls a formatting or concat CPO, so changing source strategy cannot change the oracle.
[[nodiscard]] inline bool validate_corpus(
	corpus_type const &corpus, source_pack_corpus const &packs,
	::std::size_t total_payload, ::std::uint_least64_t &digest) noexcept
{
	digest = UINT64_C(14695981039346656037);
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto result{invoke_concat(packs[record_index])};
		auto const expected_size{total_payload +
								 static_cast<::std::size_t>(selected_line)};
		if (result.size() != expected_size)
		{
			::std::fprintf(
				stderr,
				"ordered staging size mismatch: record=%zu actual=%zu expected=%zu\n",
				record_index, static_cast<::std::size_t>(result.size()),
				expected_size);
			return false;
		}
		auto const comparison{::fast_io_cpo_matrix::oracle::compare_bytes(
			result.data(), total_payload, corpus[record_index].bytes.data(),
			total_payload)};
		if (!comparison.equal ||
			(selected_line && result.data()[total_payload] != '\n'))
		{
			::std::fprintf(
				stderr,
				"ordered staging byte mismatch: record=%zu byte=%zu\n",
				record_index,
				comparison.equal ? total_payload : comparison.mismatch);
			return false;
		}
		digest = ::fast_io_cpo_matrix::oracle::digest_bytes(
			digest, result.data(), result.size());
	}
	return true;
}

[[nodiscard]] inline constexpr char const *source_name() noexcept
{
	if constexpr (selected_source == source_schedule::mixed_unretained)
	{
		return "mixed";
	}
	else if constexpr (selected_source == source_schedule::mixed_borrowed)
	{
		return "mixed-borrowed";
	}
	else
	{
		return "precise";
	}
}

[[nodiscard]] inline constexpr char const *topology_name() noexcept
{
	if constexpr (selected_topology == barrier_topology::repeated)
	{
		return "repeated";
	}
	else if constexpr (selected_topology == barrier_topology::early)
	{
		return "early";
	}
	else if constexpr (selected_topology == barrier_topology::middle)
	{
		return "middle";
	}
	else
	{
		return "late";
	}
}

[[nodiscard]] inline constexpr char const *result_name() noexcept
{
	if constexpr (selected_result == result_kind::standard_string)
	{
		return "std-string";
	}
	else
	{
		return "fast-io-string";
	}
}

[[nodiscard]] inline bool parse_profile(
	char const *text, ::std::size_t &total_payload) noexcept
{
	if (::std::string_view{text} == "small")
	{
		total_payload = selected_pack * small_leaf_size;
		return true;
	}
	::std::uint_least64_t parsed{};
	// The default runner retains its required small/2047/2048/2049 set. The optional 511/512/513 spellings are
	// narrow probes of the adaptive 512-character transition and reuse this identical binary and byte oracle.
	if (!::fast_io_cpo_matrix::parse_unsigned(text, parsed) ||
		(parsed != 511u && parsed != 512u && parsed != 513u &&
		 parsed != 2047u && parsed != 2048u && parsed != 2049u
#if FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD > 2049
		 // Larger profiles are admitted only by a separately compiled batch. The range check is performed before
		 // narrowing or corpus construction, so every admitted byte interval fits the fixture's static storage.
		 && (parsed > maximum_total_payload ||
			 (parsed != 2559u && parsed != 2560u && parsed != 2561u &&
			  parsed != 4095u && parsed != 4096u && parsed != 4097u &&
			  parsed != 8191u && parsed != 8192u && parsed != 8193u))
#endif
		 ))
	{
		return false;
	}
	total_payload = static_cast<::std::size_t>(parsed);
	return true;
}

} // namespace fast_io_concat_ordered_staging_bench

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_ORDERED_STAGING_NOINLINE __attribute__((__noinline__))
#else
#define FAST_IO_ORDERED_STAGING_NOINLINE
#endif

/// @brief Exposes one stable, externally visible code-generation root for object and linked-image measurement.
/// @details The C linkage name is available in both the official and candidate revisions. The opaque memory boundary
///          makes the completed byte interval observable without adding a traversal to the timed region; full semantic
///          validation remains outside timing. Accepting the already-materialized source tuple by reference also keeps
///          corpus construction and tuple transport out of the concat measurement.
extern "C" FAST_IO_ORDERED_STAGING_NOINLINE ::std::size_t
fast_io_concat_ordered_staging_kernel(
	fast_io_concat_ordered_staging_bench::selected_source_pack const &pack)
{
	auto result{fast_io_concat_ordered_staging_bench::invoke_concat(pack)};
	::fast_io_cpo_matrix::compiler_observe_bytes(result.data(), result.size());
	return result.size();
}

int main(int argc, char **argv)
{
	using namespace fast_io_concat_ordered_staging_bench;
	::fast_io_cpo_matrix::process_deadline_guard deadline;
	if (!deadline.armed())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	::std::size_t total_payload{};
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{40u};
	if (argc < 2 || argc > 4 || !parse_profile(argv[1], total_payload) ||
		(argc >= 3 && !::fast_io_cpo_matrix::parse_unsigned(argv[2], seed)) ||
		(argc == 4 &&
		 (!::fast_io_cpo_matrix::parse_unsigned(
			  argv[3], target_milliseconds) ||
		  target_milliseconds < 20u || target_milliseconds > 80u)))
	{
		::std::fputs(
			"usage: concat_ordered_staging_case small|511|512|513|2047|2048|2049"
#if FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD > 2049
			"|2559|2560|2561|4095|4096|4097|8191|8192|8193 (within compiled bound)"
#endif
			" [seed] [target-ms:20..80]\n",
			stderr);
		return 2;
	}

	corpus_type corpus{};
	build_corpus(corpus, total_payload, seed);
	source_pack_corpus packs{};
	build_source_packs(corpus, packs);
	::std::uint_least64_t validation_digest{};
	if (!validate_corpus(corpus, packs, total_payload, validation_digest))
	{
		return 1;
	}

	auto timed_call = [&](::std::size_t iteration) -> ::std::size_t {
		return fast_io_concat_ordered_staging_kernel(
			packs[iteration & (corpus_size - 1u)]);
	};
	auto reset_timed_state = []() noexcept {};
	auto const measured{::fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset_timed_state, target_milliseconds)};
	auto const seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"concat-ordered-staging,%s,%s,%zu,%u,%s,%zu,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		source_name(), topology_name(), selected_pack, selected_line ? 1u : 0u,
		result_name(), total_payload, static_cast<unsigned long long>(seed),
		measured.iterations, seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
}
