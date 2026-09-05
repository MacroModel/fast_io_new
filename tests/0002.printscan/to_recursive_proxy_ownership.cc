#define FAST_IO_DISABLE_FLOATING_POINT

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace to_recursive_proxy_ownership
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct protocol_counts
{
	::std::size_t aliases{};
	::std::size_t sizes{};
	::std::size_t defines{};
	::std::size_t scatters{};
};

struct fixed_source
{
	char value{};
	protocol_counts *counts{};
};

struct fixed_proxy
{
	char value{};
	protocol_counts *counts{};

	inline constexpr fixed_proxy(char character, protocol_counts *observations) noexcept
		: value(character), counts(observations)
	{}

	fixed_proxy(fixed_proxy const &) = delete;
	fixed_proxy &operator=(fixed_proxy const &) = delete;
	fixed_proxy(fixed_proxy &&) = default;
	fixed_proxy &operator=(fixed_proxy &&) = default;
};

inline fixed_proxy print_alias_define(::fast_io::io_alias_t, fixed_source &source) noexcept
{
	++source.counts->aliases;
	return fixed_proxy{source.value, source.counts};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_proxy>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_proxy>, char *destination,
	fixed_proxy &source) noexcept
{
	++source.counts->defines;
	*destination = source.value;
	return destination + 1u;
}

struct dynamic_source
{
	char value{};
	protocol_counts *counts{};
};

struct dynamic_proxy
{
	char value{};
	protocol_counts *counts{};

	inline constexpr dynamic_proxy(char character, protocol_counts *observations) noexcept
		: value(character), counts(observations)
	{}

	dynamic_proxy(dynamic_proxy const &) = delete;
	dynamic_proxy &operator=(dynamic_proxy const &) = delete;
	dynamic_proxy(dynamic_proxy &&) = default;
	dynamic_proxy &operator=(dynamic_proxy &&) = default;
};

inline dynamic_proxy print_alias_define(::fast_io::io_alias_t, dynamic_source &source) noexcept
{
	++source.counts->aliases;
	return dynamic_proxy{source.value, source.counts};
}

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_proxy>, dynamic_proxy &source) noexcept
{
	++source.counts->sizes;
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_proxy>, char *destination,
	dynamic_proxy &source) noexcept
{
	++source.counts->defines;
	*destination = source.value;
	return destination + 1u;
}

struct reusable_protocol_observations
{
	::std::size_t aliases{};
	::std::size_t sizes{};
	::std::size_t defines{};
	::std::size_t size_order[8u]{};
	::std::size_t define_order[8u]{};
	char *destinations[8u]{};
};

struct reusable_source
{
	::std::size_t extent{};
	char fill{};
	::std::size_t ordinal{};
	reusable_protocol_observations *observations{};
};

struct reusable_proxy
{
	::std::size_t extent{};
	char fill{};
	::std::size_t ordinal{};
	reusable_protocol_observations *observations{};

	inline constexpr reusable_proxy(
		::std::size_t size, char value, ::std::size_t index,
		reusable_protocol_observations *counts) noexcept
		: extent(size), fill(value), ordinal(index), observations(counts)
	{}

	reusable_proxy(reusable_proxy const &) = delete;
	reusable_proxy &operator=(reusable_proxy const &) = delete;
	reusable_proxy(reusable_proxy &&) = default;
	reusable_proxy &operator=(reusable_proxy &&) = default;
};

inline reusable_proxy print_alias_define(::fast_io::io_alias_t, reusable_source &source) noexcept
{
	++source.observations->aliases;
	return {source.extent, source.fill, source.ordinal, source.observations};
}

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, reusable_proxy>, reusable_proxy &source) noexcept
{
	auto &observations{*source.observations};
	observations.size_order[observations.sizes++] = source.ordinal;
	return source.extent;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, reusable_proxy>, char *destination,
	reusable_proxy &source) noexcept
{
	auto &observations{*source.observations};
	observations.define_order[observations.defines] = source.ordinal;
	observations.destinations[observations.defines++] = destination;
	if (source.extent == 0u)
	{
		return destination;
	}
	*destination = '[';
	if (source.extent != 1u)
	{
		for (::std::size_t position{1u}; position + 1u < source.extent; ++position)
		{
			destination[position] = source.fill;
		}
		destination[source.extent - 1u] = ']';
	}
	return destination + source.extent;
}

struct oversized_fixed_source
{
	char fill{};
	::std::size_t ordinal{};
	reusable_protocol_observations *observations{};
};

struct oversized_fixed_proxy
{
	char fill{};
	::std::size_t ordinal{};
	reusable_protocol_observations *observations{};

	inline constexpr oversized_fixed_proxy(
		char value, ::std::size_t index, reusable_protocol_observations *counts) noexcept
		: fill(value), ordinal(index), observations(counts)
	{}

	oversized_fixed_proxy(oversized_fixed_proxy const &) = delete;
	oversized_fixed_proxy &operator=(oversized_fixed_proxy const &) = delete;
	oversized_fixed_proxy(oversized_fixed_proxy &&) = default;
	oversized_fixed_proxy &operator=(oversized_fixed_proxy &&) = default;
};

inline oversized_fixed_proxy print_alias_define(
	::fast_io::io_alias_t, oversized_fixed_source &source) noexcept
{
	++source.observations->aliases;
	return {source.fill, source.ordinal, source.observations};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, oversized_fixed_proxy>) noexcept
{
	return 300u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, oversized_fixed_proxy>, char *destination,
	oversized_fixed_proxy &source) noexcept
{
	auto &observations{*source.observations};
	observations.define_order[observations.defines] = source.ordinal;
	observations.destinations[observations.defines++] = destination;
	*destination = '[';
	for (::std::size_t position{1u}; position != 299u; ++position)
	{
		destination[position] = source.fill;
	}
	destination[299u] = ']';
	return destination + 300u;
}

struct eager_source
{
	char value{};
};

struct eager_proxy
{
	char value{};

	inline explicit constexpr eager_proxy(char character) noexcept : value(character)
	{}
	eager_proxy(eager_proxy const &) = delete;
	eager_proxy &operator=(eager_proxy const &) = delete;
	eager_proxy(eager_proxy &&) = default;
	eager_proxy &operator=(eager_proxy &&) = default;
};

inline constexpr eager_proxy print_alias_define(::fast_io::io_alias_t, eager_source &source) noexcept
{
	return eager_proxy{source.value};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, eager_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, eager_proxy>, char *destination,
	eager_proxy &source) noexcept
{
	*destination = source.value;
	return destination + 1u;
}

inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, eager_proxy>) noexcept
{
	// Formatting reads one immutable code unit, cannot fail, and has no state or externally visible side effect.
	return {};
}

struct scatter_source
{
	char value{};
	protocol_counts *counts{};
};

struct scatter_proxy
{
	char value{};
	protocol_counts *counts{};

	inline constexpr scatter_proxy(char character, protocol_counts *observations) noexcept
		: value(character), counts(observations)
	{}

	scatter_proxy(scatter_proxy const &) = delete;
	scatter_proxy &operator=(scatter_proxy const &) = delete;
	scatter_proxy(scatter_proxy &&) = default;
	scatter_proxy &operator=(scatter_proxy &&) = default;
};

inline scatter_proxy print_alias_define(::fast_io::io_alias_t, scatter_source &source) noexcept
{
	++source.counts->aliases;
	return scatter_proxy{source.value, source.counts};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scatter_proxy>, scatter_proxy &source) noexcept
{
	++source.counts->scatters;
	return {__builtin_addressof(source.value), 1u};
}

struct direct_source
{
	char value{};
	protocol_counts *counts{};
};

struct direct_proxy
{
	char value{};
	protocol_counts *counts{};

	inline constexpr direct_proxy(char character, protocol_counts *observations) noexcept
		: value(character), counts(observations)
	{}

	direct_proxy(direct_proxy const &) = delete;
	direct_proxy &operator=(direct_proxy const &) = delete;
	direct_proxy(direct_proxy &&) = default;
	direct_proxy &operator=(direct_proxy &&) = default;
};

inline direct_proxy print_alias_define(::fast_io::io_alias_t, direct_source &source) noexcept
{
	++source.counts->aliases;
	return direct_proxy{source.value, source.counts};
}

template <typename output>
inline void print_define(
	::fast_io::io_reserve_type_t<char, direct_proxy>, output &&destination,
	direct_proxy &source)
{
	++source.counts->defines;
	::fast_io::operations::write_all(
		::std::forward<output>(destination), __builtin_addressof(source.value),
		__builtin_addressof(source.value) + 1u);
}

struct dual_source
{
	char reserve_value{};
	char scatter_value[4u]{};
	protocol_counts *counts{};
};

struct dual_proxy
{
	dual_source *source{};

	inline explicit constexpr dual_proxy(dual_source *value) noexcept : source(value)
	{}
	dual_proxy(dual_proxy const &) = delete;
	dual_proxy &operator=(dual_proxy const &) = delete;
	dual_proxy(dual_proxy &&) = default;
	dual_proxy &operator=(dual_proxy &&) = default;
};

inline dual_proxy print_alias_define(::fast_io::io_alias_t, dual_source &source) noexcept
{
	++source.counts->aliases;
	return dual_proxy{__builtin_addressof(source)};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dual_proxy>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dual_proxy>, char *destination,
	dual_proxy &proxy) noexcept
{
	++proxy.source->counts->defines;
	*destination = proxy.source->reserve_value;
	return destination + 1u;
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, dual_proxy>, dual_proxy &proxy) noexcept
{
	++proxy.source->counts->scatters;
	return {proxy.source->scatter_value, 4u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, dual_proxy>) noexcept
{
	// The descriptor points into the unchanged public source, whose lifetime contains the complete synchronous call.
	// Re-observation therefore returns the same address, length, and bytes; only protocol-selection consistency is tested.
	return {};
}

struct contiguous_target
{
	::std::uint_least64_t value{};
};

struct contiguous_proxy
{
	contiguous_target *target{};
};

inline constexpr ::fast_io::parse_result<char const *> parse_decimal(
	char const *first, char const *last, ::std::uint_least64_t &destination) noexcept
{
	::std::uint_least64_t value{};
	for (auto current{first}; current != last; ++current)
	{
		if (*current < '0' || '9' < *current)
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		value = value * UINT64_C(10) + static_cast<unsigned char>(*current - '0');
	}
	destination = value;
	return {last, ::fast_io::parse_code::ok};
}

inline constexpr contiguous_proxy scan_alias_define(
	::fast_io::io_alias_t, contiguous_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, contiguous_proxy>, char const *first,
	char const *last, contiguous_proxy &proxy) noexcept
{
	return parse_decimal(first, last, proxy.target->value);
}

using inplace_decay_value_entry = void (*)(contiguous_proxy, fixed_proxy);
using inplace_decay_borrowed_target_entry = void (*)(contiguous_proxy &, fixed_proxy);

// `decay` is the owning ABI boundary, so an ordinary normalized target must remain a value parameter. The separately
// named borrowed entry exists only for a customization-authored stable lvalue; making the common entry `T&&` would
// silently replace an AAPCS/SysV register-class proxy with a pointer whenever the wrapper is not inlined.
static_assert(::std::same_as<
	decltype(&::fast_io::basic_inplace_to_decay<
		char, contiguous_proxy, fixed_proxy>),
	inplace_decay_value_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::basic_inplace_to_decay_borrowed_target<
		char, contiguous_proxy, fixed_proxy>),
	inplace_decay_borrowed_target_entry>);

struct move_scan_target
{
	::std::uint_least64_t value{};
};

struct move_scan_proxy
{
	move_scan_target *target{};

	inline explicit constexpr move_scan_proxy(move_scan_target *value) noexcept : target(value)
	{}
	move_scan_proxy(move_scan_proxy const &) = delete;
	move_scan_proxy &operator=(move_scan_proxy const &) = delete;
	move_scan_proxy(move_scan_proxy &&) = default;
	move_scan_proxy &operator=(move_scan_proxy &&) = default;
};

inline constexpr move_scan_proxy scan_alias_define(
	::fast_io::io_alias_t, move_scan_target &target) noexcept
{
	return move_scan_proxy{__builtin_addressof(target)};
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, move_scan_proxy>, char const *first,
	char const *last, move_scan_proxy &proxy) noexcept
{
	return parse_decimal(first, last, proxy.target->value);
}

struct terminal_target
{
	::std::uint_least64_t value{};
	::std::size_t contiguous_calls{};
	::std::size_t context_calls{};
	::std::size_t eof_calls{};
};

struct terminal_proxy
{
	terminal_target *target{};
};

struct terminal_state
{
	::std::uint_least64_t value{};
};

inline constexpr terminal_proxy scan_alias_define(
	::fast_io::io_alias_t, terminal_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, terminal_proxy>, char const *first,
	char const *last, terminal_proxy &proxy) noexcept
{
	++proxy.target->contiguous_calls;
	return parse_decimal(first, last, proxy.target->value);
}

inline constexpr ::fast_io::io_type_t<terminal_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, terminal_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, terminal_proxy>, terminal_state &state,
	char const *first, char const *last, terminal_proxy &proxy) noexcept
{
	++proxy.target->context_calls;
	for (auto current{first}; current != last; ++current)
	{
		if (*current < '0' || '9' < *current)
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		state.value = state.value * UINT64_C(10) + static_cast<unsigned char>(*current - '0');
	}
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, terminal_proxy>, terminal_state &state,
	terminal_proxy &proxy) noexcept
{
	++proxy.target->eof_calls;
	proxy.target->value = state.value;
	return ::fast_io::parse_code::ok;
}

inline constexpr ::std::true_type scan_context_terminal_contiguous_equivalent(
	::fast_io::io_reserve_type_t<char, terminal_proxy>) noexcept
{
	// Both scanners accept exactly the same nonempty decimal language and publish the same integer value.
	return {};
}

inline constexpr ::std::true_type to_terminal_contiguous_staging_preferred(
	::fast_io::io_reserve_type_t<char, terminal_proxy>) noexcept
{
	// The target explicitly chooses one bounded contiguous transition over four incremental context transitions.
	return {};
}

struct reusable_target
{
	bool stop_after_first{};
	::std::size_t bytes{};
	::std::size_t context_calls{};
	::std::size_t eof_calls{};
	::std::size_t canary_failures{};
	::std::uint_least64_t digest{};
};

struct reusable_target_proxy
{
	reusable_target *target{};
};

struct reusable_target_state
{
	::std::size_t bytes{};
	::std::uint_least64_t digest{UINT64_C(1469598103934665603)};
};

inline constexpr reusable_target_proxy scan_alias_define(
	::fast_io::io_alias_t, reusable_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::io_type_t<reusable_target_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, reusable_target_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, reusable_target_proxy>, reusable_target_state &state,
	char const *first, char const *last, reusable_target_proxy &proxy) noexcept
{
	auto &target{*proxy.target};
	::std::size_t const fragment_index{target.context_calls++};
	::std::size_t const fragment_size{static_cast<::std::size_t>(last - first)};
	char const expected_fill{static_cast<char>('a' + fragment_index)};
	bool valid{fragment_size >= 2u && *first == '[' && last[-1] == ']'};
	for (::std::size_t position{1u}; valid && position < fragment_size - 1u; ++position)
	{
		valid = first[position] == expected_fill;
	}
	if (!valid)
	{
		++target.canary_failures;
	}
	state.bytes += fragment_size;
	for (auto current{first}; current != last; ++current)
	{
		state.digest = (state.digest ^ static_cast<unsigned char>(*current)) * UINT64_C(1099511628211);
	}
	if (target.stop_after_first && target.context_calls == 1u)
	{
		target.bytes = state.bytes;
		target.digest = state.digest;
		return {last, ::fast_io::parse_code::ok};
	}
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, reusable_target_proxy>, reusable_target_state &state,
	reusable_target_proxy &proxy) noexcept
{
	++proxy.target->eof_calls;
	proxy.target->bytes = state.bytes;
	proxy.target->digest = state.digest;
	return ::fast_io::parse_code::ok;
}

template <typename source_type>
inline constexpr source_type make_source(char value, protocol_counts &counts) noexcept
{
	return {value, __builtin_addressof(counts)};
}

template <typename target_type, typename source_type>
inline void convert_four(target_type &target, protocol_counts &counts)
{
	auto one{make_source<source_type>('1', counts)};
	auto two{make_source<source_type>('2', counts)};
	auto three{make_source<source_type>('3', counts)};
	auto four{make_source<source_type>('4', counts)};
	::fast_io::inplace_to(target, one, two, three, four);
}

template <typename source_type>
[[nodiscard]] inline ::std::uint_least64_t convert_four_value_return(protocol_counts &counts)
{
	auto one{make_source<source_type>('1', counts)};
	auto two{make_source<source_type>('2', counts)};
	auto three{make_source<source_type>('3', counts)};
	auto four{make_source<source_type>('4', counts)};
	return ::fast_io::to<::std::uint_least64_t>(one, two, three, four);
}

inline void check_context_static_reserve()
{
	protocol_counts counts{};
	::std::uint_least64_t value{};
	convert_four<::std::uint_least64_t, fixed_source>(value, counts);
	require(value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.sizes == 0u && counts.defines == 4u);
}

inline void check_context_dynamic_reserve()
{
	protocol_counts counts{};
	::std::uint_least64_t value{};
	convert_four<::std::uint_least64_t, dynamic_source>(value, counts);
	require(value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.sizes == 4u && counts.defines == 4u);
}

inline void check_reusable_heap_owner_canonical_fast_path()
{
	::fast_io::details::local_operator_new_array_ptr<char> owner;
	// Default construction establishes the canonical empty state. Since the ensure operation normalizes every request
	// to at least one character, its capacity-only hit predicate cannot accept this null owner.
	require(owner.ptr == nullptr && owner.size == 0u);
	auto *const first{::fast_io::details::inplace_to_reusable_heap_buffer_ensure<256u>(owner, 0u)};
	require(first != nullptr && first == owner.ptr && owner.size >= 1u);
	auto const first_capacity{owner.size};
	// A covered request must retain both ownership fields exactly; in particular it may neither allocate nor exchange
	// the live block after the size invariant has proved the pointer non-null.
	auto *const reused{::fast_io::details::inplace_to_reusable_heap_buffer_ensure<256u>(owner, 1u)};
	require(reused == first && owner.ptr == first && owner.size == first_capacity);
}

inline void check_lazy_reusable_heap_growth()
{
	reusable_protocol_observations observations{};
	reusable_source one{8u, 'a', 0u, __builtin_addressof(observations)};
	reusable_source two{8u, 'b', 1u, __builtin_addressof(observations)};
	reusable_source three{300u, 'c', 2u, __builtin_addressof(observations)};
	reusable_source four{16u, 'd', 3u, __builtin_addressof(observations)};
	auto const target{::fast_io::to<reusable_target>(one, two, three, four)};

	require(target.bytes == 332u && target.context_calls == 4u && target.eof_calls == 1u);
	require(target.canary_failures == 0u && target.digest != 0u);
	require(observations.aliases == 4u && observations.sizes == 4u && observations.defines == 4u);
	for (::std::size_t index{}; index != 4u; ++index)
	{
		require(observations.size_order[index] == index);
		require(observations.define_order[index] == index);
	}
	// The two small fragments must share the initial heap block. The 300-byte fragment forces a replacement which is
	// acquired while the old owner is still live, so its address differs; the final smaller fragment reuses that growth.
	require(observations.destinations[0u] == observations.destinations[1u]);
	require(observations.destinations[1u] != observations.destinations[2u]);
	require(observations.destinations[2u] == observations.destinations[3u]);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline void check_mixed_pack_without_usable_stack_scratch()
{
	reusable_protocol_observations observations{};
	oversized_fixed_source one{'a', 0u, __builtin_addressof(observations)};
	reusable_source two{8u, 'b', 1u, __builtin_addressof(observations)};
	reusable_target target{};
	::fast_io::inplace_to(target, one, two);

	require(target.bytes == 308u && target.context_calls == 2u && target.eof_calls == 1u);
	require(target.canary_failures == 0u);
	require(observations.aliases == 2u && observations.sizes == 1u && observations.defines == 2u);
	require(observations.size_order[0u] == 1u);
	require(observations.define_order[0u] == 0u && observations.define_order[1u] == 1u);
	// The oversized static fragment necessarily creates heap staging. The unhinted dynamic suffix must reuse it; neither
	// protocol can justify materializing the operation-cap array in this mixed conversion's automatic storage.
	require(observations.destinations[0u] == observations.destinations[1u]);
}

inline void check_lazy_reusable_heap_early_stop()
{
	reusable_protocol_observations observations{};
	reusable_source one{8u, 'a', 0u, __builtin_addressof(observations)};
	reusable_source two{8u, 'b', 1u, __builtin_addressof(observations)};
	reusable_source three{300u, 'c', 2u, __builtin_addressof(observations)};
	reusable_source four{16u, 'd', 3u, __builtin_addressof(observations)};
	reusable_target target{};
	target.stop_after_first = true;
	::fast_io::inplace_to(target, one, two, three, four);

	require(target.bytes == 8u && target.context_calls == 1u && target.eof_calls == 0u);
	require(target.canary_failures == 0u && target.digest != 0u);
	// Public normalization owns all four move-only aliases before execution. Once the scanner reports `ok`, however,
	// neither the size CPO nor the writer CPO of any suffix may be observed.
	require(observations.aliases == 4u && observations.sizes == 1u && observations.defines == 1u);
	require(observations.size_order[0u] == 0u && observations.define_order[0u] == 0u);
	for (::std::size_t index{1u}; index != 4u; ++index)
	{
		require(observations.destinations[index] == nullptr);
	}
}

inline void check_context_scatter()
{
	protocol_counts counts{};
	::std::uint_least64_t value{};
	convert_four<::std::uint_least64_t, scatter_source>(value, counts);
	require(value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.scatters == 4u);
}

inline void check_contiguous_static_reserve()
{
	protocol_counts counts{};
	contiguous_target target{};
	convert_four<contiguous_target, fixed_source>(target, counts);
	require(target.value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.sizes == 0u && counts.defines == 4u);
}

inline void check_contiguous_dynamic_reserve()
{
	protocol_counts counts{};
	contiguous_target target{};
	convert_four<contiguous_target, dynamic_source>(target, counts);
	require(target.value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.sizes == 4u && counts.defines == 4u);
}

inline void check_context_direct_print()
{
	protocol_counts counts{};
	::std::uint_least64_t value{};
	convert_four<::std::uint_least64_t, direct_source>(value, counts);
	require(value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.defines == 4u);
}

inline void check_contiguous_direct_print()
{
	protocol_counts counts{};
	contiguous_target target{};
	convert_four<contiguous_target, direct_source>(target, counts);
	require(target.value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.defines == 4u);
}

inline void check_single_scatter()
{
	protocol_counts counts{};
	auto source{make_source<scatter_source>('7', counts)};
	::std::uint_least64_t value{};
	::fast_io::inplace_to(value, source);
	require(value == UINT64_C(7));
	require(counts.aliases == 1u && counts.scatters == 1u);
}

inline void check_value_return_static_reserve()
{
	protocol_counts counts{};
	auto const value{convert_four_value_return<fixed_source>(counts)};
	require(value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.defines == 4u);
}

inline void check_value_return_other_protocols()
{
	protocol_counts dynamic_counts{};
	auto const dynamic_value{convert_four_value_return<dynamic_source>(dynamic_counts)};
	require(dynamic_value == UINT64_C(1234));
	require(dynamic_counts.aliases == 4u && dynamic_counts.sizes == 4u &&
			dynamic_counts.defines == 4u);

	protocol_counts scatter_counts{};
	auto const scatter_value{convert_four_value_return<scatter_source>(scatter_counts)};
	require(scatter_value == UINT64_C(1234));
	require(scatter_counts.aliases == 4u && scatter_counts.scatters == 4u);

	protocol_counts direct_counts{};
	auto const direct_value{convert_four_value_return<direct_source>(direct_counts)};
	require(direct_value == UINT64_C(1234));
	require(direct_counts.aliases == 4u && direct_counts.defines == 4u);
}

inline void check_static_reserve_scatter_coupling()
{
	protocol_counts counts{};
	dual_source one{'1', {'9', '9', '9', '9'}, __builtin_addressof(counts)};
	dual_source two{'2', {'8', '8', '8', '8'}, __builtin_addressof(counts)};
	contiguous_target target{};
	::fast_io::inplace_to(target, one, two);
	// The all-static plan proves a two-byte capacity. Its emitter must consequently select the same reserve spelling;
	// observing either four-byte scatter would invalidate that capacity proof before the scanner is reached.
	require(target.value == UINT64_C(12));
	require(counts.aliases == 2u && counts.defines == 2u && counts.scatters == 0u);
}

inline void check_move_only_scan_proxy()
{
	protocol_counts counts{};
	move_scan_target target{};
	convert_four<move_scan_target, fixed_source>(target, counts);
	require(target.value == UINT64_C(1234));
	require(counts.aliases == 4u && counts.defines == 4u);
}

inline void check_terminal_stack_move_only_sources()
{
	eager_source one{'1'};
	eager_source two{'2'};
	eager_source three{'3'};
	eager_source four{'4'};
	terminal_target target{};
	::fast_io::inplace_to(target, one, two, three, four);
	require(target.value == UINT64_C(1234));
	// The target and every source explicitly discharge the eager-equivalence proof. One contiguous transition must
	// replace the incremental state machine; any context or EOF call shows that the terminal plan was not selected.
	require(target.contiguous_calls == 1u && target.context_calls == 0u && target.eof_calls == 0u);
}

struct counted_source
{
	char value{};
	::std::size_t *copies{};

	inline counted_source(char character, ::std::size_t &counter) noexcept
		: value(character), copies(__builtin_addressof(counter))
	{}

	inline counted_source(counted_source const &other) noexcept
		: value(other.value), copies(other.copies)
	{
		++*copies;
	}
};

inline counted_source print_alias_define(::fast_io::io_alias_t, counted_source &source) noexcept
{
	return source;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, counted_source>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, counted_source>, char *destination,
	counted_source const &source) noexcept
{
	*destination = source.value;
	return destination + 1u;
}

inline void check_copy_complexity()
{
	::std::size_t copies{};
	counted_source one{'1', copies};
	counted_source two{'2', copies};
	counted_source three{'3', copies};
	counted_source four{'4', copies};
	counted_source five{'5', copies};
	counted_source six{'6', copies};
	counted_source seven{'7', copies};
	counted_source eight{'8', copies};
	::std::uint_least64_t value{};
	::fast_io::inplace_to(value, one, two, three, four, five, six, seven, eight);
	require(value == UINT64_C(12345678));
	// The two public normalization boundaries may each copy every source once. Recursive suffix walking may add no
	// copies: this linear allowance rejects the former 1+...+N triangular copy term without relying on optimizer elision.
	require(copies <= 16u);
}

static_assert(::fast_io::reserve_printable<char, fixed_proxy>);
static_assert(::fast_io::dynamic_reserve_printable<char, dynamic_proxy>);
static_assert(::fast_io::dynamic_reserve_printable<char, reusable_proxy>);
static_assert(::fast_io::reserve_printable<char, oversized_fixed_proxy>);
// Neither member of this mixed pack can write into the 256-byte operation scratch: the static bound exceeds it and the
// dynamic source publishes no stack hint or bounded-size protocol. The storage-existence fold must therefore retain only
// its one-element non-writable placeholder instead of enlarging every caller's frame.
static_assert(!::fast_io::details::to_bounded_fragment_stack_scratch_candidate_v<
	char, oversized_fixed_proxy>);
static_assert(!::fast_io::details::to_bounded_fragment_stack_scratch_candidate_v<
	char, dynamic_proxy>);
static_assert(::fast_io::scatter_printable_for<char, scatter_proxy &>);
static_assert(::fast_io::printable<char, direct_proxy>);
static_assert(::fast_io::reserve_printable<char, dual_proxy>);
static_assert(::fast_io::details::to_repeatable_named_scatter_v<char, dual_proxy>);
static_assert(::fast_io::contiguous_scannable<char, contiguous_proxy>);
static_assert(::fast_io::contiguous_scannable<char, move_scan_proxy>);
static_assert(::fast_io::context_scannable<char, terminal_proxy>);
static_assert(::fast_io::context_scannable<char, reusable_target_proxy>);
static_assert(::fast_io::terminal_contiguous_context_scannable<char, terminal_proxy>);
static_assert(::fast_io::eager_materialization_safe_printable<char, eager_proxy>);
static_assert(!::std::is_copy_constructible_v<reusable_proxy>);

} // namespace to_recursive_proxy_ownership

int main()
{
	::to_recursive_proxy_ownership::check_context_static_reserve();
	::to_recursive_proxy_ownership::check_context_dynamic_reserve();
	::to_recursive_proxy_ownership::check_reusable_heap_owner_canonical_fast_path();
	::to_recursive_proxy_ownership::check_lazy_reusable_heap_growth();
	::to_recursive_proxy_ownership::check_mixed_pack_without_usable_stack_scratch();
	::to_recursive_proxy_ownership::check_lazy_reusable_heap_early_stop();
	::to_recursive_proxy_ownership::check_context_scatter();
	::to_recursive_proxy_ownership::check_contiguous_static_reserve();
	::to_recursive_proxy_ownership::check_contiguous_dynamic_reserve();
	::to_recursive_proxy_ownership::check_context_direct_print();
	::to_recursive_proxy_ownership::check_contiguous_direct_print();
	::to_recursive_proxy_ownership::check_single_scatter();
	::to_recursive_proxy_ownership::check_value_return_static_reserve();
	::to_recursive_proxy_ownership::check_value_return_other_protocols();
	::to_recursive_proxy_ownership::check_static_reserve_scatter_coupling();
	::to_recursive_proxy_ownership::check_move_only_scan_proxy();
	::to_recursive_proxy_ownership::check_terminal_stack_move_only_sources();
	::to_recursive_proxy_ownership::check_copy_complexity();
}
