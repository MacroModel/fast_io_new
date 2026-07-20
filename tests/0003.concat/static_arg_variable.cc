#include <fast_io.h>
#include <fast_io_format.h>
#include <fast_io_unit/string.h>

#include <array>
#include <string_view>
#include <type_traits>

inline constexpr ::std::array aggregate{1u, 2u, 3u};

template <typename T>
concept nullary_callable = requires(T value) { value(); };

template <typename T>
concept unary_callable = requires(T value, int runtime) { value(runtime); };

static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::fmt::static_arg<42>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::fmt::static_arg<3.14>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::fmt::static_arg<"text">)>);
static_assert(::fast_io::fmt::is_static_named_arg_v<
	decltype(::fast_io::fmt::static_arg<"name", 42>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::fmt::static_arg<aggregate>)>);
static_assert(!nullary_callable<decltype(::fast_io::fmt::static_arg<42>)>);
static_assert(!unary_callable<decltype(::fast_io::fmt::static_arg<42>)>);

consteval auto render_raw_static_arguments()
{
	::std::array<char, 17u> result{};
	::fast_io::obuffer_view output{result};
	::fast_io::io::print(
		output, ::fast_io::fmt::static_arg<42>, ":",
		::fast_io::fmt::static_arg<"text">, ":",
		::fast_io::fmt::static_arg<aggregate>);
	return result;
}

inline constexpr auto raw_static_arguments{render_raw_static_arguments()};
static_assert(::std::string_view{
	raw_static_arguments.data(), raw_static_arguments.size()} ==
	"42:text:[1, 2, 3]");

int main()
{
	auto const raw_concat{
		::fast_io::concat_std(::fast_io::fmt::static_arg<42>)};
	if (raw_concat != "42")
	{
		return 1;
	}
	auto const formatted{
		::fast_io::fmt::concat_std<"{value}">(
			::fast_io::fmt::static_arg<"value", aggregate>)};
	return formatted == "[1, 2, 3]" ? 0 : 2;
}
