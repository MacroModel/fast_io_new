#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

using decimal_scanner = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::floating_point_default_scalar_flags, double &>;
using hexadecimal_scanner = decltype(::fast_io::manipulators::hexfloat_get(::std::declval<double &>()));
using precision_scanner = ::fast_io::manipulators::scalar_manip_precision_t<
	::fast_io::manipulators::floating_point_default_scalar_flags, double &>;

static_assert(
	::fast_io::contiguous_scanner_result_in_range<char, decimal_scanner>);
static_assert(
	!::fast_io::contiguous_scanner_result_in_range<char, hexadecimal_scanner>);
static_assert(
	!::fast_io::contiguous_scanner_result_in_range<char, precision_scanner>);

template <typename floating_type, ::std::integral char_type,
		  ::std::size_t extent>
void check_range(char_type const (&text)[extent])
{
	floating_type value{};
	using scanner_type = ::fast_io::manipulators::scalar_manip_t<
		::fast_io::manipulators::floating_point_default_scalar_flags,
		floating_type &>;
	scanner_type scanner{value};
	auto const *const begin{text};
	auto const *const end{text + extent - 1u};
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char_type, scanner_type>, begin, end,
		scanner)};
	/*
	The marker promises same-array provenance in addition to numeric address
	order. Because begin, result.iter, and end are then pointers into this one
	literal array, both relational comparisons are defined and exercise the
	closed-range postcondition for every returned parse code.
	*/
	if (result.iter < begin || end < result.iter)
	{
		::std::abort();
	}
}

template <typename floating_type>
void check_char_matrix()
{
	check_range<floating_type>("");
	check_range<floating_type>("   ");
	check_range<floating_type>("+");
	check_range<floating_type>("-");
	check_range<floating_type>(".");
	check_range<floating_type>("e");
	check_range<floating_type>("1e");
	check_range<floating_type>("1e+");
	check_range<floating_type>("1e-");
	check_range<floating_type>("1e+x");
	check_range<floating_type>("1.25tail");
	check_range<floating_type>("inf");
	check_range<floating_type>("infinity");
	check_range<floating_type>("infinix");
	check_range<floating_type>("nan");
	check_range<floating_type>("nan(payload)");
	check_range<floating_type>("1e9999");
	check_range<floating_type>("1e-9999");
	check_range<floating_type>(
		"123456789012345678901234567890123456789012345678901234567890e-20");
}

} // namespace

int main()
{
	check_char_matrix<float>();
	check_char_matrix<double>();
	check_char_matrix<long double>();

	// Character-domain coverage checks the marker's template-wide obligation.
	check_range<double>(L"-1.25e+10tail");
	check_range<double>(u8"-1.25e+10tail");
	check_range<double>(u"-1.25e+10tail");
	check_range<double>(U"-1.25e+10tail");
}
