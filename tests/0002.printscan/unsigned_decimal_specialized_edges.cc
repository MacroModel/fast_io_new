#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <fast_io_core.h>

namespace
{

/*
The invalid-position matrix deliberately varies `last` at run time. Keeping its
entry boundary out of line prevents GCC 11 interprocedural range propagation
from inventing a zero-length path to a guarded `last[-1]` expression. The
ordinary caller still supplies the exact half-open range, so ASan observes the
same physical accesses and the test does not weaken the scanner contract.
*/
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
::fast_io::parse_result<char const *> parse_decimal_runtime(
	char const *first, char const *last, ::std::uint64_t &value) noexcept
{
	return ::fast_io::parse_by_scan(first, last, value);
}

template <::std::size_t extent>
[[nodiscard]] constexpr bool check_exact(
	char const (&text)[extent], ::std::uint64_t expected,
	::fast_io::parse_code expected_code, ::std::size_t consumed,
	::std::uint64_t initial = UINT64_C(0x5a5a5a5a5a5a5a5a)) noexcept
{
	::std::uint64_t value{initial};
	auto const result{
		::fast_io::parse_by_scan(text, text + extent - 1u, value)};
	return result.iter == text + consumed && result.code == expected_code &&
		   value == expected;
}

/*
The compile-time path must remain the semantic oracle for the native 8+8+4
kernel. P2448R2 is the relevant C++23 relaxation for constexpr function bodies;
the feature-test guard avoids asking earlier language modes to instantiate a
contract they are not required to accept.
*/
#if defined(__cpp_constexpr) && __cpp_constexpr >= 202207L
static_assert(check_exact(
	"18446744073709551615",
	(::std::numeric_limits<::std::uint64_t>::max)(),
	::fast_io::parse_code::ok, 20u));
static_assert(check_exact(
	"18446744073709551616", UINT64_C(0x5a5a5a5a5a5a5a5a),
	::fast_io::parse_code::overflow, 20u));
#endif

void check_twenty_digit_boundaries()
{
	constexpr auto sentinel{UINT64_C(0x5a5a5a5a5a5a5a5a)};
	assert(check_exact(
		"10000000000000000000", UINT64_C(10000000000000000000),
		::fast_io::parse_code::ok, 20u));
	assert(check_exact(
		"18446744073709551615",
		(::std::numeric_limits<::std::uint64_t>::max)(),
		::fast_io::parse_code::ok, 20u));
	assert(check_exact(
		"18446744073709551616", sentinel,
		::fast_io::parse_code::overflow, 20u));
	assert(check_exact(
		"18446744999999999999", sentinel,
		::fast_io::parse_code::overflow, 20u));
	assert(check_exact(
		"99999999999999999999", sentinel,
		::fast_io::parse_code::overflow, 20u));
	assert(check_exact(
		"100000000000000000000", sentinel,
		::fast_io::parse_code::overflow, 21u));
	assert(check_exact(
		"00000000000000000000", sentinel,
		::fast_io::parse_code::invalid, 1u));
}

void check_invalid_cursor_matrix()
{
	/*
	Every failed packed validation delegates to the shared scanner. Thus an
	invalid first code unit is a lexical error, while a delimiter after a valid
	nonzero prefix completes that prefix successfully at the exact cursor.
	Crossing every specialized extent and every byte position checks the fixed
	eight- and twelve-digit leaves, both eight-byte groups, and the four-byte
	tail without relying on guard-page over-read accidents.
	*/
	char text[20u];
	for (auto &character : text)
	{
		character = '1';
	}
	for (::std::size_t extent{4u}; extent <= sizeof(text); ++extent)
	{
		for (::std::size_t position{}; position != extent; ++position)
		{
			text[position] = 'x';
			::std::uint64_t value{UINT64_C(0x5a5a5a5a5a5a5a5a)};
			auto const result{
				parse_decimal_runtime(text, text + extent, value)};
			assert(result.iter == text + position);
			if (position == 0u)
			{
				assert(result.code == ::fast_io::parse_code::invalid);
				assert(value == UINT64_C(0x5a5a5a5a5a5a5a5a));
			}
			else
			{
				::std::uint64_t expected{};
				for (::std::size_t index{}; index != position; ++index)
				{
					expected = expected * 10u + 1u;
				}
				assert(result.code == ::fast_io::parse_code::ok);
				assert(value == expected);
			}
			text[position] = '1';
		}
	}
}

void check_range_state_combinations()
{
	char const delimited[]{"18446744073709551615|"};
	::std::uint64_t value{};
	auto const delimited_result{::fast_io::parse_by_scan(
		delimited, delimited + sizeof(delimited) - 1u, value)};
	assert(delimited_result.iter == delimited + 20u);
	assert(delimited_result.code == ::fast_io::parse_code::ok);
	assert(value == (::std::numeric_limits<::std::uint64_t>::max)());

	char const spaced[]{" \t18446744073709551615"};
	value = 0u;
	auto const spaced_result{::fast_io::parse_by_scan(
		spaced, spaced + sizeof(spaced) - 1u, value)};
	assert(spaced_result.iter == spaced + sizeof(spaced) - 1u);
	assert(spaced_result.code == ::fast_io::parse_code::ok);
	assert(value == (::std::numeric_limits<::std::uint64_t>::max)());

	/* Signed targets select the existing signed scanner, guarding the CPO
	   specialization boundary as well as the two signed terminal values. */
	char const signed_minimum[]{"-9223372036854775808"};
	::std::int64_t signed_value{};
	auto const signed_result{::fast_io::parse_by_scan(
		signed_minimum, signed_minimum + sizeof(signed_minimum) - 1u,
		signed_value)};
	assert(signed_result.iter == signed_minimum + sizeof(signed_minimum) - 1u);
	assert(signed_result.code == ::fast_io::parse_code::ok);
	assert(signed_value == (::std::numeric_limits<::std::int64_t>::min)());
}

struct parse_observation
{
	::std::size_t consumed{};
	::fast_io::parse_code code{};
	::std::uint64_t value{};
};

[[nodiscard]] constexpr parse_observation reference_decimal(
	char const *first, char const *last, ::std::uint64_t initial) noexcept
{
	if (first == last)
	{
		return {0u, ::fast_io::parse_code::end_of_file, initial};
	}
	auto const first_digit{static_cast<unsigned>(
		static_cast<unsigned char>(*first) - static_cast<unsigned char>('0'))};
	if (9u < first_digit)
	{
		return {0u, ::fast_io::parse_code::invalid, initial};
	}
	if (first_digit == 0u)
	{
		if (first + 1u != last)
		{
			auto const second_digit{static_cast<unsigned>(
				static_cast<unsigned char>(first[1u]) -
				static_cast<unsigned char>('0'))};
			if (second_digit <= 9u)
			{
				return {1u, ::fast_io::parse_code::invalid, initial};
			}
		}
		return {1u, ::fast_io::parse_code::ok, 0u};
	}

	auto current{first};
	::std::uint64_t value{};
	bool overflow{};
	for (; current != last; ++current)
	{
		auto const digit{static_cast<unsigned>(
			static_cast<unsigned char>(*current) -
			static_cast<unsigned char>('0'))};
		if (9u < digit)
		{
			break;
		}
		if (!overflow)
		{
			constexpr auto maximum{
				(::std::numeric_limits<::std::uint64_t>::max)()};
			overflow = value > (maximum - digit) / 10u;
			if (!overflow)
			{
				value = value * 10u + digit;
			}
		}
	}
	return {static_cast<::std::size_t>(current - first),
			overflow ? ::fast_io::parse_code::overflow
					 : ::fast_io::parse_code::ok,
			overflow ? initial : value};
}

void check_differential_matrix()
{
	/*
	This deterministic corpus crosses every specialized width with valid digits,
	early delimiters, leading-zero rejection, and overflow-length runs. The oracle
	models the public decimal grammar independently of the grouped implementation;
	equality therefore covers value publication as well as code and cursor state.
	*/
	char text[32u];
	::std::uint64_t random{UINT64_C(0x9e3779b97f4a7c15)};
	for (::std::size_t iteration{}; iteration != 200000u; ++iteration)
	{
		random ^= random << 7u;
		random ^= random >> 9u;
		random ^= random << 8u;
		auto const size{static_cast<::std::size_t>(random % 33u)};
		for (::std::size_t index{}; index != size; ++index)
		{
			random ^= random << 7u;
			random ^= random >> 9u;
			random ^= random << 8u;
			auto const symbol{static_cast<unsigned>(random & 15u)};
			text[index] = symbol < 12u
						  ? static_cast<char>('0' + symbol % 10u)
						  : static_cast<char>('a' + symbol - 12u);
		}
		auto const initial{random ^ UINT64_C(0xa5a5a5a5a5a5a5a5)};
		auto const expected{reference_decimal(text, text + size, initial)};
		auto value{initial};
		auto const actual{::fast_io::parse_by_scan(text, text + size, value)};
		assert(static_cast<::std::size_t>(actual.iter - text) ==
			   expected.consumed);
		assert(actual.code == expected.code);
		assert(value == expected.value);
	}
}

#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
void check_eight_digit_validator_exhaustively()
{
	/*
	Varying every byte position through the complete code-unit domain while all
	neighbours are valid zeros exercises both directions of whole-word borrow and
	carry propagation. The packed predicate must admit exactly ASCII '0'--'9';
	for admitted lanes, the independent positional value also verifies reduction.
	*/
	constexpr ::std::uint_least64_t powers[]{
		UINT64_C(10000000), UINT64_C(1000000), UINT64_C(100000),
		UINT64_C(10000), UINT64_C(1000), UINT64_C(100), UINT64_C(10),
		UINT64_C(1)};
	char text[8u]{'0', '0', '0', '0', '0', '0', '0', '0'};
	for (::std::size_t position{}; position != sizeof(text); ++position)
	{
		for (unsigned code_unit{}; code_unit != 256u; ++code_unit)
		{
			text[position] = static_cast<char>(static_cast<unsigned char>(code_unit));
			::std::uint_least64_t value{};
			auto const valid{
				::fast_io::details::scan_int_contiguous_x86_parse_eight_decimal_digits(
					text, value)};
			auto const expected_valid{
				static_cast<unsigned>('0') <= code_unit &&
				code_unit <= static_cast<unsigned>('9')};
			assert(valid == expected_valid);
			if (expected_valid)
			{
				auto const digit{code_unit - static_cast<unsigned>('0')};
				assert(value == static_cast<::std::uint_least64_t>(digit) *
								powers[position]);
			}
		}
		text[position] = '0';
	}
}
#endif

} // namespace

int main()
{
	check_twenty_digit_boundaries();
	check_invalid_cursor_matrix();
	check_range_state_combinations();
	check_differential_matrix();
#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	check_eight_digit_validator_exhaustively();
#endif
}
