#include <fast_io_format.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <tuple>
#include <vector>

inline constexpr ::std::array unsigned_values{1u, 15u, 255u};
inline constexpr ::std::array narrow_width_values{1u};
inline constexpr ::std::array floating_values{1.25, -0.0, 3.5};
inline constexpr int c_values[]{-2, 0, 7};
inline constexpr int nested_c_values[][2]{{1, 2}, {3, 4}};
inline constexpr ::std::array nested_values{
	::std::array{1, 2}, ::std::array{3, 4}};
inline constexpr ::std::array character_values{'a', '\n', '\''};
inline constexpr ::std::array<int, 0u> empty_values{};
inline constexpr ::std::pair pair_value{9, false};
inline constexpr ::std::array<int, 256u> output_boundary_values{};
inline int mutable_c_values[]{1, 2};
inline constexpr char c_text[]{"text"};

[[nodiscard]] inline int runtime_array_value() noexcept
{
	return 1;
}

inline int const nonconstant_c_values[]{runtime_array_value(), 2};

template <auto &array>
concept accepts_static_c_array = requires {
	::fast_io::fmt::static_array_arg<array>();
};

static_assert(accepts_static_c_array<c_values>);
static_assert(!accepts_static_c_array<mutable_c_values>);
static_assert(!accepts_static_c_array<nonconstant_c_values>);
static_assert(!accepts_static_c_array<c_text>);

using depth_one = ::std::array<int, 1u>;
using depth_two = ::std::array<depth_one, 1u>;
using depth_three = ::std::array<depth_two, 1u>;
using depth_four = ::std::array<depth_three, 1u>;
using depth_five = ::std::array<depth_four, 1u>;
using depth_six = ::std::array<depth_five, 1u>;
using depth_seven = ::std::array<depth_six, 1u>;
using depth_eight = ::std::array<depth_seven, 1u>;
using depth_nine = ::std::array<depth_eight, 1u>;

static_assert(::fast_io::fmt::details::
				  make_static_format_aggregate_shape<char, depth_eight>()
					  .depth ==
			  ::fast_io::fmt::details::static_format_aggregate_recursion_limit);
static_assert(::fast_io::fmt::details::
				  make_static_format_aggregate_shape<char, depth_nine>()
					  .depth >
			  ::fast_io::fmt::details::static_format_aggregate_recursion_limit);
static_assert(::fast_io::fmt::details::
				  make_static_format_aggregate_shape<
					  char, ::std::array<int, 256u>>()
					  .elements ==
			  ::fast_io::fmt::details::static_format_aggregate_element_limit);
static_assert(::fast_io::fmt::details::
				  make_static_format_aggregate_shape<
					  char, ::std::array<int, 257u>>()
					  .elements >
			  ::fast_io::fmt::details::static_format_aggregate_element_limit);

template <typename index_sequence>
struct static_tuple_holder_from_sequence;

template <::std::size_t... index>
struct static_tuple_holder_from_sequence<::std::index_sequence<index...>>
{
	using type = ::fast_io::fmt::static_tuple_format_arg<index...>;
};

using static_std_array_type =
	decltype(::fast_io::mnp::static_arg<unsigned_values>);
using static_c_array_type =
	decltype(::fast_io::fmt::static_array_arg<c_values>());
using static_tuple_type = decltype(::fast_io::fmt::static_tuple_arg<
								   7, true, ::std::array{2u, 3u}>());
using static_named_array_type = decltype(::fast_io::fmt::static_named_array_arg<"values", c_values>());
using static_named_tuple_type = decltype(::fast_io::fmt::static_named_tuple_arg<"values", 7, true>());
using static_tuple_64_type = typename static_tuple_holder_from_sequence<
	::std::make_index_sequence<64u>>::type;
using static_tuple_65_type = typename static_tuple_holder_from_sequence<
	::std::make_index_sequence<65u>>::type;
using static_tuple_65_source_type = ::std::remove_cvref_t<decltype(static_tuple_65_type::get())>;

inline constexpr ::fast_io::fmt::basic_fixed_string sequence_format{"{}"};
inline constexpr ::fast_io::fmt::basic_fixed_string hex_sequence_format{
	"{::04x}"};
inline constexpr ::fast_io::fmt::basic_fixed_string partial_format{
	"values={} score={}"};
inline constexpr ::fast_io::fmt::basic_fixed_string named_sequence_format{
	"{values}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_boundary_format{"{::0254}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_over_budget_format{"{::0256}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_parameter_over_budget_format{"{::0257}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_public_endpoint_format{"{::062}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	empty_large_element_width_format{"{::20000}"};

using output_boundary_argument = decltype(::fast_io::mnp::static_arg<output_boundary_values>);
using empty_large_element_width_argument = decltype(::fast_io::mnp::static_arg<empty_values>);

/**
 * Verifies static lowering through the public constexpr output endpoint.
 *
 * Format syntax owns no rendered array: the fixed obuffer is the observable IO
 * destination, while the semantic predicate below separately audits whether a
 * replacement is eligible for type-owned provider lowering.
 */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::basic_fixed_string expected_literal,
		  typename... argument_types>
[[nodiscard]] consteval bool public_consteval_format_matches(
	argument_types... arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<
				  char_type, typename decltype(expected_literal)::value_type>);
	::std::array<char_type, expected_literal.size()> storage{};
	::fast_io::basic_obuffer_view<char_type> output{storage};
	::fast_io::fmt::print<format_literal>(output, arguments...);
	return output.size() == expected_literal.size() &&
		   ::std::basic_string_view<char_type>{storage.data(), output.size()} ==
			   ::std::basic_string_view<char_type>{
				   expected_literal.data(), expected_literal.size()};
}

template <::std::size_t capacity,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] consteval ::std::size_t public_consteval_format_size(
	argument_types... arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	::std::array<char_type, capacity> storage{};
	::fast_io::basic_obuffer_view<char_type> output{storage};
	::fast_io::fmt::print<format_literal>(output, arguments...);
	return output.size();
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::std::size_t field_index, typename... argument_types>
[[nodiscard]] consteval bool replacement_is_static() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<
			format_literal, ::fast_io::fmt::brace_fmt_t>};
	static_assert(field_index < program.field_count);
	return ::fast_io::fmt::details::static_format_replacement<
		format_literal, program.fields[field_index],
		::fast_io::fmt::brace_fmt_t, argument_types...>();
}

static_assert(public_consteval_format_matches<sequence_format, "[1, 15, 255]">(
	::fast_io::mnp::static_arg<unsigned_values>));
static_assert(public_consteval_format_matches<sequence_format, "[-2, 0, 7]">(
	::fast_io::fmt::static_array_arg<c_values>()));
static_assert(public_consteval_format_matches<
			  sequence_format, "[[1, 2], [3, 4]]">(
	::fast_io::fmt::static_array_arg<nested_c_values>()));
static_assert(public_consteval_format_matches<
			  sequence_format, "(7, 1, [2, 3])">(
	::fast_io::fmt::static_tuple_arg<7, true, ::std::array{2u, 3u}>()));
static_assert(replacement_is_static<
			  output_boundary_format, 0u, output_boundary_argument>());
static_assert(!::fast_io::fmt::details::
				  automatic_static_format_output_budget_exceeded<
					  output_boundary_format, ::fast_io::fmt::brace_fmt_t,
					  output_boundary_argument>());
static_assert(::fast_io::fmt::details::
				  automatic_static_format_output_budget_exceeded<
					  output_over_budget_format, ::fast_io::fmt::brace_fmt_t,
					  output_boundary_argument>());
static_assert(::fast_io::fmt::details::
				  automatic_static_format_output_budget_exceeded<
					  output_parameter_over_budget_format,
					  ::fast_io::fmt::brace_fmt_t, output_boundary_argument>());
static_assert(!::fast_io::fmt::details::
				  automatic_static_format_output_budget_exceeded<
					  empty_large_element_width_format, ::fast_io::fmt::brace_fmt_t,
					  empty_large_element_width_argument>());
static_assert(replacement_is_static<
			  empty_large_element_width_format, 0u,
			  empty_large_element_width_argument>());
static_assert(public_consteval_format_matches<
			  empty_large_element_width_format, "[]">(
	::fast_io::mnp::static_arg<empty_values>));
// Keep the public endpoint probe below the compiler's default constexpr-step
// ceiling. The semantic budget assertions above independently cover the exact
// 64 KiB boundary and its first rejected tiers.
static_assert(public_consteval_format_size<16384u,
										   output_public_endpoint_format>(
				  ::fast_io::mnp::static_arg<output_boundary_values>) == 16384u);
static_assert(public_consteval_format_size<246u, sequence_format>(
				  static_tuple_64_type{}) == 246u);
static_assert(!::fast_io::fmt::details::
				   make_static_format_aggregate_shape<
					   char, static_tuple_65_source_type>()
					   .tuple_budget);
static_assert(!::fast_io::fmt::details::
				   make_static_format_aggregate_shape<
					   char, ::std::array<static_tuple_65_source_type, 1u>>()
					   .tuple_budget);

static_assert(public_consteval_format_matches<
			  hex_sequence_format, "[0001, 000f, 00ff]">(
	::fast_io::mnp::static_arg<unsigned_values>));

inline constexpr ::fast_io::fmt::basic_fixed_string floating_sequence_format{
	"{::.2f}"};
static_assert(public_consteval_format_matches<
			  floating_sequence_format, "[1.25, -0.00, 3.50]">(
	::fast_io::mnp::static_arg<floating_values>));
static_assert(public_consteval_format_matches<
			  sequence_format, "[[1, 2], [3, 4]]">(
	::fast_io::mnp::static_arg<nested_values>));
static_assert(public_consteval_format_matches<
			  sequence_format, "['a', '\\n', '\\'']">(
	::fast_io::mnp::static_arg<character_values>));
static_assert(public_consteval_format_matches<sequence_format, "[]">(
	::fast_io::mnp::static_arg<empty_values>));
static_assert(public_consteval_format_matches<sequence_format, "(9, 0)">(
	::fast_io::mnp::static_arg<pair_value>));
static_assert(public_consteval_format_matches<sequence_format, "(7, \"hi\")">(
	::fast_io::fmt::static_tuple_arg<
		7, ::fast_io::fmt::basic_fixed_string{"hi"}>()));
static_assert(public_consteval_format_matches<
			  named_sequence_format, "[-2, 0, 7]">(
	::fast_io::fmt::static_named_array_arg<"values", c_values>()));
static_assert(public_consteval_format_matches<named_sequence_format, "(7, 1)">(
	::fast_io::fmt::static_named_tuple_arg<"values", 7, true>()));

inline constexpr ::fast_io::fmt::basic_fixed_string no_delimiter_format{
	"{:n}"};
static_assert(public_consteval_format_matches<
			  no_delimiter_format, "1, 15, 255">(
	::fast_io::mnp::static_arg<unsigned_values>));

inline constexpr ::fast_io::fmt::basic_fixed_string debug_string_format{
	"{:?s}"};
static_assert(public_consteval_format_matches<debug_string_format, "\"a\\n'\"">(
	::fast_io::mnp::static_arg<character_values>));

static_assert(replacement_is_static<
			  partial_format, 0u, static_std_array_type, unsigned>());
static_assert(!replacement_is_static<
			  partial_format, 1u, static_std_array_type, unsigned>());

inline constexpr ::fast_io::fmt::basic_fixed_string dynamic_element_width_format{
	"{0::0{1}x}"};
static_assert(!replacement_is_static<
			  dynamic_element_width_format, 0u, static_std_array_type, unsigned>());
static_assert(public_consteval_format_matches<
			  dynamic_element_width_format, "[0001, 000f, 00ff]">(
	::fast_io::mnp::static_arg<unsigned_values>,
	::fast_io::mnp::static_arg<4u>));

using static_narrow_element_width_type =
	decltype(::fast_io::mnp::static_arg<::std::uint8_t{4u}>);
using static_narrow_width_array_type =
	decltype(::fast_io::mnp::static_arg<narrow_width_values>);
static_assert(replacement_is_static<
			  dynamic_element_width_format, 0u, static_narrow_width_array_type,
			  static_narrow_element_width_type>());
static_assert(public_consteval_format_matches<
			  dynamic_element_width_format, "[0001]">(
	::fast_io::mnp::static_arg<narrow_width_values>,
	::fast_io::mnp::static_arg<::std::uint8_t{4u}>));
static_assert(!replacement_is_static<
			  dynamic_element_width_format, 0u, static_narrow_width_array_type,
			  ::std::uint8_t>());

using runtime_array_type = ::std::array<unsigned, 3u>;
using runtime_vector_type = ::std::vector<unsigned>;
using runtime_span_type = ::std::span<unsigned const>;
using runtime_string_view_type = ::std::string_view;
static_assert(!replacement_is_static<
			  sequence_format, 0u, runtime_array_type>());
static_assert(!replacement_is_static<
			  sequence_format, 0u, runtime_vector_type>());
static_assert(!replacement_is_static<
			  sequence_format, 0u, runtime_span_type>());
static_assert(!replacement_is_static<
			  sequence_format, 0u, runtime_string_view_type>());

[[nodiscard]] bool runtime_matches()
{
	auto const static_std_array{
		::fast_io::fmt::concat_std<"{}">(
			::fast_io::mnp::static_arg<unsigned_values>)};
	auto const dynamic_std_array{
		::fast_io::fmt::concat_std<"{}">(unsigned_values)};
	auto const static_floating_array{
		::fast_io::fmt::concat_std<floating_sequence_format>(
			::fast_io::mnp::static_arg<floating_values>)};
	auto const dynamic_floating_array{
		::fast_io::fmt::concat_std<floating_sequence_format>(
			floating_values)};
	auto const static_c_array{
		::fast_io::fmt::concat_std<"{}">(
			::fast_io::fmt::static_array_arg<c_values>())};
	auto const dynamic_c_array{
		::fast_io::fmt::concat_std<"{}">(c_values)};
	auto const static_tuple{
		::fast_io::fmt::concat_std<"{}">(
			::fast_io::fmt::static_tuple_arg<
				7, true, ::std::array{2u, 3u}>())};
	auto const dynamic_tuple{
		::fast_io::fmt::concat_std<"{}">(
			::std::tuple{7, true, ::std::array{2u, 3u}})};
	auto const static_partial{
		::fast_io::fmt::concat_std<partial_format>(
			::fast_io::mnp::static_arg<unsigned_values>, 42u)};
	auto const dynamic_partial{
		::fast_io::fmt::concat_std<partial_format>(unsigned_values, 42u)};
	auto const static_narrow_element_width{
		::fast_io::fmt::concat_std<dynamic_element_width_format>(
			::fast_io::mnp::static_arg<narrow_width_values>,
			::fast_io::mnp::static_arg<::std::uint8_t{4u}>)};
	::std::uint8_t const runtime_width{4u};
	auto const runtime_narrow_element_width{
		::fast_io::fmt::concat_std<dynamic_element_width_format>(
			::fast_io::mnp::static_arg<narrow_width_values>, runtime_width)};
	auto const empty_large_element_width{
		::fast_io::fmt::concat_std<empty_large_element_width_format>(
			::fast_io::mnp::static_arg<empty_values>)};
	return static_std_array == dynamic_std_array &&
		   static_floating_array == dynamic_floating_array &&
		   static_c_array == dynamic_c_array &&
		   static_tuple == dynamic_tuple &&
		   static_partial == dynamic_partial &&
		   static_narrow_element_width == runtime_narrow_element_width &&
		   empty_large_element_width == "[]";
}

int main()
{
	return runtime_matches() ? 0 : 1;
}
