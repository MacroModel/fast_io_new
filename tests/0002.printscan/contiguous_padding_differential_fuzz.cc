#include <fast_io.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef FAST_IO_PADDING_FUZZ_CHAR_INDEX
#define FAST_IO_PADDING_FUZZ_CHAR_INDEX 5
#endif

namespace
{

struct splitmix64
{
	::std::uint_least64_t state{};

	[[nodiscard]] inline ::std::uint_least64_t next() noexcept
	{
		auto value{state += UINT64_C(0x9e3779b97f4a7c15)};
		value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
		value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
		return value ^ (value >> 31u);
	}
};

struct fuzz_configuration
{
	::std::size_t shard{};
	::std::size_t shards{1u};
	::std::size_t iterations{1024u};
	::std::size_t first_iteration{};
	::std::uint_least64_t seed{UINT64_C(0x6a09e667f3bcc909)};
};

struct fuzz_statistics
{
	::std::uint_least64_t configurations{};
	::std::uint_least64_t inputs{};
	::std::uint_least64_t semantic_code_units{};
};

template <::std::integral char_type>
struct fuzz_padded_range
{
	::std::vector<char_type> *storage{};
	::std::size_t semantic_size{};
	::std::size_t padding{};

	[[nodiscard]] char_type *begin() noexcept
	{
		return storage->data();
	}

	[[nodiscard]] char_type const *begin() const noexcept
	{
		return storage->data();
	}

	[[nodiscard]] char_type *end() noexcept
	{
		return storage->data() + semantic_size;
	}

	[[nodiscard]] char_type const *end() const noexcept
	{
		return storage->data() + semantic_size;
	}
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::size_t
contiguous_range_padding_size(
	fuzz_padded_range<char_type> const &range) noexcept
{
	return range.padding;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr char_type ascii_runtime(char8_t value) noexcept
{
	return static_cast<char_type>(value);
}

[[nodiscard]] inline constexpr char8_t digit_ascii(
	::std::size_t value) noexcept
{
	return static_cast<char8_t>(
		value < 10u ? static_cast<unsigned>(u8'0') + value
					: static_cast<unsigned>(u8'a') + value - 10u);
}

template <::std::integral char_type>
[[nodiscard]] inline ::std::size_t bounded_offset(
	char_type const *begin, ::std::size_t size,
	char_type const *result) noexcept
{
	for (::std::size_t index{}; index <= size; ++index)
	{
		if (result == begin + index)
		{
			return index;
		}
	}
	return ::std::numeric_limits<::std::size_t>::max();
}

[[nodiscard]] inline ::std::size_t integer_length(
	::std::size_t iteration, splitmix64 &random) noexcept
{
	constexpr ::std::size_t boundaries[]{
		0u,  1u,  2u,  3u,  4u,  7u,  8u,  9u,  15u, 16u, 17u,
		19u, 20u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u,
		129u, 255u, 256u};
	if (iteration < ::std::size(boundaries))
	{
		return boundaries[iteration];
	}
	return static_cast<::std::size_t>(random.next() % 257u);
}

[[nodiscard]] inline ::std::size_t floating_length(
	::std::size_t iteration, splitmix64 &random) noexcept
{
	constexpr ::std::size_t boundaries[]{
		0u,	  1u,	  2u,	  3u,	  7u,	  8u,	  9u,	  15u,
		16u,  17u,	  18u,	  19u,	  20u,	  21u,	  31u,  32u,
		33u,  63u,	  64u,	  65u,	  127u,  128u,  129u, 255u,
		256u, 257u,  767u,  768u,  769u,  831u,  832u, 833u,
		11999u, 12000u, 12001u, 12031u, 12032u, 12033u};
	if (iteration < ::std::size(boundaries))
	{
		return boundaries[iteration];
	}
	if ((random.next() & 31u) == 0u)
	{
		constexpr ::std::size_t centers[]{128u, 768u, 832u, 12000u};
		auto const center{
			centers[static_cast<::std::size_t>(random.next() %
											  ::std::size(centers))]};
		auto const delta{static_cast<::std::size_t>(random.next() % 65u)};
		return center + delta;
	}
	return static_cast<::std::size_t>(random.next() % 513u);
}

template <::std::integral char_type, ::std::size_t base, typename integer_type>
inline void make_integer_input(
	::std::vector<char_type> &storage, ::std::size_t semantic_size,
	splitmix64 &random) noexcept
{
	auto const mode{static_cast<unsigned>(random.next() % 8u)};
	for (::std::size_t index{}; index != semantic_size; ++index)
	{
		char8_t value{};
		switch (mode)
		{
		case 0u:
			value = digit_ascii(static_cast<::std::size_t>(
				random.next() % base));
			break;
		case 1u:
			if (index < 2u)
			{
				value = u8' ';
			}
			else if (index == 2u &&
					 ::fast_io::details::my_signed_integral<integer_type>)
			{
				value = u8'-';
			}
			else
			{
				value = digit_ascii(static_cast<::std::size_t>(
					random.next() % base));
			}
			break;
		case 2u:
			value = index == semantic_size / 2u
						? u8'/'
						: digit_ascii(static_cast<::std::size_t>(
							  random.next() % base));
			break;
		case 3u:
			value = digit_ascii(index % base);
			break;
		case 4u:
			value = u8'0';
			break;
		case 5u:
			value = index + 1u == semantic_size ? u8'/'
												 : digit_ascii(base - 1u);
			break;
		case 6u:
		{
			constexpr char8_t alphabet[]{
				u8'0', u8'1', u8'8', u8'9', u8'a', u8'f', u8'g',
				u8'z', u8'+', u8'-', u8' ', u8'/', u8'.'};
			value = alphabet[static_cast<::std::size_t>(
				random.next() % ::std::size(alphabet))];
			break;
		}
		default:
			value = index == 0u ? u8'1' : digit_ascii(index % base);
			break;
		}
		storage[index] = ascii_runtime<char_type>(value);
	}
}

template <::std::integral char_type>
inline void make_floating_input(
	::std::vector<char_type> &storage, ::std::size_t semantic_size,
	splitmix64 &random) noexcept
{
	auto const mode{static_cast<unsigned>(random.next() % 10u)};
	for (::std::size_t index{}; index != semantic_size; ++index)
	{
		char8_t value{};
		switch (mode)
		{
		case 0u:
			value = u8'0';
			break;
		case 1u:
			value = index == 0u ? u8'1' : u8'0';
			break;
		case 2u:
			value = digit_ascii(index % 10u);
			break;
		case 3u:
			value = index == semantic_size / 2u ? u8'/' : u8'9';
			break;
		case 4u:
			value = index == 1u ? u8'.' : digit_ascii(index % 10u);
			break;
		case 5u:
			if (index == 1u)
			{
				value = u8'.';
			}
			else if (semantic_size > 6u && index == semantic_size - 5u)
			{
				value = u8'e';
			}
			else if (semantic_size > 6u && index == semantic_size - 4u)
			{
				value = u8'-';
			}
			else
			{
				value = digit_ascii(index % 10u);
			}
			break;
		case 6u:
		{
			constexpr char8_t infinity[]{u8'i', u8'n', u8'f', u8'i',
										u8'n', u8'i', u8't', u8'y'};
			value = infinity[index % ::std::size(infinity)];
			break;
		}
		case 7u:
		{
			constexpr char8_t nan_payload[]{
				u8'n', u8'a', u8'n', u8'(', u8's', u8'n', u8'a',
				u8'n', u8')', u8'/'};
			value = nan_payload[index % ::std::size(nan_payload)];
			break;
		}
		case 8u:
		{
			constexpr char8_t alphabet[]{
				u8'0', u8'1', u8'8', u8'9', u8'e', u8'E', u8'+',
				u8'-', u8'.', u8' ', u8'/', u8'i', u8'n', u8'f',
				u8'a', u8'(' , u8')'};
			value = alphabet[static_cast<::std::size_t>(
				random.next() % ::std::size(alphabet))];
			break;
		}
		default:
			value = index < 2u ? u8' ' : (index == 2u ? u8'-' : u8'7');
			break;
		}
		storage[index] = ascii_runtime<char_type>(value);
	}
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_floating_bits(
	floating_type left, floating_type right) noexcept
{
	auto const left_fields{::fast_io::details::get_punned_result(left)};
	auto const right_fields{::fast_io::details::get_punned_result(right)};
	return left_fields.mantissa == right_fields.mantissa &&
		   left_fields.exponent == right_fields.exponent &&
		   left_fields.sign == right_fields.sign;
}

template <::std::size_t char_index, ::std::size_t integer_index,
		  ::std::size_t base, ::std::integral char_type,
		  ::fast_io::details::my_integral integer_type>
[[nodiscard]] bool fuzz_integer_configuration(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	constexpr ::std::size_t case_index{
		char_index * 10u * 35u + integer_index * 35u + (base - 2u)};
	if (case_index % configuration.shards != configuration.shard)
	{
		return true;
	}
	++statistics.configurations;
	splitmix64 random{
		configuration.seed ^
		(static_cast<::std::uint_least64_t>(case_index) *
		 UINT64_C(0xd6e8feb86659fd93))};
	constexpr auto flags{
		::fast_io::details::base_scan_mani_flags_cache<
			base, false, false, false, false>};
	using manipulator_type =
		::fast_io::manipulators::scalar_manip_t<flags, integer_type &>;
	constexpr ::std::size_t maximum_padding{64u};
	for (::std::size_t step{}; step != configuration.iterations; ++step)
	{
		/*
		The libFuzzer byte stream selects an abstract iteration number rather
		than allocating a semantic buffer of that byte-stream length.  The
		first boundary-table indices therefore force every proof boundary into
		the seed corpus, while larger mutated indices select arbitrary lengths.
		Addition is modulo size_t; integer_length uses the wrapped value only as
		a table selector and otherwise derives the length from `random`, so
		wrapping cannot create an out-of-bounds access.
		*/
		auto const iteration{configuration.first_iteration + step};
		auto const semantic_size{integer_length(iteration, random)};
		auto const padding{static_cast<::std::size_t>(
			random.next() % (maximum_padding + 1u))};
		::std::vector<char_type> digit_padding(
			semantic_size + maximum_padding);
		::std::vector<char_type> nondigit_padding(
			semantic_size + maximum_padding);
		make_integer_input<char_type, base, integer_type>(
			digit_padding, semantic_size, random);
		for (::std::size_t index{}; index != semantic_size; ++index)
		{
			nondigit_padding[index] = digit_padding[index];
		}
		for (::std::size_t index{semantic_size};
			 index != semantic_size + maximum_padding; ++index)
		{
			digit_padding[index] = ascii_runtime<char_type>(u8'1');
			nondigit_padding[index] = ascii_runtime<char_type>(u8'/');
		}

		auto const initial_bits{random.next()};
		integer_type ordinary_value{static_cast<integer_type>(initial_bits)};
		integer_type digit_padding_value{ordinary_value};
		integer_type nondigit_padding_value{ordinary_value};
		auto const *const ordinary_begin{digit_padding.data()};
		auto const *const ordinary_end{ordinary_begin + semantic_size};
		auto const ordinary_result{scan_contiguous_define(
			::fast_io::io_reserve_type<char_type, manipulator_type>,
			ordinary_begin, ordinary_end,
			manipulator_type{ordinary_value})};
		fuzz_padded_range<char_type> digit_range{
			__builtin_addressof(digit_padding), semantic_size, padding};
		fuzz_padded_range<char_type> nondigit_range{
			__builtin_addressof(nondigit_padding), semantic_size, padding};
		::fast_io::basic_padded_ibuffer_view<char_type> digit_input{digit_range};
		::fast_io::basic_padded_ibuffer_view<char_type> nondigit_input{
			nondigit_range};
		auto digit_input_ref{
			::fast_io::input_stream_ref_define(digit_input)};
		auto nondigit_input_ref{
			::fast_io::input_stream_ref_define(nondigit_input)};
		manipulator_type digit_manipulator{digit_padding_value};
		manipulator_type nondigit_manipulator{nondigit_padding_value};
		auto const digit_padding_result{
			::fast_io::details::scan_contiguous_invoke(
				digit_input_ref, digit_input.curr_ptr,
				digit_input.end_ptr, digit_manipulator)};
		auto const nondigit_padding_result{
			::fast_io::details::scan_contiguous_invoke(
				nondigit_input_ref, nondigit_input.curr_ptr,
				nondigit_input.end_ptr, nondigit_manipulator)};

		auto const ordinary_offset{bounded_offset(
			ordinary_begin, semantic_size, ordinary_result.iter)};
		auto const digit_padding_offset{bounded_offset(
			digit_padding.data(), semantic_size,
			digit_padding_result.iter)};
		auto const nondigit_padding_offset{bounded_offset(
			nondigit_padding.data(), semantic_size,
			nondigit_padding_result.iter)};
		if (ordinary_result.code != digit_padding_result.code ||
			ordinary_result.code != nondigit_padding_result.code ||
			ordinary_offset != digit_padding_offset ||
			ordinary_offset != nondigit_padding_offset ||
			ordinary_value != digit_padding_value ||
			ordinary_value != nondigit_padding_value)
		{
			::std::fprintf(
				stderr,
				"integer mismatch case=%zu char=%zu type=%zu base=%zu "
				"iteration=%zu length=%zu padding=%zu codes=%u/%u/%u "
				"offsets=%zu/%zu/%zu\n",
				case_index, char_index, integer_index, base, iteration,
				semantic_size, padding,
				static_cast<unsigned>(ordinary_result.code),
				static_cast<unsigned>(digit_padding_result.code),
				static_cast<unsigned>(nondigit_padding_result.code),
				ordinary_offset, digit_padding_offset,
				nondigit_padding_offset);
			return false;
		}
		++statistics.inputs;
		statistics.semantic_code_units += semantic_size;
	}
	return true;
}

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr auto floating_fuzz_flags{[]() constexpr noexcept {
	auto flags{
		::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.rounding = rounding;
	return flags;
}()};

template <::std::size_t char_index, ::std::size_t floating_index,
		  ::std::size_t rounding_index, ::std::integral char_type,
		  typename floating_type,
		  ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] bool fuzz_floating_configuration(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	constexpr ::std::size_t integer_configuration_count{5u * 10u * 35u};
	constexpr ::std::size_t case_index{
		integer_configuration_count + char_index * 6u * 10u +
		floating_index * 10u + rounding_index};
	if (case_index % configuration.shards != configuration.shard)
	{
		return true;
	}
	if constexpr (
		!::fast_io::details::scan_decfloat_supported_floating_point<
			floating_type>)
	{
		return true;
	}
	else
	{
		++statistics.configurations;
		splitmix64 random{
			configuration.seed ^
			(static_cast<::std::uint_least64_t>(case_index) *
			 UINT64_C(0xa0761d6478bd642f))};
		constexpr auto flags{floating_fuzz_flags<rounding>};
		using manipulator_type =
			::fast_io::manipulators::scalar_manip_t<
				flags, floating_type &>;
		constexpr ::std::size_t maximum_padding{64u};
		for (::std::size_t step{}; step != configuration.iterations;
			 ++step)
		{
			/*
			The same abstract-iteration contract used by the integer domain
			places 128/768/832/12000 and their adjacent lengths directly in the
			corpus without requiring equally large fuzzer inputs.
			*/
			auto const iteration{configuration.first_iteration + step};
			auto const semantic_size{floating_length(iteration, random)};
			auto const padding{static_cast<::std::size_t>(
				random.next() % (maximum_padding + 1u))};
			::std::vector<char_type> digit_padding(
				semantic_size + maximum_padding);
			::std::vector<char_type> nondigit_padding(
				semantic_size + maximum_padding);
			make_floating_input(
				digit_padding, semantic_size, random);
			for (::std::size_t index{}; index != semantic_size; ++index)
			{
				nondigit_padding[index] = digit_padding[index];
			}
			for (::std::size_t index{semantic_size};
				 index != semantic_size + maximum_padding; ++index)
			{
				digit_padding[index] =
					ascii_runtime<char_type>(u8'9');
				nondigit_padding[index] =
					ascii_runtime<char_type>(u8'/');
			}

			floating_type ordinary_value{};
			floating_type digit_padding_value{};
			floating_type nondigit_padding_value{};
			auto const *const ordinary_begin{digit_padding.data()};
			auto const *const ordinary_end{
				ordinary_begin + semantic_size};
			auto const ordinary_result{scan_contiguous_define(
				::fast_io::io_reserve_type<
					char_type, manipulator_type>,
				ordinary_begin, ordinary_end,
				manipulator_type{ordinary_value})};
			auto const digit_padding_result{scan_contiguous_padding_define(
				::fast_io::io_reserve_type<
					char_type, manipulator_type>,
				digit_padding.data(),
				digit_padding.data() + semantic_size, padding,
				manipulator_type{digit_padding_value})};
			auto const nondigit_padding_result{scan_contiguous_padding_define(
				::fast_io::io_reserve_type<
					char_type, manipulator_type>,
				nondigit_padding.data(),
				nondigit_padding.data() + semantic_size, padding,
				manipulator_type{nondigit_padding_value})};

			auto const ordinary_offset{bounded_offset(
				ordinary_begin, semantic_size, ordinary_result.iter)};
			auto const digit_padding_offset{bounded_offset(
				digit_padding.data(), semantic_size,
				digit_padding_result.iter)};
			auto const nondigit_padding_offset{bounded_offset(
				nondigit_padding.data(), semantic_size,
				nondigit_padding_result.iter)};
			if (ordinary_result.code != digit_padding_result.code ||
				ordinary_result.code != nondigit_padding_result.code ||
				ordinary_offset != digit_padding_offset ||
				ordinary_offset != nondigit_padding_offset ||
				!same_floating_bits(
					ordinary_value, digit_padding_value) ||
				!same_floating_bits(
					ordinary_value, nondigit_padding_value))
			{
				::std::fprintf(
					stderr,
					"floating mismatch case=%zu char=%zu type=%zu "
					"rounding=%zu iteration=%zu length=%zu "
					"padding=%zu codes=%u/%u/%u "
					"offsets=%zu/%zu/%zu\n",
					case_index, char_index, floating_index,
					rounding_index, iteration, semantic_size, padding,
					static_cast<unsigned>(ordinary_result.code),
					static_cast<unsigned>(digit_padding_result.code),
					static_cast<unsigned>(
						nondigit_padding_result.code),
					ordinary_offset, digit_padding_offset,
					nondigit_padding_offset);
				return false;
			}
			++statistics.inputs;
			statistics.semantic_code_units += semantic_size;
		}
		return true;
	}
}

template <::std::size_t char_index, ::std::size_t integer_index,
		  ::std::integral char_type,
		  ::fast_io::details::my_integral integer_type,
		  ::std::size_t... indices>
[[nodiscard]] bool fuzz_integer_bases(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics,
	::std::index_sequence<indices...>) noexcept
{
	return (fuzz_integer_configuration<
				char_index, integer_index, indices + 2u, char_type,
				integer_type>(configuration, statistics) &&
			...);
}

template <::std::size_t char_index, ::std::integral char_type>
[[nodiscard]] bool fuzz_all_integer_types(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	auto const bases{::std::make_index_sequence<35u>{}};
	return fuzz_integer_bases<char_index, 0u, char_type, ::std::int8_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 1u, char_type, ::std::uint8_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 2u, char_type, ::std::int16_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 3u, char_type, ::std::uint16_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 4u, char_type, ::std::int32_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 5u, char_type, ::std::uint32_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 6u, char_type, ::std::int64_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 7u, char_type, ::std::uint64_t>(
			   configuration, statistics, bases)
#if defined(__SIZEOF_INT128__)
		   &&
		   fuzz_integer_bases<char_index, 8u, char_type, __int128_t>(
			   configuration, statistics, bases) &&
		   fuzz_integer_bases<char_index, 9u, char_type, __uint128_t>(
			   configuration, statistics, bases)
#endif
		;
}

template <::std::size_t char_index, ::std::size_t floating_index,
		  ::std::integral char_type, typename floating_type>
[[nodiscard]] bool fuzz_all_roundings(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return fuzz_floating_configuration<
			   char_index, floating_index, 0u, char_type, floating_type,
			   rounding::nearest_to_even>(configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 1u, char_type, floating_type,
			   rounding::nearest_to_odd>(configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 2u, char_type, floating_type,
			   rounding::nearest_toward_plus_infinity>(
			   configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 3u, char_type, floating_type,
			   rounding::nearest_toward_minus_infinity>(
			   configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 4u, char_type, floating_type,
			   rounding::nearest_toward_zero>(configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 5u, char_type, floating_type,
			   rounding::nearest_away_from_zero>(
			   configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 6u, char_type, floating_type,
			   rounding::toward_plus_infinity>(
			   configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 7u, char_type, floating_type,
			   rounding::toward_minus_infinity>(
			   configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 8u, char_type, floating_type,
			   rounding::toward_zero>(configuration, statistics) &&
		   fuzz_floating_configuration<
			   char_index, floating_index, 9u, char_type, floating_type,
			   rounding::away_from_zero>(configuration, statistics);
}

template <::std::size_t char_index, ::std::integral char_type>
[[nodiscard]] bool fuzz_all_floating_types(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	bool result{true};
#if (defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__)) ||             \
	defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	result = result &&
			 fuzz_all_roundings<char_index, 0u, char_type, __bf16>(
				 configuration, statistics);
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	result = result &&
			 fuzz_all_roundings<char_index, 1u, char_type, _Float16>(
				 configuration, statistics);
#endif
	result = result &&
			 fuzz_all_roundings<char_index, 2u, char_type, float>(
				 configuration, statistics) &&
			 fuzz_all_roundings<char_index, 3u, char_type, double>(
				 configuration, statistics) &&
			 fuzz_all_roundings<char_index, 4u, char_type, long double>(
				 configuration, statistics);
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
	result = result &&
			 fuzz_all_roundings<char_index, 5u, char_type, __float128>(
				 configuration, statistics);
#endif
	return result;
}

template <::std::size_t char_index, ::std::integral char_type>
[[nodiscard]] bool fuzz_character_domain(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	return fuzz_all_integer_types<char_index, char_type>(
			   configuration, statistics) &&
		   fuzz_all_floating_types<char_index, char_type>(
			   configuration, statistics);
}

using fuzz_case_function = bool (*)(
	fuzz_configuration const &, fuzz_statistics &) noexcept;

template <::std::size_t char_index, ::std::size_t integer_index,
		  ::std::integral char_type,
		  ::fast_io::details::my_integral integer_type,
		  ::std::size_t... indices>
[[nodiscard]] consteval auto make_integer_dispatch_table(
	::std::index_sequence<indices...>) noexcept
{
	/*
	Each table slot has one base in [2,36].  The array index is base-2, so the
	dispatch proof is 0<=base_index<35 => 2<=base_index+2<=36.  Taking function
	addresses instantiates every type/base case without executing the other 34
	cases for an input.
	*/
	return ::std::array<fuzz_case_function, sizeof...(indices)>{
		&fuzz_integer_configuration<
			char_index, integer_index, indices + 2u, char_type,
			integer_type>...};
}

template <::std::size_t char_index, ::std::size_t integer_index,
		  ::std::integral char_type,
		  ::fast_io::details::my_integral integer_type>
inline constexpr auto integer_dispatch_table{
	make_integer_dispatch_table<
		char_index, integer_index, char_type, integer_type>(
		::std::make_index_sequence<35u>{})};

template <::std::size_t char_index, ::std::integral char_type>
[[nodiscard]] inline bool dispatch_integer_type(
	::std::size_t integer_index, ::std::size_t base_index,
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	/*
	The caller proves integer_index<10 and base_index<35 by quotient/remainder
	over the 10*35 integer subdomain.  Each switch arm selects exactly one
	concrete integer type; the table then performs one indirect call.  Platforms
	without 128-bit integers deliberately treat their two uninstantiable slots as
	no-ops rather than aliasing them to another type.
	*/
	switch (integer_index)
	{
	case 0u:
		return integer_dispatch_table<
			char_index, 0u, char_type, ::std::int8_t>[base_index](
			configuration, statistics);
	case 1u:
		return integer_dispatch_table<
			char_index, 1u, char_type, ::std::uint8_t>[base_index](
			configuration, statistics);
	case 2u:
		return integer_dispatch_table<
			char_index, 2u, char_type, ::std::int16_t>[base_index](
			configuration, statistics);
	case 3u:
		return integer_dispatch_table<
			char_index, 3u, char_type, ::std::uint16_t>[base_index](
			configuration, statistics);
	case 4u:
		return integer_dispatch_table<
			char_index, 4u, char_type, ::std::int32_t>[base_index](
			configuration, statistics);
	case 5u:
		return integer_dispatch_table<
			char_index, 5u, char_type, ::std::uint32_t>[base_index](
			configuration, statistics);
	case 6u:
		return integer_dispatch_table<
			char_index, 6u, char_type, ::std::int64_t>[base_index](
			configuration, statistics);
	case 7u:
		return integer_dispatch_table<
			char_index, 7u, char_type, ::std::uint64_t>[base_index](
			configuration, statistics);
#if defined(__SIZEOF_INT128__)
	case 8u:
		return integer_dispatch_table<
			char_index, 8u, char_type, __int128_t>[base_index](
			configuration, statistics);
	case 9u:
		return integer_dispatch_table<
			char_index, 9u, char_type, __uint128_t>[base_index](
			configuration, statistics);
#endif
	default:
		return true;
	}
}

template <::std::size_t char_index, ::std::size_t floating_index,
		  ::std::integral char_type, typename floating_type>
[[nodiscard]] consteval auto make_floating_dispatch_table() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	/*
	The ten entries are in the same canonical order as the case-number product.
	Consequently rounding_index in [0,10) maps bijectively to the requested
	rounding policy and performs exactly one scanner comparison.
	*/
	return ::std::array<fuzz_case_function, 10u>{
		&fuzz_floating_configuration<
			char_index, floating_index, 0u, char_type, floating_type,
			rounding::nearest_to_even>,
		&fuzz_floating_configuration<
			char_index, floating_index, 1u, char_type, floating_type,
			rounding::nearest_to_odd>,
		&fuzz_floating_configuration<
			char_index, floating_index, 2u, char_type, floating_type,
			rounding::nearest_toward_plus_infinity>,
		&fuzz_floating_configuration<
			char_index, floating_index, 3u, char_type, floating_type,
			rounding::nearest_toward_minus_infinity>,
		&fuzz_floating_configuration<
			char_index, floating_index, 4u, char_type, floating_type,
			rounding::nearest_toward_zero>,
		&fuzz_floating_configuration<
			char_index, floating_index, 5u, char_type, floating_type,
			rounding::nearest_away_from_zero>,
		&fuzz_floating_configuration<
			char_index, floating_index, 6u, char_type, floating_type,
			rounding::toward_plus_infinity>,
		&fuzz_floating_configuration<
			char_index, floating_index, 7u, char_type, floating_type,
			rounding::toward_minus_infinity>,
		&fuzz_floating_configuration<
			char_index, floating_index, 8u, char_type, floating_type,
			rounding::toward_zero>,
		&fuzz_floating_configuration<
			char_index, floating_index, 9u, char_type, floating_type,
			rounding::away_from_zero>};
}

template <::std::size_t char_index, ::std::size_t floating_index,
		  ::std::integral char_type, typename floating_type>
inline constexpr auto floating_dispatch_table{
	make_floating_dispatch_table<
		char_index, floating_index, char_type, floating_type>()};

template <::std::size_t char_index, ::std::integral char_type>
[[nodiscard]] inline bool dispatch_floating_type(
	::std::size_t floating_index, ::std::size_t rounding_index,
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	/*
	The caller's quotient/remainder proves floating_index<6 and
	rounding_index<10.  Feature guards exactly match the production type
	capabilities; an unavailable representation is not substituted with a
	different format.
	*/
	switch (floating_index)
	{
	case 0u:
#if (defined(__GNUC__) && !defined(__clang__) && \
	 defined(__BFLT16_MANT_DIG__)) ||             \
	defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
		return floating_dispatch_table<
			char_index, 0u, char_type, __bf16>[rounding_index](
			configuration, statistics);
#else
		return true;
#endif
	case 1u:
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
		return floating_dispatch_table<
			char_index, 1u, char_type, _Float16>[rounding_index](
			configuration, statistics);
#else
		return true;
#endif
	case 2u:
		return floating_dispatch_table<
			char_index, 2u, char_type, float>[rounding_index](
			configuration, statistics);
	case 3u:
		return floating_dispatch_table<
			char_index, 3u, char_type, double>[rounding_index](
			configuration, statistics);
	case 4u:
		return floating_dispatch_table<
			char_index, 4u, char_type, long double>[rounding_index](
			configuration, statistics);
	case 5u:
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	(!defined(__FLT128_MANT_DIG__) || __FLT128_MANT_DIG__ == 113)
		return floating_dispatch_table<
			char_index, 5u, char_type, __float128>[rounding_index](
			configuration, statistics);
#else
		return true;
#endif
	default:
		return true;
	}
}

[[nodiscard]] inline bool dispatch_selected_configuration(
	fuzz_configuration const &configuration,
	fuzz_statistics &statistics) noexcept
{
	constexpr ::std::size_t integer_per_character{10u * 35u};
	constexpr ::std::size_t integer_configuration_count{
		5u * integer_per_character};
	auto const selected{configuration.shard};
	if (selected < integer_configuration_count)
	{
		auto const char_index{selected / integer_per_character};
		auto const within_character{selected % integer_per_character};
		auto const integer_index{within_character / 35u};
		auto const base_index{within_character % 35u};
		switch (char_index)
		{
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 0 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
		case 0u:
			return dispatch_integer_type<0u, char>(
				integer_index, base_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 1 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
		case 1u:
			return dispatch_integer_type<1u, wchar_t>(
				integer_index, base_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 2 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
		case 2u:
			return dispatch_integer_type<2u, char8_t>(
				integer_index, base_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 3 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
		case 3u:
			return dispatch_integer_type<3u, char16_t>(
				integer_index, base_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 4 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
		case 4u:
			return dispatch_integer_type<4u, char32_t>(
				integer_index, base_index, configuration, statistics);
#endif
		default:
			return true;
		}
	}
	constexpr ::std::size_t floating_per_character{6u * 10u};
	auto const floating_selected{selected - integer_configuration_count};
	auto const char_index{floating_selected / floating_per_character};
	auto const within_character{
		floating_selected % floating_per_character};
	auto const floating_index{within_character / 10u};
	auto const rounding_index{within_character % 10u};
	/*
	selected<2050 and selected>=1750 prove char_index<5 here.  The default arm
	is therefore exactly the char32_t member of the five-character product.
	*/
	switch (char_index)
	{
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 0 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
	case 0u:
		return dispatch_floating_type<0u, char>(
			floating_index, rounding_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 1 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
	case 1u:
		return dispatch_floating_type<1u, wchar_t>(
			floating_index, rounding_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 2 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
	case 2u:
		return dispatch_floating_type<2u, char8_t>(
			floating_index, rounding_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 3 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
	case 3u:
		return dispatch_floating_type<3u, char16_t>(
			floating_index, rounding_index, configuration, statistics);
#endif
#if FAST_IO_PADDING_FUZZ_CHAR_INDEX == 4 || \
	FAST_IO_PADDING_FUZZ_CHAR_INDEX == 5
	case 4u:
		return dispatch_floating_type<4u, char32_t>(
			floating_index, rounding_index, configuration, statistics);
#endif
	default:
		return true;
	}
}

[[nodiscard]] inline ::std::uint_least64_t
load_little_endian_prefix(unsigned char const *data,
						  ::std::size_t size,
						  ::std::size_t offset,
						  ::std::size_t width) noexcept
{
	/*
	Only bytes proved present by index<size are read.  Missing suffix bytes are
	defined as zero, making every input length, including the empty input, a
	total and deterministic selector.
	*/
	::std::uint_least64_t value{};
	for (::std::size_t index{}; index != width; ++index)
	{
		auto const source_index{offset + index};
		if (source_index >= size)
		{
			break;
		}
		value |= static_cast<::std::uint_least64_t>(data[source_index])
				 << (index * 8u);
	}
	return value;
}

[[nodiscard]] inline ::std::uint_least64_t
hash_fuzzer_input(unsigned char const *data, ::std::size_t size) noexcept
{
	/*
	Every byte affects the generated semantic spelling even when it lies beyond
	the fixed selector prefix.  The recurrence is an unsigned modulo-2^64
	computation, hence has no undefined signed overflow.
	*/
	::std::uint_least64_t hash{UINT64_C(0xcbf29ce484222325)};
	for (::std::size_t index{}; index != size; ++index)
	{
		hash ^= data[index];
		hash *= UINT64_C(0x100000001b3);
	}
	return hash;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(unsigned char const *data,
									 ::std::size_t size)
{
	constexpr ::std::size_t integer_configuration_count{5u * 10u * 35u};
	constexpr ::std::size_t floating_configuration_count{5u * 6u * 10u};
	constexpr ::std::size_t configuration_count{
		integer_configuration_count + floating_configuration_count};
	/*
	Bytes [0,2) select exactly one compile-time scanner configuration.  The
	modulus is over the complete character/type/base/rounding product, so every
	instantiated configuration has a preimage.  Bytes [2,10) select the abstract
	length iteration; all bytes jointly seed the spelling and padding generator.
	One callback therefore compares one ordinary execution with two padded
	executions while preserving libFuzzer's coverage-guided mutation model.
	*/
	static ::std::size_t const fixed_configuration{[]() noexcept {
		auto const *const text{
			::std::getenv("FAST_IO_PADDING_FIXED_CASE")};
		if (text == nullptr || *text == '\0')
		{
			return configuration_count;
		}
		char *end{};
		auto const value{
			::std::strtoull(text, __builtin_addressof(end), 10)};
		if (end == text || *end != '\0' ||
			value >= configuration_count)
		{
			::std::abort();
		}
		return static_cast<::std::size_t>(value);
	}()};
	auto const selected_configuration{
		fixed_configuration == configuration_count
			? static_cast<::std::size_t>(
				  load_little_endian_prefix(data, size, 0u, 2u) %
				  configuration_count)
			: fixed_configuration};
	auto const first_iteration{static_cast<::std::size_t>(
		load_little_endian_prefix(data, size, 2u, 8u))};
	fuzz_configuration configuration{
		selected_configuration, configuration_count, 1u, first_iteration,
		hash_fuzzer_input(data, size)};
	fuzz_statistics statistics;
	bool const result{
		dispatch_selected_configuration(configuration, statistics)};
	if (!result)
	{
		/*
		A semantic mismatch must be a process failure, not a recoverable return:
		libFuzzer then materializes the exact selector/seed bytes as an artifact.
		The runner suppresses engine output and replays only that artifact.
		*/
		::std::abort();
	}
	return 0;
}
