#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fast_io.h>

namespace
{

inline ::std::uint_least64_t volatile precise_size_audit_sink{};

template <::std::integral char_type, typename printable_type>
[[nodiscard]] bool check_printable(printable_type const &value) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char_type, clean_type>;
	constexpr ::std::size_t buffer_size{20000u};
	static char_type ordinary[buffer_size];
	static char_type precise[buffer_size + 1u];
	precise[buffer_size] = static_cast<char_type>(0x5au);
	auto const reserve_size{print_reserve_size(tag{}, value)};
	if (buffer_size < reserve_size)
	{
		return false;
	}
	auto *const ordinary_end{
		print_reserve_define(tag{}, ordinary, value)};
	auto const precise_size{print_reserve_precise_size(tag{}, value)};
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise, precise_size, value)};
	if (ordinary_end != ordinary + precise_size ||
		precise_end != precise + precise_size ||
		precise[buffer_size] != static_cast<char_type>(0x5au))
	{
		::fast_io::io::perr("reserve=", reserve_size, ",ordinary=",
							static_cast<::std::size_t>(ordinary_end - ordinary), ",precise_size=",
							precise_size, ",precise_define=",
							static_cast<::std::size_t>(precise_end - precise), ",");
		return false;
	}
	::std::uint_least64_t hash{precise_size};
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			::fast_io::io::perr("content_index=", index, ",ordinary_code=",
								static_cast<::std::uint_least64_t>(ordinary[index]),
								",precise_code=",
								static_cast<::std::uint_least64_t>(precise[index]), ",");
			return false;
		}
		hash = (hash ^ static_cast<::std::uint_least64_t>(precise[index])) *
			   UINT64_C(1099511628211);
	}
	precise_size_audit_sink = precise_size_audit_sink ^ hash;
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_value(floating_type value,
							   ::std::size_t minimum, ::std::size_t maximum) noexcept
{
	using namespace ::fast_io::mnp;
	if (!check_printable<char>(
			precision_range(decimal(value), minimum, maximum)))
	{
		::fast_io::io::perr("format=decimal,");
		return false;
	}
	if (!check_printable<wchar_t>(
			precision_range(fixed(value), minimum, maximum)))
	{
		::fast_io::io::perr("format=fixed,");
		return false;
	}
	if (!check_printable<char8_t>(
			precision_range(scientific(value), minimum, maximum)))
	{
		::fast_io::io::perr("format=scientific,");
		return false;
	}
	if (!check_printable<char16_t>(
			precision_range(general(value), minimum, maximum)))
	{
		::fast_io::io::perr("format=general,");
		return false;
	}
	if (!check_printable<char32_t>(json_float(
			precision_range(comma_decimal(value), minimum, maximum))))
	{
		::fast_io::io::perr("format=json,");
		return false;
	}
	return true;
}

template <typename floating_type>
[[nodiscard]] bool check_every_finite_exponent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	constexpr auto mantissa_mask{
		static_cast<mantissa_type>(
			(static_cast<mantissa_type>(1u) << trait::mbits) - 1u)};
	for (::std::uint_least32_t exponent{}; exponent != exponent_mask;
		 ++exponent)
	{
		mantissa_type mantissas[]{
			static_cast<mantissa_type>(1u),
			static_cast<mantissa_type>(
				(mantissa_mask / static_cast<mantissa_type>(3u)) |
				static_cast<mantissa_type>(1u)),
			mantissa_mask};
		for (unsigned variant{}; variant != 3u; ++variant)
		{
			::fast_io::details::punning_result<floating_type> fields{
				mantissas[variant], exponent, variant == 2u};
			auto const value{::fast_io::details::
								 compiler_constant_floating_value_from_fields<floating_type>(fields)};
			auto const minimum{static_cast<::std::size_t>(
				(exponent + variant) & 7u)};
			auto const normalized_minimum{minimum ? minimum : 1u};
			auto const maximum{normalized_minimum +
							   static_cast<::std::size_t>((exponent >> 3u) & 31u)};
			if (!check_value(value, minimum, maximum))
			{
				::fast_io::io::perr("precision_range precise-size mismatch: exponent=",
									exponent, ",variant=", variant, ",minimum=", minimum,
									",maximum=", maximum, "\n");
				return false;
			}
		}
	}
	return true;
}

} // namespace

int main()
{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	if (!check_every_finite_exponent<__bf16>())
	{
		return 1;
	}
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	if (!check_every_finite_exponent<_Float16>())
	{
		return 2;
	}
#endif
	if (!check_every_finite_exponent<float>())
	{
		return 3;
	}
	if (!check_every_finite_exponent<double>())
	{
		return 4;
	}
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	if (!check_every_finite_exponent<long double>())
	{
		return 5;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (!check_every_finite_exponent<__float128>())
	{
		return 6;
	}
#endif
}
