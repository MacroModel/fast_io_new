/*
This translation unit verifies that format-only spelling adapters preserve the
deterministic-error ABI of every run-time reserve protocol they forward.  The
same source is compiled by ordinary compilers so the compatibility branch also
proves that existing conditional-noexcept declarations remain intact.
*/

#include <fast_io_format/details/field.h>

namespace herbceptions_format_effect_contract
{

struct safe_leaf
{
	bool fail{};
};

struct deterministic_leaf
{
	bool fail{};
};

struct named_category_leaf
{
	bool fail{};
};

template <typename leaf_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, leaf_type>) noexcept
{
	return 2u;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, safe_leaf>, safe_leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, safe_leaf>, char *iter,
	safe_leaf &) noexcept
{
	*iter = '+';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, safe_leaf>, safe_leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, safe_leaf>, char *iter,
	::std::size_t, safe_leaf &) noexcept
{
	*iter = '+';
	return iter + 1;
}

inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, safe_leaf>, safe_leaf &) noexcept
{
	return 1u;
}

#if defined(__HERBCEPTIONS__)
#define FAST_IO_TEST_DETERMINISTIC_EFFECT throws
#else
#define FAST_IO_TEST_DETERMINISTIC_EFFECT noexcept
#endif

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_leaf>,
	[[maybe_unused]] deterministic_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_leaf>, char *iter,
	[[maybe_unused]] deterministic_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	*iter = '+';
	return iter + 1;
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, deterministic_leaf>,
	[[maybe_unused]] deterministic_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return 1u;
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, deterministic_leaf>, char *iter,
	::std::size_t, [[maybe_unused]] deterministic_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	*iter = '+';
	return iter + 1;
}

inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, deterministic_leaf>,
	[[maybe_unused]] deterministic_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return 1u;
}

// These paired overloads make value category observable: the wrappers receive pointers and sizes by value but invoke
// the leaf with named lvalues. A classifier that substitutes prvalues would select the harmless decoys and publish the
// wrong canonical ABI even though the wrapper body calls the deterministic-effect overloads.
inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, named_category_leaf>, char *&iter,
	[[maybe_unused]] named_category_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	*iter = 'n';
	return iter + 1;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, named_category_leaf>, char *&&iter,
	named_category_leaf &) noexcept
{
	*iter = 'x';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, named_category_leaf>,
	named_category_leaf &) noexcept
{
	return 1u;
}

template <typename size_reference>
	requires(::std::same_as<size_reference, ::std::size_t &> ||
			 ::std::same_as<size_reference, ::std::size_t const &>)
inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, named_category_leaf>, char *&iter,
	size_reference &&size,
	[[maybe_unused]] named_category_leaf &value) FAST_IO_TEST_DETERMINISTIC_EFFECT
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	if (size != 0u)
	{
		*iter = 'n';
	}
	return iter + static_cast<::std::ptrdiff_t>(size != 0u);
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, named_category_leaf>, char *&&iter,
	::std::size_t &&size, named_category_leaf &) noexcept
{
	return iter + static_cast<::std::ptrdiff_t>(size != 0u);
}

#undef FAST_IO_TEST_DETERMINISTIC_EFFECT

using safe_semantic = ::fast_io::manipulators::format_scalar_t<safe_leaf, 0u, true>;
using deterministic_semantic =
	::fast_io::manipulators::format_scalar_t<deterministic_leaf, 0u, true>;
using safe_radix = ::fast_io::manipulators::printf_force_radix_t<safe_leaf>;
using deterministic_radix =
	::fast_io::manipulators::printf_force_radix_t<deterministic_leaf>;
using named_category_semantic =
	::fast_io::manipulators::format_scalar_t<named_category_leaf, 0u, false>;
using named_category_radix =
	::fast_io::manipulators::printf_force_radix_t<named_category_leaf>;

template <typename wrapper_type>
inline constexpr auto reserve_tag = ::fast_io::io_reserve_type<char, wrapper_type>;

#if defined(__HERBCEPTIONS__)
static_assert(!throws((::fast_io::print_reserve_size(
	reserve_tag<safe_semantic>, ::std::declval<safe_semantic>()))));
static_assert(!throws((::fast_io::print_reserve_define(
	reserve_tag<safe_semantic>, ::std::declval<char *>(),
	::std::declval<safe_semantic>()))));
static_assert(!throws((::fast_io::print_reserve_precise_size(
	reserve_tag<safe_semantic>, ::std::declval<safe_semantic>()))));
static_assert(!throws((::fast_io::print_reserve_precise_define(
	reserve_tag<safe_semantic>, ::std::declval<char *>(), 1u,
	::std::declval<safe_semantic>()))));

static_assert(throws((::fast_io::print_reserve_size(
	reserve_tag<deterministic_semantic>,
	::std::declval<deterministic_semantic>()))));
static_assert(throws((::fast_io::print_reserve_define(
	reserve_tag<deterministic_semantic>, ::std::declval<char *>(),
	::std::declval<deterministic_semantic>()))));
static_assert(throws((::fast_io::print_reserve_precise_size(
	reserve_tag<deterministic_semantic>,
	::std::declval<deterministic_semantic>()))));
static_assert(throws((::fast_io::print_reserve_precise_define(
	reserve_tag<deterministic_semantic>, ::std::declval<char *>(), 1u,
	::std::declval<deterministic_semantic>()))));

static_assert(!throws((::fast_io::print_reserve_size(
	reserve_tag<safe_radix>, ::std::declval<safe_radix>()))));
static_assert(!throws((::fast_io::print_reserve_define(
	reserve_tag<safe_radix>, ::std::declval<char *>(),
	::std::declval<safe_radix>()))));
static_assert(!throws((::fast_io::print_reserve_precise_size(
	reserve_tag<safe_radix>, ::std::declval<safe_radix>()))));
static_assert(!throws((::fast_io::print_reserve_precise_define(
	reserve_tag<safe_radix>, ::std::declval<char *>(), 1u,
	::std::declval<safe_radix>()))));
static_assert(!throws((::fast_io::print_define_internal_shift(
	reserve_tag<safe_radix>, ::std::declval<safe_radix>()))));

static_assert(throws((::fast_io::print_reserve_size(
	reserve_tag<deterministic_radix>,
	::std::declval<deterministic_radix>()))));
static_assert(throws((::fast_io::print_reserve_define(
	reserve_tag<deterministic_radix>, ::std::declval<char *>(),
	::std::declval<deterministic_radix>()))));
static_assert(throws((::fast_io::print_reserve_precise_size(
	reserve_tag<deterministic_radix>,
	::std::declval<deterministic_radix>()))));
static_assert(throws((::fast_io::print_reserve_precise_define(
	reserve_tag<deterministic_radix>, ::std::declval<char *>(), 1u,
	::std::declval<deterministic_radix>()))));
static_assert(throws((::fast_io::print_define_internal_shift(
	reserve_tag<deterministic_radix>,
	::std::declval<deterministic_radix>()))));

static_assert(throws((::fast_io::print_reserve_define(
	reserve_tag<named_category_semantic>, ::std::declval<char *>(),
	::std::declval<named_category_semantic>()))));
static_assert(throws((::fast_io::print_reserve_precise_define(
	reserve_tag<named_category_semantic>, ::std::declval<char *>(), 1u,
	::std::declval<named_category_semantic>()))));
static_assert(throws((::fast_io::print_reserve_define(
	reserve_tag<named_category_radix>, ::std::declval<char *>(),
	::std::declval<named_category_radix>()))));
static_assert(throws((::fast_io::print_reserve_precise_define(
	reserve_tag<named_category_radix>, ::std::declval<char *>(), 1u,
	::std::declval<named_category_radix>()))));
#else
static_assert(noexcept(::fast_io::print_reserve_define(
	reserve_tag<safe_semantic>, ::std::declval<char *>(),
	::std::declval<safe_semantic>())));
static_assert(noexcept(::fast_io::print_reserve_define(
	reserve_tag<deterministic_semantic>, ::std::declval<char *>(),
	::std::declval<deterministic_semantic>())));
static_assert(noexcept(::fast_io::print_reserve_precise_define(
	reserve_tag<safe_radix>, ::std::declval<char *>(), 1u,
	::std::declval<safe_radix>())));
static_assert(noexcept(::fast_io::print_reserve_precise_define(
	reserve_tag<deterministic_radix>, ::std::declval<char *>(), 1u,
	::std::declval<deterministic_radix>())));
#endif

} // namespace herbceptions_format_effect_contract

int main()
{
#if defined(__HERBCEPTIONS__)
	using namespace herbceptions_format_effect_contract;
	deterministic_radix value{{true}, true};
	try
	{
		(void)::fast_io::print_reserve_size(
			reserve_tag<deterministic_radix>, value);
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
