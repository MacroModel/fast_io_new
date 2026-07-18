#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct alias_proxy
{
	char value{};

	alias_proxy() = default;
	inline explicit constexpr alias_proxy(char ch) noexcept : value(ch)
	{}
	alias_proxy(alias_proxy const &) = delete;
	alias_proxy &operator=(alias_proxy const &) = delete;
	alias_proxy(alias_proxy &&) = delete;
	alias_proxy &operator=(alias_proxy &&) = delete;
};

struct mutable_alias_source
{
	alias_proxy proxy;
};

// The deliberately narrow overload is the regression: concept recognition must not replace `T&` with a synthesized
// rvalue and must not add const to the returned proxy before the reserve CPO receives it.
inline constexpr alias_proxy &print_alias_define(
	::fast_io::io_alias_t, mutable_alias_source &source) noexcept
{
	return source.proxy;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, alias_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, alias_proxy>, char *iter, alias_proxy &proxy) noexcept
{
	*iter = proxy.value;
	return iter + 1;
}

struct throwing_alias_source
{
	alias_proxy proxy;
};

inline alias_proxy &print_alias_define(::fast_io::io_alias_t, throwing_alias_source &)
{
	throw 17;
}

struct status_reference_source
{
	alias_proxy *proxy;
};

inline constexpr alias_proxy &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, status_reference_source &source) noexcept
{
	return *source.proxy;
}

struct throwing_status_source
{
	alias_proxy *proxy;
};

inline alias_proxy &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, throwing_status_source &)
{
	throw 23;
}

struct owning_proxy
{
	char value{};
};

struct move_only_parameter_formatter
{
	char value{};
	::std::size_t reserve_size_calls{};
	::std::size_t reserve_define_calls{};
	::std::size_t precise_size_calls{};
	::std::size_t precise_define_calls{};
	::std::size_t shift_calls{};

	move_only_parameter_formatter() = default;
	inline explicit constexpr move_only_parameter_formatter(char ch) noexcept : value(ch)
	{}
	move_only_parameter_formatter(move_only_parameter_formatter const &) = delete;
	move_only_parameter_formatter &operator=(move_only_parameter_formatter const &) = delete;
	move_only_parameter_formatter(move_only_parameter_formatter &&) = default;
	move_only_parameter_formatter &operator=(move_only_parameter_formatter &&) = default;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, move_only_parameter_formatter>,
	move_only_parameter_formatter &formatter) noexcept
{
	++formatter.reserve_size_calls;
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, move_only_parameter_formatter>, char *destination,
	move_only_parameter_formatter &formatter) noexcept
{
	++formatter.reserve_define_calls;
	*destination = formatter.value;
	return destination + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, move_only_parameter_formatter>,
	move_only_parameter_formatter &formatter) noexcept
{
	++formatter.precise_size_calls;
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, move_only_parameter_formatter>, char *destination,
	::std::size_t, move_only_parameter_formatter &formatter) noexcept
{
	++formatter.precise_define_calls;
	*destination = formatter.value;
	return destination + 1;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, move_only_parameter_formatter>,
	move_only_parameter_formatter &formatter) noexcept
{
	++formatter.shift_calls;
	return 0u;
}

// This deliberately over-aligned, immovable formatter cannot enter any ABI-small value branch on a supported target.
// Its CPOs are read-only in the C++ type system, while mutable counters make object identity observable. Consequently
// the same test covers register-oriented SysV/AAPCS/MS aggregate ABIs and caller-storage/unknown ABIs: every parameter
// bridge must borrow the exact member expression, and no target-specific copy convention can mask a by-value adapter.
struct alignas(64) const_parameter_formatter
{
	char value{};
	mutable ::std::size_t direct_calls{};
	mutable ::std::size_t dynamic_size_calls{};
	mutable ::std::size_t reserve_define_calls{};
	mutable ::std::size_t precise_size_calls{};
	mutable ::std::size_t precise_define_calls{};
	mutable ::std::size_t shift_calls{};
	mutable ::std::size_t context_calls{};

	const_parameter_formatter() = default;
	inline explicit constexpr const_parameter_formatter(char ch) noexcept : value(ch)
	{}
	const_parameter_formatter(const_parameter_formatter const &) = delete;
	const_parameter_formatter &operator=(const_parameter_formatter const &) = delete;
	const_parameter_formatter(const_parameter_formatter &&) = delete;
	const_parameter_formatter &operator=(const_parameter_formatter &&) = delete;
};

struct const_parameter_direct_output
{
	const_parameter_formatter const *observed{};
};

template <typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>, output &out,
	const_parameter_formatter const &formatter) noexcept
{
	++formatter.direct_calls;
	if constexpr (requires { out.observed = __builtin_addressof(formatter); })
	{
		out.observed = __builtin_addressof(formatter);
	}
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return 1u;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>,
	const_parameter_formatter const &formatter) noexcept
{
	++formatter.dynamic_size_calls;
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>, char *destination,
	const_parameter_formatter const &formatter) noexcept
{
	++formatter.reserve_define_calls;
	*destination = formatter.value;
	return destination + 1;
}

inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return 1u;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>,
	const_parameter_formatter const &formatter) noexcept
{
	++formatter.shift_calls;
	return 0u;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>,
	const_parameter_formatter const &formatter) noexcept
{
	++formatter.precise_size_calls;
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>, char *destination,
	::std::size_t, const_parameter_formatter const &formatter) noexcept
{
	++formatter.precise_define_calls;
	*destination = formatter.value;
	return destination + 1;
}

inline constexpr ::std::size_t print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return 1u;
}

inline constexpr ::std::true_type print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_buffered_preferred(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_put_area_preferred(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_one_pass_preferred(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return {};
}

struct const_parameter_context
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		const_parameter_formatter const &formatter, char *begin, char *end) noexcept
	{
		++formatter.context_calls;
		if (begin == end)
		{
			return {begin, false};
		}
		*begin = formatter.value;
		return {begin + 1, true};
	}
};

inline constexpr ::fast_io::io_type_t<const_parameter_context> print_context_type(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char, const_parameter_formatter>) noexcept
{
	return 1u;
}

struct temporary_alias_source
{
	char value{};
};

inline constexpr owning_proxy print_alias_define(
	::fast_io::io_alias_t, temporary_alias_source &&source) noexcept
{
	return {source.value};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, owning_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, owning_proxy>, char *iter, owning_proxy proxy) noexcept
{
	*iter = proxy.value;
	return iter + 1;
}

struct temporary_borrowed_alias_source
{
	alias_proxy proxy;
};

// This adversarial rvalue customization exposes a noncopyable subobject. The reference is valid while aliasing runs,
// but no semantic node may retain it after the source full-expression ends.
inline constexpr alias_proxy &print_alias_define(
	::fast_io::io_alias_t, temporary_borrowed_alias_source &&source) noexcept
{
	return source.proxy;
}

struct immovable_owning_proxy
{
	char value{};

	inline explicit constexpr immovable_owning_proxy(char ch) noexcept : value(ch)
	{}
	immovable_owning_proxy(immovable_owning_proxy const &) = delete;
	immovable_owning_proxy &operator=(immovable_owning_proxy const &) = delete;
	immovable_owning_proxy(immovable_owning_proxy &&) = delete;
	immovable_owning_proxy &operator=(immovable_owning_proxy &&) = delete;
};

struct immovable_alias_source
{
	char value{};
};

inline constexpr immovable_owning_proxy print_alias_define(
	::fast_io::io_alias_t, immovable_alias_source &&source) noexcept
{
	return immovable_owning_proxy{source.value};
}

struct staged_token
{
	char value{};
};

struct staged_state
{
	char value{};
};

struct alignas(16) layout_large_arm
{
	char payload[32u]{};
};

struct alignas(8) layout_small_arm
{
	char payload[24u]{};
};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, staged_token>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, staged_token>, char *iter, staged_token const &token) noexcept
{
	*iter = token.value;
	return iter + 1;
}

inline constexpr ::fast_io::io_type_t<staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, staged_token>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, staged_token>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, staged_token>, staged_token const &) noexcept
{
	return true;
}

inline constexpr staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, staged_token>, staged_token const &token) noexcept
{
	return {token.value};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, staged_token>, char *iter, staged_token const &,
	staged_state const &state) noexcept
{
	*iter = state.value;
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

template <typename T>
concept can_make_pack = requires(T &&value) {
	::fast_io::mnp::pack(::std::forward<T>(value));
};

template <typename T>
concept can_make_condition = requires(T &&value) {
	::fast_io::mnp::cond(true, ::std::forward<T>(value));
};

static_assert(::fast_io::alias_printable<mutable_alias_source &>);
static_assert(!::fast_io::alias_printable<mutable_alias_source const &>);
static_assert(!::fast_io::alias_printable<mutable_alias_source>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_alias(::std::declval<mutable_alias_source &>())), alias_proxy &>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_forward<char>(::std::declval<status_reference_source &>())),
			  ::fast_io::parameter<alias_proxy &>>);
static_assert(!noexcept(::fast_io::io_print_alias(::std::declval<throwing_alias_source &>())));
static_assert(!noexcept(::fast_io::io_print_forward<char>(::std::declval<throwing_status_source &>())));
static_assert(!noexcept(::fast_io::mnp::pack(::std::declval<throwing_alias_source &>())));
static_assert(!noexcept(::fast_io::mnp::cond(
	true, ::std::declval<throwing_alias_source &>(), ::std::declval<mutable_alias_source &>())));
static_assert(!::std::is_reference_v<
			  decltype(::fast_io::io_print_alias(::std::declval<temporary_alias_source>()))>);
static_assert(!can_make_pack<temporary_borrowed_alias_source>);
static_assert(!can_make_condition<temporary_borrowed_alias_source>);
// Guaranteed copy elision can return this prvalue from the CPO, but the named forwarding parameter cannot own it.
// Rejection now occurs at alias protocol recognition rather than being deferred to each semantic factory.
static_assert(!::fast_io::alias_printable<immovable_alias_source>);
static_assert(::fast_io::staged_printable<char, staged_token>);
static_assert(::fast_io::staged_printable<char, ::fast_io::parameter<staged_token &>>);
using compact_layout_condition = decltype(::fast_io::mnp::cond(
	true, layout_large_arm{}, layout_small_arm{}));
static_assert(::std::same_as<
			  typename compact_layout_condition::alias_type1, layout_small_arm>);
static_assert(sizeof(compact_layout_condition) ==
			  sizeof(::fast_io::manipulators::condition<layout_small_arm, layout_large_arm>));
static_assert(sizeof(compact_layout_condition) <
			  sizeof(::fast_io::manipulators::condition<layout_large_arm, layout_small_arm>));
static_assert(::fast_io::dynamic_reserve_printable<
			  char, ::fast_io::parameter<move_only_parameter_formatter>>);
static_assert(::fast_io::precise_reserve_printable<
			  char, ::fast_io::parameter<move_only_parameter_formatter>>);
static_assert(::fast_io::printable_internal_shift<
			  char, ::fast_io::parameter<move_only_parameter_formatter>>);
using mutable_reference_in_const_parameter =
	::fast_io::parameter<move_only_parameter_formatter &> const;
// Const applies to the wrapper, not to a language reference stored in it. These assertions guard the less obvious
// half of `parameter_const_member_reference_t`: a mutable referenced formatter must remain mutable through a const
// transport wrapper, exactly as the core language specifies for reference members.
static_assert(::std::same_as<
			  ::fast_io::details::parameter_const_member_reference_t<move_only_parameter_formatter &>,
			  move_only_parameter_formatter &>);
static_assert(::fast_io::dynamic_reserve_printable<char, mutable_reference_in_const_parameter>);
static_assert(::fast_io::precise_reserve_printable<char, mutable_reference_in_const_parameter>);
static_assert(::fast_io::printable_internal_shift<char, mutable_reference_in_const_parameter>);
using mutable_owned_in_const_parameter =
	::fast_io::parameter<move_only_parameter_formatter> const;
// Conversely, const does apply to an owned member. The const bridges must not recover mutable access merely because
// the same unqualified parameter tag also serves mutable wrappers.
static_assert(!::fast_io::dynamic_reserve_printable<char, mutable_owned_in_const_parameter>);
static_assert(!::fast_io::precise_reserve_printable<char, mutable_owned_in_const_parameter>);
static_assert(!::fast_io::printable_internal_shift<char, mutable_owned_in_const_parameter>);

using const_owned_parameter = ::fast_io::parameter<const_parameter_formatter> const;
static_assert(::std::same_as<
			  ::fast_io::details::parameter_const_member_reference_t<const_parameter_formatter>,
			  const_parameter_formatter const &>);
static_assert(alignof(const_owned_parameter) == alignof(const_parameter_formatter));
static_assert(sizeof(const_parameter_formatter) >
			  ::fast_io::details::io_print_forward_transport_max_value_size);
static_assert(!::std::copy_constructible<const_parameter_formatter>);
static_assert(!::std::move_constructible<const_parameter_formatter>);
static_assert(::fast_io::reserve_printable<char, const_owned_parameter>);
static_assert(::fast_io::dynamic_reserve_printable<char, const_owned_parameter>);
static_assert(::fast_io::dynamic_reserve_with_possible_static_stack_size<
			  char, const_owned_parameter>);
static_assert(::fast_io::printable_internal_shift<char, const_owned_parameter>);
static_assert(::fast_io::precise_reserve_printable<char, const_owned_parameter>);
static_assert(::fast_io::static_precise_reserve_printable<char, const_owned_parameter>);
static_assert(::fast_io::precise_resize_initialization_sensitive_printable<
			  char, const_owned_parameter>);
static_assert(::fast_io::context_printable<char, const_owned_parameter>);
static_assert(::fast_io::context_printable_with_static_buffer_size<
			  char, const_owned_parameter>);
static_assert(::fast_io::printable<char, const_owned_parameter>);
static_assert(::fast_io::buffered_printable_preferred<char, const_owned_parameter>);
static_assert(::fast_io::put_area_printable_preferred<char, const_owned_parameter>);
static_assert(::fast_io::one_pass_printable_preferred<char, const_owned_parameter>);

using const_reference_parameter = ::fast_io::parameter<const_parameter_formatter &>;
using conservative_abi_nested_parameter =
	::fast_io::parameter<const_reference_parameter const &>;
using conservative_abi_nested_const_parameter = conservative_abi_nested_parameter const;
// On an ABI that declines the optional small-value result copy, normalizing a const parameter-wrapper lvalue creates
// exactly this nested reference transport. Both member-expression aliases must retain the const inner wrapper; that
// inner wrapper must in turn retain its mutable language reference to the ultimate formatter. Proving the complete
// protocol graph here prevents target-specific ABI policy from turning a valid read-only formatter into a false
// capability merely because one additional transparent parameter node was required.
static_assert(::std::same_as<
			  ::fast_io::details::parameter_mutable_member_reference_t<const_reference_parameter const &>,
			  const_reference_parameter const &>);
static_assert(::std::same_as<
			  ::fast_io::details::parameter_const_member_reference_t<const_reference_parameter const &>,
			  const_reference_parameter const &>);
static_assert(::std::is_trivially_copyable_v<conservative_abi_nested_parameter>);
static_assert(sizeof(conservative_abi_nested_parameter) == sizeof(void *));
static_assert(::fast_io::reserve_printable<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::dynamic_reserve_printable<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::printable_internal_shift<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::precise_reserve_printable<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::context_printable<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::printable<char, conservative_abi_nested_parameter>);
static_assert(::fast_io::buffered_printable_preferred<
			  char, conservative_abi_nested_parameter>);
static_assert(::fast_io::put_area_printable_preferred<
			  char, conservative_abi_nested_parameter>);
static_assert(::fast_io::one_pass_printable_preferred<
			  char, conservative_abi_nested_parameter>);
static_assert(::fast_io::reserve_printable<
			  char, conservative_abi_nested_const_parameter>);
static_assert(::fast_io::dynamic_reserve_printable<
			  char, conservative_abi_nested_const_parameter>);
static_assert(::fast_io::printable_internal_shift<
			  char, conservative_abi_nested_const_parameter>);
static_assert(::fast_io::precise_reserve_printable<
			  char, conservative_abi_nested_const_parameter>);
static_assert(::fast_io::context_printable<
			  char, conservative_abi_nested_const_parameter>);
static_assert(::fast_io::printable<char, conservative_abi_nested_const_parameter>);

} // namespace

int main()
{
	::fast_io::parameter<move_only_parameter_formatter> owned_formatter{
		move_only_parameter_formatter{'P'}};
	char parameter_buffer[1u]{};
	assert(print_reserve_size(
			   ::fast_io::io_reserve_type<char, decltype(owned_formatter)>, owned_formatter) == 1u);
	auto parameter_end{print_reserve_define(
		::fast_io::io_reserve_type<char, decltype(owned_formatter)>, parameter_buffer,
		owned_formatter)};
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'P');
	assert(print_reserve_precise_size(
			   ::fast_io::io_reserve_type<char, decltype(owned_formatter)>, owned_formatter) == 1u);
	parameter_end = print_reserve_precise_define(
		::fast_io::io_reserve_type<char, decltype(owned_formatter)>, parameter_buffer, 1u,
		owned_formatter);
	assert(parameter_end == parameter_buffer + 1u);
	assert(print_define_internal_shift(
			   ::fast_io::io_reserve_type<char, decltype(owned_formatter)>, owned_formatter) == 0u);
	assert(owned_formatter.reference.reserve_size_calls == 1u);
	assert(owned_formatter.reference.reserve_define_calls == 1u);
	assert(owned_formatter.reference.precise_size_calls == 1u);
	assert(owned_formatter.reference.precise_define_calls == 1u);
	assert(owned_formatter.reference.shift_calls == 1u);

	// A const reference-member wrapper still designates the mutable formatter. This run-time half proves that the
	// const overload does not create a second owner and that every phase updates the original referent.
	move_only_parameter_formatter referenced_formatter{'R'};
	::fast_io::parameter<move_only_parameter_formatter &> const referenced_wrapper{
		referenced_formatter};
	assert(print_reserve_size(
			   ::fast_io::io_reserve_type<char, ::std::remove_cvref_t<decltype(referenced_wrapper)>>,
			   referenced_wrapper) == 1u);
	parameter_end = print_reserve_define(
		::fast_io::io_reserve_type<char, ::std::remove_cvref_t<decltype(referenced_wrapper)>>,
		parameter_buffer, referenced_wrapper);
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'R');
	assert(referenced_formatter.reserve_size_calls == 1u);
	assert(referenced_formatter.reserve_define_calls == 1u);

	const_owned_parameter const_wrapper{const_parameter_formatter{'C'}};
	auto const *const_identity{__builtin_addressof(const_wrapper.reference)};
	const_parameter_direct_output direct_output{};
	print_define(::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
				 direct_output, const_wrapper);
	assert(direct_output.observed == const_identity);
	assert(print_reserve_size(
			   ::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
			   const_wrapper) == 1u);
	parameter_end = print_reserve_define(
		::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
		parameter_buffer, const_wrapper);
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'C');
	assert(print_reserve_precise_size(
			   ::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
			   const_wrapper) == 1u);
	parameter_end = print_reserve_precise_define(
		::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
		parameter_buffer, 1u, const_wrapper);
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'C');
	assert(print_define_internal_shift(
			   ::fast_io::io_reserve_type<char, ::std::remove_cvref_t<const_owned_parameter>>,
			   const_wrapper) == 0u);
	using const_wrapper_context =
		::fast_io::details::print_context_state_t<char, const_owned_parameter>;
	const_wrapper_context context{};
	auto const context_result{context.print_context_define(
		const_wrapper, parameter_buffer, parameter_buffer + 1u)};
	assert(context_result.iter == parameter_buffer + 1u && context_result.done &&
		   parameter_buffer[0] == 'C');
	assert(__builtin_addressof(const_wrapper.reference) == const_identity);
	assert(const_wrapper.reference.direct_calls == 1u);
	assert(const_wrapper.reference.dynamic_size_calls == 1u);
	assert(const_wrapper.reference.reserve_define_calls == 1u);
	assert(const_wrapper.reference.precise_size_calls == 1u);
	assert(const_wrapper.reference.precise_define_calls == 1u);
	assert(const_wrapper.reference.shift_calls == 1u);
	assert(const_wrapper.reference.context_calls == 1u);

	const_parameter_formatter nested_formatter{'N'};
	const_reference_parameter const inner_reference_wrapper{nested_formatter};
	conservative_abi_nested_const_parameter nested_wrapper{inner_reference_wrapper};
	auto const *nested_identity{__builtin_addressof(nested_formatter)};
	const_parameter_direct_output nested_direct_output{};
	print_define(
		::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>,
		nested_direct_output, nested_wrapper);
	assert(nested_direct_output.observed == nested_identity);
	assert(print_reserve_size(
			   ::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>,
			   nested_wrapper) == 1u);
	parameter_end = print_reserve_define(
		::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>, parameter_buffer,
		nested_wrapper);
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'N');
	assert(print_reserve_precise_size(
			   ::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>,
			   nested_wrapper) == 1u);
	parameter_end = print_reserve_precise_define(
		::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>, parameter_buffer,
		1u, nested_wrapper);
	assert(parameter_end == parameter_buffer + 1u && parameter_buffer[0] == 'N');
	assert(print_define_internal_shift(
			   ::fast_io::io_reserve_type<char, conservative_abi_nested_parameter>,
			   nested_wrapper) == 0u);
	using nested_wrapper_context = ::fast_io::details::print_context_state_t<
		char, conservative_abi_nested_const_parameter>;
	nested_wrapper_context nested_context{};
	auto const nested_context_result{nested_context.print_context_define(
		nested_wrapper, parameter_buffer, parameter_buffer + 1u)};
	assert(nested_context_result.iter == parameter_buffer + 1u &&
		   nested_context_result.done && parameter_buffer[0] == 'N');
	assert(__builtin_addressof(inner_reference_wrapper.reference) == nested_identity);
	assert(nested_formatter.direct_calls == 1u);
	assert(nested_formatter.dynamic_size_calls == 1u);
	assert(nested_formatter.reserve_define_calls == 1u);
	assert(nested_formatter.precise_size_calls == 1u);
	assert(nested_formatter.precise_define_calls == 1u);
	assert(nested_formatter.shift_calls == 1u);
	assert(nested_formatter.context_calls == 1u);

	mutable_alias_source direct{alias_proxy{'D'}};
	assert(render(direct) == "D");

	auto packed{::fast_io::mnp::pack(direct)};
	auto &packed_proxy{::fast_io::containers::get<0u>(packed.storage)};
	static_assert(::std::same_as<decltype(packed_proxy), alias_proxy &>);
	assert(__builtin_addressof(packed_proxy) == __builtin_addressof(direct.proxy));
	assert(render(packed) == "D");

	mutable_alias_source alternate{alias_proxy{'X'}};
	auto selected{::fast_io::mnp::cond(true, direct, alternate)};
	static_assert(::std::is_reference_v<typename decltype(selected)::alias_type1>);
	assert(__builtin_addressof(selected.t1) == __builtin_addressof(direct.proxy));
	assert(render(selected) == "D");

	// The source temporaries are gone before either semantic object is rendered. Both manipulators must therefore own
	// the alias values instead of retaining references into those completed full-expressions.
	auto temporary_pack{::fast_io::mnp::pack(temporary_alias_source{'P'})};
	assert(render(temporary_pack) == "P");
	auto temporary_condition{
		::fast_io::mnp::cond(true, temporary_alias_source{'C'}, temporary_alias_source{'N'})};
	assert(render(temporary_condition) == "C");

	status_reference_source status_source{__builtin_addressof(direct.proxy)};
	// Status forwarding uses the same small by-value reference transport as ordinary nontrivial lvalues. The wrapper
	// survives every by-value dispatcher boundary while its member retains the customization's exact object identity.
	auto status_forwarded{::fast_io::io_print_forward<char>(status_source)};
	assert(__builtin_addressof(status_forwarded.reference) ==
		   __builtin_addressof(direct.proxy));

	bool alias_exception{};
	throwing_alias_source throwing_alias{alias_proxy{'A'}};
	try
	{
		(void)::fast_io::mnp::pack(throwing_alias);
	}
	catch (int value)
	{
		alias_exception = value == 17;
	}
	assert(alias_exception);

	bool status_exception{};
	throwing_status_source throwing_status{__builtin_addressof(direct.proxy)};
	try
	{
		(void)::fast_io::io_print_forward<char>(throwing_status);
	}
	catch (int value)
	{
		status_exception = value == 23;
	}
	assert(status_exception);

	staged_token staged{'S'};
	::fast_io::parameter<staged_token &> wrapped{staged};
	auto prepared{print_staged_prepare(::fast_io::io_reserve_type<char, decltype(wrapped)>, wrapped)};
	char buffer{};
	assert(print_staged_define(
			   ::fast_io::io_reserve_type<char, decltype(wrapped)>, __builtin_addressof(buffer), wrapped, prepared) ==
		   __builtin_addressof(buffer) + 1);
	assert(buffer == 'S');
}
