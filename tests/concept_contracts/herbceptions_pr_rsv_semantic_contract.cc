/*
This contract isolates two normalization boundaries which are intentionally
below the public print facade. It verifies that ordinary C++ keeps the existing
conditional-noexcept ABI, while Herbception builds select a plain ABI for safe
customizations and an error-result ABI for fallible value-returning CPOs.
*/

#include <array>
#include <fast_io.h>

#include "fast_io_dsal/impl/misc/push_macros.h"

namespace herbceptions_pr_rsv_semantic_contract
{

struct reserve_value
{
	char value{'v'};
	unsigned *calls{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, reserve_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, reserve_value>, char *iter,
	reserve_value &value) noexcept
{
	*iter = value.value;
	if (value.calls != nullptr)
	{
		++*value.calls;
	}
	return iter + 1;
}

struct safe_alias_source
{
	unsigned *calls{};
};

inline constexpr reserve_value print_alias_define(
	::fast_io::io_alias_t, safe_alias_source &source) noexcept
{
	return {'s', source.calls};
}

struct fallible_alias_source
{
	bool fail{};
};

struct fallible_alias_value
{
	char value{'a'};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fallible_alias_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fallible_alias_value>, char *iter,
	fallible_alias_value &value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

// Failure switches are observed only when the Herbception effect syntax is active; the ordinary branch intentionally
// retains the same source signature so it can verify the legacy ABI without generating fixture-only warnings.
inline fallible_alias_value print_alias_define(
	::fast_io::io_alias_t, [[maybe_unused]] fallible_alias_source &source)
	FAST_IO_HERBCEPTIONS_THROWS
{
#if defined(__HERBCEPTIONS__)
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return {'a'};
}

struct reference_value
{
	char value{'r'};
	unsigned calls{};

	reference_value() = default;
	reference_value(reference_value const &) = delete;
	reference_value &operator=(reference_value const &) = delete;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, reference_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, reference_value>, char *iter,
	reference_value &value) noexcept
{
	*iter = value.value;
	++value.calls;
	return iter + 1;
}

struct reference_alias_source
{
	reference_value value{};
};

inline constexpr reference_value &print_alias_define(
	::fast_io::io_alias_t, reference_alias_source &source) noexcept
{
	return source.value;
}

struct fallible_writer
{
	bool fail{};
	unsigned *calls{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fallible_writer>) noexcept
{
	return 1u;
}

struct immovable_reserve_value
{
	char value;

	inline explicit constexpr immovable_reserve_value(char ch) noexcept : value(ch) {}
	immovable_reserve_value(immovable_reserve_value const &) = delete;
	immovable_reserve_value(immovable_reserve_value &&) = delete;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, immovable_reserve_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, immovable_reserve_value>, char *iter,
	immovable_reserve_value &value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fallible_writer>, char *iter,
	fallible_writer &value) FAST_IO_HERBCEPTIONS_THROWS
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::io_error;
	}
#endif
	*iter = 'w';
	if (value.calls != nullptr)
	{
		++*value.calls;
	}
	return iter + 1;
}

struct semantic_value
{
	char value{'p'};
};

// A non-empty payload verifies value materialization across the semantic alias boundary.
using semantic_pack = decltype(::fast_io::mnp::pack(semantic_value{}));

struct safe_semantic_source
{};

inline constexpr semantic_pack print_alias_define(
	::fast_io::io_alias_t, safe_semantic_source &) noexcept
{
	return ::fast_io::mnp::pack(semantic_value{});
}

struct fallible_semantic_source
{
	bool fail{};
};

inline semantic_pack print_alias_define(
	::fast_io::io_alias_t, [[maybe_unused]] fallible_semantic_source &source)
	FAST_IO_HERBCEPTIONS_THROWS
{
#if defined(__HERBCEPTIONS__)
	if (source.fail)
	{
		throw throws ::std::errc::invalid_argument;
	}
#endif
	return ::fast_io::mnp::pack(semantic_value{});
}

struct status_alias
{
	bool fail{};
};

struct fallible_status_source
{
	bool fail{};
};

inline constexpr status_alias print_alias_define(
	::fast_io::io_alias_t, fallible_status_source &source) noexcept
{
	return {source.fail};
}

inline semantic_value status_io_print_forward(
	::fast_io::io_alias_type_t<char>, [[maybe_unused]] status_alias value)
	FAST_IO_HERBCEPTIONS_THROWS
{
#if defined(__HERBCEPTIONS__)
	if (value.fail)
	{
		throw throws ::std::errc::result_out_of_range;
	}
#endif
	return {'f'};
}

static_assert(::fast_io::pr_rsv_size<char, safe_alias_source &> == 1u);
static_assert(::fast_io::pr_rsv_size<char, fallible_alias_source &> == 1u);
static_assert(::fast_io::pr_rsv_size<char, reference_alias_source &> == 1u);
static_assert(::fast_io::pr_rsv_size<char, fallible_writer &> == 1u);
static_assert(::fast_io::pr_rsv_size<char, immovable_reserve_value> == 1u);

template <typename T>
concept public_alias_rvalue_available = requires(T &&value) {
	::fast_io::io_print_alias(::std::forward<T>(value));
};

// Reserve materialization historically accepts a non-aliased immovable temporary by naming its forwarding parameter.
// The public alias CPO cannot own that temporary, so routing the non-alias branch through it would be a regression.
static_assert(!public_alias_rvalue_available<immovable_reserve_value>);

using safe_unchecked_function = char *(*)(char *, safe_alias_source &) noexcept;
using fallible_unchecked_function = char *(*)(char *, fallible_alias_source &)FAST_IO_HERBCEPTIONS_THROWS;
using safe_c_array_function = char *(*)(char (&)[1], safe_alias_source &) noexcept;
using fallible_c_array_function = char *(*)(char (&)[1], fallible_alias_source &)FAST_IO_HERBCEPTIONS_THROWS;
using safe_array_function = ::std::array<char, 1>::iterator (*)(
	::std::array<char, 1> &, safe_alias_source &) noexcept;
using fallible_array_function = ::std::array<char, 1>::iterator (*)(
	::std::array<char, 1> &, fallible_alias_source &)
	FAST_IO_HERBCEPTIONS_THROWS;
using immovable_rvalue_function = char *(*)(char *, immovable_reserve_value &&) noexcept;

static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_iterator_unchecked<char *, safe_alias_source &>),
			  safe_unchecked_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_c_array<char, 1u, safe_alias_source &>),
			  safe_c_array_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_array<char, 1u, safe_alias_source &>),
			  safe_array_function>);
static_assert(::std::same_as<
	decltype(&::fast_io::pr_rsv_to_iterator_unchecked<char *, immovable_reserve_value>),
	immovable_rvalue_function>);

#if defined(__HERBCEPTIONS__)
static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_iterator_unchecked<char *, fallible_alias_source &>),
			  fallible_unchecked_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_c_array<char, 1u, fallible_alias_source &>),
			  fallible_c_array_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::pr_rsv_to_array<char, 1u, fallible_alias_source &>),
			  fallible_array_function>);
static_assert(!throws((::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<safe_alias_source &>()))));
static_assert(throws((::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<fallible_alias_source &>()))));
static_assert(throws((::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<fallible_writer &>()))));
static_assert(!throws((::fast_io::pr_rsv_to_c_array(
	::std::declval<char (&)[1]>(), ::std::declval<safe_alias_source &>()))));
static_assert(throws((::fast_io::pr_rsv_to_c_array(
	::std::declval<char (&)[1]>(), ::std::declval<fallible_alias_source &>()))));
static_assert(!throws((::fast_io::pr_rsv_to_array(
	::std::declval<::std::array<char, 1> &>(),
	::std::declval<safe_alias_source &>()))));
static_assert(throws((::fast_io::pr_rsv_to_array(
	::std::declval<::std::array<char, 1> &>(),
	::std::declval<fallible_alias_source &>()))));

static_assert(!throws((::fast_io::details::decay::print_semantic_input_forward<char>(
	::std::declval<safe_semantic_source &>()))));
static_assert(throws((::fast_io::details::decay::print_semantic_input_forward<char>(
	::std::declval<fallible_semantic_source &>()))));
static_assert(throws((::fast_io::details::decay::print_semantic_input_forward<char>(
	::std::declval<fallible_status_source &>()))));

using safe_semantic_function = semantic_pack (*)(safe_semantic_source &) noexcept;
using fallible_semantic_function = semantic_pack (*)(fallible_semantic_source &) throws;
using fallible_status_function = semantic_value (*)(fallible_status_source &) throws;
static_assert(::std::same_as<
			  decltype(&::fast_io::details::decay::print_semantic_input_forward<
					   char, safe_semantic_source &>),
			  safe_semantic_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::details::decay::print_semantic_input_forward<
					   char, fallible_semantic_source &>),
			  fallible_semantic_function>);
static_assert(::std::same_as<
			  decltype(&::fast_io::details::decay::print_semantic_input_forward<
					   char, fallible_status_source &>),
			  fallible_status_function>);
#else
static_assert(!noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<fallible_alias_source &>())));
static_assert(!noexcept(::fast_io::details::decay::print_semantic_input_forward<char>(
	::std::declval<fallible_semantic_source &>())));
#endif

// Keep each runtime boundary visible as an independent call site. Besides making failures attributable to one
// transaction, this prevents the contract itself from changing the ABI shape under whole-function inlining.
[[gnu::noinline]] inline bool verify_safe_runtime() noexcept
{
	unsigned calls{};
	safe_alias_source source{__builtin_addressof(calls)};
	char raw[1]{};
	auto const raw_end{::fast_io::pr_rsv_to_c_array(raw, source)};
	if (raw_end != raw + 1 || raw[0] != 's' || calls != 1u)
	{
		return false;
	}

	::std::array<char, 1> array{};
	auto const array_end{::fast_io::pr_rsv_to_array(array, source)};
	if (array_end != array.end() || array[0] != 's' || calls != 2u)
	{
		return false;
	}

	reference_alias_source reference{};
	char reference_buffer[1]{};
	auto const reference_end{
		::fast_io::pr_rsv_to_c_array(reference_buffer, reference)};
	if (reference_end != reference_buffer + 1 ||
		reference_buffer[0] != 'r' || reference.value.calls != 1u)
	{
		return false;
	}

	char immovable_buffer[1]{};
	auto const immovable_end{::fast_io::pr_rsv_to_iterator_unchecked(
		immovable_buffer, immovable_reserve_value{'i'})};
	return immovable_end == immovable_buffer + 1 && immovable_buffer[0] == 'i';
}

#if defined(__HERBCEPTIONS__)
[[gnu::noinline]] inline bool verify_fallible_alias_runtime() noexcept
{
	char buffer[1]{};
	fallible_alias_source alias{true};
	try
	{
		(void)::fast_io::pr_rsv_to_c_array(buffer, alias);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument;
	}
}

[[gnu::noinline]] inline bool verify_fallible_writer_runtime() noexcept
{
	char buffer[1]{};
	fallible_writer writer{true, nullptr};
	try
	{
		(void)::fast_io::pr_rsv_to_iterator_unchecked(buffer, writer);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::io_error;
	}
}

[[gnu::noinline]] inline bool verify_fallible_semantic_runtime() noexcept
{
	fallible_semantic_source semantic{true};
	try
	{
		(void)::fast_io::details::decay::print_semantic_input_forward<char>(semantic);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::invalid_argument;
	}
}

[[gnu::noinline]] inline bool verify_fallible_status_runtime() noexcept
{
	fallible_status_source status{true};
	try
	{
		(void)::fast_io::details::decay::print_semantic_input_forward<char>(status);
		return false;
	}
	catch throws(::std::error error)
	{
		return error == ::std::errc::result_out_of_range;
	}
}
#endif

} // namespace herbceptions_pr_rsv_semantic_contract

#include "fast_io_dsal/impl/misc/pop_macros.h"

int main()
{
	if (!::herbceptions_pr_rsv_semantic_contract::verify_safe_runtime())
	{
		return 1;
	}
#if defined(__HERBCEPTIONS__)
	if (!::herbceptions_pr_rsv_semantic_contract::verify_fallible_alias_runtime() ||
		!::herbceptions_pr_rsv_semantic_contract::verify_fallible_writer_runtime() ||
		!::herbceptions_pr_rsv_semantic_contract::verify_fallible_semantic_runtime() ||
		!::herbceptions_pr_rsv_semantic_contract::verify_fallible_status_runtime())
	{
		return 2;
	}
#endif
	return 0;
}
