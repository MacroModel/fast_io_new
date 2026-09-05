#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>

#include <fast_io_freestanding.h>

namespace
{

/// @brief Verifies the portable two-word multiply-add recurrence at its maximal incoming carry.
/// @details For B=2^64, `(B-1)^2+(B-1)=B^2-B`; its low word is zero and its high word is B-1. This is the boundary
///          which would expose either a lost low-word carry or an overflowing high-word update on a target without a
///          native unsigned 128-bit scalar.
[[nodiscard]] constexpr bool check_mul_add_carry_boundary() noexcept
{
	constexpr auto maximum{
		(::std::numeric_limits<::std::uint_least64_t>::max)()};
	::std::uint_least64_t carry{maximum};
	auto const low{::fast_io::details::scan_decfloat_bigint_mul_add_carry(
		maximum, maximum, carry)};
	return low == 0u && carry == maximum;
}

static_assert(check_mul_add_carry_boundary());

template <::std::size_t extent>
[[nodiscard]] bool parse_has_bits(
	char const (&text)[extent], ::std::uint_least64_t expected) noexcept
{
	double value{};
	auto const result{::fast_io::from_chars(text, text + extent - 1u, value)};
	return result.ec == ::std::errc{} && result.ptr == text + extent - 1u &&
		   ::std::bit_cast<::std::uint_least64_t>(value) == expected;
}

/// @brief Exercises the exact decimal comparator on both sides of two adjacent binary64 midpoints.
/// @details The first exact midpoint lies between encodings zero and one above 1.0, so nearest-even selects the lower
///          encoding. The second lies between encodings one and two, so nearest-even selects the upper encoding. Each
///          coefficient exceeds the 19-digit scalar accumulator, forcing the arbitrary-precision limb recurrence; the
///          neighboring decimal values prove that the comparator does not merely recognize the two tie spellings.
[[nodiscard]] bool check_nearest_even_midpoints() noexcept
{
	return parse_has_bits(
			   "1.000000000000000111022302462515654042363166809082031249999",
			   UINT64_C(0x3ff0000000000000)) &&
		   parse_has_bits(
			   "1.00000000000000011102230246251565404236316680908203125",
			   UINT64_C(0x3ff0000000000000)) &&
		   parse_has_bits(
			   "1.000000000000000111022302462515654042363166809082031250001",
			   UINT64_C(0x3ff0000000000001)) &&
		   parse_has_bits(
			   "1.000000000000000333066907387546962127089500427246093749999",
			   UINT64_C(0x3ff0000000000001)) &&
		   parse_has_bits(
			   "1.00000000000000033306690738754696212708950042724609375",
			   UINT64_C(0x3ff0000000000002)) &&
		   parse_has_bits(
			   "1.000000000000000333066907387546962127089500427246093750001",
			   UINT64_C(0x3ff0000000000002));
}

} // namespace

int main()
{
	return !check_nearest_even_midpoints();
}
