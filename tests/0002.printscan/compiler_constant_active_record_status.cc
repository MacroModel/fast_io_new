#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

namespace preserved_fragment_case
{

struct source
{};

struct materialized_proxy
{};

struct source_leaf
{};

struct runtime_suffix
{
	char const *base{};
	::std::size_t size{};
};

inline constexpr char proxy_spelling[]{'P'};

[[maybe_unused]] inline constexpr source_leaf print_alias_define(
	::fast_io::io_alias_t, source &) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char>
print_alias_define(::fast_io::io_alias_t, runtime_suffix &value) noexcept
{
	return {value.base, value.size};
}

[[maybe_unused]] inline constexpr ::std::true_type
print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, runtime_suffix>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, source_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, source_leaf>, char *iter,
	source_leaf) noexcept
{
	*iter = proxy_spelling[0];
	return iter + 1;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, materialized_proxy>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, materialized_proxy>, char *iter,
	materialized_proxy) noexcept
{
	*iter = proxy_spelling[0];
	return iter + 1;
}

[[maybe_unused]] inline constexpr ::std::size_t
	print_compiler_constant_static_fragments_size(
		::fast_io::io_reserve_type_t<char, materialized_proxy>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<char, materialized_proxy>,
	::fast_io::basic_io_scatter_t<char> *iter,
	materialized_proxy const &) noexcept
{
	*iter++ = {proxy_spelling, 1u};
	return iter;
}

[[maybe_unused]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
{
	return true;
}

[[maybe_unused]] inline constexpr materialized_proxy
print_compiler_constant_materialize(
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
	// This adversarial source is intentionally classified so the test reaches
	// the active-record consumer proof instead of failing at provider admission.
	return {};
}

struct capture_state
{
	::std::array<char, 64u> bytes{};
	::std::size_t size{};
	::std::size_t status_calls{};
	::std::size_t scatter_calls{};
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

[[maybe_unused]] inline constexpr status_sink
output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr statusless_sink
output_stream_ref_define(statusless_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_synchronous_direct_scatter_output(
		::fast_io::io_reserve_type_t<char, status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_synchronous_direct_scatter_output(
		::fast_io::io_reserve_type_t<char, statusless_sink>) noexcept
{
	return {};
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
[[maybe_unused]] inline void scatter_write_all_overflow_define(
	sink_type sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		assert(len <= sink.state->bytes.size() - sink.state->size);
		for (::std::size_t offset{}; offset != len; ++offset)
		{
			sink.state->bytes[sink.state->size++] = base[offset];
		}
	}
}

using preserved_wrapper = ::fast_io::details::decay::
	print_compiler_constant_static_fragment_proxy<char, materialized_proxy>;
using normalized_suffix = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_normalized_t<
		char, false, runtime_suffix &>;
using modeled_replacement = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_replacement_normalized_t<
		char, false, source &>;

// Prefix classification must ignore a preserved proxy outside the proven contiguous run and admit the identical proxy
// when the run reaches it. These assertions cover the zero, strict-prefix, and complete-prefix boundaries of the
// position fold without relying on any output strategy selected later.
static_assert(!::fast_io::details::decay::
	print_first_n_contains_static_fragment<
		0u, char, preserved_wrapper, normalized_suffix>::value);
static_assert(::fast_io::details::decay::
	print_first_n_contains_static_fragment<
		1u, char, preserved_wrapper, normalized_suffix>::value);
static_assert(!::fast_io::details::decay::
	print_first_n_contains_static_fragment<
		1u, char, normalized_suffix, preserved_wrapper>::value);
static_assert(::fast_io::details::decay::
	print_first_n_contains_static_fragment<
		2u, char, normalized_suffix, preserved_wrapper>::value);

template <bool line>
[[maybe_unused]] inline void status_print_define(
	status_sink sink, preserved_wrapper &, normalized_suffix &) noexcept
{
	++sink.state->status_calls;
}

static_assert(::fast_io::compiler_constant_static_fragment_printable<
			  char, materialized_proxy>);
static_assert(::std::same_as<
			  normalized_suffix, ::fast_io::basic_io_scatter_t<char>>);
static_assert(!::std::same_as<modeled_replacement, preserved_wrapper>);
static_assert(
	!::fast_io::operations::decay::defines::has_status_print_define<
		false, status_sink, modeled_replacement, normalized_suffix>);
static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, status_sink, preserved_wrapper, normalized_suffix>);

// The all-fragment endpoint is deliberately unavailable because the suffix is
// a run-time scatter. The ordinary true arm would therefore preserve the
// materialized proxy in an internal descriptor wrapper before generic status
// dispatch. Admission must prove that exact wrapper pack, not merely proxy&&.
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_fragment_run_available<
			false, statusless_sink, source &, runtime_suffix &>());
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			false, status_sink, source &, runtime_suffix &>());
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			true, status_sink, source &, runtime_suffix &>());
// The statusless destination has no additional semantic rejection. Its
// availability therefore follows the compiler-specific source-candidate
// partition exactly: admitted GNU consumers prove the wrapper pack, while a
// fail-closed frontend never forms that replacement graph.
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			false, statusless_sink, source &, runtime_suffix &>() ==
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, source &>);
static_assert(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			true, statusless_sink, source &, runtime_suffix &>() ==
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, source &>);

[[nodiscard]] inline ::std::string_view captured(
	capture_state const &state) noexcept
{
	return {state.bytes.data(), state.size};
}

inline void run()
{
	source value;
	runtime_suffix suffix{"Q", 1u};

	capture_state public_state;
	status_sink public_sink{__builtin_addressof(public_state)};
	::fast_io::io::print(public_sink, value, suffix);
	::fast_io::io::println(public_sink, value, suffix);
	::fast_io::io::perr(public_sink, value, suffix);
	::fast_io::io::perrln(public_sink, value, suffix);
	assert(public_state.status_calls == 0u);
	assert(public_state.scatter_calls == 4u);
	assert(captured(public_state) == "PQPQ\nPQPQ\n");

	// The historical source bridge is the semantic reference. It never exposes
	// the strategy-owned wrapper to status customization.
	capture_state baseline_state;
	status_sink baseline_sink{__builtin_addressof(baseline_state)};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, value, suffix);
	assert(baseline_state.status_calls == 0u);
	assert(baseline_state.scatter_calls == 4u);
	assert(captured(baseline_state) == captured(public_state));

	// The extra proof is selective: an otherwise identical destination without
	// wrapper status ownership still admits and executes the optimization.
	capture_state statusless_state;
	::fast_io::io::print(
		statusless_sink{__builtin_addressof(statusless_state)}, value, suffix);
	assert(statusless_state.status_calls == 0u);
	assert(statusless_state.scatter_calls == 1u);
	assert(captured(statusless_state) == "PQ");
}

} // namespace preserved_fragment_case

namespace semantic_replacement_case
{

struct first_leaf
{};

struct second_leaf
{};

struct source
{};

struct source_leaf
{};

using semantic_proxy = decltype(::fast_io::mnp::pack(first_leaf{}, second_leaf{}));

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, first_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, first_leaf>, char *iter,
	first_leaf) noexcept
{
	*iter = 'A';
	return iter + 1;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, second_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, second_leaf>, char *iter,
	second_leaf) noexcept
{
	*iter = 'B';
	return iter + 1;
}

[[maybe_unused]] inline constexpr source_leaf print_alias_define(
	::fast_io::io_alias_t, source &) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, source_leaf>) noexcept
{
	return 2u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, source_leaf>, char *iter,
	source_leaf) noexcept
{
	*iter++ = 'A';
	*iter++ = 'B';
	return iter;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, semantic_proxy>) noexcept
{
	return 2u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, semantic_proxy>, char *iter,
	semantic_proxy) noexcept
{
	*iter++ = 'A';
	*iter++ = 'B';
	return iter;
}

[[maybe_unused]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
{
	return true;
}

[[maybe_unused]] inline constexpr semantic_proxy
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
{
	return ::fast_io::mnp::pack(first_leaf{}, second_leaf{});
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
	// The graph marker keeps this test focused on rejection of a semantic
	// replacement rather than rejection of an unclassified provider.
	return {};
}

struct capture_state
{
	::std::array<char, 64u> bytes{};
	::std::size_t size{};
	::std::size_t status_calls{};
	::std::size_t scatter_calls{};
};

struct capture_sink
{
	using output_char_type = char;
	capture_state *state{};
};

[[maybe_unused]] inline constexpr capture_sink
output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type
	print_synchronous_direct_scatter_output(
		::fast_io::io_reserve_type_t<char, capture_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline void scatter_write_all_overflow_define(
	capture_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		assert(len <= sink.state->bytes.size() - sink.state->size);
		for (::std::size_t offset{}; offset != len; ++offset)
		{
			sink.state->bytes[sink.state->size++] = base[offset];
		}
	}
}

template <bool line>
[[maybe_unused]] inline void status_print_define(
	capture_sink sink, first_leaf &, second_leaf &,
	::fast_io::basic_io_scatter_t<char> &) noexcept
{
	++sink.state->status_calls;
}

using replacement_source = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_replacement_t<char, source &>;

static_assert(
	::fast_io::compiler_constant_pre_normalization_safe<char, source>);
static_assert(
	!::fast_io::details::decay::print_semantic_input_argument_v<
		char, source &>);
// A frontend which rejects this custom source before its optimizer query keeps
// the historical source type. An admitted frontend forms the semantic
// replacement and must classify that exact replacement as an operation node.
static_assert(
	::fast_io::details::decay::print_semantic_input_argument_v<
		char, replacement_source> ==
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_candidate_v<
			char, source &>);
static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, capture_sink, first_leaf, second_leaf,
		::fast_io::basic_io_scatter_t<char>>);

// A semantic replacement's top-level type is not its active operation pack.
// Until the materialization protocol supplies a value-independent expansion
// proof, common admission must fail closed for both line policies.
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			false, capture_sink, source &, ::std::string_view &>());
static_assert(
	!::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_available<
			true, capture_sink, source &, ::std::string_view &>());

[[nodiscard]] inline ::std::string_view captured(
	capture_state const &state) noexcept
{
	return {state.bytes.data(), state.size};
}

inline void run()
{
	source value;
	::std::string_view suffix{"Q"};

	capture_state public_state;
	capture_sink public_sink{__builtin_addressof(public_state)};
	::fast_io::io::print(public_sink, value, suffix);
	::fast_io::io::println(public_sink, value, suffix);
	::fast_io::io::perr(public_sink, value, suffix);
	::fast_io::io::perrln(public_sink, value, suffix);
	assert(public_state.status_calls == 0u);
	assert(public_state.scatter_calls == 4u);
	assert(captured(public_state) == "ABQABQ\nABQABQ\n");

	capture_state baseline_state;
	capture_sink baseline_sink{__builtin_addressof(baseline_state)};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, value, suffix);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, value, suffix);
	assert(baseline_state.status_calls == 0u);
	assert(baseline_state.scatter_calls == 4u);
	assert(captured(baseline_state) == captured(public_state));
}

} // namespace semantic_replacement_case

} // namespace

int main()
{
	::preserved_fragment_case::run();
	::semantic_replacement_case::run();
}
