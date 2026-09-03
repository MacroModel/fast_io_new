#define FAST_IO_DISABLE_FLOATING_POINT

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <type_traits>
#include <vector>

#include <fast_io_freestanding.h>

namespace unsigned_decimal_specialized_random
{

inline constexpr ::std::uint64_t default_seed{UINT64_C(0x243f6a8885a308d3)};
inline constexpr ::std::size_t default_cases_per_width{UINT64_C(16384)};
inline constexpr ::std::uint64_t publication_sentinel{
	UINT64_C(0x5a5a5a5a5a5a5a5a)};

struct failure_context
{
	char const *path{"startup"};
	::std::uint64_t seed{};
	::std::uint64_t value{};
	::std::size_t width{};
	::std::size_t ordinal{};
};

inline failure_context active_context{};

[[noreturn]] inline void fail(char const *condition) noexcept
{
	::std::fprintf(stderr,
		"unsigned decimal randomized validation failed: condition=%s "
		"path=%s seed=%llu value=%llu width=%zu ordinal=%zu\n",
		condition, active_context.path,
		static_cast<unsigned long long>(active_context.seed),
		static_cast<unsigned long long>(active_context.value),
		active_context.width, active_context.ordinal);
	::std::abort();
}

inline void require(bool condition, char const *description) noexcept
{
	if (!condition) [[unlikely]]
	{
		fail(description);
	}
}

[[nodiscard]] inline constexpr ::std::uint64_t
mix_bijective(::std::uint64_t value) noexcept
{
	/*
	The SplitMix64 finalizer is a permutation of the 64-bit domain: xor-shifts are
	invertible and both multipliers are odd. It is used only to choose the initial
	position and stride of each finite decimal-width permutation, never to reduce
	independent samples modulo a range where collisions would become possible.
	*/
	value ^= value >> 30u;
	value *= UINT64_C(0xbf58476d1ce4e5b9);
	value ^= value >> 27u;
	value *= UINT64_C(0x94d049bb133111eb);
	value ^= value >> 31u;
	return value;
}

[[nodiscard]] inline ::std::uint64_t parse_argument(
	char const *text, char const *name) noexcept
{
	char *end{};
	auto const value{::std::strtoull(text, __builtin_addressof(end), 0)};
	if (text == end || *end != '\0')
	{
		::std::fprintf(stderr, "invalid %s: %s\n", name, text);
		::std::exit(2);
	}
	return static_cast<::std::uint64_t>(value);
}

[[nodiscard]] inline constexpr ::std::uint64_t
advance_permutation(::std::uint64_t position, ::std::uint64_t stride,
	::std::uint64_t modulus) noexcept
{
	/* This branch is the overflow-free form of (position + stride) % modulus. */
	auto const wrap_threshold{modulus - stride};
	return position >= wrap_threshold ? position - wrap_threshold
								  : position + stride;
}

struct width_permutation
{
	::std::uint64_t position{};
	::std::uint64_t stride{};
	::std::uint64_t modulus{};

	[[nodiscard]] inline ::std::uint64_t next() noexcept
	{
		auto const result{position};
		position = advance_permutation(position, stride, modulus);
		return result;
	}
};

[[nodiscard]] inline width_permutation make_width_permutation(
	::std::uint64_t modulus, ::std::uint64_t seed,
	::std::size_t width) noexcept
{
	auto const width_key{static_cast<::std::uint64_t>(width)};
	auto position{mix_bijective(seed ^
		(width_key * UINT64_C(0x9e3779b97f4a7c15))) % modulus};
	auto stride{mix_bijective(seed +
		(width_key * UINT64_C(0xd1b54a32d192ed03))) % modulus};
	if (stride == 0u)
	{
		stride = 1u;
	}
	/* A stride coprime to the interval size visits every residue exactly once.
	   Incrementing and wrapping inside the interval preserves the randomized
	   seed while providing a constructive no-duplicate proof. */
	while (::std::gcd(stride, modulus) != 1u)
	{
		++stride;
		if (stride == modulus)
		{
			stride = 1u;
		}
	}
	return {position, stride, modulus};
}

template <typename unsigned_type>
[[nodiscard]] inline ::std::size_t render_decimal(
	unsigned_type value, char *destination) noexcept
{
	static_assert(::std::is_unsigned_v<unsigned_type>);
	/* `digits10 + 1` is the exact maximum decimal extent of an unsigned type.
	   Keeping the bound formal also lets older GCC prove that the destination
	   objects used by the test are sufficiently large under -Wstringop-overflow. */
	char reversed[(::std::numeric_limits<unsigned_type>::digits10) + 1u];
	::std::size_t size{};
	do
	{
		auto const quotient{value / 10u};
		auto const remainder{static_cast<unsigned>(value - quotient * 10u)};
		reversed[size++] = static_cast<char>('0' + remainder);
		value = quotient;
	} while (value != 0u);
	for (::std::size_t index{}; index != size; ++index)
	{
		destination[index] = reversed[size - index - 1u];
	}
	return size;
}

struct parse_observation
{
	::std::size_t consumed{};
	::fast_io::parse_code code{};
	::std::uint64_t value{};
};

[[nodiscard]] inline constexpr parse_observation reference_decimal(
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

struct fragmented_input;

struct fragmented_input_ref
{
	using input_char_type = char;
	fragmented_input *owner{};
};

struct fragmented_input
{
	using input_char_type = char;
	char const *data{};
	::std::size_t size{};
	::std::size_t position{};
	::std::size_t chunk_size{};
	::std::size_t primitive_calls{};
	::std::size_t normalizations{};
	char buffer[32u]{};
	char *buffer_current{buffer};
	char *buffer_end{buffer};
};

[[nodiscard]] inline fragmented_input_ref
input_stream_ref_define(fragmented_input &input) noexcept
{
	++input.normalizations;
	return {__builtin_addressof(input)};
}

[[nodiscard]] inline constexpr char *ibuffer_begin(
	fragmented_input_ref input) noexcept
{
	return input.owner->buffer;
}

[[nodiscard]] inline constexpr char *ibuffer_curr(
	fragmented_input_ref input) noexcept
{
	return input.owner->buffer_current;
}

[[nodiscard]] inline constexpr char *ibuffer_end(
	fragmented_input_ref input) noexcept
{
	return input.owner->buffer_end;
}

inline constexpr void ibuffer_set_curr(
	fragmented_input_ref input, char *position) noexcept
{
	input.owner->buffer_current = position;
}

[[nodiscard]] inline bool ibuffer_underflow(
	fragmented_input_ref input) noexcept
{
	/*
	A short nonempty refill is progress, not EOF. Replacing the exhausted inline
	window with at most `chunk_size` bytes therefore models every selected split
	of the integer scan context while retaining a single normalized owner.
	*/
	++input.owner->primitive_calls;
	auto count{input.owner->size - input.owner->position};
	if (count > input.owner->chunk_size)
	{
		count = input.owner->chunk_size;
	}
	::std::memcpy(input.owner->buffer,
		input.owner->data + input.owner->position, count);
	input.owner->position += count;
	input.owner->buffer_current = input.owner->buffer;
	input.owner->buffer_end = input.owner->buffer + count;
	return count != 0u;
}

inline void check_fragmented(
	char const *text, ::std::size_t size, ::std::uint64_t expected,
	::std::size_t chunk_size)
{
	fragmented_input input{text, size, 0u, chunk_size};
	::std::uint64_t value{publication_sentinel};
	decltype(auto) input_ref{::fast_io::operations::input_stream_ref(input)};
	auto const success{
		::fast_io::operations::decay::scan_freestanding_decay_borrowed_input(
			input_ref,
			::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(value)))};
	require(success, "fragmented scan success");
	require(value == expected, "fragmented scan value");
	require(input.position == size, "fragmented physical cursor");
	require(input.normalizations == 1u, "fragmented normalization count");
	require(input.primitive_calls != 0u, "fragmented refill count");
}

inline void check_public_front_doors(
	char const *text, ::std::size_t size, ::std::uint64_t expected,
	::std::size_t ordinal)
{
	active_context.path = "parse-exact";
	::std::uint64_t value{publication_sentinel};
	auto result{::fast_io::parse_by_scan(text, text + size, value)};
	require(result.iter == text + size, "exact cursor");
	require(result.code == ::fast_io::parse_code::ok, "exact code");
	require(value == expected, "exact value");

	active_context.path = "parse-delimited";
	char delimited[68u];
	::std::memcpy(delimited, text, size);
	delimited[size] = static_cast<char>((ordinal & 1u) == 0u ? '|' : 'x');
	delimited[size + 1u] = '9';
	value = publication_sentinel;
	result = ::fast_io::parse_by_scan(delimited, delimited + size + 2u, value);
	require(result.iter == delimited + size, "delimiter cursor");
	require(result.code == ::fast_io::parse_code::ok, "delimiter code");
	require(value == expected, "delimiter value");

	active_context.path = "to";
	auto const view{::fast_io::mnp::strvw(text, text + size)};
	auto const converted{::fast_io::to<::std::uint64_t>(view)};
	require(converted == expected, "to value");

	active_context.path = "inplace-to";
	value = publication_sentinel;
	::fast_io::inplace_to(value, view);
	require(value == expected, "inplace_to value");
}

inline void check_alternate_code_units(
	char const *text, ::std::size_t size, ::std::uint64_t expected)
{
	char8_t u8text[32u];
	unsigned char unsigned_text[32u];
	for (::std::size_t index{}; index != size; ++index)
	{
		u8text[index] = static_cast<char8_t>(text[index]);
		unsigned_text[index] = static_cast<unsigned char>(text[index]);
	}
	active_context.path = "char8_t";
	::std::uint64_t value{publication_sentinel};
	auto result{::fast_io::parse_by_scan(u8text, u8text + size, value)};
	require(result.iter == u8text + size, "char8_t cursor");
	require(result.code == ::fast_io::parse_code::ok, "char8_t code");
	require(value == expected, "char8_t value");

	active_context.path = "unsigned-char";
	value = publication_sentinel;
	auto unsigned_result{::fast_io::parse_by_scan(
		unsigned_text, unsigned_text + size, value)};
	require(unsigned_result.iter == unsigned_text + size,
		"unsigned char cursor");
	require(unsigned_result.code == ::fast_io::parse_code::ok,
		"unsigned char code");
	require(value == expected, "unsigned char value");
}

inline void check_exact_heap_extent(
	char const *text, ::std::size_t size, ::std::uint64_t expected)
{
	/* The allocation has exactly the advertised extent. ASan therefore turns
	   any speculative full-group read beyond `last` into a deterministic failure. */
	auto exact{::std::make_unique<char[]>(size)};
	::std::memcpy(exact.get(), text, size);
	active_context.path = "heap-exact-extent";
	::std::uint64_t value{publication_sentinel};
	auto const result{
		::fast_io::parse_by_scan(exact.get(), exact.get() + size, value)};
	require(result.iter == exact.get() + size, "heap cursor");
	require(result.code == ::fast_io::parse_code::ok, "heap code");
	require(value == expected, "heap value");
}

inline void check_random_invalid(
	char const *digits, ::std::size_t size, ::std::size_t ordinal,
	::std::uint64_t random)
{
	char text[68u];
	::std::memcpy(text, digits, size);
	auto const position{static_cast<::std::size_t>(random % size)};
	static constexpr unsigned char invalid_code_units[]{
		33u, 47u, 58u, 64u, 91u, 96u, 123u, 127u, 128u, 191u, 255u};
	auto const replacement{invalid_code_units[
		(random >> 8u) % ::std::size(invalid_code_units)]};
	text[position] = static_cast<char>(replacement);
	/* A second random suffix ensures that replay after a failed packed proof sees
	   the complete original range instead of an accidentally shortened fixture. */
	auto const suffix_size{static_cast<::std::size_t>((random >> 16u) & 7u)};
	for (::std::size_t index{}; index != suffix_size; ++index)
	{
		text[size + index] = static_cast<char>('0' +
			((random >> ((index & 7u) * 8u)) % 10u));
	}
	auto const range_size{size + suffix_size};
	auto const initial{publication_sentinel ^ random ^ ordinal};
	auto const expected{reference_decimal(text, text + range_size, initial)};
	active_context.path = "random-invalid";
	auto value{initial};
	auto const result{
		::fast_io::parse_by_scan(text, text + range_size, value)};
	require(static_cast<::std::size_t>(result.iter - text) == expected.consumed,
		"invalid cursor");
	require(result.code == expected.code, "invalid code");
	require(value == expected.value, "invalid publication");
}

inline void check_signed_mirror(
	char const *digits, ::std::size_t size, ::std::uint64_t magnitude)
{
	if (magnitude >
		static_cast<::std::uint64_t>((::std::numeric_limits<::std::int64_t>::max)()))
	{
		return;
	}
	char negative[32u];
	negative[0] = '-';
	::std::memcpy(negative + 1u, digits, size);
	active_context.path = "signed-negative";
	::std::int64_t value{};
	auto const result{::fast_io::parse_by_scan(
		negative, negative + size + 1u, value)};
	require(result.iter == negative + size + 1u, "signed cursor");
	require(result.code == ::fast_io::parse_code::ok, "signed code");
	require(value == -static_cast<::std::int64_t>(magnitude), "signed value");
}

template <typename target_type>
inline void check_narrow_unsigned_target(
	char const *digits, ::std::size_t size, ::std::uint64_t expected)
{
	static_assert(::std::is_unsigned_v<target_type>);
	constexpr auto maximum{(::std::numeric_limits<target_type>::max)()};
	constexpr auto initial{static_cast<target_type>(0x5a5au)};
	target_type value{initial};
	auto const result{
		::fast_io::parse_by_scan(digits, digits + size, value)};
	require(result.iter == digits + size, "narrow target cursor");
	if (expected <= static_cast<::std::uint64_t>(maximum))
	{
		require(result.code == ::fast_io::parse_code::ok,
			"narrow target success code");
		require(value == static_cast<target_type>(expected),
			"narrow target value");
	}
	else
	{
		require(result.code == ::fast_io::parse_code::overflow,
			"narrow target overflow code");
		require(value == initial, "narrow target overflow publication");
	}
}

inline ::std::size_t check_unique_valid_numbers(
	::std::size_t cases_per_width, ::std::uint64_t seed,
	::std::uint64_t &digest)
{
	static constexpr ::std::uint64_t powers_of_ten[]{
		UINT64_C(1), UINT64_C(10), UINT64_C(100), UINT64_C(1000),
		UINT64_C(10000), UINT64_C(100000), UINT64_C(1000000),
		UINT64_C(10000000), UINT64_C(100000000), UINT64_C(1000000000),
		UINT64_C(10000000000), UINT64_C(100000000000),
		UINT64_C(1000000000000), UINT64_C(10000000000000),
		UINT64_C(100000000000000), UINT64_C(1000000000000000),
		UINT64_C(10000000000000000), UINT64_C(100000000000000000),
		UINT64_C(1000000000000000000), UINT64_C(10000000000000000000)};
	static constexpr ::std::size_t chunk_sizes[]{
		1u, 2u, 3u, 4u, 7u, 8u, 9u, 12u, 15u, 16u, 19u, 20u, 21u};

	::std::vector<::std::uint64_t> generated;
	generated.reserve(cases_per_width * 20u + 1u);
	generated.push_back(0u);
	char storage[96u];
	for (::std::size_t width{1u}; width <= 20u; ++width)
	{
		auto const lower{powers_of_ten[width - 1u]};
		auto const upper{width == 20u
			? (::std::numeric_limits<::std::uint64_t>::max)()
			: powers_of_ten[width] - 1u};
		auto const modulus{upper - lower + 1u};
		auto permutation{make_width_permutation(modulus, seed, width)};
		auto const count{static_cast<::std::size_t>(
			(::std::min)(static_cast<::std::uint64_t>(cases_per_width), modulus))};
		for (::std::size_t ordinal{}; ordinal != count; ++ordinal)
		{
			auto const value{lower + permutation.next()};
			active_context = {"render", seed, value, width, ordinal};
			/* Varying the address modulo 32 exercises every unaligned eight-byte load
			   shape without changing the logical half-open range. */
			auto const offset{static_cast<::std::size_t>(
				mix_bijective(value ^ seed) & 31u)};
			auto *const text{storage + offset};
			auto const size{render_decimal(value, text)};
			require(size == width, "rendered width");
			generated.push_back(value);
			check_public_front_doors(text, size, value, ordinal);
			digest = (digest ^ value ^ static_cast<::std::uint64_t>(size)) *
				UINT64_C(1099511628211);

			if ((ordinal & 63u) == 0u)
			{
				check_alternate_code_units(text, size, value);
				check_signed_mirror(text, size, value);
			}
			if ((ordinal & 255u) == 0u)
			{
				check_exact_heap_extent(text, size, value);
				active_context.path = "narrow-uint16";
				check_narrow_unsigned_target<::std::uint16_t>(text, size, value);
				active_context.path = "narrow-uint32";
				check_narrow_unsigned_target<::std::uint32_t>(text, size, value);
				if (ordinal == 0u)
				{
					/* The first value of every decimal width crosses every physical
					   refill size, explicitly covering the width/chunk Cartesian edges. */
					for (auto const chunk_size : chunk_sizes)
					{
						active_context.path = "fragmented-width-boundary";
						check_fragmented(text, size, value, chunk_size);
					}
				}
				else
				{
					auto const chunk_index{static_cast<::std::size_t>(
						mix_bijective(value + seed) % ::std::size(chunk_sizes))};
					active_context.path = "fragmented-random";
					check_fragmented(text, size, value, chunk_sizes[chunk_index]);
				}
			}
			if ((ordinal & 3u) == 0u)
			{
				check_random_invalid(text, size, ordinal,
					mix_bijective(value ^ seed ^ ordinal));
			}
		}
	}

	/* Adjacent decimal widths occupy disjoint intervals. Within one width the
	   coprime modular walk is a permutation. Sorting and checking the assembled
	   corpus independently verifies that the generator honored both proofs. */
	active_context.path = "duplicate-audit";
	::std::sort(generated.begin(), generated.end());
	auto const duplicate{::std::adjacent_find(generated.begin(), generated.end())};
	require(duplicate == generated.end(), "duplicate randomized number");
	return generated.size();
}

inline void check_overflow_corpus(
	::std::size_t cases, ::std::uint64_t seed, ::std::uint64_t &digest)
{
	constexpr ::std::uint64_t suffix_modulus{UINT64_C(10000000000000000000)};
	auto permutation{make_width_permutation(
		suffix_modulus, seed ^ UINT64_C(0xa4093822299f31d0), 20u)};
	char text[68u];
	for (::std::size_t ordinal{}; ordinal != cases; ++ordinal)
	{
		/* Every decimal beginning with '2' and containing twenty digits exceeds
		   UINT64_MAX. The nineteen-digit suffix is a coprime modular permutation,
		   so the overflow strings are randomized, standard C++20, and collision-
		   free without relying on a nonstandard 128-bit integer extension. */
		auto const suffix{permutation.next()};
		text[0] = '2';
		char suffix_digits[24u];
		auto const suffix_size{render_decimal(suffix, suffix_digits)};
		auto const padding{19u - suffix_size};
		::std::memset(text + 1u, '0', padding);
		::std::memcpy(text + 1u + padding, suffix_digits, suffix_size);
		constexpr ::std::size_t size{20u};
		active_context = {"overflow", seed,
			suffix, size, ordinal};
		::std::uint64_t parsed{publication_sentinel};
		auto const result{
			::fast_io::parse_by_scan(text, text + size, parsed)};
		require(result.iter == text + size, "overflow cursor");
		require(result.code == ::fast_io::parse_code::overflow, "overflow code");
		require(parsed == publication_sentinel, "overflow publication");

		auto random{mix_bijective(suffix ^ seed ^ ordinal)};
		auto const extended_size{
			static_cast<::std::size_t>(21u + random % 44u)};
		for (::std::size_t index{20u}; index != extended_size; ++index)
		{
			random = mix_bijective(random + index);
			text[index] = static_cast<char>('0' + random % 10u);
		}
		active_context.path = "long-overflow";
		parsed = publication_sentinel;
		auto extended_result{::fast_io::parse_by_scan(
			text, text + extended_size, parsed)};
		require(extended_result.iter == text + extended_size,
			"long overflow cursor");
		require(extended_result.code == ::fast_io::parse_code::overflow,
			"long overflow code");
		require(parsed == publication_sentinel,
			"long overflow publication");

		text[extended_size] = '|';
		active_context.path = "delimited-long-overflow";
		parsed = publication_sentinel;
		extended_result = ::fast_io::parse_by_scan(
			text, text + extended_size + 1u, parsed);
		require(extended_result.iter == text + extended_size,
			"delimited long overflow cursor");
		require(extended_result.code == ::fast_io::parse_code::overflow,
			"delimited long overflow code");
		require(parsed == publication_sentinel,
			"delimited long overflow publication");

		char leading_zero[68u];
		leading_zero[0] = '0';
		::std::memcpy(leading_zero + 1u, text, extended_size);
		active_context.path = "leading-zero";
		parsed = publication_sentinel;
		auto const leading_zero_result{::fast_io::parse_by_scan(
			leading_zero, leading_zero + extended_size + 1u, parsed)};
		require(leading_zero_result.iter == leading_zero + 1u,
			"leading-zero cursor");
		require(leading_zero_result.code == ::fast_io::parse_code::invalid,
			"leading-zero code");
		require(parsed == publication_sentinel, "leading-zero publication");

		digest = (digest ^ suffix ^ random) * UINT64_C(1099511628211);
	}
}

} // namespace unsigned_decimal_specialized_random

int main(int argc, char **argv)
{
	using namespace unsigned_decimal_specialized_random;
	if (argc > 3)
	{
		::std::fputs(
			"usage: unsigned_decimal_specialized_random [cases-per-width] [seed]\n",
			stderr);
		return 2;
	}
	auto const cases_per_width{argc >= 2
		? static_cast<::std::size_t>(parse_argument(argv[1], "cases-per-width"))
		: default_cases_per_width};
	auto const seed{argc == 3 ? parse_argument(argv[2], "seed") : default_seed};
	if (cases_per_width == 0u || cases_per_width > UINT64_C(1000000))
	{
		::std::fputs("cases-per-width must be in [1, 1000000]\n", stderr);
		return 2;
	}
	active_context.seed = seed;
	::std::uint64_t digest{UINT64_C(14695981039346656037)};
	auto const unique_count{
		check_unique_valid_numbers(cases_per_width, seed, digest)};
	check_overflow_corpus(cases_per_width, seed, digest);
	::std::printf(
		"ok seed=%llu requested-per-width=%zu unique-valid=%zu "
		"overflow=%zu digest=%llu\n",
		static_cast<unsigned long long>(seed), cases_per_width, unique_count,
		cases_per_width, static_cast<unsigned long long>(digest));
}
