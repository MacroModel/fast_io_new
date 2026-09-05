#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <tuple>
#include <utility>

#include <fast_io_format.h>

namespace range_element_decay_boundary
{

struct contract_state
{
	void const *expected_identity{};
	::std::size_t fallback_calls{};
	::std::size_t direct_calls{};
	::std::size_t print_calls{};
	::std::size_t move_calls{};
	bool identity_preserved{true};
};

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct noncopyable_proxy
{
	contract_state *state{};
	char value{};

	inline constexpr noncopyable_proxy(contract_state *audit, char character) noexcept
		: state(audit), value(character)
	{}

	noncopyable_proxy(noncopyable_proxy const &) = delete;
	noncopyable_proxy &operator=(noncopyable_proxy const &) = delete;

	inline noncopyable_proxy(noncopyable_proxy &&other) noexcept
		: state(other.state), value(other.value)
	{
		++state->move_calls;
	}

	noncopyable_proxy &operator=(noncopyable_proxy &&) = delete;
};

[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, noncopyable_proxy>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, noncopyable_proxy>, char *output,
	noncopyable_proxy &value) noexcept
{
	++value.state->print_calls;
	value.state->identity_preserved =
		value.state->identity_preserved &&
		value.state->expected_identity == static_cast<void const *>(&value);
	*output++ = value.value;
	return output;
}

struct fallback_element
{
	noncopyable_proxy proxy;

	inline constexpr fallback_element(contract_state *state, char value) noexcept
		: proxy(state, value)
	{}
};

struct fallback_parse_state
{
	constexpr bool operator==(fallback_parse_state const &) const noexcept = default;
};

template <typename context_type>
[[nodiscard]] consteval fallback_parse_state format_parse_define(
	::fast_io::io_type_t<fallback_element>, context_type) noexcept
{
	return {};
}

template <typename char_type, auto parse_state>
[[nodiscard]] inline noncopyable_proxy &format_alias_define(
	::fast_io::fmt::basic_custom_format_state_t<char_type, parse_state>,
	fallback_element &value) noexcept
{
	static_assert(parse_state == fallback_parse_state{});
	++value.proxy.state->fallback_calls;
	value.proxy.state->expected_identity =
		static_cast<void const *>(&value.proxy);
	return value.proxy;
}

struct direct_element
{
	noncopyable_proxy proxy;

	inline constexpr direct_element(contract_state *state, char value) noexcept
		: proxy(state, value)
	{}
};

template <typename format_tag>
[[nodiscard]] inline noncopyable_proxy &brace_range_format_element(
	format_tag, direct_element &value) noexcept
{
	++value.proxy.state->direct_calls;
	value.proxy.state->expected_identity =
		static_cast<void const *>(&value.proxy);
	return value.proxy;
}

template <typename format_tag>
[[nodiscard]] inline noncopyable_proxy &&brace_range_format_element(
	format_tag, direct_element &&value) noexcept
{
	++value.proxy.state->direct_calls;
	value.proxy.state->expected_identity =
		static_cast<void const *>(&value.proxy);
	return static_cast<noncopyable_proxy &&>(value.proxy);
}

using empty_argument_pack = ::std::tuple<>;
inline constexpr auto default_specification{
	::fast_io::fmt::details::default_range_format_specification<char>};

// Both range helpers are phase-two bridges. These type identities make an
// accidental return-type decay a compile-time failure before runtime coverage.
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_default_brace_range_element<char>(
		::std::declval<fallback_element &>(),
		::std::declval<empty_argument_pack &>())),
	noncopyable_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_brace_range_element<
		char, default_specification>(
		::std::declval<fallback_element &>(),
		::std::declval<empty_argument_pack &>())),
	noncopyable_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_default_brace_range_element<char>(
		::std::declval<direct_element &>(),
		::std::declval<empty_argument_pack &>())),
	noncopyable_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_default_brace_range_element<char>(
		::std::declval<direct_element &&>(),
		::std::declval<empty_argument_pack &>())),
	noncopyable_proxy &&>);
static_assert(::std::same_as<
	decltype(::fast_io::fmt::details::make_brace_range_element<
		char, default_specification>(
		::std::declval<direct_element &&>(),
		::std::declval<empty_argument_pack &>())),
	noncopyable_proxy &&>);

inline void reset(contract_state &state) noexcept
{
	state = {};
}

inline void test_fallback_rule_reference() noexcept
{
	contract_state state{};
	fallback_element values[]{{&state, 'A'}, {&state, 'B'}};
	auto const result{::fast_io::fmt::concat_std<"{}">(values)};
	require(result == "[A, B]");
	require(state.fallback_calls == 2u);
	require(state.print_calls == 2u);
	require(state.move_calls == 0u);
	require(state.identity_preserved);
}

inline void test_direct_rule_reference() noexcept
{
	contract_state state{};
	direct_element values[]{{&state, 'C'}, {&state, 'D'}};
	auto const result{::fast_io::fmt::concat_std<"{}">(values)};
	require(result == "[C, D]");
	require(state.direct_calls == 2u);
	require(state.print_calls == 2u);
	require(state.move_calls == 0u);
	require(state.identity_preserved);

	// A named owner remains alive while its xvalue subobject is synchronously
	// forwarded. The bridge must expose that same subobject and must not perform
	// the move which belongs to the downstream decay policy.
	reset(state);
	direct_element owner{&state, 'R'};
	empty_argument_pack arguments{};
	decltype(auto) xvalue{
		::fast_io::fmt::details::make_brace_range_element<
			char, default_specification>(::std::move(owner), arguments)};
	require(static_cast<void const *>(&xvalue) ==
			static_cast<void const *>(&owner.proxy));
	require(state.direct_calls == 1u);
	require(state.move_calls == 0u);
}

} // namespace range_element_decay_boundary

int main()
{
	range_element_decay_boundary::test_fallback_rule_reference();
	range_element_decay_boundary::test_direct_rule_reference();
}
