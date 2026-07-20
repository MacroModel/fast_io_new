#include <fast_io_format.h>

#include <string_view>
#include <type_traits>

namespace static_time_format_test
{

inline constexpr ::fast_io::iso8601_timestamp leap_day{
	2024, 2, 29, 13, 45, 7, 0, 0};
inline constexpr ::fast_io::iso8601_timestamp positive_offset{
	2024, 2, 29, 13, 45, 7, 0, 28800};
inline constexpr ::fast_io::iso8601_timestamp negative_second_offset{
	2021, 1, 1, 0, 0, 0, 0, -19830};
inline constexpr ::fast_io::iso8601_timestamp iso_week_one_boundary{
	1903, 1, 1, 0, 0, 0, 0, 0};
inline constexpr ::fast_io::iso8601_timestamp previous_iso_year_boundary{
	2021, 1, 1, 0, 0, 0, 0, 0};
inline constexpr ::fast_io::basic_timestamp<0> leap_day_timestamp{
	::fast_io::to_timestamp(leap_day)};

using static_iso_argument =
	decltype(::fast_io::mnp::static_arg<leap_day>);
using static_timestamp_argument =
	decltype(::fast_io::mnp::static_arg<leap_day_timestamp>);
using named_static_iso_argument =
	decltype(::fast_io::mnp::static_arg<"when", leap_day>);
using positive_offset_argument =
	decltype(::fast_io::mnp::static_arg<positive_offset>);
using negative_offset_argument =
	decltype(::fast_io::mnp::static_arg<negative_second_offset>);
using iso_week_one_boundary_argument =
	decltype(::fast_io::mnp::static_arg<iso_week_one_boundary>);
using previous_iso_year_boundary_argument =
	decltype(::fast_io::mnp::static_arg<previous_iso_year_boundary>);

inline constexpr ::fast_io::fmt::basic_fixed_string narrow_format{
	"<{:%Y|%F|%A|%B}>"};
inline constexpr ::fast_io::fmt::basic_fixed_string wide_format{
	L"<{:%Y|%F|%A|%B}>"};
inline constexpr ::fast_io::fmt::basic_fixed_string utf8_format{
	u8"<{:%Y|%F|%A|%B}>"};
inline constexpr ::fast_io::fmt::basic_fixed_string utf16_format{
	u"<{:%Y|%F|%A|%B}>"};
inline constexpr ::fast_io::fmt::basic_fixed_string utf32_format{
	U"<{:%Y|%F|%A|%B}>"};

using narrow_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		narrow_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
using wide_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		wide_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
using utf8_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		utf8_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
using utf16_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		utf16_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
using utf32_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		utf32_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;

static_assert(::std::same_as<narrow_static_program::char_type, char>);
static_assert(::std::same_as<wide_static_program::char_type, wchar_t>);
static_assert(::std::same_as<utf8_static_program::char_type, char8_t>);
static_assert(::std::same_as<utf16_static_program::char_type, char16_t>);
static_assert(::std::same_as<utf32_static_program::char_type, char32_t>);

static_assert(::std::string_view{
				  narrow_static_program::storage.data(),
				  narrow_static_program::size} ==
			  "<2024|2024-02-29|Thursday|February>");
static_assert(::std::wstring_view{
				  wide_static_program::storage.data(),
				  wide_static_program::size} ==
			  L"<2024|2024-02-29|Thursday|February>");
static_assert(::std::u8string_view{
				  utf8_static_program::storage.data(),
				  utf8_static_program::size} ==
			  u8"<2024|2024-02-29|Thursday|February>");
static_assert(::std::u16string_view{
				  utf16_static_program::storage.data(),
				  utf16_static_program::size} ==
			  u"<2024|2024-02-29|Thursday|February>");
static_assert(::std::u32string_view{
				  utf32_static_program::storage.data(),
				  utf32_static_program::size} ==
			  U"<2024|2024-02-29|Thursday|February>");

inline constexpr ::fast_io::fmt::basic_fixed_string timestamp_format{
	"{:%F|%A|%B|%Z}"};
using timestamp_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		timestamp_format, ::fast_io::fmt::brace_fmt_t,
		static_timestamp_argument>;
static_assert(::std::string_view{
				  timestamp_static_program::storage.data(),
				  timestamp_static_program::size} ==
			  "2024-02-29|Thursday|February|UTC");

inline constexpr ::fast_io::fmt::basic_fixed_string default_format{
	"[{}]"};
using default_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		default_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
static_assert(::std::string_view{
				  default_static_program::storage.data(),
				  default_static_program::size} ==
			  "[2024-02-29T13:45:07Z]");

inline constexpr ::fast_io::fmt::basic_fixed_string offset_format{
	"{0:%z}|{1:%z}"};
using offset_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		offset_format, ::fast_io::fmt::brace_fmt_t,
		positive_offset_argument, negative_offset_argument>;
static_assert(::std::string_view{
				  offset_static_program::storage.data(),
				  offset_static_program::size} ==
			  "+0800|-053030");

inline constexpr ::fast_io::fmt::basic_fixed_string escaped_locale_format{
	"{:%%c|%%x|%%X}"};
using escaped_locale_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		escaped_locale_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument>;
static_assert(::std::string_view{
					  escaped_locale_static_program::storage.data(),
					  escaped_locale_static_program::size} ==
				  "%c|%x|%X");

inline constexpr ::fast_io::fmt::basic_fixed_string iso_week_boundary_format{
	"{0:%G-W%V-%u}|{1:%G-W%V-%u}"};
using iso_week_boundary_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		iso_week_boundary_format, ::fast_io::fmt::brace_fmt_t,
		iso_week_one_boundary_argument,
		previous_iso_year_boundary_argument>;
static_assert(::std::string_view{
				  iso_week_boundary_static_program::storage.data(),
				  iso_week_boundary_static_program::size} ==
			  "1903-W01-4|2020-W53-5");

inline constexpr ::fast_io::fmt::basic_fixed_string named_format{
	"date={when:%F|%A}"};
using named_static_program =
	::fast_io::fmt::details::compiled_static_format_program<
		named_format, ::fast_io::fmt::brace_fmt_t,
		named_static_iso_argument>;
static_assert(::std::string_view{
				  named_static_program::storage.data(),
				  named_static_program::size} ==
			  "date=2024-02-29|Thursday");

inline constexpr ::fast_io::fmt::basic_fixed_string partial_format{
	"date={when:%F} value={1}"};
inline constexpr auto partial_plan{
	::fast_io::fmt::details::static_format_groups<
		partial_format, ::fast_io::fmt::brace_fmt_t,
		named_static_iso_argument, unsigned>};
static_assert(partial_plan.has_static_replacement);
static_assert(partial_plan.group_count == 2u);
static_assert(partial_plan.is_static[0u]);
static_assert(!partial_plan.is_static[1u]);

using partial_static_prefix =
	::fast_io::fmt::details::compiled_static_format_run<
		partial_format, ::fast_io::fmt::brace_fmt_t, 0u, 3u,
		named_static_iso_argument, unsigned>;
static_assert(::std::string_view{
				  partial_static_prefix::storage.data(),
				  partial_static_prefix::size} ==
			  "date=2024-02-29 value=");

inline constexpr ::fast_io::fmt::basic_fixed_string locale_format{
	"<{0:%c}|{1:%x}|{2:%X}>"};
inline constexpr auto locale_program{
	::fast_io::fmt::details::checked_program<
		locale_format, ::fast_io::fmt::brace_fmt_t>};
inline constexpr auto locale_plan{
	::fast_io::fmt::details::static_format_groups<
		locale_format, ::fast_io::fmt::brace_fmt_t,
		static_iso_argument, static_iso_argument,
		static_iso_argument>};

static_assert(locale_program.field_count == 3u);
static_assert(!locale_plan.has_static_replacement);
static_assert(!::fast_io::fmt::details::automatic_static_replacement_probe<
			  locale_format, locale_program.fields[0u],
			  ::fast_io::fmt::brace_fmt_t,
			  static_iso_argument, static_iso_argument,
			  static_iso_argument>::value);
static_assert(!::fast_io::fmt::details::automatic_static_replacement_probe<
			  locale_format, locale_program.fields[1u],
			  ::fast_io::fmt::brace_fmt_t,
			  static_iso_argument, static_iso_argument,
			  static_iso_argument>::value);
static_assert(!::fast_io::fmt::details::automatic_static_replacement_probe<
			  locale_format, locale_program.fields[2u],
			  ::fast_io::fmt::brace_fmt_t,
			  static_iso_argument, static_iso_argument,
			  static_iso_argument>::value);

[[nodiscard]] bool all_character_domains_match_dynamic()
{
	auto const narrow_static{
		::fast_io::fmt::concat_std<narrow_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const narrow_dynamic{
		::fast_io::fmt::concat_std<narrow_format>(leap_day)};
	auto const wide_static{
		::fast_io::fmt::wconcat_std<wide_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const wide_dynamic{
		::fast_io::fmt::wconcat_std<wide_format>(leap_day)};
	auto const utf8_static{
		::fast_io::fmt::u8concat_std<utf8_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const utf8_dynamic{
		::fast_io::fmt::u8concat_std<utf8_format>(leap_day)};
	auto const utf16_static{
		::fast_io::fmt::u16concat_std<utf16_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const utf16_dynamic{
		::fast_io::fmt::u16concat_std<utf16_format>(leap_day)};
	auto const utf32_static{
		::fast_io::fmt::u32concat_std<utf32_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const utf32_dynamic{
		::fast_io::fmt::u32concat_std<utf32_format>(leap_day)};

	return narrow_static == narrow_dynamic &&
		   wide_static == wide_dynamic &&
		   utf8_static == utf8_dynamic &&
		   utf16_static == utf16_dynamic &&
		   utf32_static == utf32_dynamic;
}

[[nodiscard]] bool timestamp_and_named_paths_match_dynamic()
{
	auto const timestamp_static{
		::fast_io::fmt::concat_std<timestamp_format>(
			::fast_io::mnp::static_arg<leap_day_timestamp>)};
	auto const timestamp_dynamic{
		::fast_io::fmt::concat_std<timestamp_format>(
			leap_day_timestamp)};
	auto const named_static{
		::fast_io::fmt::concat_std<named_format>(
			::fast_io::mnp::static_arg<"when", leap_day>)};
	auto const named_dynamic{
		::fast_io::fmt::concat_std<named_format>(
			::fast_io::fmt::arg<"when">(leap_day))};
	auto const partial_static{
		::fast_io::fmt::concat_std<partial_format>(
			::fast_io::mnp::static_arg<"when", leap_day>, 42u)};
	auto const partial_dynamic{
		::fast_io::fmt::concat_std<partial_format>(
			::fast_io::fmt::arg<"when">(leap_day), 42u)};
	auto const default_static{
		::fast_io::fmt::concat_std<default_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const default_dynamic{
		::fast_io::fmt::concat_std<default_format>(leap_day)};
	auto const offsets_static{
		::fast_io::fmt::concat_std<offset_format>(
			::fast_io::mnp::static_arg<positive_offset>,
			::fast_io::mnp::static_arg<negative_second_offset>)};
	auto const offsets_dynamic{
		::fast_io::fmt::concat_std<offset_format>(
			positive_offset, negative_second_offset)};
	auto const escaped_static{
		::fast_io::fmt::concat_std<escaped_locale_format>(
			::fast_io::mnp::static_arg<leap_day>)};
	auto const iso_week_boundaries_static{
		::fast_io::fmt::concat_std<iso_week_boundary_format>(
			::fast_io::mnp::static_arg<iso_week_one_boundary>,
			::fast_io::mnp::static_arg<previous_iso_year_boundary>)};
	auto const iso_week_boundaries_dynamic{
		::fast_io::fmt::concat_std<iso_week_boundary_format>(
			iso_week_one_boundary, previous_iso_year_boundary)};

	return timestamp_static == timestamp_dynamic &&
		   timestamp_static ==
			   "2024-02-29|Thursday|February|UTC" &&
		   named_static == named_dynamic &&
		   named_static == "date=2024-02-29|Thursday" &&
		   partial_static == partial_dynamic &&
		   partial_static == "date=2024-02-29 value=42" &&
		   default_static == default_dynamic &&
		   default_static == "[2024-02-29T13:45:07Z]" &&
		   offsets_static == offsets_dynamic &&
		   offsets_static == "+0800|-053030" &&
		   escaped_static == "%c|%x|%X" &&
		   iso_week_boundaries_static == iso_week_boundaries_dynamic &&
		   iso_week_boundaries_static == "1903-W01-4|2020-W53-5";
}

[[nodiscard]] bool locale_conversions_stay_dynamic()
{
	auto const static_holders{
		::fast_io::fmt::concat_std<locale_format>(
			::fast_io::mnp::static_arg<leap_day>,
			::fast_io::mnp::static_arg<leap_day>,
			::fast_io::mnp::static_arg<leap_day>)};
	auto const dynamic_values{
		::fast_io::fmt::concat_std<locale_format>(
			leap_day, leap_day, leap_day)};
	return static_holders == dynamic_values;
}

} // namespace static_time_format_test

int main()
{
	using namespace static_time_format_test;
	if (!all_character_domains_match_dynamic())
	{
		return 1;
	}
	if (!timestamp_and_named_paths_match_dynamic())
	{
		return 2;
	}
	if (!locale_conversions_stay_dynamic())
	{
		return 3;
	}
}
