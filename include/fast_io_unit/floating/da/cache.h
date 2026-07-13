#pragma once

namespace fast_io::details::da
{

struct uint64x2
{
	::std::uint_least64_t hi;
	::std::uint_least64_t lo;
};

[[nodiscard]] inline constexpr uint64x2 umul64x64(::std::uint_least64_t x,
												  ::std::uint_least64_t y) noexcept
{
	::std::uint_least64_t hi;
	auto const lo{::fast_io::intrinsics::umul(x, y, hi)};
	return {hi, lo};
}

[[nodiscard]] inline constexpr ::std::uint_least64_t umul64x64_add_high(
	::std::uint_least64_t x, ::std::uint_least64_t y, ::std::uint_least64_t addend) noexcept
{
#if defined(__SIZEOF_INT128__)
	auto const product{static_cast<__uint128_t>(x) * y + addend};
	return static_cast<::std::uint_least64_t>(product >> 64u);
#else
	auto const product{::fast_io::details::da::umul64x64(x, y)};
	auto const lo{static_cast<::std::uint_least64_t>(product.lo + addend)};
	return static_cast<::std::uint_least64_t>(product.hi + (lo < product.lo));
#endif
}

[[nodiscard]] inline constexpr uint64x2 umul64x128_high(::std::uint_least64_t x,
														uint64x2 y) noexcept
{
#if defined(__SIZEOF_INT128__)
	auto const upper{static_cast<__uint128_t>(x) * y.hi};
	auto const upper_low{static_cast<::std::uint_least64_t>(upper)};
	auto const lo{static_cast<::std::uint_least64_t>(upper_low + ::fast_io::intrinsics::umulh(x, y.lo))};
	return {static_cast<::std::uint_least64_t>((upper >> 64u) + (lo < upper_low)), lo};
#else
	auto const upper{::fast_io::details::da::umul64x64(x, y.hi)};
	auto const lower_high{::fast_io::intrinsics::umulh(x, y.lo)};
	auto const lo{static_cast<::std::uint_least64_t>(upper.lo + lower_high)};
	return {static_cast<::std::uint_least64_t>(upper.hi + (lo < upper.lo)), lo};
#endif
}

inline constexpr ::std::uint_least64_t power10_minor[]{
	static_cast<::std::uint_least64_t>(0x8000000000000000), static_cast<::std::uint_least64_t>(0xa000000000000000), static_cast<::std::uint_least64_t>(0xc800000000000000),
	static_cast<::std::uint_least64_t>(0xfa00000000000000), static_cast<::std::uint_least64_t>(0x9c40000000000000), static_cast<::std::uint_least64_t>(0xc350000000000000),
	static_cast<::std::uint_least64_t>(0xf424000000000000), static_cast<::std::uint_least64_t>(0x9896800000000000), static_cast<::std::uint_least64_t>(0xbebc200000000000),
	static_cast<::std::uint_least64_t>(0xee6b280000000000), static_cast<::std::uint_least64_t>(0x9502f90000000000), static_cast<::std::uint_least64_t>(0xba43b74000000000),
	static_cast<::std::uint_least64_t>(0xe8d4a51000000000), static_cast<::std::uint_least64_t>(0x9184e72a00000000), static_cast<::std::uint_least64_t>(0xb5e620f480000000),
	static_cast<::std::uint_least64_t>(0xe35fa931a0000000), static_cast<::std::uint_least64_t>(0x8e1bc9bf04000000), static_cast<::std::uint_least64_t>(0xb1a2bc2ec5000000),
	static_cast<::std::uint_least64_t>(0xde0b6b3a76400000), static_cast<::std::uint_least64_t>(0x8ac7230489e80000), static_cast<::std::uint_least64_t>(0xad78ebc5ac620000),
	static_cast<::std::uint_least64_t>(0xd8d726b7177a8000), static_cast<::std::uint_least64_t>(0x878678326eac9000), static_cast<::std::uint_least64_t>(0xa968163f0a57b400),
	static_cast<::std::uint_least64_t>(0xd3c21bcecceda100), static_cast<::std::uint_least64_t>(0x84595161401484a0), static_cast<::std::uint_least64_t>(0xa56fa5b99019a5c8),
	static_cast<::std::uint_least64_t>(0xcecb8f27f4200f3a)};

inline constexpr uint64x2 power10_major[]{
	{static_cast<::std::uint_least64_t>(0xaf8e5410288e1b6f), static_cast<::std::uint_least64_t>(0x07ecf0ae5ee44dda)},
	{static_cast<::std::uint_least64_t>(0xb1442798f49ffb4a), static_cast<::std::uint_least64_t>(0x99cd11cfdf41779d)},
	{static_cast<::std::uint_least64_t>(0xb2fe3f0b8599ef07), static_cast<::std::uint_least64_t>(0x861fa7e6dcb4aa15)},
	{static_cast<::std::uint_least64_t>(0xb4bca50b065abe63), static_cast<::std::uint_least64_t>(0x0fed077a756b53aa)},
	{static_cast<::std::uint_least64_t>(0xb67f6455292cbf08), static_cast<::std::uint_least64_t>(0x1a3bc84c17b1d543)},
	{static_cast<::std::uint_least64_t>(0xb84687c269ef3bfb), static_cast<::std::uint_least64_t>(0x3d5d514f40eea742)},
	{static_cast<::std::uint_least64_t>(0xba121a4650e4ddeb), static_cast<::std::uint_least64_t>(0x92f34d62616ce413)},
	{static_cast<::std::uint_least64_t>(0xbbe226efb628afea), static_cast<::std::uint_least64_t>(0x890489f70a55368c)},
	{static_cast<::std::uint_least64_t>(0xbdb6b8e905cb600f), static_cast<::std::uint_least64_t>(0x5400e987bbc1c921)},
	{static_cast<::std::uint_least64_t>(0xbf8fdb78849a5f96), static_cast<::std::uint_least64_t>(0xde98520472bdd034)},
	{static_cast<::std::uint_least64_t>(0xc16d9a0095928a27), static_cast<::std::uint_least64_t>(0x75b7053c0f178294)},
	{static_cast<::std::uint_least64_t>(0xc350000000000000), static_cast<::std::uint_least64_t>(0x0000000000000000)},
	{static_cast<::std::uint_least64_t>(0xc5371912364ce305), static_cast<::std::uint_least64_t>(0x6c28000000000000)},
	{static_cast<::std::uint_least64_t>(0xc722f0ef9d80aad6), static_cast<::std::uint_least64_t>(0x424d3ad2b7b97ef6)},
	{static_cast<::std::uint_least64_t>(0xc913936dd571c84c), static_cast<::std::uint_least64_t>(0x03bc3a19cd1e38ea)},
	{static_cast<::std::uint_least64_t>(0xcb090c8001ab551c), static_cast<::std::uint_least64_t>(0x5cadf5bfd3072cc6)},
	{static_cast<::std::uint_least64_t>(0xcd036837130890a1), static_cast<::std::uint_least64_t>(0x36dba887c37a8c10)},
	{static_cast<::std::uint_least64_t>(0xcf02b2c21207ef2e), static_cast<::std::uint_least64_t>(0x94f967e45e03f4bc)},
	{static_cast<::std::uint_least64_t>(0xd106f86e69d785c7), static_cast<::std::uint_least64_t>(0xe13336d701beba52)},
	{static_cast<::std::uint_least64_t>(0xd31045a8341ca07c), static_cast<::std::uint_least64_t>(0x1ede48111209a051)},
	{static_cast<::std::uint_least64_t>(0xd51ea6fa85785631), static_cast<::std::uint_least64_t>(0x552a74227f3ea566)},
	{static_cast<::std::uint_least64_t>(0xd732290fbacaf133), static_cast<::std::uint_least64_t>(0xa97c177947ad4096)},
	{static_cast<::std::uint_least64_t>(0xd94ad8b1c7380874), static_cast<::std::uint_least64_t>(0x18375281ae7822bc)}};

inline constexpr ::std::uint_least32_t power10_fixups[]{
	static_cast<::std::uint_least32_t>(0x0a4e363f), static_cast<::std::uint_least32_t>(0x00001840), static_cast<::std::uint_least32_t>(0x00006400), static_cast<::std::uint_least32_t>(0x24200040),
	static_cast<::std::uint_least32_t>(0x00000000), static_cast<::std::uint_least32_t>(0x0c000000), static_cast<::std::uint_least32_t>(0x82c81380), static_cast<::std::uint_least32_t>(0x5e4ce01f),
	static_cast<::std::uint_least32_t>(0xd730f60f), static_cast<::std::uint_least32_t>(0x0000001b), static_cast<::std::uint_least32_t>(0x00000000), static_cast<::std::uint_least32_t>(0xcdf7fffc),
	static_cast<::std::uint_least32_t>(0x6e8201d8), static_cast<::std::uint_least32_t>(0x40cd3fd1), static_cast<::std::uint_least32_t>(0xdb642501), static_cast<::std::uint_least32_t>(0x00000d0d),
	static_cast<::std::uint_least32_t>(0x14042400), static_cast<::std::uint_least32_t>(0x53713840), static_cast<::std::uint_least32_t>(0x11781db4), static_cast<::std::uint_least32_t>(0x00000000)};

struct power10_cache
{
	inline static constexpr ::std::size_t size{618u};
	inline static constexpr ::std::int_least32_t minimum_exponent{-293};
	alignas(64)::std::uint_least64_t data[size * 2u]{};

	[[nodiscard]] inline static constexpr uint64x2 compute(::std::size_t i) noexcept
	{
		constexpr ::std::size_t stride{sizeof(power10_minor) / sizeof(*power10_minor)};
		auto const minor{power10_minor[(i + 10u) % stride]};
		auto const major{power10_major[(i + 10u) / stride]};
		auto const h1{::fast_io::intrinsics::umulh(major.lo, minor)};
		auto const c0{static_cast<::std::uint_least64_t>(major.lo * minor)};
		auto const major_product{::fast_io::details::da::umul64x64(major.hi, minor)};
		auto const c1{static_cast<::std::uint_least64_t>(h1 + major_product.lo)};
		auto const c2{static_cast<::std::uint_least64_t>(major_product.hi + (c1 < h1))};
		uint64x2 result;
		if (c2 >> 63u)
		{
			result = {c2, c1};
		}
		else
		{
			result = {static_cast<::std::uint_least64_t>((c2 << 1u) | (c1 >> 63u)),
					  static_cast<::std::uint_least64_t>((c1 << 1u) | (c0 >> 63u))};
		}
		result.lo -= (power10_fixups[i >> 5u] >> (i & 31u)) & 1u;
		return result;
	}

	inline constexpr power10_cache() noexcept
	{
		for (::std::size_t i{}; i != size; ++i)
		{
			auto const value{compute(i)};
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
			data[size - i - 1u] = value.hi;
			data[size * 2u - i - 1u] = value.lo;
#else
			data[i * 2u] = value.hi;
			data[i * 2u + 1u] = value.lo;
#endif
		}
	}

	[[nodiscard]] inline constexpr uint64x2 operator[](::std::int_least32_t exponent) const noexcept
	{
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
		auto const base{data + size + minimum_exponent};
		auto const index{static_cast<::std::ptrdiff_t>(~exponent)};
		return {base[index], base[index + static_cast<::std::ptrdiff_t>(size)]};
#else
		auto const index{static_cast<::std::size_t>(exponent - minimum_exponent) * 2u};
		return {data[index], data[index + 1u]};
#endif
	}
};

[[nodiscard]] inline constexpr ::std::int_least32_t compute_decimal_exponent(
	::std::int_least32_t binary_exponent, bool regular = true) noexcept
{
	return (binary_exponent * 315653 - static_cast<::std::int_least32_t>(!regular) * 131072) >> 20;
}

[[nodiscard]] inline constexpr ::std::int_least32_t compute_decimal_exponent_binary32(
	::std::int_least32_t binary_exponent) noexcept
{
	return (binary_exponent * 78913) >> 18;
}

[[nodiscard]] inline constexpr ::std::uint_least8_t compute_exponent_shift(
	::std::int_least32_t binary_exponent, ::std::int_least32_t decimal_exponent) noexcept
{
	auto const power10_binary_exponent{(-decimal_exponent * 217707) >> 16};
	return static_cast<::std::uint_least8_t>(binary_exponent + power10_binary_exponent + 1);
}

inline constexpr bool verify_binary32_decimal_exponents() noexcept
{
	for (::std::int_least32_t exponent{-149}; exponent <= 104; ++exponent)
	{
		if (compute_decimal_exponent_binary32(exponent) != compute_decimal_exponent(exponent))
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_binary32_decimal_exponents());

struct exponent_shift_cache
{
	inline static constexpr ::std::size_t size{2048u};
	inline static constexpr ::std::int_least32_t binary64_exponent_offset{1075};
	inline static constexpr ::std::uint_least8_t extra_shift{6u};
	::std::uint_least8_t data[size]{};

	inline constexpr exponent_shift_cache() noexcept
	{
		for (::std::size_t raw_exponent{}; raw_exponent != size; ++raw_exponent)
		{
			auto binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - binary64_exponent_offset};
			if (raw_exponent == 0u)
			{
				++binary_exponent;
			}
			auto const decimal_exponent{compute_decimal_exponent(binary_exponent)};
			data[raw_exponent] = static_cast<::std::uint_least8_t>(
				compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift);
		}
	}
};

struct cache
{
	exponent_shift_cache exponent_shifts;
	alignas(64) power10_cache powers;
};

#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
alignas(64) inline constexpr cache cached_data{};

} // namespace fast_io::details::da
