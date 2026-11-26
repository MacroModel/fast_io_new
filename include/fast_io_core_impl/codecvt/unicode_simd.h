#pragma once

// The validating block/mask and shuffle organisation follows Muła and Lemire (simdutf).
// Layouts are generated from Unicode sequence widths. Scalar recovery owns the error cursor.
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && \
	defined(__SIZEOF_INT__) && __SIZEOF_INT__ == 4 && __CHAR_BIT__ == 8 &&                                              \
	(defined(__SSSE3__) || defined(__ARM_NEON) || defined(__wasm_simd128__) || defined(__ALTIVEC__) ||                  \
	 defined(__loongarch_sx) || defined(__riscv_vector) || (defined(__s390x__) && defined(__VEC__))) &&                 \
	FAST_IO_HAS_BUILTIN(__builtin_shufflevector) && FAST_IO_HAS_BUILTIN(__builtin_convertvector) && FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
#define FAST_IO_UNICODE_BLOCKS
namespace fast_io::details::codecvt
{
// Extended vectors keep elementwise comparisons on Clang/POWER across AltiVec compatibility modes.
#if defined(__clang__) && defined(__ALTIVEC__)
#define FAST_IO_UTF_LANES(bytes, type) __attribute__((ext_vector_type((bytes) / sizeof(type))))
#else
#define FAST_IO_UTF_LANES(bytes, type) [[gnu::vector_size(bytes)]]
#endif
/** @brief Names fixed-lane vectors after the lane count is substituted, including on older GCC front ends. */
template <unsigned N>
struct utf_vector_types
{
	using bytes FAST_IO_UTF_LANES(N, unsigned char) = unsigned char;
	using halves FAST_IO_UTF_LANES(N * 2, unsigned short) = unsigned short;
	using words FAST_IO_UTF_LANES(N * 4, unsigned) = unsigned;
};
using utf_b16 FAST_IO_UTF_LANES(16, unsigned char) = unsigned char;
using utf_h4 FAST_IO_UTF_LANES(8, unsigned short) = unsigned short;
using utf_h8 FAST_IO_UTF_LANES(16, unsigned short) = unsigned short;
using utf_w4 FAST_IO_UTF_LANES(16, unsigned) = unsigned;
using utf_b4 FAST_IO_UTF_LANES(4, unsigned char) = unsigned char;

#if defined(__riscv_vector) && defined(__clang__) && FAST_IO_HAS_BUILTIN(__builtin_rvv_vrgather_vv)
#define FAST_IO_UTF_RVV_GATHER
// Clang exposes these overloaded builtins through typed aliases. VL=16 works for every VLEN >= 128.
using utf_rvv_bytes = __rvv_uint8m1_t;
utf_rvv_bytes utf_rvv_load(unsigned char const *, ::std::size_t) __attribute__((__clang_builtin_alias__(__builtin_rvv_vle8_v)));
void utf_rvv_store(unsigned char *, utf_rvv_bytes, ::std::size_t) __attribute__((__clang_builtin_alias__(__builtin_rvv_vse8_v)));
utf_rvv_bytes utf_rvv_gather(utf_rvv_bytes, utf_rvv_bytes, ::std::size_t) __attribute__((__clang_builtin_alias__(__builtin_rvv_vrgather_vv)));
#endif

/** @brief Reads any sufficiently large, possibly unaligned object representation into a vector. */
template <typename V, typename T>
inline V utf_vector_load(T p) noexcept
{
	V value;
	__builtin_memcpy(&value, unicode_address(p), sizeof(V));
	return value;
}

/** @brief Converts byte comparison lanes to a bitset in increasing memory order. */
inline unsigned utf_mask(utf_b16 v) noexcept
{
#if defined(__SSE2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
	using bytes FAST_IO_UTF_LANES(16, char) = char;
	return static_cast<unsigned>(__builtin_ia32_pmovmskb128(__builtin_bit_cast(bytes, v)));
#elif defined(__loongarch_sx) && FAST_IO_HAS_BUILTIN(__builtin_lsx_vmskltz_b)
	using bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	return __builtin_bit_cast(utf_w4, __builtin_lsx_vmskltz_b(__builtin_bit_cast(bytes, v)))[0];
#elif defined(__wasm_simd128__) && FAST_IO_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
	using bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	return static_cast<unsigned>(__builtin_wasm_bitmask_i8x16(__builtin_bit_cast(bytes, v)));
#else
	unsigned bits{};
	for (unsigned i{}; i != 16u; ++i)
	{
		bits |= static_cast<unsigned>(v[i] >> 7u) << i;
	}
	return bits;
#endif
}

/** @brief Selects bytes within one vector; an index outside [0,16) supplies zero. */
inline utf_b16 utf_shuffle(utf_b16 data, utf_b16 index) noexcept
{
#if defined(__SSSE3__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb128)
	using bytes FAST_IO_UTF_LANES(16, char) = char;
	return __builtin_bit_cast(utf_b16, __builtin_ia32_pshufb128(__builtin_bit_cast(bytes, data), __builtin_bit_cast(bytes, index)));
#elif defined(__aarch64__) && FAST_IO_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return __builtin_aarch64_qtbl1v16qi_uuu(data, index);
#elif defined(__aarch64__) && FAST_IO_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)
	using bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	if constexpr (::std::endian::native == ::std::endian::big)
	{
		// The hardware table is numbered from the low register byte. C++ lanes follow memory order.
		auto ix{__builtin_shufflevector(index, index, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)};
		auto r{__builtin_bit_cast(utf_b16, __builtin_neon_vqtbl1q_v(__builtin_bit_cast(bytes, data), __builtin_bit_cast(bytes, ix), 48))};
		return __builtin_shufflevector(r, r, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
	}
	else
	{
		return __builtin_bit_cast(utf_b16, __builtin_neon_vqtbl1q_v(__builtin_bit_cast(bytes, data), __builtin_bit_cast(bytes, index), 48));
	}
#elif defined(__ARM_NEON) && FAST_IO_HAS_BUILTIN(__builtin_neon_vtbl2_v) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	using half FAST_IO_UTF_LANES(8, signed char) = signed char;
	auto lo{__builtin_bit_cast(half, __builtin_shufflevector(data, data, 0, 1, 2, 3, 4, 5, 6, 7))};
	auto hi{__builtin_bit_cast(half, __builtin_shufflevector(data, data, 8, 9, 10, 11, 12, 13, 14, 15))};
	auto a{__builtin_neon_vtbl2_v(lo, hi, __builtin_bit_cast(half, __builtin_shufflevector(index, index, 0, 1, 2, 3, 4, 5, 6, 7)), 16)};
	auto b{__builtin_neon_vtbl2_v(lo, hi, __builtin_bit_cast(half, __builtin_shufflevector(index, index, 8, 9, 10, 11, 12, 13, 14, 15)), 16)};
	return __builtin_bit_cast(utf_b16, __builtin_shufflevector(a, b, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
#elif defined(__loongarch_sx) && FAST_IO_HAS_BUILTIN(__builtin_lsx_vshuf_b)
	using bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	return __builtin_bit_cast(utf_b16, __builtin_lsx_vshuf_b(bytes{}, __builtin_bit_cast(bytes, data), __builtin_bit_cast(bytes, index)));
#elif defined(__ALTIVEC__) && defined(__clang__) && FAST_IO_HAS_BUILTIN(__builtin_altivec_vperm_4si)
	using words FAST_IO_UTF_LANES(16, int) = int;
	if constexpr (::std::endian::native == ::std::endian::little)
	{
		return __builtin_bit_cast(utf_b16, __builtin_altivec_vperm_4si(words{}, __builtin_bit_cast(words, data), ~index));
	}
	else
	{
		return __builtin_bit_cast(utf_b16, __builtin_altivec_vperm_4si(__builtin_bit_cast(words, data), words{}, index));
	}
#elif defined(__s390x__) && FAST_IO_HAS_BUILTIN(__builtin_s390_vperm)
	return __builtin_s390_vperm(data, utf_b16{}, index);
#elif defined(__wasm_simd128__) && FAST_IO_HAS_BUILTIN(__builtin_wasm_swizzle_i8x16)
	using bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	return __builtin_bit_cast(utf_b16, __builtin_wasm_swizzle_i8x16(__builtin_bit_cast(bytes, data), __builtin_bit_cast(bytes, index)));
#elif defined(FAST_IO_UTF_RVV_GATHER)
	utf_b16 result;
	auto indices{utf_rvv_load(reinterpret_cast<unsigned char const *>(&index), 16)};
	auto source{utf_rvv_load(reinterpret_cast<unsigned char const *>(&data), 16)};
	utf_rvv_store(reinterpret_cast<unsigned char *>(&result), utf_rvv_gather(source, indices, 16), 16);
	return result & (index < 16);
#elif defined(__GNUC__) && !defined(__clang__)
	// Every generated layout uses indices 0..15 or 255. The upper vector therefore supplies zeros.
	return __builtin_shuffle(data, utf_b16{}, index);
#else
	utf_b16 v{};
	for (unsigned i{}; i != 16; ++i)
	{
		if (index[i] < 16)
		{
			v[i] = data[index[i]];
		}
	}
	return v;
#endif
}

/** @brief Stores the two shuffle directions and the scalar payload mask for a four-code-point layout. */
struct utf_byte_layout
{
	unsigned char expand[16]{}, pack[16]{};
	unsigned payload[4]{};
	unsigned char count{};
};

/**
 * @brief Generates all 4⁴ UTF-8 length layouts directly from their four two-bit widths.
 * @details Defer constant evaluation until a concrete converter needs the tables. All code unit types
 * select the same specialization, so the linker retains one copy across directions and translation units.
 */
template <bool Enabled>
inline constexpr auto utf_byte_layouts = [] {
	::fast_io::freestanding::array<utf_byte_layout, 256> table{};
	for (unsigned layout{}; layout != 256; ++layout)
	{
		auto &entry{table[layout]};
		for (auto &x : entry.expand)
		{
			x = 255;
		}
		for (auto &x : entry.pack)
		{
			x = 255;
		}
		for (unsigned i{}; i != 4; ++i)
		{
			unsigned n{1 + ((layout >> (2 * i)) & 3u)};
			entry.payload[i] = n == 1 ? 0x7fu : n == 2 ? 0x1f3fu
											: n == 3   ? 0x0f3f3fu
													   : 0x073f3f3fu;
			for (unsigned j{}; j != n; ++j)
			{
				entry.expand[4 * i + j] = static_cast<unsigned char>(entry.count + n - 1 - j);
				entry.pack[entry.count + j] = static_cast<unsigned char>(4 * i + n - 1 - j);
			}
			entry.count += static_cast<unsigned char>(n);
		}
	}
	return table;
}();

/**
 * @brief Stores contiguous 16-byte expansion shuffles for groups of four UTF-8 scalars.
 * @details Separate lengths allow indexing each shuffle with a single shift.
 */
template <bool Enabled>
inline constexpr auto utf_four_group = [] {
	struct tables
	{
		unsigned char expand[256][16]{};
		unsigned char count[256]{};
	};
	tables result{};
	for (unsigned row{}; row != 256; ++row)
	{
		auto const &layout{utf_byte_layouts<Enabled>[row]};
		for (unsigned i{}; i != 16; ++i)
		{
			result.expand[row][i] = layout.expand[i];
		}
		result.count[row] = layout.count;
	}
	return result;
}();

/**
 * @brief Maps four UTF-8 scalar boundaries to a shuffle row.
 * @details RVV uses an eleven-bit mask and stores the row plus one. A zero entry
 * requests the wider lookup. Row 255 consumes sixteen bytes, outside that mask.
 * Other vector targets use a fourteen-bit mask and zero also represents four ASCII bytes.
 */
template <bool Enabled>
inline constexpr auto utf_four_boundary = [] {
#if defined(FAST_IO_UTF_RVV_GATHER)
	constexpr unsigned boundary_bits{11}, row_offset{1};
#else
	constexpr unsigned boundary_bits{14}, row_offset{};
#endif
	::fast_io::freestanding::array<unsigned char, 1u << boundary_bits> rows{};
	for (unsigned row{}; row != 256; ++row)
	{
		unsigned at{}, mask{};
		for (unsigned i{}; i != 4; ++i)
		{
			at += 1u + ((row >> (2 * i)) & 3u);
			mask |= 1u << (at - 1u);
		}
		if (at > boundary_bits)
		{
			continue;
		}
		for (unsigned m{mask}; m < rows.size(); m += 1u << at)
		{
			rows[m] = static_cast<unsigned char>(row + row_offset);
		}
	}
	return rows;
}();

/** @brief Encodes two consecutive character boundaries in an eight-bit end-of-character mask. */
template <bool Enabled>
inline constexpr auto utf_pair_layout = [] {
	struct pair
	{
		unsigned char bits{}, count{};
	};
	::fast_io::freestanding::array<pair, 256> table{};
	for (unsigned m{1}; m != 256; ++m)
	{
		unsigned a{1}, b{1};
		while (a <= 4 && !(m & (1u << (a - 1))))
		{
			++a;
		}
		if (a > 4)
		{
			continue;
		}
		while (b <= 4 && !(m & (1u << (a + b - 1))))
		{
			++b;
		}
		if (b <= 4)
		{
			table[m] = {static_cast<unsigned char>((a - 1) | ((b - 1) << 2)), static_cast<unsigned char>(a + b)};
		}
	}
	return table;
}();

/** @brief Resolves four validated UTF-8 scalar lengths from their end-of-character bits. */
template <bool Enabled>
inline unsigned utf_four_shuffle_row(unsigned long long ends) noexcept
{
#if defined(FAST_IO_UTF_RVV_GATHER)
	auto row{static_cast<unsigned>(utf_four_boundary<Enabled>[ends & 2047u])};
	if (row == 0)
	{
		auto const first{utf_pair_layout<Enabled>[ends & 255u]};
		auto const second{utf_pair_layout<Enabled>[(ends >> first.count) & 255u]};
		row = 1u + (static_cast<unsigned>(first.bits) | (static_cast<unsigned>(second.bits) << 4u));
	}
	return row - 1u;
#else
	auto low_ends{static_cast<unsigned>(ends) & 0x3fffu};
	auto row{static_cast<unsigned>(utf_four_boundary<Enabled>[low_ends])};
	if (!row && (low_ends & 15u) != 15u)
	{
		auto first{utf_pair_layout<Enabled>[ends & 255u]};
		auto second{utf_pair_layout<Enabled>[(ends >> first.count) & 255u]};
		row = static_cast<unsigned>(first.bits) | (static_cast<unsigned>(second.bits) << 4u);
	}
	return row;
#endif
}

/** @brief Expands four UTF-16 scalars to pairs of lanes, or compacts four prepared pairs for output. */
template <bool Enabled>
inline constexpr auto utf_half_layouts = [] {
	struct entry
	{
		unsigned char expand[16]{}, pack[16]{}, units{};
	};
	::fast_io::freestanding::array<entry, 16> table{};
	for (unsigned k{}; k != 16; ++k)
	{
		auto &p{table[k]};
		for (auto &x : p.expand)
		{
			x = 255;
		}
		for (auto &x : p.pack)
		{
			x = 255;
		}
		for (unsigned i{}; i != 4; ++i)
		{
			unsigned n{1u + ((k >> i) & 1u)};
			for (unsigned j{}; j != n * 2; ++j)
			{
				p.expand[4 * i + j] = static_cast<unsigned char>(2 * p.units + j);
				p.pack[2 * p.units + j] = static_cast<unsigned char>(4 * i + j);
			}
			p.units += static_cast<unsigned char>(n);
		}
	}
	return table;
}();

/** @brief Swaps bytes inside each word only when wire and host order differ. */
template <encoding_scheme Order, typename V>
inline V utf_swap_units(V v) noexcept
{
	if constexpr (Order == encoding_scheme::utf)
	{
		return v;
	}
	else if constexpr (sizeof(v[0]) == 2)
	{
		return (v << 8u) | (v >> 8u);
	}
	else
	{
		return (v << 24u) | ((v << 8u) & 0xff0000u) | ((v >> 8u) & 0xff00u) | (v >> 24u);
	}
}

/** @brief Interprets each four shuffled bytes with the least significant byte in the first memory lane. */
inline utf_w4 utf_word_values(utf_b16 bytes) noexcept
{
	return utf_swap_units<encoding_scheme::utf_le>(__builtin_bit_cast(utf_w4, bytes));
}

/** @brief Exposes little-endian byte lanes for packing independently of the CPU's register representation. */
inline utf_b16 utf_word_bytes(utf_w4 lanes) noexcept
{
	return __builtin_bit_cast(utf_b16, utf_swap_units<encoding_scheme::utf_le>(lanes));
}

/** @brief Reconstructs four code points from shuffled UTF-8 payload bytes. */
inline utf_w4 utf_unpack_four(utf_w4 packed) noexcept
{
	// A three-byte lead contributes four payload bits. Its E prefix clears the spurious upper bit.
	return (packed & 0x7fu) | ((packed >> 2u) & 0xfc0u) | ((packed >> 4u) & 0x1f000u) |
		(((packed >> 4u) ^ (packed >> 5u)) & 0x20000u) | ((packed >> 6u) & 0x1c0000u);
}

/** @brief Decodes one already validated four-scalar group selected by its UTF-8 widths. */
template <bool Enabled>
inline utf_w4 utf_decode_group(utf_b16 bytes, unsigned row) noexcept
{
	auto const indices{utf_vector_load<utf_b16>(utf_four_group<Enabled>.expand[row])};
	return utf_unpack_four(utf_word_values(utf_shuffle(bytes, indices)));
}

/** @brief Tests the low bit of each canonical 32-bit SIMD comparison result. */
template <typename V>
inline unsigned utf_lane_bits(V v) noexcept
{
#if defined(__SSE__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_movmskps)
	using floats FAST_IO_UTF_LANES(16, float) = float;
	return static_cast<unsigned>(__builtin_ia32_movmskps(__builtin_bit_cast(floats, v)));
#else
	return (v[0] & 1u) | ((v[1] & 1u) << 1u) | ((v[2] & 1u) << 2u) | ((v[3] & 1u) << 3u);
#endif
}

// Lookup4 factors the adjacent-byte restrictions into three nibble predicates. Bit 7 means that
// two consecutive continuations must be justified by a three- or four-byte lead in an earlier lane.
// Adapted from simdutf, Copyright 2021 The simdutf authors (MIT).
inline constexpr utf_b16 utf_lookup_lead_high{2, 2, 2, 2, 2, 2, 2, 2, 128, 128, 128, 128, 33, 1, 21, 73};
inline constexpr utf_b16 utf_lookup_lead_low{231, 163, 131, 131, 139, 203, 203, 203, 203, 203, 203, 203, 203, 219, 203, 203};
inline constexpr utf_b16 utf_lookup_next_high{1, 1, 1, 1, 1, 1, 1, 1, 230, 174, 186, 186, 1, 1, 1, 1};

/** @brief Validates a complete UTF-8 prefix using the preceding lead bytes and restricted second-byte ranges. */
inline utf_b16 utf_lookup_errors(utf_b16 prev, utf_b16 x) noexcept
{
	auto p1{__builtin_shufflevector(prev, x, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30)};
	auto p2{__builtin_shufflevector(prev, x, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29)};
	auto p3{__builtin_shufflevector(prev, x, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28)};
	auto adjacent{utf_shuffle(utf_lookup_lead_high, p1 >> 4u) & utf_shuffle(utf_lookup_lead_low, p1 & 15u) &
		utf_shuffle(utf_lookup_next_high, x >> 4u)};
	auto two_or_three{__builtin_bit_cast(utf_b16, (p2 >= 0xe0u) | (p3 >= 0xf0u)) & 128u};
	return adjacent ^ two_or_three;
}

#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb256) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb256) && FAST_IO_HAS_BUILTIN(__builtin_ia32_palignr256)
#define FAST_IO_UTF_WIDE_LOOKUP
using utf_b32 FAST_IO_UTF_LANES(32, unsigned char) = unsigned char;
using utf_char32 FAST_IO_UTF_LANES(32, char) = char;

/** @brief Performs two independent sixteen-byte table shuffles using one AVX2 instruction. */
inline utf_b32 utf_shuffle(utf_b32 values, utf_b32 indices) noexcept
{
	return __builtin_bit_cast(utf_b32,
		__builtin_ia32_pshufb256(__builtin_bit_cast(utf_char32, values), __builtin_bit_cast(utf_char32, indices)));
}

/** @brief Broadcasts one lookup table to both independent AVX2 byte-shuffle lanes. */
inline utf_b32 utf_broadcast_lookup(utf_b16 table) noexcept
{
	return __builtin_shufflevector(table, table, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
}

/** @brief Concatenates matching 128-bit halves and selects the preceding bytes in each half. */
template <unsigned Shift>
inline utf_b32 utf_half_context(utf_b32 previous, utf_b32 current) noexcept
{
#if defined(__clang__)
	return __builtin_bit_cast(utf_b32, __builtin_ia32_palignr256(
		__builtin_bit_cast(utf_char32, current), __builtin_bit_cast(utf_char32, previous), 16 - Shift));
#else
	using words FAST_IO_UTF_LANES(32, long long) = long long;
	return __builtin_bit_cast(utf_b32, __builtin_ia32_palignr256(
		__builtin_bit_cast(words, current), __builtin_bit_cast(words, previous), 8 * (16 - Shift)));
#endif
}

/** @brief Runs Lookup4 across two adjacent AVX2 halves, including the boundary from the previous vector. */
inline utf_b32 utf_lookup_errors(utf_b32 previous, utf_b32 current) noexcept
{
	auto left{__builtin_shufflevector(previous, current, 16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47)};
	auto p1{utf_half_context<1>(left, current)};
	auto p2{utf_half_context<2>(left, current)};
	auto p3{utf_half_context<3>(left, current)};
	auto adjacent{utf_shuffle(utf_broadcast_lookup(utf_lookup_lead_high), p1 >> 4u) &
		utf_shuffle(utf_broadcast_lookup(utf_lookup_lead_low), p1 & 15u) &
		utf_shuffle(utf_broadcast_lookup(utf_lookup_next_high), current >> 4u)};
	return adjacent ^ (__builtin_bit_cast(utf_b32, (p2 >= 0xe0u) | (p3 >= 0xf0u)) & 128u);
}

/** @brief Extracts all thirty-two character-start bits with one signed-byte comparison. */
inline unsigned utf_start_mask(utf_b32 data) noexcept
{
	using signed_bytes FAST_IO_UTF_LANES(32, signed char) = signed char;
	auto signed_data{__builtin_bit_cast(signed_bytes, data)};
	return static_cast<unsigned>(__builtin_ia32_pmovmskb256(__builtin_bit_cast(utf_char32, signed_data >= -64)));
}
#endif

/** @brief Checks a short complete prefix beginning at a known scalar boundary. */
inline bool utf_valid_prefix(utf_b16 x, unsigned length) noexcept
{
	auto invalid{utf_lookup_errors(utf_b16{}, x) != 0};
	return (utf_mask(__builtin_bit_cast(utf_b16, invalid)) & ((1u << length) - 1u)) == 0;
}

/** @brief Normalizes four complete UTF scalars into lanes; malformed bytes remain at the scalar recovery cursor. */
template <encoding_scheme Order, typename Input>
inline unsigned utf_decode_four(Input p, utf_w4 &values) noexcept
{
	using T [[maybe_unused]] = unicode_cursor_value_t<Input>;
	if constexpr (sizeof(T) == 4)
	{
		values = utf_swap_units<Order>(utf_vector_load<utf_w4>(p));
		return utf_lane_bits((values > 0x10ffffu) | ((values >= 0xd800u) & (values < 0xe000u))) ? 0 : 4;
	}
	else if constexpr (sizeof(T) == 2)
	{
		auto h{utf_swap_units<Order>(utf_vector_load<utf_h8>(p))};
		auto first{__builtin_shufflevector(h, h, 0, 1, 2, 3)};
		values = __builtin_convertvector(first, utf_w4);
		if (!utf_lane_bits((values >= 0xd800u) & (values < 0xe000u)))
		{
			return 4;
		}
		// Four adjacent high/low pairs admit a single fixed permutation and no dependent index lookup.
		auto halves{__builtin_bit_cast(utf_b16, utf_swap_units<encoding_scheme::utf_le>(h))};
		auto packed{utf_word_values(halves)};
		if (!utf_lane_bits((packed & 0xfc00fc00u) != 0xdc00d800u))
		{
			values = 0x10000u + ((packed & 1023u) << 10u) + ((packed >> 16u) & 1023u);
			return 8;
		}
		// The first four scalar widths select one of sixteen gather layouts. Validate both halves after gathering.
		unsigned consumed{}, mask{};
		for (unsigned i{}; i != 4; ++i)
		{
			unsigned unit{h[consumed++]};
			if (unit >= 0xd800u && unit <= 0xdbffu)
			{
				mask |= 1u << i;
				++consumed;
			}
		}
		auto const &layout{utf_half_layouts<(sizeof(T) != 0)>[mask]};
		auto pair{utf_word_values(utf_shuffle(halves, utf_vector_load<utf_b16>(layout.expand)))};
		auto a{pair & 65535u}, b{pair >> 16u};
		auto high{__builtin_bit_cast(utf_w4, (a >= 0xd800u) & (a < 0xdc00u))};
		if (utf_lane_bits(((a >= 0xdc00u) & (a < 0xe000u)) | (high & ((b < 0xdc00u) | (b > 0xdfffu)))))
		{
			return 0;
		}
		values = (high & (0x10000u + ((a - 0xd800u) << 10u) + b - 0xdc00u)) | (~high & a);
		return consumed;
	}
	else
	{
		auto data{utf_vector_load<utf_b16>(p)};
		auto ends{((~utf_mask(__builtin_bit_cast(utf_b16, (data & 0xc0) == 0x80))) >> 1u) | 0x8000u};
		auto a{utf_pair_layout<(sizeof(T) != 0)>[ends & 255u]};
		auto b{utf_pair_layout<(sizeof(T) != 0)>[(ends >> a.count) & 255u]};
		if (!a.count || !b.count)
		{
			return 0;
		}
		auto const &layout{utf_byte_layouts<(sizeof(T) != 0)>[static_cast<::std::size_t>(a.bits | (b.bits << 4u))]};
		// The next lane can be outside the vector. Check the last lead independently of its successor.
		unsigned final_width{1u + (b.bits >> 2u)};
		unsigned lead{data[layout.count - final_width]};
		unsigned expected{1u + (lead >= 0xc0u) + (lead >= 0xe0u) + (lead >= 0xf0u)};
		if (final_width != expected || !utf_valid_prefix(data, layout.count))
		{
			return 0;
		}
		auto packed{utf_word_values(utf_shuffle(data, utf_vector_load<utf_b16>(layout.expand))) &
					utf_vector_load<utf_w4>(layout.payload)};
		values = (packed & 0x7fu) | ((packed >> 2u) & 0xfc0u) |
				 ((packed >> 4u) & 0x3f000u) | ((packed >> 6u) & 0x1c0000u);
		return layout.count;
	}
}

/**
 * @brief Emits four validated scalars and reports the number of units produced.
 * @details The ordinary path stores only the logical result. A caller may select @p FullStore for UTF-8 or UTF-16
 * when the next sixteen output bytes are writable and part of the final result. Later groups overwrite any unused
 * bytes of the store before the conversion exposes a cursor.
 */
template <encoding_scheme Order, bool FullStore = false, bool FixedUtf16Store = false, typename Output>
inline unsigned utf_encode_four(utf_w4 cp, Output out, ::std::size_t room) noexcept
{
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	static_assert(!FullStore || sizeof(T) <= 2);
	static_assert(!FixedUtf16Store || (!FullStore && sizeof(T) == 2));
	if constexpr (sizeof(T) == 4)
	{
		if (room < 4u)
		{
			return 0;
		}
		auto v{utf_swap_units<Order>(cp)};
		__builtin_memcpy(unicode_address(out), &v, 16);
		return 4;
	}
	else if constexpr (sizeof(T) == 2)
	{
		auto large{__builtin_bit_cast(utf_w4, cp > 0xffffu)};
		unsigned m{utf_lane_bits(large)};
		if (!m)
		{
			if (room < 4u)
			{
				return 0;
			}
			auto h{utf_swap_units<Order>(__builtin_convertvector(cp, utf_h4))};
			__builtin_memcpy(unicode_address(out), &h, 8);
			return 4;
		}
		auto const &layout{utf_half_layouts<(sizeof(T) != 0)>[m]};
		if (room < layout.units)
		{
			return 0;
		}
		auto x{cp - 0x10000u};
		auto pairs{(large & (0xd800u | (x >> 10u) | (0xdc00u << 16u) | ((x & 1023u) << 16u))) | (~large & cp)};
		auto bytes{utf_word_bytes(pairs)};
		if constexpr (Order == encoding_scheme::utf_be)
		{
			bytes = __builtin_shufflevector(bytes, bytes, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
		}
		auto v{utf_shuffle(bytes, utf_vector_load<utf_b16>(layout.pack))};
		if constexpr (FullStore)
		{
			__builtin_memcpy(unicode_address(out), &v, sizeof(v));
		}
		else if constexpr (FixedUtf16Store)
		{
			// Assign an exact store width for each nonempty supplementary mask.
			switch (layout.units)
			{
			case 5:
				__builtin_memcpy(unicode_address(out), &v, 10);
				break;
			case 6:
				__builtin_memcpy(unicode_address(out), &v, 12);
				break;
			case 7:
				__builtin_memcpy(unicode_address(out), &v, 14);
				break;
			default:
				__builtin_memcpy(unicode_address(out), &v, 16);
				break;
			}
		}
		else
		{
			__builtin_memcpy(unicode_address(out), &v, 2u * layout.units);
		}
		return layout.units;
	}
	else
	{
		auto two{__builtin_bit_cast(utf_w4, cp >= 0x80u)};
		auto three{__builtin_bit_cast(utf_w4, cp >= 0x800u)};
		auto four{__builtin_bit_cast(utf_w4, cp >= 0x10000u)};
		if (!utf_lane_bits(two))
		{
			if (room < 4u)
			{
				return 0;
			}
			auto bytes{__builtin_convertvector(cp, utf_b4)};
			__builtin_memcpy(unicode_address(out), &bytes, 4);
			return 4;
		}
#if defined(__BMI2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pdep_si)
		auto low{utf_lane_bits((two ^ three) | four)};
		auto high{utf_lane_bits(three)};
		unsigned mask{__builtin_ia32_pdep_si(low, 0x55u) | __builtin_ia32_pdep_si(high, 0xaau)};
#else
		auto widths{-(two + three + four)};
		unsigned mask{widths[0] | (widths[1] << 2u) | (widths[2] << 4u) | (widths[3] << 6u)};
#endif
		auto const &layout{utf_byte_layouts<(sizeof(T) != 0)>[mask]};
		if (room < layout.count)
		{
			return 0;
		}
		auto b0{((cp & 63u) | 0x80u) & two};
		auto b1{((cp >> 6u) & 63u) | ((three & 0x80u) | (~three & 0xc0u))};
		auto b2{((cp >> 12u) & 63u) | ((four & 0x80u) | (~four & 0xe0u))};
		auto bytes{(~two & cp) | b0 | ((two & b1) << 8u) | ((three & b2) << 16u) | ((four & (0xf0u | (cp >> 18u))) << 24u)};
		auto packed{utf_shuffle(utf_word_bytes(bytes), utf_vector_load<utf_b16>(layout.pack))};
		if constexpr (FullStore)
		{
			__builtin_memcpy(unicode_address(out), &packed, sizeof(packed));
		}
		else
		{
			__builtin_memcpy(unicode_address(out), &packed, layout.count);
		}
		return layout.count;
	}
}

#if defined(__AVX512BW__)
inline constexpr unsigned utf_register_bytes{64};
#elif defined(__AVX2__) || defined(__loongarch_asx)
inline constexpr unsigned utf_register_bytes{32};
#elif defined(__riscv_v_fixed_vlen) && __riscv_v_fixed_vlen >= 256
inline constexpr unsigned utf_register_bytes{32};
#else
inline constexpr unsigned utf_register_bytes{16};
#endif

/** @brief Tests whether any comparison lane is set, without requiring scalar division or byte extraction. */
template <typename V>
inline bool utf_any(V const &v) noexcept
{
#if defined(__SSE4_1__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_ptestz128)
	if constexpr (sizeof(V) == 16)
	{
		using bits FAST_IO_UTF_LANES(16, long long) = long long;
		auto raw{__builtin_bit_cast(bits, v)};
		return __builtin_ia32_ptestz128(raw, raw) == 0;
	}
#endif
#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_ptestz256)
	if constexpr (sizeof(V) == 32)
	{
		using bits FAST_IO_UTF_LANES(32, long long) = long long;
		auto raw{__builtin_bit_cast(bits, v)};
		return __builtin_ia32_ptestz256(raw, raw) == 0;
	}
#endif
#if FAST_IO_HAS_BUILTIN(__builtin_reduce_or)
	return __builtin_reduce_or(v) != 0;
#else
	auto words{__builtin_bit_cast(::fast_io::freestanding::array<::std::uint_least64_t, sizeof(V) / 8>, v)};
	::std::uint_least64_t bits{};
	for (auto word : words)
	{
		bits |= word;
	}
	return bits != 0;
#endif
}

/** @brief Changes the object byte order of each numeric vector lane, without changing lane order. */
template <encoding_scheme Order, typename V>
inline void utf_order_lanes(V &v) noexcept
{
	if constexpr (Order != encoding_scheme::utf && sizeof(v[0]) == 2)
	{
		v = (v << 8u) | (v >> 8u);
	}
	else if constexpr (Order != encoding_scheme::utf && sizeof(v[0]) == 4)
	{
		v = (v << 24u) | ((v << 8u) & 0xff0000u) | ((v >> 8u) & 0xff00u) | (v >> 24u);
	}
}

/** @brief Identifies lanes that cannot be copied as a single destination code unit. */
template <typename F, typename T, typename V>
inline auto utf_direct_errors(V units) noexcept
{
	if constexpr (sizeof(F) == 1 || sizeof(T) == 1)
	{
		using lane = ::std::remove_reference_t<decltype(units[0])>;
		return units & static_cast<lane>(~0x7fu);
	}
	else
	{
		using lane = ::std::remove_reference_t<decltype(units[0])>;
		auto surrogates{__builtin_bit_cast(V, (units & static_cast<lane>(~0x7ffu)) == 0xd800u)};
		if constexpr (sizeof(F) == 4 && sizeof(T) == 2)
		{
			return surrogates | (units & 0xffff0000u);
		}
		else if constexpr (sizeof(F) == 4)
		{
			return surrogates | __builtin_bit_cast(V, units > 0x10ffffu);
		}
		else
		{
			return surrogates;
		}
	}
}

#if defined(__SSE4_1__) && !defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
/** @brief Widens an ASCII run whose first 32 UTF-8 bytes have already been validated. */
template <encoding_scheme B, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void utf_ascii_to_utf32_run(Input &first, Input stop, Output &out) noexcept
{
	auto cursor{first};
	auto dest{out};
	do
	{
		// SSE4.1 can widen each four-byte group directly from memory.
		for (unsigned i{}; i != 32u; i += 4u)
		{
			auto bytes{utf_vector_load<utf_b4>(cursor + i)};
			auto words{utf_swap_units<B>(__builtin_convertvector(bytes, utf_w4))};
			__builtin_memcpy(unicode_address(dest + i), &words, sizeof(words));
		}
		cursor += 32;
		dest += 32;
	}
	while (stop - cursor >= 32 &&
		   !utf_mask(utf_vector_load<utf_b16>(cursor) | utf_vector_load<utf_b16>(cursor + 16)));
	first = cursor;
	out = dest;
}
#endif

#if defined(__ARM_FEATURE_SVE) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && \
	__has_attribute(__clang_arm_builtin_alias) && FAST_IO_HAS_BUILTIN(__builtin_sve_svst1b_u16)
// Clang exposes these overloaded instructions through typed builtin aliases.
__SVBool_t utf_sve_active_bytes() __attribute__((__clang_arm_builtin_alias(__builtin_sve_svptrue_b8)));
__UINT64_TYPE__ utf_sve_half_lanes() __attribute__((__clang_arm_builtin_alias(__builtin_sve_svcnth)));
__SVUint8_t utf_sve_load(__SVBool_t, unsigned char const *) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svld1_u8)));
__SVUint16_t utf_sve_halves(__SVUint8_t) __attribute__((__clang_arm_builtin_alias(__builtin_sve_reinterpret_u16_u8)));
__SVUint16_t utf_sve_reverse(__SVBool_t, __SVUint16_t) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svrevb_u16_x)));
__SVUint16_t utf_sve_or(__SVBool_t, __SVUint16_t, __SVUint16_t) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svorr_u16_x)));
__SVBool_t utf_sve_above(__SVBool_t, __SVUint16_t, unsigned short) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svcmpgt_n_u16)));
bool utf_sve_any(__SVBool_t, __SVBool_t) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svptest_any)));
void utf_sve_store_bytes(__SVBool_t, unsigned char *, __SVUint16_t) __attribute__((__clang_arm_builtin_alias(__builtin_sve_svst1b_u16)));

/**
 * @brief Narrows pairs of scalable vectors containing only UTF-16 ASCII code units.
 * @details Both loads use byte pointers so a byte-stream cursor may have arbitrary alignment.
 * The destination has one byte per active halfword. A failed range check leaves both vectors
 * for the existing vector and scalar tails, including a surrogate crossing their boundary.
 */
template <encoding_scheme A, typename Input, typename Output>
inline void utf_sve_ascii_run(Input &cursor, Input stop, Output &destination) noexcept
{
	auto const lanes{static_cast<::std::ptrdiff_t>(utf_sve_half_lanes())};
	auto const active{utf_sve_active_bytes()};
	while (stop - cursor >= 2 * lanes)
	{
		auto first{utf_sve_halves(utf_sve_load(active, reinterpret_cast<unsigned char const *>(unicode_address(cursor))))};
		auto second{utf_sve_halves(utf_sve_load(active, reinterpret_cast<unsigned char const *>(unicode_address(cursor + lanes))))};
		if constexpr (A == encoding_scheme::utf_be)
		{
			first = utf_sve_reverse(active, first);
			second = utf_sve_reverse(active, second);
		}
		if (utf_sve_any(active, utf_sve_above(active, utf_sve_or(active, first, second), 127u)))
		{
			break;
		}
		utf_sve_store_bytes(active, reinterpret_cast<unsigned char *>(unicode_address(destination)), first);
		utf_sve_store_bytes(active, reinterpret_cast<unsigned char *>(unicode_address(destination + lanes)), second);
		cursor += 2 * lanes;
		destination += 2 * lanes;
	}
}
#define FAST_IO_UTF_SVE_ASCII
#endif

/**
 * @brief Transcodes consecutive one-to-one scalars from pairs of source vectors.
 * @details Every pair shares a range check. The exit check still admits one valid vector before an
 * out-of-range lane. The limiting end is the shorter of the source range and the destination budget.
 * Keeping private cursors avoids loop-carried dependencies through the caller's pointer references.
 */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_direct_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovzxbd256)
	constexpr bool widen_bytes{sizeof(F) == 1 && sizeof(T) == 4 && utf_register_bytes == 32};
#else
	constexpr bool widen_bytes{};
#endif
	// Widening a smaller input vector limits extraction and keeps the output stores register-sized.
	constexpr unsigned widening_bytes{utf_register_bytes == 64 ? 32u : 16u};
	constexpr unsigned lanes{widen_bytes ? 8u : (sizeof(F) < sizeof(T) ? widening_bytes : utf_register_bytes) / sizeof(F)};
	using b = typename utf_vector_types<lanes>::bytes;
	using h = typename utf_vector_types<lanes>::halves;
	using w = typename utf_vector_types<lanes>::words;
	using input = ::std::conditional_t<sizeof(F) == 1, b, ::std::conditional_t<sizeof(F) == 2, h, w>>;
	using output = ::std::conditional_t<sizeof(T) == 1, b, ::std::conditional_t<sizeof(T) == 2, h, w>>;
	auto const available{static_cast<::std::size_t>(last - p)};
	auto const stop{p + (available < room ? available : room)};
	auto cursor{p};
	auto destination{out};
#if defined(FAST_IO_UTF_SVE_ASCII)
	if constexpr (sizeof(F) == 2 && sizeof(T) == 1)
	{
		utf_sve_ascii_run<A>(cursor, stop, destination);
	}
#undef FAST_IO_UTF_SVE_ASCII
#endif
	auto load = [](Input pointer) noexcept {
		auto value{utf_vector_load<input>(pointer)};
		utf_order_lanes<A>(value);
		return value;
	};
	auto store = [](input data, Output pointer) noexcept {
		output converted;
#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovzxbd256)
		if constexpr (widen_bytes)
		{
			// Direct AVX2 widening prevents the two-stage byte-to-word-to-dword expansion.
			using bytes FAST_IO_UTF_LANES(16, char) = char;
			auto wide{__builtin_shufflevector(data, input{}, 0, 1, 2, 3, 4, 5, 6, 7,
				8, 9, 10, 11, 12, 13, 14, 15)};
			converted = __builtin_bit_cast(output, __builtin_ia32_pmovzxbd256(__builtin_bit_cast(bytes, wide)));
		}
		else
#endif
		{
			converted = __builtin_convertvector(data, output);
		}
		utf_order_lanes<B>(converted);
		__builtin_memcpy(unicode_address(pointer), &converted, sizeof(converted));
	};
#if defined(__SSE4_1__) && !defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
	if constexpr (sizeof(F) == 1 && sizeof(T) == 4)
	{
		if (stop - cursor >= 128 && !utf_mask(load(cursor) | load(cursor + 16)))
		{
			utf_ascii_to_utf32_run<B>(cursor, stop, destination);
		}
	}
#endif
	for (; stop - cursor >= 2 * lanes; cursor += 2 * lanes, destination += 2 * lanes)
	{
		auto first{load(cursor)}, second{load(cursor + lanes)};
		if (utf_any(utf_direct_errors<F, T>(first) | utf_direct_errors<F, T>(second)))
		{
			break;
		}
#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_packusdw256)
		if constexpr (sizeof(F) == 4 && sizeof(T) == 2 && lanes == 8)
		{
			using words FAST_IO_UTF_LANES(32, int) = int;
			using halves = typename utf_vector_types<16>::halves;
			auto packed{__builtin_bit_cast(halves,
				__builtin_ia32_packusdw256(__builtin_bit_cast(words, first), __builtin_bit_cast(words, second)))};
			packed = __builtin_shufflevector(packed, packed, 0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15);
			utf_order_lanes<B>(packed);
			__builtin_memcpy(unicode_address(destination), &packed, sizeof(packed));
		}
		else
#endif
		{
			store(first, destination);
			store(second, destination + lanes);
		}
	}
	if (stop - cursor >= lanes)
	{
		auto units{load(cursor)};
		if (!utf_any(utf_direct_errors<F, T>(units)))
		{
			store(units, destination);
			cursor += lanes;
			destination += lanes;
		}
	}
	auto const consumed{static_cast<::std::size_t>(cursor - p)};
	p = cursor;
	out = destination;
	room -= consumed;
	return consumed != 0;
}

/** @brief Consumes uniform two-byte UTF-8 directly into UTF-16 or UTF-32 lanes. */
template <encoding_scheme B, typename Input, typename Output>
inline bool utf_pairs_block(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	constexpr unsigned n{utf_register_bytes / 2};
	if (static_cast<::std::size_t>(last - p) < 2 * n || room < n)
	{
		return false;
	}
	using halves = typename utf_vector_types<n>::halves;
	using words = typename utf_vector_types<n>::words;
	halves x;
	__builtin_memcpy(&x, unicode_address(p), sizeof(x));
	utf_order_lanes<encoding_scheme::utf_le>(x);
	if (utf_any(((x & 0xc0e0u) != 0x80c0u) | ((x & 255u) < 0xc2u)))
	{
		return false;
	}
	auto cp{((x & 31u) << 6u) | ((x >> 8u) & 63u)};
	using result_type = ::std::conditional_t<sizeof(T) == 2, halves, words>;
	result_type result{__builtin_convertvector(cp, result_type)};
	utf_order_lanes<B>(result);
	__builtin_memcpy(unicode_address(out), &result, sizeof(result));
	p += 2 * n;
	out += n;
	room -= n;
	return true;
}

/** @brief Emits four BMP or four supplementary scalars without dynamic shuffle masks or output overstore. */
template <unsigned Width, encoding_scheme B, typename Output>
inline unsigned utf_fixed_output(utf_w4 cp, Output out, ::std::size_t room) noexcept
{
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	if constexpr (sizeof(T) == 4)
	{
		if (room < 4)
		{
			return 0;
		}
		utf_order_lanes<B>(cp);
		__builtin_memcpy(unicode_address(out), &cp, 16);
		return 4;
	}
	else if constexpr (sizeof(T) == 2)
	{
		if constexpr (Width == 4)
		{
			if (room < 8)
			{
				return 0;
			}
			auto x{cp - 0x10000u};
			auto bytes{utf_word_bytes(0xd800u | (x >> 10u) | (0xdc00u << 16u) | ((x & 1023u) << 16u))};
			if constexpr (B == encoding_scheme::utf_be)
			{
				bytes = __builtin_shufflevector(bytes, bytes, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
			}
			__builtin_memcpy(unicode_address(out), &bytes, 16);
			return 8;
		}
		else
		{
			if (room < 4)
			{
				return 0;
			}
			auto h{__builtin_convertvector(cp, utf_h4)};
			utf_order_lanes<B>(h);
			__builtin_memcpy(unicode_address(out), &h, 8);
			return 4;
		}
	}
	else
	{
		if (room < 4 * Width)
		{
			return 0;
		}
		if constexpr (Width == 2)
		{
			auto h{__builtin_convertvector((0xc0u | (cp >> 6u)) | ((0x80u | (cp & 63u)) << 8u), utf_h4)};
			utf_order_lanes<encoding_scheme::utf_le>(h);
			__builtin_memcpy(unicode_address(out), &h, 8);
		}
		else if constexpr (Width == 3)
		{
			auto bytes{utf_word_bytes((0xe0u | (cp >> 12u)) | ((0x80u | ((cp >> 6u) & 63u)) << 8u) | ((0x80u | (cp & 63u)) << 16u))};
			auto packed{__builtin_shufflevector(bytes, bytes, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 0, 0, 0, 0)};
			__builtin_memcpy(unicode_address(out), &packed, 12);
		}
		else
		{
			auto bytes{utf_word_bytes((0xf0u | (cp >> 18u)) | ((0x80u | ((cp >> 12u) & 63u)) << 8u) |
									  ((0x80u | ((cp >> 6u) & 63u)) << 16u) | ((0x80u | (cp & 63u)) << 24u))};
			__builtin_memcpy(unicode_address(out), &bytes, 16);
		}
		return 4 * Width;
	}
}

/**
 * @brief Packs a register of equal-width BMP scalars into two or three UTF-8 bytes per scalar.
 * @details The entire vector must lie in the selected scalar interval. UTF-16 surrogates and UTF-32
 * encodings of them are rejected before a store. Output stores have exact byte lengths.
 */
template <unsigned Width, encoding_scheme A, typename Input, typename Output>
inline bool utf_bmp_output(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	constexpr unsigned n{utf_register_bytes / sizeof(F)};
	if (last - p < n || room < Width * n)
	{
		return false;
	}
	using h = typename utf_vector_types<n>::halves;
	using w = typename utf_vector_types<n>::words;
	using input = ::std::conditional_t<sizeof(F) == 2, h, w>;
	input cp;
	__builtin_memcpy(&cp, unicode_address(p), sizeof(cp));
	utf_order_lanes<A>(cp);
	if constexpr (Width == 2)
	{
		if (utf_any((cp < 0x80u) | (cp >= 0x800u)))
		{
			return false;
		}
		h bytes{__builtin_convertvector((0xc0u | (cp >> 6u)) | ((0x80u | (cp & 63u)) << 8u), h)};
		utf_order_lanes<encoding_scheme::utf_le>(bytes);
		__builtin_memcpy(unicode_address(out), &bytes, sizeof(bytes));
	}
	else
	{
		constexpr auto limit{static_cast<::std::remove_reference_t<decltype(cp[0])>>(0xffffu - 0x800u)};
		auto invalid{((cp - 0x800u) > limit) | ((cp & 0xf800u) == 0xd800u)};
#if defined(__AVX512VBMI__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_ucmpw512_mask) && FAST_IO_HAS_BUILTIN(__builtin_ia32_cmpw512_mask)
		if constexpr (sizeof(F) == 2 && n == 32)
		{
			using words FAST_IO_UTF_LANES(64, short) = short;
			auto const small{__builtin_ia32_ucmpw512_mask(__builtin_bit_cast(words, cp),
				__builtin_bit_cast(words, h{} + 0x800u), 1, ~0u)};
			auto const surrogates{__builtin_ia32_cmpw512_mask(__builtin_bit_cast(words, cp & 0xf800u),
				__builtin_bit_cast(words, h{} + 0xd800u), 0, ~0u)};
			if (small | surrogates)
			{
				return false;
			}
		}
		else
#endif
		if (utf_any(invalid))
		{
			return false;
		}
#if defined(FAST_IO_UTF_WIDE_LOOKUP) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#if defined(__AVX512VBMI__)
#if FAST_IO_HAS_BUILTIN(__builtin_ia32_vpermi2varqi512) || FAST_IO_HAS_BUILTIN(__builtin_ia32_vpermi2varqi512_mask)
		if constexpr (sizeof(F) == 2 && n == 32)
		{
			using bytes FAST_IO_UTF_LANES(64, char) = char;
			auto const heads{__builtin_bit_cast(bytes, 0x80e0u | (cp >> 12u) | ((cp & 0xfc0u) << 2u))};
			auto const tails{__builtin_bit_cast(bytes, 0x80u | (cp & 63u))};
			auto const indices = [](unsigned offset) constexpr {
				::fast_io::freestanding::array<char, 64> values{};
				for (unsigned i{}; i != 64 && offset + i < 96u; ++i)
				{
					auto const byte{offset + i};
					auto const field{byte % 3u};
					values[i] = static_cast<char>(2u * (byte / 3u) + (field == 2u ? 64u : field));
				}
				return values;
			};
			auto const pack = [&](bytes selected) noexcept {
#if FAST_IO_HAS_BUILTIN(__builtin_ia32_vpermi2varqi512)
				return __builtin_ia32_vpermi2varqi512(heads, selected, tails);
#else
				return __builtin_ia32_vpermi2varqi512_mask(heads, selected, tails, ~0ull);
#endif
			};
			// Each permutation selects bytes from adjacent UTF-8 heads and tails.
			constexpr auto first_bytes{indices(0)};
			constexpr auto second_bytes{indices(64)};
			auto const first{pack(__builtin_bit_cast(bytes, first_bytes))};
			auto const second{pack(__builtin_bit_cast(bytes, second_bytes))};
			__builtin_memcpy(unicode_address(out), &first, sizeof(first));
			__builtin_memcpy(unicode_address(out + 64), &second, 32);
		}
		else
#endif
#endif
		if constexpr (sizeof(F) == 2 && n == 16)
		{
			constexpr utf_b16 positions{0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 255, 255, 255, 255};
			auto const indices{utf_broadcast_lookup(positions)};
			auto heads{0x80e0u | (cp >> 12u) | ((cp & 0xfc0u) << 2u)};
			auto tails{0x80u | (cp & 63u)};
			auto lower{__builtin_shufflevector(heads, tails, 0, 16, 1, 17, 2, 18, 3, 19,
				8, 24, 9, 25, 10, 26, 11, 27)};
			auto upper{__builtin_shufflevector(heads, tails, 4, 20, 5, 21, 6, 22, 7, 23,
				12, 28, 13, 29, 14, 30, 15, 31)};
			auto a{utf_shuffle(__builtin_bit_cast(utf_b32, lower), indices)};
			auto b{utf_shuffle(__builtin_bit_cast(utf_b32, upper), indices)};
			auto a0{__builtin_shufflevector(a, a, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)};
			auto b0{__builtin_shufflevector(b, b, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)};
			auto a1{__builtin_shufflevector(a, a, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
			auto b1{__builtin_shufflevector(b, b, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
			// Each following store overwrites the preceding padding. The last store is exact.
			__builtin_memcpy(unicode_address(out), &a0, 16);
			__builtin_memcpy(unicode_address(out + 12), &b0, 16);
			__builtin_memcpy(unicode_address(out + 24), &a1, 16);
			__builtin_memcpy(unicode_address(out + 36), &b1, 12);
		}
		else
#endif
		if constexpr (sizeof(F) == 2)
		{
			auto pack = [](utf_h8 words) noexcept {
				auto raw{__builtin_bit_cast(utf_b16, words)};
				return __builtin_shufflevector(raw, raw, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 0, 0, 0, 0);
			};
			for (unsigned i{}; i != n; i += 8)
			{
				auto words{utf_vector_load<utf_h8>(reinterpret_cast<unsigned char const *>(&cp) + 2 * i)};
				auto heads{utf_swap_units<encoding_scheme::utf_le>(
					__builtin_bit_cast(utf_h8, (0x80e0u | (words >> 12u) | ((words & 0xfc0u) << 2u))))};
				auto tails{utf_swap_units<encoding_scheme::utf_le>(
					__builtin_bit_cast(utf_h8, (0x80u | (words & 63u))))};
				auto first{pack(__builtin_shufflevector(heads, tails, 0, 8, 1, 9, 2, 10, 3, 11))};
				auto second{pack(__builtin_shufflevector(heads, tails, 4, 12, 5, 13, 6, 14, 7, 15))};
				// The next group replaces the four speculative bytes inside this accepted packet.
				__builtin_memcpy(unicode_address(out + i * 3), &first, sizeof(first));
				__builtin_memcpy(unicode_address(out + i * 3 + 12), &second, 12);
			}
		}
#if defined(FAST_IO_UTF_WIDE_LOOKUP)
		else if constexpr (sizeof(F) == 4 && n == 8)
		{
			// One AVX2 packet holds eight three-byte outputs, padded to one word per scalar.
			constexpr utf_b16 positions{0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 255, 255, 255, 255};
			auto words{0x8080e0u | (cp >> 12u) | ((cp & 0xfc0u) << 2u) | ((cp & 63u) << 16u)};
			auto packed{utf_shuffle(__builtin_bit_cast(utf_b32, words), utf_broadcast_lookup(positions))};
			auto first{__builtin_shufflevector(packed, packed, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)};
			auto second{__builtin_shufflevector(packed, packed, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
			// The adjacent group covers the four padding bytes and ends exactly at the packet boundary.
			__builtin_memcpy(unicode_address(out), &first, sizeof(first));
			__builtin_memcpy(unicode_address(out + 12), &second, 12);
		}
#endif
		else
		{
			w values{__builtin_convertvector(cp, w)};
			// The loop bounds are physical register widths, so the compiler emits a fixed number of triple packs.
			for (unsigned i{}; i != n; i += 4)
			{
				utf_w4 group;
				__builtin_memcpy(&group, reinterpret_cast<unsigned char const *>(&values) + 4 * i, 16);
				utf_fixed_output<3, encoding_scheme::utf>(group, out + i * 3, 12);
			}
		}
	}
	p += n;
	out += Width * n;
	room -= Width * n;
	return true;
}

/** @brief Keeps cursors local for a run of equal-width BMP packets. */
template <unsigned Width, encoding_scheme A, typename Input, typename Output>
inline bool utf_bmp_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto cursor{p};
	auto destination{out};
	auto available{room};
	while (utf_bmp_output<Width, A>(cursor, last, destination, available))
	{
	}
	auto progress{cursor != p};
	p = cursor;
	out = destination;
	room = available;
	return progress;
}

/** @brief Validates four three-byte or four four-byte sequences without consulting a variable layout table. */
template <unsigned Width, encoding_scheme B, typename Input, typename Output>
inline bool utf_fixed_input(Input &p, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto data{utf_vector_load<utf_b16>(p)};
	if constexpr (Width == 3)
	{
		data = __builtin_shufflevector(data, utf_b16{}, 0, 1, 2, 16, 3, 4, 5, 16, 6, 7, 8, 16, 9, 10, 11, 16);
	}
	auto x{utf_word_values(data)};
	utf_w4 cp;
	if constexpr (Width == 3)
	{
		cp = ((x & 15u) << 12u) | ((x >> 2u) & 0xfc0u) | ((x >> 16u) & 63u);
		if (utf_any(((x & 0xc0c0f0u) != 0x8080e0u) | (cp < 0x800u) | ((cp >= 0xd800u) & (cp < 0xe000u))))
		{
			return false;
		}
	}
	else
	{
		cp = ((x & 7u) << 18u) | ((x << 4u) & 0x3f000u) | ((x >> 10u) & 0xfc0u) | ((x >> 24u) & 63u);
		if (utf_any(((x & 0xc0c0c0f8u) != 0x808080f0u) | (cp < 0x10000u) | (cp > 0x10ffffu)))
		{
			return false;
		}
	}
	auto written{utf_fixed_output<Width, B>(cp, out, room)};
	if (!written)
	{
		return false;
	}
	p += 4 * Width;
	out += written;
	room -= written;
	return true;
}

/**
 * @brief Maps the next twelve character-end bits to a validating converter's best fixed shuffle.
 * @details Generate the three width classes described by Muła and Lemire (simdutf), from lowest to highest
 * priority. Three scalars of up to four bytes fit in twelve input bytes. Four BMP scalars or six ASCII/two-byte
 * scalars can replace that layout when they fit. Bits after the selected prefix leave its plan unchanged.
 */
template <bool Enabled>
inline constexpr auto utf_validated_layouts = [] {
	struct plan
	{
		unsigned char row{}, consumed{};
	};
	struct tables
	{
		::fast_io::freestanding::array<plan, 4096> plans{};
		unsigned char shuffles[209][16]{};
	};
	tables result{};
	auto insert = [&]<unsigned Count, unsigned Width, unsigned Rows>(unsigned first) {
		for (unsigned row{}; row != Rows; ++row)
		{
			auto &shuffle{result.shuffles[first + row]};
			for (auto &lane : shuffle)
			{
				lane = 255;
			}
			unsigned widths{row}, length{}, ends{};
			for (unsigned slot{}; slot != Count; ++slot)
			{
				unsigned bytes{1u + widths % Width};
				widths /= Width;
				for (unsigned j{}; j != bytes; ++j)
				{
					shuffle[(Width == 2 ? 2u : 4u) * slot + j] =
						static_cast<unsigned char>(length + bytes - 1u - j);
				}
				length += bytes;
				ends |= 1u << (length - 1u);
			}
			for (unsigned mask{ends}; mask < 4096; mask += 1u << length)
			{
				result.plans[mask] = {static_cast<unsigned char>(first + row), static_cast<unsigned char>(length)};
			}
		}
	};
	insert.template operator()<3, 4, 64>(145);
	insert.template operator()<4, 3, 81>(64);
	insert.template operator()<6, 2, 64>(0);
	return result;
}();

/** @brief Converts one group selected from an already validated twelve-byte character-boundary mask. */
template <encoding_scheme B, bool Replace = false, typename Output>
inline unsigned utf_validated_group(utf_b16 source, unsigned row, Output out, bool full) noexcept
{
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto shuffled{utf_shuffle(source, utf_vector_load<utf_b16>(utf_validated_layouts<(sizeof(T) > 1)>.shuffles[row]))};
	if (row < 64)
	{
		auto x{utf_swap_units<encoding_scheme::utf_le>(__builtin_bit_cast(utf_h8, shuffled))};
		auto cp{(x & 0x7fu) | ((x >> 2u) & 0x7c0u)};
		using wide = ::std::conditional_t<sizeof(T) == 2, utf_h8, typename utf_vector_types<8>::words>;
		auto converted{__builtin_convertvector(cp, wide)};
		// Keep wide vectors local when the target ABI only passes narrower vectors in registers.
		utf_order_lanes<B>(converted);
		__builtin_memcpy(unicode_address(out), &converted, 6 * sizeof(T));
		return 6;
	}
	auto x{utf_word_values(shuffled)};
	auto cp{(x & 0x7fu) | ((x >> 2u) & 0xfc0u) | ((x >> 4u) & 0xf000u)};
	if (row < 145)
	{
		if constexpr (sizeof(T) == 4)
		{
			cp = utf_swap_units<B>(cp);
			__builtin_memcpy(unicode_address(out), &cp, sizeof(cp));
		}
		else
		{
			auto units{utf_swap_units<B>(__builtin_convertvector(cp, utf_h4))};
			__builtin_memcpy(unicode_address(out), &units, sizeof(units));
		}
		return 4;
	}
	// In byte lane two, an E-prefix contributes four payload bits and a continuation contributes six.
	// Its bit 6 distinguishes the E-prefix. Clear its spurious bit 5 without another table load.
	cp |= (((x >> 4u) ^ (x >> 5u)) & 0x20000u) | ((x >> 4u) & 0x10000u) | ((x >> 6u) & 0x1c0000u);
	if constexpr (sizeof(T) == 4)
	{
		cp = utf_swap_units<B>(cp);
		__builtin_memcpy(unicode_address(out), &cp, full ? sizeof(cp) : 3u * sizeof(T));
		return 3;
	}
	else
	{
		// The zero fourth lane is a placeholder. A short store omits it and the unused half of each BMP pair.
		auto large{__builtin_bit_cast(utf_w4, cp > 0xffffu)};
		auto mask{utf_lane_bits(large)};
		auto delta{cp - 0x10000u};
		auto pairs{(large & (0xd800u | (delta >> 10u) | (0xdc00u << 16u) | ((delta & 1023u) << 16u))) | (~large & cp)};
		auto bytes{utf_word_bytes(pairs)};
		if constexpr (B == encoding_scheme::utf_be)
		{
			bytes = __builtin_shufflevector(bytes, bytes, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
		}
		auto const &layout{utf_half_layouts<(sizeof(T) != 0)>[mask]};
		auto result{utf_shuffle(bytes, utf_vector_load<utf_b16>(layout.pack))};
#if defined(__clang__) && defined(__AVX2__)
		// Clang 21 gives the replacement loop a faster layout with the variable-width copy.
		if constexpr (__clang_major__ != 21 || !Replace)
		{
			// The group has three scalars and one placeholder lane. Each scalar contributes
			// one or two UTF-16 units, so the exact store covers three to six units.
			switch (layout.units - 1u)
			{
			case 3:
				__builtin_memcpy(unicode_address(out), &result, 6);
				break;
			case 4:
				__builtin_memcpy(unicode_address(out), &result, 8);
				break;
			case 5:
				__builtin_memcpy(unicode_address(out), &result, 10);
				break;
			default:
				__builtin_memcpy(unicode_address(out), &result, 12);
				break;
			}
		}
		else
#endif
		{
			__builtin_memcpy(unicode_address(out), &result, (layout.units - 1u) * 2u);
		}
		return layout.units - 1u;
	}
}

/** @brief Marks each lead or ASCII byte in the four-vector validating tile. */
inline unsigned utf_start_mask(utf_b16 bytes) noexcept
{
	return utf_mask(__builtin_bit_cast(utf_b16, (bytes & 0xc0u) != 0x80u));
}

/** @brief Shifts the last partial UTF-8 vector into the low lanes and zeros the other lanes. */
inline constexpr auto utf_short_input_tail = [] {
	struct row
	{
		unsigned char index[16];
	};
	::fast_io::freestanding::array<row, 16> rows{};
	for (unsigned n{1}; n != 16; ++n)
	{
		for (unsigned i{}; i != 16; ++i)
		{
			rows[n].index[i] = static_cast<unsigned char>(i < n ? 16u - n + i : 255u);
		}
	}
	return rows;
}();

/**
 * @brief Validates a short UTF-8 packet and emits its complete scalars into an exact-size destination.
 * @details Zero padding completes the validation window so a truncated scalar falls back with the original cursors.
 * Each validated start contributes one output unit plus a second unit for supplementary scalars in UTF-16.
 */
template <encoding_scheme B, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_short_input(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using T = unicode_cursor_value_t<Output>;
	auto const length{static_cast<unsigned>(last - p)};
	if (length < 16 || length >= 64 || room < (length + 3u) / 4u)
	{
		return false;
	}
	utf_b16 previous{}, errors{};
	unsigned long long starts{};
	unsigned astral{};
	unsigned offset{};
	auto consume = [&](utf_b16 data, unsigned mask) noexcept {
		errors |= utf_lookup_errors(previous, data);
		previous = data;
		starts |= static_cast<unsigned long long>(utf_start_mask(data) & mask) << offset;
		if constexpr (sizeof(T) == 2)
		{
			astral += static_cast<unsigned>(__builtin_popcount(utf_mask(__builtin_bit_cast(utf_b16, data >= 0xf0u))));
		}
	};
	for (; length - offset >= 16; offset += 16)
	{
		consume(utf_vector_load<utf_b16>(p + offset), 65535u);
	}
	if (auto const suffix{length - offset})
	{
		consume(utf_shuffle(utf_vector_load<utf_b16>(last - 16), utf_vector_load<utf_b16>(utf_short_input_tail[suffix].index)), (1u << suffix) - 1u);
	}
	else
	{
		errors |= utf_lookup_errors(previous, utf_b16{});
	}
	auto scalars{static_cast<unsigned>(__builtin_popcountll(starts))};
	auto units{scalars + astral};
	if (units > room || utf_any(errors))
	{
		return false;
	}
	auto cursor{p};
	auto q{out};
	// The virtual start after the packet also marks the final scalar's end.
	auto ends{(starts | (1ull << length)) >> 1u};
	while (scalars >= 4)
	{
		auto row{utf_four_shuffle_row<(sizeof(T) > 1)>(ends)};
		auto const remaining{static_cast<unsigned>(last - cursor)};
		auto data{remaining >= 16 ? utf_vector_load<utf_b16>(cursor) :
			utf_shuffle(utf_vector_load<utf_b16>(last - 16), utf_vector_load<utf_b16>(utf_short_input_tail[remaining].index))};
		auto cp{utf_decode_group<(sizeof(T) > 1)>(data, row)};
		unsigned written{};
		if constexpr (sizeof(T) == 2)
		{
			written = units >= 8 ? utf_encode_four<B, true>(cp, q, units) : utf_encode_four<B, false, true>(cp, q, units);
		}
		else
		{
			cp = utf_swap_units<B>(cp);
			__builtin_memcpy(unicode_address(q), &cp, sizeof(cp));
			written = 4;
		}
		scalars -= 4;
		units -= written;
		q += written;
		auto count{utf_four_group<(sizeof(T) > 1)>.count[row]};
		cursor += count;
		ends >>= count;
	}
	while (cursor != last)
	{
		unsigned head{static_cast<unsigned char>(*cursor++)};
		if (head < 0x80u)
		{
			*q++ = unicode_unit<B, T>(head);
			continue;
		}
		unsigned cp{static_cast<unsigned char>(*cursor++) & 63u};
		if (head < 0xe0u)
		{
			cp |= (head & 31u) << 6u;
		}
		else
		{
			cp = (cp << 6u) | (static_cast<unsigned char>(*cursor++) & 63u);
			if (head < 0xf0u)
			{
				cp |= (head & 15u) << 12u;
			}
			else
			{
				cp = ((head & 7u) << 18u) | (cp << 6u) | (static_cast<unsigned char>(*cursor++) & 63u);
			}
		}
		auto width{unicode_width<T>(cp)};
		unicode_emit<B>(cp, width, q);
		q += width;
	}
	room -= static_cast<::std::size_t>(q - out);
	p = last;
	out = q;
	return true;
}

#if defined(FAST_IO_UTF_WIDE_LOOKUP)
/**
 * @brief Recovers a bytewise UTF-8 window made only of ASCII and invalid leading bytes.
 * @details At a scalar boundary, every byte in this subset produces one UTF-16 code unit.
 * Four high-bit bytes in the first tile keep isolated errors on scalar recovery.
 */
template <encoding_scheme Order, typename Input, typename Output>
[[gnu::cold, gnu::noinline]] inline bool
utf_recover_single_byte_run(Input &first, Input last, Output &out, ::std::size_t &room) noexcept
{
	using signed_bytes FAST_IO_UTF_LANES(16, signed char) = signed char;
	using signed_halves FAST_IO_UTF_LANES(32, short) = short;
	using halves = typename utf_vector_types<16>::halves;
	auto cursor{first};
	auto dest{out};
	auto available{room};
	unsigned minimum_invalid{4u};
	while (last - cursor >= 16 && available >= 16u)
	{
		auto const input{utf_vector_load<utf_b16>(cursor)};
		auto const potential_leads{input - static_cast<unsigned char>(0xc2u)};
		if (static_cast<unsigned>(__builtin_popcount(utf_mask(input))) < minimum_invalid ||
			utf_any(__builtin_bit_cast(utf_b16, potential_leads <= static_cast<unsigned char>(0xf4u - 0xc2u))))
		{
			break;
		}
		auto const widened{__builtin_convertvector(__builtin_bit_cast(signed_bytes, input), signed_halves)};
		auto const invalid{__builtin_bit_cast(halves, widened < 0)};
		auto result{utf_swap_units<Order>((__builtin_bit_cast(halves, widened) & ~invalid) |
										 (invalid & static_cast<unsigned short>(0xfffdu)))};
		__builtin_memcpy(unicode_address(dest), &result, sizeof(result));
		cursor += 16;
		dest += 16;
		available -= 16;
		minimum_invalid = 0;
	}
	auto const progress{cursor != first};
	first = cursor;
	out = dest;
	room = available;
	return progress;
}

#endif

/**
 * @brief Validates four successive 16-byte vectors together and converts their complete scalar prefix.
 * @details The previous vector supplies the leading context for Lookup4. The last scalar is left for the
 * next block, even when complete. This gives exactly one restart point for a split scalar or a failing block.
 * At least 64 destination units guarantee space for any valid 64 UTF-8 bytes in either wider encoding.
 */
template <encoding_scheme B, bool Replace = false, typename Input, typename Output>
inline bool utf_mixed_input(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	if (last - p < 64 || room < 64)
	{
		return false;
	}
	using mask_type = unsigned long long;
#if defined(FAST_IO_UTF_WIDE_LOOKUP)
	auto first{utf_vector_load<utf_b32>(p)}, second{utf_vector_load<utf_b32>(p + 32)};
	if (utf_any(utf_lookup_errors(utf_b32{}, first) | utf_lookup_errors(first, second)))
	{
		if constexpr (Replace && sizeof(T) == 2)
		{
			auto const first_tile{__builtin_shufflevector(first, first,
				0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)};
			auto const high{utf_mask(first_tile) & 15u};
			if (high & (high - 1u)) [[unlikely]]
			{
				auto const potential_leads{first_tile - static_cast<unsigned char>(0xc2u)};
				if (!utf_any(__builtin_bit_cast(utf_b16, potential_leads <= static_cast<unsigned char>(0xf4u - 0xc2u))))
				{
					return utf_recover_single_byte_run<B>(p, last, out, room);
				}
			}
		}
		return false;
	}
	auto starts{static_cast<mask_type>(utf_start_mask(first)) | (static_cast<mask_type>(utf_start_mask(second)) << 32u)};
#else
	auto a{utf_vector_load<utf_b16>(p)}, b{utf_vector_load<utf_b16>(p + 16)};
	auto c{utf_vector_load<utf_b16>(p + 32)}, d{utf_vector_load<utf_b16>(p + 48)};
	auto errors{utf_lookup_errors(utf_b16{}, a) | utf_lookup_errors(a, b) | utf_lookup_errors(b, c) | utf_lookup_errors(c, d)};
	if (utf_any(errors))
	{
		return false;
	}
	auto starts{static_cast<mask_type>(utf_start_mask(a)) | (static_cast<mask_type>(utf_start_mask(b)) << 16u) |
		(static_cast<mask_type>(utf_start_mask(c)) << 32u) | (static_cast<mask_type>(utf_start_mask(d)) << 48u)};
#endif
	auto cursor{p};
	// Lookup4 proved that the tile starts on a scalar boundary, so this mask cannot be zero.
	constexpr unsigned mask_bits{::std::numeric_limits<mask_type>::digits};
	auto end{cursor + (mask_bits - 1u - static_cast<unsigned>(__builtin_clzll(starts)))};
	auto q{out};
	auto ends{starts >> 1u};
	auto const vector_end{last - 16};
	// Consecutive scalar starts propose a stride. Use the scalar loop only after confirming it
	// against the complete boundary mask. The validation above remains the source of correctness.
	auto adjacent_starts{starts & (starts >> 1u) & (starts >> 2u) & (starts >> 3u)};
	auto run_ends{adjacent_starts & ~(adjacent_starts >> 1u)};
	auto other_runs{run_ends & (run_ends - 1u)};
	auto period{other_runs ? static_cast<unsigned>(__builtin_ctzll(other_runs) - __builtin_ctzll(run_ends)) : 0u};
	if (period != 0 && period < 32 && !((starts ^ (starts >> period)) & (~mask_type{} >> period)))
	{
		// Reuse the same load for a four-ASCII widening or one complete scalar. The last
		// three bytes use the short tail below, so neither route reads past the validated prefix.
		while (end - cursor >= 4)
		{
			auto data{utf_vector_load<utf_b4>(cursor)};
			auto little{unicode_load<encoding_scheme::utf_le>(__builtin_bit_cast(unsigned, data))};
			if ((little & 0x80808080u) == 0)
			{
				using V = ::std::conditional_t<sizeof(T) == 2, utf_h4, utf_w4>;
				auto wide{utf_swap_units<B>(__builtin_convertvector(data, V))};
				__builtin_memcpy(unicode_address(q), &wide, sizeof(wide));
				cursor += 4;
				q += 4;
				continue;
			}
			auto head{little & 255u};
			if (head < 0x80u)
			{
				*q++ = unicode_unit<B, T>(head);
				++cursor;
				continue;
			}
			if (head < 0xe0u)
			{
				*q++ = unicode_unit<B, T>(((head & 31u) << 6u) | ((little >> 8u) & 63u));
				cursor += 2;
				continue;
			}
			if (head < 0xf0u)
			{
				*q++ = unicode_unit<B, T>(((head & 15u) << 12u) | ((little >> 2u) & 0xfc0u) | ((little >> 16u) & 63u));
				cursor += 3;
				continue;
			}
			auto cp{((head & 7u) << 18u) | ((little << 4u) & 0x3f000u) | ((little >> 10u) & 0xfc0u) | ((little >> 24u) & 63u)};
			auto width{unicode_width<T>(cp)};
			unicode_emit<B>(cp, width, q);
			q += width;
			cursor += 4;
		}
		while (cursor < end)
		{
			unsigned head{static_cast<unsigned char>(*cursor++)};
			if (head < 128)
			{
				*q++ = unicode_unit<B, T>(head);
				continue;
			}
			unsigned cp{static_cast<unsigned char>(*cursor++) & 63u};
			if (head < 0xe0u)
			{
				cp |= (head & 31u) << 6u;
			}
			else
			{
				cp = (cp << 6u) | (static_cast<unsigned char>(*cursor++) & 63u);
				if (head < 0xf0u)
				{
					cp |= (head & 15u) << 12u;
				}
				else
				{
					cp = ((head & 7u) << 18u) | (cp << 6u) | (static_cast<unsigned char>(*cursor++) & 63u);
				}
			}
			auto width{unicode_width<T>(cp)};
			unicode_emit<B>(cp, width, q);
			q += width;
		}
	}
	else
	{
		if constexpr (sizeof(T) == 4)
		{
			// Sixteen complete bytes guarantee four scalar ends. A fixed four-lane store avoids
			// a variable output count in each iteration of the UTF-32 path.
			while (end - cursor >= 16)
			{
				auto const row{utf_four_shuffle_row<(sizeof(T) > 1)>(ends)};
				auto const data{utf_vector_load<utf_b16>(cursor)};
#if defined(FAST_IO_UTF_RVV_GATHER)
				auto const &layout{utf_byte_layouts<(sizeof(T) > 1)>[row]};
				auto const indices{utf_vector_load<utf_b16>(layout.expand)};
				auto cp{utf_unpack_four(utf_word_values(utf_shuffle(data, indices)))};
				auto const count{layout.count};
#else
				auto cp{utf_decode_group<(sizeof(T) > 1)>(data, row)};
#endif
				cp = utf_swap_units<B>(cp);
				__builtin_memcpy(unicode_address(q), &cp, sizeof(cp));
				q += 4;
#if !defined(FAST_IO_UTF_RVV_GATHER)
				auto const count{utf_four_group<(sizeof(T) > 1)>.count[row]};
#endif
				cursor += count;
				ends >>= count;
			}
		}
		while (cursor <= vector_end)
		{
			auto plan{utf_validated_layouts<(sizeof(T) > 1)>.plans[ends & 0xfffu]};
			// Selecting a complete leading group never reads payload outside the validated prefix.
			if (!plan.consumed)
			{
				break;
			}
			q += utf_validated_group<B, Replace>(utf_vector_load<utf_b16>(cursor), plan.row, q, end - cursor >= 16);
			cursor += plan.consumed;
			ends >>= plan.consumed;
		}
	}
	room -= static_cast<::std::size_t>(q - out);
	auto const consumed{cursor != p};
	p = cursor;
	out = q;
	return consumed;
}

/**
 * @brief Transcodes consecutive validated UTF-8 tiles with private cursors.
 * @details The next tile starts at the first unprocessed scalar, which may be in the preceding tile's tail.
 * A separate entry point keeps the dispatcher's other SIMD and scalar paths out of the register allocator's live set.
 */
template <encoding_scheme B, bool Replace = false, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_mixed_input_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto cursor{p};
	auto destination{out};
	auto available{room};
	while (utf_mixed_input<B, Replace>(cursor, last, destination, available))
	{
	}
	auto progress{cursor != p};
	p = cursor;
	out = destination;
	room = available;
	return progress;
}

#define FAST_IO_UNICODE_SHORT_INPUT
/** @brief Identifies bytes that cannot begin a UTF-8 scalar at a known boundary. */
inline constexpr bool utf_invalid_lead(unsigned char lead) noexcept
{
	return (lead >= 0x80u && lead < 0xc2u) || lead > 0xf4u;
}

/** @brief Uses exact output sizing for a short mixed UTF-8 range and for its last partial tile. */
template <encoding_scheme B, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_short_input_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	// Leave ASCII and fixed-width runs to the ordinary dispatcher.
	auto high_bits{utf_mask(utf_vector_load<utf_b16>(p))};
	if (high_bits == 0 || high_bits == 65535u)
	{
		return false;
	}
	if (last - p >= 64)
	{
		utf_mixed_input_run<B>(p, last, out, room);
	}
	return utf_short_input<B>(p, last, out, room);
}

/**
 * @brief Maps four validated UTF-16 lanes to the retained UTF-32 lane order.
 * @details Each set bit identifies the trailing half of a scalar already represented by its leading half.
 * A consumer may write all 16 bytes only when it has proved that this address range lies in the final output.
 */
template <bool enabled>
	requires(enabled)
inline constexpr auto utf_surrogate_layouts = [] {
	struct layout
	{
		unsigned char bytes[16]{};
		unsigned count{};
	};
	::fast_io::freestanding::array<layout, 16> result{};
	for (unsigned mask{}; mask != 16; ++mask)
	{
		auto &out{result[mask]};
		for (unsigned lane{}; lane != 4; ++lane)
		{
			if (!(mask & (1u << lane)))
			{
				for (unsigned j{}; j != 4; ++j)
				{
					out.bytes[4u * out.count + j] = static_cast<unsigned char>(4u * lane + j);
				}
				++out.count;
			}
		}
	}
	return result;
}();

#if defined(__SSE2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
/** @brief Tests whether a surrogate occurrence mask repeats with the spacing of its first two occurrences. */
inline unsigned utf_surrogate_period(::std::uint_least64_t marks) noexcept
{
	auto rest{marks & (marks - 1u)};
	if (!rest)
	{
		return 0;
	}
	auto stride{static_cast<unsigned>(__builtin_ctzll(rest) - __builtin_ctzll(marks))};
	auto prefix{~::std::uint_least64_t{} >> stride};
	return stride < 32u && !((marks ^ (marks >> stride)) & prefix) ? stride : 0;
}
#endif

/**
 * @brief Uses scalar validation for a bounded UTF-16-to-UTF-8 run with recurring lane classes.
 * @details Three output bytes per input unit suffice for every accepted scalar, including a four-byte
 * encoding that consumes two source units. Strict decoding leaves a rejected pair at its leading unit.
 * Replacement decoding emits one U+FFFD for each isolated surrogate. A trailing high surrogate
 * stays available in both modes. The dispatcher reassesses the input after at most four tiles.
 */
template <encoding_scheme A, bool Replace = false, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_regular_utf16(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto budget{room / 3u};
	auto length{static_cast<::std::size_t>(last - p)};
	auto count{budget < length ? budget : length};
	constexpr unsigned window{128};
	auto cursor{p};
	auto end{p + (count < window ? count : window)};
	auto q{out};
	while (cursor != end)
	{
		auto word{unicode_load<A>(*cursor)};
		if (word < 0x80u)
		{
			*q++ = static_cast<T>(word);
		}
		else if (word < 0x800u)
		{
			auto packet{static_cast<::std::uint_least16_t>((0xc0u | (word >> 6u)) | ((0x80u | (word & 63u)) << 8u))};
			packet = ::fast_io::little_endian(packet);
			__builtin_memcpy(unicode_address(q), &packet, 2);
			q += 2;
		}
		else if ((word & 0xf800u) != 0xd800u)
		{
			*q++ = static_cast<T>(0xe0u | (word >> 12u));
			*q++ = static_cast<T>(0x80u | ((word >> 6u) & 63u));
			*q++ = static_cast<T>(0x80u | (word & 63u));
		}
		else
		{
			// A high surrogate at the window edge stays available for the next conversion.
			if (word < 0xdc00u && cursor + 1 == end)
			{
				break;
			}
			auto low{word >= 0xdc00u ? 0u : unicode_load<A>(cursor[1])};
			if ((low & 0xfc00u) != 0xdc00u)
			{
				if constexpr (Replace)
				{
					// One isolated surrogate consumes one unit of the three-byte output budget.
					unicode_emit<encoding_scheme::utf>(0xfffdu, 3u, q);
					q += 3;
					++cursor;
					continue;
				}
				else
				{
					break;
				}
			}
			auto cp{0x10000u + ((word & 1023u) << 10u) + (low & 1023u)};
			auto packet{(0xf0u | (cp >> 18u)) | ((0x80u | ((cp >> 12u) & 63u)) << 8u) |
				((0x80u | ((cp >> 6u) & 63u)) << 16u) | ((0x80u | (cp & 63u)) << 24u)};
			packet = ::fast_io::little_endian(packet);
			__builtin_memcpy(unicode_address(q), &packet, 4);
			q += 4;
			++cursor;
		}
		++cursor;
	}
	auto progress{cursor != p};
	p = cursor;
	room -= static_cast<::std::size_t>(q - out);
	out = q;
	return progress;
}

#if defined(FAST_IO_UTF_WIDE_LOOKUP)
/**
 * @brief Returns the end of a prefix with only complete UTF-16 scalars.
 * @details The low-surrogate mask must equal the high-surrogate mask shifted by one lane.
 * An unpaired high at a vector boundary remains for the next vector or the ordinary decoder.
 */
template <encoding_scheme A, typename Input>
inline Input utf_valid_utf16_prefix(Input first, Input last) noexcept
{
	using V = typename utf_vector_types<16>::halves;
	unsigned trailing_high{};
	while (last - first >= 16)
	{
		auto kind{utf_swap_units<A>(utf_vector_load<V>(first)) & 0xfc00u};
		auto high{static_cast<unsigned>(__builtin_ia32_pmovmskb256(__builtin_bit_cast(utf_char32, kind == 0xd800u)))};
		auto low{static_cast<unsigned>(__builtin_ia32_pmovmskb256(__builtin_bit_cast(utf_char32, kind == 0xdc00u)))};
		if (((high << 2u) | trailing_high) != low)
		{
			break;
		}
		trailing_high = high >> 30u;
		first += 16;
	}
	return first + (trailing_high ? -1 : 0);
}
#endif

/**
 * @brief Packs mixed UTF-16 into UTF-8, replacing isolated surrogates in unchecked input.
 * @details A valid pair contributes two bytes from each source lane. The first two depend only on
 * the high surrogate. The next two use the low surrogate and the bottom two bits of its predecessor.
 * Each unpaired surrogate contributes the three-byte encoding of U+FFFD. The successor vector
 * supplies one unit of lookahead. A high surrogate in the last processed lane may contribute
 * its first two bytes there. Its other two bytes are committed before publishing either cursor.
 *
 * A packet requires at most three output bytes per input unit. Keeping sixteen more input units
 * in reserve covers the padding of both sixteen-byte stores, including a possible incomplete
 * final high surrogate. The outer conversion resumes at a scalar boundary and handles the tail.
 */
template <encoding_scheme A, bool Validated = false, typename Input, typename Output>
inline bool utf_mixed_utf16_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using T = unicode_cursor_value_t<Output>;
	auto length{static_cast<::std::size_t>(last - p)};
	auto budget{room / 3u};
	auto count{length < budget ? length : budget};
	if (count < 24u)
	{
		return false;
	}
	auto cursor{p};
	auto q{out};
	auto end{p + count};
	utf_h8 previous{}, previous_high{};
	while (end - cursor >= 24)
	{
		auto words{utf_swap_units<A>(utf_vector_load<utf_h8>(cursor))};
		if (!utf_any(words > 0x7fu))
		{
			using bytes = typename utf_vector_types<8>::bytes;
			auto narrow{__builtin_convertvector(words, bytes)};
			__builtin_memcpy(unicode_address(q), &narrow, sizeof(narrow));
			q += 8;
			cursor += 8;
			previous_high = utf_h8{};
			if constexpr (Validated)
			{
				// Continue inside the validated prefix so short ASCII islands need no new validation.
				auto remaining{static_cast<::std::size_t>(end - cursor)};
				utf_direct_run<A, encoding_scheme::utf>(cursor, end, q, remaining);
				continue;
			}
			// Hand a newly encountered ASCII run back to the wide narrowing loop.
			break;
		}
		auto prev{__builtin_shufflevector(previous, words, 7, 8, 9, 10, 11, 12, 13, 14)};
		auto kinds{words & 0xfc00u};
		auto high{__builtin_bit_cast(utf_h8, kinds == 0xd800u)};
		auto cp{words};
		if constexpr (!Validated)
		{
			auto next{utf_swap_units<A>(utf_vector_load<utf_h8>(cursor + 1))};
			high &= __builtin_bit_cast(utf_h8, (next & 0xfc00u) == 0xdc00u);
			auto surrogate{__builtin_bit_cast(utf_h8, (words & 0xf800u) == 0xd800u)};
			cp = (words & ~surrogate) | (0xfffdu & surrogate);
		}
		auto low{__builtin_shufflevector(previous_high, high, 7, 8, 9, 10, 11, 12, 13, 14)};
		auto paired{high | low};
		auto ascii{__builtin_bit_cast(utf_h8, words < 0x80u)};
		auto three{__builtin_bit_cast(utf_h8, words >= 0x800u) & ~paired};
		auto two{~(ascii | three)};
#if defined(__aarch64__) && defined(__ARM_NEON) && FAST_IO_HAS_BUILTIN(__builtin_reduce_add)
		// A horizontal sum gathers both width bits directly into their disjoint positions.
		constexpr utf_h8 weights{1u, 4u, 16u, 64u, 256u, 1024u, 4096u, 16384u};
		auto row{static_cast<unsigned>(__builtin_reduce_add((two & weights) | (three & (weights << 1u))))};
#else
		auto widths{utf_swap_units<encoding_scheme::utf_le>((two & 0x00ffu) | (three & 0xff00u))};
		// Two bits per source lane encode the contribution length minus one.
		auto row{utf_mask(__builtin_bit_cast(utf_b16, widths))};
#endif
		auto last_byte{(cp & ascii) | (((cp & 63u) | 0x80u) & ~ascii)};
		auto mid{((cp >> 6u) & 63u) | 0x80u | (~three & 0x40u)};
		auto first_two{last_byte | (mid << 8u)};
		if (utf_any(paired))
		{
			// The UTF-16 bias 0x10000 adds 64 to the high payload before the 10-bit shift.
			auto leading{(words & 1023u) + 64u};
			auto leading_bytes{0xf080u | ((leading >> 2u) & 63u) | ((leading >> 8u) << 8u)};
			auto trailing_bytes{0x8080u | (words & 63u) | ((words & 0x3c0u) << 2u) | ((prev & 3u) << 12u)};
			first_two = (first_two & ~paired) | (leading_bytes & high) | (trailing_bytes & low);
		}
		auto third{(cp >> 12u) | 0xe0u};
		first_two = utf_swap_units<encoding_scheme::utf_le>(first_two);
		third = utf_swap_units<encoding_scheme::utf_le>(third);
		auto lo{__builtin_bit_cast(utf_b16, __builtin_shufflevector(first_two, third, 0, 8, 1, 9, 2, 10, 3, 11))};
		auto hi{__builtin_bit_cast(utf_b16, __builtin_shufflevector(first_two, third, 4, 12, 5, 13, 6, 14, 7, 15))};
		auto store = [&](utf_b16 packet, unsigned index) noexcept {
			auto const &layout{utf_byte_layouts<(sizeof(T) != 0)>[index]};
			auto packed{utf_shuffle(packet, utf_vector_load<utf_b16>(layout.pack))};
			__builtin_memcpy(unicode_address(q), &packed, 16);
			q += layout.count;
		};
		store(lo, row & 255u);
		store(hi, row >> 8u);
		previous_high = high;
		previous = words;
		cursor += 8;
	}
	if (previous_high[7])
	{
		auto low{unicode_load<A>(*cursor++)};
		*q++ = static_cast<T>(0x80u | ((previous[7] & 3u) << 4u) | ((low >> 6u) & 15u));
		*q++ = static_cast<T>(0x80u | (low & 63u));
	}
	room -= static_cast<::std::size_t>(q - out);
	p = cursor;
	out = q;
	return true;
}

#if defined(FAST_IO_UTF_WIDE_LOOKUP)

// GCC otherwise lowers the width comparisons to repeated equality tests and loop constants.
#if defined(__GNUC__) && !defined(__clang__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_paddusw256) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pblendvb256)
#define FAST_IO_UTF_GCC_BMP_WIDTHS
#endif

/** @brief Generates shuffle indices for four BMP scalars and their UTF-8 output counts. */
template <bool Enabled>
	requires(Enabled)
inline constexpr auto utf_bmp_mixed_layouts = [] {
	struct tables
	{
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
		alignas(64)
#endif
		unsigned char shuffle[256][16]{};
		unsigned char count[256]{};
	};
	tables result{};
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
	constexpr unsigned ascii_key{0u};
#else
	constexpr unsigned ascii_key{3u};
#endif
	for (unsigned row{}; row != 256; ++row)
	{
		auto &indices{result.shuffle[row]};
		for (auto &index : indices)
		{
			index = 255u;
		}
		unsigned size{};
		for (unsigned lane{}; lane != 4u; ++lane)
		{
			// One-byte lanes use byte 1. Two-byte lanes use bytes 3 and 0.
			// Three-byte lanes prepend byte 2 to those same two slots.
			unsigned const width{(row >> (2u * lane)) & 3u};
			if (width == ascii_key)
			{
				indices[size++] = static_cast<unsigned char>(4u * lane + 1u);
				continue;
			}
			if (width == (ascii_key ^ 3u))
			{
				indices[size++] = static_cast<unsigned char>(4u * lane + 2u);
			}
			indices[size++] = static_cast<unsigned char>(4u * lane + 3u);
			indices[size++] = static_cast<unsigned char>(4u * lane);
		}
		result.count[row] = static_cast<unsigned char>(size);
	}
	return result;
}();

#if defined(__BMI2__) && defined(__POPCNT__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pext_si) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pblendvb256)

/** @brief Generates one shuffle for every arrangement of eight one- or two-byte scalars. */
template <bool Enabled>
	requires(Enabled)
inline constexpr auto utf_bmp_pair_shuffles = [] {
	::fast_io::freestanding::array<::fast_io::freestanding::array<unsigned char, 16>, 256> table{};
	for (unsigned mask{}; mask != 256u; ++mask)
	{
		auto &indices{table[mask]};
		for (unsigned i{}; i != 16u; ++i)
		{
			indices[i] = 255u;
		}
		unsigned count{};
		for (unsigned lane{}; lane != 8u; ++lane)
		{
			// ASCII retains its low byte. For two bytes the continuation is high.
			indices[count++] = static_cast<unsigned char>(2u * lane);
			if (!(mask & (1u << lane)))
			{
				indices[count++] = static_cast<unsigned char>(2u * lane + 1u);
			}
		}
	}
	return table;
}();

#endif

/** @brief Loads sixteen UTF-16 code units in native byte order. */
template <encoding_scheme A, typename Input>
inline auto utf_bmp_load(Input p) noexcept
{
	using halves = typename utf_vector_types<16>::halves;
	auto const value{utf_vector_load<halves>(p)};
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
	if constexpr (A != encoding_scheme::utf)
	{
		constexpr utf_b16 swap{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14};
		return __builtin_bit_cast(halves, utf_shuffle(__builtin_bit_cast(utf_b32, value), utf_broadcast_lookup(swap)));
	}
#endif
	return utf_swap_units<A>(value);
}

#if defined(__BMI2__) && defined(__POPCNT__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pext_si) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pblendvb256)
/** @brief Packs a run of ASCII and two-byte BMP units with two stores per AVX2 packet. */
template <encoding_scheme A, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_bmp_pair_run(Input &p, Input end, Output &out, ::std::size_t &room) noexcept
{
	using halves = typename utf_vector_types<16>::halves;
	using bits FAST_IO_UTF_LANES(32, long long) = long long;
	auto const is_wide = [](halves value) noexcept {
		return !__builtin_ia32_ptestz256(__builtin_bit_cast(bits, value), __builtin_bit_cast(bits, halves{} + 0xf800u));
	};
	auto cursor{p};
	auto q{out};
	auto words{utf_bmp_load<A>(cursor)};
	if (is_wide(words))
	{
		return false;
	}
	auto const &table{utf_bmp_pair_shuffles<(sizeof(unicode_cursor_value_t<Input>) == 2)>};
	while (end - cursor >= 32)
	{
		auto const next{utf_bmp_load<A>(cursor + 16)};
		if (is_wide(next))
		{
			break;
		}
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
		using signed_halves FAST_IO_UTF_LANES(32, short) = short;
		// The validated words are at most 0x7ff, so this bias sets bit 15 at 0x80.
		auto const flags{__builtin_bit_cast(signed_halves, words + 0x7f80u)};
		auto const wide{__builtin_bit_cast(utf_char32, flags >> 15u)};
		auto const high_bytes{static_cast<unsigned>(__builtin_ia32_pmovmskb256(__builtin_bit_cast(utf_char32, flags)))};
		auto const mask{__builtin_ia32_pext_si(high_bytes, 0xaaaaaaaau) ^ 0xffffu};
		auto const first{mask & 255u}, second{mask >> 8u};
		auto const pair{((words >> 6u) | 0xc0u) | (((words & 63u) | 0x80u) << 8u)};
		auto const bytes{__builtin_bit_cast(utf_b32, __builtin_ia32_pblendvb256(
			__builtin_bit_cast(utf_char32, words), __builtin_bit_cast(utf_char32, pair), wide))};
#else
		auto const ascii{__builtin_bit_cast(utf_char32, (words & 0xff80u) == 0u)};
		auto const mask{__builtin_ia32_pext_si(static_cast<unsigned>(__builtin_ia32_pmovmskb256(ascii)), 0x55555555u)};
		auto const first{mask & 255u}, second{mask >> 8u};
		auto const pair{((words >> 6u) | 0xc0u) | (((words & 63u) | 0x80u) << 8u)};
		auto const bytes{__builtin_bit_cast(utf_b32, __builtin_ia32_pblendvb256(
			__builtin_bit_cast(utf_char32, pair), __builtin_bit_cast(utf_char32, words), ascii))};
#endif
		auto const a{utf_vector_load<utf_b16>(table[first].data())};
		auto const b{utf_vector_load<utf_b16>(table[second].data())};
		auto const indices{__builtin_shufflevector(a, b, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
		auto const packed{utf_shuffle(bytes, indices)};
		auto const lo{__builtin_shufflevector(packed, packed, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)};
		auto const hi{__builtin_shufflevector(packed, packed, 16, 17, 18, 19, 20, 21, 22, 23,
			24, 25, 26, 27, 28, 29, 30, 31)};
		// The already checked next packet emits at least 16 bytes within the caller's budget.
		__builtin_memcpy(unicode_address(q), &lo, sizeof(lo));
		q += 16u - static_cast<unsigned>(__builtin_popcount(first));
		__builtin_memcpy(unicode_address(q), &hi, sizeof(hi));
		q += 16u - static_cast<unsigned>(__builtin_popcount(second));
		cursor += 16;
		words = next;
	}
	auto const progress{cursor != p};
	room -= static_cast<::std::size_t>(q - out);
	p = cursor;
	out = q;
	return progress;
}
#endif

/** @brief Converts 16 independently validated BMP code units in each AVX2 iteration. */
template <encoding_scheme A, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_bmp_mixed_run(Input &p, Input end, Output &out, ::std::size_t &room) noexcept
{
	using halves = typename utf_vector_types<16>::halves;
	auto const has_surrogate = [](halves words) noexcept {
		return utf_any(__builtin_bit_cast(halves, (words & 0xf800u) == 0xd800u));
	};
	auto cursor{p};
	auto q{out};
	auto words{utf_bmp_load<A>(cursor)};
	if (has_surrogate(words))
	{
		return false;
	}
	auto const &layouts{utf_bmp_mixed_layouts<(sizeof(unicode_cursor_value_t<Input>) == 2)>};
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
	for (auto packets{(end - cursor) / 16}; packets > 1; --packets)
#else
	while (end - cursor >= 32)
#endif
	{
		// The next 16 BMP units will emit at least 16 bytes and fit within the
		// caller's three-byte budget. Their output covers every speculative store.
		auto const next{utf_bmp_load<A>(cursor + 16)};
		if (has_surrogate(next))
		{
			break;
		}
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
		// Saturation sets bit 15 at the two UTF-8 width thresholds and keeps it set.
		using signed_halves FAST_IO_UTF_LANES(32, short) = short;
		auto const adds = [](halves value, halves bias) noexcept {
			return __builtin_bit_cast(halves, __builtin_ia32_paddusw256(
				__builtin_bit_cast(signed_halves, value), __builtin_bit_cast(signed_halves, bias)));
		};
		auto const ascii_flag{adds(words, halves{} + 0x7f80u)};
		auto const wide_flag{adds(words, halves{} + 0x7800u)};
		auto const kinds{__builtin_ia32_pblendvb256(
			__builtin_bit_cast(utf_char32, ascii_flag >> 8u), __builtin_bit_cast(utf_char32, wide_flag),
			__builtin_bit_cast(utf_char32, halves{} + 0xff00u))};
#else
		auto const ascii{__builtin_bit_cast(halves, (words & 0xff80u) == 0u)};
		auto const small{__builtin_bit_cast(halves, (words & 0xf800u) == 0u)};
		auto const kinds{(ascii & 0x00ffu) | (small & 0xff00u)};
#endif
		auto const row{static_cast<unsigned>(__builtin_ia32_pmovmskb256(__builtin_bit_cast(utf_char32, kinds)))};

#if FAST_IO_HAS_BUILTIN(__builtin_ia32_pmaddubsw256)
		constexpr utf_b16 duplicate{0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14};
		auto const payload{__builtin_bit_cast(halves, utf_shuffle(__builtin_bit_cast(utf_b32, words),
			utf_broadcast_lookup(duplicate)))};
		auto const trailing{(payload & 0x7f3fu) | 0x0080u};
		// The low byte times 64 moves the middle six payload bits into byte 3.
		// The high byte times 1 carries the upper nibble into byte 2, without saturation.
		auto const shifted{(words >> 4u) & 0x0ffcu};
		auto const multiply{__builtin_bit_cast(utf_char32, halves{} + 0x0140u)};
		auto const shuffled{__builtin_bit_cast(halves,
			__builtin_ia32_pmaddubsw256(__builtin_bit_cast(utf_char32, shifted), multiply))};
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
		auto const tag{__builtin_ia32_pblendvb256(
			__builtin_bit_cast(utf_char32, halves{} + 0xc0e0u),
			__builtin_bit_cast(utf_char32, halves{} + 0x80e0u),
			__builtin_bit_cast(utf_char32, wide_flag))};
		auto const leading{shuffled | __builtin_bit_cast(halves, tag)};
#else
		auto const leading{shuffled | (small & 0x4000u) | 0x80e0u};
#endif
#else
		auto const trailing{(words & 0x003fu) | ((words & 0x007fu) << 8u) | 0x0080u};
#if defined(FAST_IO_UTF_GCC_BMP_WIDTHS)
		auto const two_byte_tag{(~wide_flag >> 9u) & 0x0040u};
#else
		auto const two_byte_tag{small & 0x0040u};
#endif
		auto const leading{((words >> 12u) | 0xe0u) |
			(((((words >> 6u) & 63u) | 0x80u) | two_byte_tag) << 8u)};
#endif
		auto const even{__builtin_bit_cast(utf_b32, __builtin_shufflevector(trailing, leading,
			0, 16, 1, 17, 2, 18, 3, 19, 8, 24, 9, 25, 10, 26, 11, 27))};
		auto const odd{__builtin_bit_cast(utf_b32, __builtin_shufflevector(trailing, leading,
			4, 20, 5, 21, 6, 22, 7, 23, 12, 28, 13, 29, 14, 30, 15, 31))};

		auto const low{row & 255u};
		auto const low_middle{(row >> 8u) & 255u};
		auto const high_middle{(row >> 16u) & 255u};
		auto const high{row >> 24u};
		auto const pack = [&](utf_b32 packet, unsigned first, unsigned second) noexcept {
			auto const a{utf_vector_load<utf_b16>(layouts.shuffle[first])};
			auto const b{utf_vector_load<utf_b16>(layouts.shuffle[second])};
			auto const indices{__builtin_shufflevector(a, b, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
				16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
			return utf_shuffle(packet, indices);
		};
		auto const first{pack(even, low, high_middle)};
		auto const second{pack(odd, low_middle, high)};
		auto const lo = [](utf_b32 bytes) noexcept {
			return __builtin_shufflevector(bytes, bytes, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
		};
		auto const hi = [](utf_b32 bytes) noexcept {
			return __builtin_shufflevector(bytes, bytes, 16, 17, 18, 19, 20, 21, 22, 23,
				24, 25, 26, 27, 28, 29, 30, 31);
		};
		auto store = [&](utf_b16 bytes, unsigned index) noexcept {
			__builtin_memcpy(unicode_address(q), &bytes, sizeof(bytes));
			q += layouts.count[index];
		};
		store(lo(first), low);
		store(lo(second), low_middle);
		store(hi(first), high_middle);
		store(hi(second), high);
		cursor += 16;
		words = next;
	}
	auto const progress{cursor != p};
	room -= static_cast<::std::size_t>(q - out);
	p = cursor;
	out = q;
	return progress;
}

#undef FAST_IO_UTF_GCC_BMP_WIDTHS

/** @brief Packs a checked mixed UTF-16 prefix and leaves the exact error and output cursors to the caller. */
template <encoding_scheme A, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_strict_mixed_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	auto const length{static_cast<::std::size_t>(last - p)};
	auto const budget{room / 3u};
	auto const count{length < budget ? length : budget};
	// Short inputs retain their existing exact-size kernels.
	if (count < 128u)
	{
		return false;
	}

#if defined(__BMI2__) && defined(__POPCNT__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pext_si) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pblendvb256)
	if (utf_bmp_pair_run<A>(p, p + count, out, room))
	{
		return true;
	}
#endif
	if (utf_bmp_mixed_run<A>(p, p + count, out, room))
	{
		return true;
	}
	constexpr unsigned window{1024};
	auto const end{utf_valid_utf16_prefix<A>(p, p + (count < window ? count : window))};
	// The validated suffix fits in the destination and its eventual stores cover all padding.
	return utf_mixed_utf16_run<A, true>(p, end, out, room);
}
#endif

/**
 * @brief Validates thirty-two fixed-width units together and emits only their complete scalars.
 * @details In UTF-16 the low-surrogate mask equals the high-surrogate mask shifted by one source lane.
 * A high surrogate in the last lane belongs to the next call. UTF-32 excludes surrogates and values
 * above U+10FFFF. Once both facts hold, no scalar requires another validity or capacity check.
 */
template <encoding_scheme A, encoding_scheme B, bool Replace = false, typename Input, typename Output>
inline bool utf_mixed_wide(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	constexpr unsigned count{32};
	constexpr unsigned worst{sizeof(T) == 1 ? (sizeof(F) == 2 ? 3u : 4u) : (sizeof(T) == 2 && sizeof(F) == 4 ? 2u : 1u)};
	if (last - p < count || room < count * worst)
	{
		return false;
	}
#if defined(__SSE2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
	[[maybe_unused]] ::std::uint_least64_t high_positions{};
	[[maybe_unused]] ::std::uint_least64_t ascii_positions{};
#endif
	unsigned n{count};
	if constexpr (sizeof(F) == 2)
	{
		utf_h8 errors{}, previous{};
		for (unsigned i{}; i != count; i += 8)
		{
			auto unit{utf_swap_units<A>(utf_vector_load<utf_h8>(p + i))};
			auto high{__builtin_bit_cast(utf_h8, (unit & 0xfc00u) == 0xd800u)};
#if defined(__SSE2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
			if constexpr (sizeof(T) == 1)
			{
				auto mask{utf_mask(__builtin_bit_cast(utf_b16, high)) & 0x5555u};
				high_positions |= static_cast<::std::uint_least64_t>(mask) << (2u * i);
				auto ascii{utf_mask(__builtin_bit_cast(utf_b16, unit < 0x80u)) & 0x5555u};
				ascii_positions |= static_cast<::std::uint_least64_t>(ascii) << (2u * i);
			}
#endif
			auto low{__builtin_bit_cast(utf_h8, (unit & 0xfc00u) == 0xdc00u)};
			errors |= low ^ __builtin_shufflevector(previous, high, 7, 8, 9, 10, 11, 12, 13, 14);
			previous = high;
		}
		if (utf_any(errors))
		{
			if constexpr (Replace && sizeof(T) == 1)
			{
				return utf_regular_utf16<A, true>(p, last, out, room);
			}
			return false;
		}
		n -= previous[7] & 1u;
	}
	else
	{
		utf_w4 errors{};
		for (unsigned i{}; i != count; i += 4)
		{
			auto unit{utf_swap_units<A>(utf_vector_load<utf_w4>(p + i))};
			errors |= __builtin_bit_cast(utf_w4, (unit > 0x10ffffu) | ((unit & 0xfffff800u) == 0xd800u));
		}
		if (utf_any(errors))
		{
			return false;
		}
	}
	auto cursor{p};
	auto end{cursor + n};
	auto q{out};
	if constexpr (sizeof(F) == 4 && sizeof(T) == 1)
	{
		while (end - cursor >= 16)
		{
			// Every validated UTF-32 scalar contributes at least one byte to the final result.
			auto values{utf_swap_units<A>(utf_vector_load<utf_w4>(cursor))};
			q += utf_encode_four<B, true>(values, q, 16u);
			cursor += 4;
		}
	}
	if constexpr (sizeof(F) == 4 && sizeof(T) == 2)
	{
		while (end - cursor >= 8)
		{
			// Eight already-validated scalars guarantee at least eight final units.
			// The following group overwrites any bytes beyond this group's logical end.
			auto values{utf_swap_units<A>(utf_vector_load<utf_w4>(cursor))};
			q += utf_encode_four<B, true>(values, q, 8u);
			cursor += 4;
		}
	}
	if constexpr (sizeof(F) == 2 && (sizeof(T) == 4 || sizeof(T) == 1))
	{
		// A leading low surrogate may belong to the preceding group. The remaining eight units guarantee four
		// UTF-32 scalars, and sixteen units guarantee at least sixteen UTF-8 bytes.
		constexpr ::std::ptrdiff_t min_units{sizeof(T) == 4 ? 9 : 17};
#if defined(__SSE2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
		if constexpr (sizeof(T) == 1)
		{
			auto period{utf_surrogate_period(high_positions)};
			// Repeated ASCII and surrogate positions favour the branch-predictable scalar packet loop.
			if (period && !((ascii_positions ^ (ascii_positions >> period)) & (~::std::uint_least64_t{} >> period)))
			{
				return utf_regular_utf16<A>(p, last, out, room);
			}
		}
#endif
		while (end - cursor >= min_units)
		{
			auto data{utf_swap_units<A>(utf_vector_load<utf_h4>(cursor))};
			auto first{__builtin_convertvector(data, utf_w4)};
			auto kind{first & 0xfc00u};
			if constexpr (sizeof(T) == 1)
			{
				if (!utf_any((first & 0xf800u) == 0xd800u))
				{
					q += utf_encode_four<B, true>(first, q, 16u);
					cursor += 4;
					continue;
				}
			}
			auto next{utf_swap_units<A>(utf_vector_load<utf_h4>(cursor + 1u))};
			auto second{__builtin_convertvector(next, utf_w4)};
			auto high{__builtin_bit_cast(utf_w4, kind == 0xd800u)};
			auto low{utf_lane_bits(kind == 0xdc00u)};
			auto pair{0x10000u + ((first & 1023u) << 10u) + (second & 1023u)};
			auto points{(high & pair) | (~high & first)};
			if constexpr (sizeof(T) == 4)
			{
				points = utf_swap_units<B>(points);
			}
			auto const &layout{utf_surrogate_layouts<(sizeof(T) == 4 || sizeof(T) == 1)>[low]};
			auto packed{utf_shuffle(__builtin_bit_cast(utf_b16, points), utf_vector_load<utf_b16>(layout.bytes))};
			if constexpr (sizeof(T) == 4)
			{
				__builtin_memcpy(unicode_address(q), &packed, sizeof(packed));
				q += layout.count;
			}
			else
			{
				// Dropped lanes become one-byte NUL placeholders at the end of the packed vector.
				auto valid{__builtin_bit_cast(utf_w4, utf_w4{0, 1, 2, 3} < layout.count)};
				q += utf_encode_four<B, true>(__builtin_bit_cast(utf_w4, packed) & valid, q, 16) - (4u - layout.count);
			}
			cursor += 4;
		}
		if (cursor != p && (unicode_load<A>(*cursor) & 0xfc00u) == 0xdc00u)
		{
			// A surrogate pair beginning in the last vector lane was already emitted.
			++cursor;
		}
	}
	if constexpr (sizeof(F) == 2 && sizeof(T) == 1)
	{
		while (cursor != end)
		{
			auto word{unicode_load<A>(*cursor++)};
			if (word < 0x80u)
			{
				*q++ = static_cast<T>(word);
			}
			else if (word < 0x800u)
			{
				auto packet{static_cast<::std::uint_least16_t>((0xc0u | (word >> 6u)) | ((0x80u | (word & 63u)) << 8u))};
				packet = ::fast_io::little_endian(packet);
				__builtin_memcpy(unicode_address(q), &packet, 2);
				q += 2;
			}
			else if ((word & 0xf800u) != 0xd800u)
			{
				*q++ = static_cast<T>(0xe0u | (word >> 12u));
				*q++ = static_cast<T>(0x80u | ((word >> 6u) & 63u));
				*q++ = static_cast<T>(0x80u | (word & 63u));
			}
			else
			{
				auto cp{0x10000u + ((word & 1023u) << 10u) + (unicode_load<A>(*cursor++) & 1023u)};
				auto packet{(0xf0u | (cp >> 18u)) | ((0x80u | ((cp >> 12u) & 63u)) << 8u) |
					((0x80u | ((cp >> 6u) & 63u)) << 16u) | ((0x80u | (cp & 63u)) << 24u)};
				packet = ::fast_io::little_endian(packet);
				__builtin_memcpy(unicode_address(q), &packet, 4);
				q += 4;
			}
		}
	}
	else
	{
		while (cursor != end)
		{
			auto cp{unicode_load<A>(*cursor++)};
			if constexpr (sizeof(F) == 2)
			{
				if ((cp & 0xfc00u) == 0xd800u)
				{
					cp = 0x10000u + ((cp & 1023u) << 10u) + (unicode_load<A>(*cursor++) & 1023u);
				}
			}
			if constexpr (sizeof(T) == 4)
			{
				*q++ = unicode_unit<B, T>(cp);
			}
			else if constexpr (sizeof(T) == 2)
			{
				if (cp < 0x10000u)
				{
					*q++ = unicode_unit<B, T>(cp);
				}
				else
				{
					unicode_emit<B>(cp, 2, q);
					q += 2;
				}
			}
			else
			{
				if (cp < 0x80u)
				{
					*q++ = static_cast<T>(cp);
					if constexpr (sizeof(F) == 2)
					{
						// This cursor excludes the trailing surrogate pending at the block boundary.
						if (cursor != end)
						{
							auto next{unicode_load<A>(*cursor)};
							if (next < 0x80u)
							{
								*q++ = static_cast<T>(next);
								++cursor;
							}
						}
					}
				}
				else if (cp < 0x800u)
				{
					unicode_emit<B>(cp, 2, q);
					q += 2;
				}
				else if (cp < 0x10000u)
				{
					unicode_emit<B>(cp, 3, q);
					q += 3;
				}
				else
				{
					unicode_emit<B>(cp, 4, q);
					q += 4;
				}
			}
		}
	}
	room -= static_cast<::std::size_t>(q - out);
	p = cursor;
	out = q;
	return true;
}

/** @brief Transcodes a run of mixed validated wide input tiles with local cursors. */
template <encoding_scheme A, encoding_scheme B, bool Replace = false, typename Input, typename Output>
inline bool utf_mixed_wide_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	auto cursor{p};
	auto destination{out};
	auto available{room};
	while (utf_mixed_wide<A, B, Replace>(cursor, last, destination, available))
	{
	}
	auto progress{cursor != p};
	p = cursor;
	out = destination;
	room = available;
	return progress;
}

/**
 * @brief Converts one vector of adjacent supplementary scalars without a shuffle table.
 * @details UTF-16 supplies matched surrogate pairs, and UTF-32 supplies values in U+10000 through U+10FFFF.
 * Each accepted vector writes a fixed number of complete scalars and leaves an exact restart cursor.
 */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_astral_block(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	constexpr unsigned count{utf_register_bytes / 4};
	constexpr unsigned in_width{4u / sizeof(F)};
	constexpr unsigned out_width{4u / sizeof(T)};
	if (last - p < count * in_width || room < count * out_width)
	{
		return false;
	}
	using W = typename utf_vector_types<count>::words;
	W cp;
	if constexpr (sizeof(F) == 2)
	{
		using H = typename utf_vector_types<2 * count>::halves;
		auto halves{utf_swap_units<A>(utf_vector_load<H>(p))};
		auto pairs{utf_swap_units<encoding_scheme::utf_le>(__builtin_bit_cast(W, utf_swap_units<encoding_scheme::utf_le>(halves)))};
		if (utf_any((pairs & 0xfc00fc00u) != 0xdc00d800u))
		{
			return false;
		}
		cp = 0x10000u + ((pairs & 1023u) << 10u) + ((pairs >> 16u) & 1023u);
	}
	else
	{
		cp = utf_swap_units<A>(utf_vector_load<W>(p));
		if (utf_any((cp < 0x10000u) | (cp > 0x10ffffu)))
		{
			return false;
		}
	}
	if constexpr (sizeof(T) == 1)
	{
		cp = utf_swap_units<encoding_scheme::utf_le>(
			(0xf0u | (cp >> 18u)) | ((0x80u | ((cp >> 12u) & 63u)) << 8u) |
			((0x80u | ((cp >> 6u) & 63u)) << 16u) | ((0x80u | (cp & 63u)) << 24u));
	}
	else if constexpr (sizeof(T) == 2)
	{
		using H = typename utf_vector_types<2 * count>::halves;
		auto value{cp - 0x10000u};
		auto packed{utf_swap_units<encoding_scheme::utf_le>(0xd800u | (value >> 10u) |
			(0xdc00u << 16u) | ((value & 1023u) << 16u))};
		// The generated halves start in little-endian wire order. Reverse each half for big-endian output.
		if constexpr (B == encoding_scheme::utf_be)
		{
			auto words{__builtin_bit_cast(H, packed)};
			cp = __builtin_bit_cast(W, (words << 8u) | (words >> 8u));
		}
		else
		{
			cp = packed;
		}
	}
	else
	{
		cp = utf_swap_units<B>(cp);
	}
	__builtin_memcpy(unicode_address(out), &cp, sizeof(cp));
	p += count * in_width;
	out += count * out_width;
	room -= count * out_width;
	return true;
}

/**
 * @brief Consumes adjacent complete supplementary vectors and publishes the resulting cursors once.
 * @details Private cursors let the vector loop keep its progress in registers across stores through the output pointer.
 * A rejected first vector leaves all three caller-owned cursors unchanged.
 */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_astral_run(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	// These locals have distinct storage. The writes cannot alias the caller's cursors.
	auto cursor{p};
	auto dest{out};
	auto available{room};
	while (utf_astral_block<A, B>(cursor, last, dest, available))
	{
	}
	if (cursor == p)
	{
		return false;
	}
	p = cursor;
	out = dest;
	room = available;
	return true;
}

/**
 * @brief Validates a short UTF-16 or UTF-32 packet and measures its UTF-8 extent in one pass.
 * @details Each unit contributes one byte plus one byte at each applicable UTF-8 width threshold.
 * A UTF-16 surrogate pair contributes four bytes. Comparing the low-surrogate mask with the
 * preceding high-surrogate mask checks adjacency across vector and scalar boundaries.
 * The caller limits the packet to 96 units, so narrow lane sums cannot overflow.
 * Zero denotes a malformed packet. The ordinary bounded decoder then locates the exact error.
 */
template <encoding_scheme Order, typename Input>
inline ::std::size_t utf_short_valid_extent(Input p, Input last) noexcept
{
	using F = unicode_cursor_value_t<Input>;
	constexpr unsigned lanes{sizeof(F) == 2 ? 8u : utf_register_bytes / 4u};
	using V = ::std::conditional_t<sizeof(F) == 2, utf_h8, typename utf_vector_types<lanes>::words>;
	V extra{}, errors{}, previous{};
	auto bytes{static_cast<::std::size_t>(last - p)};
	while (last - p >= lanes)
	{
		auto v{utf_swap_units<Order>(utf_vector_load<V>(p))};
		extra -= __builtin_bit_cast(V, v >= 0x80u) + __builtin_bit_cast(V, v >= 0x800u);
		if constexpr (sizeof(F) == 4)
		{
			extra -= __builtin_bit_cast(V, v >= 0x10000u);
			errors |= __builtin_bit_cast(V, (v > 0x10ffffu) | ((v & 0xfffff800u) == 0xd800u));
		}
		else
		{
			auto high{__builtin_bit_cast(V, (v & 0xfc00u) == 0xd800u)};
			extra += high * 2u;
			errors |= __builtin_bit_cast(V, (v & 0xfc00u) == 0xdc00u) ^
				__builtin_shufflevector(previous, high, 7, 8, 9, 10, 11, 12, 13, 14);
			previous = high;
		}
		p += lanes;
	}
	if (utf_any(errors)) return 0;
	for (unsigned i{}; i != lanes; ++i) bytes += extra[i];
	bool pending{};
	if constexpr (sizeof(F) == 2) pending = previous[7] != 0;
	for (; p != last; ++p)
	{
		auto v{unicode_load<Order>(*p)};
		bytes += (v >= 0x80u) + (v >= 0x800u);
		if constexpr (sizeof(F) == 4)
		{
			if (v > 0x10ffffu || (v & 0xfffff800u) == 0xd800u) return 0;
			bytes += v >= 0x10000u;
		}
		else
		{
			if (((v & 0xfc00u) == 0xdc00u) != pending) return 0;
			pending = (v & 0xfc00u) == 0xd800u;
			bytes -= pending * 2u;
		}
	}
	return pending ? 0u : bytes;
}

/**
 * @brief Emits a checked short packet directly into its final UTF-8 span.
 * @details The extent proof lets each scalar store its exact byte count without a capacity check.
 * An incomplete packet and a short destination leave all three cursors available to the ordinary path.
 */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_short_mixed(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F = unicode_cursor_value_t<Input>;
	using T = unicode_cursor_value_t<Output>;
	if (room < static_cast<::std::size_t>(last - p)) return false;
	auto bytes{utf_short_valid_extent<A>(p, last)};
	if (!bytes || bytes > room) return false;
	auto q{out};
	for (auto cursor{p}; cursor != last; ++cursor)
	{
		auto cp{unicode_load<A>(*cursor)};
		if (cp < 0x80u)
			*q++ = static_cast<T>(cp);
		else if (cp < 0x800u)
		{
			unicode_emit<B>(cp, 2u, q);
			q += 2;
		}
		else
		{
			if constexpr (sizeof(F) == 2)
			{
				if ((cp & 0xfc00u) == 0xd800u)
					cp = 0x10000u + ((cp & 1023u) << 10u) + (unicode_load<A>(*++cursor) & 1023u);
			}
			if (cp < 0x10000u)
			{
				unicode_emit<B>(cp, 3u, q);
				q += 3;
			}
			else
			{
				unicode_emit<B>(cp, 4u, q);
				q += 4;
			}
		}
	}
	p = last;
	out = q;
	room -= bytes;
	return true;
}

/** @brief Chooses the widening loop from the first sixteen UTF-16 code units.
 * @details A packet that exceeds the number of available UTF-32 slots needs a surrogate pair to fit.
 * A prefix made entirely of surrogate code units remains eligible for the supplementary run.
 */
template <encoding_scheme A, typename Input>
inline bool utf_widen_mixed_prefix(Input first, bool needs_pair) noexcept
{
	constexpr unsigned lanes{utf_register_bytes >= 32 ? 16u : 8u};
	using H = typename utf_vector_types<lanes>::halves;
	auto read = [](Input p) noexcept {
		auto words{utf_swap_units<A>(utf_vector_load<H>(p))};
		return __builtin_bit_cast(H, (words & 0xf800u) == 0xd800u);
	};
	auto some{read(first)}, all{some};
	for (unsigned i{lanes}; i != 16u; i += lanes)
	{
		auto marks{read(first + i)};
		some |= marks;
		all &= marks;
	}
	return utf_any(~all) && (needs_pair || utf_any(some));
}

/** @brief Selects seven UTF-16 halfwords from an eight-lane packet, omitting its low surrogate. */
inline constexpr auto utf_widen_pair_layout = [] {
	struct layout
	{
		unsigned char index[16]{};
	};
	::fast_io::freestanding::array<layout, 7> rows{};
	for (unsigned position{}; position != rows.size(); ++position)
	{
		for (unsigned lane{}; lane != 7; ++lane)
		{
			auto const source{2u * (lane + (lane > position))};
			rows[position].index[2u * lane] = static_cast<unsigned char>(source);
			rows[position].index[2u * lane + 1] = static_cast<unsigned char>(source + 1u);
		}
	}
	return rows;
}();

/** @brief Checks a complete BMP packet before writing any widened code units. */
template <unsigned Units, encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_widen_bmp_packet(Input first, Output out) noexcept
{
	constexpr unsigned lanes{utf_register_bytes >= 32u ? 16u : 8u};
	static_assert(Units % lanes == 0);
	using Half = typename utf_vector_types<lanes>::halves;
	using Wide = typename utf_vector_types<lanes>::words;
	Half words[Units / lanes];
	Half invalid{};
	for (unsigned i{}; i != Units / lanes; ++i)
	{
		auto value{utf_vector_load<Half>(first + i * lanes)};
		utf_order_lanes<A>(value);
		words[i] = value;
		invalid |= __builtin_bit_cast(Half, (value & 0xf800u) == 0xd800u);
	}
	if (utf_any(invalid))
	{
		return false;
	}
	for (unsigned i{}; i != Units / lanes; ++i)
	{
		auto converted{__builtin_convertvector(words[i], Wide)};
		utf_order_lanes<B>(converted);
		__builtin_memcpy(unicode_address(out + i * lanes), &converted, sizeof(converted));
	}
	return true;
}

/** @brief Checks all eight surrogate pairs before writing a dense supplementary packet. */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_widen_pair_packet(Input first, Output out) noexcept
{
	constexpr unsigned lanes{utf_register_bytes >= 32 ? 8u : 4u};
	using Half = typename utf_vector_types<2u * lanes>::halves;
	using Words = typename utf_vector_types<lanes>::words;
	Words data[8u / lanes], invalid{};
	for (unsigned i{}; i != 8u / lanes; ++i)
	{
		auto halves{utf_swap_units<A>(utf_vector_load<Half>(first + i * 2u * lanes))};
		auto pairs{utf_swap_units<encoding_scheme::utf_le>(__builtin_bit_cast(Words, utf_swap_units<encoding_scheme::utf_le>(halves)))};
		data[i] = pairs;
		invalid |= (pairs ^ 0xdc00d800u) & 0xfc00fc00u;
	}
	if (utf_any(invalid))
		return false;
	for (unsigned i{}; i != 8u / lanes; ++i)
	{
		auto pairs{data[i]};
		auto cp{0x10000u + ((pairs & 1023u) << 10u) + ((pairs >> 16u) & 1023u)};
		utf_order_lanes<B>(cp);
		__builtin_memcpy(unicode_address(out + i * lanes), &cp, sizeof(cp));
	}
	return true;
}

#define FAST_IO_UNICODE_FIXED_BMP_WIDEN
#define FAST_IO_UNICODE_SHORT_WIDE
/** @brief Converts a short UTF-16 prefix directly to complete UTF-32 scalars, preserving strict error positions.
 * @details Consecutive non-surrogate lanes widen together. A pair consumes two source units and one destination
 * unit. A short packet can therefore finish without returning through the general block dispatcher.
 * An invalid or incomplete pair remains at the cursor for the caller's strict or replacement policy.
 * Every commit fits the caller's remaining room, including the last unaligned wire unit. */
template <encoding_scheme A, encoding_scheme B, bool Medium = false, typename Input, typename Output>
inline bool utf_short_widen(Input &first, Input last, Output &out, ::std::size_t &room) noexcept
{
	using T = unicode_cursor_value_t<Output>;
	auto cursor{first};
	auto destination{out};
	auto available{room};
	auto append_scalar = [&]() noexcept {
		// Finish a scalar straddling the vector boundary, or leave its exact start for error handling.
		auto value{unicode_load<A>(*cursor)};
		if ((value & 0xf800u) == 0xd800u)
		{
			if ((value & 0x400u) || last - cursor < 2)
			{
				return false;
			}
			auto low{unicode_load<A>(cursor[1])};
			if ((low & 0xfc00u) != 0xdc00u)
			{
				return false;
			}
			value = 0x10000u + ((value & 1023u) << 10u) + (low & 1023u);
			++cursor;
		}
		*destination++ = unicode_unit<B, T>(value);
		++cursor;
		--available;
		return true;
	};
	auto next_block = [&]() noexcept {
		if constexpr (Medium)
		{
			return last - cursor >= 8 && available >= 8;
		}
		else
		{
			return cursor != last && available;
		}
	};
	while (next_block())
	{
		if (Medium || (last - cursor >= 8 && available >= 8))
		{
			auto words{utf_swap_units<A>(utf_vector_load<utf_h8>(cursor))};
			auto mask{utf_mask(__builtin_bit_cast(utf_b16, (words & 0xf800u) == 0xd800u))};
			// No surrogate lanes means that all eight output words can be committed together.
			if (!mask)
			{
				using wide = typename utf_vector_types<8>::words;
				auto raw{__builtin_convertvector(words, wide)};
				utf_order_lanes<B>(raw);
				__builtin_memcpy(unicode_address(destination), &raw, sizeof(raw));
				cursor += 8;
				destination += 8;
				available -= 8;
				continue;
			}
			// Exactly four adjacent mask bits describe exactly two UTF-16 code units.
			auto bit{static_cast<unsigned>(__builtin_ctz(mask))};
			if ((mask >> bit) == 15u)
			{
				auto at{bit >> 1u};
				// The mask places both units within this vector. Validate and decode them as one word.
				using halves = typename utf_vector_types<2>::halves;
				auto pair_units{utf_swap_units<A>(utf_vector_load<halves>(cursor + at))};
				auto combined{__builtin_bit_cast(unsigned, pair_units)};
				if constexpr (::std::endian::native == ::std::endian::big)
				{
					combined = (combined >> 16u) | (combined << 16u);
				}
				if ((combined & 0xfc00fc00u) == 0xdc00d800u)
				{
					using wide = typename utf_vector_types<8>::words;
					auto const &layout{utf_widen_pair_layout[at]};
					auto compact{utf_shuffle(__builtin_bit_cast(utf_b16, words), utf_vector_load<utf_b16>(layout.index))};
					auto result{__builtin_convertvector(__builtin_bit_cast(utf_h8, compact), wide)};
					auto pair{0x10000u + ((combined & 1023u) << 10u) + ((combined >> 16u) & 1023u)};
					utf_order_lanes<B>(result);
					// Overlapping four-word stores cover seven outputs. Replace the high surrogate's lane with the decoded scalar.
					auto lo{__builtin_shufflevector(result, result, 0, 1, 2, 3)};
					auto hi{__builtin_shufflevector(result, result, 3, 4, 5, 6)};
					__builtin_memcpy(unicode_address(destination), &lo, sizeof(lo));
					__builtin_memcpy(unicode_address(destination + 3), &hi, sizeof(hi));
					destination[at] = unicode_unit<B, T>(pair);
					cursor += 8;
					destination += 7;
					available -= 7;
					continue;
				}
			}
			else
			{
				if constexpr (Medium)
				{
					// The high surrogate occupies the last lane. Check its next unit before widening.
					if (mask == 0xc000u && last - cursor >= 9)
					{
						auto pair{unicode_decode<A>(cursor + 7, 2u)};
						if (pair.status == unicode_status::complete)
						{
							using wide = typename utf_vector_types<8>::words;
							auto raw{__builtin_convertvector(words, wide)};
							raw[7] = pair.value;
							utf_order_lanes<B>(raw);
							__builtin_memcpy(unicode_address(destination), &raw, sizeof(raw));
							cursor += 9;
							destination += 8;
							available -= 8;
							continue;
						}
					}
				}
				if (mask == 0xffffu && utf_astral_block<A, B>(cursor, last, destination, available))
				{
					if constexpr (utf_register_bytes == 16)
					{
						// Two 128-bit blocks can commit eight supplementary scalars before rechecking the mixed mask.
						utf_astral_block<A, B>(cursor, last, destination, available);
					}
					continue;
				}
			}
		}
		// A four-unit short block can widen as a group on targets with 128-bit registers.
		if (last - cursor >= 4 && available >= 4)
		{
			auto words{utf_swap_units<A>(utf_vector_load<utf_h4>(cursor))};
			if (!utf_any((words & 0xf800u) == 0xd800u))
			{
				auto stored{utf_swap_units<B>(__builtin_convertvector(words, utf_w4))};
				__builtin_memcpy(unicode_address(destination), &stored, sizeof(stored));
				cursor += 4;
				destination += 4;
				available -= 4;
				continue;
			}
		}
		if (!append_scalar())
		{
			break;
		}
	}
	if constexpr (Medium)
	{
		// Finish the bounded suffix without repeating the eight-lane dispatch.
		while (cursor != last && available && append_scalar())
		{
		}
	}
	first = cursor;
	out = destination;
	room = available;
	return cursor == last;
}

template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline bool utf_medium_widen(Input &first, Input last, Output &out, ::std::size_t &room) noexcept
{
	return utf_short_widen<A, B, true>(first, last, out, room);
}

/** @brief Converts validated groups directly to the available UTF-16 span. */
template <encoding_scheme A, encoding_scheme B, typename Input, typename Output>
inline bool utf_narrow_medium(Input &first, Input last, Output &out, ::std::size_t &room) noexcept
{
	auto cursor{first};
	auto dest{out};
	auto available{room};
	while (last - cursor >= 4 && available >= 4)
	{
		auto unit{utf_swap_units<A>(utf_vector_load<utf_w4>(cursor))};
		if (utf_any((unit > 0x10ffffu) | ((unit & 0xfffff800u) == 0xd800u)))
		{
			break;
		}
		auto count{utf_encode_four<B, false, true>(unit, dest, available)};
		if (!count)
		{
			break;
		}
		cursor += 4;
		dest += count;
		available -= count;
	}
	first = cursor;
	out = dest;
	room = available;
	return cursor == last;
}

/** @brief Widens or narrows an ASCII prefix, then dispatches mixed blocks through validated shuffle layouts. */
template <encoding_scheme FromOrder, encoding_scheme ToOrder, bool Replace = false, typename Input, typename Output>
inline bool unicode_block(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using From [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using To [[maybe_unused]] = unicode_cursor_value_t<Output>;
	if (static_cast<::std::size_t>(last - p) < 16u / sizeof(From) || room < 4u)
	{
		return false;
	}
	if (utf_direct_run<FromOrder, ToOrder>(p, last, out, room))
	{
		return true;
	}
	if constexpr (sizeof(From) > 1 && sizeof(To) == 1)
	{
		auto head{unicode_load<FromOrder>(*p)};
		if (head >= 0x80u && head < 0x800u && utf_bmp_run<2, FromOrder>(p, last, out, room))
		{
			return true;
		}
		if (head >= 0x800u && head < 0x10000u && (head & 0xf800u) != 0xd800u && utf_bmp_run<3, FromOrder>(p, last, out, room))
		{
			return true;
		}
	}
	if constexpr (sizeof(From) == 1)
	{
		auto lead{static_cast<unsigned char>(*p)};
		if constexpr (!Replace && sizeof(To) == 2)
		{
			if (utf_invalid_lead(lead) && last - p >= 128) [[unlikely]]
			{
				return false;
			}
		}
		if constexpr (sizeof(To) != 1)
		{
			if (lead >= 0xc2u && lead < 0xe0u && (static_cast<unsigned char>(p[2]) & 0xe0u) == 0xc0u &&
				utf_pairs_block<ToOrder>(p, last, out, room))
			{
				while (utf_pairs_block<ToOrder>(p, last, out, room))
				{
				}
				return true;
			}
			if (lead >= 0xe0u && lead < 0xf0u && (static_cast<unsigned char>(p[3]) & 0xf0u) == 0xe0u &&
				utf_fixed_input<3, ToOrder>(p, out, room))
			{
				while (last - p >= 16 && utf_fixed_input<3, ToOrder>(p, out, room))
				{
				}
				return true;
			}
			if (lead >= 0xf0u && (static_cast<unsigned char>(p[4]) & 0xf8u) == 0xf0u &&
				utf_fixed_input<4, ToOrder>(p, out, room))
			{
				while (last - p >= 16 && utf_fixed_input<4, ToOrder>(p, out, room))
				{
				}
				return true;
			}
		}
		if constexpr (sizeof(To) > 1)
		{
			if (utf_mixed_input_run<ToOrder, Replace>(p, last, out, room))
			{
				return true;
			}
		}
		auto data{utf_vector_load<utf_b16>(p)};
		auto n{static_cast<unsigned>(__builtin_ctz(utf_mask(data) | 0x10000u))};
		if (n >= 4 && room >= n)
		{
			if constexpr (sizeof(To) == 1)
			{
				__builtin_memcpy(unicode_address(out), &data, n);
			}
			else if constexpr (sizeof(To) == 2)
			{
				using wide FAST_IO_UTF_LANES(32, unsigned short) = unsigned short;
				auto v{__builtin_convertvector(data, wide)};
				if constexpr (ToOrder != encoding_scheme::utf)
				{
					v = (v << 8u) | (v >> 8u);
				}
				__builtin_memcpy(unicode_address(out), &v, n * 2);
			}
			else
			{
				using wide FAST_IO_UTF_LANES(64, unsigned) = unsigned;
				auto v{__builtin_convertvector(data, wide)};
				if constexpr (ToOrder != encoding_scheme::utf)
				{
					v = (v << 24u) | ((v << 8u) & 0xff0000u) | ((v >> 8u) & 0xff00u) | (v >> 24u);
				}
				__builtin_memcpy(unicode_address(out), &v, n * 4);
			}
			p += n;
			out += n;
			room -= n;
			return true;
		}
	}
	if constexpr (sizeof(From) > 1 && sizeof(To) == 1)
	{
		if (last - p <= 96)
		{
			// Two leading supplementary scalars keep their fixed-width vector path.
			auto first{unicode_load<FromOrder>(p[0])};
			if constexpr (sizeof(From) == 4)
			{
				if ((first < 0x10000u || unicode_load<FromOrder>(p[1]) < 0x10000u) &&
					utf_short_mixed<FromOrder, ToOrder>(p, last, out, room)) return true;
			}
			else if (((first & 0xfc00u) != 0xd800u || (unicode_load<FromOrder>(p[2]) & 0xfc00u) != 0xd800u) &&
				utf_short_mixed<FromOrder, ToOrder>(p, last, out, room)) return true;
		}
	}
	if constexpr (sizeof(From) == 2 && sizeof(To) != 2)
	{
		// A single supplementary scalar between BMP runs uses the ordinary mixed path.
		if ((unicode_load<FromOrder>(p[0]) & 0xfc00u) == 0xd800u &&
			(unicode_load<FromOrder>(p[2]) & 0xfc00u) == 0xd800u &&
			utf_astral_run<FromOrder, ToOrder>(p, last, out, room))
		{
			return true;
		}
	}
	if constexpr (sizeof(From) == 2 && sizeof(To) != 2)
	{
		// The RVV lowering of the mixed packer exceeds the cost of scalar recovery.
#if defined(__riscv_vector)
		constexpr bool scalar_replacement{Replace};
#else
		constexpr bool scalar_replacement{};
#endif
#if defined(FAST_IO_UTF_WIDE_LOOKUP)
		if constexpr (!Replace && sizeof(To) == 1)
		{
			if (utf_strict_mixed_run<FromOrder>(p, last, out, room))
			{
				return true;
			}
		}
#endif
		if constexpr (Replace && sizeof(To) == 1 && !scalar_replacement)
		{
			if (utf_mixed_utf16_run<FromOrder>(p, last, out, room))
			{
				return true;
			}
		}
		else if (utf_mixed_wide_run<FromOrder, ToOrder, scalar_replacement>(p, last, out, room))
		{
			return true;
		}
	}
	utf_w4 values;
	auto read{utf_decode_four<FromOrder>(p, values)};
	if (!read)
	{
		return false;
	}
	unsigned written{};
	if constexpr (sizeof(From) > 1 && sizeof(To) == 1)
	{
		if (!utf_any((values < 0x80u) | (values >= 0x800u)))
		{
			written = utf_fixed_output<2, ToOrder>(values, out, room);
		}
		else if (!utf_any((values < 0x800u) | (values >= 0x10000u)))
		{
			written = utf_fixed_output<3, ToOrder>(values, out, room);
		}
		else if (!utf_any(values < 0x10000u))
		{
			if constexpr (sizeof(From) == 4)
			{
				if (utf_astral_run<FromOrder, ToOrder>(p, last, out, room))
				{
					return true;
				}
			}
			written = utf_fixed_output<4, ToOrder>(values, out, room);
		}
		else
		{
			if constexpr (sizeof(From) == 4)
			{
				if (utf_mixed_wide_run<FromOrder, ToOrder>(p, last, out, room))
				{
					return true;
				}
			}
			written = utf_encode_four<ToOrder>(values, out, room);
		}
	}
	else
	{
		if constexpr (sizeof(From) > 1 && sizeof(To) > 1)
		{
			auto astral{values >= 0x10000u};
			if (!utf_any(~astral))
			{
				if constexpr (sizeof(From) != sizeof(To))
				{
					if (utf_astral_run<FromOrder, ToOrder>(p, last, out, room))
					{
						return true;
					}
				}
				written = utf_fixed_output<4, ToOrder>(values, out, room);
			}
			else if (utf_any(astral) && utf_mixed_wide_run<FromOrder, ToOrder>(p, last, out, room))
			{
				return true;
			}
			else
			{
				written = utf_encode_four<ToOrder>(values, out, room);
			}
		}
		else
		{
			written = utf_encode_four<ToOrder>(values, out, room);
		}
	}
	if (!written)
	{
		return false;
	}
	p += read;
	out += written;
	room -= written;
	return true;
}
} // namespace fast_io::details::codecvt
#include "latin_simd.h"
#undef FAST_IO_UTF_WIDE_LOOKUP
#undef FAST_IO_UTF_LANES
#undef FAST_IO_UTF_RVV_GATHER
#elif defined(__SSE2__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128) && FAST_IO_HAS_BUILTIN(__builtin_convertvector) && FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
#define FAST_IO_UNICODE_BLOCKS
namespace fast_io::details::codecvt
{
/** @brief Names same-lane types for baseline SSE2 widening and narrowing without a byte-shuffle table. */
template <unsigned N>
struct utf_sse2_vectors
{
	using b [[gnu::vector_size(N)]] = unsigned char;
	using h [[gnu::vector_size(N * 2)]] = unsigned short;
	using w [[gnu::vector_size(N * 4)]] = unsigned;
};

/** @brief Exchanges code-unit byte order in place without requiring SSSE3 byte shuffles. */
template <encoding_scheme Order, typename V>
inline void utf_sse2_order(V &v) noexcept
{
	if constexpr (Order == encoding_scheme::utf_be && sizeof(v[0]) == 2)
	{
		v = (v << 8u) | (v >> 8u);
	}
	else if constexpr (Order == encoding_scheme::utf_be && sizeof(v[0]) == 4)
	{
		v = (v << 24u) | ((v << 8u) & 0xff0000u) | ((v >> 8u) & 0xff00u) | (v >> 24u);
	}
}

/** @brief Converts full ASCII vectors and one-to-one BMP vectors on the baseline x86-64 instruction set. */
template <encoding_scheme A, encoding_scheme B, bool Replace = false, typename Input, typename Output>
inline bool unicode_block(Input &p, Input last, Output &out, ::std::size_t &room) noexcept
{
	using F [[maybe_unused]] = unicode_cursor_value_t<Input>;
	using T [[maybe_unused]] = unicode_cursor_value_t<Output>;
	constexpr unsigned n{16 / sizeof(F)};
	using lanes = utf_sse2_vectors<n>;
	using input = ::std::conditional_t<sizeof(F) == 1, typename lanes::b, ::std::conditional_t<sizeof(F) == 2, typename lanes::h, typename lanes::w>>;
	using output = ::std::conditional_t<sizeof(T) == 1, typename lanes::b, ::std::conditional_t<sizeof(T) == 2, typename lanes::h, typename lanes::w>>;
	using mask [[gnu::vector_size(16)]] = char;
	auto first{p};
	while (last - p >= static_cast<::std::ptrdiff_t>(n) && room >= n)
	{
		input v;
		__builtin_memcpy(&v, unicode_address(p), 16);
		utf_sse2_order<A>(v);
		mask bad;
		if constexpr (sizeof(F) == 1)
		{
			bad = __builtin_bit_cast(mask, v);
		}
		else if constexpr (sizeof(T) == 1)
		{
			bad = __builtin_bit_cast(mask, v > 127u);
		}
		else if constexpr (sizeof(F) == 2)
		{
			bad = __builtin_bit_cast(mask, (v >= 0xd800u) & (v <= 0xdfffu));
		}
		else
		{
			constexpr unsigned max{sizeof(T) == 2 ? 0xffffu : 0x10ffffu};
			bad = __builtin_bit_cast(mask, (v > max) | ((v >= 0xd800u) & (v <= 0xdfffu)));
		}
		if (__builtin_ia32_pmovmskb128(bad))
		{
			break;
		}
		output packed{__builtin_convertvector(v, output)};
		utf_sse2_order<B>(packed);
		__builtin_memcpy(unicode_address(out), &packed, sizeof(packed));
		p += n;
		out += n;
		room -= n;
	}
	return p != first;
}
} // namespace fast_io::details::codecvt
#endif
