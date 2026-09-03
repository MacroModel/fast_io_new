#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

using base36_source = decltype(::fast_io::mnp::base<36>(::std::declval<::std::uint_least32_t>()));
using floating_source = decltype(::fast_io::mnp::hexfloat(::std::declval<double>()));
using boolean_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.base = 36u}, bool>;
using referenced_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.base = 36u}, unsigned &>;
using alphabetic_integer_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.base = 36u, .alphabet = true},
	unsigned>;
using percentage_integer_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{
		.base = 36u,
		.percentage = ::fast_io::manipulators::percentage_flag::percent},
	unsigned>;
using decimal_source = decltype(::fast_io::mnp::base<10>(::std::declval<::std::uint_least32_t>()));
struct incomplete_payload;
using incomplete_reference_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.base = 36u}, incomplete_payload &>;
using function_reference_source = ::fast_io::manipulators::scalar_manip_t<
	::fast_io::manipulators::scalar_flags{.base = 36u}, void (&)()>;

/*
Admission is a conjunction, not a structural guess: the source owns one valid
library integral scalar, ordinary alias/forward normalization is type-closed,
and the destination is the explicitly audited fixed external buffer adapter.
Volatile, referenced, floating, boolean, and integer-invalid flag records must
remain on the general dispatcher.
*/
static_assert(
	::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			base36_source>::available);
static_assert(
	::fast_io::details::decay::
		print_fixed_public_integral_manip_source_available<
			char, base36_source>());
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_available<
			char, base36_source volatile>());
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			referenced_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			floating_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			boolean_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			alphabetic_integer_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			percentage_integer_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			decimal_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			incomplete_reference_source>::available);
static_assert(
	!::fast_io::details::decay::
		print_fixed_public_integral_manip_source_traits<
			function_reference_source>::available);
static_assert(
	::fast_io::details::decay::
		print_fixed_public_integral_manip_run_available<
			false, ::fast_io::basic_obuffer_view_ref<char>, char,
			base36_source>());

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__) && 23 <= __clang_major__
static_assert(
	::fast_io::operations::defines::
		print_freestanding_fixed_integral_manip_entry_available<
			false, ::fast_io::basic_obuffer_view<char> &, base36_source>());
#else
static_assert(
	!::fast_io::operations::defines::
		print_freestanding_fixed_integral_manip_entry_available<
			false, ::fast_io::basic_obuffer_view<char> &, base36_source>());
#endif

template <::std::size_t capacity, bool line, ::std::integral char_type,
		  ::std::size_t extent, typename... Args>
[[nodiscard]] bool check_with_capacity(
	char_type const (&expected)[extent], Args &&...args)
{
	static_assert(extent != 1u);
	static_assert(extent - 1u <= capacity);
	char_type storage[capacity];
	// Use the physical array extent for the endpoint. This is exactly the one-past pointer and lets a direct-fit
	// case provide reserve headroom while the checks below continue to validate only the logical spelling.
	char_type *const storage_end{storage + capacity};
	::fast_io::basic_obuffer_view<char_type> output{
		storage, storage_end};
	::fast_io::operations::print_freestanding<line>(
		output, ::std::forward<Args>(args)...);
	if (output.size() != extent - 1u)
	{
		return false;
	}
	for (::std::size_t index{}; index != extent - 1u; ++index)
	{
		if (storage[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

template <bool line, ::std::integral char_type, ::std::size_t extent,
		  typename... Args>
[[nodiscard]] bool check_exact(
	char_type const (&expected)[extent], Args &&...args)
{
	return check_with_capacity<extent - 1u, line>(
		expected, ::std::forward<Args>(args)...);
}

template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] bool check_character_domain(char_type const (&expected)[extent])
{
	return check_exact<false>(
		expected, ::fast_io::mnp::base<2>(5u),
		::fast_io::mnp::base<36>(35u));
}

} // namespace

int main()
{
	/*
	The first run crosses radix, prefix, case, sign, and multi-leaf ordering on
	the generic fallback. Exact-size views force scratch staging, while the second
	plain-radix run supplies enough physical capacity to exercise the measured
	Apple-Clang direct-fit entry, including signed output and ordered pack commit.
	*/
	if (!check_exact<false>(
			"101110x2aZ-z", ::fast_io::mnp::base<2>(5u),
			::fast_io::mnp::base<8>(9u), ::fast_io::mnp::hex0x(42u),
			::fast_io::mnp::baseupper<36>(35u),
			::fast_io::mnp::base<36>(-35)) ||
		!check_with_capacity<64u, false>(
			"101z-z", ::fast_io::mnp::base<2>(5u),
			::fast_io::mnp::base<36>(35u),
			::fast_io::mnp::base<36>(-35)) ||
		!check_exact<true>("zz\n", ::fast_io::mnp::base<36>(1295u)) ||
		!check_character_domain(L"101z") ||
		!check_character_domain(u8"101z") ||
		!check_character_domain(u"101z") ||
		!check_character_domain(U"101z"))
	{
		return 1;
	}
}
