#include <fast_io_core.h>

inline constexpr bool is_leap_year(::std::int_least64_t year) noexcept
{
	return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

inline constexpr ::std::uint_least8_t days_in_month(::std::int_least64_t year,
													::std::uint_least8_t month) noexcept
{
	constexpr ::std::uint_least8_t month_lengths[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return static_cast<::std::uint_least8_t>(month_lengths[month - 1u] +
											 (month == 2u && is_leap_year(year)));
}

inline constexpr bool same_civil_time(::fast_io::iso8601_timestamp const &left,
									  ::fast_io::iso8601_timestamp const &right) noexcept
{
	return left.year == right.year && left.month == right.month && left.day == right.day &&
		   left.hours == right.hours && left.minutes == right.minutes && left.seconds == right.seconds &&
		   left.subseconds == right.subseconds;
}

inline constexpr bool year_roundtrips(::std::int_least64_t year) noexcept
{
	for (::std::uint_least8_t month{1}; month <= 12u; ++month)
	{
		for (::std::uint_least8_t day{1}; day <= days_in_month(year, month); ++day)
		{
			::fast_io::iso8601_timestamp const input{year, month, day, 23, 59, 59, 123, 0};
			if (!same_civil_time(input, ::fast_io::utc(::fast_io::to_timestamp(input))))
			{
				return false;
			}
		}
	}
	return true;
}

static_assert(::fast_io::to_timestamp(::fast_io::iso8601_timestamp{1970, 1, 1, 0, 0, 0, 0, 0}).seconds == 0);
static_assert(::fast_io::to_timestamp(::fast_io::iso8601_timestamp{2024, 2, 28, 0, 0, 0, 0, 0}).seconds ==
			  1709078400);
static_assert(::fast_io::to_timestamp(::fast_io::iso8601_timestamp{2024, 2, 29, 0, 0, 0, 0, 0}).seconds ==
			  1709164800);
static_assert(::fast_io::to_timestamp(::fast_io::iso8601_timestamp{2024, 3, 1, 0, 0, 0, 0, 0}).seconds ==
			  1709251200);

static_assert(year_roundtrips(-400));
static_assert(year_roundtrips(-1));
static_assert(year_roundtrips(0));
static_assert(year_roundtrips(1900));
static_assert(year_roundtrips(2000));
static_assert(year_roundtrips(2023));
static_assert(year_roundtrips(2024));

int main()
{
}
