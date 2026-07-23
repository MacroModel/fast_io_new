#include <fast_io_format.h>

namespace nonconstant_static_output_probe
{

struct value_type
{
	unsigned value{};

	constexpr bool operator==(value_type const &) const noexcept = default;
};

[[nodiscard]] inline constexpr unsigned format_as(value_type value) noexcept
{
	return value.value;
}

// These CPOs deliberately satisfy the structural protocol but cannot be
// evaluated at compile time. Static provider construction must diagnose this
// violated semantic requirement; silently selecting runtime formatting would
// make the advertised terminal static-output opt-in unsound.
template <auto specification, ::std::size_t depth>
[[nodiscard]] inline ::std::size_t format_static_reserve_size(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>,
	value_type const &) noexcept
{
	return 2u;
}

template <auto specification, ::std::size_t depth>
[[nodiscard]] inline char *format_static_reserve_define(
	::fast_io::fmt::basic_static_format_context_t<specification, depth>,
	::fast_io::fmt::basic_static_format_as_t<char>, char *output,
	value_type const &value) noexcept
{
	*output++ = static_cast<char>('0' + value.value / 10u);
	*output++ = static_cast<char>('0' + value.value % 10u);
	return output;
}

} // namespace nonconstant_static_output_probe

[[maybe_unused]] auto must_fail_at_static_provider_proof()
{
	return ::fast_io::fmt::concat_std<"{}">(
		::fast_io::mnp::static_arg<
			nonconstant_static_output_probe::value_type{42u}>);
}
