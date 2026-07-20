#include <fast_io.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dynamic_float_put_area_test
{

struct legacy_cost_marker_source
{};

inline constexpr ::std::true_type
	concat_single_pass_bounded_materialization_preferred(
		::fast_io::io_reserve_type_t<char, legacy_cost_marker_source>) noexcept
{
	return {};
}

inline constexpr ::std::size_t
concat_single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, legacy_cost_marker_source>,
	legacy_cost_marker_source, ::std::size_t maximum_size) noexcept
{
	return maximum_size == 0u ? SIZE_MAX : 1u;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, legacy_cost_marker_source>,
	legacy_cost_marker_source) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, legacy_cost_marker_source>,
	char *iter, legacy_cost_marker_source) noexcept
{
	*iter++ = 'x';
	return iter;
}

struct potentially_throwing_companion
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, potentially_throwing_companion>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, potentially_throwing_companion>,
	char *iter, potentially_throwing_companion)
{
	*iter++ = 'x';
	return iter;
}

struct dynamic_format_direct_path_probe
{
	template <typename prefix_type, typename field_type>
	[[nodiscard]] constexpr bool operator()(
		prefix_type &&, field_type &&) const noexcept
	{
		using prefix_expression = ::std::add_lvalue_reference_t<
			::std::remove_reference_t<prefix_type>>;
		using field_expression = ::std::add_lvalue_reference_t<
			::std::remove_reference_t<field_type>>;
		return ::fast_io::operations::decay::
			print_semantic_single_pass_bounded_put_area_run<
				char, ::fast_io::basic_obuffer_view_ref<char>,
				prefix_expression, field_expression>();
	}
};

[[nodiscard]] consteval bool dynamic_format_direct_path_available()
{
	double value{3.14};
	unsigned width{8u};
	unsigned precision{2u};
	return ::fast_io::fmt::details::lower_format_program<
		::fast_io::fmt::basic_fixed_string{"v={0:{1}.{2}f}"},
		::fast_io::fmt::brace_fmt_t>(
		dynamic_format_direct_path_probe{}, value, width, precision);
}

static_assert(dynamic_format_direct_path_available());
static_assert(::fast_io::dynamic_reserve_printable<
			  char, legacy_cost_marker_source>);
static_assert(::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_define_nothrow<
					  char, legacy_cost_marker_source &>());
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<
					  char, legacy_cost_marker_source>);
static_assert(::fast_io::reserve_printable<
			  char, potentially_throwing_companion>);
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_passive_companion<
					  char, potentially_throwing_companion>);
using internal_dynamic_float_type = decltype(::fast_io::mnp::internal(
	::fast_io::mnp::fixed(3.14, 2u), 8u));
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<
					  char, internal_dynamic_float_type>);

struct observed_buffer
{
	using output_char_type = char;
	::std::array<char, 512u> storage{};
	char *current{storage.data()};
	::std::size_t curr_calls{};
	::std::size_t end_calls{};
	::std::size_t set_calls{};
};

struct observed_buffer_ref
{
	using output_char_type = char;
	observed_buffer *state{};
};

struct rejected_bounded_output
{
	using output_char_type = char;
	::std::string *storage{};
	::std::size_t *write_calls{};
};

inline constexpr rejected_bounded_output output_stream_ref_define(
	rejected_bounded_output output) noexcept
{
	return output;
}

inline constexpr ::std::size_t full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char, rejected_bounded_output>) noexcept
{
	// This admits print's 512-code-unit single-pass probe but deliberately
	// rejects the 600-digit field below, exercising its ordered fallback.
	return 512u;
}

inline void write_all_overflow_define(rejected_bounded_output output,
	char const *first, char const *last)
{
	++*output.write_calls;
	output.storage->append(first, last);
}

inline constexpr observed_buffer_ref output_stream_ref_define(
	observed_buffer &output) noexcept
{
	return {__builtin_addressof(output)};
}

inline constexpr char *obuffer_begin(observed_buffer_ref output) noexcept
{
	return output.state->storage.data();
}

inline char *obuffer_curr(observed_buffer_ref output) noexcept
{
	++output.state->curr_calls;
	return output.state->current;
}

inline char *obuffer_end(observed_buffer_ref output) noexcept
{
	++output.state->end_calls;
	return output.state->storage.data() + output.state->storage.size();
}

inline void obuffer_set_curr(observed_buffer_ref output, char *current) noexcept
{
	++output.state->set_calls;
	output.state->current = current;
}

inline void obuffer_overflow(observed_buffer_ref, char) noexcept
{
	::fast_io::fast_terminate();
}

inline void write_all_overflow_define(
	observed_buffer_ref output, char const *first, char const *last) noexcept
{
	auto &state{*output.state};
	auto const size{static_cast<::std::size_t>(last - first)};
	auto const available{static_cast<::std::size_t>(
		state.storage.data() + state.storage.size() - state.current)};
	if (available < size)
	{
		::fast_io::fast_terminate();
	}
	state.current = ::fast_io::details::non_overlapped_copy_n(
		first, size, state.current);
}

[[nodiscard]] bool direct_put_area_matches()
{
	::std::array<char, 514u> storage{};
	storage.fill('#');
	::fast_io::obuffer_view output{storage.data() + 1u,
								   storage.data() + storage.size() - 1u};
	unsigned const width{8u};
	unsigned const precision{2u};
	::fast_io::fmt::print<"v={0:{1}.{2}f}">(
		output, 3.14, width, precision);
	return storage.front() == '#' && storage.back() == '#' &&
		   ::std::string_view{output.data(), output.size()} == "v=    3.14";
}

[[nodiscard]] bool exact_capacity_falls_back_without_overflow()
{
	::std::array<char, 10u> storage{};
	storage.fill('#');
	::fast_io::obuffer_view output{storage.data() + 1u,
								   storage.data() + storage.size() - 1u};
	unsigned const width{8u};
	unsigned const precision{2u};
	::fast_io::fmt::print<"{0:{1}.{2}f}">(
		output, 3.14, width, precision);
	return storage.front() == '#' && storage.back() == '#' &&
		   output.size() == 8u &&
		   ::std::string_view{output.data(), output.size()} == "    3.14";
}

[[nodiscard]] bool exact_capacity_line_falls_back_without_overflow()
{
	::std::array<char, 11u> storage{};
	storage.fill('#');
	::fast_io::obuffer_view output{storage.data() + 1u,
								   storage.data() + storage.size() - 1u};
	auto field{::fast_io::mnp::width(
		::fast_io::mnp::scalar_placement::right,
		::fast_io::mnp::fixed(3.14, 2u), 8u)};
	::fast_io::println(output, field);
	return storage.front() == '#' && storage.back() == '#' &&
		   output.size() == 9u &&
		   ::std::string_view{output.data(), output.size()} == "    3.14\n";
}

[[nodiscard]] bool const_wrappers_are_printable()
{
	::std::array<char, 512u> storage{};
	::fast_io::obuffer_view output{storage};
	auto const field{::fast_io::mnp::width(
		::fast_io::mnp::scalar_placement::right,
		::fast_io::mnp::fixed(3.14, 2u), 10u)};
	::fast_io::print(output, field);

	using scalar_type = decltype(::fast_io::mnp::fixed(3.14, 2u));
	using format_scalar_type =
		::fast_io::manipulators::format_scalar_t<scalar_type, 0u, false>;
	format_scalar_type const scalar{
		::fast_io::mnp::fixed(3.14, 2u)};
	::fast_io::print(output, scalar);

	using force_radix_type =
		::fast_io::manipulators::printf_force_radix_t<format_scalar_type>;
	force_radix_type const force_radix{
		format_scalar_type{::fast_io::mnp::fixed(3.0, 0u)}, true};
	::fast_io::print(output, force_radix);

	return ::std::string_view{output.data(), output.size()} ==
		   "      3.143.143.";
}

[[nodiscard]] bool wide_character_domain_matches()
{
	::std::array<char16_t, 512u> storage{};
	::fast_io::u16obuffer_view output{storage};
	unsigned const width{8u};
	unsigned const precision{2u};
	::fast_io::fmt::u16print<u"v={0:{1}.{2}f}">(
		output, 3.14, width, precision);
	return ::std::u16string_view{output.data(), output.size()} ==
		   u"v=    3.14";
}

[[nodiscard]] bool unmarked_obuffer_keeps_precise_probe_order()
{
	// Merely exposing cursor CPOs does not opt this sink into speculative
	// deferred-commit probing.  The established precise path observes each
	// cursor once after sizing, then publishes the final cursor once.
	observed_buffer output{};
	unsigned const width{8u};
	unsigned const precision{2u};
	::fast_io::fmt::print<"{0:{1}.{2}f}">(
		output, 3.14, width, precision);
	return output.curr_calls == 1u && output.end_calls == 1u &&
		   output.set_calls == 1u &&
		   ::std::string_view{output.storage.data(),
							  static_cast<::std::size_t>(
								  output.current - output.storage.data())} == "    3.14";
}

[[nodiscard]] bool large_precision_rejected_bound_matches_concat()
{
	double const value{3.125};
	unsigned const width{700u};
	unsigned const precision{600u};
	::std::string actual;
	::std::size_t write_calls{};
	rejected_bounded_output output{__builtin_addressof(actual), &write_calls};
	::fast_io::fmt::print<"v={0:*^{1}.{2}f}">(
		output, value, width, precision);
	auto const expected{::fast_io::fmt::concat_std<"v={0:*^{1}.{2}f}">(
		value, width, precision)};
	// The post-rejection fallback may stream ordered components, so only byte
	// identity and forward progress are contractual here—not a write count.
	return write_calls != 0u && actual == expected;
}

} // namespace dynamic_float_put_area_test

int main()
{
	using namespace dynamic_float_put_area_test;
	return direct_put_area_matches() &&
				   exact_capacity_falls_back_without_overflow() &&
				   exact_capacity_line_falls_back_without_overflow() &&
				   const_wrappers_are_printable() &&
				   wide_character_domain_matches() &&
				   unmarked_obuffer_keeps_precise_probe_order() &&
				   large_precision_rejected_bound_matches_concat()
			   ? 0
			   : 1;
}
