#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

using source_static = ::std::remove_cvref_t<
	decltype(::fast_io::mnp::static_arg<"x">)>;
using normalized_static = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_normalized_t<
		char, false, source_static &>;
using normalized_text = ::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_normalized_t<
		char, false, ::std::string_view &>;

static_assert(::std::same_as<
			  normalized_text, ::fast_io::basic_io_scatter_t<char>>);

struct capture_state
{
	::std::array<char, 8u> bytes{};
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

struct category_sensitive_source
{};

inline constexpr char mutable_spelling[]{'N'};
inline constexpr char const_spelling[]{'C'};

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, category_sensitive_source &) noexcept
{
	return {mutable_spelling, 1u};
}

[[maybe_unused]] inline constexpr ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, category_sensitive_source const &) noexcept
{
	return {const_spelling, 1u};
}

[[maybe_unused]] inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, category_sensitive_source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_scatter_output_state_independent(
	::fast_io::io_reserve_type_t<char, category_sensitive_source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	::fast_io::io_reserve_type_t<char, category_sensitive_source>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	::fast_io::io_reserve_type_t<char, category_sensitive_source>) noexcept
{
	return {};
}

inline constexpr status_sink output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

inline constexpr statusless_sink
output_stream_ref_define(statusless_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, statusless_sink>) noexcept
{
	return {};
}

template <bool line>
inline void status_print_define(
	status_sink sink, normalized_static &, normalized_text &) noexcept
{
	++sink.state->status_calls;
}

template <typename sink_type>
	requires(::std::same_as<sink_type, status_sink> ||
			 ::std::same_as<sink_type, statusless_sink>)
inline void scatter_write_all_overflow_define(
	sink_type sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const [base, len]{scatters[index]};
		for (::std::size_t offset{}; offset != len; ++offset)
		{
			sink.state->bytes[sink.state->size++] = base[offset];
		}
	}
}

static_assert(
	!::fast_io::operations::decay::defines::has_status_print_define<
		false, status_sink, source_static, ::std::string_view>);
static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, status_sink, normalized_static, normalized_text>);

// Provider/scatter substitution is available for the same physical record on
// a destination without status ownership. Once the historically normalized
// operation pack has an exact status CPO, the pre-normalization mixed shortcut
// must fail closed even though the source-type pack itself has no such CPO.
static_assert(
	::fast_io::operations::decay::
		print_static_provider_mixed_run_available<
			false, statusless_sink,
			source_static &, ::std::string_view &>());
static_assert(
	!::fast_io::operations::decay::
		print_static_provider_mixed_run_available<
			false, status_sink,
			source_static &, ::std::string_view &>());
static_assert(
	::fast_io::operations::decay::
		print_static_provider_mixed_dynamic_component_v<
			char, category_sensitive_source &>);

} // namespace

int main()
{
	::std::string_view text{"y"};

	capture_state public_state;
	status_sink public_sink{__builtin_addressof(public_state)};
	::fast_io::io::print(
		public_sink, ::fast_io::mnp::static_arg<"x">, text);
	::fast_io::io::println(
		public_sink, ::fast_io::mnp::static_arg<"x">, text);
	::fast_io::io::perr(
		public_sink, ::fast_io::mnp::static_arg<"x">, text);
	::fast_io::io::perrln(
		public_sink, ::fast_io::mnp::static_arg<"x">, text);
	assert(public_state.status_calls == 4u);
	assert(public_state.scatter_calls == 0u);
	assert(public_state.size == 0u);

	// The ordinary normalized entry is the semantic baseline: it observes the
	// same exact status pack and performs no primitive output.
	capture_state baseline_state;
	status_sink baseline_sink{__builtin_addressof(baseline_state)};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		baseline_sink, ::fast_io::mnp::static_arg<"x">, text);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<true>(
		baseline_sink, ::fast_io::mnp::static_arg<"x">, text);
	assert(baseline_state.status_calls == 2u);
	assert(baseline_state.scatter_calls == 0u);
	assert(baseline_state.size == 0u);

	// The added status proof does not disable the optimization for a destination
	// which has no normalized whole-record owner.
	capture_state statusless_state;
	::fast_io::io::print(
		statusless_sink{__builtin_addressof(statusless_state)},
		::fast_io::mnp::static_arg<"x">, text);
	assert(statusless_state.scatter_calls == 1u);
	assert(statusless_state.size == 2u);
	assert((::std::string_view{
				statusless_state.bytes.data(), statusless_state.size} == "xy"));

	// Public source normalization observes every named argument as an lvalue.
	// The mixed shortcut must use that same category; projecting through const&
	// would select a different valid ADL overload and change the byte sequence.
	category_sensitive_source category_sensitive;
	capture_state category_state;
	::fast_io::io::print(
		statusless_sink{__builtin_addressof(category_state)},
		::fast_io::mnp::static_arg<"x">, category_sensitive);
	assert(category_state.scatter_calls == 1u);
	assert(category_state.size == 2u);
	assert((::std::string_view{
				category_state.bytes.data(), category_state.size} == "xN"));

	capture_state category_baseline_state;
	statusless_sink category_baseline{
		__builtin_addressof(category_baseline_state)};
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		category_baseline, ::fast_io::mnp::static_arg<"x">,
		category_sensitive);
	assert(category_baseline_state.scatter_calls == 1u);
	assert(category_baseline_state.size == 2u);
	assert((::std::string_view{
				category_baseline_state.bytes.data(),
				category_baseline_state.size} == "xN"));
}
