#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

inline constexpr ::std::size_t fixed_reserve_bound{16u};

enum class source_family : unsigned char
{
	fixed_reserve,
	dynamic_reserve,
	precise_reserve,
	scatter,
	alias,
	borrowed_scatter,
	mixed
};

inline void matrix_require(bool condition) noexcept
{
	/* Release test configurations define NDEBUG.  This executable semantic
	   proof must therefore remain active independently of the assert macro. */
	if (!condition)
	{
		::std::abort();
	}
}

struct protocol_counts
{
	::std::size_t fixed_defines{};
	::std::size_t dynamic_sizes{};
	::std::size_t dynamic_defines{};
	::std::size_t precise_dynamic_sizes{};
	::std::size_t precise_dynamic_defines{};
	::std::size_t precise_sizes{};
	::std::size_t precise_defines{};
	::std::size_t scatter_defines{};
	::std::size_t alias_defines{};
	::std::size_t alias_proxy_defines{};
	/* The address is inspected only while its source witness is live.  The
	   separate Boolean transports the observation fact beyond source death
	   without evaluating an invalid pointer value. */
	char const *observed_source_base{};
	bool observed_source{};

	[[nodiscard]] inline constexpr ::std::size_t total_calls() const noexcept
	{
		return fixed_defines + dynamic_sizes + dynamic_defines +
			   precise_dynamic_sizes + precise_dynamic_defines + precise_sizes +
			   precise_defines + scatter_defines + alias_defines +
			   alias_proxy_defines;
	}
};

struct tracked_view
{
	char const *data{};
	::std::size_t size{};
	protocol_counts *counts{};
	bool const *alive{};
};

inline void prove_live(tracked_view view) noexcept
{
	matrix_require(view.data != nullptr);
	matrix_require(view.counts != nullptr);
	matrix_require(view.alive != nullptr && *view.alive);
}

inline char *copy_view(char *destination, tracked_view view) noexcept
{
	prove_live(view);
	for (::std::size_t index{}; index != view.size; ++index)
	{
		destination[index] = view.data[index];
	}
	return destination + view.size;
}

struct fixed_source
{
	tracked_view view{};
};

/*
Formal fixed-reserve invariant: the type-level CPO supplies only a physical
upper bound.  Exactly one writer invocation must publish the logical endpoint;
unused capacity is neither output nor an additional observable source pass.
*/
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_source>) noexcept
{
	return fixed_reserve_bound;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_source>, char *destination,
	fixed_source source) noexcept
{
	++source.view.counts->fixed_defines;
	return copy_view(destination, source.view);
}

struct dynamic_source
{
	tracked_view view{};
};

/*
Formal dynamic-reserve invariant: sizing is a stable, non-consuming upper-bound
observation and emission returns the only logical endpoint.  A composite may
cache that one observation, but it may not replay either CPO for this fixture.
*/
inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_source>,
	dynamic_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->dynamic_sizes;
	return source.view.size + 3u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_source>, char *destination,
	dynamic_source source) noexcept
{
	++source.view.counts->dynamic_defines;
	return copy_view(destination, source.view);
}

struct precise_source
{
	tracked_view view{};
};

/*
The ordinary and precise spellings are semantically identical alternatives.
A valid dispatch state selects exactly one complete pair: dynamic size/define
or precise size/define.  Mixing their writers or emitting twice would violate
the single-publication proof even when both byte sequences happen to match.
*/
inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_source>,
	precise_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->precise_dynamic_sizes;
	return source.view.size + 5u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_source>, char *destination,
	precise_source source) noexcept
{
	++source.view.counts->precise_dynamic_defines;
	return copy_view(destination, source.view);
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_source>,
	precise_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->precise_sizes;
	return source.view.size;
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_source>, char *destination,
	::std::size_t precise_size, precise_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->precise_defines;
	matrix_require(precise_size == source.view.size);
	return copy_view(destination, source.view);
}

struct scatter_source
{
	tracked_view view{};
};

/*
This descriptor names fixture-owned bytes which remain live through the
synchronous call, but the type intentionally publishes no borrowed/repeatable
marker.  Consequently shape proves one immediate observation only; replay or
retention would be an unjustified provenance assumption and is rejected by the
exact one-call assertion below.
*/
inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scatter_source>,
	scatter_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->scatter_defines;
	source.view.counts->observed_source_base = source.view.data;
	source.view.counts->observed_source = true;
	return {source.view.data, source.view.size};
}

struct borrowed_scatter_source
{
	tracked_view view{};
};

/*
The descriptor designates immutable fixture storage whose lifetime contains
the complete synchronous print call.  For any two producer evaluations i and
j in that call, evaluating j cannot mutate or end the interval returned by i.
The marker therefore proves descriptor retention independently of pointer
shape; the otherwise identical unmarked source remains the negative control.
*/
inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, borrowed_scatter_source>,
	borrowed_scatter_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->scatter_defines;
	source.view.counts->observed_source_base = source.view.data;
	source.view.counts->observed_source = true;
	return {source.view.data, source.view.size};
}

[[nodiscard]] inline constexpr ::std::true_type
	print_borrowed_scatter_source(
		::fast_io::io_reserve_type_t<char, borrowed_scatter_source>) noexcept
{
	return {};
}

struct alias_proxy
{
	tracked_view view{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, alias_proxy>) noexcept
{
	return fixed_reserve_bound;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, alias_proxy>, char *destination,
	alias_proxy proxy) noexcept
{
	prove_live(proxy.view);
	++proxy.view.counts->alias_proxy_defines;
	proxy.view.counts->observed_source_base = proxy.view.data;
	proxy.view.counts->observed_source = true;
	return copy_view(destination, proxy.view);
}

struct alias_source
{
	tracked_view view{};
};

/*
Alias normalization returns a value proxy carrying an externally owned view.
No reference to the alias_source object itself escapes.  The proxy therefore
survives normalization moves while its byte lifetime remains explicitly tied
to the fixture's `alive` witness.
*/
inline alias_proxy print_alias_define(
	::fast_io::io_alias_t, alias_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->alias_defines;
	return {source.view};
}

static_assert(::fast_io::reserve_printable<char, fixed_source>);
static_assert(::fast_io::dynamic_reserve_printable<char, dynamic_source>);
static_assert(::fast_io::precise_reserve_printable<char, precise_source>);
static_assert(::fast_io::scatter_printable<char, scatter_source>);
static_assert(!::fast_io::borrowed_scatter_source<char, scatter_source>);
static_assert(::fast_io::scatter_printable<char, borrowed_scatter_source>);
static_assert(
	::fast_io::borrowed_scatter_source<char, borrowed_scatter_source>);
static_assert(::fast_io::alias_printable<alias_source>);

template <::std::size_t count>
struct fixture
{
	::std::array<::std::array<char, 7u>, count> storage{};
	::std::array<::std::size_t, count> sizes{};
	::std::array<protocol_counts, count> counters{};
	::std::array<bool, count> alive{};

	fixture() noexcept
	{
		for (::std::size_t argument{}; argument != count; ++argument)
		{
			alive[argument] = true;
			sizes[argument] = 1u + argument % 6u;
			for (::std::size_t index{}; index != sizes[argument]; ++index)
			{
				storage[argument][index] = static_cast<char>(
					'a' + (argument * 7u + index) % 26u);
			}
		}
	}

	~fixture()
	{
		for (auto &witness : alive)
		{
			witness = false;
		}
	}

	[[nodiscard]] tracked_view view(::std::size_t index) noexcept
	{
		return {storage[index].data(), sizes[index],
				__builtin_addressof(counters[index]),
				__builtin_addressof(alive[index])};
	}

	[[nodiscard]] ::std::string expected(bool line) const
	{
		::std::string result;
		for (::std::size_t argument{}; argument != count; ++argument)
		{
			result.append(storage[argument].data(), sizes[argument]);
		}
		if (line)
		{
			result.push_back('\n');
		}
		return result;
	}
};

template <source_family family>
struct source_type;

template <>
struct source_type<source_family::fixed_reserve>
{
	using type = fixed_source;
};

template <>
struct source_type<source_family::dynamic_reserve>
{
	using type = dynamic_source;
};

template <>
struct source_type<source_family::precise_reserve>
{
	using type = precise_source;
};

template <>
struct source_type<source_family::scatter>
{
	using type = scatter_source;
};

template <>
struct source_type<source_family::alias>
{
	using type = alias_source;
};

template <>
struct source_type<source_family::borrowed_scatter>
{
	using type = borrowed_scatter_source;
};

template <source_family family, ::std::size_t index>
inline constexpr source_family effective_family{
	family == source_family::mixed
		? static_cast<source_family>(index % 5u)
		: family};

template <source_family family, ::std::size_t index>
using source_for = typename source_type<effective_family<family, index>>::type;

template <source_family family, ::std::size_t count, ::std::size_t... indices>
[[nodiscard]] auto make_pack(
	fixture<count> &values, ::std::index_sequence<indices...>) noexcept
{
	return ::std::tuple<source_for<family, indices>...>{
		source_for<family, indices>{values.view(indices)}...};
}

inline void verify_counts(protocol_counts const &counts, source_family family)
{
	switch (family)
	{
	case source_family::fixed_reserve:
		matrix_require(counts.fixed_defines == 1u);
		matrix_require(counts.total_calls() == 1u);
		break;
	case source_family::dynamic_reserve:
		matrix_require(counts.dynamic_sizes == 1u);
		matrix_require(counts.dynamic_defines == 1u);
		matrix_require(counts.total_calls() == 2u);
		break;
	case source_family::precise_reserve:
	{
		bool const dynamic_pair{
			counts.precise_dynamic_sizes == 1u &&
			counts.precise_dynamic_defines == 1u &&
			counts.precise_sizes == 0u && counts.precise_defines == 0u};
		bool const precise_pair{
			counts.precise_dynamic_sizes == 0u &&
			counts.precise_dynamic_defines == 0u &&
			counts.precise_sizes == 1u && counts.precise_defines == 1u};
		matrix_require(dynamic_pair != precise_pair);
		matrix_require(counts.total_calls() == 2u);
		break;
	}
	case source_family::scatter:
	case source_family::borrowed_scatter:
		matrix_require(counts.scatter_defines == 1u);
		matrix_require(counts.observed_source);
		matrix_require(counts.total_calls() == 1u);
		break;
	case source_family::alias:
		matrix_require(counts.alias_defines == 1u);
		matrix_require(counts.alias_proxy_defines == 1u);
		matrix_require(counts.observed_source);
		matrix_require(counts.total_calls() == 2u);
		break;
	case source_family::mixed:
		matrix_require(false);
	}
}

inline void verify_bytes(
	char const *actual, ::std::size_t actual_size,
	::std::string_view expected) noexcept
{
	matrix_require(actual_size == expected.size());
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		matrix_require(actual[index] == expected[index]);
	}
}

template <source_family family, ::std::size_t count, bool line>
void run_case()
{
	fixture<count> values;
	auto sources{make_pack<family>(
		values, ::std::make_index_sequence<count>{})};
	::std::array<char, count * fixed_reserve_bound + 1u> storage{};
	::fast_io::basic_obuffer_view<char> output{
		storage.data(), storage.data() + storage.size()};
	::std::apply(
		[&output](auto &...arguments) {
			::fast_io::operations::print_freestanding<line>(
				output, arguments...);
		},
		sources);
	auto const expected{values.expected(line)};
	verify_bytes(storage.data(), output.size(), expected);
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const family_for_argument{
			family == source_family::mixed
				? static_cast<source_family>(index % 5u)
				: family};
		verify_counts(values.counters[index], family_for_argument);
		matrix_require(values.alive[index]);
		if (family_for_argument == source_family::scatter ||
			family_for_argument == source_family::borrowed_scatter ||
			family_for_argument == source_family::alias)
		{
			/* Provenance is exact, not merely non-null: the selected CPO must
			   observe this argument's fixture interval.  The public output owns a
			   disjoint buffer and cannot publish that interval as output storage. */
			matrix_require(values.counters[index].observed_source_base ==
						   values.storage[index].data());
			matrix_require(storage.data() != values.counters[index].observed_source_base);
		}
	}
}

struct lifetime_alias_proxy
{
	tracked_view view{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, lifetime_alias_proxy>) noexcept
{
	return fixed_reserve_bound;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, lifetime_alias_proxy>,
	char *destination, lifetime_alias_proxy proxy) noexcept
{
	prove_live(proxy.view);
	++proxy.view.counts->alias_proxy_defines;
	proxy.view.counts->observed_source_base = proxy.view.data;
	proxy.view.counts->observed_source = true;
	return copy_view(destination, proxy.view);
}

struct lifetime_alias_source
{
	::std::array<char, 5u> storage{'a', 'l', 'i', 'a', 's'};
	protocol_counts *counts{};
	bool *alive{};
	bool owns_witness{true};

	/* `owns_witness` is a linear destruction token.  Each normalization move
	   transfers the sole right to close the external lifetime interval; a
	   moved-from transport cannot report a premature death. */

	lifetime_alias_source(protocol_counts &observations, bool &witness) noexcept
		: counts(__builtin_addressof(observations)),
		  alive(__builtin_addressof(witness))
	{
		witness = true;
	}

	lifetime_alias_source(lifetime_alias_source const &) = delete;
	lifetime_alias_source &operator=(lifetime_alias_source const &) = delete;

	lifetime_alias_source(lifetime_alias_source &&other) noexcept
		: storage(other.storage), counts(other.counts), alive(other.alive),
		  owns_witness(other.owns_witness)
	{
		other.owns_witness = false;
	}

	~lifetime_alias_source()
	{
		if (owns_witness)
		{
			*alive = false;
		}
	}
};

inline lifetime_alias_proxy print_alias_define(
	::fast_io::io_alias_t, lifetime_alias_source const &source) noexcept
{
	matrix_require(*source.alive);
	++source.counts->alias_defines;
	return {{source.storage.data(), source.storage.size(), source.counts,
			 source.alive}};
}

struct lifetime_scatter_source
{
	::std::array<char, 7u> storage{'s', 'c', 'a', 't', 't', 'e', 'r'};
	protocol_counts *counts{};
	bool *alive{};
	bool owns_witness{true};

	/* The same linear token models every implementation move before scatter
	   observation.  Thus a live witness inside the CPO identifies the current
	   owner, while a false witness after the call proves final destruction. */

	lifetime_scatter_source(protocol_counts &observations, bool &witness) noexcept
		: counts(__builtin_addressof(observations)),
		  alive(__builtin_addressof(witness))
	{
		witness = true;
	}

	lifetime_scatter_source(lifetime_scatter_source const &) = delete;
	lifetime_scatter_source &operator=(lifetime_scatter_source const &) = delete;

	lifetime_scatter_source(lifetime_scatter_source &&other) noexcept
		: storage(other.storage), counts(other.counts), alive(other.alive),
		  owns_witness(other.owns_witness)
	{
		other.owns_witness = false;
	}

	~lifetime_scatter_source()
	{
		if (owns_witness)
		{
			*alive = false;
		}
	}
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, lifetime_scatter_source>,
	lifetime_scatter_source const &source) noexcept
{
	matrix_require(*source.alive);
	++source.counts->scatter_defines;
	source.counts->observed_source_base = source.storage.data();
	source.counts->observed_source = true;
	return {source.storage.data(), source.storage.size()};
}

static_assert(::fast_io::alias_printable<lifetime_alias_source>);
static_assert(::fast_io::scatter_printable<char, lifetime_scatter_source>);
static_assert(
	!::fast_io::borrowed_scatter_source<char, lifetime_scatter_source>);

void verify_temporary_lifetimes()
{
	/* Neither source opts into retained scatter provenance.  Successful byte
	   verification after the unique owner closes its lifetime is therefore a
	   constructive proof that print eagerly copied every borrowed character. */
	{
		protocol_counts counts;
		bool alive{};
		::std::array<char, 32u> storage{};
		::fast_io::basic_obuffer_view<char> output{
			storage.data(), storage.data() + storage.size()};
		::fast_io::operations::print_freestanding<true>(
			output, lifetime_alias_source{counts, alive});
		matrix_require(!alive);
		verify_bytes(storage.data(), output.size(), "alias\n");
		verify_counts(counts, source_family::alias);
	}
	{
		protocol_counts counts;
		bool alive{};
		::std::array<char, 32u> storage{};
		::fast_io::basic_obuffer_view<char> output{
			storage.data(), storage.data() + storage.size()};
		::fast_io::operations::print_freestanding<true>(
			output, lifetime_scatter_source{counts, alive});
		matrix_require(!alive);
		verify_bytes(storage.data(), output.size(), "scatter\n");
		verify_counts(counts, source_family::scatter);
	}
}

template <source_family family>
void run_singleton_protocol()
{
	run_case<family, 1u, false>();
	run_case<family, 1u, true>();
}

template <::std::size_t count>
void run_mixed_count()
{
	run_case<source_family::mixed, count, false>();
	run_case<source_family::mixed, count, true>();
}

} // namespace

int main()
{
	/* Singleton cases isolate each protocol's state machine; mixed cases prove
	   ordered composition at every selected parameter-pack cardinality. */
	run_singleton_protocol<source_family::fixed_reserve>();
	run_singleton_protocol<source_family::dynamic_reserve>();
	run_singleton_protocol<source_family::precise_reserve>();
	run_singleton_protocol<source_family::scatter>();
	run_singleton_protocol<source_family::alias>();
	run_singleton_protocol<source_family::borrowed_scatter>();
	/* Eight retained descriptors are the first admitted vector-copy cardinality;
	   N=32 proves that the same loop invariant composes beyond one small pack. */
	run_case<source_family::borrowed_scatter, 8u, false>();
	run_case<source_family::borrowed_scatter, 8u, true>();
	run_case<source_family::borrowed_scatter, 32u, false>();
	run_case<source_family::borrowed_scatter, 32u, true>();
	run_mixed_count<1u>();
	run_mixed_count<2u>();
	run_mixed_count<8u>();
	run_mixed_count<32u>();
	verify_temporary_lifetimes();
}
