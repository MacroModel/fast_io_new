#include <fast_io_format.h>

namespace missing_custom_alias
{

struct value
{};

struct state
{
	constexpr bool operator==(state const &) const noexcept = default;
};

template <typename context_type>
[[nodiscard]] consteval state format_parse_define(
	::fast_io::io_type_t<value>, context_type) noexcept
{
	return {};
}

} // namespace missing_custom_alias

int main()
{
	(void)::fast_io::fmt::concat_std<"{}">(missing_custom_alias::value{});
}
