/*
This translation unit proves that width normalization and pattern-width
materialization preserve two orthogonal failure channels without changing the
established value/reference storage policy. It is compiled by ordinary Clang
and by the experimental Herbception toolchain.
*/

#define FAST_IO_HERBCEPTIONS_THROWS 801
#define FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(herb_may_fail, ordinary_nothrow) 802
#define FAST_IO_HERBCEPTIONS_NOTHROWS(expression) 803
#define FAST_IO_HERBCEPTIONS_NOEXCEPT(expression) 804

#include <fast_io_format/details/field.h>

static_assert(FAST_IO_HERBCEPTIONS_THROWS == 801);
static_assert(FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(0, 0) == 802);
static_assert(FAST_IO_HERBCEPTIONS_NOTHROWS(0) == 803);
static_assert(FAST_IO_HERBCEPTIONS_NOEXCEPT(0) == 804);

#undef FAST_IO_HERBCEPTIONS_THROWS
#undef FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT
#undef FAST_IO_HERBCEPTIONS_NOTHROWS
#undef FAST_IO_HERBCEPTIONS_NOEXCEPT

#if defined(__HERBCEPTIONS__)
#define FAST_IO_TEST_DETERMINISTIC_EFFECT throws
#else
#define FAST_IO_TEST_DETERMINISTIC_EFFECT noexcept
#endif

namespace herbceptions_width_effect_contract
{

struct alias_value
{
	int payload{};
};

struct safe_alias_source
{
};

inline alias_value print_alias_define(
	::fast_io::io_alias_t, safe_alias_source &) noexcept
{
	return {11};
}

struct deterministic_alias_source
{
	bool fail{};
};

inline alias_value print_alias_define(
	::fast_io::io_alias_t,
	[[maybe_unused]] deterministic_alias_source &source)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return {17};
}

using safe_width = decltype(::fast_io::manipulators::left(
	::std::declval<safe_alias_source &>(), 4u));
using deterministic_width = decltype(::fast_io::manipulators::left(
	::std::declval<deterministic_alias_source &>(), 4u));

// A value-returning alias remains value storage on both ABIs. Effect adaptation must never replace this decay boundary
// with a reference merely to avoid constructing the deterministic success payload.
static_assert(!::std::is_reference_v<decltype(safe_width::reference)>);
static_assert(!::std::is_reference_v<decltype(deterministic_width::reference)>);

struct safe_leaf
{
	bool fail{};
};

struct deterministic_size_leaf
{
	bool fail{};
};

struct deterministic_emit_leaf
{
	bool fail{};
};

struct deterministic_shift_leaf
{
	bool fail{};
};

struct deterministic_scatter_leaf
{
	bool fail{};
};

struct constexpr_static_size_leaf
{
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, safe_leaf>, safe_leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, safe_leaf>, char *iter,
	safe_leaf &) noexcept
{
	*iter = 's';
	return iter + 1;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, safe_leaf>, safe_leaf &) noexcept
{
	return 1u;
}

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_size_leaf>,
	[[maybe_unused]] deterministic_size_leaf &value)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_size_leaf>, char *iter,
	deterministic_size_leaf &) noexcept
{
	*iter = 'z';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_emit_leaf>,
	deterministic_emit_leaf &) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_emit_leaf>, char *iter,
	[[maybe_unused]] deterministic_emit_leaf &value)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	*iter = 'e';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_shift_leaf>,
	deterministic_shift_leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_shift_leaf>, char *iter,
	deterministic_shift_leaf &) noexcept
{
	*iter = '-';
	return iter + 1;
}

inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, deterministic_shift_leaf>,
	[[maybe_unused]] deterministic_shift_leaf &value)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return 1u;
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, deterministic_scatter_leaf>,
	[[maybe_unused]] deterministic_scatter_leaf &value)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	static constexpr char text[]{'q'};
	return {text, 1u};
}

// The static extent is a type-only policy. Although its declaration has a deterministic effect, reserve_printable
// proves successful constant evaluation and pattern-width caches that result rather than exposing a run-time call.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, constexpr_static_size_leaf>)
	FAST_IO_TEST_DETERMINISTIC_EFFECT
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, constexpr_static_size_leaf>, char *iter,
	constexpr_static_size_leaf &) noexcept
{
	*iter = 'c';
	return iter + 1;
}

template <typename leaf_type>
using pattern = ::fast_io::fmt::details::basic_pattern_width<char, 2u, leaf_type>;

template <typename leaf_type>
inline constexpr auto pattern_tag = ::fast_io::io_reserve_type<char, pattern<leaf_type>>;

#if defined(__HERBCEPTIONS__)
static_assert(!throws((::fast_io::manipulators::left(
	::std::declval<safe_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::left(
	::std::declval<deterministic_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::middle(
	::std::declval<deterministic_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::right(
	::std::declval<deterministic_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::internal(
	::std::declval<deterministic_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::left(
	::std::declval<deterministic_alias_source &>(), 4u, '-'))));
static_assert(throws((::fast_io::manipulators::middle(
	::std::declval<deterministic_alias_source &>(), 4u, '-'))));
static_assert(throws((::fast_io::manipulators::right(
	::std::declval<deterministic_alias_source &>(), 4u, '-'))));
static_assert(throws((::fast_io::manipulators::internal(
	::std::declval<deterministic_alias_source &>(), 4u, '-'))));
static_assert(throws((::fast_io::manipulators::width(
	::fast_io::manipulators::scalar_placement::left,
	::std::declval<deterministic_alias_source &>(), 4u))));
static_assert(throws((::fast_io::manipulators::width(
	::fast_io::manipulators::scalar_placement::left,
	::std::declval<deterministic_alias_source &>(), 4u, '-'))));
static_assert(!throws((::fast_io::fmt::details::make_pattern_width<2u>(
	::std::declval<safe_alias_source &>(), 4u,
	::fast_io::manipulators::scalar_placement::right,
	::std::declval<char const *>()))));
static_assert(throws((::fast_io::fmt::details::make_pattern_width<2u>(
	::std::declval<deterministic_alias_source &>(), 4u,
	::fast_io::manipulators::scalar_placement::right,
	::std::declval<char const *>()))));

static_assert(!throws((::fast_io::print_reserve_size(
	pattern_tag<safe_leaf>, ::std::declval<pattern<safe_leaf>>()))));
static_assert(!throws((::fast_io::print_reserve_define(
	pattern_tag<safe_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<safe_leaf>>()))));
static_assert(throws((::fast_io::print_reserve_size(
	pattern_tag<deterministic_size_leaf>,
	::std::declval<pattern<deterministic_size_leaf>>()))));
static_assert(!throws((::fast_io::print_reserve_define(
	pattern_tag<deterministic_size_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<deterministic_size_leaf>>()))));
static_assert(!throws((::fast_io::print_reserve_size(
	pattern_tag<deterministic_emit_leaf>,
	::std::declval<pattern<deterministic_emit_leaf>>()))));
static_assert(throws((::fast_io::print_reserve_define(
	pattern_tag<deterministic_emit_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<deterministic_emit_leaf>>()))));
static_assert(!throws((::fast_io::print_reserve_size(
	pattern_tag<deterministic_shift_leaf>,
	::std::declval<pattern<deterministic_shift_leaf>>()))));
static_assert(throws((::fast_io::print_reserve_define(
	pattern_tag<deterministic_shift_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<deterministic_shift_leaf>>()))));
static_assert(throws((::fast_io::print_reserve_size(
	pattern_tag<deterministic_scatter_leaf>,
	::std::declval<pattern<deterministic_scatter_leaf>>()))));
static_assert(throws((::fast_io::print_reserve_define(
	pattern_tag<deterministic_scatter_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<deterministic_scatter_leaf>>()))));
static_assert(!throws((::fast_io::print_reserve_size(
	pattern_tag<constexpr_static_size_leaf>,
	::std::declval<pattern<constexpr_static_size_leaf>>()))));
#else
static_assert(noexcept(::fast_io::manipulators::left(
	::std::declval<safe_alias_source &>(), 4u)));
static_assert(noexcept(::fast_io::manipulators::left(
	::std::declval<deterministic_alias_source &>(), 4u)));
static_assert(noexcept(::fast_io::print_reserve_size(
	pattern_tag<deterministic_size_leaf>,
	::std::declval<pattern<deterministic_size_leaf>>())));
static_assert(noexcept(::fast_io::print_reserve_define(
	pattern_tag<deterministic_emit_leaf>, ::std::declval<char *>(),
	::std::declval<pattern<deterministic_emit_leaf>>())));
#endif

#if defined(__HERBCEPTIONS__)
alias_value width_store_codegen_probe(deterministic_alias_source &source) throws
{
	return ::fast_io::details::width_store(source);
}

char *pattern_shift_codegen_probe(
	char *output, pattern<deterministic_shift_leaf> field) throws
{
	return ::fast_io::print_reserve_define(
		pattern_tag<deterministic_shift_leaf>, output, field);
}
#endif

} // namespace herbceptions_width_effect_contract

#undef FAST_IO_TEST_DETERMINISTIC_EFFECT

int main()
{
#if defined(__HERBCEPTIONS__)
	using namespace herbceptions_width_effect_contract;
	pattern<deterministic_shift_leaf> field{
		{true}, 4u, ::fast_io::manipulators::scalar_placement::internal, {'x', 'y'}};
	char buffer[16]{};
	try
	{
		(void)pattern_shift_codegen_probe(buffer, field);
		return 1;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument ? 0 : 2;
	}
#else
	return 0;
#endif
}
