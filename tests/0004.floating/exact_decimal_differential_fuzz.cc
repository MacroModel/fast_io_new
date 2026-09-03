#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

#include <fast_io.h>

struct synthetic_ibm_double_double_carrier
{
	unsigned char storage[16u];
};

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
namespace fast_io::details
{

template <>
struct iec559_traits<::synthetic_ibm_double_double_carrier>
{
	using mantissa_type = __uint128_t;
	inline static constexpr ::std::size_t mbits{105u};
	inline static constexpr ::std::size_t ebits{11u};
	inline static constexpr ::std::uint_least32_t m10digits{33u};
	inline static constexpr ::std::uint_least32_t m2hexdigits{27u};
	inline static constexpr ::std::uint_least32_t e10digits{3u};
	inline static constexpr ::std::uint_least32_t e2hexdigits{4u};
	inline static constexpr ::std::uint_least32_t e10max{308u};
};

} // namespace fast_io::details
#endif

namespace
{

[[nodiscard]] ::std::uint_least64_t load64(
	unsigned char const *data, ::std::size_t size,
	::std::size_t offset = 0u) noexcept
{
	::std::uint_least64_t value{};
	for (::std::size_t index{}; index != 8u; ++index)
	{
		value |= static_cast<::std::uint_least64_t>(
					 index + offset < size ? data[index + offset] : 0u)
				 << (index * 8u);
	}
	return value;
}

template <typename printable_type>
[[nodiscard]] bool check_buffer_protocol(printable_type const &printable)
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
	auto const exact_result{::fast_io::to_chars(
		exact_fit.data(), exact_fit.data() + precise_size, printable)};
	if (exact_result.ec != ::std::errc{} ||
		exact_result.ptr != exact_fit.data() + precise_size ||
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
	if (precise_size)
	{
		::std::vector<char> rejected(precise_size + 1u, '!');
		auto const rejected_result{::fast_io::to_chars(
			rejected.data(), rejected.data() + precise_size - 1u, printable)};
		if (rejected_result.ec != ::std::errc::value_too_large ||
			rejected_result.ptr != rejected.data() + precise_size - 1u)
		{
			return false;
		}
		for (auto const character : rejected)
		{
			if (character != '!')
			{
				return false;
			}
		}
	}
	return true;
}

template <typename char_type>
[[nodiscard]] constexpr char_type translate_ascii(char value) noexcept
{
	auto const byte{static_cast<unsigned char>(value)};
	if (static_cast<unsigned char>('0') <= byte &&
		byte <= static_cast<unsigned char>('9'))
	{
		return ::fast_io::char_literal_add<char_type>(
			byte - static_cast<unsigned char>('0'));
	}
	switch (value)
	{
	case '-':
		return ::fast_io::char_literal_v<u8'-', char_type>;
	case '+':
		return ::fast_io::char_literal_v<u8'+', char_type>;
	case '.':
		return ::fast_io::char_literal_v<u8'.', char_type>;
	case ',':
		return ::fast_io::char_literal_v<u8',', char_type>;
	case 'e':
		return ::fast_io::char_literal_v<u8'e', char_type>;
	case 'E':
		return ::fast_io::char_literal_v<u8'E', char_type>;
	case 'i':
		return ::fast_io::char_literal_v<u8'i', char_type>;
	case 'n':
		return ::fast_io::char_literal_v<u8'n', char_type>;
	case 'f':
		return ::fast_io::char_literal_v<u8'f', char_type>;
	case 'a':
		return ::fast_io::char_literal_v<u8'a', char_type>;
	case 'N':
		return ::fast_io::char_literal_v<u8'N', char_type>;
	case 'I':
		return ::fast_io::char_literal_v<u8'I', char_type>;
	case 'F':
		return ::fast_io::char_literal_v<u8'F', char_type>;
	case 'A':
		return ::fast_io::char_literal_v<u8'A', char_type>;
	default:
		return char_type{};
	}
}

template <typename char_type, typename printable_type>
[[nodiscard]] bool check_character_output(
	printable_type const &printable) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using narrow_tag = ::fast_io::io_reserve_type_t<char, clean_type>;
	using wide_tag = ::fast_io::io_reserve_type_t<char_type, clean_type>;
	auto const narrow_bound{[&]() noexcept {
		if constexpr (requires {
						  print_reserve_size(narrow_tag{}, printable);
					  })
		{
			return print_reserve_size(narrow_tag{}, printable);
		}
		else
		{
			return print_reserve_size(narrow_tag{});
		}
	}()};
	auto const wide_bound{[&]() noexcept {
		if constexpr (requires {
						  print_reserve_size(wide_tag{}, printable);
					  })
		{
			return print_reserve_size(wide_tag{}, printable);
		}
		else
		{
			return print_reserve_size(wide_tag{});
		}
	}()};
	::std::vector<char> narrow(narrow_bound);
	::std::vector<char_type> wide(wide_bound);
	auto *const narrow_end{
		print_reserve_define(narrow_tag{}, narrow.data(), printable)};
	auto *const wide_end{
		print_reserve_define(wide_tag{}, wide.data(), printable)};
	auto const size{static_cast<::std::size_t>(narrow_end - narrow.data())};
	if (wide_end != wide.data() + size)
	{
		return false;
	}
	for (::std::size_t index{}; index != size; ++index)
	{
		if (wide[index] != translate_ascii<char_type>(narrow[index]))
		{
			return false;
		}
	}
	return true;
}

template <typename printable_type>
[[nodiscard]] bool check_printable(printable_type const &printable,
								   unsigned character_selector)
{
	if (!check_buffer_protocol(printable))
	{
		return false;
	}
	switch (character_selector % 5u)
	{
	case 0u:
		return check_character_output<char>(printable);
	case 1u:
		return check_character_output<wchar_t>(printable);
	case 2u:
		return check_character_output<char8_t>(printable);
	case 3u:
		return check_character_output<char16_t>(printable);
	default:
		return check_character_output<char32_t>(printable);
	}
}

template <typename floating_type>
[[nodiscard]] bool check_frontends(floating_type value,
								   ::std::uint_least64_t selector)
{
	using namespace ::fast_io::mnp;
	auto const minimum{static_cast<::std::size_t>((selector >> 8u) & 15u)};
	auto const maximum{
		(minimum ? minimum : 1u) +
		static_cast<::std::size_t>((selector >> 16u) & 31u)};
	auto const character_selector{static_cast<unsigned>(selector >> 24u)};
	switch (selector & 7u)
	{
	case 0u:
		return check_printable(exact_decimal(value), character_selector) &&
			   check_printable(
				   precision_range(decimal(value), minimum, maximum),
				   character_selector);
	case 1u:
		return check_printable(exact_decimal(fixed(value)), character_selector) &&
			   check_printable(
				   precision_range(fixed(value), minimum, maximum),
				   character_selector);
	case 2u:
		return check_printable(
				   exact_decimal(scientific(value)), character_selector) &&
			   check_printable(
				   precision_range(scientific(value), minimum, maximum),
				   character_selector);
	case 3u:
		return check_printable(
				   exact_decimal(scientific<true>(value)), character_selector) &&
			   check_printable(
				   precision_range(scientific<true>(value), minimum, maximum),
				   character_selector);
	case 4u:
		return check_printable(
				   exact_decimal(comma_decimal(value)), character_selector) &&
			   check_printable(
				   precision_range(comma_decimal(value), minimum, maximum),
				   character_selector);
	default:
		return check_printable(
				   json_float(exact_decimal(decimal(value))),
				   character_selector) &&
			   check_printable(json_float(precision_range(
								   decimal(value), minimum, maximum)),
							   character_selector);
	}
}

template <typename floating_type>
[[nodiscard]] bool exact_backends_equal(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type
		mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const maximum_exponent{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == maximum_exponent || (!exponent && !mantissa))
	{
		return true;
	}
	auto const optimized{
		::fast_io::details::exact_decimal_from_binary<floating_type>(
			mantissa, exponent)};
	auto const reference{
		::fast_io::details::exact_precision_from_binary<floating_type>(
			mantissa, exponent)};
	auto const layout{
		::fast_io::details::floating_precise_exact_decimal_layout_from_binary<
			floating_type>(mantissa, exponent)};
	if (layout.size != reference.size ||
		layout.exponent != reference.exponent)
	{
		return false;
	}
	if (optimized.size != reference.size ||
		optimized.exponent != reference.exponent)
	{
		return false;
	}
	for (::std::size_t index{}; index != optimized.size; ++index)
	{
		if (optimized.digits[index] != reference.digits[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] unsigned selected_domain() noexcept
{
	static unsigned const domain{[]() noexcept {
		auto const *const text{::std::getenv("FAST_IO_EXACT_FUZZ_DOMAIN")};
		if (text == nullptr || *text == '\0')
		{
			return 0u;
		}
		char *end{};
		auto const value{::std::strtoul(text, __builtin_addressof(end), 10)};
		if (end == text || *end != '\0' || 4u < value)
		{
			::std::abort();
		}
		return static_cast<unsigned>(value);
	}()};
	return domain;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(unsigned char const *data,
									  ::std::size_t size)
{
	auto const low{load64(data, size)};
	auto const high{load64(data, size, 8u)};
	bool result{};
	switch (selected_domain())
	{
	case 0u:
		result = check_frontends(
			::std::bit_cast<float>(static_cast<::std::uint_least32_t>(low)),
			high);
		break;
	case 1u:
		result = check_frontends(::std::bit_cast<double>(low), high);
		break;
	case 2u:
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	{
		using storage_type = ::fast_io::details::float80_storage<
			sizeof(long double) - sizeof(::std::uint_least64_t) -
			sizeof(::std::uint_least16_t)>;
		auto const exponent{
			static_cast<::std::uint_least16_t>((high >> 48u) & 0x7fffu)};
		storage_type storage{};
		storage.mantissa = low & UINT64_C(0x7fffffffffffffff);
		if (exponent)
		{
			storage.mantissa |= UINT64_C(0x8000000000000000);
		}
		storage.exponent = static_cast<::std::uint_least16_t>(
			exponent | ((high >> 63u) << 15u));
		auto const value{::std::bit_cast<long double>(storage)};
		result = exact_backends_equal<long double>(
					 low & UINT64_C(0x7fffffffffffffff), exponent) &&
				 check_frontends(value, high);
	}
#else
		result = true;
#endif
	break;
	case 3u:
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	{
		auto const bits{static_cast<__uint128_t>(high) << 64u | low};
		auto const value{::std::bit_cast<__float128>(bits)};
		auto const fields{::fast_io::details::get_punned_result(value)};
		result = exact_backends_equal<__float128>(
					 fields.mantissa, fields.exponent) &&
				 check_frontends(value, high);
	}
#else
		result = true;
#endif
	break;
	default:
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	{
		constexpr auto mask{
			(static_cast<__uint128_t>(1u) << 105u) - 1u};
		auto const mantissa{
			(static_cast<__uint128_t>(high) << 64u | low) & mask};
		auto const exponent{
			static_cast<::std::uint_least32_t>((high >> 41u) & 0x7ffu)};
		result = exact_backends_equal<
			::synthetic_ibm_double_double_carrier>(mantissa, exponent);
	}
#else
		result = true;
#endif
	break;
	}
	if (!result)
	{
		::std::abort();
	}
	return 0;
}
