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
	output_boundary_format{"{::062}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_over_budget_format{"{::064}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	output_parameter_over_budget_format{"{::065}"};
inline constexpr ::fast_io::fmt::basic_fixed_string
	empty_large_element_width_format{"{::20000}"};

using static_std_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type>;
using static_c_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_c_array_type>;
using static_nested_c_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::fmt::static_array_arg<nested_c_values>())>;
using static_tuple_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_tuple_type>;
using output_boundary_argument = decltype(::fast_io::mnp::static_arg<output_boundary_values>);
using output_boundary_program =
	::fast_io::fmt::details::compiled_static_format_program<
		output_boundary_format, ::fast_io::fmt::brace_fmt_t,
		output_boundary_argument>;
using empty_large_element_width_argument = decltype(::fast_io::mnp::static_arg<empty_values>);
using empty_large_element_width_program =
	::fast_io::fmt::details::compiled_static_format_program<
		empty_large_element_width_format, ::fast_io::fmt::brace_fmt_t,
		empty_large_element_width_argument>;

static_assert(::std::string_view{
				  static_std_array_program::storage.data(), static_std_array_program::size} ==
			  "[1, 15, 255]");
static_assert(::std::string_view{
				  static_c_array_program::storage.data(), static_c_array_program::size} ==
			  "[-2, 0, 7]");
static_assert(::std::string_view{
				  static_nested_c_array_program::storage.data(),
				  static_nested_c_array_program::size} == "[[1, 2], [3, 4]]");
static_assert(::std::string_view{
				  static_tuple_program::storage.data(), static_tuple_program::size} ==
			  "(7, 1, [2, 3])");
static_assert(::fast_io::fmt::details::static_format_program<
			  output_boundary_format, ::fast_io::fmt::brace_fmt_t,
			  output_boundary_argument>());
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
static_assert(::fast_io::fmt::details::static_format_program<
			  empty_large_element_width_format, ::fast_io::fmt::brace_fmt_t,
			  empty_large_element_width_argument>());
static_assert(::std::string_view{
				  empty_large_element_width_program::storage.data(),
				  empty_large_element_width_program::size} == "[]");
static_assert(output_boundary_program::size ==
			  ::fast_io::fmt::details::static_format_output_code_unit_limit);

using static_tuple_64_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_tuple_64_type>;
static_assert(static_tuple_64_program::size == 246u);
static_assert(!::fast_io::fmt::details::
				   make_static_format_aggregate_shape<
					   char, static_tuple_65_source_type>()
					   .tuple_budget);
static_assert(!::fast_io::fmt::details::
				   make_static_format_aggregate_shape<
					   char, ::std::array<static_tuple_65_source_type, 1u>>()
					   .tuple_budget);

using static_hex_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		hex_sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type>;
static_assert(::std::string_view{
				  static_hex_array_program::storage.data(), static_hex_array_program::size} ==
			  "[0001, 000f, 00ff]");

inline constexpr ::fast_io::fmt::basic_fixed_string floating_sequence_format{
	"{::.2f}"};
using static_floating_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		floating_sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<floating_values>)>;
static_assert(::std::string_view{
	static_floating_array_program::storage.data(),
	static_floating_array_program::size} == "[1.25, -0.00, 3.50]");

using static_nested_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<nested_values>)>;
static_assert(::std::string_view{
				  static_nested_program::storage.data(), static_nested_program::size} ==
			  "[[1, 2], [3, 4]]");

using static_character_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<character_values>)>;
static_assert(::std::string_view{
				  static_character_program::storage.data(), static_character_program::size} ==
			  "['a', '\\n', '\\'']");

using static_empty_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<empty_values>)>;
static_assert(::std::string_view{
				  static_empty_program::storage.data(), static_empty_program::size} == "[]");

using static_pair_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<pair_value>)>;
static_assert(::std::string_view{
				  static_pair_program::storage.data(), static_pair_program::size} ==
			  "(9, 0)");

using static_string_tuple_program =
	::fast_io::fmt::details::compiled_static_format_program<
		sequence_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::fmt::static_tuple_arg<
				 7, ::fast_io::fmt::basic_fixed_string{"hi"}>())>;
static_assert(::std::string_view{
				  static_string_tuple_program::storage.data(),
				  static_string_tuple_program::size} == "(7, \"hi\")");

using static_named_array_program =
	::fast_io::fmt::details::compiled_static_format_program<
		named_sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_named_array_type>;
static_assert(::std::string_view{
				  static_named_array_program::storage.data(),
				  static_named_array_program::size} == "[-2, 0, 7]");

using static_named_tuple_program =
	::fast_io::fmt::details::compiled_static_format_program<
		named_sequence_format, ::fast_io::fmt::brace_fmt_t,
		static_named_tuple_type>;
static_assert(::std::string_view{
				  static_named_tuple_program::storage.data(),
				  static_named_tuple_program::size} == "(7, 1)");

inline constexpr ::fast_io::fmt::basic_fixed_string no_delimiter_format{
	"{:n}"};
using static_no_delimiter_program =
	::fast_io::fmt::details::compiled_static_format_program<
		no_delimiter_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type>;
static_assert(::std::string_view{
				  static_no_delimiter_program::storage.data(),
				  static_no_delimiter_program::size} == "1, 15, 255");

inline constexpr ::fast_io::fmt::basic_fixed_string debug_string_format{
	"{:?s}"};
using static_debug_string_program =
	::fast_io::fmt::details::compiled_static_format_program<
		debug_string_format, ::fast_io::fmt::brace_fmt_t,
		decltype(::fast_io::mnp::static_arg<character_values>)>;
static_assert(::std::string_view{
				  static_debug_string_program::storage.data(),
				  static_debug_string_program::size} == "\"a\\n'\"");

inline constexpr auto partial_plan{
	::fast_io::fmt::details::static_format_groups<
		partial_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type, unsigned>};
static_assert(partial_plan.has_static_replacement);
static_assert(partial_plan.group_count == 2u);
static_assert(partial_plan.is_static[0u]);
static_assert(!partial_plan.is_static[1u]);

inline constexpr ::fast_io::fmt::basic_fixed_string dynamic_element_width_format{
	"{0::0{1}x}"};
inline constexpr auto dynamic_element_width_plan{
	::fast_io::fmt::details::static_format_groups<
		dynamic_element_width_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type, unsigned>};
static_assert(!dynamic_element_width_plan.has_static_replacement);

using static_element_width_program =
	::fast_io::fmt::details::compiled_static_format_program<
		dynamic_element_width_format, ::fast_io::fmt::brace_fmt_t,
		static_std_array_type,
		decltype(::fast_io::mnp::static_arg<4u>)>;
static_assert(::std::string_view{
				  static_element_width_program::storage.data(),
				  static_element_width_program::size} == "[0001, 000f, 00ff]");

using static_narrow_element_width_type =
	decltype(::fast_io::mnp::static_arg<::std::uint8_t{4u}>);
using static_narrow_width_array_type =
	decltype(::fast_io::mnp::static_arg<narrow_width_values>);
inline constexpr auto static_narrow_element_width_plan{
	::fast_io::fmt::details::static_format_groups<
		dynamic_element_width_format, ::fast_io::fmt::brace_fmt_t,
		static_narrow_width_array_type, static_narrow_element_width_type>};
static_assert(static_narrow_element_width_plan.has_static_replacement);
using static_narrow_element_width_program =
	::fast_io::fmt::details::compiled_static_format_program<
		dynamic_element_width_format, ::fast_io::fmt::brace_fmt_t,
		static_narrow_width_array_type, static_narrow_element_width_type>;
static_assert(::std::string_view{
				  static_narrow_element_width_program::storage.data(),
				  static_narrow_element_width_program::size} ==
			  "[0001]");

inline constexpr auto runtime_narrow_element_width_plan{
	::fast_io::fmt::details::static_format_groups<
		dynamic_element_width_format, ::fast_io::fmt::brace_fmt_t,
		static_narrow_width_array_type, ::std::uint8_t>};
static_assert(!runtime_narrow_element_width_plan.has_static_replacement);

using runtime_array_type = ::std::array<unsigned, 3u>;
using runtime_vector_type = ::std::vector<unsigned>;
using runtime_span_type = ::std::span<unsigned const>;
using runtime_string_view_type = ::std::string_view;
static_assert(!::fast_io::fmt::details::static_format_groups<
				   sequence_format, ::fast_io::fmt::brace_fmt_t,
				   runtime_array_type>
				   .has_static_replacement);
static_assert(!::fast_io::fmt::details::static_format_groups<
				   sequence_format, ::fast_io::fmt::brace_fmt_t,
				   runtime_vector_type>
				   .has_static_replacement);
static_assert(!::fast_io::fmt::details::static_format_groups<
				   sequence_format, ::fast_io::fmt::brace_fmt_t,
				   runtime_span_type>
				   .has_static_replacement);
static_assert(!::fast_io::fmt::details::static_format_groups<
				   sequence_format, ::fast_io::fmt::brace_fmt_t,
				   runtime_string_view_type>
				   .has_static_replacement);

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
