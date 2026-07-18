#include <fast_io.h>
#include <fast_io_i18n.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

namespace
{

template <typename value_type, ::std::size_t extent>
inline ::fast_io::basic_scatter<value_type> append_locale_range(
	::std::vector<value_type> &storage,
	::std::array<value_type, extent> const &values)
{
	return ::fast_io::basic_scatter<value_type>::append_range(storage, values);
}

inline ::fast_io::basic_scatter<char> append_locale_text(
	::fast_io::lc_object &locale, ::std::string_view text)
{
	return ::fast_io::basic_scatter<char>::append_range(locale.data_storage.chars, text);
}

[[noreturn]] inline void fail() noexcept
{
	::std::abort();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		fail();
	}
}

} // namespace

int main()
{
	::fast_io::lc_object locale;

	// Locale numeric and chrono facets contain compact RVAs. This fixture deliberately places unrelated text before
	// every observed field, so accidentally treating an RVA as a process pointer cannot produce a plausible result.
	(void)append_locale_text(locale, "prefix-that-must-not-be-observed");
	locale.all.numeric.grouping = append_locale_range(
		locale.data_storage.integers, ::std::array<::std::size_t, 1u>{3u});
	locale.all.numeric.thousands_sep = append_locale_text(locale, "::");
	locale.all.numeric.decimal_point = append_locale_text(locale, "<dot>");

	locale.all.time.day[0u] = append_locale_text(locale, "Sunday-long");
	locale.all.time.abday[0u] = append_locale_text(locale, "Sun-short");
	locale.all.time.mon[0u] = append_locale_text(locale, "January-long");
	locale.all.time.abmon[0u] = append_locale_text(locale, "Jan-short");
	locale.all.time.ab_alt_mon[0u] = append_locale_text(locale, "Jan-alt");

	::std::array<::fast_io::basic_scatter<char>, 3u> alternate_digits{
		append_locale_text(locale, "zero-alt"),
		append_locale_text(locale, "one-alt"),
		append_locale_text(locale, "two-alt")};
	locale.all.time.alt_digits = append_locale_range(
		locale.data_storage.strings, alternate_digits);

	using namespace ::std::chrono;
	using namespace ::fast_io::manipulators;
	auto const rendered{::fast_io::lc_concat(
		__builtin_addressof(locale.all),
		1234567, "|", weekday{0u}, "|", month{1u}, "|", abbr(weekday{0u}), "|",
		abbr(month{1u}), "|", abbr_alt(month{1u}), "|", alt_num(month{1u}), "|",
		alt_num(day{2u}))};
	require(rendered ==
		"1::234::567|Sunday-long|January-long|Sun-short|Jan-short|Jan-alt|one-alt|two-alt");

	// Complete locale copies own new vectors and rebind their facet aggregate. Formatting through the copy proves the
	// integer and chrono adapters resolve against that new owner rather than retaining the source's table addresses.
	::fast_io::lc_object copied{locale};
	locale.data_storage.chars.clear();
	locale.data_storage.integers.clear();
	locale.data_storage.strings.clear();
	require(::fast_io::lc_concat(
			__builtin_addressof(copied.all), 1234, "|", abbr_alt(month{1u}), "|",
			alt_num(day{2u})) == "1::234|Jan-alt|two-alt");
}
