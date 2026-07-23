#include <fast_io_format.h>

#include <cstddef>
#include <type_traits>

namespace valid_static_output_provider
{

struct value_type
{
	unsigned value{};
};

// Provider CPOs are ordinary constexpr functions. The library-owned consteval
// wrapper, rather than a non-portable consteval provider signature, proves that
// the selected invocation is usable during static replacement construction.
template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return 2u;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr char *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &value) noexcept
{
	*output++ = static_cast<char>('0' + value.value / 10u);
	*output++ = static_cast<char>('0' + value.value % 10u);
	return output;
}

} // namespace valid_static_output_provider

namespace malformed_static_output_provider
{

struct value_type
{};

// A truth-valued size and pointer-to-const endpoint are intentionally close to
// the protocol spelling but violate both exact result-type requirements.
template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr bool format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return true;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] constexpr char const *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &) noexcept
{
	return output;
}

} // namespace malformed_static_output_provider

namespace
{

inline constexpr ::fast_io::fmt::details::format_specification<char>
	empty_specification{};
using context_type = ::fast_io::fmt::basic_static_format_context_t<
	empty_specification, 0u>;
using formatter_type = ::fast_io::fmt::basic_static_format_as_t<char>;

static_assert(
	::fast_io::fmt::details::static_format_output_adl::expression<
		context_type, formatter_type,
		valid_static_output_provider::value_type const &>);
static_assert(
	!::fast_io::fmt::details::static_format_output_adl::expression<
		context_type, formatter_type,
		malformed_static_output_provider::value_type const &>);

[[nodiscard]] consteval bool provider_round_trip()
{
	context_type context{};
	valid_static_output_provider::value_type const value{42u};
	char output[2u]{};
	auto const size{
		::fast_io::fmt::details::static_format_output_adl::size<
			context_type, formatter_type>(context, value)};
	auto const end{
		::fast_io::fmt::details::static_format_output_adl::define<
			context_type, formatter_type>(context, output, value)};
	return size == 2u && end == output + 2u && output[0u] == '4' &&
		output[1u] == '2';
}

static_assert(provider_round_trip());

} // namespace

int main() {}
