#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <fast_io.h>

namespace
{

enum class anchor_domain : unsigned char
{
	compact,
	constant,
	lazy
};

struct measurement
{
	::std::uint_least64_t nanoseconds{};
	::std::uint_least64_t values{};
	::std::uint_least64_t hash{};
};

struct segment_cold_measurement
{
	::std::uint_least64_t minimum{};
	::std::uint_least64_t median{};
	::std::uint_least64_t maximum{};
	::std::uint_least64_t hash{};
};

template <typename floating_type>
[[nodiscard]] constexpr anchor_domain classify_exponent(
	::std::uint_least32_t exponent) noexcept
{
	if constexpr (!::fast_io::details::exact_precision_is_wide_binary<
					  floating_type>)
	{
		return anchor_domain::compact;
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		constexpr ::std::int_least32_t bias{
			(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) -
			1};
		auto const binary_exponent{
			exponent
				? static_cast<::std::int_least32_t>(exponent) - bias -
					  static_cast<::std::int_least32_t>(trait::mbits)
				: 1 - bias - static_cast<::std::int_least32_t>(trait::mbits)};
		if (-binary_exponent >= static_cast<::std::int_least32_t>(
									::fast_io::details::exact_decimal_pow5_runtime_first_index *
									::fast_io::details::exact_decimal_pow5_anchor_stride))
		{
			return anchor_domain::lazy;
		}
		if (-binary_exponent >= static_cast<::std::int_least32_t>(
									::fast_io::details::exact_decimal_pow5_anchor_minimum_exponent))
		{
			return anchor_domain::constant;
		}
		return anchor_domain::compact;
	}
}

template <typename floating_type>
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
[[nodiscard]] ::std::uint_least64_t convert_and_hash(
	::std::uint_least32_t exponent, unsigned variant) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto mantissa_mask{
		(static_cast<mantissa_type>(1u) << trait::mbits) - 1u};
	mantissa_type mantissa{};
	switch (variant)
	{
	case 0u:
		mantissa = static_cast<mantissa_type>(1u);
		break;
	case 1u:
		mantissa = static_cast<mantissa_type>(
			(mantissa_mask / static_cast<mantissa_type>(3u)) |
			static_cast<mantissa_type>(1u));
		break;
	default:
		mantissa = mantissa_mask;
		break;
	}
	auto const decimal{
		::fast_io::details::exact_decimal_from_binary<floating_type>(
			mantissa, exponent)};
	::std::uint_least64_t hash{UINT64_C(1469598103934665603)};
	for (::std::size_t index{}; index != decimal.size; ++index)
	{
		hash = (hash ^ decimal.digits[index]) * UINT64_C(1099511628211);
	}
	return hash ^ static_cast<::std::uint_least64_t>(decimal.exponent);
}

template <typename floating_type>
[[nodiscard]] measurement measure_domain(anchor_domain domain,
										 ::std::size_t repetitions) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	measurement result{};
	auto const begin{::std::chrono::steady_clock::now()};
	for (::std::size_t repetition{}; repetition != repetitions; ++repetition)
	{
		for (::std::uint_least32_t exponent{}; exponent != exponent_mask;
			 ++exponent)
		{
			if (classify_exponent<floating_type>(exponent) != domain)
			{
				continue;
			}
			for (unsigned variant{}; variant != 3u; ++variant)
			{
				result.hash ^= convert_and_hash<floating_type>(exponent, variant);
				++result.values;
			}
		}
	}
	auto const end{::std::chrono::steady_clock::now()};
	result.nanoseconds = static_cast<::std::uint_least64_t>(
		::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin)
			.count());
	return result;
}

template <typename floating_type>
[[nodiscard]] constexpr ::std::uint_least32_t
raw_exponent_for_anchor(::std::size_t anchor_index) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	auto const raw{bias + static_cast<::std::int_least32_t>(trait::mbits) -
				   static_cast<::std::int_least32_t>(
					   anchor_index *
					   ::fast_io::details::exact_decimal_pow5_anchor_stride)};
	return static_cast<::std::uint_least32_t>(raw);
}

template <typename floating_type>
[[nodiscard]] measurement measure_constant_first() noexcept
{
	measurement result{};
	if constexpr (::fast_io::details::exact_precision_is_wide_binary<
					  floating_type>)
	{
		auto const exponent{raw_exponent_for_anchor<floating_type>(
			::fast_io::details::exact_decimal_pow5_anchor_first_index)};
		auto const begin{::std::chrono::steady_clock::now()};
		result.hash = convert_and_hash<floating_type>(exponent, 0u);
		auto const end{::std::chrono::steady_clock::now()};
		result.nanoseconds = static_cast<::std::uint_least64_t>(
			::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin)
				.count());
		result.values = 1u;
	}
	return result;
}

template <typename floating_type>
[[nodiscard]] segment_cold_measurement measure_lazy_segments_cold() noexcept
{
	segment_cold_measurement result{};
	if constexpr (::fast_io::details::exact_precision_is_wide_binary<
					  floating_type>)
	{
		constexpr auto segment_count{
			::fast_io::details::exact_decimal_pow5_runtime_segment_count};
		::std::uint_least64_t samples[segment_count]{};
		for (::std::size_t segment{}; segment != segment_count; ++segment)
		{
			auto const anchor_index{
				::fast_io::details::exact_decimal_pow5_runtime_first_index +
				segment *
					::fast_io::details::exact_decimal_pow5_runtime_segment_extent};
			auto const exponent{
				raw_exponent_for_anchor<floating_type>(anchor_index)};
			auto const begin{::std::chrono::steady_clock::now()};
			result.hash ^= convert_and_hash<floating_type>(exponent, 0u);
			auto const end{::std::chrono::steady_clock::now()};
			samples[segment] = static_cast<::std::uint_least64_t>(
				::std::chrono::duration_cast<::std::chrono::nanoseconds>(end - begin)
					.count());
		}
		for (::std::size_t index{1u}; index != segment_count; ++index)
		{
			auto const value{samples[index]};
			auto position{index};
			for (; position && value < samples[position - 1u]; --position)
			{
				samples[position] = samples[position - 1u];
			}
			samples[position] = value;
		}
		result.minimum = samples[0u];
		result.median = samples[segment_count / 2u];
		result.maximum = samples[segment_count - 1u];
	}
	return result;
}

inline void print_measurement(char const *type_name, char const *domain_name,
							  measurement value)
{
	if (!value.values)
	{
		return;
	}
	::fast_io::io::perr(::fast_io::mnp::os_c_str(type_name), ",",
						::fast_io::mnp::os_c_str(domain_name), ",values=", value.values,
						",ns_per_value=", value.nanoseconds / value.values, ",hash=",
						value.hash, "\n");
}

template <typename floating_type>
void benchmark_type(char const *name)
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_count{
		(static_cast<::std::size_t>(1u) << trait::ebits) - 1u};
	constexpr auto value_count{exponent_count * 3u};
	constexpr ::std::size_t target_warm_values{262144u};
	constexpr ::std::size_t repetitions{
		value_count < target_warm_values
			? (target_warm_values + value_count - 1u) / value_count
			: 1u};

	auto const constant_first{measure_constant_first<floating_type>()};
	if (constant_first.values)
	{
		print_measurement(name, "constant-first", constant_first);
		auto const segments{measure_lazy_segments_cold<floating_type>()};
		::fast_io::io::perr(::fast_io::mnp::os_c_str(name),
							",lazy-segments-cold,count=",
							::fast_io::details::exact_decimal_pow5_runtime_segment_count,
							",min_ns=", segments.minimum, ",median_ns=", segments.median,
							",max_ns=", segments.maximum, ",hash=", segments.hash, "\n");
	}
	print_measurement(name, "constant",
					  measure_domain<floating_type>(anchor_domain::constant, repetitions));
	print_measurement(name, "compact",
					  measure_domain<floating_type>(anchor_domain::compact, repetitions));
	print_measurement(name, "lazy-warm",
					  measure_domain<floating_type>(anchor_domain::lazy, repetitions));
}

[[nodiscard]] bool selected(char const *argument, char const *name) noexcept
{
	return ::std::string_view{argument} == ::std::string_view{name};
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		::fast_io::io::perr("usage: exact_decimal_all_exponents_bench ",
							"bf16|f16|f32|f64|f80|f128\n");
		return 1;
	}
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	if (selected(argv[1], "bf16"))
	{
		benchmark_type<__bf16>("bf16");
		return 0;
	}
#endif
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	if (selected(argv[1], "f16"))
	{
		benchmark_type<_Float16>("f16");
		return 0;
	}
#endif
	if (selected(argv[1], "f32"))
	{
		benchmark_type<float>("f32");
		return 0;
	}
	if (selected(argv[1], "f64"))
	{
		benchmark_type<double>("f64");
		return 0;
	}
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	if (selected(argv[1], "f80"))
	{
		benchmark_type<long double>("f80");
		return 0;
	}
#endif
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	if (selected(argv[1], "f128"))
	{
		benchmark_type<__float128>("f128");
		return 0;
	}
#endif
	::fast_io::io::perr("unsupported format: ", ::fast_io::mnp::os_c_str(argv[1]),
						"\n");
	return 2;
}
