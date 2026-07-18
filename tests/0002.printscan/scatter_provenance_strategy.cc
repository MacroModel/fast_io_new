#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <fast_io.h>

namespace
{

// The proxy is intentionally scatter-printable but not a borrowed_scatter_source. Every formatter call reuses one
// character of shared scratch, so retaining two returned descriptors before the final write would turn "AB" into
// "BB". This is a valid immediate-consumption protocol and models timestamp/thread-id adapters that format through a
// reusable cache; the strategy layer, not the shape-only scatter concept, must decide whether retention is permitted.
struct scratch_scatter_proxy
{
	char value{};
};

struct scratch_source
{
	char value{};
};

inline char shared_scratch{};
inline ::std::size_t shared_scratch_calls{};

inline constexpr scratch_scatter_proxy
print_alias_define(::fast_io::io_alias_t, scratch_source source) noexcept
{
	return {source.value};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scratch_scatter_proxy>, scratch_scatter_proxy proxy) noexcept
{
	++shared_scratch_calls;
	shared_scratch = proxy.value;
	return {__builtin_addressof(shared_scratch), 1u};
}

static_assert(::fast_io::alias_printable<scratch_source>);
static_assert(::fast_io::scatter_printable<char, scratch_scatter_proxy>);
static_assert(!::fast_io::borrowed_scatter_source<char, scratch_scatter_proxy>);
// Shape alone is not a replay proof. Both semantic allocation strategies would observe this CPO once for sizing and
// again for emission, so an unmarked scratch producer must remain outside both admission predicates.
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
	char, scratch_scatter_proxy &>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
	char, scratch_scatter_proxy &>::value);
// A raw scatter view owns the library's built-in stable-observation marker and remains eligible.
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
	char, ::fast_io::basic_io_scatter_t<char> &>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
	char, ::fast_io::basic_io_scatter_t<char> &>::value);

struct capture_state
{
	::std::string output;
	::std::size_t scalar_calls{};
	::std::size_t scatter_calls{};
	::std::size_t maximum_scatter_count{};
};

struct capture_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(capture_sink sink, char const *first, char const *last)
{
	++sink.state->scalar_calls;
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(
	capture_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	++sink.state->scatter_calls;
	if (sink.state->maximum_scatter_count < count)
	{
		sink.state->maximum_scatter_count = count;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->output.append(scatters[i].base, scatters[i].len);
	}
}

template <typename... Args>
inline capture_state render(Args &&...args)
{
	capture_state state;
	::fast_io::print(capture_sink{__builtin_addressof(state)}, ::std::forward<Args>(args)...);
	return state;
}

inline void test_immediate_scatter_and_semantic_composition()
{
	scratch_source const a{'A'};
	scratch_source const b{'B'};

	auto direct{render(a, b)};
	assert(direct.output == "AB");
	// The unmarked proxies cannot share one retained descriptor run.  A leaf may lower a one-entry scatter to scalar
	// write, so call count is deliberately not prescribed; the observable proof is that no scatter call receives two
	// descriptors whose shared storage could be overwritten before consumption.
	assert(direct.maximum_scatter_count <= 1u);

	auto packed{::fast_io::mnp::pack(a, b)};
	shared_scratch_calls = 0u;
	assert(render(packed).output == "AB");
	assert(shared_scratch_calls == 2u);
	shared_scratch_calls = 0u;
	assert(::fast_io::concat_std(a, b) == "AB");
	assert(shared_scratch_calls == 2u);
	shared_scratch_calls = 0u;
	assert(::fast_io::concat_std(packed) == "AB");
	assert(shared_scratch_calls == 2u);

	auto selected{::fast_io::mnp::pack(
		::fast_io::mnp::cond(true, a, b), ::fast_io::mnp::cond(false, a, b))};
	shared_scratch_calls = 0u;
	assert(render(selected).output == "AB");
	assert(shared_scratch_calls == 2u);
	shared_scratch_calls = 0u;
	assert(::fast_io::concat_std(selected) == "AB");
	assert(shared_scratch_calls == 2u);

	auto padded{::fast_io::mnp::left(a, 3u, '_')};
	shared_scratch_calls = 0u;
	assert(render(padded).output == "A__");
	assert(shared_scratch_calls == 1u);
	shared_scratch_calls = 0u;
	assert(::fast_io::concat_std(padded) == "A__");
	assert(shared_scratch_calls == 1u);
}

inline void test_range_materialization_does_not_invent_borrowing()
{
	::std::array<scratch_source, 3u> sources{{{'A'}, {'B'}, {'C'}}};
	auto range{::fast_io::mnp::rgvw(sources, ",")};
	assert(render(range).output == "A,B,C");
	assert(::fast_io::concat_std(range) == "A,B,C");
}

} // namespace

int main()
{
	test_immediate_scatter_and_semantic_composition();
	test_range_materialization_does_not_invent_borrowing();
}
