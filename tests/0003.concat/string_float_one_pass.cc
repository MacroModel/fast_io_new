#if defined(__GNUC__) && !defined(__clang__)
// This test deliberately feeds an opaque SIZE_MAX precision to non-finite and
// zero-only fallback cases. GCC still diagnoses the unreachable finite padding
// arm while inlining the formatter; sanitizers exercise the same calls below.
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#include <fast_io_format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace concat_float_one_pass_test
{

volatile ::std::size_t maximum_precision_source{SIZE_MAX};

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::size_t runtime_maximum_precision() noexcept
{
	auto value{maximum_precision_source};
#if defined(__GNUC__) || defined(__clang__)
	__asm__ volatile("" : "+r"(value));
#endif
	return value;
}

[[nodiscard]] inline auto fixed6(double value) noexcept
{
	return ::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
		value, 6u);
}

[[nodiscard]] inline auto fixed_max(double value, ::std::size_t precision) noexcept
{
	return ::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
		value, precision);
}

[[nodiscard]] inline auto fixed_trim(double value, ::std::size_t precision) noexcept
{
	return ::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional>(value, precision);
}

template <::std::integral char_type>
[[nodiscard]] bool candidate_bound_boundary_is_correct() noexcept
{
	auto value{::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
		3.125, 0u)};
	using value_type = decltype(value);
	constexpr ::std::size_t budget{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_stack_size<char_type>()};
	static_assert(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			char_type, value_type>() == budget);
	static_assert(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			char_type, value_type, value_type>() <= budget);
	static_assert(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			char_type, value_type, value_type>() * sizeof(char_type) <= 512u);
	// Exercise the public destination-neutral accessor rather than reaching into a consumer-specific CPO.
	auto const base_size{
		::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
			value, SIZE_MAX)};
	if (base_size == SIZE_MAX)
	{
		return false;
	}
	if (budget < base_size)
	{
		return ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
			value, budget) == SIZE_MAX;
	}
	value.precision = budget - base_size;
	if (::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
			value, budget) != budget)
	{
		return false;
	}
	++value.precision;
	return ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
		value, budget) == SIZE_MAX;
}

struct stateful_dynamic_leaf
{
	::std::size_t bound;
	unsigned *size_calls;
	unsigned *define_calls;
};

[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, stateful_dynamic_leaf>,
	stateful_dynamic_leaf value) noexcept
{
	++*value.size_calls;
	return value.bound;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, stateful_dynamic_leaf>, char *output,
	stateful_dynamic_leaf value) noexcept
{
	++*value.define_calls;
	*output = 'x';
	return output + 1;
}

struct throwing_static_leaf
{
	unsigned *define_calls;
};

struct rvalue_only_candidate_leaf
{};

struct oversized_static_leaf
{};

struct observed_candidate_leaf
{
	unsigned *candidate_calls;
};

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, rvalue_only_candidate_leaf>,
	rvalue_only_candidate_leaf const &) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, rvalue_only_candidate_leaf>, char *output,
	rvalue_only_candidate_leaf const &) noexcept
{
	*output = 'x';
	return output + 1;
}

[[nodiscard]] inline constexpr ::std::true_type
single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char, rvalue_only_candidate_leaf>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::size_t
single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, rvalue_only_candidate_leaf>,
	rvalue_only_candidate_leaf &&, ::std::size_t) noexcept
{
	return 1u;
}

static_assert(!::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
	char, rvalue_only_candidate_leaf>);

using fixed_type = decltype(fixed6(3.125));
using fixed_width_type =
	decltype(::fast_io::mnp::right(fixed6(3.125), 12u));
using fixed_width_ch_type =
	decltype(::fast_io::mnp::left(fixed6(3.125), 12u, '_'));
using runtime_width_type = decltype(::fast_io::mnp::width(
	::fast_io::manipulators::scalar_placement::middle, fixed6(3.125), 12u));
using runtime_width_ch_type = decltype(::fast_io::mnp::width(
	::fast_io::manipulators::scalar_placement::right, fixed6(3.125), 12u,
	'0'));
using forced_radix_type =
	::fast_io::manipulators::printf_force_radix_t<fixed_type>;
using unmarked_width_type =
	decltype(::fast_io::mnp::right(stateful_dynamic_leaf{}, 4u));
using rvalue_only_width_type =
	decltype(::fast_io::mnp::right(rvalue_only_candidate_leaf{}, 4u));

static_assert(
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, fixed_width_type>);
static_assert(
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, fixed_width_ch_type>);
static_assert(
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, runtime_width_type>);
static_assert(
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, runtime_width_ch_type>);
static_assert(
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, forced_radix_type>);
static_assert(
	!::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, unmarked_width_type>);
static_assert(
	!::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<
		char, rvalue_only_width_type>);

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, oversized_static_leaf>) noexcept
{
	return 2048u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, oversized_static_leaf>, char *output,
	oversized_static_leaf) noexcept
{
	return ::fast_io::details::my_fill_n(output, 2048u, 'z');
}

[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, observed_candidate_leaf>,
	observed_candidate_leaf) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, observed_candidate_leaf>, char *output,
	observed_candidate_leaf) noexcept
{
	*output = 'x';
	return output + 1;
}

[[nodiscard]] inline constexpr ::std::true_type
single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char, observed_candidate_leaf>) noexcept
{
	return {};
}

[[nodiscard]] inline ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, observed_candidate_leaf>,
	observed_candidate_leaf value, ::std::size_t maximum_size) noexcept
{
	++*value.candidate_calls;
	return maximum_size == 0u ? SIZE_MAX : 1u;
}

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, throwing_static_leaf>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_static_leaf>, char *,
	throwing_static_leaf value)
{
	++*value.define_calls;
	throw ::std::runtime_error{"throwing concat leaf"};
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
[[nodiscard]] bool extreme_precision_is_correct()
{
	// An attacker-controlled precision must reject the speculative bound without
	// invoking ordinary reserve-size overflow. Precise fallback still recognizes
	// non-finite values and zero bodies before inspecting irrelevant precision.
	auto const infinity{::std::numeric_limits<double>::infinity()};
	auto const quiet_nan{::std::numeric_limits<double>::quiet_NaN()};
	auto const maximum_precision{runtime_maximum_precision()};
	auto maximum_scalar{fixed_max(infinity, maximum_precision)};
	using maximum_scalar_type = decltype(maximum_scalar);
	auto wrapped_maximum_scalar{
		::fast_io::manipulators::format_scalar_t<maximum_scalar_type, 0u, false>{
			maximum_scalar}};
	return ::fast_io::concat_std(fixed_max(infinity, maximum_precision)) == "inf" &&
		::fast_io::concat_std(fixed_max(quiet_nan, maximum_precision)) == "nan" &&
		::fast_io::concat_std(fixed_trim(0.0, maximum_precision)) == "0" &&
		::fast_io::concat_std(fixed_trim(-0.0, maximum_precision)) == "-0" &&
		::fast_io::wconcat_std(fixed_max(infinity, maximum_precision)) == L"inf" &&
		::fast_io::u8concat_std(fixed_max(infinity, maximum_precision)) == u8"inf" &&
		::fast_io::u16concat_std(fixed_max(infinity, maximum_precision)) == u"inf" &&
		::fast_io::u32concat_std(fixed_max(infinity, maximum_precision)) == U"inf" &&
		::fast_io::concat_std(wrapped_maximum_scalar) == "inf" &&
		::fast_io::fmt::concat_std<"{:.{}f}">(
			infinity, static_cast<::std::size_t>((::std::numeric_limits<int>::max)())) == "inf";
}

consteval bool constexpr_float_is_correct()
{
	return ::fast_io::fmt::concat_std<"{:.2f}">(3.125) == "3.12";
}

static_assert(constexpr_float_is_correct());

consteval bool constexpr_large_float_is_correct()
{
	auto const text{::fast_io::concat_std(::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
		3.125, 600u))};
	auto const wide_text{::fast_io::wconcat_std(::fast_io::mnp::fixed<
		::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
		3.125, 200u))};
	return text.size() == 602u && text.compare(0u, 8u, "3.125000") == 0 &&
		wide_text.size() == 202u && wide_text.compare(0u, 8u, L"3.125000") == 0;
}

static_assert(constexpr_large_float_is_correct());

consteval bool constexpr_float_wrappers_are_correct()
{
	return ::fast_io::fmt::concat_std<"{:12.2f}">(3.125) ==
			   "        3.12" &&
		::fast_io::fmt::concat_std<"{:{}.2f}">(3.125, 12u) ==
			"        3.12" &&
		::fast_io::fmt::concatf_std<"%#8.0f">(3.125) == "      3.";
}

static_assert(constexpr_float_wrappers_are_correct());

[[nodiscard]] bool four_width_representations_are_correct()
{
	return ::fast_io::concat_std(
			   ::fast_io::mnp::right(fixed6(3.125), 12u)) ==
			   "    3.125000" &&
		::fast_io::concat_std(
			::fast_io::mnp::left(fixed6(3.125), 12u, '_')) ==
			"3.125000____" &&
		::fast_io::concat_std(::fast_io::mnp::width(
			::fast_io::manipulators::scalar_placement::middle,
			fixed6(3.125), 12u)) == "  3.125000  " &&
		::fast_io::concat_std(::fast_io::mnp::width(
			::fast_io::manipulators::scalar_placement::right,
			fixed6(3.125), 12u, '0')) == "00003.125000" &&
		::fast_io::concatln_std(
			::fast_io::mnp::right(fixed6(3.125), 12u)) ==
			"    3.125000\n";
}

[[nodiscard]] bool brace_and_printf_wrappers_are_correct()
{
	return ::fast_io::fmt::concat_std<"[{:12.2f}]">(3.125) ==
			   "[        3.12]" &&
		::fast_io::fmt::concat_std<"[{:{}.2f}]">(3.125, 12u) ==
			"[        3.12]" &&
		::fast_io::fmt::concatf_std<"%12.2f">(3.125) ==
			"        3.12" &&
		::fast_io::fmt::concatf_std<"%*.*f">(12, 2, 3.125) ==
			"        3.12" &&
		::fast_io::fmt::concatf_std<"%*.*f">(-12, 2, 3.125) ==
			"3.12        " &&
		::fast_io::fmt::concatf_std<"%#*.*f">(8, 0, 3.125) ==
			"      3." &&
		::fast_io::fmt::concatf_std<"%#.0f">(3.125) == "3." &&
		::fast_io::fmt::concatf_std<"%#.0e">(3.125) == "3.e+00" &&
		::fast_io::fmt::concatf_std<"%#.0f">(
			::std::numeric_limits<double>::infinity()) == "inf";
}

[[nodiscard]] bool five_character_wrapper_domains_are_correct()
{
	return ::fast_io::fmt::concatf_std<"%#8.0f">(3.125) == "      3." &&
		::fast_io::fmt::wconcatf_std<L"%#8.0f">(3.125) == L"      3." &&
		::fast_io::fmt::u8concatf_std<u8"%#8.0f">(3.125) == u8"      3." &&
		::fast_io::fmt::u16concatf_std<u"%#8.0f">(3.125) == u"      3." &&
		::fast_io::fmt::u32concatf_std<U"%#8.0f">(3.125) == U"      3.";
}

[[nodiscard]] bool width_stack_boundaries_are_correct()
{
	constexpr ::std::size_t single_budget{
		::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_run_stack_size<
				char, fixed_width_type>()};
	using suffix_type = decltype(::fast_io::mnp::chvw('x'));
	constexpr ::std::size_t multi_budget{
		::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_run_stack_size<
				char, fixed_width_type, suffix_type>()};
	static_assert(single_budget != 0u && single_budget <= 1024u);
	static_assert(multi_budget != 0u && multi_budget <= 512u);

	auto single_at_limit{
		::fast_io::mnp::right(fixed6(3.125), single_budget)};
	auto single_over_limit{
		::fast_io::mnp::right(fixed6(3.125), single_budget + 1u)};
	auto multi_over_limit{
		::fast_io::mnp::right(fixed6(3.125), multi_budget)};
	auto suffix{::fast_io::mnp::chvw('x')};
	if (::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_total_size<false, char>(
				single_at_limit) != single_budget ||
		::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_total_size<true, char>(
				single_at_limit) != SIZE_MAX ||
		::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_total_size<false, char>(
				single_over_limit) != SIZE_MAX ||
		::fast_io::details::decay::
			basic_general_concat_single_pass_bounded_total_size<false, char>(
				multi_over_limit, suffix) != SIZE_MAX)
	{
		return false;
	}

	auto const exact_result{::fast_io::concat_std(single_at_limit)};
	auto const line_fallback{::fast_io::concatln_std(single_at_limit)};
	auto const single_fallback{::fast_io::concat_std(single_over_limit)};
	auto const multi_fallback{
		::fast_io::concat_std(multi_over_limit, suffix)};
	return exact_result.size() == single_budget &&
		exact_result.ends_with("3.125000") &&
		line_fallback.size() == single_budget + 1u &&
		line_fallback.ends_with("3.125000\n") &&
		single_fallback.size() == single_budget + 1u &&
		single_fallback.ends_with("3.125000") &&
		multi_fallback.size() == multi_budget + 1u &&
		multi_fallback.ends_with("3.125000x");
}

[[nodiscard]] bool wrapper_short_circuit_and_custom_fallback_are_correct()
{
	unsigned width_candidate_calls{};
	auto width{::fast_io::mnp::right(
		observed_candidate_leaf{&width_candidate_calls}, SIZE_MAX)};
	auto const width_size{
		::fast_io::single_pass_bounded_materialization_size_invoke<char>(
			width, 1024u)};
	if (width_size != SIZE_MAX || width_candidate_calls != 0u)
	{
		return false;
	}

	unsigned force_candidate_calls{};
	auto force{::fast_io::manipulators::printf_force_radix_t{
		observed_candidate_leaf{&force_candidate_calls}, true}};
	auto const force_size{
		::fast_io::single_pass_bounded_materialization_size_invoke<char>(
			force, 0u)};
	if (force_size != SIZE_MAX || force_candidate_calls != 0u)
	{
		return false;
	}

	unsigned size_calls{};
	unsigned define_calls{};
	auto const custom_result{::fast_io::concat_std(::fast_io::mnp::right(
		stateful_dynamic_leaf{1u, &size_calls, &define_calls}, 4u, '_'))};
	return custom_result == "___x" && size_calls == 1u &&
		define_calls == 1u;
}

} // namespace concat_float_one_pass_test

int main()
{
	using namespace concat_float_one_pass_test;

	if (::fast_io::fmt::concat_std<"{:.6f}">(3.125) != "3.125000" ||
		::fast_io::concat_std(fixed6(-0.0)) != "-0.000000" ||
		::fast_io::concatln_std(fixed6(3.125)) != "3.125000\n" ||
		::fast_io::fmt::concat_std<"{:+.6f}\n">(3.125) != "+3.125000\n" ||
		::fast_io::fmt::concat_std<"id={},hex={:#x},f={:.6f},word={}">(
			::std::uint64_t{18446744073709551557ull},
			::std::uint64_t{0x1234abcdef987654ull}, 3.125,
			::std::string_view{"runtime"}) !=
			"id=18446744073709551557,hex=0x1234abcdef987654,f=3.125000,word=runtime" ||
		::fast_io::fmt::wconcat_std<L"{:.2f}">(3.125) != L"3.12" ||
		::fast_io::fmt::u8concat_std<u8"{:.2f}">(3.125) != u8"3.12" ||
		::fast_io::fmt::concat_std<"{:.6f}">(
			::std::numeric_limits<double>::infinity()) != "inf" ||
		!candidate_bound_boundary_is_correct<char>() ||
		!candidate_bound_boundary_is_correct<wchar_t>() ||
		!candidate_bound_boundary_is_correct<char8_t>() ||
		!candidate_bound_boundary_is_correct<char16_t>() ||
		!candidate_bound_boundary_is_correct<char32_t>())
	{
		::std::abort();
	}

	if (!extreme_precision_is_correct())
	{
		::std::abort();
	}

	if (!four_width_representations_are_correct() ||
		!brace_and_printf_wrappers_are_correct() ||
		!five_character_wrapper_domains_are_correct() ||
		!width_stack_boundaries_are_correct() ||
		!wrapper_short_circuit_and_custom_fallback_are_correct())
	{
		::std::abort();
	}

	// A large but bounded precision still fits concat's audited one-pass frame.
	auto const large_precision{
		::fast_io::concat_std(::fast_io::mnp::fixed<
			::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
			3.125, 600u))};
	if (large_precision.size() != 602u ||
		large_precision.compare(0u, 8u, "3.125000") != 0)
	{
		::std::abort();
	}

	// A result beyond the one-pass frame retains the established fallback.
	auto const oversized_precision{
		::fast_io::concat_std(::fast_io::mnp::fixed<
			::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
			3.125, 1400u))};
	if (oversized_precision.size() != 1402u ||
		oversized_precision.compare(0u, 8u, "3.125000") != 0)
	{
		::std::abort();
	}

	// An unrelated object-dependent size CPO prevents the tentative bounded
	// strategy. Even though its bound exceeds the stack budget, fallback must
	// observe its size and define protocols exactly once.
	unsigned size_calls{};
	unsigned define_calls{};
	auto const stateful_result{::fast_io::concat_std(
		fixed6(3.125),
		stateful_dynamic_leaf{1400u, &size_calls, &define_calls})};
	if (stateful_result != "3.125000x" || size_calls != 1u ||
		define_calls != 1u)
	{
		::std::abort();
	}

	// Formatting happens before construction and outside resize-and-overwrite;
	// a throwing static-bound companion therefore propagates normally.
	unsigned throwing_calls{};
	bool caught{};
	try
	{
		(void)::fast_io::concat_std(
			fixed6(3.125), throwing_static_leaf{&throwing_calls});
	}
	catch (::std::runtime_error const &)
	{
		caught = true;
	}
	if (!caught || throwing_calls != 1u)
	{
		::std::abort();
	}

	// Multi-leaf runs retain the compact frame policy. A record exceeding it
	// falls back without swallowing or replaying a companion's exception.
	unsigned large_throwing_calls{};
	bool large_caught{};
	try
	{
		(void)::fast_io::concat_std(
			::fast_io::mnp::fixed<
				::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
				3.125, 600u),
			throwing_static_leaf{&large_throwing_calls});
	}
	catch (::std::runtime_error const &)
	{
		large_caught = true;
	}
	if (!large_caught || large_throwing_calls != 1u)
	{
		::std::abort();
	}

	// Once an earlier component exhausts the optional frame, no later candidate
	// bound is observed. The fallback strategy remains the only object consumer.
	unsigned candidate_calls{};
	oversized_static_leaf oversized{};
	observed_candidate_leaf observed{&candidate_calls};
	auto const rejected_total{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_total_size<
			false, char>(oversized, observed)};
	if (rejected_total != SIZE_MAX || candidate_calls != 0u)
	{
		::std::abort();
	}
}
