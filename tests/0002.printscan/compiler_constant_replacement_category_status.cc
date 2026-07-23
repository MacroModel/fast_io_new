#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct source
{};

struct materialized_proxy
{};

struct source_leaf
{};

struct proxy_lvalue_leaf
{};

struct proxy_rvalue_leaf
{};

inline constexpr char source_spelling[]{'S'};
inline constexpr char lvalue_spelling[]{'L'};
inline constexpr char rvalue_spelling[]{'R'};

inline constexpr source_leaf print_alias_define(
	::fast_io::io_alias_t, source &) noexcept
{
	return {};
}

inline constexpr proxy_lvalue_leaf print_alias_define(
	::fast_io::io_alias_t, materialized_proxy &) noexcept
{
	return {};
}

inline constexpr proxy_rvalue_leaf print_alias_define(
	::fast_io::io_alias_t, materialized_proxy &&) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, source_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, source_leaf>, char *iter,
	source_leaf) noexcept
{
	*iter = source_spelling[0];
	return iter + 1;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, proxy_lvalue_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, proxy_lvalue_leaf>, char *iter,
	proxy_lvalue_leaf) noexcept
{
	*iter = lvalue_spelling[0];
	return iter + 1;
}

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, proxy_rvalue_leaf>,
	proxy_rvalue_leaf &) noexcept
{
	return {rvalue_spelling, 1u};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, materialized_proxy>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, materialized_proxy>, char *iter,
	materialized_proxy) noexcept
{
	*iter = rvalue_spelling[0];
	return iter + 1;
}

inline constexpr bool print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
{
	return true;
}

inline constexpr materialized_proxy print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_compiler_constant_materialization_query_inline_safe(
		::fast_io::io_reserve_type_t<char, source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_compiler_constant_pre_normalization_safe(
		::fast_io::io_reserve_type_t<char, source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_compiler_constant_materialization_graph_proven(
		::fast_io::io_reserve_type_t<char, source>) noexcept
{
	// Explicit provider classification lets this test isolate the later
	// replacement-category status proof and its ref-qualified aliases.
	return {};
}

struct capture_state
{
	::std::array<char, 16u> bytes{};
	char *current{bytes.data()};
	::std::size_t status_calls{};
	::std::size_t overflow_calls{};
};

struct status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct statusless_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr status_sink output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

inline constexpr statusless_sink
output_stream_ref_define(statusless_sink sink) noexcept
{
	return sink;
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline char *obuffer_begin(sink_type sink) noexcept
{
	return sink.state->bytes.data();
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline char *obuffer_curr(sink_type sink) noexcept
{
	return sink.state->current;
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline char *obuffer_end(sink_type sink) noexcept
{
	return sink.state->bytes.data() + sink.state->bytes.size();
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline void obuffer_set_curr(sink_type sink, char *current) noexcept
{
	sink.state->current = current;
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline void write_all_overflow_define(
	sink_type sink, char const *, char const *) noexcept
{
	++sink.state->overflow_calls;
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_compiler_constant_obuffer_materialization_safe(
		::fast_io::io_reserve_type_t<char, status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_compiler_constant_obuffer_materialization_safe(
		::fast_io::io_reserve_type_t<char, statusless_sink>) noexcept
{
	return {};
}

template <bool line>
inline void status_print_define(
	status_sink sink, proxy_rvalue_leaf &) noexcept
{
	++sink.state->status_calls;
}

using historical_source_type = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_normalized_t<
		char, false, source &>;
using old_named_proxy_model = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_normalized_t<
		char, false,
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_replacement_t<
				char, source &>>;
using true_arm_replacement_type = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_replacement_normalized_t<
		char, false, source &>;
inline constexpr bool source_candidate{
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, source &>};
using expected_named_proxy_model = ::std::conditional_t<
	source_candidate, proxy_lvalue_leaf, source_leaf>;
using expected_true_arm_type = ::std::conditional_t<
	source_candidate, proxy_rvalue_leaf, source_leaf>;

static_assert(::std::same_as<historical_source_type, source_leaf>);
static_assert(::std::same_as<old_named_proxy_model, expected_named_proxy_model>);
static_assert(::std::same_as<true_arm_replacement_type, expected_true_arm_type>);
static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, status_sink, true_arm_replacement_type> == source_candidate);

// Ref-qualified aliasing makes the materialized proxy's xvalue normalization
// observably different from a named proxy lvalue. The shortcut must prove the
// actual xvalue result pack and decline when only that pack owns status.
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			false, status_sink, source &>());
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			false, statusless_sink, source &>() == source_candidate);

} // namespace

int main()
{
	source value;

	capture_state public_state;
	status_sink public_sink{__builtin_addressof(public_state)};
	::fast_io::io::print(public_sink, value);
	::fast_io::io::println(public_sink, value);
	::fast_io::io::perr(public_sink, value);
	::fast_io::io::perrln(public_sink, value);
	assert(public_state.status_calls == 0u);
	assert(public_state.overflow_calls == 0u);
	assert((::std::string_view{
				public_state.bytes.data(),
				static_cast<::std::size_t>(
					public_state.current - public_state.bytes.data())} == "SS\nSS\n"));

	// The ordinary source-normalization bridge is the semantic baseline. It
	// selects source&, not the materialized proxy's ref-qualified alias CPO.
	capture_state baseline_state;
	status_sink baseline_sink{__builtin_addressof(baseline_state)};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, value);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, value);
	assert(baseline_state.status_calls == 0u);
	assert(baseline_state.overflow_calls == 0u);
	assert((::std::string_view{
				baseline_state.bytes.data(),
				static_cast<::std::size_t>(
					baseline_state.current - baseline_state.bytes.data())} == "SS\n"));

	// A destination with no replacement-only status owner emits the replacement
	// spelling only on a frontend whose source partition admits this custom
	// query. A fail-closed frontend retains the historical source spelling.
	capture_state statusless_state;
	::fast_io::io::print(
		statusless_sink{__builtin_addressof(statusless_state)}, value);
	assert(statusless_state.status_calls == 0u);
	assert(statusless_state.overflow_calls == 0u);
	assert((::std::string_view{
				statusless_state.bytes.data(),
				static_cast<::std::size_t>(
					statusless_state.current - statusless_state.bytes.data())} ==
		(source_candidate ? "R" : "S")));
}
