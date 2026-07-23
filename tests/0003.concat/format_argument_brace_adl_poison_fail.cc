#include <fast_io_format.h>

namespace argument_validation_adl_poison
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

// A source namespace must not participate in validator ADL. Otherwise this
// overload could hide the mandatory brace-domain proof and accept an unused
// argument.
template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] constexpr bool validate_format_argument_list(
	::fast_io::fmt::brace_fmt_t) noexcept
{
	return true;
}

} // namespace argument_validation_adl_poison

int main()
{
	argument_validation_adl_poison::source unused{1};
	argument_validation_adl_poison::source selected{2};
	char storage[1u]{};
	::fast_io::basic_obuffer_view<char> output{storage, storage + 1u};
	::fast_io::fmt::print<"{1}">(output, unused, selected);
}
