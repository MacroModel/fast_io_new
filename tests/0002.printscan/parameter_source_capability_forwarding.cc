#include <fast_io.h>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace
{

struct staging_source
{
	/// @brief Makes the test source non-trivial so normalization must preserve its object identity.
	~staging_source()
	{}
};

/// @brief Supplies the dynamic print protocol required by the staging capability test.
template <typename output>
inline void print_define(::fast_io::io_reserve_type_t<char, staging_source>,
						 output &, staging_source &)
{
}

/// @brief Certifies the test source's explicit one-pass staging contract.
inline constexpr ::std::true_type print_single_pass_staging_safe(
	::fast_io::io_reserve_type_t<char, staging_source>) noexcept
{
	return {};
}

struct bounded_source
{
	::std::size_t size{};
	/// @brief Keeps the bounded test source non-trivial across parameter transport.
	~bounded_source()
	{}
};

/// @brief Selects bounded one-pass materialization for the dedicated test source.
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char, bounded_source>) noexcept
{
	return {};
}

/// @brief Reports the test source's size only when it fits the caller's stated bound.
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, bounded_source>,
	bounded_source const &value, ::std::size_t maximum_size) noexcept
{
	return value.size <= maximum_size ? value.size : SIZE_MAX;
}

struct scatter_source
{
	char const *data{};
	::std::size_t size{};
	/// @brief Keeps the scatter source non-trivial so its retained lifetime proof is exercised.
	~scatter_source()
	{}
};

/// @brief Exposes the test source as one borrowed scatter descriptor.
inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, scatter_source>,
	scatter_source &value) noexcept
{
	return {value.data, value.size};
}

/// @brief Certifies the borrowed lifetime of the test scatter source.
inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, scatter_source>) noexcept
{
	return {};
}

/// @brief Certifies that the test descriptor does not depend on a destination cursor.
inline constexpr ::std::true_type print_scatter_output_state_independent(
	::fast_io::io_reserve_type_t<char, scatter_source>) noexcept
{
	return {};
}

/// @brief Certifies that retained scatter and direct emission have identical test semantics.
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	::fast_io::io_reserve_type_t<char, scatter_source>) noexcept
{
	return {};
}

using normalized_staging = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<staging_source &>())));
using normalized_bounded = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<bounded_source &>())));
using normalized_scatter = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<scatter_source &>())));

static_assert(::std::same_as<normalized_staging,
							 ::fast_io::parameter<staging_source &>>);
static_assert(::std::same_as<normalized_bounded,
							 ::fast_io::parameter<bounded_source &>>);
static_assert(::std::same_as<normalized_scatter,
							 ::fast_io::parameter<scatter_source &>>);

// `parameter<T>` changes only argument transport. These four source-authored
// proofs must therefore survive normalization without being manufactured for
// types that did not provide the corresponding proof themselves.
static_assert(::fast_io::single_pass_staging_printable<
			  char, normalized_staging &>);
static_assert(::fast_io::single_pass_bounded_materialization_source<
			  char, normalized_bounded &>);
static_assert(::fast_io::scatter_output_state_independent<
			  char, normalized_scatter>);
static_assert(::fast_io::scatter_direct_print_equivalent<
			  char, normalized_scatter>);

} // namespace

/// @brief Instantiates the compile-time capability assertions in a complete executable translation unit.
int main()
{}
