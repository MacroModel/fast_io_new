#include <fast_io_format.h>

#include <cstddef>

namespace argument_validation_dispatch_contract
{

struct source
{
	int value;
};

inline constexpr int print_alias_define(
	::fast_io::io_alias_t, source &value) noexcept
{
	return value.value;
}

// This deliberately conflicting name is associated only with a source type.
// Grammar-only ADL must ignore it and retain the built-in brace validator.
template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] constexpr bool validate_format_argument_list(
	::fast_io::fmt::brace_fmt_t) noexcept
{
	return false;
}

inline constexpr ::fast_io::fmt::basic_fixed_string poison_control_format{
	"{1}{0}"};
inline constexpr ::fast_io::fmt::basic_fixed_string large_brace_format{
	"{0}{0}{0}{0}{0}{0}{0}{0}{0}"};
inline constexpr ::fast_io::fmt::basic_fixed_string large_printf_format{
	"%1$d%1$d%1$d%1$d%1$d%1$d%1$d%1$d%1$d"};

namespace validation_protocol =
	::fast_io::fmt::details::format_argument_list_validation_adl;

static_assert(validation_protocol::expression<
			  poison_control_format, ::fast_io::fmt::brace_fmt_t,
			  source, source>);
static_assert(validation_protocol::invoke<
			  poison_control_format, ::fast_io::fmt::brace_fmt_t,
			  source, source>());
static_assert(
	::fast_io::fmt::details::checked_program<
		large_brace_format, ::fast_io::fmt::brace_fmt_t>
		.operation_count > 8u);
static_assert(
	::fast_io::fmt::details::checked_program<
		large_printf_format, ::fast_io::fmt::printf_fmt_t>
		.operation_count > 8u);

} // namespace argument_validation_dispatch_contract

int main()
{
	using namespace argument_validation_dispatch_contract;
	source first{1};
	source second{2};
	char storage[32u]{};
	::fast_io::basic_obuffer_view<char> output{storage, storage + 32u};

	::fast_io::fmt::print<poison_control_format>(
		output, first, second);
	::fast_io::fmt::print<large_brace_format>(output, 7);
	::fast_io::fmt::printf<large_printf_format>(output, 8);
	::fast_io::fmt::print<large_brace_format>(
		output, ::fast_io::mnp::static_arg<9>);

	if (output.size() != 29u || storage[0u] != '2' || storage[1u] != '1')
	{
		return 1;
	}
	for (::std::size_t index{}; index != 9u; ++index)
	{
		if (storage[index + 2u] != '7' ||
			storage[index + 11u] != '8' ||
			storage[index + 20u] != '9')
		{
			return 1;
		}
	}
}
