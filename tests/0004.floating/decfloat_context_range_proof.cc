#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{

inline constexpr auto decimal_flags{
	::fast_io::manipulators::floating_point_default_scalar_flags};
inline constexpr auto hexadecimal_flags = []() constexpr noexcept {
	auto flags{decimal_flags};
	flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	return flags;
}();

using decimal_scanner = ::fast_io::manipulators::scalar_manip_t<
	decimal_flags, double &>;
using decimal_precision_scanner =
	::fast_io::manipulators::scalar_manip_precision_t<decimal_flags, double &>;
using hexadecimal_scanner = ::fast_io::manipulators::scalar_manip_t<
	hexadecimal_flags, double &>;
using hexadecimal_precision_scanner =
	::fast_io::manipulators::scalar_manip_precision_t<hexadecimal_flags, double &>;

static_assert(
	::fast_io::context_scanner_result_in_range<char, decimal_scanner>);
static_assert(::fast_io::context_scanner_result_in_range<
			  char, decimal_precision_scanner>);
static_assert(
	!::fast_io::context_scanner_result_in_range<char, hexadecimal_scanner>);
static_assert(!::fast_io::context_scanner_result_in_range<
			  char, hexadecimal_precision_scanner>);

template <typename floating_type>
[[nodiscard]] inline constexpr bool same_fields(
	floating_type left, floating_type right) noexcept
{
	auto const lhs{::fast_io::details::get_punned_result(left)};
	auto const rhs{::fast_io::details::get_punned_result(right)};
	return lhs.mantissa == rhs.mantissa && lhs.exponent == rhs.exponent &&
		   lhs.sign == rhs.sign;
}

struct observation
{
	double value{};
	::std::size_t consumed{};
	::fast_io::parse_code code{};
};

[[nodiscard]] inline constexpr bool same_observation(
	observation const &left, observation const &right) noexcept
{
	return left.code == right.code && left.consumed == right.consumed &&
		   same_fields(left.value, right.value);
}

template <bool use_precision>
[[nodiscard]] observation scan_contiguous(
	::std::string_view text, ::std::size_t precision) noexcept
{
	double value{-42.25};
	auto const *const begin{text.data()};
	auto const *const end{begin + text.size()};
	if constexpr (use_precision)
	{
		decimal_precision_scanner scanner{value, precision};
		auto const result{::fast_io::scan_contiguous_define(
			::fast_io::io_reserve_type<char, decimal_precision_scanner>, begin,
			end, scanner)};
		return {value, static_cast<::std::size_t>(result.iter - begin),
				result.code};
	}
	else
	{
		(void)precision;
		decimal_scanner scanner{value};
		auto const result{::fast_io::scan_contiguous_define(
			::fast_io::io_reserve_type<char, decimal_scanner>, begin, end,
			scanner)};
		return {value, static_cast<::std::size_t>(result.iter - begin),
				result.code};
	}
}

template <bool use_precision>
[[nodiscard]] observation scan_fragmented(
	::std::string_view text, ::std::size_t chunk_size,
	::std::size_t precision) noexcept
{
	double value{-42.25};
	using scanner_type = ::std::conditional_t<
		use_precision, decimal_precision_scanner, decimal_scanner>;
	using state_type = typename ::std::remove_cvref_t<decltype(::fast_io::scan_context_type(
		::fast_io::io_reserve_type<char, scanner_type>))>::type;
	state_type state{};
	auto const tag{::fast_io::io_reserve_type<char, scanner_type>};
	auto const *const begin{text.data()};
	auto const *const end{begin + text.size()};
	::std::size_t offset{};

	auto scan_one = [&](char const *chunk_begin, char const *chunk_end) {
		if constexpr (use_precision)
		{
			return ::fast_io::scan_context_define(
				tag, state, chunk_begin, chunk_end,
				decimal_precision_scanner{value, precision});
		}
		else
		{
			return ::fast_io::scan_context_define(
				tag, state, chunk_begin, chunk_end,
				decimal_scanner{value});
		}
	};

	if (chunk_size == 0u)
	{
		auto const result{scan_one(begin, begin)};
		if (result.iter != begin || result.code != ::fast_io::parse_code::partial)
		{
			::std::abort();
		}
		chunk_size = 1u;
	}

	while (offset != text.size())
	{
		auto const remaining{text.size() - offset};
		auto const count{remaining < chunk_size ? remaining : chunk_size};
		auto const *cursor{begin + offset};
		auto const *const chunk_end{cursor + count};
		for (;;)
		{
			auto const result{scan_one(cursor, chunk_end)};
			/*
			The marker promises common-array provenance. These relational checks
			are therefore defined and directly exercise B <= R <= E for every
			fragment transition before any cursor is committed.
			*/
			if (result.iter < cursor || chunk_end < result.iter)
			{
				::std::abort();
			}
			offset += static_cast<::std::size_t>(result.iter - cursor);
			if (result.code != ::fast_io::parse_code::partial)
			{
				return {value, offset, result.code};
			}
			if (result.iter == chunk_end)
			{
				break;
			}
			if (result.iter == cursor)
			{
				::std::abort();
			}
			cursor = result.iter;
		}
	}

	if constexpr (use_precision)
	{
		auto const code{::fast_io::scan_context_eof_define(
			tag, state, decimal_precision_scanner{value, precision})};
		return {value, static_cast<::std::size_t>(end - begin), code};
	}
	else
	{
		auto const code{::fast_io::scan_context_eof_define(
			tag, state, decimal_scanner{value})};
		return {value, static_cast<::std::size_t>(end - begin), code};
	}
}

template <bool use_precision>
void check_matrix() noexcept
{
	struct token_case
	{
		::std::string_view text;
		bool contiguous_comparable;
	};
	constexpr token_case tokens[]{
		{"+12.5e2!", true},                   // Positive normal result.
		{"-0.03125!", true},                  // Negative normal result.
		{"inf!", true},                       // Special infinity path.
		{"-nan(payload)!", true},             // Signed NaN and special-buffer mapping.
		{"1e", false},                        // Exponent marker truncated by EOF.
		{"-1e+", false},                      // Exponent sign truncated by EOF.
		{"1e999999999999999999999999!", true} // Saturated exponent.
	};
	constexpr ::std::size_t chunk_sizes[]{0u, 1u, 3u, 7u};
	constexpr ::std::size_t precision{5u};
	for (auto const &test : tokens)
	{
		auto const expected{scan_fragmented<use_precision>(
			test.text, test.text.size() == 0u ? 1u : test.text.size(), precision)};
		if (test.contiguous_comparable &&
			!same_observation(
				scan_contiguous<use_precision>(test.text, precision), expected))
		{
			::std::abort();
		}
		for (auto const chunk_size : chunk_sizes)
		{
			auto const actual{
				scan_fragmented<use_precision>(test.text, chunk_size, precision)};
			if (!same_observation(expected, actual))
			{
				::std::fprintf(stderr,
							   "context range mismatch: precision=%d token=%.*s chunk=%zu "
							   "expected=(%u,%zu) actual=(%u,%zu)\n",
							   static_cast<int>(use_precision),
							   static_cast<int>(test.text.size()), test.text.data(), chunk_size,
							   static_cast<unsigned>(expected.code), expected.consumed,
							   static_cast<unsigned>(actual.code), actual.consumed);
				::std::abort();
			}
		}
	}
}

} // namespace

int main()
{
	check_matrix<false>();
	check_matrix<true>();
}
