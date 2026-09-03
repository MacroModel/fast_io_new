#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

#ifndef FAST_IO_FTOA_ATOF_TEST_CHAR_TYPE
#define FAST_IO_FTOA_ATOF_TEST_CHAR_TYPE char
#endif

namespace ftoa_atof_strategy_matrix
{

using test_char_type = FAST_IO_FTOA_ATOF_TEST_CHAR_TYPE;

inline constexpr ::std::size_t refill_storage_size{64u};

struct refill_source
{
	using input_char_type = test_char_type;

	::std::basic_string_view<test_char_type> source{};
	::std::size_t source_position{};
	::std::size_t chunk_size{1u};
	test_char_type storage[refill_storage_size]{};
	test_char_type *current{storage};
	test_char_type *end{storage};

	inline constexpr void reset(
		::std::basic_string_view<test_char_type> text,
		::std::size_t requested_chunk_size) noexcept
	{
		source = text;
		source_position = 0u;
		chunk_size = requested_chunk_size == 0u
						 ? 1u
						 : (requested_chunk_size < refill_storage_size
								? requested_chunk_size
								: refill_storage_size);
		current = end = storage;
	}

	[[nodiscard]] inline constexpr ::std::size_t consumed() const noexcept
	{
		return source_position - static_cast<::std::size_t>(end - current);
	}
};

struct refill_source_ref
{
	using input_char_type = test_char_type;
	refill_source *source;
};

[[nodiscard]] inline constexpr refill_source_ref
input_stream_ref_define(refill_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

[[nodiscard]] inline constexpr test_char_type *
ibuffer_begin(refill_source_ref ref) noexcept
{
	return ref.source->storage;
}

[[nodiscard]] inline constexpr test_char_type *
ibuffer_curr(refill_source_ref ref) noexcept
{
	return ref.source->current;
}

[[nodiscard]] inline constexpr test_char_type *
ibuffer_end(refill_source_ref ref) noexcept
{
	return ref.source->end;
}

inline constexpr void ibuffer_set_curr(
	refill_source_ref ref, test_char_type *current) noexcept
{
	ref.source->current = current;
}

inline bool ibuffer_underflow(refill_source_ref ref) noexcept
{
	auto &source{*ref.source};
	auto const remaining{source.source.size() - source.source_position};
	auto const count{remaining < source.chunk_size ? remaining : source.chunk_size};
	if (count == 0u)
	{
		return false;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		source.storage[index] = source.source[source.source_position + index];
	}
	source.source_position += count;
	source.current = source.storage;
	source.end = source.storage + count;
	return true;
}

template <typename input_type, typename scanner_type>
[[nodiscard]] inline bool scan_one(
	input_type &input, scanner_type &scanner) noexcept
{
	// This test supplies an already-normalized scalar scanner so it enters the
	// same freestanding dispatcher used by the hosted io::scan facade without
	// pulling platform headers into the C++20 matrix build.
	decltype(auto) input_ref{input_stream_ref_define(input)};
	return ::fast_io::operations::decay::
		scan_freestanding_decay_borrowed_input(input_ref, scanner);
}

template <::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
inline constexpr auto print_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	flags.precision = precision_mode;
	flags.showbase = decorated;
	flags.showpos = decorated;
	flags.uppercase_showbase = decorated;
	flags.uppercase = decorated;
	flags.uppercase_e = decorated;
	flags.comma = decorated;
	flags.json_float =
		decorated && format != ::fast_io::manipulators::floating_format::hexfloat;
	flags.nan_show_type = decorated;
	return flags;
}();

template <auto flags>
inline constexpr auto matching_scan_flags = []() constexpr noexcept {
	auto result{flags};
	// The formatter's showpos policy and the scanner's leading-plus policy are
	// intentionally independent.  A generated leading plus is valid input for
	// this round-trip matrix only after the scanner explicitly opts in.
	result.allow_leading_plus = flags.showpos;
	return result;
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
struct scan_observation
{
	floating_type value{};
	::std::size_t consumed{};
	::fast_io::parse_code code{};
};

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_observation(
	scan_observation<floating_type> const &left,
	scan_observation<floating_type> const &right) noexcept
{
	return left.code == right.code && left.consumed == right.consumed &&
		   same_fields(left.value, right.value);
}

template <auto flags, typename floating_type>
[[nodiscard]] inline scan_observation<floating_type> scan_direct(
	test_char_type const *first, test_char_type const *last) noexcept
{
	constexpr auto scan_flags{matching_scan_flags<flags>};
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		scan_flags, floating_type &>;
	floating_type value{static_cast<floating_type>(-42.25)};
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
		last, manipulator_type{value})};
	return {value, static_cast<::std::size_t>(result.iter - first), result.code};
}

template <auto flags, typename floating_type>
[[nodiscard]] inline bool check_direct_padding(
	test_char_type const *first, test_char_type const *last,
	::std::size_t padding,
	scan_observation<floating_type> const &expected) noexcept
{
	constexpr auto scan_flags{matching_scan_flags<flags>};
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		scan_flags, floating_type &>;
	if constexpr (::fast_io::terminal_contiguous_padding_scannable<
					  test_char_type, manipulator_type>)
	{
		floating_type value{static_cast<floating_type>(-42.25)};
		auto const result{::fast_io::scan_contiguous_padding_define(
			::fast_io::io_reserve_type<test_char_type, manipulator_type>, first,
			last, padding, manipulator_type{value})};
		return same_observation(
			expected,
			{value, static_cast<::std::size_t>(result.iter - first), result.code});
	}
	else
	{
		(void)first;
		(void)last;
		(void)padding;
		(void)expected;
		return true;
	}
}

template <auto flags, typename floating_type>
[[nodiscard]] inline bool check_terminal_view(
	test_char_type const *first, test_char_type const *last,
	scan_observation<floating_type> const &expected) noexcept
{
	constexpr auto scan_flags{matching_scan_flags<flags>};
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		scan_flags, floating_type &>;
	::fast_io::basic_ibuffer_view<test_char_type> input{first, last};
	floating_type value{static_cast<floating_type>(-42.25)};
	manipulator_type manipulator{value};
	if (!scan_one(input, manipulator))
	{
		return false;
	}
	return same_observation(
		expected,
		{value, static_cast<::std::size_t>(input.curr_ptr - first),
		 ::fast_io::parse_code::ok});
}

template <auto flags, typename floating_type>
[[nodiscard]] inline bool check_padded_view(
	test_char_type const *first, test_char_type const *last,
	::std::size_t padding,
	scan_observation<floating_type> const &expected) noexcept
{
	constexpr auto scan_flags{matching_scan_flags<flags>};
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		scan_flags, floating_type &>;
	::fast_io::basic_padded_ibuffer_view<test_char_type> input{
		first, last, padding};
	floating_type value{static_cast<floating_type>(-42.25)};
	manipulator_type manipulator{value};
	if (!scan_one(input, manipulator))
	{
		return false;
	}
	return same_observation(
		expected,
		{value, static_cast<::std::size_t>(input.curr_ptr - first),
		 ::fast_io::parse_code::ok});
}

template <auto flags, typename floating_type>
[[nodiscard]] inline bool check_refill_view(
	::std::basic_string_view<test_char_type> text, ::std::size_t chunk_size,
	scan_observation<floating_type> const &expected) noexcept
{
	constexpr auto scan_flags{matching_scan_flags<flags>};
	using manipulator_type = ::fast_io::manipulators::scalar_manip_t<
		scan_flags, floating_type &>;
	refill_source input;
	input.reset(text, chunk_size);
	floating_type value{static_cast<floating_type>(-42.25)};
	manipulator_type manipulator{value};
	if (!scan_one(input, manipulator))
	{
		return false;
	}
	return same_observation(
		expected,
		{value, input.consumed(), ::fast_io::parse_code::ok});
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
[[nodiscard]] bool check_value(
	floating_type value, ::std::size_t precision,
	::std::size_t value_index) noexcept
{
	constexpr auto flags{print_flags<format, rounding, precision_mode, decorated>};
	using print_type = ::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type>;
	static_assert(::fast_io::precise_reserve_printable<test_char_type, print_type>);

	constexpr ::std::size_t capacity{4096u};
	constexpr ::std::size_t padding{64u};
	test_char_type ordinary[capacity + padding]{};
	test_char_type precise[capacity]{};
	print_type printable{value, precision};
	auto const tag{::fast_io::io_reserve_type<test_char_type, print_type>};
	auto const reserve_size{::fast_io::print_reserve_size(tag, printable)};
	auto const precise_size{
		::fast_io::print_reserve_precise_size(tag, printable)};
	if (capacity <= precise_size || reserve_size < precise_size ||
		capacity < reserve_size)
	{
		return false;
	}
	auto const ordinary_end{
		::fast_io::print_reserve_define(tag, ordinary, printable)};
	auto const precise_end{::fast_io::print_reserve_precise_define(
		tag, precise, precise_size, printable)};
	if (static_cast<::std::size_t>(ordinary_end - ordinary) != precise_size ||
		static_cast<::std::size_t>(precise_end - precise) != precise_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			return false;
		}
	}
	if constexpr (
		precision_mode !=
		::fast_io::manipulators::floating_precision::significant)
	{
		// Runtime precision changes ftoa spelling, while a scalar atof CPO does
		// not consume the formatter's precision policy.  All four policies keep
		// the ftoa checks above; one representative policy exercises every scan
		// path so the test retains 800 formatter configurations without creating
		// four semantically duplicate context state machines per flag set.
		return true;
	}
	else
	{

		test_char_type scan_text[capacity + padding]{};
		for (::std::size_t index{}; index != precise_size; ++index)
		{
			scan_text[index] = ordinary[index];
		}

		// The delimiter is semantic input but is not part of the floating token.
		// Two physically different padding tails prove that a padded CPO cannot
		// consume or use their values to alter the result.
		scan_text[precise_size] =
			::fast_io::char_literal_v<u8'|', test_char_type>;
		auto const semantic_size{precise_size + 1u};
		auto const semantic_end{scan_text + semantic_size};
		for (::std::size_t index{}; index != padding; ++index)
		{
			scan_text[semantic_size + index] =
				::fast_io::char_literal_v<u8'9', test_char_type>;
		}
		auto const expected{scan_direct<flags, floating_type>(
			scan_text, semantic_end)};
		if (expected.code == ::fast_io::parse_code::overflow &&
			expected.consumed == precise_size)
		{
			// A deliberately coarse directed ftoa can round max finite past the
			// destination type's range (for example float max at p=0).  That spelling
			// is valid and the direct atof check above must report overflow; public
			// scan converts the non-success protocol code into its error policy, so it
			// is not entered by this success-path strategy matrix.
			return true;
		}
		if (expected.code != ::fast_io::parse_code::ok ||
			expected.consumed != precise_size)
		{
			::std::fprintf(
				stderr,
				"ftoa spelling is outside atof grammar: sizeof(flt)=%zu "
				"sizeof(char)=%zu format=%u rounding=%u precision_mode=%u "
				"decorated=%u value=%zu precision=%zu code=%u consumed=%zu/%zu\n",
				sizeof(floating_type), sizeof(test_char_type),
				static_cast<unsigned>(format), static_cast<unsigned>(rounding),
				static_cast<unsigned>(precision_mode),
				static_cast<unsigned>(decorated), value_index, precision,
				static_cast<unsigned>(expected.code), expected.consumed,
				precise_size);
			return false;
		}
		auto const direct_padding_matches{check_direct_padding<flags>(
			scan_text, semantic_end, padding, expected)};
		auto const terminal_matches{
			check_terminal_view<flags>(scan_text, semantic_end, expected)};
		auto const padded_matches{check_padded_view<flags>(
			scan_text, semantic_end, padding, expected)};
		if (!direct_padding_matches || !terminal_matches || !padded_matches)
		{
			::std::fprintf(
				stderr,
				"ftoa/atof strategy mismatch: sizeof(flt)=%zu sizeof(char)=%zu "
				"format=%u rounding=%u precision_mode=%u decorated=%u "
				"value=%zu precision=%zu stage=terminal code=%u consumed=%zu/%zu "
				"direct-padding=%u terminal=%u padded=%u\n",
				sizeof(floating_type), sizeof(test_char_type),
				static_cast<unsigned>(format), static_cast<unsigned>(rounding),
				static_cast<unsigned>(precision_mode),
				static_cast<unsigned>(decorated), value_index, precision,
				static_cast<unsigned>(expected.code), expected.consumed, precise_size,
				static_cast<unsigned>(direct_padding_matches),
				static_cast<unsigned>(terminal_matches),
				static_cast<unsigned>(padded_matches));
			::std::fputs("token code units:", stderr);
			for (::std::size_t index{}; index != precise_size; ++index)
			{
				::std::fprintf(
					stderr, " %x",
					static_cast<unsigned>(
						static_cast<::std::make_unsigned_t<test_char_type>>(
							scan_text[index])));
			}
			::std::fputc('\n', stderr);
			return false;
		}
		for (::std::size_t index{}; index != padding; ++index)
		{
			scan_text[semantic_size + index] =
				::fast_io::char_literal_v<u8'/', test_char_type>;
		}
		if (!check_direct_padding<flags>(
				scan_text, semantic_end, padding, expected) ||
			!check_padded_view<flags>(
				scan_text, semantic_end, padding, expected))
		{
			::std::fprintf(
				stderr,
				"ftoa/atof padding-value mismatch: sizeof(flt)=%zu "
				"sizeof(char)=%zu format=%u rounding=%u precision_mode=%u "
				"decorated=%u value=%zu precision=%zu\n",
				sizeof(floating_type), sizeof(test_char_type),
				static_cast<unsigned>(format), static_cast<unsigned>(rounding),
				static_cast<unsigned>(precision_mode),
				static_cast<unsigned>(decorated), value_index, precision);
			return false;
		}

		constexpr ::std::size_t chunks[]{1u, 2u, 3u, 7u, 16u, 31u};
		auto const text{::std::basic_string_view<test_char_type>{
			scan_text, semantic_size}};
		for (auto const chunk : chunks)
		{
			if (!check_refill_view<flags>(text, chunk, expected))
			{
				::std::fprintf(
					stderr,
					"ftoa/atof refill mismatch: sizeof(flt)=%zu sizeof(char)=%zu "
					"format=%u rounding=%u precision_mode=%u decorated=%u "
					"value=%zu precision=%zu chunk=%zu\n",
					sizeof(floating_type), sizeof(test_char_type),
					static_cast<unsigned>(format), static_cast<unsigned>(rounding),
					static_cast<unsigned>(precision_mode),
					static_cast<unsigned>(decorated), value_index, precision,
					chunk);
				return false;
			}
		}
		return true;
	}
}

template <typename floating_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool decorated>
[[nodiscard]] bool check_configuration() noexcept
{
	constexpr floating_type values[]{
		floating_type{},
		-static_cast<floating_type>(0.0),
		static_cast<floating_type>(0.1),
		static_cast<floating_type>(-0.1),
		static_cast<floating_type>(1.0),
		static_cast<floating_type>(9.9999995),
		static_cast<floating_type>(0.000123456789),
		static_cast<floating_type>(123456789.25),
		(::std::numeric_limits<floating_type>::denorm_min)(),
		(::std::numeric_limits<floating_type>::min)(),
		(::std::numeric_limits<floating_type>::max)(),
		(::std::numeric_limits<floating_type>::infinity)(),
		-(::std::numeric_limits<floating_type>::infinity)(),
		(::std::numeric_limits<floating_type>::quiet_NaN)(),
		-(::std::numeric_limits<floating_type>::quiet_NaN)()};
	constexpr ::std::size_t precisions[]{
		0u, 1u, 2u, 6u,
		static_cast<::std::size_t>(
			::std::numeric_limits<floating_type>::max_digits10),
		31u};
	for (::std::size_t value_index{};
		 value_index != sizeof(values) / sizeof(*values); ++value_index)
	{
		for (auto const precision : precisions)
		{
			if (!check_value<floating_type, format, rounding, precision_mode,
							 decorated>(values[value_index], precision, value_index))
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
		  ::fast_io::manipulators::floating_precision precision_mode>
[[nodiscard]] bool check_decorations() noexcept
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
	return check_decorations<floating_type, format, rounding,
							 precision::significant>() &&
		   check_decorations<floating_type, format, rounding,
							 precision::fractional>() &&
		   check_decorations<
			   floating_type, format, rounding,
			   precision::significant_preserve_trailing_zero>() &&
		   check_decorations<
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
		   check_precision_modes<
			   floating_type, format,
			   rounding::nearest_toward_plus_infinity>() &&
		   check_precision_modes<
			   floating_type, format,
			   rounding::nearest_toward_minus_infinity>() &&
		   check_precision_modes<floating_type, format,
								 rounding::nearest_toward_zero>() &&
		   check_precision_modes<floating_type, format,
								 rounding::nearest_away_from_zero>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_plus_infinity>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_minus_infinity>() &&
		   check_precision_modes<floating_type, format,
								 rounding::toward_zero>() &&
		   check_precision_modes<floating_type, format,
								 rounding::away_from_zero>();
}

template <typename floating_type>
[[nodiscard]] bool check_formats() noexcept
{
	using format = ::fast_io::manipulators::floating_format;
	return check_roundings<floating_type, format::general>() &&
		   check_roundings<floating_type, format::decimal>() &&
		   check_roundings<floating_type, format::fixed>() &&
		   check_roundings<floating_type, format::scientific>() &&
		   check_roundings<floating_type, format::hexfloat>();
}

} // namespace ftoa_atof_strategy_matrix

int main()
{
	using namespace ::ftoa_atof_strategy_matrix;
	if (!check_formats<float>() || !check_formats<double>())
	{
		return 1;
	}
	::std::fprintf(
		stdout,
		"ftoa/atof matrix passed: 800 static configurations, 72000 "
		"formatted values, 18000 atof inputs with success cases checked across "
		"12 scan observations\n");
}
