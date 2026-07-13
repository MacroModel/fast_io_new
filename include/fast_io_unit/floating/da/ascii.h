#pragma once

namespace fast_io::details::da
{

struct ascii_digit_block
{
	::std::uint_least64_t low;
	::std::uint_least64_t high;
	::std::uint_least32_t span;
};

inline constexpr ::std::uint_least64_t ascii_zeroes{static_cast<::std::uint_least64_t>(0x3030303030303030)};
inline constexpr ::std::uint_least64_t ascii_div10000_multiplier{static_cast<::std::uint_least64_t>(109951163)};
inline constexpr ::std::uint_least64_t ascii_div100_multiplier{static_cast<::std::uint_least64_t>(5243)};
inline constexpr ::std::uint_least64_t ascii_div10_multiplier{static_cast<::std::uint_least64_t>(103)};

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least64_t
ascii_bcd8(::std::uint_least64_t value) noexcept
{
	auto const four_digit_pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
											((value * ascii_div10000_multiplier) >> 40u)};
	auto const two_digit_pairs{four_digit_pairs + static_cast<::std::uint_least64_t>(65436) *
													  (((four_digit_pairs * ascii_div100_multiplier) >> 19u) & static_cast<::std::uint_least64_t>(0x7f0000007f))};
	auto const digits{two_digit_pairs + static_cast<::std::uint_least64_t>(246) *
											(((two_digit_pairs * ascii_div10_multiplier) >> 10u) & static_cast<::std::uint_least64_t>(0xf000f000f000f))};
	return ::fast_io::byte_swap(digits);
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least32_t
ascii_bcd8_span(::std::uint_least64_t bcd) noexcept
{
	if (!bcd)
	{
		return 0u;
	}
	return static_cast<::std::uint_least32_t>(
		8u - static_cast<::std::uint_least32_t>(::std::countl_zero(bcd) >> 3u));
}

#if (defined(__aarch64__) || defined(__arm64__)) && (!defined(_MSC_VER) || defined(__clang__))
using ascii_i8x16 [[gnu::vector_size(16)]] = signed char;
using ascii_u8x16 [[gnu::vector_size(16)]] = unsigned char;
using ascii_u8x8 [[gnu::vector_size(8)]] = unsigned char;
using ascii_i16x8 [[gnu::vector_size(16)]] = short;
using ascii_u16x8 [[gnu::vector_size(16)]] = unsigned short;
using ascii_u16x4 [[gnu::vector_size(8)]] = unsigned short;
using ascii_i32x2 [[gnu::vector_size(8)]] = int;
using ascii_i32x4 [[gnu::vector_size(16)]] = int;
using ascii_u64x2 [[gnu::vector_size(16)]] = unsigned long long;

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i32x4
ascii_qdmulh(ascii_i32x4 value, int multiplier) noexcept
{
#if defined(__clang__)
	return __builtin_bit_cast(ascii_i32x4,
							  __builtin_neon_vqdmulhq_v(__builtin_bit_cast(ascii_i8x16, value),
														__builtin_bit_cast(ascii_i8x16, ascii_i32x4{multiplier, multiplier, multiplier, multiplier}), 34));
#else
	return __builtin_aarch64_sqdmulh_nv4si(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i32x2
ascii_qdmulh(ascii_i32x2 value, int multiplier) noexcept
{
#if defined(__clang__)
	using i8x8 [[gnu::vector_size(8)]] = signed char;
	return __builtin_bit_cast(ascii_i32x2,
							  __builtin_neon_vqdmulh_v(__builtin_bit_cast(i8x8, value),
													   __builtin_bit_cast(i8x8, ascii_i32x2{multiplier, multiplier}), 2));
#else
	return __builtin_aarch64_sqdmulh_nv2si(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i16x8
ascii_qdmulh(ascii_i16x8 value, short multiplier) noexcept
{
#if defined(__clang__)
	return __builtin_bit_cast(ascii_i16x8,
							  __builtin_neon_vqdmulhq_v(__builtin_bit_cast(ascii_i8x16, value),
														__builtin_bit_cast(ascii_i8x16,
																		   ascii_i16x8{multiplier, multiplier, multiplier, multiplier,
																					   multiplier, multiplier, multiplier, multiplier}),
														33));
#else
	return __builtin_aarch64_sqdmulh_nv8hi(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_u8x16
ascii_bcd4x4(ascii_i32x4 value) noexcept
{
#if defined(__GNUC__)
	__asm__("" : "+w"(value));
#endif
	auto const hundreds{::fast_io::details::da::ascii_qdmulh(value, 21475328)};
	auto const pairs{__builtin_bit_cast(ascii_i16x8,
										value + hundreds * ascii_i32x4{65436, 65436, 65436, 65436})};
	auto const tens{::fast_io::details::da::ascii_qdmulh(pairs, static_cast<short>(3296))};
	return __builtin_bit_cast(ascii_u8x16,
							  pairs + tens * ascii_i16x8{246, 246, 246, 246, 246, 246, 246, 246});
}

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_digit_block
make_ascii_digit_block_simd(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
									 ((value * ascii_div10000_multiplier) >> 40u)};
		auto const unshuffled{::fast_io::details::da::ascii_bcd4x4(ascii_i32x4{
			static_cast<int>(static_cast<::std::uint_least32_t>(pairs)),
			static_cast<int>(static_cast<::std::uint_least32_t>(pairs >> 32u)), 0, 0})};
		auto const raw{__builtin_bit_cast(ascii_u64x2, unshuffled)[0]};
		auto const bcd{::fast_io::byte_swap(raw)};
		auto const span{raw ? static_cast<::std::uint_least32_t>(
								  8u - (static_cast<::std::uint_least32_t>(::std::countr_zero(raw)) >> 3u))
							: 0u};
		return {bcd + ascii_zeroes, 0u, span};
	}
	else
	{
#if defined(__SIZEOF_INT128__)
		auto const high_value{static_cast<::std::uint_least64_t>(
			(static_cast<__uint128_t>(value) * static_cast<::std::uint_least64_t>(0xabcc77118461cefd)) >> 90u)};
#else
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
#endif
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const combined{ascii_i32x2{
			static_cast<int>(static_cast<::std::uint_least32_t>(high_value)),
			static_cast<int>(static_cast<::std::uint_least32_t>(low_value))}};
		auto const high_limbs{::fast_io::details::da::ascii_qdmulh(
								  combined, static_cast<int>(ascii_div10000_multiplier)) >>
							  9u};
		auto const packed_limbs{combined + high_limbs * ascii_i32x2{55536, 55536}};
		auto const limbs{__builtin_convertvector(
			__builtin_bit_cast(ascii_u16x4, packed_limbs), ascii_i32x4)};
		auto const unshuffled{::fast_io::details::da::ascii_bcd4x4(limbs)};
		auto const digits{__builtin_shufflevector(
			unshuffled, unshuffled, 7, 6, 5, 4, 3, 2, 1, 0,
			15, 14, 13, 12, 11, 10, 9, 8)};
		auto const nonzero_bytes{__builtin_bit_cast(ascii_i8x16, digits) > 0};
		auto const nonzero_words{__builtin_bit_cast(ascii_u16x8, nonzero_bytes) >> 4u};
		auto const nonzero_mask{__builtin_bit_cast(::std::uint_least64_t,
												   __builtin_convertvector(nonzero_words, ascii_u8x8))};
		auto const span{static_cast<::std::uint_least32_t>(
			16u - (static_cast<::std::uint_least32_t>(::std::countl_zero(nonzero_mask)) >> 2u))};
		auto const ascii_digits{digits + ascii_u8x16{
											 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
		auto const packed{__builtin_bit_cast(ascii_u64x2, ascii_digits)};
		return {packed[0], packed[1], span};
	}
}

#endif

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
using ascii_x86_i8x16 [[gnu::vector_size(16)]] = signed char;
using ascii_x86_c8x16 [[gnu::vector_size(16)]] = char;
using ascii_x86_u8x16 [[gnu::vector_size(16)]] = unsigned char;
using ascii_x86_i16x8 [[gnu::vector_size(16)]] = short;
using ascii_x86_u16x8 [[gnu::vector_size(16)]] = unsigned short;
using ascii_x86_u32x4 [[gnu::vector_size(16)]] = unsigned int;
using ascii_x86_i32x4 [[gnu::vector_size(16)]] = int;
using ascii_x86_u64x2 [[gnu::vector_size(16)]] = unsigned long long;

struct ascii_x86_digit_data
{
	ascii_digit_block digits;
	ascii_x86_u8x16 unshuffled;
};

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u16x8
ascii_x86_mul_high_u16(ascii_x86_u16x8 left, ascii_x86_u16x8 right) noexcept
{
	return __builtin_bit_cast(ascii_x86_u16x8,
							  __builtin_ia32_pmulhuw128(__builtin_bit_cast(ascii_x86_i16x8, left),
														__builtin_bit_cast(ascii_x86_i16x8, right)));
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u64x2
ascii_x86_mul_low_u32_to_u64(ascii_x86_u32x4 left, ascii_x86_u32x4 right) noexcept
{
	return __builtin_bit_cast(ascii_x86_u64x2,
							  __builtin_ia32_pmuludq128(__builtin_bit_cast(ascii_x86_i32x4, left),
														__builtin_bit_cast(ascii_x86_i32x4, right)));
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u8x16
ascii_x86_bcd4x4(ascii_x86_u32x4 value) noexcept
{
	auto const hundreds_words{::fast_io::details::da::ascii_x86_mul_high_u16(
		__builtin_bit_cast(ascii_x86_u16x8, value),
		ascii_x86_u16x8{5243, 0, 5243, 0, 5243, 0, 5243, 0})};
	auto const hundreds{__builtin_bit_cast(ascii_x86_u32x4, hundreds_words) >> 3u};
	auto const pairs{value + hundreds * ascii_x86_u32x4{65436u, 65436u, 65436u, 65436u}};
	auto const tens{::fast_io::details::da::ascii_x86_mul_high_u16(
		__builtin_bit_cast(ascii_x86_u16x8, pairs),
		ascii_x86_u16x8{6554, 6554, 6554, 6554, 6554, 6554, 6554, 6554})};
	return __builtin_bit_cast(ascii_x86_u8x16,
							  __builtin_bit_cast(ascii_x86_u16x8, pairs) +
								  tens * ascii_x86_u16x8{246, 246, 246, 246, 246, 246, 246, 246});
}

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_digit_data
make_ascii_digit_data_x86(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
									 ((value * ascii_div10000_multiplier) >> 40u)};
		auto const unshuffled{::fast_io::details::da::ascii_x86_bcd4x4(
			ascii_x86_u32x4{static_cast<unsigned int>(pairs),
							static_cast<unsigned int>(pairs >> 32u), 0u, 0u})};
		auto const raw{__builtin_bit_cast(ascii_x86_u64x2, unshuffled)[0]};
		auto const span{raw ? static_cast<::std::uint_least32_t>(
								  8u - (static_cast<::std::uint_least32_t>(::std::countr_zero(raw)) >> 3u))
							: 0u};
		return {{::fast_io::byte_swap(raw) + ascii_zeroes, 0u, span}, unshuffled};
	}
	else
	{
#if defined(__SIZEOF_INT128__)
		auto const high_value{static_cast<::std::uint_least64_t>(
			(static_cast<__uint128_t>(value) * static_cast<::std::uint_least64_t>(0xabcc77118461cefd)) >> 90u)};
#else
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
#endif
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const limbs{ascii_x86_u64x2{low_value, high_value}};
		auto const quotients{::fast_io::details::da::ascii_x86_mul_low_u32_to_u64(
								 __builtin_bit_cast(ascii_x86_u32x4, limbs),
								 ascii_x86_u32x4{static_cast<unsigned int>(ascii_div10000_multiplier), 0u,
												 static_cast<unsigned int>(ascii_div10000_multiplier), 0u}) >>
							 40u};
		auto const pairs{limbs + ::fast_io::details::da::ascii_x86_mul_low_u32_to_u64(
									 __builtin_bit_cast(ascii_x86_u32x4, quotients),
									 ascii_x86_u32x4{4294957296u, 0u, 4294957296u, 0u})};
		auto const unshuffled{::fast_io::details::da::ascii_x86_bcd4x4(
			__builtin_bit_cast(ascii_x86_u32x4, pairs))};
		auto const nonzero_mask{static_cast<::std::uint_least32_t>(
			__builtin_ia32_pmovmskb128(__builtin_bit_cast(ascii_x86_c8x16,
														  __builtin_bit_cast(ascii_x86_i8x16, unshuffled) > 0)))};
		auto const span{nonzero_mask ? static_cast<::std::uint_least32_t>(
										   16u - static_cast<::std::uint_least32_t>(::std::countr_zero(nonzero_mask)))
									 : 0u};
		auto const shuffled{__builtin_bit_cast(ascii_x86_u8x16,
											   __builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, unshuffled),
																		ascii_x86_c8x16{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}))};
		auto const ascii_digits{shuffled + ascii_x86_u8x16{
											   48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
		auto const packed{__builtin_bit_cast(ascii_x86_u64x2, ascii_digits)};
		return {{packed[0], packed[1], span}, unshuffled};
	}
}

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_digit_block
make_ascii_digit_block_x86(::std::uint_least64_t value) noexcept
{
	return ::fast_io::details::da::make_ascii_digit_data_x86<flt>(value).digits;
}
#endif

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ascii_digit_block
make_ascii_digit_block(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const low_bcd{::fast_io::details::da::ascii_bcd8(value)};
		return {low_bcd + ascii_zeroes, 0u,
				::fast_io::details::da::ascii_bcd8_span(low_bcd)};
	}
	else
	{
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const high_bcd{::fast_io::details::da::ascii_bcd8(high_value)};
		auto const low_bcd{::fast_io::details::da::ascii_bcd8(low_value)};
		auto const span{low_bcd ? 8u + ::fast_io::details::da::ascii_bcd8_span(low_bcd)
								: ::fast_io::details::da::ascii_bcd8_span(high_bcd)};
		return {high_bcd + ascii_zeroes, low_bcd + ascii_zeroes, span};
	}
}

struct ascii_exponent_cache
{
	inline static constexpr ::std::int_least32_t minimum{-324};
	inline static constexpr ::std::int_least32_t maximum{308};
	::std::uint_least64_t data[static_cast<::std::size_t>(maximum - minimum + 1)]{};

	consteval ascii_exponent_cache() noexcept
	{
		for (auto exponent{minimum}; exponent <= maximum; ++exponent)
		{
			auto magnitude{static_cast<::std::uint_least32_t>(exponent < 0 ? -exponent : exponent)};
			::std::uint_least64_t packed{static_cast<::std::uint_least64_t>(u8'e') |
										 (static_cast<::std::uint_least64_t>(exponent < 0 ? u8'-' : u8'+') << 8u)};
			::std::uint_least32_t length{4u};
			if (100u <= magnitude)
			{
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 100u) << 16u;
				magnitude %= 100u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 10u) << 24u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude % 10u) << 32u;
				length = 5u;
			}
			else
			{
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 10u) << 16u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude % 10u) << 24u;
			}
			data[static_cast<::std::size_t>(exponent - minimum)] =
				packed | (static_cast<::std::uint_least64_t>(length) << 56u);
		}
	}
};

#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_exponent_cache ascii_exponents{};

struct ascii_fixed_layout_cache
{
	inline static constexpr ::std::int_least32_t minimum{-4};
	inline static constexpr ::std::int_least32_t compact_maximum{6};
	inline static constexpr ::std::int_least32_t binary64_shuffle_maximum{15};
	inline static constexpr ::std::int_least32_t maximum{22};
	inline static constexpr ::std::size_t entry_alignment{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
		64u
#else
		32u
#endif
	};

	struct alignas(entry_alignment) entry
	{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
		::std::uint_least8_t binary64_shuffle[2][16]{};
		::std::uint_least8_t binary64_last_digit_position[2]{};
#endif
		::std::uint_least8_t start_position{};
		::std::uint_least8_t point_position{};
		::std::uint_least8_t shift_position{};
		::std::uint_least8_t end_position[17]{};
		::std::uint_least32_t decimal_fixed_mask{};
	};

	entry data[static_cast<::std::size_t>(maximum - minimum + 1)]{};

	consteval ascii_fixed_layout_cache() noexcept
	{
		for (auto exponent{minimum}; exponent <= maximum; ++exponent)
		{
			auto &layout{data[static_cast<::std::size_t>(exponent - minimum)]};
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
			for (::std::uint_least32_t extra{}; extra != 2u; ++extra)
			{
				auto source{static_cast<::std::uint_least8_t>(!extra)};
				auto const point_slot{0 <= exponent && exponent <= 14 ? exponent + 1 : 128};
				for (::std::uint_least32_t position{}; position != 16u; ++position)
				{
					layout.binary64_shuffle[extra][position] =
						static_cast<::std::uint_least8_t>(static_cast<::std::int_least32_t>(position) == point_slot
															  ? 0x80u
															  : source++);
				}
				auto const length{15u + extra};
				layout.binary64_last_digit_position[extra] = static_cast<::std::uint_least8_t>(
					length + static_cast<::std::uint_least32_t>(0 <= exponent && exponent < static_cast<::std::int_least32_t>(length)));
			}
#endif
			layout.start_position = static_cast<::std::uint_least8_t>(exponent < 0 ? 1 - exponent : 0);
			layout.point_position = static_cast<::std::uint_least8_t>(exponent < 0 ? 1 : exponent + 1);
			layout.shift_position = static_cast<::std::uint_least8_t>(
				layout.point_position + static_cast<::std::uint_least8_t>(0 <= exponent));
			for (::std::uint_least32_t length{1u}; length <= 17u; ++length)
			{
				auto end_position{static_cast<::std::int_least32_t>(length)};
				if (0 <= exponent)
				{
					end_position = static_cast<::std::int_least32_t>(length) > exponent + 1
									   ? static_cast<::std::int_least32_t>(length) + 1
									   : exponent + 1;
				}
				layout.end_position[length - 1u] = static_cast<::std::uint_least8_t>(end_position);
				::std::uint_least32_t fixed_length{};
				if (static_cast<::std::int_least32_t>(length) <= exponent)
				{
					fixed_length = static_cast<::std::uint_least32_t>(exponent + 1);
				}
				else if (0 <= exponent)
				{
					fixed_length = length + 2u - static_cast<::std::uint_least32_t>(static_cast<::std::int_least32_t>(length) == exponent + 1);
				}
				else
				{
					fixed_length = static_cast<::std::uint_least32_t>(-exponent) + length + 1u;
				}
				auto const scientific_length{length == 1u ? length + 3u : length + 5u};
				layout.decimal_fixed_mask |= static_cast<::std::uint_least32_t>(
												 fixed_length <= scientific_length)
											 << (length - 1u);
			}
		}
	}
};

#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_fixed_layout_cache ascii_fixed_layouts{};

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	defined(__GNUC__) && !defined(__clang__)
/// @brief Compile-time generated GCC x86 shuffle layouts for complete binary32 scientific output.
struct ascii_binary32_scientific_cache
{
	struct alignas(16) entry
	{
		::std::uint_least8_t shuffle[16]{};
	};

	entry data[32]{};

	consteval ascii_binary32_scientific_cache() noexcept
	{
		for (::std::uint_least32_t index{}; index != 32u; ++index)
		{
			auto &layout{data[index]};
			for (auto &position : layout.shuffle)
			{
				position = 0x80u;
			}
			auto const digit_span{index / 4u + 1u};
			auto const has_last_digit{static_cast<bool>((index >> 1u) & 1u)};
			auto const has_extra_digit{static_cast<bool>(index & 1u)};
			auto const leading_position{static_cast<::std::uint_least8_t>(has_extra_digit ? 7u : 6u)};
			::std::uint_least32_t length{};
			if (has_last_digit)
			{
				layout.shuffle[length++] = leading_position;
				layout.shuffle[length++] = 13u;
				for (auto position{static_cast<::std::int_least32_t>(leading_position) - 1};
					 0 <= position; --position)
				{
					layout.shuffle[length++] = static_cast<::std::uint_least8_t>(position);
				}
				layout.shuffle[length++] = 12u;
			}
			else
			{
				length = digit_span + static_cast<::std::uint_least32_t>(has_extra_digit);
				length -= static_cast<::std::uint_least32_t>(length == 2u);
				layout.shuffle[0] = leading_position;
				layout.shuffle[1] = 13u;
				for (::std::uint_least32_t position{2u}; position < length; ++position)
				{
					layout.shuffle[position] = static_cast<::std::uint_least8_t>(
						leading_position + 1u - position);
				}
			}
			for (::std::uint_least8_t exponent_position{8u}; exponent_position != 12u; ++exponent_position)
			{
				layout.shuffle[length++] = exponent_position;
			}
			layout.shuffle[15] = static_cast<::std::uint_least8_t>(length);
		}
	}
};

#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_binary32_scientific_cache ascii_binary32_scientific_layouts{};
#endif

template <typename flt>
FAST_IO_GNU_ALWAYS_INLINE inline void store_ascii_digits(
	char *destination, ascii_digit_block digits, bool drop_leading_zero) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const shifted{drop_leading_zero ? digits.low >> 8u : digits.low};
		__builtin_memcpy(destination, __builtin_addressof(shifted), sizeof(shifted));
	}
	else
	{
		auto low{digits.low};
		auto high{digits.high};
		if (drop_leading_zero)
		{
			low = (low >> 8u) | (high << 56u);
			high >>= 8u;
		}
		__builtin_memcpy(destination, __builtin_addressof(low), sizeof(low));
		__builtin_memcpy(destination + sizeof(low), __builtin_addressof(high), sizeof(high));
	}
}

template <bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_exponent(
	char *destination, ::std::int_least32_t exponent) noexcept
{
	auto packed{ascii_exponents.data[static_cast<::std::size_t>(exponent - ascii_exponent_cache::minimum)]};
	auto const length{static_cast<::std::uint_least32_t>(packed >> 56u)};
	if constexpr (uppercase_e)
	{
		packed ^= static_cast<::std::uint_least64_t>(u8'e' ^ u8'E');
	}
	__builtin_memcpy(destination, __builtin_addressof(packed), sizeof(packed));
	return destination + length;
}

template <typename flt, bool comma, bool json_float>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_fixed(
	char *destination, ascii_digit_block digits, ::std::uint_least32_t digit_count,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	__builtin_memcpy(destination, __builtin_addressof(ascii_zeroes), sizeof(ascii_zeroes));
	auto const &layout{ascii_fixed_layouts.data[static_cast<::std::size_t>(exponent - ascii_fixed_layout_cache::minimum)]};
	auto buffer{destination + layout.start_position};
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
	if constexpr (sizeof(flt) > sizeof(float))
	{
		auto const extra{static_cast<::std::uint_least32_t>(has_extra_digit)};
		auto const packed{ascii_x86_u64x2{digits.low, digits.high}};
		ascii_x86_c8x16 shuffle;
		__builtin_memcpy(__builtin_addressof(shuffle), layout.binary64_shuffle[extra], sizeof(shuffle));
		auto const assembled{__builtin_bit_cast(
			ascii_x86_u8x16,
			__builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, packed), shuffle))};
		__builtin_memcpy(buffer, __builtin_addressof(assembled), sizeof(assembled));
		buffer[16u] = static_cast<char>(digits.high >> 56u);
		destination[layout.point_position] = static_cast<char>(comma ? u8',' : u8'.');
		buffer[layout.binary64_last_digit_position[extra]] =
			static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
		auto end{buffer + layout.end_position[digit_count - 1u]};
		if constexpr (json_float)
		{
			if (0 <= exponent &&
				digit_count <= static_cast<::std::uint_least32_t>(exponent + 1))
			{
				*end++ = static_cast<char>(comma ? u8',' : u8'.');
				*end++ = '0';
			}
		}
		return end;
	}
#endif
	::fast_io::details::da::store_ascii_digits<flt>(buffer, digits, !has_extra_digit);
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	buffer[block_size + static_cast<::std::uint_least32_t>(has_extra_digit) - 1u] =
		static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
	__builtin_memmove(destination + layout.shift_position,
					  destination + layout.point_position, block_size);
	destination[layout.point_position] = static_cast<char>(comma ? u8',' : u8'.');
	auto end{buffer + layout.end_position[digit_count - 1u]};
	if constexpr (json_float)
	{
		if (0 <= exponent &&
			digit_count <= static_cast<::std::uint_least32_t>(exponent + 1))
		{
			*end++ = static_cast<char>(comma ? u8',' : u8'.');
			*end++ = '0';
		}
	}
	return end;
}

template <typename flt, bool comma, bool json_float>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_fixed_extended(
	char *destination, ascii_digit_block digits, ::std::uint_least32_t digit_count,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	::fast_io::details::da::store_ascii_digits<flt>(destination, digits, !has_extra_digit);
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	destination[block_size + static_cast<::std::uint_least32_t>(has_extra_digit) - 1u] =
		static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
	auto const point_position{static_cast<::std::uint_least32_t>(exponent + 1)};
	if (digit_count <= point_position)
	{
		// General notation admits at most six appended zeroes; decimal's length decision admits at most five.
		switch (point_position - digit_count)
		{
		case 6u:
			destination[digit_count + 5u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 5u:
			destination[digit_count + 4u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 4u:
			destination[digit_count + 3u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 3u:
			destination[digit_count + 2u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 2u:
			destination[digit_count + 1u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 1u:
			destination[digit_count] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 0u:
			break;
		}
		auto end{destination + point_position};
		if constexpr (json_float)
		{
			*end++ = static_cast<char>(comma ? u8',' : u8'.');
			*end++ = static_cast<char>(u8'0');
		}
		return end;
	}
	auto constexpr decimal_point{static_cast<char>(comma ? u8',' : u8'.')};
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		destination[point_position + 1u] = destination[point_position];
		destination[point_position] = decimal_point;
	}
	else
	{
		// Extended fixed notation starts at byte eight, so the low eight digits never move.
		auto trailing_digit{destination[16u]};
		if (point_position == 16u)
		{
			destination[17u] = trailing_digit;
			destination[16u] = decimal_point;
		}
		else
		{
			::std::uint_least64_t high_digits;
			__builtin_memcpy(__builtin_addressof(high_digits), destination + 8u, sizeof(high_digits));
			auto const last_high_digit{static_cast<char>(high_digits >> 56u)};
			auto const shift{static_cast<::std::uint_least32_t>((point_position - 8u) * 8u)};
			auto const lower_mask{(static_cast<::std::uint_least64_t>(1u) << shift) - 1u};
			high_digits = (high_digits & lower_mask) |
						  ((high_digits & ~lower_mask) << 8u) |
						  (static_cast<::std::uint_least64_t>(static_cast<unsigned char>(decimal_point)) << shift);
			__builtin_memcpy(destination + 8u, __builtin_addressof(high_digits), sizeof(high_digits));
			if (digit_count == 17u)
			{
				destination[17u] = trailing_digit;
			}
			if (16u <= digit_count)
			{
				destination[16u] = last_high_digit;
			}
		}
	}
	return destination + digit_count + 1u;
}

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	defined(__GNUC__) && !defined(__clang__)
template <bool comma, bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_scientific_x86_binary32(
	char *destination, ascii_x86_u8x16 unshuffled, ::std::uint_least32_t digit_span,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	auto exponent_data{
		ascii_exponents.data[static_cast<::std::size_t>(exponent - ascii_exponent_cache::minimum)]};
	if constexpr (uppercase_e)
	{
		exponent_data ^= static_cast<::std::uint_least64_t>(u8'e' ^ u8'E');
	}
	auto source{__builtin_bit_cast(ascii_x86_u64x2,
								   unshuffled + ascii_x86_u8x16{48, 48, 48, 48, 48, 48, 48, 48,
																48, 48, 48, 48, 48, 48, 48, 48})};
	source[1] = (exponent_data & static_cast<::std::uint_least64_t>(0xffffffffu)) |
				(static_cast<::std::uint_least64_t>(u8'0' + last_digit) << 32u) |
				(static_cast<::std::uint_least64_t>(comma ? u8',' : u8'.') << 40u);
	auto const index{(digit_span - 1u) * 4u +
					 static_cast<::std::uint_least32_t>(has_last_digit) * 2u +
					 static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const &layout{ascii_binary32_scientific_layouts.data[index]};
	ascii_x86_c8x16 shuffle;
	__builtin_memcpy(__builtin_addressof(shuffle), layout.shuffle, sizeof(shuffle));
	auto const assembled{__builtin_bit_cast(
		ascii_x86_u8x16,
		__builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, source), shuffle))};
	__builtin_memcpy(destination, __builtin_addressof(assembled), sizeof(assembled));
	return destination + layout.shuffle[15];
}
#endif

template <typename flt, bool comma, bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_scientific(
	char *destination, ascii_digit_block digits, ::std::uint_least32_t digit_count,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	auto buffer{destination + static_cast<::std::uint_least32_t>(has_extra_digit)};
	::fast_io::details::da::store_ascii_digits<flt>(buffer, digits, false);
	buffer[block_size] = static_cast<char>(u8'0' + last_digit);
	buffer += has_last_digit ? block_size + 1u : digits.span;
	destination[0] = destination[1];
	destination[1] = static_cast<char>(comma ? u8',' : u8'.');
	buffer -= static_cast<::std::uint_least32_t>(buffer == destination + 2u);
	return ::fast_io::details::da::print_ascii_exponent<uppercase_e>(buffer, exponent);
}

template <typename flt, ::fast_io::manipulators::scalar_flags flags>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_shortest(
	char *destination, conversion_result converted) noexcept
{
	constexpr bool binary32{sizeof(flt) <= sizeof(float)};
	constexpr ::std::int_least32_t fast_fixed_maximum{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
		binary32 ? ascii_fixed_layout_cache::compact_maximum : ascii_fixed_layout_cache::binary64_shuffle_maximum
#else
		ascii_fixed_layout_cache::compact_maximum
#endif
	};
	constexpr ::std::uint_least64_t extra_digit_threshold{
		binary32 ? static_cast<::std::uint_least64_t>(10000000) : static_cast<::std::uint_least64_t>(1000000000000000)};
	constexpr ::std::uint_least32_t block_size{binary32 ? 8u : 16u};
	if constexpr (binary32)
	{
		if (converted.significand < static_cast<::std::uint_least64_t>(1000000)) [[unlikely]]
		{
			converted.significand = converted.significand * 10u +
									(converted.has_last_digit ? converted.last_digit : 0u);
			converted.has_last_digit = false;
			--converted.exponent;
		}
	}
#if (defined(__aarch64__) || defined(__arm64__)) && (!defined(_MSC_VER) || defined(__clang__))
	auto const digits{::fast_io::details::da::make_ascii_digit_block_simd<flt>(converted.significand)};
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__))
#if defined(__GNUC__) && !defined(__clang__)
	auto const digit_data{::fast_io::details::da::make_ascii_digit_data_x86<flt>(converted.significand)};
	auto const digits{digit_data.digits};
#else
	auto const digits{::fast_io::details::da::make_ascii_digit_block_x86<flt>(converted.significand)};
#endif
#else
	auto const digits{::fast_io::details::da::make_ascii_digit_block<flt>(converted.significand)};
#endif
	auto const has_extra_digit{converted.significand >= extra_digit_threshold};
	auto const digit_count{converted.has_last_digit
							   ? block_size + static_cast<::std::uint_least32_t>(has_extra_digit)
							   : digits.span - 1u + static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const exponent{static_cast<::std::int_least32_t>(
		converted.exponent + (binary32 ? 7 : 15) + static_cast<::std::int_least32_t>(has_extra_digit))};
	bool use_fixed{};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
	{
		use_fixed = true;
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		auto const decimal_exponent{static_cast<::std::int_least32_t>(
			exponent - static_cast<::std::int_least32_t>(digit_count) + 1)};
		use_fixed = -5 < decimal_exponent && decimal_exponent < 7;
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::decimal)
	{
		if (ascii_fixed_layout_cache::minimum <= exponent &&
			exponent <= ascii_fixed_layout_cache::maximum)
		{
			auto const &layout{ascii_fixed_layouts.data[static_cast<::std::size_t>(exponent - ascii_fixed_layout_cache::minimum)]};
			use_fixed = static_cast<bool>(
				(layout.decimal_fixed_mask >> (digit_count - 1u)) & 1u);
		}
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		if (use_fixed)
		{
			if (exponent <= fast_fixed_maximum)
			{
				return ::fast_io::details::da::print_ascii_fixed<flt, flags.comma, flags.json_float>(
					destination, digits, digit_count, exponent, has_extra_digit,
					converted.last_digit, converted.has_last_digit);
			}
			return ::fast_io::details::da::print_ascii_fixed_extended<flt, flags.comma, flags.json_float>(
				destination, digits, digit_count, exponent, has_extra_digit,
				converted.last_digit, converted.has_last_digit);
		}
	}
	else if (use_fixed && ascii_fixed_layout_cache::minimum <= exponent &&
			 exponent <= fast_fixed_maximum)
	{
		return ::fast_io::details::da::print_ascii_fixed<flt, flags.comma, flags.json_float>(
			destination, digits, digit_count, exponent, has_extra_digit,
			converted.last_digit, converted.has_last_digit);
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::decimal)
	{
		if (use_fixed)
		{
			return ::fast_io::details::da::print_ascii_fixed_extended<flt, flags.comma, flags.json_float>(
				destination, digits, digit_count, exponent, has_extra_digit,
				converted.last_digit, converted.has_last_digit);
		}
	}
	if (use_fixed)
	{
		return nullptr;
	}
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	defined(__GNUC__) && !defined(__clang__)
	if constexpr (binary32)
	{
		return ::fast_io::details::da::print_ascii_scientific_x86_binary32<flags.comma, flags.uppercase_e>(
			destination, digit_data.unshuffled, digits.span, exponent, has_extra_digit,
			converted.last_digit, converted.has_last_digit);
	}
#endif
	return ::fast_io::details::da::print_ascii_scientific<flt, flags.comma, flags.uppercase_e>(
		destination, digits, digit_count, exponent, has_extra_digit,
		converted.last_digit, converted.has_last_digit);
}

} // namespace fast_io::details::da
