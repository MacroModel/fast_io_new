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
	decltype(::fast_io::mnp::static_arg<42>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::mnp::static_arg<3.14>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::mnp::static_arg<"text">)>);
static_assert(::fast_io::fmt::is_static_named_arg_v<
	decltype(::fast_io::mnp::static_arg<"name", 42>)>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
	decltype(::fast_io::mnp::static_arg<aggregate>)>);
static_assert(!nullary_callable<decltype(::fast_io::mnp::static_arg<42>)>);
static_assert(!unary_callable<decltype(::fast_io::mnp::static_arg<42>)>);

using scalar_static_arg_type = ::std::remove_cv_t<
	decltype(::fast_io::mnp::static_arg<42>)>;
using named_static_arg_type = ::std::remove_cv_t<
	decltype(::fast_io::mnp::static_arg<"value", 42>)>;
using scalar_materialized_type = decltype(
	::fast_io::manipulators::print_compiler_constant_materialize(
		::fast_io::io_reserve_type<char, scalar_static_arg_type>,
		scalar_static_arg_type{}));
static_assert(::std::is_empty_v<scalar_static_arg_type>);
static_assert(::std::is_empty_v<named_static_arg_type>);
static_assert(::std::is_empty_v<scalar_materialized_type>);
static_assert(::std::is_trivially_copyable_v<scalar_static_arg_type>);
static_assert(::std::is_trivially_copyable_v<named_static_arg_type>);
static_assert(::std::is_trivially_copyable_v<scalar_materialized_type>);

consteval auto render_raw_static_arguments()
{
	::std::array<char, 7u> result{};
	::fast_io::obuffer_view output{result};
	::fast_io::io::print(
		output, ::fast_io::mnp::static_arg<42>, ":",
		::fast_io::mnp::static_arg<"text">);
	return result;
}

inline constexpr auto raw_static_arguments{render_raw_static_arguments()};
static_assert(::std::string_view{
	raw_static_arguments.data(), raw_static_arguments.size()} ==
	"42:text");

int main()
{
	auto const raw_concat{
		::fast_io::concat_std(::fast_io::mnp::static_arg<42>)};
	if (raw_concat != "42")
	{
		return 1;
	}
	auto const formatted{
		::fast_io::fmt::concat_std<"{value}">(
			::fast_io::mnp::static_arg<"value", aggregate>)};
	return formatted == "[1, 2, 3]" ? 0 : 2;
}
