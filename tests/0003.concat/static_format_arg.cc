#if defined(__GNUC__) && !defined(__clang__)
// GCC 15 loses the non-null post-resize destination fact while inlining this
// test's static-versus-dynamic concat matrix and reports the existing memcpy
// leaf as a zero-sized destination. ASan/UBSan exercises the same matrix below;
// keep every other warning enabled for this translation unit.
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#include <fast_io_device.h>
#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

enum class static_printf_enum : unsigned
{
	value = 42u
};

using static_printf_enum_argument = decltype(
	::fast_io::mnp::static_arg<static_printf_enum::value>);
inline constexpr ::fast_io::fmt::basic_fixed_string
	static_printf_enum_format{"%u"};
static_assert(::fast_io::fmt::details::static_format_groups<
	static_printf_enum_format, ::fast_io::fmt::printf_fmt_t,
	static_printf_enum_argument>.has_static_replacement);

template <::fast_io::fmt::basic_fixed_string format_literal, auto value>
[[nodiscard]] bool brace_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::concat_std<format_literal>(
			::fast_io::mnp::static_arg<value>)};
	auto const dynamically_formatted{
		::fast_io::fmt::concat_std<format_literal>(value)};
	return statically_formatted == dynamically_formatted;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  auto value, auto parameter>
[[nodiscard]] bool brace_parameter_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::concat_std<format_literal>(
			::fast_io::mnp::static_arg<value>,
			::fast_io::mnp::static_arg<parameter>)};
	auto const dynamically_formatted{
		::fast_io::fmt::concat_std<format_literal>(value, parameter)};
	return statically_formatted == dynamically_formatted;
}

template <::fast_io::fmt::basic_fixed_string format_literal, auto value>
[[nodiscard]] bool printf_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::concatf_std<format_literal>(
			::fast_io::mnp::static_arg<value>)};
	auto const dynamically_formatted{
		::fast_io::fmt::concatf_std<format_literal>(value)};
	return statically_formatted == dynamically_formatted;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  auto value, auto parameter>
[[nodiscard]] bool printf_parameter_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::concatf_std<format_literal>(
			::fast_io::mnp::static_arg<parameter>,
			::fast_io::mnp::static_arg<value>)};
	auto const dynamically_formatted{
		::fast_io::fmt::concatf_std<format_literal>(parameter, value)};
	return statically_formatted == dynamically_formatted;
}

template <::fast_io::fmt::basic_fixed_string format_literal, auto value>
[[nodiscard]] bool u8brace_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::u8concat_std<format_literal>(
			::fast_io::mnp::static_arg<value>)};
	auto const dynamically_formatted{
		::fast_io::fmt::u8concat_std<format_literal>(value)};
	return statically_formatted == dynamically_formatted;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::basic_fixed_string value>
[[nodiscard]] bool static_string_matches_dynamic()
{
	auto const statically_formatted{
		::fast_io::fmt::concat_std<format_literal>(
			::fast_io::mnp::static_arg<value>)};
	auto const &dynamic_value{
		decltype(::fast_io::mnp::static_arg<value>)::get()};
	auto const dynamically_formatted{
		::fast_io::fmt::concat_std<format_literal>(dynamic_value)};
	return statically_formatted == dynamically_formatted;
}

consteval auto render_static_record()
{
	::std::array<char, 64u> result{};
	::fast_io::obuffer_view buffer{result};
	::fast_io::fmt::print<"user={name} id={id:08x} pi={pi:.2f}">(
		buffer,
		::fast_io::mnp::static_arg<"name", "xxx">,
		::fast_io::mnp::static_arg<"id", 42u>,
		::fast_io::mnp::static_arg<"pi", 3.14>);
	return result;
}

inline constexpr auto static_record{render_static_record()};
static_assert(::std::string_view{static_record.data(), 28u} ==
			  "user=xxx id=0000002a pi=3.14");

consteval auto render_direct_width_overlap()
{
	::std::array<char, 15u> result{};
	::fast_io::obuffer_view buffer{result};
	::fast_io::io::print(
		buffer, ::fast_io::mnp::right(123, 4u),
		::fast_io::mnp::chvw('|'), ::fast_io::mnp::middle(123, 5u, '.'),
		::fast_io::mnp::chvw('|'),
		::fast_io::mnp::internal(-12, 4u, '0'));
	return result;
}

inline constexpr auto direct_width_overlap{render_direct_width_overlap()};
static_assert(::std::string_view{direct_width_overlap.data(),
								 direct_width_overlap.size()} ==
			  " 123|.123.|-012");

inline constexpr char linked_static_text[]{"linked-array"};
using linked_static_text_type =
	decltype(::fast_io::mnp::static_arg<linked_static_text>);
using named_linked_static_text_type =
	decltype(::fast_io::mnp::static_arg<"text", linked_static_text>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
			  linked_static_text_type>);
static_assert(::fast_io::fmt::is_static_format_argument_holder_v<
			  named_linked_static_text_type>);

inline constexpr ::fast_io::fmt::basic_fixed_string mixed_format{
	"user={} id={:08x} score={:.2f}"};
using static_name_type = decltype(::fast_io::mnp::static_arg<"xxx">);
using static_id_type = decltype(::fast_io::mnp::static_arg<42u>);
inline constexpr auto mixed_plan{
	::fast_io::fmt::details::static_format_groups<
		mixed_format, ::fast_io::fmt::brace_fmt_t,
		static_name_type, static_id_type, double>};
static_assert(mixed_plan.has_static_replacement);
static_assert(mixed_plan.group_count == 2u);
static_assert(mixed_plan.is_static[0u] && mixed_plan.begin[0u] == 0u &&
			  mixed_plan.count[0u] == 5u);
static_assert(!mixed_plan.is_static[1u] && mixed_plan.begin[1u] == 5u &&
			  mixed_plan.count[1u] == 1u);
static_assert(::fast_io::fmt::details::static_format_output_bound<
				  mixed_format, ::fast_io::fmt::brace_fmt_t,
				  static_name_type, static_id_type, double>() == 27u);

using mixed_prefix_type =
	::fast_io::fmt::details::compiled_static_format_run<
		mixed_format, ::fast_io::fmt::brace_fmt_t, 0u, 5u,
		static_name_type, static_id_type, double>;
static_assert(mixed_prefix_type::size == 27u);
static_assert(::std::string_view{mixed_prefix_type::storage.data(),
								 mixed_prefix_type::size} == "user=xxx id=0000002a score=");

inline constexpr ::fast_io::fmt::basic_fixed_string runtime_width_format{
	"{:0{}x}"};
inline constexpr auto runtime_width_plan{
	::fast_io::fmt::details::static_format_groups<
		runtime_width_format, ::fast_io::fmt::brace_fmt_t,
		static_id_type, unsigned>};
static_assert(!runtime_width_plan.has_static_replacement);

inline constexpr int pointed_value{};
using static_pointer_type =
	decltype(::fast_io::mnp::static_arg<&pointed_value>);
inline constexpr ::fast_io::fmt::basic_fixed_string pointer_format{"{}"};
inline constexpr auto pointer_plan{
	::fast_io::fmt::details::static_format_groups<
		pointer_format, ::fast_io::fmt::brace_fmt_t,
		static_pointer_type>};
static_assert(!pointer_plan.has_static_replacement);

using runtime_named_type =
	decltype(::fast_io::fmt::arg<"value">(42u));
inline constexpr ::fast_io::fmt::basic_fixed_string named_format{"{value}"};
inline constexpr auto runtime_named_plan{
	::fast_io::fmt::details::static_format_groups<
		named_format, ::fast_io::fmt::brace_fmt_t,
		runtime_named_type>};
static_assert(!runtime_named_plan.has_static_replacement);

inline constexpr ::fast_io::fmt::basic_fixed_string wchar_static_format{
	L"{:☆^5}:{:04x}"};
using wchar_text_type = decltype(::fast_io::mnp::static_arg<L"猫">);
using wchar_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		wchar_static_format, ::fast_io::fmt::brace_fmt_t,
		wchar_text_type, static_id_type>;
static_assert(::std::same_as<typename wchar_static_program::char_type, wchar_t>);
static_assert(wchar_static_program::size == 9u);
static_assert(::std::wstring_view{wchar_static_program::storage.data(),
								  wchar_static_program::size} == L"☆猫☆☆:002a");

inline constexpr ::fast_io::fmt::basic_fixed_string u16_static_format{
	u"{:☆^5}:{:04x}"};
using u16_text_type = decltype(::fast_io::mnp::static_arg<u"猫">);
using u16_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		u16_static_format, ::fast_io::fmt::brace_fmt_t,
		u16_text_type, static_id_type>;
static_assert(::std::same_as<typename u16_static_program::char_type, char16_t>);
static_assert(u16_static_program::size == 9u);
static_assert(::std::u16string_view{u16_static_program::storage.data(),
									u16_static_program::size} == u"☆猫☆☆:002a");

inline constexpr ::fast_io::fmt::basic_fixed_string u32_static_format{
	U"{:☆^5}:{:04x}"};
using u32_text_type = decltype(::fast_io::mnp::static_arg<U"猫">);
using u32_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		u32_static_format, ::fast_io::fmt::brace_fmt_t,
		u32_text_type, static_id_type>;
static_assert(::std::same_as<typename u32_static_program::char_type, char32_t>);
static_assert(u32_static_program::size == 9u);
static_assert(::std::u32string_view{u32_static_program::storage.data(),
									u32_static_program::size} == U"☆猫☆☆:002a");

[[nodiscard]] bool named_and_mixed_records()
{
	char output[128u]{};
	::fast_io::obuffer_view buffer{output, output + 128u};
	::fast_io::fmt::print<mixed_format>(
		buffer, ::fast_io::mnp::static_arg<"xxx">,
		::fast_io::mnp::static_arg<42u>, 3.14);
	return ::std::string_view{output, buffer.size()} ==
		   "user=xxx id=0000002a score=3.14";
}

[[nodiscard]] bool mixed_argument_semantics()
{
	auto const reordered{
		::fast_io::fmt::concat_std<"{2}:{0}:{1}">(
			::fast_io::mnp::static_arg<42u>, "runtime",
			::fast_io::mnp::static_arg<'X'>)};
	if (reordered != "X:42:runtime")
	{
		return false;
	}

	unsigned const dynamic_width{8u};
	auto const width_result{
		::fast_io::fmt::concat_std<"[{1:0{0}x}]">(
			dynamic_width, ::fast_io::mnp::static_arg<42u>)};
	if (width_result != "[0000002a]")
	{
		return false;
	}

	unsigned const dynamic_precision{3u};
	auto const precision_result{
		::fast_io::fmt::concat_std<"{0:.{1}f}">(
			::fast_io::mnp::static_arg<3.1415926535>,
			dynamic_precision)};
	if (precision_result != "3.142")
	{
		return false;
	}

	unsigned runtime_value{7u};
	auto const named_result{
		::fast_io::fmt::concat_std<"{runtime}:{fixed}:{runtime}">(
			::fast_io::fmt::arg<"runtime">(runtime_value),
			::fast_io::mnp::static_arg<"fixed", 42u>)};
	return named_result == "7:42:7";
}

[[nodiscard]] bool integral_matrix()
{
	bool result{true};
	result = result && brace_matches_dynamic<"{}", 0>();
	result = result && brace_matches_dynamic<"{}", -1>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<::std::int64_t>::min)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<::std::int64_t>::max)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<::std::uint64_t>::max)()>();
	result = result && brace_matches_dynamic<"{:+}", 42>();
	result = result && brace_matches_dynamic<"{: }", 42>();
	result = result && brace_matches_dynamic<"{:#b}", 42u>();
	result = result && brace_matches_dynamic<"{:#B}", 42u>();
	result = result && brace_matches_dynamic<"{:#o}", 42u>();
	result = result && brace_matches_dynamic<"{:#o}", 0u>();
	result = result && brace_matches_dynamic<"{:#x}", 0xfeedu>();
	result = result && brace_matches_dynamic<"{:#X}", 0xfeedu>();
	result = result && brace_matches_dynamic<"{:08x}", 42u>();
	result = result && brace_matches_dynamic<"{:#010x}", 42u>();
	result = result && brace_matches_dynamic<"{:<12d}", -42>();
	result = result && brace_matches_dynamic<"{:>12d}", -42>();
	result = result && brace_matches_dynamic<"{:^12d}", -42>();
	result = result && brace_matches_dynamic<"{:*^12d}", -42>();
	result = result && brace_parameter_matches_dynamic<"{:0{}x}", 42u, 12u>();
	result = result && brace_matches_dynamic<"{:c}", 65u>();
	result = result && brace_matches_dynamic<"{}", 'X'>();
	result = result && brace_matches_dynamic<"{:?}", '\n'>();
	result = result && brace_matches_dynamic<"{}", true>();
	result = result && brace_matches_dynamic<"{:d}", ::std::byte{255u}>();
	result = result && brace_matches_dynamic<"{}", nullptr>();
	result = result && printf_matches_dynamic<"%d", -42>();
	result = result && printf_matches_dynamic<"%+08d", 42>();
	result = result && printf_matches_dynamic<"%#x", 42u>();
	result = result && printf_matches_dynamic<"%#o", 0u>();
	result = result && printf_matches_dynamic<"%.0d", 0>();
	result = result && printf_matches_dynamic<"%.8x", 42u>();
	result = result && printf_matches_dynamic<
		"%u", static_printf_enum::value>();
	result = result && printf_parameter_matches_dynamic<"%.*d", 42, 8>();
	result = result && printf_parameter_matches_dynamic<"%*d", 42, -12>();
	result = result && printf_matches_dynamic<"%p", nullptr>();
	return result;
}

[[nodiscard]] bool text_matrix()
{
	bool result{true};
	result = result && static_string_matches_dynamic<"{}", "abc">();
	result = result && static_string_matches_dynamic<"{:?}", "a\n\t\"\\">();
	result = result && static_string_matches_dynamic<"{:*>12}", "abc">();
	result = result && static_string_matches_dynamic<"{:*^12.2}", "abcdef">();
	result = result &&
			 ::fast_io::fmt::concat_std<"[{}]">(
				 ::fast_io::mnp::static_arg<linked_static_text>) ==
				 "[linked-array]";
	result = result &&
			 ::fast_io::fmt::concat_std<"[{text}]">(
				 ::fast_io::mnp::static_arg<"text", linked_static_text>) ==
				 "[linked-array]";
	return result;
}

[[nodiscard]] bool floating_matrix()
{
	bool result{true};
	result = result && brace_matches_dynamic<"{}", 0.0>();
	result = result && brace_matches_dynamic<"{}", -0.0>();
	result = result && brace_matches_dynamic<"{}", 3.14>();
	result = result && brace_matches_dynamic<"{:+}", 3.14>();
	result = result && brace_matches_dynamic<"{: }", 3.14>();
	result = result && brace_matches_dynamic<"{:.0f}", 3.5>();
	result = result && brace_matches_dynamic<"{:#.0f}", 3.5>();
	result = result && brace_matches_dynamic<"{:.6f}", 1.25>();
	result = result && brace_matches_dynamic<"{:.4e}", 12345.0>();
	result = result && brace_matches_dynamic<"{:.4E}", 0.0012345>();
	result = result && brace_matches_dynamic<"{:.4g}", 12345.0>();
	result = result && brace_matches_dynamic<"{:#.4G}", 0.0012345>();
	result = result && brace_matches_dynamic<"{:.4a}", 3.14>();
	result = result && brace_matches_dynamic<"{:#.0A}", 3.14>();
	result = result && brace_matches_dynamic<"{:012.2f}", -3.14>();
	result = result && brace_matches_dynamic<"{:*^16.2f}", 3.14>();
	result = result && brace_parameter_matches_dynamic<"{:.{}f}", 3.1415926535, 5u>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<double>::min)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<double>::max)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<double>::denorm_min)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<double>::infinity)()>();
	result = result && brace_matches_dynamic<"{}", -(std::numeric_limits<double>::infinity)()>();
	result = result && brace_matches_dynamic<"{}", (std::numeric_limits<double>::quiet_NaN)()>();
	result = result && brace_matches_dynamic<"{:.3f}", 3.14f>();
	result = result && brace_matches_dynamic<"{:.5e}", 3.14L>();
	result = result && printf_matches_dynamic<"%.2f", 3.14>();
	result = result && printf_matches_dynamic<"%#.0f", 3.14>();
	result = result && printf_matches_dynamic<"%.4e", 12345.0>();
	result = result && printf_matches_dynamic<"%.4g", 0.0012345>();
	result = result && printf_matches_dynamic<"%.4a", 3.14>();
	result = result && printf_parameter_matches_dynamic<"%.*f", 3.1415926535, 5>();
	return result;
}

[[nodiscard]] bool static_width_overlap_matrix()
{
	return ::fast_io::fmt::concat_std<"{:>4d}">(
			   ::fast_io::mnp::static_arg<123>) == " 123" &&
		   ::fast_io::fmt::concat_std<"{:^5d}">(
			   ::fast_io::mnp::static_arg<123>) == " 123 " &&
		   ::fast_io::fmt::concat_std<"{:04d}">(
			   ::fast_io::mnp::static_arg<-12>) == "-012" &&
		   ::fast_io::fmt::concat_std<"{:#06x}">(
			   ::fast_io::mnp::static_arg<0xabcu>) == "0x0abc" &&
		   ::fast_io::fmt::concat_std<"{:08}">(
			   ::fast_io::mnp::static_arg<-3.14>) == "-0003.14" &&
		   ::fast_io::fmt::concat_std<"{:^16}">(
			   ::fast_io::mnp::static_arg<
				   (::std::numeric_limits<double>::denorm_min)()>) ==
			   "     5e-324     ";
}

[[nodiscard]] bool character_domain_matrix()
{
	bool result{true};
	result = result && u8brace_matches_dynamic<u8"id={:08x}", 42u>();
	result = result && u8brace_matches_dynamic<u8"pi={:.2f}", 3.14>();
	result = result &&
			 ::fast_io::fmt::wconcat_std<wchar_static_format>(
				 ::fast_io::mnp::static_arg<L"猫">,
				 ::fast_io::mnp::static_arg<42u>) == L"☆猫☆☆:002a";
	result = result &&
			 ::fast_io::fmt::u16concat_std<u16_static_format>(
				 ::fast_io::mnp::static_arg<u"猫">,
				 ::fast_io::mnp::static_arg<42u>) == u"☆猫☆☆:002a";
	result = result &&
			 ::fast_io::fmt::u32concat_std<u32_static_format>(
				 ::fast_io::mnp::static_arg<U"猫">,
				 ::fast_io::mnp::static_arg<42u>) == U"☆猫☆☆:002a";
	return result;
}

int main()
{
	return integral_matrix() && floating_matrix() && text_matrix() &&
				   character_domain_matrix() && named_and_mixed_records() &&
				   mixed_argument_semantics() && static_width_overlap_matrix()
			   ? 0
			   : 1;
}
