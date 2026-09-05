#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

#include "../0026.cpo_matrix/case_driver.h"

#ifndef FAST_IO_TO_PAYLOAD_SOURCE
#define FAST_IO_TO_PAYLOAD_SOURCE 0
#endif
#ifndef FAST_IO_TO_PAYLOAD_PROFILE
#define FAST_IO_TO_PAYLOAD_PROFILE 0
#endif
#ifndef FAST_IO_TO_PAYLOAD_PACK
#define FAST_IO_TO_PAYLOAD_PACK 4
#endif
#ifndef FAST_IO_TO_PAYLOAD_FRONTDOOR
#define FAST_IO_TO_PAYLOAD_FRONTDOOR 0
#endif
#ifndef FAST_IO_TO_PAYLOAD_OLD
#define FAST_IO_TO_PAYLOAD_OLD 0
#endif

#if FAST_IO_TO_PAYLOAD_OLD && FAST_IO_TO_PAYLOAD_PROFILE == 3
#error "stop-boundary is new-only: the official old controller does not stop on ok at the current fragment end"
#endif

namespace fast_io_to_large_payload
{

// One translation unit owns one source protocol, size schedule, pack, and public front door. Runtime leaf lengths
// never change the descriptor type or collector capacity, so neighboring lengths share the same compiled kernel.
inline constexpr unsigned selected_source{FAST_IO_TO_PAYLOAD_SOURCE};
inline constexpr unsigned selected_profile{FAST_IO_TO_PAYLOAD_PROFILE};
inline constexpr ::std::size_t selected_pack{FAST_IO_TO_PAYLOAD_PACK};
inline constexpr unsigned selected_frontdoor{FAST_IO_TO_PAYLOAD_FRONTDOOR};
static_assert(selected_source <= 2u && selected_profile <= 3u);
static_assert(selected_pack == 4u || selected_pack == 8u);
static_assert(selected_frontdoor <= 1u);
static_assert(FAST_IO_TO_PAYLOAD_OLD == 0 || FAST_IO_TO_PAYLOAD_OLD == 1);

inline constexpr bool stops_early{selected_profile >= 2u};
inline constexpr ::std::size_t corpus_size{8u};
inline constexpr ::std::size_t maximum_leaf_size{4097u};
inline constexpr ::std::size_t collector_capacity{selected_pack * maximum_leaf_size};
inline constexpr ::std::uint_least64_t fnv_offset{UINT64_C(14695981039346656037)};
inline constexpr ::std::uint_least64_t fnv_prime{UINT64_C(1099511628211)};

template <unsigned family>
struct source_view
{
	char const *data{};
	::std::size_t size{};
};

template <unsigned family>
	requires(family != 2u)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, source_view<family>>, source_view<family> source) noexcept
{
	// Size observation is pure and non-consuming. An old implementation may legally inspect an unused suffix;
	// the benchmark compares that planning cost without treating one implementation's query schedule as semantics.
	return source.size;
}

template <unsigned family>
	requires(family != 2u)
[[nodiscard]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, source_view<family>>, char *destination,
	source_view<family> source) noexcept
{
	// The size CPO is an exact bound for this immutable slice. The caller owns a distinct staging interval of at least
	// that extent, and the returned cursor is the only logical-length publication made by the writer.
	if (source.size != 0u)
	{
		::std::memcpy(destination, source.data, source.size);
	}
	return destination + source.size;
}

[[nodiscard]] inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, source_view<1u>>) noexcept
{
	// This is a scratch preference, not a bound on the source domain. A fragment larger than 256 bytes still reports
	// its full runtime size and must use a correctly sized fallback. The unhinted control deliberately has no such CPO.
	return 256u;
}

[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, source_view<2u>>, source_view<2u> source) noexcept
{
	// The negative control exposes the identical immutable bytes without formatter scratch. No source owns or mutates
	// the interval, and its corpus lifetime contains every synchronous conversion in this process.
	return {source.data, source.size};
}

using source_type = source_view<selected_source>;
using source_pack = ::std::array<source_type, selected_pack>;
static_assert((selected_source == 2u && ::fast_io::scatter_printable<char, source_type>) ||
			  (selected_source != 2u && ::fast_io::dynamic_reserve_printable<char, source_type>));

struct collector
{
	::std::array<char, collector_capacity> bytes;
	::std::size_t size{};
	::std::size_t eof_calls{};

	// A user-provided default constructor leaves the byte array uninitialized: only the committed prefix is ever
	// read. This avoids timing an unrelated whole-capacity zero fill, including in an eight-byte early-stop sample.
	inline collector() noexcept
	{}

	// NRVO is an optimization, not a lifetime premise. If a compiler transports this result through a copy, only the
	// initialized prefix is copied; no defaulted array copy may inspect indeterminate bytes beyond the logical extent.
	inline collector(collector const &other) noexcept : size(other.size), eof_calls(other.eof_calls)
	{
		if (size != 0u)
		{
			::std::memcpy(bytes.data(), other.bytes.data(), size);
		}
	}

	inline collector &operator=(collector const &other) noexcept
	{
		if (this != __builtin_addressof(other))
		{
			size = other.size;
			eof_calls = other.eof_calls;
			if (size != 0u)
			{
				::std::memcpy(bytes.data(), other.bytes.data(), size);
			}
		}
		return *this;
	}
};

struct collector_ref
{
	collector *target{};
};

struct collector_state
{
	::std::size_t written{};
	bool terminal{};
};

[[nodiscard]] inline constexpr collector_ref scan_alias_define(
	::fast_io::io_alias_t, collector &target) noexcept
{
	return {__builtin_addressof(target)};
}

[[nodiscard]] inline constexpr ::fast_io::io_type_t<collector_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, collector_ref>) noexcept
{
	return {};
}

[[nodiscard]] inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, collector_ref>, collector_state &state,
	char const *first, char const *last, collector_ref destination) noexcept
{
	if (state.terminal)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	auto const available{static_cast<::std::size_t>(last - first)};
	auto accepted{available};
	bool found_delimiter{};
	if constexpr (stops_early)
	{
		// Delimiter recognition depends on bytes, never on fragment boundaries. Splitting or coalescing a source thus
		// preserves the accepted prefix; the delimiter itself belongs to that prefix and its successor is returned.
		if (available != 0u)
		{
			auto const *delimiter{static_cast<char const *>(::std::memchr(first, '|', available))};
			if (delimiter != nullptr)
			{
				accepted = static_cast<::std::size_t>(delimiter - first) + 1u;
				found_delimiter = true;
			}
		}
	}
	// State invariant: written <= collector_capacity. Subtraction precedes addition, so a malformed or unexpectedly
	// oversized source cannot wrap the capacity check or write beyond the destination's live array.
	if (accepted > collector_capacity - state.written)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	if (accepted != 0u)
	{
		::std::memcpy(destination.target->bytes.data() + state.written, first, accepted);
	}
	state.written += accepted;
	destination.target->size = state.written;
	state.terminal = found_delimiter;
	return {first + accepted, found_delimiter ? ::fast_io::parse_code::ok : ::fast_io::parse_code::partial};
}

[[nodiscard]] inline ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, collector_ref>, collector_state &state,
	collector_ref destination) noexcept
{
	// Full consumption has exactly one EOF publication; delimiter success has none. A call after delimiter success
	// or an early-stop corpus that reaches EOF without its delimiter is a protocol failure, not an alternate schedule.
	if (state.terminal || stops_early)
	{
		return ::fast_io::parse_code::invalid;
	}
	state.terminal = true;
	++destination.target->eof_calls;
	return ::fast_io::parse_code::ok;
}

static_assert(::fast_io::context_scannable<char, collector_ref>);
static_assert(!::fast_io::contiguous_scannable<char, collector_ref>);

struct corpus_record
{
	::std::array<char, collector_capacity> bytes;
	source_pack sources{};
	::std::size_t size{};
};
using corpus_type = ::std::array<corpus_record, corpus_size>;

[[nodiscard]] inline constexpr ::std::size_t fragment_size(
	::std::size_t index, ::std::size_t leaf_size) noexcept
{
	if constexpr (selected_profile == 0u)
	{
		return leaf_size - (index & 1u);
	}
	else if constexpr (selected_profile == 1u)
	{
		// The first two leaves are small; the third requires a larger extent; later leaves revisit that extent or a
		// smaller one. "Growth" names this source schedule, not a mandatory allocator strategy for either library.
		return index == 0u ? 8u : ((index & 1u) != 0u ? 16u : leaf_size);
	}
	else
	{
		return index == 0u ? (selected_profile == 2u ? 32u : 8u) : leaf_size;
	}
}

inline void build_corpus(corpus_type &corpus, ::std::size_t leaf_size,
						 ::std::uint_least64_t seed) noexcept
{
	static constexpr char alphabet[]{"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"};
	auto random{seed ^ UINT64_C(0x6a09e667f3bcc909)};
	for (auto &record : corpus)
	{
		record.size = 0u;
		for (::std::size_t index{}; index != selected_pack; ++index)
		{
			auto const extent{fragment_size(index, leaf_size)};
			// Every extent is in [1,maximum_leaf_size], and exactly selected_pack extents enter the record. Therefore
			// their sum fits collector_capacity without arithmetic overflow; pointers never refer to another record.
			record.sources[index] = {record.bytes.data() + record.size, extent};
			for (::std::size_t offset{}; offset != extent; ++offset)
			{
				random ^= random << 7u;
				random ^= random >> 9u;
				random ^= random << 8u;
				record.bytes[record.size + offset] = alphabet[static_cast<::std::size_t>(random) % (sizeof(alphabet) - 1u)];
			}
			record.size += extent;
		}
		if constexpr (stops_early)
		{
			// The generated alphabet excludes '|'. This one delimiter ends the semantic result after eight bytes;
			// stop-interior leaves 24 bytes in the first fragment, whereas stop-boundary ends that fragment exactly.
			record.bytes[7u] = '|';
		}
	}
}

template <::std::size_t... indices>
[[nodiscard]] inline collector invoke(source_pack const &sources, ::std::index_sequence<indices...>)
{
	if constexpr (selected_frontdoor == 0u)
	{
		return ::fast_io::to<collector>(sources[indices]...);
	}
	else
	{
		collector result;
		::fast_io::inplace_to(result, sources[indices]...);
		return result;
	}
}

struct oracle_record
{
	::std::size_t size{};
	::std::size_t eof_calls{};
	::std::uint_least64_t digest{fnv_offset};
};

[[nodiscard]] inline oracle_record inspect_original_bytes(corpus_record const &record) noexcept
{
	// This oracle walks the owned byte interval directly. It calls no alias, reserve, writer, scatter, scanner, or
	// conversion CPO and does not infer the accepted prefix from the selected implementation's fragment schedule.
	oracle_record result{};
	for (; result.size != record.size;)
	{
		auto const byte{static_cast<unsigned char>(record.bytes[result.size++])};
		result.digest = (result.digest ^ byte) * fnv_prime;
		if constexpr (stops_early)
		{
			if (byte == static_cast<unsigned char>('|'))
			{
				return result;
			}
		}
	}
	result.eof_calls = 1u;
	return result;
}

[[nodiscard]] inline bool validate_corpus(corpus_type const &corpus,
										  ::std::uint_least64_t &validation_digest)
{
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const &record{corpus[index]};
		auto const expected{inspect_original_bytes(record)};
		auto const actual{invoke(record.sources, ::std::make_index_sequence<selected_pack>{})};
		bool valid{actual.size == expected.size && actual.eof_calls == expected.eof_calls};
		for (::std::size_t offset{}; valid && offset != expected.size; ++offset)
		{
			valid = actual.bytes[offset] == record.bytes[offset];
		}
		if (!valid)
		{
			::std::fprintf(stderr, "to-large-payload preflight failed: record=%zu actual=%zu expected=%zu eof=%zu expected_eof=%zu\n",
						   index, actual.size, expected.size, actual.eof_calls, expected.eof_calls);
			return false;
		}
		validation_digest = (validation_digest ^ expected.digest ^ static_cast<::std::uint_least64_t>(expected.size)) * fnv_prime;
	}
	return true;
}

[[nodiscard]] inline constexpr char const *source_name() noexcept
{
	return selected_source == 0u ? "dynamic" : (selected_source == 1u ? "hinted-256" : "scatter");
}

[[nodiscard]] inline constexpr char const *profile_name() noexcept
{
	return selected_profile == 0u ? "reuse" : (selected_profile == 1u ? "growth" : (selected_profile == 2u ? "stop-interior" : "stop-boundary"));
}

} // namespace fast_io_to_large_payload

// A stable external code-generation root permits disassembly and linked-symbol measurements. The entire committed
// result escapes to opaque code, but no oracle traversal or digest computation enters the calibrated interval.
extern "C" [[gnu::noinline]] ::std::size_t fast_io_to_large_payload_kernel(
	fast_io_to_large_payload::source_pack const &sources)
{
	using namespace fast_io_to_large_payload;
	auto const result{invoke(sources, ::std::make_index_sequence<selected_pack>{})};
	::fast_io_cpo_matrix::compiler_observe_bytes(result.bytes.data(), result.size);
	return result.size ^ (result.eof_calls << 16u);
}

int main(int argc, char **argv)
{
	using namespace fast_io_to_large_payload;
	::fast_io_cpo_matrix::process_deadline_guard deadline;
	if (!deadline.armed())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	::std::uint_least64_t leaf_size{};
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{40u};
	if (argc < 2 || argc > 4 || !::fast_io_cpo_matrix::parse_unsigned(argv[1], leaf_size) ||
		leaf_size < 32u || leaf_size > maximum_leaf_size ||
		(argc >= 3 && !::fast_io_cpo_matrix::parse_unsigned(argv[2], seed)) ||
		(argc == 4 && (!::fast_io_cpo_matrix::parse_unsigned(argv[3], target_milliseconds) ||
					   target_milliseconds < 20u || target_milliseconds > 80u)))
	{
		::std::fputs("usage: to_large_payload_case LEAF_BYTES:32..4097 [seed] [target-ms:20..80]\n", stderr);
		return 2;
	}
	corpus_type corpus;
	build_corpus(corpus, static_cast<::std::size_t>(leaf_size), seed);
	::std::uint_least64_t validation_digest{fnv_offset};
	if (!validate_corpus(corpus, validation_digest))
	{
		return 1;
	}
	auto timed_call = [&](::std::size_t iteration) {
		return fast_io_to_large_payload_kernel(corpus[iteration & (corpus_size - 1u)].sources);
	};
	auto reset = []() noexcept {};
	auto const measured{::fast_io_cpo_matrix::calibrate_and_measure(timed_call, reset, target_milliseconds)};
	auto const expected{inspect_original_bytes(corpus.front())};
	::std::printf("to-large-payload,%s,%s,%zu,%s,%s,%llu,%zu,%zu,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
				  source_name(), profile_name(), selected_pack, selected_frontdoor == 0u ? "to" : "inplace-to",
				  FAST_IO_TO_PAYLOAD_OLD ? "official-old" : "new", static_cast<unsigned long long>(leaf_size),
				  corpus.front().size, expected.size, static_cast<unsigned long long>(seed), measured.iterations,
				  static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9,
				  static_cast<double>(measured.elapsed_nanoseconds) / static_cast<double>(measured.iterations),
				  static_cast<unsigned long long>(measured.checksum), static_cast<unsigned long long>(validation_digest));
}
