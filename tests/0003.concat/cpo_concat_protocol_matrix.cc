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
#include <fast_io_dsal/string.h>
#include <fast_io_unit/string.h>

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
	mixed,
	mixed_borrowed
};

enum class result_family : unsigned char
{
	standard_string,
	fast_io_string
};

inline void matrix_require(bool condition) noexcept
{
	/* Release test configurations define NDEBUG.  Protocol counts, lifetime
	   witnesses, and full-byte checks must remain executable in every mode. */
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
The fixed bound is capacity, never logical length.  Concat may select stack,
SSO, direct-result, or append storage, but every valid destination strategy
must invoke the source writer exactly once and construct only its returned
prefix.  This is the endpoint induction used by the full-byte assertion.
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
The dynamic query is a stable non-consuming upper bound.  A concat strategy may
cache it across allocation and emission, but a second observation or writer
call would be an unnecessary source replay and violates this matrix contract.
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
Destination capabilities may make either representation cheaper.  The formal
choice is exclusive: one dynamic size/define pair or one exact size/define
pair.  Both pairs spell identical bytes; crossing the pairs, measuring twice,
or invoking both writers would invalidate concat's one-publication proof.
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
The source proves descriptor shape and synchronous fixture lifetime only.  It
does not opt into borrowed/repeatable provenance.  Concat must therefore copy
the descriptor exactly once into destination-owned storage and may neither
retain nor replay it during a measure-then-emit strategy.
*/
[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
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
The result must copy its bytes, but planning may retain descriptors during the
synchronous construction.  Immutable fixture storage outlives that complete
interval, and no later producer evaluation can mutate an earlier descriptor's
range.  This marker is the positive provenance proof paired with the otherwise
identical unmarked scatter source above.
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

[[maybe_unused]] [[nodiscard]] inline constexpr ::std::true_type
	print_borrowed_scatter_source(
		::fast_io::io_reserve_type_t<char, borrowed_scatter_source>) noexcept
{
	return {};
}

struct scratch_scatter_source
{
	char value{};
	::std::size_t *defines{};
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scratch_scatter_source>,
	scratch_scatter_source source) noexcept
{
	static char shared_scratch;
	++*source.defines;
	shared_scratch = source.value;
	return {__builtin_addressof(shared_scratch), 1u};
}

static_assert(::fast_io::scatter_printable<char, scratch_scatter_source>);
static_assert(!::fast_io::borrowed_scatter_source<char, scratch_scatter_source>);

template <::std::size_t extent>
struct sized_scratch_scatter_source
{
	char value{};
	::std::size_t *defines{};
};

template <::std::size_t extent>
inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, sized_scratch_scatter_source<extent>>,
	sized_scratch_scatter_source<extent> source) noexcept
{
	static ::std::array<char, extent> shared_scratch{};
	++*source.defines;
	shared_scratch.fill(source.value);
	return {shared_scratch.data(), shared_scratch.size()};
}

inline constexpr ::std::size_t promotion_scratch_extent{96u};
using promotion_scratch_scatter_source =
	sized_scratch_scatter_source<promotion_scratch_extent>;

inline constexpr ::std::size_t oversized_scratch_extent{
	::fast_io::details::basic_concat_buffer<char>::buffer_size + 257u};
using oversized_scratch_scatter_source =
	sized_scratch_scatter_source<oversized_scratch_extent>;

static_assert(::fast_io::scatter_printable<char, promotion_scratch_scatter_source>);
static_assert(!::fast_io::borrowed_scatter_source<char, promotion_scratch_scatter_source>);
static_assert(::fast_io::scatter_printable<char, oversized_scratch_scatter_source>);
static_assert(!::fast_io::borrowed_scatter_source<char, oversized_scratch_scatter_source>);

struct ordered_staging_status_probe_source
{
};

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordered_staging_status_probe_source>,
	ordered_staging_status_probe_source) noexcept
{
	static constexpr char value{'x'};
	return {__builtin_addressof(value), 1u};
}

struct ordered_staging_status_probe_result
{
};

[[maybe_unused]] [[nodiscard]] inline ordered_staging_status_probe_result strlike_construct_define(
	::fast_io::io_strlike_type_t<char, ordered_staging_status_probe_result>,
	char const *, char const *) noexcept
{
	return {};
}

[[maybe_unused]] [[nodiscard]] inline constexpr ::std::size_t concat_ordered_staging_minimum_leaf_count(
	::fast_io::io_strlike_type_t<char, ordered_staging_status_probe_result>) noexcept
{
	return 1u;
}

template <bool line>
	requires(!line)
[[maybe_unused]] inline void status_print_define(
	::fast_io::io_strlike_reference_wrapper<
		char, ::fast_io::details::basic_concat_buffer<char>>,
	ordered_staging_status_probe_source) noexcept
{
}

struct ordered_original_status_probe_output
{
	using output_char_type = char;
};

struct ordered_original_status_probe_source
{
};

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordered_original_status_probe_source>,
	ordered_original_status_probe_source) noexcept
{
	static constexpr char value{'y'};
	return {__builtin_addressof(value), 1u};
}

struct ordered_original_status_probe_result
{
};

[[maybe_unused]] [[nodiscard]] inline ordered_original_status_probe_result strlike_construct_define(
	::fast_io::io_strlike_type_t<char, ordered_original_status_probe_result>,
	char const *, char const *) noexcept
{
	return {};
}

[[maybe_unused]] [[nodiscard]] inline constexpr ::std::size_t concat_ordered_staging_minimum_leaf_count(
	::fast_io::io_strlike_type_t<char, ordered_original_status_probe_result>) noexcept
{
	return 1u;
}

[[maybe_unused]] [[nodiscard]] inline ordered_original_status_probe_output io_strlike_ref(
	::fast_io::io_alias_t, ordered_original_status_probe_result &) noexcept
{
	return {};
}

template <bool line>
	requires line
[[maybe_unused]] inline void status_print_define(
	ordered_original_status_probe_output,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source) noexcept
{
}

// These recognition-only sentinels make the cost/source candidate true on every target. The first exact status hook
// belongs only to concat's fixed staging adapter; the second belongs only to the original result adapter and line mode.
// Neither function is executed. Rejecting both final policies proves that ordered per-leaf dispatch cannot intercept a
// provider operation or bypass a whole-record provider while the underlying candidate remains independently visible.
static_assert(::fast_io::details::decay::basic_general_concat_ordered_staging_candidate_v<
	char, ordered_staging_status_probe_result,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source>);
static_assert(!::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
	false, char, ordered_staging_status_probe_result,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source,
	ordered_staging_status_probe_source, ordered_staging_status_probe_source>);
static_assert(::fast_io::details::decay::basic_general_concat_ordered_staging_candidate_v<
	char, ordered_original_status_probe_result,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source>);
static_assert(!::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
	true, char, ordered_original_status_probe_result,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source,
	ordered_original_status_probe_source, ordered_original_status_probe_source>);

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
struct ordered_adaptive_status_probe_source
{
};

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordered_adaptive_status_probe_source>,
	ordered_adaptive_status_probe_source) noexcept
{
	static constexpr char value{'z'};
	return {__builtin_addressof(value), 1u};
}

template <bool line>
	requires(!line)
[[maybe_unused]] inline void status_print_define(
	::fast_io::io_strlike_reference_wrapper<
		char, ::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<
			  char, ::fast_io::string>>,
	ordered_adaptive_status_probe_source) noexcept
{
}

// The adaptive staging adapter is a distinct ADL surface from the fixed two-KiB adapter above. This target-specific
// sentinel proves the final gate asks about that exact cached-cursor output type before enabling the seven-leaf policy.
static_assert(::fast_io::details::decay::basic_general_concat_ordered_staging_adaptive_destination<
	char, ::fast_io::string>);
static_assert(::fast_io::details::decay::basic_general_concat_ordered_staging_candidate_v<
	char, ::fast_io::string,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source>);
static_assert(!::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
	false, char, ::fast_io::string,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source, ordered_adaptive_status_probe_source,
	ordered_adaptive_status_probe_source>);

struct ordered_adaptive_direct_probe_source
{
};

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordered_adaptive_direct_probe_source>,
	ordered_adaptive_direct_probe_source) noexcept
{
	static constexpr char value{'w'};
	return {__builtin_addressof(value), 1u};
}

[[maybe_unused]] inline void print_define(
	::fast_io::io_reserve_type_t<char, ordered_adaptive_direct_probe_source>,
	::fast_io::io_strlike_reference_wrapper<
		char, ::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<
			  char, ::fast_io::string>>,
	ordered_adaptive_direct_probe_source) noexcept
{
}

using ordered_adaptive_probe_output = ::fast_io::io_strlike_reference_wrapper<
	char, ::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<
		  char, ::fast_io::string>>;

// An output-specific direct CPO is invisible to the historical dummy-output `printable` probe, but it still disproves
// the staging policy's destination-independent source model. The candidate is intentionally kept true so rejection can
// be attributed to the completed-output ADL gate rather than to scatter shape, pack cost, or public printability.
static_assert(!::fast_io::printable<char, ordered_adaptive_direct_probe_source>);
static_assert(::fast_io::details::direct_printable_to<
	char, ordered_adaptive_probe_output,
	ordered_adaptive_direct_probe_source>);
static_assert(::fast_io::details::decay::basic_general_concat_ordered_staging_candidate_v<
	char, ::fast_io::string,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source>);
static_assert(!::fast_io::details::decay::basic_general_concat_ordered_staging_run_v<
	false, char, ::fast_io::string,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source, ordered_adaptive_direct_probe_source,
	ordered_adaptive_direct_probe_source>);
#endif

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
Normalization returns an owned proxy value, not a reference to the alias_source
transport.  The proxy deliberately remains a non-owning byte view so the test
separately proves that the final std/fast_io result copies those bytes before
the fixture or temporary owner expires.
*/
inline alias_proxy print_alias_define(
	::fast_io::io_alias_t, alias_source source) noexcept
{
	prove_live(source.view);
	++source.view.counts->alias_defines;
	return {source.view};
}

struct ordered_reserve_dynamic_priority_probe
{
};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_dynamic_priority_probe>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_dynamic_priority_probe>,
	ordered_reserve_dynamic_priority_probe) noexcept
{
	return 1u;
}

[[maybe_unused]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_dynamic_priority_probe>,
	char *destination, ordered_reserve_dynamic_priority_probe) noexcept
{
	*destination = 'r';
	return destination + 1u;
}

struct ordered_reserve_scatter_priority_probe
{
};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_scatter_priority_probe>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_scatter_priority_probe>,
	char *destination, ordered_reserve_scatter_priority_probe) noexcept
{
	*destination = 'r';
	return destination + 1u;
}

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char,
		ordered_reserve_scatter_priority_probe>,
	ordered_reserve_scatter_priority_probe) noexcept
{
	static constexpr char value{'s'};
	return {__builtin_addressof(value), 1u};
}

struct ordered_dynamic_scatter_priority_probe
{
};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char,
		ordered_dynamic_scatter_priority_probe>,
	ordered_dynamic_scatter_priority_probe) noexcept
{
	return 1u;
}

[[maybe_unused]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char,
		ordered_dynamic_scatter_priority_probe>,
	char *destination, ordered_dynamic_scatter_priority_probe) noexcept
{
	*destination = 'd';
	return destination + 1u;
}

[[maybe_unused]] inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char,
		ordered_dynamic_scatter_priority_probe>,
	ordered_dynamic_scatter_priority_probe) noexcept
{
	static constexpr char value{'s'};
	return {__builtin_addressof(value), 1u};
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

// The specialized adaptive emitter duplicates only the destination-independent portion of ordinary leaf priority.
// Static reserve therefore remains admissible when dynamic reserve is also present. Any overlap with scatter remains
// on the generic dispatcher because static-scatter representation and destination retention can reverse that choice.
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, fixed_source>);
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, dynamic_source>);
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, precise_source>);
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, scatter_source>);
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, alias_proxy>);
static_assert(
	::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, ordered_reserve_dynamic_priority_probe>);
static_assert(
	!::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, ordered_reserve_scatter_priority_probe>);
static_assert(
	!::fast_io::details::decay::basic_general_concat_ordered_one_shot_leaf_v<
		char, ordered_dynamic_scatter_priority_probe>);

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
					'A' + (argument * 11u + index) % 26u);
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
	[]() consteval {
		if constexpr (family == source_family::mixed ||
					  family == source_family::mixed_borrowed)
		{
			constexpr auto slot{index % 5u};
			if constexpr (family == source_family::mixed_borrowed && slot == 3u)
			{
				return source_family::borrowed_scatter;
			}
			else
			{
				return static_cast<source_family>(slot);
			}
		}
		else
		{
			return family;
		}
	}()};

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
	case source_family::mixed_borrowed:
		matrix_require(false);
	}
}

template <typename Result>
inline void verify_bytes(Result const &actual, ::std::string_view expected)
{
	matrix_require(actual.size() == expected.size());
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		matrix_require(actual.data()[index] == expected[index]);
	}
}

template <result_family result, bool line, typename Pack>
[[nodiscard]] auto concat_pack(Pack &sources)
{
	return ::std::apply(
		[](auto &...arguments) {
			if constexpr (result == result_family::standard_string)
			{
				if constexpr (line)
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
				if constexpr (line)
				{
					return ::fast_io::concatln_fast_io(arguments...);
				}
				else
				{
					return ::fast_io::concat_fast_io(arguments...);
				}
			}
		},
		sources);
}

template <result_family result, source_family family, ::std::size_t count,
		  bool line>
void run_case()
{
	fixture<count> values;
	auto sources{make_pack<family>(
		values, ::std::make_index_sequence<count>{})};
	auto output{concat_pack<result, line>(sources)};
	auto const expected{values.expected(line)};
	verify_bytes(output, expected);
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const family_for_argument{
			family == source_family::mixed_borrowed && index % 5u == 3u
				? source_family::borrowed_scatter
				: (family == source_family::mixed ||
						   family == source_family::mixed_borrowed
					   ? static_cast<source_family>(index % 5u)
					   : family)};
		verify_counts(values.counters[index], family_for_argument);
		matrix_require(values.alive[index]);
		if (family_for_argument == source_family::scatter ||
			family_for_argument == source_family::borrowed_scatter ||
			family_for_argument == source_family::alias)
		{
			/* Provenance is exact, not merely non-null: dispatch must observe this
			   argument's fixture interval.  The owning result cannot publish that
			   borrowed interval; pointer inequality is checked while both live. */
			matrix_require(values.counters[index].observed_source_base ==
						   values.storage[index].data());
			matrix_require(output.data() !=
						   values.counters[index].observed_source_base);
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
	   owner, while a false witness after concat proves final destruction. */

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

template <result_family result>
void verify_temporary_lifetimes()
{
	/* Both sources are deliberately ineligible for retained scatter planning.
	   Full-byte equality after their unique owners die proves that each result
	   acquired independent storage before returning to its caller. */
	{
		protocol_counts counts;
		bool alive{};
		auto output{[&] {
			if constexpr (result == result_family::standard_string)
			{
				return ::fast_io::concatln_std(
					lifetime_alias_source{counts, alive});
			}
			else
			{
				return ::fast_io::concatln_fast_io(
					lifetime_alias_source{counts, alive});
			}
		}()};
		matrix_require(!alive);
		verify_bytes(output, "alias\n");
		verify_counts(counts, source_family::alias);
	}
	{
		protocol_counts counts;
		bool alive{};
		auto output{[&] {
			if constexpr (result == result_family::standard_string)
			{
				return ::fast_io::concatln_std(
					lifetime_scatter_source{counts, alive});
			}
			else
			{
				return ::fast_io::concatln_fast_io(
					lifetime_scatter_source{counts, alive});
			}
		}()};
		matrix_require(!alive);
		verify_bytes(output, "scatter\n");
		verify_counts(counts, source_family::scatter);
	}
}

template <result_family result>
void verify_long_scratch_scatter_order()
{
	::std::size_t defines{};
	auto output{[](::std::size_t &count) {
		if constexpr (result == result_family::standard_string)
		{
			return ::fast_io::concat_std(
				scratch_scatter_source{'A', __builtin_addressof(count)},
				scratch_scatter_source{'B', __builtin_addressof(count)},
				scratch_scatter_source{'C', __builtin_addressof(count)},
				scratch_scatter_source{'D', __builtin_addressof(count)},
				scratch_scatter_source{'E', __builtin_addressof(count)},
				scratch_scatter_source{'F', __builtin_addressof(count)},
				scratch_scatter_source{'G', __builtin_addressof(count)},
				scratch_scatter_source{'H', __builtin_addressof(count)});
		}
		else
		{
			return ::fast_io::concat_fast_io(
				scratch_scatter_source{'A', __builtin_addressof(count)},
				scratch_scatter_source{'B', __builtin_addressof(count)},
				scratch_scatter_source{'C', __builtin_addressof(count)},
				scratch_scatter_source{'D', __builtin_addressof(count)},
				scratch_scatter_source{'E', __builtin_addressof(count)},
				scratch_scatter_source{'F', __builtin_addressof(count)},
				scratch_scatter_source{'G', __builtin_addressof(count)},
				scratch_scatter_source{'H', __builtin_addressof(count)});
		}
	}(defines)};

	// The eight-leaf record reaches the measured ordered-staging threshold on supported targets. Every descriptor
	// aliases the same byte, so `ABCDEFGH` proves each scatter was copied before the following CPO overwrote that byte;
	// a descriptor-table plan would instead observe eight copies of `H`.
	verify_bytes(output, "ABCDEFGH");
	matrix_require(defines == 8u);
}

template <result_family result>
void verify_promoted_scratch_scatter_order()
{
	::std::size_t defines{};
	auto output{[](::std::size_t &count) {
		if constexpr (result == result_family::standard_string)
		{
			return ::fast_io::concat_std(
				promotion_scratch_scatter_source{'A', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'B', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'C', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'D', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'E', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'F', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'G', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'H', __builtin_addressof(count)});
		}
		else
		{
			return ::fast_io::concat_fast_io(
				promotion_scratch_scatter_source{'A', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'B', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'C', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'D', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'E', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'F', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'G', __builtin_addressof(count)},
				promotion_scratch_scatter_source{'H', __builtin_addressof(count)});
		}
	}(defines)};

	// Eight 96-character descriptors cross the adaptive destination's 512-character boundary during `F`. Every CPO
	// overwrites the one shared array, so exact A--H blocks prove `F` was consumed before the destination-only promotion
	// and before `G` ran. A replay would increment the count; descriptor retention would expose later bytes.
	matrix_require(output.size() == 8u * promotion_scratch_extent);
	for (::std::size_t block{}; block != 8u; ++block)
	{
		for (::std::size_t index{}; index != promotion_scratch_extent; ++index)
		{
			matrix_require(output.data()[block * promotion_scratch_extent + index] ==
						   static_cast<char>('A' + block));
		}
	}
	matrix_require(defines == 8u);
}

template <result_family result>
void verify_line_feed_after_exact_threshold()
{
	using exact_source = sized_scratch_scatter_source<64u>;
	::std::size_t defines{};
	auto output{[](::std::size_t &count) {
		if constexpr (result == result_family::standard_string)
		{
			return ::fast_io::concatln_std(
				exact_source{'A', __builtin_addressof(count)},
				exact_source{'B', __builtin_addressof(count)},
				exact_source{'C', __builtin_addressof(count)},
				exact_source{'D', __builtin_addressof(count)},
				exact_source{'E', __builtin_addressof(count)},
				exact_source{'F', __builtin_addressof(count)},
				exact_source{'G', __builtin_addressof(count)},
				exact_source{'H', __builtin_addressof(count)});
		}
		else
		{
			return ::fast_io::concatln_fast_io(
				exact_source{'A', __builtin_addressof(count)},
				exact_source{'B', __builtin_addressof(count)},
				exact_source{'C', __builtin_addressof(count)},
				exact_source{'D', __builtin_addressof(count)},
				exact_source{'E', __builtin_addressof(count)},
				exact_source{'F', __builtin_addressof(count)},
				exact_source{'G', __builtin_addressof(count)},
				exact_source{'H', __builtin_addressof(count)});
		}
	}(defines)};

	// The eight descriptors exactly fill the 512-character policy threshold. The separately sequenced line feed remains
	// inside the larger physical safety window after the final pair, so its exact position proves that skipping a useless
	// final promotion cannot drop, duplicate, or reorder the record suffix. Source counts remain unchanged because
	// destination finalization never replays a producer.
	matrix_require(output.size() == 513u);
	for (::std::size_t block{}; block != 8u; ++block)
	{
		for (::std::size_t index{}; index != 64u; ++index)
		{
			matrix_require(output.data()[block * 64u + index] ==
						   static_cast<char>('A' + block));
		}
	}
	matrix_require(output.data()[512u] == '\n');
	matrix_require(defines == 8u);
}

template <result_family result>
void verify_oversized_leaf_promotes_in_place()
{
	::std::size_t defines{};
	auto output{[](::std::size_t &count) {
		if constexpr (result == result_family::standard_string)
		{
			return ::fast_io::concat_std(
				oversized_scratch_scatter_source{'A', __builtin_addressof(count)},
				scratch_scatter_source{'B', __builtin_addressof(count)},
				scratch_scatter_source{'C', __builtin_addressof(count)},
				scratch_scatter_source{'D', __builtin_addressof(count)},
				scratch_scatter_source{'E', __builtin_addressof(count)},
				scratch_scatter_source{'F', __builtin_addressof(count)},
				scratch_scatter_source{'G', __builtin_addressof(count)},
				scratch_scatter_source{'H', __builtin_addressof(count)});
		}
		else
		{
			return ::fast_io::concat_fast_io(
				oversized_scratch_scatter_source{'A', __builtin_addressof(count)},
				scratch_scatter_source{'B', __builtin_addressof(count)},
				scratch_scatter_source{'C', __builtin_addressof(count)},
				scratch_scatter_source{'D', __builtin_addressof(count)},
				scratch_scatter_source{'E', __builtin_addressof(count)},
				scratch_scatter_source{'F', __builtin_addressof(count)},
				scratch_scatter_source{'G', __builtin_addressof(count)},
				scratch_scatter_source{'H', __builtin_addressof(count)});
		}
	}(defines)};

	// The first actual descriptor exceeds the complete physical staging area, so overflow must promote while that leaf
	// is still active and then resume from reacquired cursors. Exact bytes and a single call per producer prove that the
	// suffix was neither replayed nor retained when the following shared-scratch descriptors overwrote their own byte.
	matrix_require(output.size() == oversized_scratch_extent + 7u);
	for (::std::size_t index{}; index != oversized_scratch_extent; ++index)
	{
		matrix_require(output.data()[index] == 'A');
	}
	for (::std::size_t index{}; index != 7u; ++index)
	{
		matrix_require(output.data()[oversized_scratch_extent + index] ==
					   static_cast<char>('B' + index));
	}
	matrix_require(defines == 8u);
}

template <result_family result, source_family family>
void run_singleton_protocol()
{
	run_case<result, family, 1u, false>();
	run_case<result, family, 1u, true>();
}

template <result_family result, ::std::size_t count>
void run_mixed_count()
{
	run_case<result, source_family::mixed, count, false>();
	run_case<result, source_family::mixed, count, true>();
}

template <result_family result, ::std::size_t count>
void run_mixed_borrowed_count()
{
	run_case<result, source_family::mixed_borrowed, count, false>();
	run_case<result, source_family::mixed_borrowed, count, true>();
}

template <result_family result, ::std::size_t count>
void run_dynamic_count()
{
	run_case<result, source_family::dynamic_reserve, count, false>();
	run_case<result, source_family::dynamic_reserve, count, true>();
}

template <result_family result>
void run_result_matrix()
{
	/* Singleton cases isolate source protocol selection.  Mixed cases prove
	   ordered construction and line publication at every selected pack size. */
	run_singleton_protocol<result, source_family::fixed_reserve>();
	run_singleton_protocol<result, source_family::dynamic_reserve>();
	run_singleton_protocol<result, source_family::precise_reserve>();
	run_singleton_protocol<result, source_family::scatter>();
	run_singleton_protocol<result, source_family::alias>();
	run_singleton_protocol<result, source_family::borrowed_scatter>();
	/* Homogeneous dynamic runs exercise compact cached bounds, same-type loop
	   quotienting, GCC's audited direct standard-string put area, and both sides
	   of the 64-leaf policy ceiling. Every object must still observe exactly one
	   size/define pair, including concatln's final cursor publication. */
	run_dynamic_count<result, 2u>();
	run_dynamic_count<result, 8u>();
	run_dynamic_count<result, 32u>();
	run_dynamic_count<result, 64u>();
	/* Long homogeneous exact records exercise the ordered pointer-table quotient on audited destinations. Every
	   original decay object must still receive exactly one coherent dynamic or precise query/writer pair. */
	run_case<result, source_family::precise_reserve, 8u, false>();
	run_case<result, source_family::precise_reserve, 8u, true>();
	run_case<result, source_family::precise_reserve, 32u, false>();
	run_case<result, source_family::precise_reserve, 32u, true>();
	/* N=8 exercises retained planning at the optimization threshold; N=32
	   verifies ordered ownership transfer across a larger descriptor run. */
	run_case<result, source_family::borrowed_scatter, 8u, false>();
	run_case<result, source_family::borrowed_scatter, 8u, true>();
	run_case<result, source_family::borrowed_scatter, 32u, false>();
	run_case<result, source_family::borrowed_scatter, 32u, true>();
	run_mixed_count<result, 1u>();
	run_mixed_count<result, 2u>();
	run_mixed_count<result, 8u>();
	run_mixed_count<result, 32u>();
	/* Replacing the unmarked scatter in the mixed rotation with an independently borrowed descriptor proves that the
	   complete-neutral destination policy neither repeats a CPO nor retains any source interval in the returned value. */
	run_mixed_borrowed_count<result, 8u>();
	run_mixed_borrowed_count<result, 32u>();
	verify_long_scratch_scatter_order<result>();
	verify_promoted_scratch_scatter_order<result>();
	verify_line_feed_after_exact_threshold<result>();
	verify_oversized_leaf_promotes_in_place<result>();
	verify_temporary_lifetimes<result>();
}

} // namespace

int main()
{
	run_result_matrix<result_family::standard_string>();
	run_result_matrix<result_family::fast_io_string>();
}
