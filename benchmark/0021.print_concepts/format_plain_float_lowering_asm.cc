#include <fast_io.h>
#include <fast_io_format.h>

#include <type_traits>
#include <utility>

namespace
{

template <typename>
inline constexpr bool is_native_scalar_v{};

template <::fast_io::manipulators::scalar_flags flags, typename value_type>
inline constexpr bool is_native_scalar_v<
	::fast_io::manipulators::scalar_manip_t<flags, value_type>>{true};

template <typename>
inline constexpr bool is_format_scalar_v{};

template <typename scalar_type, ::std::size_t prefix_size, bool space_sign>
inline constexpr bool is_format_scalar_v<
	::fast_io::manipulators::format_scalar_t<scalar_type, prefix_size, space_sign>>{true};

template <typename>
inline constexpr bool is_force_radix_v{};

template <typename value_type>
inline constexpr bool is_force_radix_v<
	::fast_io::manipulators::printf_force_radix_t<value_type>>{true};

template <typename>
inline constexpr bool is_general_float_v{};

template <typename fixed_type, typename scientific_type, bool alternate_form>
inline constexpr bool is_general_float_v<
	::fast_io::manipulators::general_float_t<fixed_type, scientific_type,
											 alternate_form>>{true};

using ::fast_io::fmt::details::format_parameter_kind;
using ::fast_io::fmt::details::format_sign;
using ::fast_io::fmt::details::format_specification;
using ::fast_io::fmt::details::presentation_type;
using ::fast_io::fmt::details::resolved_format_parameter;

inline constexpr format_specification<char> plain_specification{};

inline constexpr auto precision_specification = []() consteval {
	format_specification<char> result{};
	result.precision.kind = format_parameter_kind::literal;
	result.precision.value = 6u;
	return result;
}();

inline constexpr auto alternate_specification = []() consteval {
	format_specification<char> result{};
	result.alternate_form = true;
	return result;
}();

inline constexpr auto sign_specification = []() consteval {
	format_specification<char> result{};
	result.sign = format_sign::plus;
	return result;
}();

inline constexpr auto width_specification = []() consteval {
	format_specification<char> result{};
	result.width.kind = format_parameter_kind::literal;
	result.width.value = 20u;
	return result;
}();

template <presentation_type presentation>
inline constexpr auto presentation_specification = []() consteval {
	format_specification<char> result{};
	result.presentation = presentation;
	return result;
}();

template <format_specification<char> specification>
using lowering_result_t = decltype(::fast_io::fmt::details::make_brace_floating<specification>(
	::std::declval<double &>(), resolved_format_parameter{}));

// This compile-time classifier is intentionally independent of emitted symbol
// names.  It proves that only a grammar-empty `{}` reaches the native scalar
// leaf; precision, alternate form, sign, width, and every floating presentation
// retain their semantic wrappers even when the optimizer later inlines them.
static_assert(is_native_scalar_v<lowering_result_t<plain_specification>>);
static_assert(is_format_scalar_v<lowering_result_t<precision_specification>>);
static_assert(is_force_radix_v<lowering_result_t<alternate_specification>>);
static_assert(is_format_scalar_v<lowering_result_t<sign_specification>>);
static_assert(is_format_scalar_v<lowering_result_t<width_specification>>);
static_assert(is_format_scalar_v<lowering_result_t<
				  presentation_specification<presentation_type::fixed_lower>>>);
static_assert(is_format_scalar_v<lowering_result_t<
				  presentation_specification<presentation_type::scientific_lower>>>);
static_assert(is_general_float_v<lowering_result_t<
				  presentation_specification<presentation_type::general_lower>>>);
static_assert(is_format_scalar_v<lowering_result_t<
				  presentation_specification<presentation_type::hexfloat_lower>>>);

} // namespace

// These exported leaves separate the value-constant and optimizer-unknown
// halves of the plain floating lowering rule.  The assembly gate classifies
// calls and semantic types; byte spelling is checked by the runtime test.
extern "C" void fast_io_fmt_plain_constant_float()
{
	::fast_io::fmt::print<"{}">(::fast_io::out(), 3.14f);
}

extern "C" void fast_io_fmt_plain_constant_double()
{
	::fast_io::fmt::print<"{}">(::fast_io::out(), 3.14);
}

extern "C" void fast_io_fmt_plain_prefixed_constant_double()
{
	::fast_io::fmt::print<"value={}">(::fast_io::out(), 3.14);
}

extern "C" void fast_io_fmt_plain_runtime_float(float value)
{
	::fast_io::fmt::print<"{}">(::fast_io::out(), value);
}

extern "C" void fast_io_fmt_plain_runtime_double(double value)
{
	::fast_io::fmt::print<"{}">(::fast_io::out(), value);
}
