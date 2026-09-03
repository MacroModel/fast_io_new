#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include <fast_io.h>

#ifndef FAST_IO_CPO_MATRIX_SOURCE
#define FAST_IO_CPO_MATRIX_SOURCE 5
#endif

#ifndef FAST_IO_CPO_MATRIX_PACK
#define FAST_IO_CPO_MATRIX_PACK 1
#endif

#ifndef FAST_IO_CPO_MATRIX_LINE
#define FAST_IO_CPO_MATRIX_LINE 0
#endif

namespace fast_io_cpo_matrix
{

inline constexpr ::std::size_t maximum_token_size{23u};
inline constexpr ::std::size_t maximum_protocol_bound{32u};
inline constexpr ::std::size_t maximum_pack_count{64u};
inline constexpr ::std::size_t corpus_size{256u};

enum class source_family : unsigned char
{
	fixed_reserve,
	dynamic_reserve,
	precise_reserve,
	scatter,
	alias,
	mixed,
	borrowed_scatter,
	mixed_borrowed,
	bounded_dynamic,
	precise_preferred,
	stable_scatter,
	mixed_proven
};

inline constexpr source_family selected_source_family{
	static_cast<source_family>(FAST_IO_CPO_MATRIX_SOURCE)};
inline constexpr ::std::size_t selected_pack_count{FAST_IO_CPO_MATRIX_PACK};
inline constexpr bool selected_line{FAST_IO_CPO_MATRIX_LINE != 0};

static_assert(FAST_IO_CPO_MATRIX_SOURCE >= 0 && FAST_IO_CPO_MATRIX_SOURCE <= 11);
static_assert(selected_pack_count >= 1u &&
			  selected_pack_count <= maximum_pack_count);
static_assert(FAST_IO_CPO_MATRIX_LINE == 0 || FAST_IO_CPO_MATRIX_LINE == 1);

struct token_text
{
	::std::array<char, maximum_token_size> bytes{};
	::std::size_t size{};
};

struct token_view
{
	char const *data{};
	::std::size_t size{};
};

struct corpus_record
{
	::std::array<token_text, maximum_pack_count> tokens{};
};

using corpus_type = ::std::array<corpus_record, corpus_size>;

[[nodiscard]] inline constexpr ::std::uint_least64_t
xorshift64(::std::uint_least64_t value) noexcept
{
	value ^= value << 7u;
	value ^= value >> 9u;
	value ^= value << 8u;
	return value;
}

inline void build_corpus(corpus_type &corpus, ::std::uint_least64_t seed) noexcept
{
	static constexpr ::std::array<::std::size_t, 8u> edge_sizes{
		0u, 1u, 7u, 8u, 15u, 16u, 22u, 23u};
	static constexpr char alphabet[]{
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_"};
	auto random{seed ^ UINT64_C(0x9e3779b97f4a7c15)};
	auto const size_offset{static_cast<::std::size_t>(seed) & 7u};
	for (::std::size_t record_index{}; record_index != corpus.size();
		 ++record_index)
	{
		auto &record{corpus[record_index]};
		for (::std::size_t token_index{}; token_index != record.tokens.size();
			 ++token_index)
		{
			auto &token{record.tokens[token_index]};
			random = xorshift64(random + UINT64_C(0xd1b54a32d192ed03));
			/*
			Cycling by record index guarantees every compiled pack, including N=1,
			crosses every length boundary.  The seed rotates that cycle and still
			controls every payload byte, so old/new receive identical run-time data.
			*/
			token.size = edge_sizes[(record_index + token_index + size_offset) % edge_sizes.size()];
			for (::std::size_t index{}; index != token.size; ++index)
			{
				random = xorshift64(random);
				token.bytes[index] = alphabet[static_cast<::std::size_t>(random) % (sizeof(alphabet) - 1u)];
			}
		}
	}
}

[[nodiscard]] inline constexpr token_view view_of(token_text const &token) noexcept
{
	return {token.bytes.data(), token.size};
}

inline constexpr char *copy_token(char *destination, token_view view) noexcept
{
	for (::std::size_t index{}; index != view.size; ++index)
	{
		destination[index] = view.data[index];
	}
	return destination + view.size;
}

struct fixed_reserve_source
{
	token_view view{};
};

/*
The type-level reserve value is an upper bound, while the returned cursor is
the only authority for the emitted extent.  Keeping the bound deliberately
larger than every corpus token verifies that a dispatcher does not confuse
capacity with logical length when it coalesces several fixed-reserve leaves.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_reserve_source>) noexcept
{
	return maximum_protocol_bound;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_reserve_source>, char *destination,
	fixed_reserve_source source) noexcept
{
	return copy_token(destination, source.view);
}

[[nodiscard]] inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, fixed_reserve_source>) noexcept
{
	/*
	The writer only copies an immutable corpus view, cannot throw, and has no
	visible state.  This explicit premise permits a fresh-result strategy to
	stage it before allocation; reserve shape alone intentionally does not.
	*/
	return {};
}

struct dynamic_reserve_source
{
	token_view view{};
};

/*
The dynamic query is a stable per-object upper bound and may be observed before
the writer.  It intentionally exceeds the actual extent so both implementations
must honor the writer's end cursor instead of treating the query as precise.
The source contains only an immutable view, making repeated sizing valid for
the complete logical print operation.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_reserve_source>,
	dynamic_reserve_source source) noexcept
{
	return source.view.size + 4u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_reserve_source>,
	char *destination, dynamic_reserve_source source) noexcept
{
	return copy_token(destination, source.view);
}

struct bounded_dynamic_source
{
	token_view view{};
};

/*
This source has the same conservative dynamic-reserve spelling as the ordinary
control, plus a destination-neutral one-pass bound.  Let L be the immutable
token extent and B=L+4.  The query returns B exactly when B<=maximum_size and
SIZE_MAX otherwise; it neither consumes the source nor allocates.  Therefore a
consumer may choose bounded materialization without replaying the writer or
turning failure-to-fit into a fatal size operation.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_source>,
	bounded_dynamic_source source) noexcept
{
	return source.view.size + 4u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_source>,
	char *destination, bounded_dynamic_source source) noexcept
{
	return copy_token(destination, source.view);
}

[[nodiscard]] inline constexpr ::std::true_type
	single_pass_bounded_materialization_preferred(
		::fast_io::io_reserve_type_t<char, bounded_dynamic_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::size_t
single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_source>,
	bounded_dynamic_source const &source, ::std::size_t maximum_size) noexcept
{
	auto const bound{source.view.size + 4u};
	return bound <= maximum_size
			   ? bound
			   : (::std::numeric_limits<::std::size_t>::max)();
}

[[nodiscard]] inline constexpr ::std::true_type
	print_eager_materialization_safe(
		::fast_io::io_reserve_type_t<char, bounded_dynamic_source>) noexcept
{
	// The immutable view has no observable sizing or formatting side effect.
	return {};
}

struct precise_reserve_source
{
	token_view view{};
};

/*
The ordinary dynamic protocol remains a conservative fallback.  The precise
protocol is a stronger, semantically identical proof: its query is exact and
its writer consumes exactly the supplied extent.  Returning `char *` satisfies
the stricter new contract while remaining accepted by the official old tree.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_reserve_source>,
	precise_reserve_source source) noexcept
{
	return source.view.size + 7u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_reserve_source>, char *destination,
	precise_reserve_source source) noexcept
{
	return copy_token(destination, source.view);
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_reserve_source>,
	precise_reserve_source source) noexcept
{
	return source.view.size;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_reserve_source>, char *destination,
	::std::size_t precise_size, precise_reserve_source source) noexcept
{
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		destination[index] = source.view.data[index];
	}
	return destination + precise_size;
}

struct precise_preferred_source
{
	token_view view{};
};

/*
The exact extent is a cached field of immutable corpus storage, so querying it
is constant-time, non-throwing, and independent of any result allocation.
Writing copies exactly that many bytes and returns destination+size.  The
separate markers make those cost and aliasing facts explicit; precise shape
alone would not authorize a fresh-result resize strategy.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_preferred_source>,
	precise_preferred_source source) noexcept
{
	return source.view.size + 7u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_preferred_source>,
	char *destination, precise_preferred_source source) noexcept
{
	return copy_token(destination, source.view);
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_preferred_source>,
	precise_preferred_source source) noexcept
{
	return source.view.size;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_preferred_source>,
	char *destination, ::std::size_t precise_size,
	precise_preferred_source source) noexcept
{
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		destination[index] = source.view.data[index];
	}
	return destination + precise_size;
}

[[nodiscard]] inline constexpr ::std::true_type
	print_precise_reserve_size_cached(
		::fast_io::io_reserve_type_t<char, precise_preferred_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_precise_reserve_output_growth_independent(
		::fast_io::io_reserve_type_t<char, precise_preferred_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_concat_fresh_precise_resize_preferred(
		::fast_io::io_reserve_type_t<char, precise_preferred_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_eager_materialization_safe(
		::fast_io::io_reserve_type_t<char, precise_preferred_source>) noexcept
{
	return {};
}

struct scatter_source
{
	token_view view{};
};

/*
The descriptor refers to bytes owned by the run-time corpus, which outlives all
source-pack tuples and every synchronous print/concat call.  This common
old/new source deliberately does not publish the new borrowed-scatter marker:
pointer shape and fixture lifetime are not a generic provenance proof, so a
new retained-descriptor optimization must not be enabled by the benchmark.
*/
inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scatter_source>,
	scatter_source source) noexcept
{
	return {source.view.data, source.view.size};
}

struct borrowed_scatter_source
{
	token_view view{};
};

/*
This companion source carries the same pointer/length representation as the
unmarked scatter, but its descriptor designates immutable corpus storage whose
lifetime strictly contains every source object and synchronous print call.
The marker is therefore a formal retention proof, not a benchmark hint: for
any two invocations i and j, producer(j) cannot mutate the interval returned by
producer(i).  Keeping the marked and unmarked types distinct lets the matrix
measure provenance-aware grouping without granting that property to a generic
scatter merely because this fixture happens to keep its bytes alive.
*/
inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, borrowed_scatter_source>,
	borrowed_scatter_source source) noexcept
{
	return {source.view.data, source.view.size};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, borrowed_scatter_source>) noexcept
{
	return {};
}

struct stable_scatter_source
{
	token_view view{};
};

/*
This positive-control source publishes the complete scatter proof lattice.
Its descriptor refers to immutable corpus storage independent of the producer
object and every destination cursor; the descriptor bytes are the complete
print semantics and observing them has no side effect.  Each marker states one
independent premise so dispatch cannot infer purity, copy stability, or direct
equivalence from the borrowed pointer/length shape alone.
*/
inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, stable_scatter_source>,
	stable_scatter_source source) noexcept
{
	return {source.view.data, source.view.size};
}

[[nodiscard]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, stable_scatter_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_scatter_output_state_independent(
		::fast_io::io_reserve_type_t<char, stable_scatter_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_scatter_direct_print_equivalent(
		::fast_io::io_reserve_type_t<char, stable_scatter_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_copy_stable_borrowed_source(
		::fast_io::io_reserve_type_t<char, stable_scatter_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_eager_materialization_safe(
		::fast_io::io_reserve_type_t<char, stable_scatter_source>) noexcept
{
	return {};
}

struct alias_source
{
	token_view view{};
};

/*
Aliasing returns a value proxy rather than a reference into the alias object.
The proxy's view designates corpus storage, so it stays valid even when source
normalization materializes or moves the proxy.  Both implementations are thus
required to normalize exactly once without relying on a temporary subobject's
lifetime, after which the fixed-reserve protocol is the sole formatter.
*/
inline constexpr fixed_reserve_source print_alias_define(
	::fast_io::io_alias_t, alias_source source) noexcept
{
	return {source.view};
}

static_assert(::fast_io::reserve_printable<char, fixed_reserve_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, dynamic_reserve_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, bounded_dynamic_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, precise_reserve_source>);
static_assert(::fast_io::precise_reserve_printable<char, precise_reserve_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, precise_preferred_source>);
static_assert(::fast_io::precise_reserve_printable<char, precise_preferred_source>);
static_assert(::fast_io::scatter_printable<char, scatter_source>);
static_assert(::fast_io::scatter_printable<char, borrowed_scatter_source>);
static_assert(::fast_io::scatter_printable<char, stable_scatter_source>);
static_assert(::fast_io::alias_printable<alias_source>);

template <source_family family>
struct source_type;

template <>
struct source_type<source_family::fixed_reserve>
{
	using type = fixed_reserve_source;
};

template <>
struct source_type<source_family::dynamic_reserve>
{
	using type = dynamic_reserve_source;
};

template <>
struct source_type<source_family::precise_reserve>
{
	using type = precise_reserve_source;
};

template <>
struct source_type<source_family::scatter>
{
	using type = scatter_source;
};

template <>
struct source_type<source_family::borrowed_scatter>
{
	using type = borrowed_scatter_source;
};

template <>
struct source_type<source_family::bounded_dynamic>
{
	using type = bounded_dynamic_source;
};

template <>
struct source_type<source_family::precise_preferred>
{
	using type = precise_preferred_source;
};

template <>
struct source_type<source_family::stable_scatter>
{
	using type = stable_scatter_source;
};

template <>
struct source_type<source_family::alias>
{
	using type = alias_source;
};

template <::std::size_t index>
inline constexpr source_family source_family_for_index{[]() consteval {
	if constexpr (selected_source_family == source_family::mixed)
	{
		return static_cast<source_family>(index % 5u);
	}
	else if constexpr (selected_source_family == source_family::mixed_borrowed)
	{
		constexpr auto slot{index % 5u};
		if constexpr (slot == 3u)
		{
			return source_family::borrowed_scatter;
		}
		else if constexpr (slot == 4u)
		{
			return source_family::alias;
		}
		else
		{
			return static_cast<source_family>(slot);
		}
	}
	else if constexpr (selected_source_family == source_family::mixed_proven)
	{
		constexpr auto slot{index % 5u};
		if constexpr (slot == 0u)
		{
			return source_family::fixed_reserve;
		}
		else if constexpr (slot == 1u)
		{
			return source_family::bounded_dynamic;
		}
		else if constexpr (slot == 2u)
		{
			return source_family::precise_preferred;
		}
		else if constexpr (slot == 3u)
		{
			return source_family::stable_scatter;
		}
		else
		{
			return source_family::alias;
		}
	}
	else
	{
		return selected_source_family;
	}
}()};

template <::std::size_t index>
using source_type_for_index = typename source_type<source_family_for_index<index>>::type;

template <::std::size_t index>
[[nodiscard]] inline constexpr source_type_for_index<index>
make_source(token_text const &token) noexcept
{
	return {view_of(token)};
}

template <::std::size_t... indices>
[[nodiscard]] inline constexpr auto make_source_pack_impl(
	corpus_record const &record, ::std::index_sequence<indices...>) noexcept
{
	/*
	The mixed schedules use deterministic F/D/P/S/A, F/D/P/B/A, and
	F/BD/PP/SS/A rotations.  B is the independently borrowed scatter; BD, PP,
	and SS publish bounded, precise-preferred, and complete scatter proofs.  Each
	supplies one heterogeneous parameter-pack type per selected N, so protocol
	precedence is stable across runs while the referenced bytes remain run-time
	random.
	*/
	return ::std::tuple<source_type_for_index<indices>...>{
		make_source<indices>(record.tokens[indices])...};
}

[[nodiscard]] inline constexpr auto make_source_pack(
	corpus_record const &record) noexcept
{
	return make_source_pack_impl(
		record, ::std::make_index_sequence<selected_pack_count>{});
}

using selected_source_pack = decltype(make_source_pack(
	::std::declval<corpus_record const &>()));
using source_pack_corpus = ::std::array<selected_source_pack, corpus_size>;

inline void build_source_packs(
	corpus_type const &corpus, source_pack_corpus &packs) noexcept
{
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		packs[index] = make_source_pack(corpus[index]);
	}
}

[[nodiscard]] inline constexpr char const *source_family_name() noexcept
{
	switch (selected_source_family)
	{
	case source_family::fixed_reserve:
		return "f";
	case source_family::dynamic_reserve:
		return "d";
	case source_family::precise_reserve:
		return "p";
	case source_family::scatter:
		return "s";
	case source_family::borrowed_scatter:
		return "bs";
	case source_family::alias:
		return "a";
	case source_family::mixed:
		return "mixed";
	case source_family::mixed_borrowed:
		return "mixed-borrowed";
	case source_family::bounded_dynamic:
		return "bd";
	case source_family::precise_preferred:
		return "pp";
	case source_family::stable_scatter:
		return "ss";
	case source_family::mixed_proven:
		return "mixed-proven";
	}
	return "invalid";
}

} // namespace fast_io_cpo_matrix
