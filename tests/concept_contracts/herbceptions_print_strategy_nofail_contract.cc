/*
This compile-time contract verifies that speculative print strategies admit a
customization only when its exact named-lvalue invocation has neither the
Herbception deterministic channel nor a traditional C++ unwind edge.
*/

#include <fast_io.h>

#if defined(__HERBCEPTIONS__)
#define FAST_IO_TEST_DETERMINISTIC_EFFECT throws
#else
#define FAST_IO_TEST_DETERMINISTIC_EFFECT noexcept
#endif

namespace herbceptions_print_strategy_nofail_contract
{

struct retained_effect_leaf
{};

struct byte_effect_leaf
{};

struct define_effect_leaf
{};

struct shift_effect_leaf
{};

struct extended_effect_leaf
{};

struct public_alias_effect_leaf
{};

struct public_define_effect_leaf
{};

struct scatter_effect_leaf
{};

struct alias_scatter_effect_leaf
{};

struct cursor_effect_output
{};

template <typename value_type>
inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, retained_effect_leaf>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	retained_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, byte_effect_leaf>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	byte_effect_leaf &) noexcept
{
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, byte_effect_leaf>,
	::fast_io::io_scatter_t *scatters, char *reserve,
	byte_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {scatters, reserve};
}

template <typename value_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, define_effect_leaf>, char *iter,
	define_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return iter;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, shift_effect_leaf>, char *iter,
	shift_effect_leaf &) noexcept
{
	return iter;
}

inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, shift_effect_leaf>,
	shift_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, extended_effect_leaf>, char *iter,
	extended_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return iter;
}

inline constexpr ::std::true_type print_extended_bounded_passive_companion_safe(
	::fast_io::io_reserve_type_t<char, extended_effect_leaf>) noexcept
{
	return {};
}

inline public_alias_effect_leaf print_alias_define(
	::fast_io::io_alias_t,
	public_alias_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {};
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, public_alias_effect_leaf>, char *iter,
	public_alias_effect_leaf &) noexcept
{
	return iter;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, public_define_effect_leaf>, char *iter,
	public_define_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return iter;
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scatter_effect_leaf>,
	scatter_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {};
}

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t,
	alias_scatter_effect_leaf &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return {};
}

template <typename value_type>
	requires(::std::same_as<value_type, scatter_effect_leaf> ||
			 ::std::same_as<value_type, alias_scatter_effect_leaf>)
inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return {};
}

template <typename value_type>
	requires(::std::same_as<value_type, scatter_effect_leaf> ||
			 ::std::same_as<value_type, alias_scatter_effect_leaf>)
inline constexpr ::std::true_type print_scatter_output_state_independent(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return {};
}

template <typename value_type>
	requires(::std::same_as<value_type, scatter_effect_leaf> ||
			 ::std::same_as<value_type, alias_scatter_effect_leaf>)
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return {};
}

template <typename value_type>
	requires(::std::same_as<value_type, scatter_effect_leaf> ||
			 ::std::same_as<value_type, alias_scatter_effect_leaf>)
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	::fast_io::io_reserve_type_t<char, value_type>) noexcept
{
	return {};
}

inline char *obuffer_curr(cursor_effect_output &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return nullptr;
}

inline char *obuffer_end(cursor_effect_output &) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return nullptr;
}

inline void obuffer_set_curr(
	cursor_effect_output &, char *) FAST_IO_TEST_DETERMINISTIC_EFFECT
{}

} // namespace herbceptions_print_strategy_nofail_contract

namespace fast_io::details::decay
{

template <>
struct print_buffered_passive_reserve_leaf<
	::herbceptions_print_strategy_nofail_contract::define_effect_leaf>
	: ::std::true_type
{};

template <>
struct print_fixed_public_integral_manip_source_traits<
	::herbceptions_print_strategy_nofail_contract::public_alias_effect_leaf>
{
	inline static constexpr bool available{true};
};

template <>
struct print_fixed_public_integral_manip_source_traits<
	::herbceptions_print_strategy_nofail_contract::public_define_effect_leaf>
{
	inline static constexpr bool available{true};
};

} // namespace fast_io::details::decay

namespace herbceptions_print_strategy_nofail_contract
{

inline constexpr bool retained_safe{
	::fast_io::details::decay::print_retained_buffered_reserve_scatters_nothrow_v<
		char, retained_effect_leaf>};
inline constexpr bool byte_safe{
	::fast_io::details::decay::print_retained_buffered_reserve_scatters_exact_byte_nothrow_v<
		char, byte_effect_leaf>};
inline constexpr bool semantic_define_safe{
	::fast_io::operations::decay::print_semantic_single_pass_bounded_define_nothrow<
		char, define_effect_leaf &>()};
inline constexpr bool semantic_shift_safe{
	::fast_io::operations::decay::print_semantic_single_pass_bounded_internal_shift_nothrow<
		char, shift_effect_leaf &>()};
inline constexpr bool passive_safe{
	::fast_io::operations::decay::print_semantic_single_pass_bounded_passive_companion<
		char, define_effect_leaf>};
inline constexpr bool extended_safe{
	::fast_io::operations::decay::print_semantic_extended_bounded_passive_companion_impl<
		char, extended_effect_leaf>()};
inline constexpr bool prefix_safe{
	::fast_io::operations::decay::print_fixed_prefix_scalar_define_nofail_v<
		char, define_effect_leaf>};
inline constexpr bool public_alias_safe{
	::fast_io::details::decay::print_fixed_public_integral_manip_source_available<
		char, public_alias_effect_leaf &>()};
inline constexpr bool public_define_safe{
	::fast_io::details::decay::print_fixed_public_integral_manip_source_available<
		char, public_define_effect_leaf &>()};
inline constexpr bool mixed_leaf_safe{
	::fast_io::details::decay::print_buffered_mixed_put_area_leaf<
		char, define_effect_leaf &>()};
inline constexpr bool compiler_constant_passive_safe{
	::fast_io::operations::decay::print_compiler_constant_pre_normalization_static_source<
		char, define_effect_leaf>::passive};
inline constexpr bool compiler_constant_scatter_safe{
	::fast_io::operations::decay::print_compiler_constant_pre_normalization_fragment_source<
		char, scatter_effect_leaf>::stable_scatter};
inline constexpr bool static_provider_scatter_safe{
	::fast_io::operations::decay::print_static_provider_mixed_dynamic_component_query<
		char, scatter_effect_leaf>()};
inline constexpr bool static_provider_alias_scatter_safe{
	::fast_io::operations::decay::print_static_provider_mixed_alias_scatter_component_query<
		char, alias_scatter_effect_leaf>()};
inline constexpr bool mixed_cursor_safe{
	::fast_io::details::decay::print_buffered_mixed_nothrow_put_area<
		cursor_effect_output, char>};

#if defined(__HERBCEPTIONS__)
static_assert(!retained_safe);
static_assert(!byte_safe);
static_assert(!semantic_define_safe);
static_assert(!semantic_shift_safe);
static_assert(!passive_safe);
static_assert(!extended_safe);
static_assert(!prefix_safe);
static_assert(!public_alias_safe);
static_assert(!public_define_safe);
static_assert(!mixed_leaf_safe);
static_assert(!compiler_constant_passive_safe);
static_assert(!compiler_constant_scatter_safe);
static_assert(!static_provider_scatter_safe);
static_assert(!static_provider_alias_scatter_safe);
static_assert(!mixed_cursor_safe);
#else
// On standard compilers the test effect macro is `noexcept`; these positive checks prove that the existing strategy
// admission and ordinary generated-code decisions have not been narrowed by Herb-only requirements.
static_assert(retained_safe);
static_assert(byte_safe);
static_assert(semantic_define_safe);
static_assert(semantic_shift_safe);
static_assert(passive_safe);
static_assert(extended_safe);
static_assert(prefix_safe);
static_assert(public_alias_safe);
static_assert(public_define_safe);
static_assert(mixed_leaf_safe);
static_assert(compiler_constant_passive_safe);
static_assert(compiler_constant_scatter_safe);
static_assert(static_provider_scatter_safe);
static_assert(static_provider_alias_scatter_safe);
static_assert(mixed_cursor_safe);
#endif

} // namespace herbceptions_print_strategy_nofail_contract

#undef FAST_IO_TEST_DETERMINISTIC_EFFECT

int main()
{
	return 0;
}
