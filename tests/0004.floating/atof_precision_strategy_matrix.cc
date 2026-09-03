#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

#ifndef FAST_IO_ATOF_PRECISION_TEST_CHAR_TYPE
#define FAST_IO_ATOF_PRECISION_TEST_CHAR_TYPE char
#endif

namespace
{

using test_char_type = FAST_IO_ATOF_PRECISION_TEST_CHAR_TYPE;

[[nodiscard]] inline constexpr bool execution_to_u8(
	char input, char8_t &output) noexcept
{
	if ('0' <= input && input <= '9')
	{
		output = static_cast<char8_t>(u8'0' + (input - '0'));
		return true;
	}
	switch (input)
	{
	case '-':
		output = u8'-';
		return true;
	case '.':
		output = u8'.';
		return true;
	case 'e':
		output = u8'e';
		return true;
	default:
		return false;
	}
}

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool comma>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	flags.precision = precision_mode;
	flags.comma = comma;
	return flags;
}();

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_fields(
	floating_type left, floating_type right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa && lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

template <typename floating_type>
struct observation
{
	floating_type value{};
	::std::size_t consumed{};
	::fast_io::parse_code code{};
};

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_observation(
	observation<floating_type> const &left,
	observation<floating_type> const &right) noexcept
{
	return left.code == right.code && left.consumed == right.consumed &&
		   same_fields(left.value, right.value);
}

template <auto flags, typename floating_type>
[[nodiscard]] inline observation<floating_type> scan_direct(
	test_char_type const *first, test_char_type const *last,
	::std::size_t precision) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type &>;
	floating_type value{static_cast<floating_type>(-42.25)};
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
		last, manipulator_type{value, precision})};
	return {value, static_cast<::std::size_t>(result.iter - first), result.code};
}

template <auto flags, typename floating_type>
[[nodiscard]] inline observation<floating_type> scan_padded(
	test_char_type const *first, test_char_type const *last,
	::std::size_t precision, ::std::size_t padding) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type &>;
	floating_type value{static_cast<floating_type>(-42.25)};
	if constexpr (::fast_io::terminal_contiguous_padding_scannable<
					  test_char_type, manipulator_type>)
	{
		auto const result{::fast_io::scan_contiguous_padding_define(
			::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
			last, padding, manipulator_type{value, precision})};
		return {value, static_cast<::std::size_t>(result.iter - first),
				result.code};
	}
	else
	{
		(void)padding;
		return scan_direct<flags, floating_type>(first, last, precision);
	}
}

template <auto flags, typename floating_type>
[[nodiscard]] observation<floating_type> scan_context_chunks(
	test_char_type const *first, ::std::size_t size,
	::std::size_t precision, ::std::size_t chunk_size) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type &>;
	using state_type = typename ::std::remove_cvref_t<decltype(::fast_io::scan_context_type(
		::fast_io::io_reserve_type<test_char_type, manipulator_type>))>::type;
	state_type state{};
	floating_type value{static_cast<floating_type>(-42.25)};
	manipulator_type manipulator{value, precision};
	auto const tag{::fast_io::io_reserve_type<test_char_type, manipulator_type>};
	auto offset{::std::size_t{}};
	chunk_size = chunk_size == 0u ? 1u : chunk_size;
	while (offset != size)
	{
		auto const remaining{size - offset};
		auto const count{remaining < chunk_size ? remaining : chunk_size};
		auto const *const chunk_begin{first + offset};
		auto const *const chunk_end{chunk_begin + count};
		auto const result{::fast_io::scan_context_define(
			tag, state, chunk_begin, chunk_end, manipulator)};
		if (result.iter < chunk_begin || chunk_end < result.iter)
		{
			return {value, offset, ::fast_io::parse_code::invalid};
		}
		offset += static_cast<::std::size_t>(result.iter - chunk_begin);
		if (result.code != ::fast_io::parse_code::partial)
		{
			return {value, offset, result.code};
		}
		if (result.iter != chunk_end)
		{
			return {value, offset, ::fast_io::parse_code::invalid};
		}
	}
	auto const code{
		::fast_io::scan_context_eof_define(tag, state, manipulator)};
	return {value, offset, code};
}

inline constexpr ::std::string_view general_tokens[]{
	"0.1",
	"-0.1",
	"1.234567890123456789",
	"9.9999995",
	"0.000000000000000000000000000123456789",
	"123456789012345678901234567890.125",
	"1.1754943508222875e-38",
	"3.4028236692093846e38",
	"4.9406564584124654e-324",
	"1e5000",
	"1e-5000"};

inline constexpr ::std::string_view fixed_tokens[]{
	"0.1",
	"-0.1",
	"1.234567890123456789",
	"9.9999995",
	"0.000000000000000000000000000123456789",
	"123456789012345678901234567890.125",
	"340282366920938463463374607431768211456",
	"0.0000000000000000000000000000000000000000000000000000001"};

inline constexpr ::std::string_view scientific_tokens[]{
	"1e-1",
	"-1e-1",
	"1.234567890123456789e0",
	"9.9999995e0",
	"1.23456789e-31",
	"1.23456789012345678901234567890125e29",
	"1.1754943508222875e-38",
	"3.4028236692093846e38",
	"4.9406564584124654e-324",
	"1e5000",
	"1e-5000"};

template <auto flags, typename floating_type>
[[nodiscard]] bool check_token(
	::std::string_view token, ::std::size_t token_index,
	::std::size_t precision) noexcept
{
	constexpr ::std::size_t capacity{256u};
	constexpr ::std::size_t maximum_padding{64u};
	if (capacity <= token.size() + 1u + maximum_padding)
	{
		return false;
	}
	test_char_type storage[capacity]{};
	for (::std::size_t index{}; index != token.size(); ++index)
	{
		char8_t code_unit{};
		if (!execution_to_u8(token[index], code_unit))
		{
			return false;
		}
		if constexpr (flags.comma)
		{
			if (code_unit == u8'.')
			{
				code_unit = u8',';
			}
		}
		storage[index] = ::fast_io::char_literal<test_char_type>(code_unit);
	}
	storage[token.size()] =
		::fast_io::char_literal_v<u8'|', test_char_type>;
	auto const delimiter_size{token.size() + 1u};
	auto const ordinary{scan_direct<flags, floating_type>(
		storage, storage + delimiter_size, precision)};
	auto const context{scan_context_chunks<flags, floating_type>(
		storage, delimiter_size, precision, delimiter_size)};
	if (!same_observation(ordinary, context))
	{
		::std::fprintf(
			stderr,
			"precision atof contiguous/context mismatch: sizeof(flt)=%zu "
			"sizeof(char)=%zu format=%u rounding=%u mode=%u token=%zu "
			"precision=%zu ordinary=%u/%zu context=%u/%zu text=%.*s\n",
			sizeof(floating_type), sizeof(test_char_type),
			static_cast<unsigned>(flags.floating),
			static_cast<unsigned>(flags.rounding),
			static_cast<unsigned>(flags.precision), token_index, precision,
			static_cast<unsigned>(ordinary.code), ordinary.consumed,
			static_cast<unsigned>(context.code), context.consumed,
			static_cast<int>(token.size()), token.data());
		return false;
	}
	auto const eof_context{scan_context_chunks<flags, floating_type>(
		storage, token.size(), precision, token.size())};
	auto const maximum_chunk{
		delimiter_size < 20u ? delimiter_size : ::std::size_t{20u}};
	for (::std::size_t chunk{1u}; chunk <= maximum_chunk; ++chunk)
	{
		auto const split{scan_context_chunks<flags, floating_type>(
			storage, delimiter_size, precision, chunk)};
		auto const split_eof{scan_context_chunks<flags, floating_type>(
			storage, token.size(), precision, chunk)};
		if (!same_observation(context, split) ||
			!same_observation(eof_context, split_eof))
		{
			::std::fprintf(
				stderr,
				"precision atof fragmentation mismatch: sizeof(flt)=%zu "
				"sizeof(char)=%zu format=%u rounding=%u mode=%u token=%zu "
				"precision=%zu chunk=%zu text=%.*s\n",
				sizeof(floating_type), sizeof(test_char_type),
				static_cast<unsigned>(flags.floating),
				static_cast<unsigned>(flags.rounding),
				static_cast<unsigned>(flags.precision), token_index, precision,
				chunk, static_cast<int>(token.size()), token.data());
			return false;
		}
	}
	constexpr ::std::size_t paddings[]{1u, 15u, 16u, 31u, 32u, 64u};
	for (auto const padding : paddings)
	{
		for (unsigned pattern{}; pattern != 2u; ++pattern)
		{
			for (::std::size_t index{}; index != maximum_padding; ++index)
			{
				storage[delimiter_size + index] = pattern == 0u
													  ? ::fast_io::char_literal_v<u8'9', test_char_type>
													  : ::fast_io::char_literal_v<u8'/', test_char_type>;
			}
			auto const padded{scan_padded<flags, floating_type>(
				storage, storage + delimiter_size, precision, padding)};
			if (!same_observation(ordinary, padded))
			{
				::std::fprintf(
					stderr,
					"precision atof padding mismatch: sizeof(flt)=%zu "
					"sizeof(char)=%zu format=%u rounding=%u mode=%u token=%zu "
					"precision=%zu padding=%zu pattern=%u text=%.*s\n",
					sizeof(floating_type), sizeof(test_char_type),
					static_cast<unsigned>(flags.floating),
					static_cast<unsigned>(flags.rounding),
					static_cast<unsigned>(flags.precision), token_index,
					precision, padding, pattern,
					static_cast<int>(token.size()), token.data());
				return false;
			}
		}
	}
	return true;
}

template <auto flags, typename floating_type, ::std::size_t extent>
[[nodiscard]] bool check_tokens(
	::std::string_view const (&tokens)[extent]) noexcept
{
	constexpr ::std::size_t precisions[]{
		0u, 1u, 2u, 6u,
		static_cast<::std::size_t>(
			::std::numeric_limits<floating_type>::max_digits10),
		31u};
	for (::std::size_t token_index{}; token_index != extent; ++token_index)
	{
		for (auto const precision : precisions)
		{
			if (!check_token<flags, floating_type>(
					tokens[token_index], token_index, precision))
			{
				return false;
			}
		}
	}
	return true;
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool comma>
[[nodiscard]] bool check_configuration() noexcept
{
	constexpr auto flags{test_flags<format, rounding, precision_mode, comma>};
	if constexpr (format == ::fast_io::manipulators::floating_format::fixed)
	{
		return check_tokens<flags, floating_type>(fixed_tokens);
	}
	else if constexpr (
		format == ::fast_io::manipulators::floating_format::scientific)
	{
		return check_tokens<flags, floating_type>(scientific_tokens);
	}
	else
	{
		return check_tokens<flags, floating_type>(general_tokens);
	}
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode>
[[nodiscard]] bool check_radix_characters() noexcept
{
	return check_configuration<floating_type, format, rounding, precision_mode,
							   false>() &&
		   check_configuration<floating_type, format, rounding, precision_mode,
							   true>();
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] bool check_precision_modes() noexcept
{
	using precision = ::fast_io::manipulators::floating_precision;
	return check_radix_characters<floating_type, format, rounding,
								  precision::significant>() &&
		   check_radix_characters<floating_type, format, rounding,
								  precision::fractional>() &&
		   check_radix_characters<
			   floating_type, format, rounding,
			   precision::significant_preserve_trailing_zero>() &&
		   check_radix_characters<
			   floating_type, format, rounding,
			   precision::fractional_preserve_trailing_zero>();
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format>
[[nodiscard]] bool check_roundings() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return check_precision_modes<floating_type, format,
								 rounding::nearest_to_even>() &&
		   check_precision_modes<floating_type, format,
								 rounding::nearest_to_odd>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_plus_infinity>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_minus_infinity>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_zero>();
}

template <typename floating_type>
[[nodiscard]] bool check_type() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	return check_roundings<floating_type, format::general>() &&
		   check_roundings<floating_type, format::decimal>() &&
		   check_roundings<floating_type, format::fixed>() &&
		   check_roundings<floating_type, format::scientific>();
}

} // namespace

int main()
{
	if (!check_type<float>() || !check_type<double>())
	{
		return 1;
	}
	::std::fputs(
		"precision atof strategy matrix passed: 320 static configurations, "
		"6 runtime precisions, decimal formats, chunk boundaries, EOF and "
		"12 padding variants\n",
		stdout);
}
