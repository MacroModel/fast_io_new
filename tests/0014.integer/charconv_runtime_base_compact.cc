#include <fast_io_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <system_error>

namespace
{

template <unsigned base>
constexpr bool check_literal_constant_evaluation() noexcept
{
	constexpr ::std::array<char, 4u> input{
		::fast_io::char_literal_v<u8'1', char>,
		::fast_io::char_literal_v<u8'2', char>,
		::fast_io::char_literal_v<u8'3', char>,
		::fast_io::char_literal_v<u8'#', char>};
	::std::uint_least64_t value{};
	auto const result{::fast_io::from_chars(
		input.data(), input.data() + input.size(), value, static_cast<int>(base))};
	return result.ptr == input.data() + 3u && result.ec == ::std::errc{} &&
		   value == static_cast<::std::uint_least64_t>(base * base + 2u * base + 3u);
}

static_assert(check_literal_constant_evaluation<11u>());
static_assert(check_literal_constant_evaluation<12u>());
static_assert(check_literal_constant_evaluation<13u>());
static_assert(check_literal_constant_evaluation<14u>());
static_assert(check_literal_constant_evaluation<15u>());
static_assert(check_literal_constant_evaluation<17u>());
static_assert(check_literal_constant_evaluation<36u>());

template <typename integer_type>
constexpr bool check_narrow_runtime_power_constant_evaluation() noexcept
{
	constexpr ::std::array<char, 1u> input{
		::fast_io::char_literal_v<u8'#', char>};
	constexpr integer_type initial{static_cast<integer_type>(0x5au)};
	integer_type value{initial};
	auto const result{
		::fast_io::details::from_chars_integral_runtime_base_compact(
			input.data(), input.data(), value, 15u)};
	return result.ptr == input.data() &&
		   result.ec == ::std::errc::invalid_argument &&
		   value == initial;
}

/*
The compact scanner precomputes B^8 before inspecting the input.  An empty
range therefore provides the smallest compile-time regression for narrow
integer promotion: on ordinary 32-bit-int ABIs, the `uint_least16_t` case
rejects a signed B^4 * B^4.  On a 16-bit-int target where `uint_least8_t` has
lower rank than `int` (as on AVR), the second assertion also exposes an earlier
signed B^2 * B^2.  Base 15 reaches both boundaries even though no digit is
consumed.  Checking the unchanged destination also preserves the public
invalid-input contract while keeping this test independent of the digit loop.
*/
static_assert(check_narrow_runtime_power_constant_evaluation<::std::uint_least8_t>());
static_assert(check_narrow_runtime_power_constant_evaluation<::std::uint_least16_t>());

[[noreturn]] inline void fail() noexcept
{
	::std::abort();
}

template <typename char_type, typename integer_type>
inline void check_one(char_type const *first, char_type const *last,
					  integer_type initial, int base)
{
	integer_type fixed_value{initial};
	auto const fixed{::fast_io::details::from_chars_integral_literal_base(
		first, last, fixed_value, base)};

	integer_type compact_value{initial};
	auto const compact{::fast_io::details::from_chars_integral_runtime_base_compact(
		first, last, compact_value, static_cast<unsigned>(base))};

	// A volatile load prevents the public wrapper from treating this call as a
	// literal-base probe. This checks the same entry that production code reaches
	// when the radix is known only at run time.
	int volatile dynamic_base{base};
	integer_type public_value{initial};
	auto const public_result{
		::fast_io::from_chars(first, last, public_value, dynamic_base)};

	auto const fixed_offset{fixed.ptr - first};
	if (compact.ptr - first != fixed_offset || compact.ec != fixed.ec ||
		compact_value != fixed_value || public_result.ptr - first != fixed_offset ||
		public_result.ec != fixed.ec || public_value != fixed_value)
	{
		fail();
	}
}

#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <unsigned base, typename char_type, typename integer_type>
inline void check_literal_one(char_type const *first, char_type const *last,
							  integer_type initial)
{
	integer_type reference_value{initial};
	auto const reference{::fast_io::details::from_chars_integral_literal_base(
		first, last, reference_value, static_cast<int>(base))};

	integer_type public_value{initial};
	auto const public_result{
		::fast_io::from_chars(first, last, public_value, static_cast<int>(base))};
	if (public_result.ptr - first != reference.ptr - first ||
		public_result.ec != reference.ec || public_value != reference_value)
	{
		fail();
	}
}

#if __GNUC__ == 15 && defined(__SSE4_1__) && !defined(__AVX__)
template <unsigned base = 17u, typename char_type, typename integer_type>
inline void check_literal_upper_base(char_type const *first,
									 char_type const *last,
									 integer_type initial, int selected_base)
{
	if (selected_base == static_cast<int>(base))
	{
		check_literal_one<base>(first, last, initial);
	}
	if constexpr (base != 36u)
	{
		check_literal_upper_base<base + 1u>(first, last, initial,
											selected_base);
	}
}
#endif

template <typename char_type, typename integer_type>
inline void check_literal_mid_base(char_type const *first, char_type const *last,
								   integer_type initial, int base)
{
	switch (base)
	{
	case 11:
		check_literal_one<11u>(first, last, initial);
		break;
	case 12:
		check_literal_one<12u>(first, last, initial);
		break;
	case 13:
		check_literal_one<13u>(first, last, initial);
		break;
	case 14:
		check_literal_one<14u>(first, last, initial);
		break;
	case 15:
		check_literal_one<15u>(first, last, initial);
		break;
	default:
		break;
	}
#if __GNUC__ == 15 && defined(__SSE4_1__) && !defined(__AVX__)
	check_literal_upper_base(first, last, initial, base);
#endif
}
#endif

template <typename char_type, typename integer_type>
inline void check_matrix()
{
	::std::array<char_type, 160u> buffer{};
	::std::uint_least64_t state{UINT64_C(0x9e3779b97f4a7c15)};
	auto next_random = [&state]() noexcept {
		state ^= state << 7u;
		state ^= state >> 9u;
		state ^= state << 8u;
		return state;
	};

	for (int base{2}; base != 37; ++base)
	{
		for (::std::size_t length{}; length != 129u; ++length)
		{
			for (::std::size_t index{}; index != length; ++index)
			{
				auto const selector{static_cast<unsigned>(next_random() % 78u)};
				if (selector < 36u)
				{
					buffer[index] = ::fast_io::details::charliteralofnumber<
						char_type, false>(static_cast<char8_t>(selector));
				}
				else if (selector < 72u)
				{
					buffer[index] = ::fast_io::details::charliteralofnumber<
						char_type, true>(static_cast<char8_t>(selector - 36u));
				}
				else
				{
					switch (selector)
					{
					case 72u:
						buffer[index] = ::fast_io::char_literal_v<u8'-', char_type>;
						break;
					case 73u:
						buffer[index] = ::fast_io::char_literal_v<u8'+', char_type>;
						break;
					case 74u:
						buffer[index] = ::fast_io::char_literal_v<u8'#', char_type>;
						break;
					default:
						buffer[index] = static_cast<char_type>(0x7fu + selector);
						break;
					}
				}
			}
			auto const initial{static_cast<integer_type>(next_random())};
			check_one(buffer.data(), buffer.data() + length, initial, base);
#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			check_literal_mid_base(buffer.data(), buffer.data() + length,
								   initial, base);
#endif

			// Repeated maximum digits cover every length, including the exact
			// destination boundary and the complete overflow-digit continuation.
			auto const maximum_digit{::fast_io::details::charliteralofnumber<
				char_type, false>(static_cast<char8_t>(base - 1))};
			for (::std::size_t index{}; index != length; ++index)
			{
				buffer[index] = maximum_digit;
			}
			check_one(buffer.data(), buffer.data() + length, initial, base);
#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			check_literal_mid_base(buffer.data(), buffer.data() + length,
								   initial, base);
#endif
			if (length != buffer.size())
			{
				buffer[length] = ::fast_io::char_literal_v<u8'#', char_type>;
				check_one(buffer.data(), buffer.data() + length + 1u, initial, base);
#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
				check_literal_mid_base(buffer.data(), buffer.data() + length + 1u,
									   initial, base);
#endif
			}
		}

		constexpr ::std::array<integer_type, 5u> values{
			integer_type{}, static_cast<integer_type>(1), static_cast<integer_type>(-1),
			::std::numeric_limits<integer_type>::min(),
			::std::numeric_limits<integer_type>::max()};
		for (auto const value : values)
		{
			auto const formatted{::fast_io::to_chars(
				buffer.data(), buffer.data() + buffer.size(), value, base)};
			if (formatted.ec != ::std::errc{})
			{
				fail();
			}
			check_one(buffer.data(), formatted.ptr,
					  static_cast<integer_type>(next_random()), base);
			*formatted.ptr = ::fast_io::char_literal_v<u8'#', char_type>;
			check_one(buffer.data(), formatted.ptr + 1u,
					  static_cast<integer_type>(next_random()), base);
		}
	}
}

template <typename char_type>
inline void check_character()
{
	check_matrix<char_type, ::std::uint_least8_t>();
	check_matrix<char_type, ::std::int_least8_t>();
	check_matrix<char_type, ::std::uint_least16_t>();
	check_matrix<char_type, ::std::int_least16_t>();
	check_matrix<char_type, ::std::uint_least32_t>();
	check_matrix<char_type, ::std::int_least32_t>();
	check_matrix<char_type, ::std::uint_least64_t>();
	check_matrix<char_type, ::std::int_least64_t>();
#if defined(__SIZEOF_INT128__)
	check_matrix<char_type, __uint128_t>();
	check_matrix<char_type, __int128_t>();
#endif
}

} // namespace

int main()
{
	check_character<char>();
	check_character<wchar_t>();
	check_character<char8_t>();
	check_character<char16_t>();
	check_character<char32_t>();
}
