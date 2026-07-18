#include <array>
#include <cassert>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

/// A protocol proxy rather than a descriptor object. Keeping the customization on this distinct type proves that
/// range admission is based on the exact `print_scatter_define` expression, not on an implementation-specific demand
/// that character forwarding itself return `basic_io_scatter_t`.
struct custom_scatter_proxy
{
	char const *base;
	::std::size_t size;
	::std::size_t *named_calls;
	::std::size_t *forwarded_calls;
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, custom_scatter_proxy>, custom_scatter_proxy &proxy) noexcept
{
	if (proxy.named_calls != nullptr)
	{
		++*proxy.named_calls;
	}
	return {proxy.base, proxy.size};
}

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, custom_scatter_proxy>, custom_scatter_proxy &&proxy) noexcept
{
	if (proxy.forwarded_calls != nullptr)
	{
		++*proxy.forwarded_calls;
	}
	return {proxy.base, proxy.size};
}

struct marked_proxy_source
{
	::std::string_view value;
	::std::size_t *named_calls{};
	::std::size_t *forwarded_calls{};
};

inline constexpr custom_scatter_proxy
print_alias_define(::fast_io::io_alias_t, marked_proxy_source const &source) noexcept
{
	return {source.value.data(), source.value.size(), source.named_calls, source.forwarded_calls};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, marked_proxy_source>) noexcept
{
	// The proxy points into the source's string-view payload. For an lvalue range element those characters remain live,
	// and neither aliasing nor observing the proxy mutates its address, length, or byte sequence.
	return {};
}

struct scatter_sink_state
{
	::std::string output;
	::std::array<char const *, 8u> descriptor_bases{};
	::std::size_t descriptor_count{};
	::std::size_t scatter_calls{};
	::std::size_t contiguous_calls{};
};

struct custom_scatter_sink
{
	using output_char_type = char;
	scatter_sink_state *state;
};

inline constexpr custom_scatter_sink output_stream_ref_define(custom_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::size_t
scatter_write_maximum_count(::fast_io::io_reserve_type_t<char, custom_scatter_sink>) noexcept
{
	return 8u;
}

inline void write_all_overflow_define(custom_scatter_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_calls;
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(
	custom_scatter_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count)
{
	++sink.state->scatter_calls;
	assert(sink.state->descriptor_count + count <= sink.state->descriptor_bases.size());
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->descriptor_bases[sink.state->descriptor_count++] = scatters[i].base;
		sink.state->output.append(scatters[i].base, scatters[i].len);
	}
}

/// This source has the same stable representation as the positive case but deliberately supplies no provenance opt-in.
/// Shape and actual lifetime in one test fixture are insufficient evidence for a generic retained-descriptor strategy.
struct unmarked_proxy_source
{
	::std::string_view value;
};

inline constexpr custom_scatter_proxy
print_alias_define(::fast_io::io_alias_t, unmarked_proxy_source const &source) noexcept
{
	return {source.value.data(), source.value.size(), nullptr, nullptr};
}

/// A one-shot custom proxy used to prove why an unmarked source must remain on the single-pass range representation.
struct consuming_proxy_source
{
	char value;
	::std::size_t observations{};
};

struct consuming_scatter_proxy
{
	consuming_proxy_source *source;
};

inline constexpr char consumed_text[]{'?', '?'};

inline constexpr consuming_scatter_proxy
print_alias_define(::fast_io::io_alias_t, consuming_proxy_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, consuming_scatter_proxy>, consuming_scatter_proxy proxy) noexcept
{
	if (proxy.source->observations++ == 0u)
	{
		return {__builtin_addressof(proxy.source->value), 1u};
	}
	return {consumed_text, 2u};
}

using marked_array = ::std::array<marked_proxy_source, 3u>;
using marked_iterator = ::std::ranges::iterator_t<marked_array &>;
using marked_sized_view = ::fast_io::sized_range_view_t<char, marked_iterator>;
using marked_forwarded_type = typename marked_sized_view::forwarded_value_type;
using marked_range = decltype(::fast_io::mnp::rgvw(
	::std::declval<marked_array &>(), ::std::declval<char const (&)[2u]>()));

static_assert(!::std::same_as<marked_forwarded_type, ::fast_io::basic_io_scatter_t<char>>);
static_assert(::fast_io::range_two_pass_scatter_printable_v<
	char, typename marked_sized_view::forwarded_expression_type>);
static_assert(::fast_io::sized_range_view_borrowed_scatter_source_v<char, marked_iterator>);
static_assert(::fast_io::sized_range_view_two_pass_scatter_element_v<char, marked_iterator>);
static_assert(::std::same_as<marked_range, marked_sized_view>);
static_assert(::fast_io::dynamic_reserve_scatters_printable<char, marked_range>);
static_assert(::fast_io::borrowed_reserve_scatters_source<char, marked_range>);
static_assert(::fast_io::put_area_printable_preferred<char, marked_range>);

using unmarked_array = ::std::array<unmarked_proxy_source, 2u>;
using unmarked_iterator = ::std::ranges::iterator_t<unmarked_array &>;
using unmarked_sized_view = ::fast_io::sized_range_view_t<char, unmarked_iterator>;
using unmarked_range = decltype(::fast_io::mnp::rgvw(
	::std::declval<unmarked_array &>(), ::std::declval<char const (&)[2u]>()));

static_assert(::fast_io::range_two_pass_scatter_printable_v<
	char, typename unmarked_sized_view::forwarded_expression_type>);
static_assert(!::fast_io::sized_range_view_borrowed_scatter_source_v<char, unmarked_iterator>);
static_assert(!::fast_io::sized_range_view_two_pass_scatter_element_v<char, unmarked_iterator>);
static_assert(::std::same_as<unmarked_range, ::fast_io::range_view_t<char, unmarked_iterator>>);
static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, unmarked_range>);

using consuming_array = ::std::array<consuming_proxy_source, 3u>;
using consuming_iterator = ::std::ranges::iterator_t<consuming_array &>;
using consuming_range = decltype(::fast_io::mnp::rgvw(
	::std::declval<consuming_array &>(), ::std::declval<char const (&)[2u]>()));

static_assert(::std::same_as<consuming_range, ::fast_io::range_view_t<char, consuming_iterator>>);
static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, consuming_range>);

inline void test_custom_proxy_descriptor_plan()
{
	using namespace ::std::literals;
	::std::size_t named_calls{};
	::std::size_t forwarded_calls{};
	marked_array sources{{
		{"alpha"sv, __builtin_addressof(named_calls), __builtin_addressof(forwarded_calls)},
		{"b"sv, __builtin_addressof(named_calls), __builtin_addressof(forwarded_calls)},
		{"charlie"sv, __builtin_addressof(named_calls), __builtin_addressof(forwarded_calls)}}};
	auto range{::fast_io::mnp::rgvw(sources, "|")};
	using range_type = decltype(range);

	auto const capacity{::fast_io::print_reserve_scatters_size(
		::fast_io::io_reserve_type<char, range_type>, range)};
	assert(capacity.scatters_size == 5u);
	assert(capacity.reserve_size == 0u);

	::std::array<::fast_io::basic_io_scatter_t<char>, 5u> scatters{};
	char reserve_sentinel{};
	auto const cursors{::fast_io::print_reserve_scatters_define(
		::fast_io::io_reserve_type<char, range_type>, scatters.data(),
		__builtin_addressof(reserve_sentinel), range)};
	assert(cursors.scatters_pos_ptr == scatters.data() + scatters.size());
	assert(cursors.reserve_pos_ptr == __builtin_addressof(reserve_sentinel));
	assert(forwarded_calls == sources.size());

	// Provenance is checked directly rather than inferred from equal output bytes: every retained element descriptor
	// must still name the caller-owned payload after the alias proxy used to produce it has been destroyed.
	for (::std::size_t i{}; i != sources.size(); ++i)
	{
		auto const &element{scatters[i * 2u]};
		assert(element.base == sources[i].value.data());
		assert(element.len == sources[i].value.size());
	}
	assert(scatters[1u].base[0] == '|');
	assert(scatters[3u].base[0] == '|');

	::std::string materialized;
	for (auto const scatter : scatters)
	{
		materialized.append(scatter.base, scatter.len);
	}
	assert(materialized == "alpha|b|charlie");

	auto const precise_size{::fast_io::print_reserve_precise_size(
		::fast_io::io_reserve_type<char, range_type>, range)};
	assert(precise_size == materialized.size());
	assert(named_calls == sources.size());
	assert(::fast_io::concat_std(range) == materialized);

	scatter_sink_state state;
	::fast_io::print(custom_scatter_sink{__builtin_addressof(state)}, range);
	assert(state.output == materialized);
	assert(state.scatter_calls == 1u);
	assert(state.contiguous_calls == 0u);
	assert(state.descriptor_count == scatters.size());
	assert(state.descriptor_bases[0u] == sources[0u].value.data());
	assert(state.descriptor_bases[2u] == sources[1u].value.data());
	assert(state.descriptor_bases[4u] == sources[2u].value.data());
}

inline void test_unmarked_and_consuming_sources_stay_single_pass()
{
	using namespace ::std::literals;
	unmarked_array stable{{{"left"sv}, {"right"sv}}};
	auto stable_range{::fast_io::mnp::rgvw(stable, "/")};
	assert(::fast_io::concat_std(stable_range) == "left/right");

	consuming_array consuming{{{'a', 0u}, {'b', 0u}, {'c', 0u}}};
	auto consuming_view{::fast_io::mnp::rgvw(consuming, ",")};
	assert(::fast_io::concat_std(consuming_view) == "a,b,c");
	for (auto const &source : consuming)
	{
		// A forbidden sizing pass would observe every producer twice and the emitted bytes would become "??".
		assert(source.observations == 1u);
	}
}

inline void test_marked_prvalue_elements_are_not_retained()
{
	using namespace ::std::literals;
	::std::array values{"x"sv, "yy"sv, "zzz"sv};
	auto prvalue_sources{values | ::std::views::transform([](::std::string_view value) {
		return marked_proxy_source{value};
	})};
	using iterator = ::std::ranges::iterator_t<decltype(prvalue_sources) &>;
	using hypothetical_sized_view = ::fast_io::sized_range_view_t<char, iterator>;
	static_assert(!::std::is_lvalue_reference_v<typename hypothetical_sized_view::source_reference>);
	static_assert(!::fast_io::sized_range_view_borrowed_scatter_source_v<char, iterator>);

	auto range{::fast_io::mnp::rgvw(prvalue_sources, ":")};
	static_assert(::std::same_as<decltype(range), ::fast_io::range_view_t<char, iterator>>);
	static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, decltype(range)>);
	assert(::fast_io::concat_std(range) == "x:yy:zzz");
}

} // namespace

int main()
{
	test_custom_proxy_descriptor_plan();
	test_unmarked_and_consuming_sources_stay_single_pass();
	test_marked_prvalue_elements_are_not_retained();
}
