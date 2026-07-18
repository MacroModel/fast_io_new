#include <fast_io.h>
#include <fast_io_i18n.h>

#include <array>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

inline ::fast_io::basic_scatter<char> append_text(
	::fast_io::lc_object &locale, ::std::string_view text)
{
	return ::fast_io::basic_scatter<char>::append_range(locale.data_storage.chars, text);
}

template <typename value_type, ::std::size_t extent>
inline ::fast_io::basic_scatter<value_type> append_table(
	::std::vector<value_type> &storage, ::std::array<value_type, extent> const &table)
{
	return ::fast_io::basic_scatter<value_type>::append_range(storage, table);
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

inline ::fast_io::lc_object make_locale()
{
	::fast_io::lc_object locale;
	(void)append_text(locale, "unrelated-prefix:");

	// Weekday is computed by the formatter. Populate the complete fixed table with the same value so this fixture
	// verifies descriptor resolution without coupling the storage regression to calendar arithmetic.
	for (auto &entry : locale.all.time.day)
	{
		entry = append_text(locale, "DAY");
	}
	for (auto &entry : locale.all.time.abday)
	{
		entry = append_text(locale, "DY");
	}
	locale.all.time.mon[0u] = append_text(locale, "MONTH");
	locale.all.time.abmon[0u] = append_text(locale, "MON");
	locale.all.time.ab_alt_mon[0u] = append_text(locale, "ALT-MON");
	locale.all.time.am_pm[0u] = append_text(locale, "AM");
	locale.all.time.am_pm[1u] = append_text(locale, "PM");

	::std::array<::fast_io::basic_scatter<char>, 3u> const alternate_digits{
		append_text(locale, "ZERO"), append_text(locale, "ONE"), append_text(locale, "TWO")};
	locale.all.time.alt_digits = append_table(locale.data_storage.strings, alternate_digits);

	::fast_io::basic_lc_time_era<char> era{
		.direction = true,
		.offset = 1,
		.start_date_year = 0,
		.start_date_month = 1,
		.start_date_day = 1,
		.end_date_special = 1,
		.era_name = append_text(locale, "ERA"),
		.era_format = append_text(locale, "ERA-%Y")};
	locale.all.time.era = append_table(locale.data_storage.eras,
									   ::std::array<::fast_io::basic_lc_time_era<char>, 1u>{era});

	locale.all.time.d_t_fmt = append_text(locale, "%A|%B|%p|%Ob|%OC|%EC|%EY|%Od");
	locale.all.time.date_fmt = append_text(locale, "%c");
	locale.all.time.d_fmt = append_text(locale, "%a");
	locale.all.time.t_fmt_ampm = append_text(locale, "%P");
	locale.all.time.t_fmt = append_text(locale, "%r");
	// The alternate-era format is intentionally empty: the public CPO must select the ordinary fallback before
	// resolving its bytes. An invalid descriptor in a different, unselected month proves that the implementation no
	// longer performs an eager whole-facet translation.
	locale.all.time.era_d_fmt = {};
	locale.all.time.abmon[11u] = {
		::std::numeric_limits<::fast_io::i18n_scatter_size_type>::max(), 1u};
	return locale;
}

inline void verify(::fast_io::lc_object const &locale)
{
	using namespace ::fast_io::manipulators;
	::fast_io::iso8601_timestamp const timestamp{
		.year = 200, .month = 1, .day = 2, .hours = 13};
	require(::fast_io::lc_concat(__builtin_addressof(locale.all), date_fmt(timestamp)) ==
			"DAY|MONTH|PM|ALT-MON|TWO|ERA|ERA-0200|TWO");
	require(::fast_io::lc_concat(__builtin_addressof(locale.all), era_d_fmt(timestamp)) == "DY");
	require(::fast_io::lc_concat(__builtin_addressof(locale.all), t_fmt(timestamp)) == "pm");
}

} // namespace

int main()
{
	auto locale{make_locale()};
	verify(locale);

	// Reserve-size computation is a metadata pass. Once the alternate-digit descriptor table itself has been
	// bounds-checked, it may read the selected character count without resolving that character RVA. An invalid RVA is
	// therefore safe here and would deliberately terminate if a define pass attempted to dereference it.
	auto size_only_locale{locale};
	size_only_locale.all.time.d_fmt = append_text(size_only_locale, "%Od");
	size_only_locale.data_storage.strings[2u] = {
		::std::numeric_limits<::fast_io::i18n_scatter_size_type>::max(), 4u};
	::fast_io::iso8601_timestamp const timestamp{
		.year = 200, .month = 1, .day = 2, .hours = 13};
	require(::fast_io::print_reserve_size(
				__builtin_addressof(size_only_locale.all),
				::fast_io::manipulators::d_fmt(timestamp)) == 4u);

	// A complete locale copy owns independent vectors and rebinds `all.data_storage`. Mutating the source after the
	// copy makes a stale owner link immediately observable without introducing dangling storage into the test.
	::fast_io::lc_object copied{locale};
	for (auto &character : locale.data_storage.chars)
	{
		character = 'x';
	}
	verify(copied);
}
