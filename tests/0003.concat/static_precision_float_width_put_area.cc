#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>

namespace static_precision_float_width_put_area_test
{

template <bool shift_is_nothrow>
struct audited_internal_leaf
{
	unsigned *bound_calls{};
	unsigned *define_calls{};
	unsigned *shift_calls{};
};

template <bool shift_is_nothrow>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>) noexcept
{
	return 2u;
}

template <bool shift_is_nothrow>
inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>,
	char *iter, audited_internal_leaf<shift_is_nothrow> value) noexcept
{
	++*value.define_calls;
	*iter++ = '+';
	*iter++ = 'x';
	return iter;
}

template <bool shift_is_nothrow>
inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>,
	audited_internal_leaf<shift_is_nothrow> value) noexcept(shift_is_nothrow)
{
	++*value.shift_calls;
	return 1u;
}

template <bool shift_is_nothrow>
inline constexpr ::std::true_type
	concat_single_pass_bounded_materialization_preferred(
		::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>) noexcept
{
	return {};
}

template <bool shift_is_nothrow>
inline constexpr ::std::true_type
	print_single_pass_bounded_direct_put_area_safe(
		::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>) noexcept
{
	return {};
}

template <bool shift_is_nothrow>
inline ::std::size_t concat_single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, audited_internal_leaf<shift_is_nothrow>>,
	audited_internal_leaf<shift_is_nothrow> value,
	::std::size_t maximum_size) noexcept
{
	++*value.bound_calls;
	return maximum_size < 2u ? SIZE_MAX : 2u;
}

using accepted_internal_width = decltype(::fast_io::mnp::internal(
	audited_internal_leaf<true>{}, 8u, '0'));
using rejected_internal_width = decltype(::fast_io::mnp::internal(
	audited_internal_leaf<false>{}, 8u, '0'));
using accepted_runtime_width = decltype(::fast_io::mnp::width(
	::fast_io::manipulators::scalar_placement::internal,
	audited_internal_leaf<true>{}, 8u, '0'));
using rejected_runtime_width = decltype(::fast_io::mnp::width(
	::fast_io::manipulators::scalar_placement::right,
	audited_internal_leaf<false>{}, 8u, '0'));

static_assert(::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<char, accepted_internal_width>);
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<char, rejected_internal_width>);
static_assert(::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<char, accepted_runtime_width>);
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_source<char, rejected_runtime_width>);
static_assert(!::fast_io::operations::decay::
				  print_semantic_single_pass_bounded_put_area_run<
					  char, ::fast_io::basic_obuffer_view_ref<char>, accepted_internal_width,
					  rejected_internal_width>());

struct static_internal_format_probe
{
	template <typename field_type>
	[[nodiscard]] consteval bool operator()(field_type &&) const noexcept
	{
		using field_expression = ::std::add_lvalue_reference_t<
			::std::remove_reference_t<field_type>>;
		return ::fast_io::operations::decay::
			print_semantic_single_pass_bounded_put_area_run<
				char, ::fast_io::basic_obuffer_view_ref<char>,
				field_expression>();
	}
};

[[nodiscard]] consteval bool static_internal_format_direct_path_available()
{
	double value{3.125};
	return ::fast_io::fmt::details::lower_format_program<
		::fast_io::fmt::basic_fixed_string{"{:+020.6f}"},
		::fast_io::fmt::brace_fmt_t>(static_internal_format_probe{}, value);
}

static_assert(static_internal_format_direct_path_available());

[[nodiscard]] consteval bool printf_static_internal_direct_path_available()
{
	double value{3.125};
	return ::fast_io::fmt::details::lower_format_program<
		::fast_io::fmt::basic_fixed_string{"%+020.6f"},
		::fast_io::fmt::printf_fmt_t>(static_internal_format_probe{}, value);
}

static_assert(printf_static_internal_direct_path_available());

[[nodiscard]] bool audited_internal_field_is_defined_once()
{
	unsigned bound_calls{};
	unsigned define_calls{};
	unsigned shift_calls{};
	::std::array<char, 32u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::print(output, ::fast_io::mnp::internal(
								 audited_internal_leaf<true>{
									 &bound_calls, &define_calls, &shift_calls},
								 8u, '0'));
	return ::std::string_view{output.data(), output.size()} == "+000000x" &&
		   bound_calls == 1u && define_calls == 1u && shift_calls == 1u;
}

[[nodiscard]] bool audited_runtime_internal_field_is_defined_once()
{
	unsigned bound_calls{};
	unsigned define_calls{};
	unsigned shift_calls{};
	::std::array<char, 32u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::print(output, ::fast_io::mnp::width(
								 ::fast_io::manipulators::scalar_placement::internal,
								 audited_internal_leaf<true>{
									 &bound_calls, &define_calls, &shift_calls},
								 8u, '0'));
	return ::std::string_view{output.data(), output.size()} == "+000000x" &&
		   bound_calls == 1u && define_calls == 1u && shift_calls == 1u;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type>
[[nodiscard]] bool print_matches_concat(value_type value)
{
	::std::array<char, 512u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::fmt::print<format_literal>(output, value);
	auto const expected{
		::fast_io::fmt::concat_std<format_literal>(value)};
	return output.size() == expected.size() &&
		   ::std::string_view{output.data(), output.size()} == expected;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type>
[[nodiscard]] bool dynamic_width_precision_matches_concat(value_type value)
{
	constexpr unsigned width{20u};
	constexpr unsigned precision{6u};
	::std::array<char, 512u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::fmt::print<format_literal>(output, value, width, precision);
	auto const expected{::fast_io::fmt::concat_std<format_literal>(
		value, width, precision)};
	return output.size() == expected.size() &&
		   ::std::string_view{output.data(), output.size()} == expected;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename value_type>
[[nodiscard]] bool printf_matches_concatf(value_type value)
{
	::std::array<char, 512u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::fmt::printf<format_literal>(output, value);
	auto const expected{::fast_io::fmt::concatf_std<format_literal>(value)};
	return output.size() == expected.size() &&
		   ::std::string_view{output.data(), output.size()} == expected;
}

template <typename value_type>
[[nodiscard]] bool all_width_presentations_match(value_type value)
{
	return print_matches_concat<"{:+020.6f}">(value) &&
		   print_matches_concat<"{:>20.6f}">(value) &&
		   print_matches_concat<"{:<20.6f}">(value) &&
		   print_matches_concat<"{:*^20.6f}">(value) &&
		   print_matches_concat<"{:+5.6f}">(value) &&
		   print_matches_concat<"{:+#020.6a}">(value) &&
		   dynamic_width_precision_matches_concat<"{0:{1}.{2}f}">(value) &&
		   dynamic_width_precision_matches_concat<"{0:+0{1}.{2}f}">(value) &&
		   printf_matches_concatf<"%+020.6f">(value);
}

template <typename value_type>
[[nodiscard]] bool value_matrix_matches()
{
	value_type const values[]{
		static_cast<value_type>(0.0),
		static_cast<value_type>(-0.0),
		static_cast<value_type>(1.25),
		static_cast<value_type>(-1.25),
		static_cast<value_type>(12345.6789012345),
		static_cast<value_type>(-12345.6789012345),
		(::std::numeric_limits<value_type>::denorm_min)(),
		(::std::numeric_limits<value_type>::min)(),
		(::std::numeric_limits<value_type>::max)(),
		::std::numeric_limits<value_type>::infinity(),
		-::std::numeric_limits<value_type>::infinity(),
		::std::numeric_limits<value_type>::quiet_NaN(),
		::std::numeric_limits<value_type>::signaling_NaN()};
	for (auto const value : values)
	{
		if (!all_width_presentations_match(value))
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool known_spellings_are_correct()
{
	::std::array<char, 128u> storage{};
	::fast_io::obuffer_view output{storage};
	::fast_io::fmt::print<"{:+020.6f}|{:+020.6f}|{:+020.6f}">(
		output, 3.125, -3.125, -0.0);
	return ::std::string_view{output.data(), output.size()} ==
		   "+000000000003.125000|-000000000003.125000|-000000000000.000000";
}

} // namespace static_precision_float_width_put_area_test

int main()
{
	using namespace static_precision_float_width_put_area_test;
	return audited_internal_field_is_defined_once() &&
				   audited_runtime_internal_field_is_defined_once() &&
				   known_spellings_are_correct() && value_matrix_matches<float>() &&
				   value_matrix_matches<double>()
			   ? 0
			   : 1;
}
