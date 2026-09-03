#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include <fast_io.h>

namespace
{

template <typename floating_type>
[[nodiscard]] bool decimal_digit_oracle_matches(floating_type value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	auto const fields{::fast_io::details::get_punned_result(value)};
	auto const maximum_exponent{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (fields.exponent == maximum_exponent ||
		(!fields.exponent && !fields.mantissa))
	{
		return true;
	}

	auto mantissa{fields.mantissa};
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	::std::int_least32_t binary_exponent{};
	if (fields.exponent)
	{
		mantissa |= static_cast<mantissa_type>(
			static_cast<mantissa_type>(1u) << trait::mbits);
		binary_exponent =
			static_cast<::std::int_least32_t>(fields.exponent) - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}

	/* Little-endian base-10 digits form an intentionally independent oracle:
	the production backend uses base 1e9, chunked powers and anchor tables. */
	unsigned char digits[::fast_io::details::exact_precision_digit_capacity<floating_type>]{};
	::std::size_t digit_size{};
	for (; mantissa; mantissa = static_cast<mantissa_type>(mantissa / 10u))
	{
		digits[digit_size++] =
			static_cast<unsigned char>(mantissa % 10u);
	}
	auto const multiplier{static_cast<unsigned>(
		binary_exponent < 0 ? 5u : 2u)};
	auto count{static_cast<::std::uint_least32_t>(
		binary_exponent < 0 ? -binary_exponent : binary_exponent)};
	for (; count; --count)
	{
		unsigned carry{};
		for (::std::size_t index{}; index != digit_size; ++index)
		{
			auto const product{
				static_cast<unsigned>(digits[index]) * multiplier + carry};
			digits[index] = static_cast<unsigned char>(product % 10u);
			carry = product / 10u;
		}
		for (; carry; carry /= 10u)
		{
			digits[digit_size++] = static_cast<unsigned char>(carry % 10u);
		}
	}

	auto decimal_exponent{
		binary_exponent < 0 ? binary_exponent : ::std::int_least32_t{0}};
	::std::size_t first{};
	for (; first + 1u != digit_size && !digits[first]; ++first)
	{
		++decimal_exponent;
	}
	auto const converted{
		::fast_io::details::exact_precision_from_binary<floating_type>(
			fields.mantissa, fields.exponent)};
	if (converted.exponent != decimal_exponent ||
		converted.size != digit_size - first)
	{
		return false;
	}
	for (::std::size_t index{}; index != converted.size; ++index)
	{
		if (converted.digits[index] != digits[digit_size - 1u - index])
		{
			return false;
		}
	}
	return true;
}

template <typename printable_type>
[[nodiscard]] bool buffer_protocol_matches(printable_type const &printable)
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	auto const precise_size{print_reserve_precise_size(tag{}, printable)};
	auto const reserve_size{[&]() noexcept {
		if constexpr (requires { print_reserve_size(tag{}, printable); })
		{
			return print_reserve_size(tag{}, printable);
		}
		else
		{
			return print_reserve_size(tag{});
		}
	}()};
	if (reserve_size < precise_size)
	{
		return false;
	}
	::std::vector<char> ordinary(reserve_size + 1u, '!');
	auto *const ordinary_end{
		print_reserve_define(tag{}, ordinary.data(), printable)};
	if (ordinary_end != ordinary.data() + precise_size ||
		ordinary[reserve_size] != '!')
	{
		return false;
	}
	::std::vector<char> precise(precise_size + 1u, '!');
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise.data(), precise_size, printable)};
	if (precise_end != precise.data() + precise_size ||
		precise[precise_size] != '!')
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
	::std::vector<char> exact_fit(precise_size + 1u, '!');
	auto const result{::fast_io::to_chars(
		exact_fit.data(), exact_fit.data() + precise_size, printable)};
	if (result.ec != ::std::errc{} ||
		result.ptr != exact_fit.data() + precise_size ||
		exact_fit[precise_size] != '!')
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (ordinary[index] != exact_fit[index])
		{
			return false;
		}
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_value(floating_type value,
							   ::std::uint_least16_t bits)
{
	using namespace ::fast_io::mnp;
	if (!decimal_digit_oracle_matches(value))
	{
		return false;
	}
	auto const minimum{static_cast<::std::size_t>((bits >> 3u) & 7u)};
	auto const maximum{
		(minimum ? minimum : 1u) +
		static_cast<::std::size_t>((bits >> 7u) & 15u)};
	switch ((bits >> 12u) & 3u)
	{
	case 0u:
		return buffer_protocol_matches(exact_decimal(value)) &&
			   buffer_protocol_matches(
				   precision_range(decimal(value), minimum, maximum));
	case 1u:
		return buffer_protocol_matches(exact_decimal(fixed(value))) &&
			   buffer_protocol_matches(
				   precision_range(fixed(value), minimum, maximum));
	case 2u:
		return buffer_protocol_matches(exact_decimal(scientific(value))) &&
			   buffer_protocol_matches(
				   precision_range(scientific(value), minimum, maximum));
	default:
		return buffer_protocol_matches(
				   json_float(exact_decimal(decimal(value)))) &&
			   buffer_protocol_matches(json_float(
				   precision_range(decimal(value), minimum, maximum)));
	}
}

template <typename floating_type>
[[nodiscard]] bool exhaust_binary16_domain()
{
	for (::std::uint_least32_t raw{}; raw != 0x10000u; ++raw)
	{
		auto const bits{static_cast<::std::uint_least16_t>(raw)};
		auto const value{::std::bit_cast<floating_type>(bits)};
		if (!check_value(value, bits))
		{
			::fast_io::io::perr("exact narrow exhaustive failure: bits=",
								::fast_io::mnp::hex0x(bits), ",oracle=",
								decimal_digit_oracle_matches(value), "\n");
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	bool result{true};
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	result = result && exhaust_binary16_domain<_Float16>();
#endif
#if defined(__BFLT16_MANT_DIG__) && __BFLT16_MANT_DIG__ == 8
	result = result && exhaust_binary16_domain<__bf16>();
#endif
	return !result;
}
