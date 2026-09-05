/*
This translation unit verifies the two format extension boundaries which can
execute user code. Ordinary compilers retain the historical conditional-
noexcept API, while Herbception builds use a conditional deterministic-error
specification and preserve the complete success type, including reference
identity.
*/

#define FAST_IO_HERBCEPTIONS_THROWS 811
#define FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(...) 812
#define FAST_IO_HERBCEPTIONS_NOTHROWS(...) 813
#define FAST_IO_HERBCEPTIONS_NOEXCEPT(...) 814

#include <fast_io_format/details/custom.h>
#include <fast_io_format/details/rule_protocol.h>

static_assert(FAST_IO_HERBCEPTIONS_THROWS == 811);
static_assert(FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(0) == 812);
static_assert(FAST_IO_HERBCEPTIONS_NOTHROWS(0) == 813);
static_assert(FAST_IO_HERBCEPTIONS_NOEXCEPT(0) == 814);

#undef FAST_IO_HERBCEPTIONS_THROWS
#undef FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT
#undef FAST_IO_HERBCEPTIONS_NOTHROWS
#undef FAST_IO_HERBCEPTIONS_NOEXCEPT

namespace herbceptions_custom_rule_effect_contract
{

#if defined(__HERBCEPTIONS__)
#define FAST_IO_TEST_DETERMINISTIC_EFFECT throws
#else
#define FAST_IO_TEST_DETERMINISTIC_EFFECT noexcept(false)
#endif

struct printable_value
{
	char value{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, printable_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, printable_value>, char *iter,
	printable_value value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

struct custom_state
{
	char marker{};

	constexpr bool operator==(custom_state const &) const noexcept = default;
};

struct safe_custom_source
{};
struct fallible_custom_source
{};
struct safe_reference_custom_source
{};
struct safe_xvalue_reference_custom_source
{};
struct fallible_reference_custom_source
{};
struct fallible_xvalue_reference_custom_source
{};

template <typename value_type, auto format_literal, auto source>
		requires(::std::same_as<value_type, safe_custom_source> ||
				 ::std::same_as<value_type, fallible_custom_source> ||
				 ::std::same_as<value_type, safe_reference_custom_source> ||
				 ::std::same_as<value_type, safe_xvalue_reference_custom_source> ||
				 ::std::same_as<value_type, fallible_reference_custom_source> ||
				 ::std::same_as<value_type, fallible_xvalue_reference_custom_source>)
[[nodiscard]] consteval custom_state format_parse_define(
	::fast_io::io_type_t<value_type>,
	::fast_io::fmt::basic_custom_format_parse_context<
		format_literal, source>) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {'!'};
}

template <typename char_type, auto state>
[[nodiscard]] inline constexpr printable_value format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	safe_custom_source &) noexcept
{
	static_assert(state.marker == '!');
	return {'s'};
}

template <typename char_type, auto state>
[[nodiscard]] inline printable_value format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	fallible_custom_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static_assert(state.marker == '!');
	return {'f'};
}

template <typename char_type, auto state>
[[nodiscard]] inline printable_value &format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	safe_reference_custom_source &) noexcept
{
	static printable_value result{'l'};
	return result;
}

template <typename char_type, auto state>
[[nodiscard]] inline printable_value &&format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	safe_xvalue_reference_custom_source &) noexcept
{
	static printable_value result{'x'};
	return static_cast<printable_value &&>(result);
}

template <typename char_type, auto state>
[[nodiscard]] inline printable_value &format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	fallible_reference_custom_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'r'};
	return result;
}

template <typename char_type, auto state>
[[nodiscard]] inline printable_value &&format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, state>,
	fallible_xvalue_reference_custom_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'y'};
	return static_cast<printable_value &&>(result);
}

struct safe_as_source
{};
struct fallible_as_source
{};
struct safe_reference_as_source
{};
struct safe_xvalue_reference_as_source
{};
struct fallible_reference_as_source
{};
struct fallible_xvalue_reference_as_source
{};

[[nodiscard]] inline constexpr printable_value format_as(
	safe_as_source &) noexcept
{
	return {'a'};
}

[[nodiscard]] inline printable_value format_as(
	fallible_as_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {'b'};
}

[[nodiscard]] inline printable_value &format_as(
	safe_reference_as_source &) noexcept
{
	static printable_value result{'d'};
	return result;
}

[[nodiscard]] inline printable_value &&format_as(
	safe_xvalue_reference_as_source &) noexcept
{
	static printable_value result{'e'};
	return static_cast<printable_value &&>(result);
}

[[nodiscard]] inline printable_value &format_as(
	fallible_reference_as_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'c'};
	return result;
}

[[nodiscard]] inline printable_value &&format_as(
	fallible_xvalue_reference_as_source &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'g'};
	return static_cast<printable_value &&>(result);
}

inline constexpr ::fast_io::fmt::basic_fixed_string test_literal{""};
inline constexpr ::fast_io::fmt::details::source_slice test_source{};

template <typename value_type>
using custom_state_tag =
	::fast_io::fmt::details::custom_format_state_tag<
		test_literal, test_source, value_type>;

template <typename value_type>
concept custom_alias_callable = requires(value_type &value) {
	::fast_io::fmt::details::custom_format_adl::alias<
		custom_state_tag<value_type &>>(value);
};

template <typename value_type>
concept format_as_callable = requires(value_type &value) {
	::fast_io::fmt::details::custom_format_adl::as(value);
};

template <typename value_type>
concept complete_custom_callable = requires(value_type &value) {
	::fast_io::fmt::details::make_custom_format_value<
		test_literal, test_source>(value);
};

template <typename value_type>
concept complete_format_as_callable = requires(value_type &value) {
	::fast_io::fmt::details::make_format_as_value<char>(value);
};

// The deterministic-error channel is orthogonal to the success payload. Both
// plain and fallible CPOs must preserve lvalue and xvalue references through
// the low-level adapter and through the complete format-lowering front door.
static_assert(custom_alias_callable<safe_reference_custom_source>);
static_assert(custom_alias_callable<safe_xvalue_reference_custom_source>);
static_assert(custom_alias_callable<fallible_reference_custom_source>);
static_assert(custom_alias_callable<fallible_xvalue_reference_custom_source>);
static_assert(complete_custom_callable<safe_reference_custom_source>);
static_assert(complete_custom_callable<safe_xvalue_reference_custom_source>);
static_assert(complete_custom_callable<fallible_reference_custom_source>);
static_assert(complete_custom_callable<fallible_xvalue_reference_custom_source>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::alias<
		custom_state_tag<safe_reference_custom_source &>>(
		::std::declval<safe_reference_custom_source &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::alias<
		custom_state_tag<safe_xvalue_reference_custom_source &>>(
		::std::declval<safe_xvalue_reference_custom_source &>())),
	printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::alias<
		custom_state_tag<fallible_reference_custom_source &>>(
		::std::declval<fallible_reference_custom_source &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::alias<
		custom_state_tag<fallible_xvalue_reference_custom_source &>>(
		::std::declval<fallible_xvalue_reference_custom_source &>())),
	printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_custom_format_value<
		test_literal, test_source>(
		::std::declval<safe_reference_custom_source &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_custom_format_value<
		test_literal, test_source>(
		::std::declval<safe_xvalue_reference_custom_source &>())),
	printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_custom_format_value<
		test_literal, test_source>(
		::std::declval<fallible_reference_custom_source &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_custom_format_value<
		test_literal, test_source>(
		::std::declval<fallible_xvalue_reference_custom_source &>())),
	printable_value &&>);

static_assert(format_as_callable<safe_reference_as_source>);
static_assert(format_as_callable<safe_xvalue_reference_as_source>);
static_assert(format_as_callable<fallible_reference_as_source>);
static_assert(format_as_callable<fallible_xvalue_reference_as_source>);
static_assert(complete_format_as_callable<safe_reference_as_source>);
static_assert(complete_format_as_callable<safe_xvalue_reference_as_source>);
static_assert(complete_format_as_callable<fallible_reference_as_source>);
static_assert(complete_format_as_callable<fallible_xvalue_reference_as_source>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::as(
		::std::declval<safe_reference_as_source &>())), printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::as(
		::std::declval<safe_xvalue_reference_as_source &>())), printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::as(
		::std::declval<fallible_reference_as_source &>())), printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::custom_format_adl::as(
		::std::declval<fallible_xvalue_reference_as_source &>())), printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_format_as_value<char>(
		::std::declval<safe_reference_as_source &>())), printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_format_as_value<char>(
		::std::declval<safe_xvalue_reference_as_source &>())), printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_format_as_value<char>(
		::std::declval<fallible_reference_as_source &>())), printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_format_as_value<char>(
		::std::declval<fallible_xvalue_reference_as_source &>())), printable_value &&>);

#if defined(__HERBCEPTIONS__)
static_assert(!throws((::fast_io::fmt::details::custom_format_adl::alias<
					   custom_state_tag<safe_custom_source &>>(
	::std::declval<safe_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::alias<
					  custom_state_tag<fallible_custom_source &>>(
	::std::declval<fallible_custom_source &>()))));
static_assert(!throws((::fast_io::fmt::details::custom_format_adl::alias<
	custom_state_tag<safe_reference_custom_source &>>(
	::std::declval<safe_reference_custom_source &>()))));
static_assert(!throws((::fast_io::fmt::details::custom_format_adl::alias<
	custom_state_tag<safe_xvalue_reference_custom_source &>>(
	::std::declval<safe_xvalue_reference_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::alias<
	custom_state_tag<fallible_reference_custom_source &>>(
	::std::declval<fallible_reference_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::alias<
	custom_state_tag<fallible_xvalue_reference_custom_source &>>(
	::std::declval<fallible_xvalue_reference_custom_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_custom_format_value<
					   test_literal, test_source>(::std::declval<safe_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_custom_format_value<
					  test_literal, test_source>(
	::std::declval<fallible_custom_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_custom_format_value<
					   test_literal, test_source>(
	::std::declval<safe_reference_custom_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_custom_format_value<
					   test_literal, test_source>(
	::std::declval<safe_xvalue_reference_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_custom_format_value<
					  test_literal, test_source>(
	::std::declval<fallible_reference_custom_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_custom_format_value<
					  test_literal, test_source>(
	::std::declval<fallible_xvalue_reference_custom_source &>()))));

static_assert(!throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<safe_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<fallible_as_source &>()))));
static_assert(!throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<safe_reference_as_source &>()))));
static_assert(!throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<safe_xvalue_reference_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<fallible_reference_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<fallible_xvalue_reference_as_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<safe_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<fallible_as_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<safe_reference_as_source &>()))));
static_assert(!throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<safe_xvalue_reference_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<fallible_reference_as_source &>()))));
static_assert(throws((::fast_io::fmt::details::make_format_as_value<char>(
	::std::declval<fallible_xvalue_reference_as_source &>()))));
using safe_alias_function =
	printable_value (*)(safe_custom_source &) throws(false);
using fallible_alias_function =
	printable_value (*)(fallible_custom_source &) throws(true);
using safe_as_function = printable_value (*)(safe_as_source &) throws(false);
using fallible_as_function = printable_value (*)(fallible_as_source &) throws;
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::custom_format_adl::alias<
					   custom_state_tag<safe_custom_source &>, safe_custom_source &>),
			  safe_alias_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::custom_format_adl::alias<
					   custom_state_tag<fallible_custom_source &>,
					   fallible_custom_source &>),
			  fallible_alias_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::custom_format_adl::as<
					   safe_as_source &>),
			  safe_as_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::custom_format_adl::as<
					   fallible_as_source &>),
			  fallible_as_function>);
#else
static_assert(noexcept(::fast_io::fmt::details::custom_format_adl::alias<
					   custom_state_tag<safe_custom_source &>>(
	::std::declval<safe_custom_source &>())));
static_assert(!noexcept(::fast_io::fmt::details::custom_format_adl::alias<
						custom_state_tag<fallible_custom_source &>>(
	::std::declval<fallible_custom_source &>())));
static_assert(custom_alias_callable<fallible_reference_custom_source>);
static_assert(noexcept(::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<safe_as_source &>())));
static_assert(!noexcept(::fast_io::fmt::details::custom_format_adl::as(
	::std::declval<fallible_as_source &>())));
static_assert(format_as_callable<fallible_reference_as_source>);
#endif

struct rule_value
{};
struct argument_pack
{};
struct safe_grammar
{};
struct fallible_grammar
{};
struct safe_reference_grammar
{};
struct safe_xvalue_reference_grammar
{};
struct fallible_reference_grammar
{};
struct fallible_xvalue_reference_grammar
{};
struct effectful_selector_grammar
{};
struct absent_selector_grammar
{};
#if defined(__HERBCEPTIONS__)
struct failing_selector_grammar
{};
#endif

struct safe_rule
{};
struct fallible_rule
{};
struct safe_reference_rule
{};
struct safe_xvalue_reference_rule
{};
struct fallible_reference_rule
{};
struct fallible_xvalue_reference_rule
{};
struct effectful_selector_rule
{};
#if defined(__HERBCEPTIONS__)
struct failing_selector_rule
{};
#endif

struct effectful_constructor_rule
{
	effectful_constructor_rule() FAST_IO_TEST_DETERMINISTIC_EFFECT
	{}
};

static_assert(!::fast_io::fmt::details::format_replacement_rule_token<
			  effectful_constructor_rule>);

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	safe_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<safe_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	fallible_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<fallible_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	safe_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<safe_reference_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	safe_xvalue_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<safe_xvalue_reference_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	fallible_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<fallible_reference_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	fallible_xvalue_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) noexcept
{
	return ::fast_io::io_type_t<fallible_xvalue_reference_rule>{};
}

template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	effectful_selector_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return ::fast_io::io_type_t<effectful_selector_rule>{};
}

#if defined(__HERBCEPTIONS__)
template <auto format_literal, auto field>
[[nodiscard]] consteval auto format_replacement_rule_type(
	failing_selector_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<rule_value &>) throws
{
	throw throws ::std::errc::invalid_argument;
	return ::fast_io::io_type_t<failing_selector_rule>{};
}
#endif

template <auto format_literal, auto field>
[[nodiscard]] inline constexpr printable_value format_replacement_rule_define(
	::fast_io::io_type_t<safe_rule>, safe_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) noexcept
{
	return {'1'};
}

template <auto format_literal, auto field>
[[nodiscard]] inline printable_value format_replacement_rule_define(
	::fast_io::io_type_t<fallible_rule>, fallible_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {'2'};
}

template <auto format_literal, auto field>
[[nodiscard]] inline printable_value &format_replacement_rule_define(
	::fast_io::io_type_t<safe_reference_rule>,
	safe_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) noexcept
{
	static printable_value result{'5'};
	return result;
}

template <auto format_literal, auto field>
[[nodiscard]] inline printable_value &&format_replacement_rule_define(
	::fast_io::io_type_t<safe_xvalue_reference_rule>,
	safe_xvalue_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) noexcept
{
	static printable_value result{'6'};
	return static_cast<printable_value &&>(result);
}

template <auto format_literal, auto field>
[[nodiscard]] inline printable_value &format_replacement_rule_define(
	::fast_io::io_type_t<fallible_reference_rule>,
	fallible_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'3'};
	return result;
}

template <auto format_literal, auto field>
[[nodiscard]] inline printable_value &&format_replacement_rule_define(
	::fast_io::io_type_t<fallible_xvalue_reference_rule>,
	fallible_xvalue_reference_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	static printable_value result{'7'};
	return static_cast<printable_value &&>(result);
}

template <auto format_literal, auto field>
[[nodiscard]] inline constexpr printable_value format_replacement_rule_define(
	::fast_io::io_type_t<effectful_selector_rule>,
	effectful_selector_grammar,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	rule_value &, argument_pack &) noexcept
{
	return {'4'};
}

template <typename grammar_type>
concept rule_invoke_callable = requires(rule_value &value,
										argument_pack &arguments) {
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		grammar_type, test_literal, 0>(value, arguments);
};

static_assert(rule_invoke_callable<safe_grammar>);
static_assert(rule_invoke_callable<safe_reference_grammar>);
static_assert(rule_invoke_callable<safe_xvalue_reference_grammar>);
static_assert(rule_invoke_callable<fallible_reference_grammar>);
static_assert(rule_invoke_callable<fallible_xvalue_reference_grammar>);
static_assert(::fast_io::fmt::details::format_replacement_rule_for<
	safe_reference_grammar, test_literal, 0, rule_value &, argument_pack>);
static_assert(::fast_io::fmt::details::format_replacement_rule_for<
	safe_xvalue_reference_grammar, test_literal, 0, rule_value &, argument_pack>);
static_assert(::fast_io::fmt::details::format_replacement_rule_for<
	fallible_reference_grammar, test_literal, 0, rule_value &, argument_pack>);
static_assert(::fast_io::fmt::details::format_replacement_rule_for<
	fallible_xvalue_reference_grammar, test_literal, 0, rule_value &, argument_pack>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(), ::std::declval<argument_pack &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_xvalue_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(), ::std::declval<argument_pack &>())),
	printable_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(), ::std::declval<argument_pack &>())),
	printable_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_xvalue_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(), ::std::declval<argument_pack &>())),
	printable_value &&>);
static_assert(!::fast_io::fmt::details::format_replacement_rule_type_adl::expression<
			  absent_selector_grammar, test_literal, 0, rule_value &>);
static_assert(!::fast_io::fmt::details::format_replacement_rule_for<
			  absent_selector_grammar, test_literal, 0, rule_value &, argument_pack>);

#if defined(__HERBCEPTIONS__)
static_assert(!::fast_io::fmt::details::format_replacement_rule_type_adl::expression<
			  failing_selector_grammar, test_literal, 0, rule_value &>);
static_assert(rule_invoke_callable<fallible_grammar>);
static_assert(rule_invoke_callable<effectful_selector_grammar>);
static_assert(!throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
static_assert(throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
static_assert(!throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
static_assert(!throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_xvalue_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
static_assert(throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
static_assert(throws((
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_xvalue_reference_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>()))));
using safe_rule_invoke_function =
	printable_value (*)(rule_value &, argument_pack &) throws(false);
using fallible_rule_invoke_function =
	printable_value (*)(rule_value &, argument_pack &) throws(true);
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::format_replacement_rule_adl::invoke<
					   safe_grammar, test_literal, 0, rule_value &, argument_pack>),
			  safe_rule_invoke_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::fmt::details::format_replacement_rule_adl::invoke<
					   fallible_grammar, test_literal, 0, rule_value &, argument_pack>),
			  fallible_rule_invoke_function>);
#else
static_assert(rule_invoke_callable<fallible_grammar>);
static_assert(rule_invoke_callable<fallible_reference_grammar>);
static_assert(!rule_invoke_callable<effectful_selector_grammar>);
static_assert(noexcept(
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		safe_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>())));
static_assert(!noexcept(
	::fast_io::fmt::details::format_replacement_rule_adl::invoke<
		fallible_grammar, test_literal, 0>(
		::std::declval<rule_value &>(),
		::std::declval<argument_pack &>())));
#endif

#undef FAST_IO_TEST_DETERMINISTIC_EFFECT

} // namespace herbceptions_custom_rule_effect_contract

int main()
{
	return 0;
}
