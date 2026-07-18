#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct incomplete_leaf;

template <typename T>
concept can_use_print_alias = requires {
	::fast_io::io_print_alias(::std::declval<T>());
};

template <typename T>
concept can_use_print_transport = requires {
	::fast_io::details::io_print_forward_transport(::std::declval<T>());
};

template <typename T>
concept can_use_character_print_transport = requires {
	::fast_io::details::io_print_forward_transport_for<char>(::std::declval<T>());
};

template <typename T>
concept can_use_print_forward = requires {
	::fast_io::io_print_forward<char>(::std::declval<T>());
};

template <typename T>
concept can_alias_then_print_forward = requires {
	::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(::std::declval<T>()));
};

template <typename T>
concept can_make_pack = requires {
	::fast_io::mnp::pack(::std::declval<T>());
};

template <typename T>
concept can_make_condition = requires {
	::fast_io::mnp::cond(true, ::std::declval<T>());
};

template <typename T>
concept can_make_width = requires {
	::fast_io::mnp::left(::std::declval<T>(), 4u);
};

// A forward-declared class remains an object type, but the standard permits triviality traits to require a complete
// class. These assertions are intentionally formed before `incomplete_leaf` is defined: the three value policies must
// reject value transport without diagnosing, while factories receiving an lvalue must retain the exact cv/ref type.
static_assert(!::fast_io::details::io_print_complete_object<incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_transport_by_value<incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_transport_by_value<incomplete_leaf &>);
static_assert(!::fast_io::details::io_print_forward_transport_by_value<incomplete_leaf const &>);
static_assert(!::fast_io::details::pack_value_transferable<incomplete_leaf volatile &>);
static_assert(!::fast_io::details::cond_value_transferable<incomplete_leaf const volatile &>);
static_assert(!::fast_io::details::io_print_forward_transportable<incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_transportable_for<char, incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_transport_nothrow<incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_transport_nothrow_for<
	char, incomplete_leaf>);
static_assert(!::fast_io::details::io_print_alias_nothrow<incomplete_leaf>);
static_assert(!::fast_io::details::io_print_forward_nothrow<char, incomplete_leaf>);
static_assert(!can_use_print_alias<incomplete_leaf>);
static_assert(!can_use_print_transport<incomplete_leaf>);
static_assert(!can_use_character_print_transport<incomplete_leaf>);
static_assert(!can_use_print_forward<incomplete_leaf>);
static_assert(!can_make_pack<incomplete_leaf>);
static_assert(!can_make_condition<incomplete_leaf>);
static_assert(!can_make_width<incomplete_leaf>);

static_assert(::std::same_as<
	::fast_io::details::pack_alias_type<incomplete_leaf &>, incomplete_leaf &>);
static_assert(::std::same_as<
	::fast_io::details::cond_alias_type<incomplete_leaf const &>, incomplete_leaf const &>);
static_assert(::std::same_as<
	::fast_io::details::width_storage_type<incomplete_leaf const volatile &>,
	incomplete_leaf const volatile &>);
static_assert(::fast_io::details::pack_alias_storable<incomplete_leaf &>);
static_assert(::fast_io::details::cond_alias_storable<incomplete_leaf const &>);
static_assert(::fast_io::details::width_storable<incomplete_leaf const volatile &>);

using incomplete_pack = decltype(
	::fast_io::mnp::pack(::std::declval<incomplete_leaf &>()));
using incomplete_condition = decltype(
	::fast_io::mnp::cond(true, ::std::declval<incomplete_leaf const &>()));
using incomplete_width = decltype(
	::fast_io::mnp::left(::std::declval<incomplete_leaf const volatile &>(), 4u));

static_assert(::std::same_as<
	decltype(::std::declval<incomplete_condition &>().t1), incomplete_leaf const &>);
static_assert(::std::same_as<
	decltype(::std::declval<incomplete_width &>().reference),
	incomplete_leaf const volatile &>);
static_assert(::std::same_as<
	decltype(::fast_io::details::io_print_forward_transport(
		::std::declval<incomplete_leaf &>())),
	::fast_io::parameter<incomplete_leaf &>>);
static_assert(sizeof(incomplete_pack) != 0u);

struct incomplete_alias_lvalue_source
{};

struct incomplete_alias_rvalue_source
{};

struct incomplete_status_lvalue_source
{};

struct incomplete_status_rvalue_source
{};

// Only unevaluated protocol probes use these definitions. `__builtin_unreachable` avoids manufacturing storage for a
// deliberately incomplete class while keeping strict builds free of internal-linkage "declared but undefined" noise.
inline incomplete_leaf &print_alias_define(
	::fast_io::io_alias_t, incomplete_alias_lvalue_source &) noexcept
{
	__builtin_unreachable();
}

inline incomplete_leaf &&print_alias_define(
	::fast_io::io_alias_t, incomplete_alias_rvalue_source &&) noexcept
{
	__builtin_unreachable();
}

inline incomplete_leaf &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, incomplete_status_lvalue_source &) noexcept
{
	__builtin_unreachable();
}

inline incomplete_leaf &&status_io_print_forward(
	::fast_io::io_alias_type_t<char>, incomplete_status_rvalue_source &&) noexcept
{
	__builtin_unreachable();
}

// An incomplete lvalue result has an existing lifetime and uses one exact-reference wrapper. An incomplete xvalue
// would require new owned storage and must be rejected before a constructibility trait receives the incomplete class.
static_assert(::fast_io::alias_printable<incomplete_alias_lvalue_source &>);
static_assert(!::fast_io::alias_printable<incomplete_alias_rvalue_source>);
static_assert(::fast_io::status_io_print_forwardable<
	char, incomplete_status_lvalue_source &>);
static_assert(!::fast_io::status_io_print_forwardable<
	char, incomplete_status_rvalue_source>);
static_assert(::std::same_as<
	decltype(::fast_io::io_print_forward<char>(
		::std::declval<incomplete_status_lvalue_source &>())),
	::fast_io::parameter<incomplete_leaf &>>);

struct immovable_print_result
{
	inline constexpr immovable_print_result() noexcept = default;
	immovable_print_result(immovable_print_result const &) = delete;
	immovable_print_result &operator=(immovable_print_result const &) = delete;
	immovable_print_result(immovable_print_result &&) = delete;
	immovable_print_result &operator=(immovable_print_result &&) = delete;
};

struct immovable_alias_result_source
{};

struct immovable_status_result_source
{};

inline constexpr immovable_print_result print_alias_define(
	::fast_io::io_alias_t, immovable_alias_result_source &&) noexcept
{
	return {};
}

inline constexpr immovable_print_result status_io_print_forward(
	::fast_io::io_alias_type_t<char>, immovable_status_result_source &&) noexcept
{
	return {};
}

// Returning the prvalue itself uses guaranteed copy elision inside the CPO. The subsequent forwarding helper receives
// a named object and would need a move, so both protocols must reject this result during concept substitution.
static_assert(!::fast_io::alias_printable<immovable_alias_result_source>);
static_assert(!::fast_io::status_io_print_forwardable<
	char, immovable_status_result_source>);

struct copy_only_small_result
{
	int value{};

	inline explicit constexpr copy_only_small_result(int input) noexcept : value(input) {}
	inline constexpr copy_only_small_result(copy_only_small_result const &) noexcept = default;
	inline constexpr copy_only_small_result &operator=(copy_only_small_result const &) noexcept = default;
	copy_only_small_result(copy_only_small_result &&) = delete;
	copy_only_small_result &operator=(copy_only_small_result &&) = delete;
};

struct copy_only_alias_result_source
{};

struct copy_only_status_result_source
{};

inline constexpr copy_only_small_result print_alias_define(
	::fast_io::io_alias_t, copy_only_alias_result_source &&) noexcept
{
	return copy_only_small_result{17};
}

inline constexpr copy_only_small_result status_io_print_forward(
	::fast_io::io_alias_type_t<char>, copy_only_status_result_source &&) noexcept
{
	return copy_only_small_result{23};
}

// The ABI-small path intentionally copies from the forwarding helper's named lvalue. A deleted move therefore does
// not invalidate this protocol when that exact copy is trivial and available; this distinguishes the branch proof
// from an unnecessarily strict blanket `constructible_from<result, result&&>` requirement.
static_assert(::std::is_trivially_copyable_v<copy_only_small_result>);
static_assert(::std::is_trivially_copy_constructible_v<copy_only_small_result>);
static_assert(::std::is_trivially_destructible_v<copy_only_small_result>);
static_assert(!::std::is_trivially_move_constructible_v<copy_only_small_result>);
static_assert(::std::constructible_from<copy_only_small_result, copy_only_small_result &>);
static_assert(!::std::constructible_from<copy_only_small_result, copy_only_small_result>);
static_assert(::fast_io::details::print_forward_result_copied_from_named_value<
	copy_only_small_result>());
static_assert(!::fast_io::details::print_forward_result_copied_from_named_value<
	copy_only_small_result &>());
static_assert(::fast_io::alias_printable<copy_only_alias_result_source>);
static_assert(::fast_io::status_io_print_forwardable<
	char, copy_only_status_result_source>);

struct observable_move_small_result
{
	int value{};

	inline constexpr observable_move_small_result() noexcept = default;
	inline constexpr observable_move_small_result(observable_move_small_result const &) noexcept = default;
	inline constexpr observable_move_small_result(observable_move_small_result &&other) noexcept
		: value(other.value)
	{
		other.value = -1;
	}
};

// A non-trivial available move is an observable operation, even when copying would be trivial. The named-copy rescue
// is specifically for an unavailable move and must not silently replace this constructor with a copy.
static_assert(::std::is_trivially_copy_constructible_v<observable_move_small_result>);
static_assert(::std::is_trivially_destructible_v<observable_move_small_result>);
static_assert(::std::is_move_constructible_v<observable_move_small_result>);
static_assert(!::std::is_trivially_move_constructible_v<observable_move_small_result>);
static_assert(!::fast_io::details::print_forward_result_copied_from_named_value<
	observable_move_small_result>());

struct copy_only_borrowed_result
{
	char value{'B'};

	inline constexpr copy_only_borrowed_result() noexcept = default;
	inline constexpr copy_only_borrowed_result(copy_only_borrowed_result const &) noexcept = default;
	inline constexpr copy_only_borrowed_result &operator=(copy_only_borrowed_result const &) noexcept = default;
	copy_only_borrowed_result(copy_only_borrowed_result &&) = delete;
	copy_only_borrowed_result &operator=(copy_only_borrowed_result &&) = delete;
};

struct copy_only_borrowed_alias_source
{};

struct copy_only_borrowed_status_source
{};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, copy_only_borrowed_result>,
	copy_only_borrowed_result &result) noexcept
{
	return {__builtin_addressof(result.value), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, copy_only_borrowed_result>) noexcept
{
	return {};
}

inline constexpr copy_only_borrowed_result print_alias_define(
	::fast_io::io_alias_t, copy_only_borrowed_alias_source &&) noexcept
{
	return {};
}

inline constexpr copy_only_borrowed_result status_io_print_forward(
	::fast_io::io_alias_type_t<char>, copy_only_borrowed_status_source &&) noexcept
{
	return {};
}

// Here the borrowed descriptor can point into the normalized object, so the character-aware identity rule vetoes the
// otherwise valid small copy. With no move constructor, status recognition and the alias-then-forward composition are
// both cleanly rejected. Alias shape recognition itself remains character-independent and is rechecked downstream.
static_assert(::fast_io::alias_printable<copy_only_borrowed_alias_source>);
static_assert(!::fast_io::status_io_print_forwardable<
	char, copy_only_borrowed_status_source>);
static_assert(!can_alias_then_print_forward<copy_only_borrowed_alias_source>);

struct explicit_copy_proxy
{
	char value{};
	unsigned *conversion_count{};

	inline constexpr explicit_copy_proxy(char ch, unsigned &count) noexcept
		: value(ch), conversion_count(__builtin_addressof(count))
	{}

	// This constructor is intentionally explicit. A `static_cast` can select it, but aggregate element
	// copy-initialization cannot. The regression therefore distinguishes the old factory body from `cond_store`.
	inline explicit explicit_copy_proxy(explicit_copy_proxy const &other) noexcept
		: value(other.value), conversion_count(other.conversion_count)
	{
		++*conversion_count;
	}

	explicit_copy_proxy &operator=(explicit_copy_proxy const &) = delete;
	inline constexpr explicit_copy_proxy(explicit_copy_proxy &&) noexcept = default;
	inline constexpr explicit_copy_proxy &operator=(explicit_copy_proxy &&) noexcept = default;
};

struct explicit_alias_source
{
	explicit_copy_proxy const *proxy{};
	unsigned *alias_count{};
};

// Returning a const reference from an rvalue source forces the semantic lifetime policy to materialize an owned
// `explicit_copy_proxy`. The pointee is external, so this test isolates conversion semantics rather than dangling.
inline constexpr explicit_copy_proxy const &print_alias_define(
	::fast_io::io_alias_t, explicit_alias_source &&source) noexcept
{
	++*source.alias_count;
	return *source.proxy;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, explicit_copy_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, explicit_copy_proxy>, char *iter,
	explicit_copy_proxy const &proxy) noexcept
{
	*iter = proxy.value;
	return iter + 1;
}

template <typename T>
inline ::std::string render(T &&value)
{
	::std::string result;
	::fast_io::ostring_ref_std output{__builtin_addressof(result)};
	::fast_io::print(output, ::std::forward<T>(value));
	return result;
}

static_assert(::fast_io::details::cond_alias_storable<explicit_alias_source>);
static_assert(::std::same_as<
	::fast_io::details::cond_alias_type<explicit_alias_source>, explicit_copy_proxy>);
static_assert(noexcept(::fast_io::details::cond_store(
	::std::declval<explicit_alias_source>())));
static_assert(noexcept(::fast_io::mnp::cond(
	true, ::std::declval<explicit_alias_source>())));

struct throwing_explicit_copy_proxy
{
	inline throwing_explicit_copy_proxy() noexcept = default;
	inline explicit throwing_explicit_copy_proxy(
		throwing_explicit_copy_proxy const &)
	{
		throw 41;
	}

	throwing_explicit_copy_proxy &operator=(throwing_explicit_copy_proxy const &) = delete;
	inline throwing_explicit_copy_proxy(throwing_explicit_copy_proxy &&) noexcept = default;
	inline throwing_explicit_copy_proxy &operator=(throwing_explicit_copy_proxy &&) noexcept = default;
};

struct throwing_explicit_alias_source
{
	throwing_explicit_copy_proxy const *proxy{};
};

inline constexpr throwing_explicit_copy_proxy const &print_alias_define(
	::fast_io::io_alias_t, throwing_explicit_alias_source &&source) noexcept
{
	return *source.proxy;
}

static_assert(::fast_io::details::cond_alias_storable<throwing_explicit_alias_source>);
static_assert(!::fast_io::details::cond_alias_nothrow_constructible<
	throwing_explicit_alias_source>);
static_assert(!noexcept(::fast_io::mnp::cond(
	true, ::std::declval<throwing_explicit_alias_source>())));

} // namespace

int main()
{
	auto alias_copy_only{::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(copy_only_alias_result_source{}))};
	assert(alias_copy_only.value == 17);
	auto status_copy_only{
		::fast_io::io_print_forward<char>(copy_only_status_result_source{})};
	assert(status_copy_only.value == 23);

	unsigned conversion_count{};
	unsigned alias_count{};
	explicit_copy_proxy yes{'Y', conversion_count};
	explicit_copy_proxy no{'N', conversion_count};

	auto one_arm{::fast_io::mnp::cond(
		true, explicit_alias_source{__builtin_addressof(yes), __builtin_addressof(alias_count)})};
	assert(conversion_count == 1u);
	assert(alias_count == 1u);
	assert(one_arm.t1.value == 'Y');
	assert(render(one_arm) == "Y");

	auto two_arms{::fast_io::mnp::cond(
		false,
		explicit_alias_source{__builtin_addressof(yes), __builtin_addressof(alias_count)},
		explicit_alias_source{__builtin_addressof(no), __builtin_addressof(alias_count)})};
	assert(conversion_count == 3u);
	assert(alias_count == 3u);
	assert(render(two_arms) == "N");

	throwing_explicit_copy_proxy throwing_proxy;
	bool conversion_exception{};
	try
	{
		(void)::fast_io::mnp::cond(
			true, throwing_explicit_alias_source{__builtin_addressof(throwing_proxy)});
	}
	catch (int value)
	{
		conversion_exception = value == 41;
	}
	assert(conversion_exception);
}
