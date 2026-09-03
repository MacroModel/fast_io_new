#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

template <typename char_type, typename T>
using normalized_print_type = ::std::remove_cvref_t<decltype(::fast_io::io_print_forward<char_type>(
	::fast_io::io_print_alias(::std::declval<T &>())))>;

using decimal_integer = normalized_print_type<char, ::std::uint_least64_t>;
using hexadecimal_source = decltype(::fast_io::mnp::base<16>(::std::declval<::std::uint_least64_t &>()));
using hexadecimal_integer = normalized_print_type<char, hexadecimal_source>;
using decimal_floating = normalized_print_type<char, double>;
using referenced_decimal_integer = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{}, ::std::uint_least64_t &>;
using policy_decimal_integer = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.json_float = true},
	::std::uint_least64_t>;
struct incomplete_payload;
using incomplete_reference = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{}, incomplete_payload &>;

/*
Admission invariant: only one normalized built-in default-decimal integer leaf
and an explicitly audited fresh destination may select direct construction.
Radix manipulators, floating scalars, multi-leaf runs, and extensible string
traits or allocators retain their existing materialization policies.
*/
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__ &&                        \
	defined(_LIBCPP_VERSION) && 230000 <= _LIBCPP_VERSION &&              \
	defined(_LIBCPP_ABI_VERSION) && _LIBCPP_ABI_VERSION == 1 &&           \
	defined(_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT)
static_assert(
	::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			false, char, ::std::string, decimal_integer>);
static_assert(
	::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			true, char, ::std::string, decimal_integer>);
#else
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			false, char, ::std::string, decimal_integer>);
#endif
static_assert(
	::fast_io::details::decay::
		print_concat_fresh_fixed_decimal_scalar_traits<
			decimal_integer>::available);
static_assert(
	!::fast_io::details::decay::
		print_concat_fresh_fixed_decimal_scalar_traits<
			referenced_decimal_integer>::available);
static_assert(
	!::fast_io::details::decay::
		print_concat_fresh_fixed_decimal_scalar_traits<
			policy_decimal_integer>::available);
static_assert(
	!::fast_io::details::decay::
		print_concat_fresh_fixed_decimal_scalar_traits<
			incomplete_reference>::available);
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			false, char, ::std::string, hexadecimal_integer>);
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			false, char, ::std::string, decimal_floating>);
static_assert(
	!::fast_io::details::decay::
		basic_general_concat_fresh_fixed_scalar_direct_run_v<
			false, char, ::std::string, decimal_integer,
			decimal_integer>);

struct custom_traits : ::std::char_traits<char>
{};

using custom_traits_string = ::std::basic_string<char, custom_traits>;
using polymorphic_string = ::std::pmr::string;

static_assert(
	!::fast_io::concat_fresh_fixed_scalar_direct_preferred_strlike<
		char, custom_traits_string>);
static_assert(
	!::fast_io::concat_fresh_fixed_scalar_direct_preferred_strlike<
		char, polymorphic_string>);
static_assert(
	!::fast_io::concat_fresh_fixed_scalar_direct_preferred_strlike<
		wchar_t, ::std::wstring>);

#if __cpp_lib_constexpr_string >= 201907L
/*
The run-time cost CPO must not expose standard-string spare capacity during
constant evaluation; the displaced portable construction path remains valid.
*/
static_assert(::fast_io::concat_std(42u) == "42");
static_assert(::fast_io::concatln_std(42u) == "42\n");
#endif

} // namespace

int main()
{
	constexpr auto unsigned_max{
		::std::numeric_limits<::std::uint_least64_t>::max()};
	constexpr auto signed_min{
		::std::numeric_limits<::std::int_least64_t>::min()};

	/*
	These checks intentionally use explicit failure returns instead of assert:
	Release test configurations define NDEBUG, but the observable-equivalence
	contract must still be executed and diagnosed in every build mode.
	*/
	if (::fast_io::concat_std(0u) != "0" ||
		::fast_io::concat_std(unsigned_max) != "18446744073709551615" ||
		::fast_io::concat_std(signed_min) != "-9223372036854775808" ||
		::fast_io::concatln_std(-42) != "-42\n")
	{
		return 1;
	}

	// Each excluded source shape is checked for unchanged observable spelling.
	if (::fast_io::concat_std(::fast_io::mnp::base<16>(unsigned_max)) !=
			"ffffffffffffffff" ||
		::fast_io::concat_std(1.25) != "1.25" ||
		::fast_io::concat_std(12u, 34u) != "1234" ||
		::fast_io::wconcat_std(unsigned_max) != L"18446744073709551615" ||
		::fast_io::u8concat_std(unsigned_max) != u8"18446744073709551615" ||
		::fast_io::u16concat_std(unsigned_max) != u"18446744073709551615" ||
		::fast_io::u32concat_std(unsigned_max) != U"18446744073709551615")
	{
		return 2;
	}
}
