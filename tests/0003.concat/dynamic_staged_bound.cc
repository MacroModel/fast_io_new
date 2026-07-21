#include <fast_io.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dynamic_staged_bound_test
{

struct portable_staged_state
{
	::std::size_t digit{};
};

struct portable_staged_value
{
	::std::size_t digit{};
};

template <::std::integral char_type>
inline constexpr auto print_staged_type(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>) noexcept
{
	return ::fast_io::io_type_t<portable_staged_state>{};
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>) noexcept
{
	return 2u;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_staged_max_count(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>) noexcept
{
	return 2u;
}

template <::std::integral char_type>
inline constexpr bool print_staged_fallback_inline(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>) noexcept
{
	return true;
}

template <::std::integral char_type>
inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>,
	portable_staged_value const &) noexcept
{
	return true;
}

template <::std::integral char_type>
inline constexpr portable_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>,
	portable_staged_value const &value) noexcept
{
	return {value.digit};
}

template <::std::integral char_type>
inline constexpr char_type *print_staged_define(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>,
	char_type *iter, portable_staged_value const &,
	portable_staged_state const &state) noexcept
{
	*iter = static_cast<char_type>('0' + state.digit % 10u);
	return iter + 1;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, portable_staged_value>,
	char_type *iter, portable_staged_value const &value) noexcept
{
	*iter = static_cast<char_type>('0' + value.digit % 10u);
	return iter + 1;
}

using normalized_dynamic_string = ::std::remove_cvref_t<decltype(
	::fast_io::io_print_forward<char>(::fast_io::io_print_alias(
		::std::declval<::std::string_view &>())))>;
using normalized_portable_staged = ::std::remove_cvref_t<decltype(
	::fast_io::io_print_forward<char>(::fast_io::io_print_alias(
		::std::declval<portable_staged_value &>())))>;
using dynamic_staged_group =
	::fast_io::operations::decay::print_semantic_staged_group<
		char, normalized_dynamic_string, normalized_portable_staged,
		normalized_portable_staged>;
static_assert(dynamic_staged_group::available);
static_assert(
	::fast_io::operations::decay::print_semantic_static_bounded_total_size<
		false, char, normalized_dynamic_string, normalized_portable_staged,
		normalized_portable_staged>() == SIZE_MAX);

[[nodiscard]] bool outputs_match()
{
	::std::string_view const text{"dynamic:"};
	portable_staged_value const first{1u};
	portable_staged_value const second{2u};
	::std::array<char, 32u> format_storage{};
	::std::array<char, 32u> raw_storage{};
	::fast_io::obuffer_view format_output{format_storage};
	::fast_io::obuffer_view raw_output{raw_storage};
	::fast_io::fmt::print<"{}{}{}">(
		format_output, text, first, second);
	::fast_io::io::print(raw_output, text, first, second);
	constexpr ::std::string_view expected{"dynamic:12"};
	return ::std::string_view{format_output.data(), format_output.size()} ==
			   expected &&
		   ::std::string_view{raw_output.data(), raw_output.size()} == expected;
}

} // namespace dynamic_staged_bound_test

int main()
{
	return dynamic_staged_bound_test::outputs_match() ? 0 : 1;
}
