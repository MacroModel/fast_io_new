#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

#ifndef FAST_IO_ATOF_FRAGMENT_TEST_CHAR_TYPE
#define FAST_IO_ATOF_FRAGMENT_TEST_CHAR_TYPE char
#endif

namespace
{

using test_char_type = FAST_IO_ATOF_FRAGMENT_TEST_CHAR_TYPE;

struct token_case
{
	char const *text;
	bool complete;
	bool requires_plus;
	bool requires_skip;
};

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
	case ' ':
		output = u8' ';
		break;
	case '+':
		output = u8'+';
		break;
	case '-':
		output = u8'-';
		break;
	case '.':
		output = u8'.';
		break;
	case '(':
		output = u8'(';
		break;
	case ')':
		output = u8')';
		break;
	case 'E':
		output = u8'E';
		break;
	case 'A':
		output = u8'A';
		break;
	case 'F':
		output = u8'F';
		break;
	case 'I':
		output = u8'I';
		break;
	case 'N':
		output = u8'N';
		break;
	case 'P':
		output = u8'P';
		break;
	case 'T':
		output = u8'T';
		break;
	case 'X':
		output = u8'X';
		break;
	case 'Y':
		output = u8'Y';
		break;
	case 'a':
		output = u8'a';
		break;
	case 'd':
		output = u8'd';
		break;
	case 'e':
		output = u8'e';
		break;
	case 'f':
		output = u8'f';
		break;
	case 'i':
		output = u8'i';
		break;
	case 'n':
		output = u8'n';
		break;
	case 'p':
		output = u8'p';
		break;
	case 't':
		output = u8't';
		break;
	case 'x':
		output = u8'x';
		break;
	case 'y':
		output = u8'y';
		break;
	default:
		return false;
	}
	return true;
}

inline constexpr token_case general_tokens[]{
	{"0", true, false, false},
	{"-0", true, false, false},
	{"1", true, false, false},
	{"-1", true, false, false},
	{"+1", true, true, false},
	{".5", true, false, false},
	{"0.", true, false, false},
	{"1.25", true, false, false},
	{"000001.2500", true, false, false},
	{"1e0", true, false, false},
	{"1E+10", true, false, false},
	{"-1e-10", true, false, false},
	{"  1.25e2", true, false, true},
	{"123456789012345678901234567890.125", true, false, false},
	{"1.1754943508222875e-38", true, false, false},
	{"3.4028236692093846e38", true, false, false},
	{"4.9406564584124654e-324", true, false, false},
	{"1e5000", true, false, false},
	{"-1e5000", true, false, false},
	{"1e-5000", true, false, false},
	{"inf", true, false, false},
	{"-inf", true, false, false},
	{"+inf", true, true, false},
	{"infinity", true, false, false},
	{"INF", true, false, false},
	{"INFINITY", true, false, false},
	{"nan", true, false, false},
	{"NAN", true, false, false},
	{"-nan", true, false, false},
	{"", false, false, false},
	{" ", false, false, false},
	{"+", false, true, false},
	{"-", false, false, false},
	{".", false, false, false},
	{"e10", false, false, false},
	{"1e", false, false, false},
	{"1e+", false, false, false},
	{"1e-", false, false, false},
	{"--1", false, false, false},
	{"++1", false, true, false},
	{"infi", true, false, false},
	{"infx", true, false, false},
	{"infinityx", true, false, false},
	{"INFi", true, false, false},
	{"nanx", true, false, false},
	{"NANx", true, false, false},
	{"nan(ind)", true, false, false},
	{"nan(ind)x", true, false, false},
	{"nan(", false, false, false}};

inline constexpr token_case fixed_tokens[]{
	{"0", true, false, false},
	{"-0", true, false, false},
	{"1", true, false, false},
	{"-1", true, false, false},
	{"+1", true, true, false},
	{".5", true, false, false},
	{"0.", true, false, false},
	{"1.25", true, false, false},
	{"000001.2500", true, false, false},
	{"  1.25", true, false, true},
	{"123456789012345678901234567890.125", true, false, false},
	{"340282366920938463463374607431768211456", true, false, false},
	{"0.000000000000000000000000000000000000000000000001", true, false, false},
	{"inf", true, false, false},
	{"-inf", true, false, false},
	{"+inf", true, true, false},
	{"infinity", true, false, false},
	{"INF", true, false, false},
	{"INFINITY", true, false, false},
	{"nan", true, false, false},
	{"NAN", true, false, false},
	{"", false, false, false},
	{" ", false, false, false},
	{"+", false, true, false},
	{"-", false, false, false},
	{".", false, false, false},
	{"--1", false, false, false},
	{"++1", false, true, false},
	{"infi", true, false, false},
	{"infx", true, false, false},
	{"infinityx", true, false, false},
	{"INFi", true, false, false},
	{"nanx", true, false, false},
	{"NANx", true, false, false},
	{"nan(ind)", true, false, false},
	{"nan(ind)x", true, false, false},
	{"nan(", false, false, false}};

inline constexpr token_case scientific_tokens[]{
	{"0e0", true, false, false},
	{"-0E+0", true, false, false},
	{"1e0", true, false, false},
	{"-1e0", true, false, false},
	{"+1e0", true, true, false},
	{".5e-3", true, false, false},
	{"0.e0", true, false, false},
	{"1.25E+10", true, false, false},
	{"000001.2500e-2", true, false, false},
	{"  1.25e2", true, false, true},
	{"123456789012345678901234567890.125e-20", true, false, false},
	{"1.1754943508222875e-38", true, false, false},
	{"3.4028236692093846e38", true, false, false},
	{"4.9406564584124654e-324", true, false, false},
	{"1e5000", true, false, false},
	{"-1e5000", true, false, false},
	{"1e-5000", true, false, false},
	{"inf", true, false, false},
	{"-inf", true, false, false},
	{"+inf", true, true, false},
	{"infinity", true, false, false},
	{"INF", true, false, false},
	{"INFINITY", true, false, false},
	{"nan", true, false, false},
	{"NAN", true, false, false},
	{"", false, false, false},
	{" ", false, false, false},
	{"+", false, true, false},
	{"-", false, false, false},
	{".", false, false, false},
	{"1", false, false, false},
	{"1.", false, false, false},
	{"e10", false, false, false},
	{"1e", false, false, false},
	{"1e+", false, false, false},
	{"1e-", false, false, false},
	{"--1e0", false, false, false},
	{"++1e0", false, true, false},
	{"infi", true, false, false},
	{"infx", true, false, false},
	{"infinityx", true, false, false},
	{"INFi", true, false, false},
	{"nanx", true, false, false},
	{"NANx", true, false, false},
	{"nan(ind)", true, false, false},
	{"nan(ind)x", true, false, false},
	{"nan(", false, false, false}};

inline constexpr token_case hex_tokens[]{
	{"0p0", true, false, false},
	{"-0p0", true, false, false},
	{"1p0", true, false, false},
	{"-1.8p+2", true, false, false},
	{"+1p0", true, true, false},
	{".8p-1", true, false, false},
	{"0.p0", true, false, false},
	{"1.fffffffffffffp+1023", true, false, false},
	{"1p+20000", true, false, false},
	{"1p-20000", true, false, false},
	{"  1.8p+1", true, false, true},
	{"inf", true, false, false},
	{"-inf", true, false, false},
	{"+inf", true, true, false},
	{"infinity", true, false, false},
	{"INF", true, false, false},
	{"INFINITY", true, false, false},
	{"nan", true, false, false},
	{"NAN", true, false, false},
	{"", false, false, false},
	{" ", false, false, false},
	{"+", false, true, false},
	{"-", false, false, false},
	{".", false, false, false},
	{"p1", false, false, false},
	{"1p", false, false, false},
	{"1p+", false, false, false},
	{"1p-", false, false, false},
	{"infi", true, false, false},
	{"infx", true, false, false},
	{"infinityx", true, false, false},
	{"INFi", true, false, false},
	{"nanx", true, false, false},
	{"NANx", true, false, false},
	{"nan(ind)", true, false, false},
	{"nan(ind)x", true, false, false},
	{"nan(", false, false, false}};

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  unsigned lexical_policy>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	auto const syntax_policy{lexical_policy % 3u};
	flags.allow_leading_plus = syntax_policy == 1u;
	flags.noskipws = syntax_policy == 2u;
	flags.comma = 3u <= lexical_policy;
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

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_outcome(
	observation<floating_type> const &left,
	observation<floating_type> const &right) noexcept
{
	return left.code == right.code && same_fields(left.value, right.value);
}

template <auto flags, typename floating_type>
[[nodiscard]] inline observation<floating_type> scan_direct(
	test_char_type const *first, test_char_type const *last) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		flags, floating_type &>;
	floating_type value{static_cast<floating_type>(-42.25)};
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
		last, manipulator_type{value})};
	return {value, static_cast<::std::size_t>(result.iter - first), result.code};
}

template <auto flags, typename floating_type>
[[nodiscard]] inline observation<floating_type> scan_padded(
	test_char_type const *first, test_char_type const *last,
	::std::size_t padding) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		flags, floating_type &>;
	floating_type value{static_cast<floating_type>(-42.25)};
	if constexpr (::fast_io::terminal_contiguous_padding_scannable<
					  test_char_type, manipulator_type>)
	{
		auto const result{::fast_io::scan_contiguous_padding_define(
			::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
			last, padding, manipulator_type{value})};
		return {value, static_cast<::std::size_t>(result.iter - first),
				result.code};
	}
	else
	{
		(void)padding;
		return scan_direct<flags, floating_type>(first, last);
	}
}

template <auto flags, typename floating_type>
[[nodiscard]] observation<floating_type> scan_context_chunks(
	test_char_type const *first, ::std::size_t size,
	::std::size_t chunk_size) noexcept
{
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		flags, floating_type &>;
	using state_type = typename ::std::remove_cvref_t<decltype(::fast_io::scan_context_type(
		::fast_io::io_reserve_type<test_char_type, manipulator_type>))>::type;
	state_type state{};
	floating_type value{static_cast<floating_type>(-42.25)};
	manipulator_type manipulator{value};
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

template <auto flags, typename floating_type>
[[nodiscard]] bool check_token(
	token_case const &test, ::std::size_t token_index) noexcept
{
	constexpr ::std::size_t capacity{256u};
	constexpr ::std::size_t maximum_padding{64u};
	char const *const ascii_first{test.text};
	::std::size_t token_size{};
	while (ascii_first[token_size] != '\0')
	{
		++token_size;
	}
	if (capacity <= token_size + 1u + maximum_padding)
	{
		return false;
	}
	test_char_type storage[capacity]{};
	for (::std::size_t index{}; index != token_size; ++index)
	{
		char8_t code_unit{};
		if (!execution_to_u8(ascii_first[index], code_unit))
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
	storage[token_size] =
		::fast_io::char_literal_v<u8'|', test_char_type>;
	auto const delimiter_size{token_size + 1u};

	auto const delimiter_context{scan_context_chunks<flags, floating_type>(
		storage, delimiter_size, delimiter_size)};
	auto const eof_context{scan_context_chunks<flags, floating_type>(
		storage, token_size, token_size)};
	auto const maximum_chunk{
		delimiter_size < 32u ? delimiter_size : ::std::size_t{32u}};
	for (::std::size_t chunk{1u}; chunk <= maximum_chunk; ++chunk)
	{
		auto const split_delimiter{scan_context_chunks<flags, floating_type>(
			storage, delimiter_size, chunk)};
		auto const split_eof{scan_context_chunks<flags, floating_type>(
			storage, token_size, chunk)};
		// An incomplete special prefix may consume a speculative suffix in an
		// earlier refill ("in" can still become "infinity").  A refillable
		// stream cannot rewind that previous chunk, and the context contract does
		// not promise an identical error cursor.  Complete spellings retain the
		// stronger cursor equality; incomplete spellings still require identical
		// status and target effects.
		auto const delimiter_matches{
			test.complete
				? same_observation(delimiter_context, split_delimiter)
				: same_outcome(delimiter_context, split_delimiter)};
		auto const eof_matches{
			test.complete
				? same_observation(eof_context, split_eof)
				: same_outcome(eof_context, split_eof)};
		if (!delimiter_matches || !eof_matches)
		{
			::std::fprintf(
				stderr,
				"atof context fragmentation mismatch: sizeof(flt)=%zu "
				"sizeof(char)=%zu format=%u rounding=%u plus=%u noskipws=%u "
				"token=%zu chunk=%zu delimiter(code/offset)=%u/%zu:%u/%zu "
				"eof(code/offset)=%u/%zu:%u/%zu text=%s\n",
				sizeof(floating_type), sizeof(test_char_type),
				static_cast<unsigned>(flags.floating),
				static_cast<unsigned>(flags.rounding),
				static_cast<unsigned>(flags.allow_leading_plus),
				static_cast<unsigned>(flags.noskipws), token_index, chunk,
				static_cast<unsigned>(delimiter_context.code),
				delimiter_context.consumed,
				static_cast<unsigned>(split_delimiter.code),
				split_delimiter.consumed,
				static_cast<unsigned>(eof_context.code), eof_context.consumed,
				static_cast<unsigned>(split_eof.code), split_eof.consumed,
				test.text);
			return false;
		}
	}

	constexpr bool allow_plus{flags.allow_leading_plus};
	constexpr bool allow_skip{!flags.noskipws};
	auto const contiguous_comparable{
		test.complete && (!test.requires_plus || allow_plus) &&
		(!test.requires_skip || allow_skip)};
	if (!contiguous_comparable)
	{
		return true;
	}
	auto const ordinary{scan_direct<flags, floating_type>(
		storage, storage + delimiter_size)};
	if (!same_observation(ordinary, delimiter_context))
	{
		::std::fprintf(
			stderr,
			"atof contiguous/context mismatch: sizeof(flt)=%zu sizeof(char)=%zu "
			"format=%u rounding=%u plus=%u noskipws=%u token=%zu "
			"ordinary(code/offset)=%u/%zu context=%u/%zu text=%s\n",
			sizeof(floating_type), sizeof(test_char_type),
			static_cast<unsigned>(flags.floating),
			static_cast<unsigned>(flags.rounding),
			static_cast<unsigned>(flags.allow_leading_plus),
			static_cast<unsigned>(flags.noskipws), token_index,
			static_cast<unsigned>(ordinary.code), ordinary.consumed,
			static_cast<unsigned>(delimiter_context.code),
			delimiter_context.consumed, test.text);
		return false;
	}
	constexpr ::std::size_t paddings[]{1u, 7u, 15u, 16u, 31u, 32u, 63u, 64u};
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
				storage, storage + delimiter_size, padding)};
			if (!same_observation(ordinary, padded))
			{
				::std::fprintf(
					stderr,
					"atof padding mismatch: sizeof(flt)=%zu sizeof(char)=%zu "
					"format=%u rounding=%u plus=%u noskipws=%u token=%zu "
					"padding=%zu pattern=%u text=%s\n",
					sizeof(floating_type), sizeof(test_char_type),
					static_cast<unsigned>(flags.floating),
					static_cast<unsigned>(flags.rounding),
					static_cast<unsigned>(flags.allow_leading_plus),
					static_cast<unsigned>(flags.noskipws), token_index,
					padding, pattern, test.text);
				return false;
			}
		}
	}
	return true;
}

template <auto flags, typename floating_type, ::std::size_t extent>
[[nodiscard]] bool check_tokens(token_case const (&tokens)[extent]) noexcept
{
	for (::std::size_t index{}; index != extent; ++index)
	{
		if (!check_token<flags, floating_type>(tokens[index], index))
		{
			return false;
		}
	}
	return true;
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  unsigned lexical_policy>
[[nodiscard]] bool check_configuration() noexcept
{
	constexpr auto flags{test_flags<format, rounding, lexical_policy>};
	if constexpr (format == ::fast_io::manipulators::floating_format::fixed)
	{
		return check_tokens<flags, floating_type>(fixed_tokens);
	}
	else if constexpr (
		format == ::fast_io::manipulators::floating_format::scientific)
	{
		return check_tokens<flags, floating_type>(scientific_tokens);
	}
	else if constexpr (
		format == ::fast_io::manipulators::floating_format::hexfloat)
	{
		return check_tokens<flags, floating_type>(hex_tokens);
	}
	else
	{
		return check_tokens<flags, floating_type>(general_tokens);
	}
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] bool check_lexical_policies() noexcept
{
	return check_configuration<floating_type, format, rounding, 0u>() &&
		   check_configuration<floating_type, format, rounding, 1u>() &&
		   check_configuration<floating_type, format, rounding, 2u>() &&
		   check_configuration<floating_type, format, rounding, 3u>() &&
		   check_configuration<floating_type, format, rounding, 4u>() &&
		   check_configuration<floating_type, format, rounding, 5u>();
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format>
[[nodiscard]] bool check_roundings() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	return check_lexical_policies<floating_type, format,
								  rounding::nearest_to_even>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::nearest_to_odd>() &&
		   check_lexical_policies<
			   floating_type, format,
			   rounding::nearest_toward_plus_infinity>() &&
		   check_lexical_policies<
			   floating_type, format,
			   rounding::nearest_toward_minus_infinity>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::nearest_toward_zero>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::nearest_away_from_zero>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::toward_plus_infinity>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::toward_minus_infinity>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::toward_zero>() &&
		   check_lexical_policies<floating_type, format,
								  rounding::away_from_zero>();
}

template <typename floating_type>
[[nodiscard]] bool check_type() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	return check_roundings<floating_type, format::general>() &&
		   check_roundings<floating_type, format::decimal>() &&
		   check_roundings<floating_type, format::fixed>() &&
		   check_roundings<floating_type, format::scientific>() &&
		   check_roundings<floating_type, format::hexfloat>();
}

} // namespace

int main()
{
	if (!check_type<float>() || !check_type<double>())
	{
		return 1;
	}
	::std::fputs(
		"atof fragmentation matrix passed: 600 static flag configurations, "
		"decimal/hex/special/error spellings, every small chunk boundary, EOF and "
		"16 padding variants\n",
		stdout);
}
