#include <concepts>
#include <cstddef>

#include <fast_io_freestanding.h>

namespace
{

inline constexpr auto diagnostic_flags = []() consteval {
	auto flags{
		::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::scientific;
	flags.showpos = true;
	flags.uppercase = true;
	flags.uppercase_e = true;
	flags.comma = true;
	flags.rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_odd;
	return flags;
}();

using expected_type = ::fast_io::manipulators::scalar_manip_t<
	diagnostic_flags, ::fast_io::details::float_alias_type<double>>;
using actual_type = decltype(
	::fast_io::mnp::scalar_generic<diagnostic_flags>(1.25));

// scalar_generic is the policy-preserving scalar escape hatch. Checking its
// exact type catches an accidental replacement of the caller's scalar_flags
// NTTP before any reserve or output strategy can hide the lost policy.
static_assert(::std::same_as<actual_type, expected_type>);

[[nodiscard]] bool runtime_policy_is_visible() noexcept
{
	auto const value{
		::fast_io::mnp::scalar_generic<diagnostic_flags>(1.25)};
	char buffer[128u]{};
	auto const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type_t<char, actual_type>{}, buffer, value)};
	bool plus{};
	bool comma{};
	bool uppercase_exponent{};
	for (auto iter{buffer}; iter != end; ++iter)
	{
		plus = plus || *iter == '+';
		comma = comma || *iter == ',';
		uppercase_exponent = uppercase_exponent || *iter == 'E';
	}
	return buffer != end && plus && comma && uppercase_exponent;
}

} // namespace

int main()
{
	return runtime_policy_is_visible() ? 0 : 1;
}
