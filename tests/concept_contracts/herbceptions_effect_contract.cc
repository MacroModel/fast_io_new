/*
This translation unit is compiled in both ordinary and Herbception modes.  It
guards three independent contracts: public headers restore user macros, exact
wrappers retain their existing conditional-noexcept behavior on standard
compilers, and a Herbception-capable CPO cannot enter a strategy which promises
that no failure channel exists.
*/

#define FAST_IO_HERBCEPTIONS_THROWS 701
#define FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(condition) 704
#define FAST_IO_HERBCEPTIONS_NOTHROWS(expression) 705
#define FAST_IO_HERBCEPTIONS_NOEXCEPT(expression) 706

#include <fast_io_core.h>

static_assert(FAST_IO_HERBCEPTIONS_THROWS == 701);
static_assert(FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(true) == 704);
static_assert(FAST_IO_HERBCEPTIONS_NOTHROWS(0) == 705);
static_assert(FAST_IO_HERBCEPTIONS_NOEXCEPT(0) == 706);

#undef FAST_IO_HERBCEPTIONS_THROWS
#undef FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT
#undef FAST_IO_HERBCEPTIONS_NOTHROWS
#undef FAST_IO_HERBCEPTIONS_NOEXCEPT

#include "fast_io_dsal/impl/misc/push_macros.h"

namespace herbceptions_effect_contract
{

inline void ordinary_nothrow_source() noexcept
{
}

inline void deterministic_source() FAST_IO_HERBCEPTIONS_THROWS
{
}

static_assert(FAST_IO_HERBCEPTIONS_NOEXCEPT(ordinary_nothrow_source()));
static_assert(!FAST_IO_HERBCEPTIONS_NOEXCEPT(deterministic_source()));
static_assert(FAST_IO_HERBCEPTIONS_NOTHROWS(ordinary_nothrow_source()));
#if defined(__HERBCEPTIONS__)
static_assert(!FAST_IO_HERBCEPTIONS_NOTHROWS(deterministic_source()));
#endif

struct alias_value
{
	int payload{};
};

struct safe_alias_source
{
};

inline alias_value print_alias_define(::fast_io::io_alias_t, safe_alias_source &) noexcept
{
	return {11};
}

struct legacy_throwing_alias_source
{
};

// A customization may use the traditional C++ exception channel without a deterministic Herbception result.  The
// wrapper must still select active basic-throws in Herbception mode so that an escaping exception can be converted to
// std::error; throws(false) would instead promise the same no-failure ABI as noexcept(true).
inline alias_value print_alias_define(
	::fast_io::io_alias_t, legacy_throwing_alias_source &) noexcept(false)
{
	return {13};
}

struct deterministic_alias_source
{
};

inline alias_value print_alias_define(
	::fast_io::io_alias_t, deterministic_alias_source &) FAST_IO_HERBCEPTIONS_THROWS
{
	return {17};
}

struct deterministic_reference_alias_source
{
};

inline alias_value &print_alias_define(
	::fast_io::io_alias_t, deterministic_reference_alias_source &) FAST_IO_HERBCEPTIONS_THROWS
{
	static alias_value value{19};
	return value;
}

struct deterministic_rvalue_reference_alias_source
{
};

inline alias_value &&print_alias_define(
	::fast_io::io_alias_t, deterministic_rvalue_reference_alias_source &)
	FAST_IO_HERBCEPTIONS_THROWS
{
	static alias_value value{29};
	return static_cast<alias_value &&>(value);
}

struct deterministic_rvalue_reference_scan_alias_source
{
};

inline alias_value &&scan_alias_define(
	::fast_io::io_alias_t, deterministic_rvalue_reference_scan_alias_source &)
	FAST_IO_HERBCEPTIONS_THROWS
{
	static alias_value value{31};
	return static_cast<alias_value &&>(value);
}

struct output_observer
{
	using output_char_type = char;
	int state{};
};

struct safe_output_handle
{
	output_observer observer{};
};

inline output_observer &output_stream_ref_define(safe_output_handle &handle) noexcept
{
	return handle.observer;
}

struct deterministic_value_output_handle
{
};

inline output_observer output_stream_ref_define(
	deterministic_value_output_handle &) FAST_IO_HERBCEPTIONS_THROWS
{
	return {23};
}

struct deterministic_reference_output_handle
{
	output_observer observer{};
};

inline output_observer &output_stream_ref_define(
	deterministic_reference_output_handle &handle) FAST_IO_HERBCEPTIONS_THROWS
{
	return handle.observer;
}

struct deterministic_const_reference_output_handle
{
	output_observer observer{};
};

inline output_observer const &output_stream_ref_define(
	deterministic_const_reference_output_handle &handle) FAST_IO_HERBCEPTIONS_THROWS
{
	return handle.observer;
}

#if defined(__HERBCEPTIONS__)
// Deterministic failure is orthogonal to the success category: value aliases use a value payload while lvalue and
// xvalue aliases retain their exact identities in the canonical throws function type.
static_assert(!throws(::fast_io::io_print_alias(::std::declval<safe_alias_source &>())));
static_assert(throws(::fast_io::io_print_alias(::std::declval<legacy_throwing_alias_source &>())));
static_assert(throws(::fast_io::io_print_alias(::std::declval<deterministic_alias_source &>())));
static_assert(::fast_io::details::io_print_alias_admissible<deterministic_reference_alias_source &>);
static_assert(::fast_io::details::io_print_alias_admissible<
			  deterministic_rvalue_reference_alias_source &>);
static_assert(::fast_io::details::io_scan_alias_admissible<
			  deterministic_rvalue_reference_scan_alias_source &>);
static_assert(::std::same_as<
	decltype(::fast_io::io_print_alias(
		::std::declval<deterministic_reference_alias_source &>())),
	alias_value &>);
static_assert(::std::same_as<
	decltype(::fast_io::io_print_alias(
		::std::declval<deterministic_rvalue_reference_alias_source &>())),
	alias_value &&>);
static_assert(::std::same_as<
	decltype(::fast_io::io_scan_alias(
		::std::declval<deterministic_rvalue_reference_scan_alias_source &>())),
	alias_value &&>);
static_assert(throws((::fast_io::io_print_alias(
	::std::declval<deterministic_reference_alias_source &>()))));
static_assert(throws((::fast_io::io_print_alias(
	::std::declval<deterministic_rvalue_reference_alias_source &>()))));
static_assert(throws((::fast_io::io_scan_alias(
	::std::declval<deterministic_rvalue_reference_scan_alias_source &>()))));
using safe_alias_function = alias_value (*)(safe_alias_source &) noexcept;
using legacy_throwing_alias_function = alias_value (*)(legacy_throwing_alias_source &) throws;
using deterministic_alias_function = alias_value (*)(deterministic_alias_source &) throws;
static_assert(::std::same_as<
	decltype(&::fast_io::io_print_alias<safe_alias_source &>), safe_alias_function>);
static_assert(::std::same_as<
	decltype(&::fast_io::io_print_alias<legacy_throwing_alias_source &>),
	legacy_throwing_alias_function>);
static_assert(::std::same_as<
	decltype(&::fast_io::io_print_alias<deterministic_alias_source &>), deterministic_alias_function>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::output_stream_ref(::std::declval<safe_output_handle &>())),
	output_observer &>);
static_assert(!throws(::fast_io::operations::output_stream_ref(
	::std::declval<safe_output_handle &>())));
static_assert(throws(::fast_io::operations::output_stream_ref(
	::std::declval<deterministic_value_output_handle &>())));
using safe_output_ref_function = output_observer &(*)(safe_output_handle &) noexcept;
using deterministic_output_ref_function = output_observer (*)(deterministic_value_output_handle &) throws;
static_assert(::std::same_as<
	decltype(&::fast_io::operations::output_stream_ref<safe_output_handle &>), safe_output_ref_function>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::output_stream_ref<deterministic_value_output_handle &>),
	deterministic_output_ref_function>);
static_assert(::fast_io::operations::defines::has_output_stream_ref_define<
	deterministic_reference_output_handle &>);
static_assert(::fast_io::operations::defines::has_output_stream_ref_define<
	deterministic_const_reference_output_handle &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::output_stream_ref(
		::std::declval<deterministic_reference_output_handle &>())),
	output_observer &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::output_stream_ref(
		::std::declval<deterministic_const_reference_output_handle &>())),
	output_observer>);
static_assert(throws((::fast_io::operations::output_stream_ref(
	::std::declval<deterministic_reference_output_handle &>()))));
static_assert(throws((::fast_io::operations::output_stream_ref(
	::std::declval<deterministic_const_reference_output_handle &>()))));
#else
static_assert(noexcept(::fast_io::io_print_alias(::std::declval<safe_alias_source &>())));
static_assert(!noexcept(::fast_io::io_print_alias(
	::std::declval<legacy_throwing_alias_source &>())));
static_assert(::std::same_as<
	decltype(::fast_io::operations::output_stream_ref(::std::declval<safe_output_handle &>())),
	output_observer &>);
static_assert(noexcept(::fast_io::operations::output_stream_ref(
	::std::declval<safe_output_handle &>())));
#endif

struct safe_precise
{
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, safe_precise>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, safe_precise>, char *iter,
	safe_precise &) noexcept
{
	*iter = 's';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, safe_precise>, safe_precise &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, safe_precise>, char *iter,
	::std::size_t, safe_precise &) noexcept
{
	*iter = 's';
	return iter + 1;
}

struct deterministic_precise
{
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_precise>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_precise>, char *iter,
	deterministic_precise &) noexcept
{
	*iter = 'd';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, deterministic_precise>,
	deterministic_precise &) noexcept
{
	return 1u;
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, deterministic_precise>, char *iter,
	::std::size_t, deterministic_precise &) FAST_IO_HERBCEPTIONS_THROWS
{
	*iter = 'd';
	return iter + 1;
}

static_assert(::fast_io::nothrow_precise_reserve_printable<char, safe_precise>);
static_assert(::fast_io::precise_reserve_printable<char, deterministic_precise>);
static_assert(!::fast_io::nothrow_precise_reserve_printable<char, deterministic_precise>);

struct deterministic_precise_size
{
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, deterministic_precise_size>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, deterministic_precise_size>, char *iter,
	deterministic_precise_size &) noexcept
{
	*iter = 'z';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, deterministic_precise_size>,
	deterministic_precise_size &) FAST_IO_HERBCEPTIONS_THROWS
{
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, deterministic_precise_size>, char *iter,
	::std::size_t, deterministic_precise_size &) noexcept
{
	*iter = 'z';
	return iter + 1;
}

using safe_precise_parameter = ::fast_io::parameter<safe_precise &>;
using deterministic_precise_parameter = ::fast_io::parameter<deterministic_precise &>;
using deterministic_precise_size_parameter = ::fast_io::parameter<deterministic_precise_size &>;

static_assert(noexcept(print_reserve_precise_size(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<safe_precise_parameter &>())));
static_assert(noexcept(print_reserve_precise_size(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<safe_precise_parameter const &>())));
static_assert(noexcept(print_reserve_precise_define(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<safe_precise_parameter &>())));
static_assert(noexcept(print_reserve_precise_define(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<safe_precise_parameter const &>())));

#if defined(__HERBCEPTIONS__)
// A transparent parameter must keep its plain ABI only when the exact delegated CPO has no deterministic channel.
static_assert(!throws((print_reserve_precise_size(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<safe_precise_parameter &>()))));
static_assert(!throws((print_reserve_precise_size(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<safe_precise_parameter const &>()))));
static_assert(throws((print_reserve_precise_size(
	::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>,
	::std::declval<deterministic_precise_size_parameter &>()))));
static_assert(throws((print_reserve_precise_size(
	::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>,
	::std::declval<deterministic_precise_size_parameter const &>()))));
static_assert(!throws((print_reserve_precise_define(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<safe_precise_parameter &>()))));
static_assert(!throws((print_reserve_precise_define(
	::fast_io::io_reserve_type<char, safe_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<safe_precise_parameter const &>()))));
static_assert(throws((print_reserve_precise_define(
	::fast_io::io_reserve_type<char, deterministic_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<deterministic_precise_parameter &>()))));
static_assert(throws((print_reserve_precise_define(
	::fast_io::io_reserve_type<char, deterministic_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<deterministic_precise_parameter const &>()))));
#else
// Standard builds retain the child expressions' original conditional-noexcept behavior for both wrapper cv variants.
static_assert(!noexcept(print_reserve_precise_size(
	::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>,
	::std::declval<deterministic_precise_size_parameter &>())));
static_assert(!noexcept(print_reserve_precise_size(
	::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>,
	::std::declval<deterministic_precise_size_parameter const &>())));
static_assert(!noexcept(print_reserve_precise_define(
	::fast_io::io_reserve_type<char, deterministic_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<deterministic_precise_parameter &>())));
static_assert(!noexcept(print_reserve_precise_define(
	::fast_io::io_reserve_type<char, deterministic_precise_parameter>,
	::std::declval<char *>(), 1u, ::std::declval<deterministic_precise_parameter const &>())));
#endif

::std::size_t exercise_safe_parameter_precise_size(
	safe_precise_parameter &value) noexcept
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char, safe_precise_parameter>, value);
}

::std::size_t exercise_safe_const_parameter_precise_size(
	safe_precise_parameter const &value) noexcept
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char, safe_precise_parameter>, value);
}

char *exercise_safe_parameter_precise_define(
	char *iter, safe_precise_parameter &value) noexcept
{
	return print_reserve_precise_define(
		::fast_io::io_reserve_type<char, safe_precise_parameter>, iter, 1u, value);
}

char *exercise_safe_const_parameter_precise_define(
	char *iter, safe_precise_parameter const &value) noexcept
{
	return print_reserve_precise_define(
		::fast_io::io_reserve_type<char, safe_precise_parameter>, iter, 1u, value);
}

::std::size_t exercise_deterministic_parameter_precise_size(
	deterministic_precise_size_parameter &value) FAST_IO_HERBCEPTIONS_THROWS
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>, value);
}

::std::size_t exercise_deterministic_const_parameter_precise_size(
	deterministic_precise_size_parameter const &value) FAST_IO_HERBCEPTIONS_THROWS
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char, deterministic_precise_size_parameter>, value);
}

char *exercise_deterministic_parameter_precise_define(
	char *iter, deterministic_precise_parameter &value) FAST_IO_HERBCEPTIONS_THROWS
{
	return print_reserve_precise_define(
		::fast_io::io_reserve_type<char, deterministic_precise_parameter>, iter, 1u, value);
}

char *exercise_deterministic_const_parameter_precise_define(
	char *iter, deterministic_precise_parameter const &value) FAST_IO_HERBCEPTIONS_THROWS
{
	return print_reserve_precise_define(
		::fast_io::io_reserve_type<char, deterministic_precise_parameter>, iter, 1u, value);
}

struct deterministic_staged
{
};

struct staged_state
{
};

inline constexpr ::fast_io::io_type_t<staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, deterministic_staged>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, deterministic_staged>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, deterministic_staged>,
	deterministic_staged const &) noexcept
{
	return true;
}

inline staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, deterministic_staged>,
	deterministic_staged const &) FAST_IO_HERBCEPTIONS_THROWS
{
	return {};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, deterministic_staged>, char *iter,
	deterministic_staged const &, staged_state const &) noexcept
{
	return iter;
}

// Traditional noexcept alone reports true for a Herbception function in the
// reference compiler.  The concept must nevertheless reject that producer.
static_assert(!::fast_io::staged_printable<char, deterministic_staged>);

struct safe_copy_iterator
{
	using value_type = int;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;

	int *current{};

	constexpr safe_copy_iterator() noexcept = default;
	constexpr safe_copy_iterator(safe_copy_iterator const &) noexcept = default;
	constexpr safe_copy_iterator &operator=(safe_copy_iterator const &) noexcept = default;
	[[nodiscard]] constexpr int &operator*() const noexcept
	{
		return *current;
	}
	constexpr safe_copy_iterator &operator++() noexcept
	{
		++current;
		return *this;
	}
	constexpr void operator++(int) noexcept
	{
		++*this;
	}
	friend constexpr bool operator==(safe_copy_iterator const &, safe_copy_iterator const &) noexcept = default;
};

struct deterministic_mutable_copy_iterator
{
	using value_type = int;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;

	int *current{};

	constexpr deterministic_mutable_copy_iterator() noexcept = default;
	constexpr deterministic_mutable_copy_iterator(
		deterministic_mutable_copy_iterator const &other) noexcept : current(other.current)
	{}
	constexpr deterministic_mutable_copy_iterator(
		deterministic_mutable_copy_iterator &other) FAST_IO_HERBCEPTIONS_THROWS : current(other.current)
	{}
	constexpr deterministic_mutable_copy_iterator &operator=(
		deterministic_mutable_copy_iterator const &) noexcept = default;
	[[nodiscard]] constexpr int &operator*() const noexcept
	{
		return *current;
	}
	constexpr deterministic_mutable_copy_iterator &operator++() noexcept
	{
		++current;
		return *this;
	}
	constexpr void operator++(int) noexcept
	{
		++*this;
	}
	friend constexpr bool operator==(
		deterministic_mutable_copy_iterator const &,
		deterministic_mutable_copy_iterator const &) noexcept = default;
};

struct deterministic_const_copy_iterator
{
	using value_type = int;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;

	int *current{};

	constexpr deterministic_const_copy_iterator() noexcept = default;
	constexpr deterministic_const_copy_iterator(
		deterministic_const_copy_iterator &other) noexcept : current(other.current)
	{}
	constexpr deterministic_const_copy_iterator(
		deterministic_const_copy_iterator const &other) FAST_IO_HERBCEPTIONS_THROWS : current(other.current)
	{}
	constexpr deterministic_const_copy_iterator &operator=(
		deterministic_const_copy_iterator const &) noexcept = default;
	[[nodiscard]] constexpr int &operator*() const noexcept
	{
		return *current;
	}
	constexpr deterministic_const_copy_iterator &operator++() noexcept
	{
		++current;
		return *this;
	}
	constexpr void operator++(int) noexcept
	{
		++*this;
	}
	friend constexpr bool operator==(
		deterministic_const_copy_iterator const &,
		deterministic_const_copy_iterator const &) noexcept = default;
};

static_assert(::std::input_iterator<safe_copy_iterator>);
static_assert(::std::input_iterator<deterministic_mutable_copy_iterator>);
static_assert(::std::input_iterator<deterministic_const_copy_iterator>);
static_assert(::fast_io::sized_range_view_nothrow_reserve_define_v<char, safe_copy_iterator>);
static_assert(!::fast_io::sized_range_view_nothrow_reserve_define_v<
			  char, deterministic_mutable_copy_iterator>);
static_assert(!::fast_io::sized_range_view_nothrow_reserve_define_v<
			  char, deterministic_const_copy_iterator>);

#if defined(__HERBCEPTIONS__)
// Active `throws` behaves as `noexcept(false)` in standard nothrow queries even though its implementation is
// nounwind.  The dedicated trait additionally proves that the deterministic channel, rather than legacy unwinding,
// is the reason each construction is not nothrow.
static_assert(!::std::is_nothrow_constructible_v<
			  deterministic_mutable_copy_iterator, deterministic_mutable_copy_iterator &>);
static_assert(::std::is_herbceptions_throws_constructible_v<
			  deterministic_mutable_copy_iterator, deterministic_mutable_copy_iterator &>);
static_assert(!::std::is_nothrow_constructible_v<
			  deterministic_const_copy_iterator, deterministic_const_copy_iterator const &>);
static_assert(::std::is_herbceptions_throws_constructible_v<
			  deterministic_const_copy_iterator, deterministic_const_copy_iterator const &>);
#endif

} // namespace herbceptions_effect_contract

#include "fast_io_dsal/impl/misc/pop_macros.h"

int main()
{
}
