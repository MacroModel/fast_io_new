#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr auto test_flags = []() constexpr noexcept {
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = ::fast_io::manipulators::floating_format::general;
	flags.precision = ::fast_io::manipulators::floating_precision::significant;
	flags.rounding = rounding;
	return flags;
}();

template <auto flags>
[[nodiscard]] ::std::string_view format(float value, char (&buffer)[64u]) noexcept
{
	using printable_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, float>;
	printable_type printable{value, 0u};
	auto const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char, printable_type>, buffer, printable)};
	return {buffer, static_cast<::std::size_t>(end - buffer)};
}

template <auto flags>
[[nodiscard]] ::fast_io::parse_code parse(
	::std::string_view text, float &value) noexcept
{
	using scanner_type =
		::fast_io::manipulators::scalar_manip_t<flags, float &>;
	auto const result{::fast_io::scan_contiguous_define(
		::fast_io::io_reserve_type<char, scanner_type>, text.data(),
		text.data() + text.size(), scanner_type{value})};
	return result.iter == text.data() + text.size()
			   ? result.code
			   : ::fast_io::parse_code::invalid;
}

template <auto flags>
[[nodiscard]] bool check_dispatches(
	::std::string_view text, ::fast_io::parse_code expected_code,
	float expected_value = 0.0f) noexcept
{
	using scanner_type =
		::fast_io::manipulators::scalar_manip_t<flags, float &>;
	auto const tag{::fast_io::io_reserve_type<char, scanner_type>};
	auto const value_matches = [expected_code, expected_value](float value) noexcept {
		return expected_code != ::fast_io::parse_code::ok ||
			   value == expected_value;
	};

	float direct_value{};
	if (parse<flags>(text, direct_value) != expected_code ||
		!value_matches(direct_value))
	{
		return false;
	}

	char padded_storage[128u]{};
	for (::std::size_t index{}; index != text.size(); ++index)
	{
		padded_storage[index] = text[index];
	}
	for (::std::size_t index{text.size()}; index != 128u; ++index)
	{
		padded_storage[index] = '9';
	}
	float padded_value{};
	auto const padded_result{::fast_io::scan_contiguous_padding_define(
		tag, padded_storage, padded_storage + text.size(), 64u,
		scanner_type{padded_value})};
	if (padded_result.code != expected_code ||
		padded_result.iter != padded_storage + text.size() ||
		!value_matches(padded_value))
	{
		return false;
	}

	for (::std::size_t chunk_size{1u}; chunk_size <= text.size(); ++chunk_size)
	{
		using state_type = typename ::std::remove_cvref_t<decltype(::fast_io::scan_context_type(tag))>::type;
		state_type state{};
		float context_value{};
		scanner_type scanner{context_value};
		::std::size_t offset{};
		bool finished{};
		while (offset != text.size())
		{
			auto const remaining{text.size() - offset};
			auto const count{remaining < chunk_size ? remaining : chunk_size};
			auto const *const chunk_begin{text.data() + offset};
			auto const *const chunk_end{chunk_begin + count};
			auto const result{::fast_io::scan_context_define(
				tag, state, chunk_begin, chunk_end, scanner)};
			if (result.iter < chunk_begin || chunk_end < result.iter)
			{
				return false;
			}
			offset += static_cast<::std::size_t>(result.iter - chunk_begin);
			if (result.code != ::fast_io::parse_code::partial)
			{
				if (result.code != expected_code || offset != text.size() ||
					!value_matches(context_value))
				{
					return false;
				}
				finished = true;
				break;
			}
			if (result.iter != chunk_end)
			{
				return false;
			}
		}
		if (!finished)
		{
			auto const code{
				::fast_io::scan_context_eof_define(tag, state, scanner)};
			if (code != expected_code || !value_matches(context_value))
			{
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] bool check_positive() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	constexpr auto upward_flags{test_flags<rounding::toward_plus_infinity>};
	constexpr auto toward_zero_flags{test_flags<rounding::toward_zero>};
	char upward_buffer[64u]{};
	char toward_zero_buffer[64u]{};
	auto const maximum{(::std::numeric_limits<float>::max)()};
	auto const upward{format<upward_flags>(maximum, upward_buffer)};
	auto const toward_zero{
		format<toward_zero_flags>(maximum, toward_zero_buffer)};
	/*
	float max is approximately 3.4028235e38.  On a one-significant-decimal
	digit grid, the least representable decimal not below it is 4e38, while the
	greatest one not above it is 3e38.  Therefore the two formatter results are
	the required directed roundings; changing 4e38 to keep a same-type
	round-trip would violate the requested ftoa policy.

	The corresponding atof decision is independent of dispatch.  4e38 lies
	above float's finite range: toward +infinity reports range overflow, whereas
	toward zero selects max finite.  This pins the numeric contract separately
	from contiguous/context/padding CPO strategy tests.
	*/
	if (upward != "4e+38" || toward_zero != "3e+38")
	{
		return false;
	}

	if (!check_dispatches<upward_flags>(
			upward, ::fast_io::parse_code::overflow))
	{
		return false;
	}
	return check_dispatches<toward_zero_flags>(
		upward, ::fast_io::parse_code::ok, maximum);
}

[[nodiscard]] bool check_negative() noexcept
{
	using rounding = ::fast_io::manipulators::floating_rounding;
	constexpr auto downward_flags{test_flags<rounding::toward_minus_infinity>};
	char buffer[64u]{};
	auto const text{format<downward_flags>(
		-(::std::numeric_limits<float>::max)(), buffer)};
	if (text != "-4e+38")
	{
		return false;
	}
	return check_dispatches<downward_flags>(
		text, ::fast_io::parse_code::overflow);
}

} // namespace

int main()
{
	return check_positive() && check_negative() ? 0 : 1;
}
