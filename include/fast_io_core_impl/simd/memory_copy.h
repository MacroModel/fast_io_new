#pragma once

namespace fast_io::details
{

/** @brief Snapshots the head and tail of a bounded byte range, then stores both.
 * @details The caller proves that both source and destination extents fit.
 * The two snapshots precede both stores, so input and output may overlap.
 * The caller supplies unaligned addresses to the ordinary vector load and store.
 */
template <::std::size_t Width>
FAST_IO_GNU_ALWAYS_INLINE inline void copy_head_tail_bytes(
	void *destination_head, void *destination_tail, void const *source_head, void const *source_tail) noexcept
{
	if constexpr (::fast_io::details::can_simd_vector_run_with_cpu_instruction<Width>)
	{
		// Byte vectors retain the same representation for every integral code-unit type.
		::fast_io::intrinsics::simd_vector<unsigned char, Width> head, tail;
		head.load(source_head);
		tail.load(source_tail);
		head.store(destination_head);
		tail.store(destination_tail);
	}
	else if constexpr (Width == sizeof(::std::uint_least64_t))
	{
		::std::uint_least64_t head, tail;
		::fast_io::freestanding::my_memcpy(__builtin_addressof(head), source_head, Width);
		::fast_io::freestanding::my_memcpy(__builtin_addressof(tail), source_tail, Width);
		::fast_io::freestanding::my_memcpy(destination_head, __builtin_addressof(head), Width);
		::fast_io::freestanding::my_memcpy(destination_tail, __builtin_addressof(tail), Width);
	}
	else
	{
		::std::byte head[Width], tail[Width];
		::fast_io::freestanding::my_memcpy(head, source_head, Width);
		::fast_io::freestanding::my_memcpy(tail, source_tail, Width);
		::fast_io::freestanding::my_memcpy(destination_head, head, Width);
		::fast_io::freestanding::my_memcpy(destination_tail, tail, Width);
	}
}

#if defined(__x86_64__) && !defined(__ILP32__) && defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_assume_aligned)

/** @brief Copies four consecutive vectors from a disjoint interval. */
template <typename Vector>
FAST_IO_GNU_ALWAYS_INLINE inline void copy_disjoint_vector_tile(
	unsigned char *__restrict destination, unsigned char const *__restrict source) noexcept
{
	constexpr auto width{sizeof(Vector)};
	Vector first, second, third, fourth;
	first.load(source);
	second.load(source + width);
	third.load(source + 2u * width);
	fourth.load(source + 3u * width);
	first.store(destination);
	second.store(destination + width);
	third.store(destination + 2u * width);
	fourth.store(destination + 3u * width);
}

/** @brief Copies a disjoint block with aligned interior stores.
 * @details The caller proves an interval of at least 1024 bytes and disjoint
 * source and destination addresses. The first and last vectors cover the
 * unaligned edges. Rounding both interior boundaries to alignment leaves
 * room for at least one complete tile. The final tile may overlap a previous
 * store, which is equivalent because the source is unchanged. Every vector
 * lies entirely inside the two original intervals.
 */
FAST_IO_GNU_ALWAYS_INLINE inline void copy_disjoint_resident_bytes(
	unsigned char *__restrict destination, unsigned char const *__restrict source,
	::std::size_t bytes) noexcept
{
	using Vector = ::fast_io::intrinsics::simd_vector<unsigned char, 32u>;
	constexpr auto width{sizeof(Vector)};
	constexpr auto tile{8u * width};
	auto const address{reinterpret_cast<::std::uintptr_t>(destination)};
	auto const front{(0u - address) & (width - 1u)};
	auto const back{bytes - ((address + bytes) & (width - 1u))};
	::fast_io::details::copy_head_tail_bytes<width>(
		destination, destination + bytes - width, source, source + bytes - width);

	// Align stores in the interior and finish with one complete tile.
	auto const final_tile{back - tile};
	auto copy = [&](::std::size_t offset) {
		auto *out{static_cast<unsigned char *>(__builtin_assume_aligned(destination + offset, width))};
		auto const *in{source + offset};
		::fast_io::details::copy_disjoint_vector_tile<Vector>(out, in);
		::fast_io::details::copy_disjoint_vector_tile<Vector>(out + 4u * width, in + 4u * width);
	};
	for (auto offset{front}; offset < final_tile; offset += tile)
		copy(offset);
	copy(final_tile);
}

/** @brief Snapshots both halves of a medium block before storing either half.
 * @details Each half contains Lanes vectors and the block is between one
 * and two such halves in length. The snapshots preserve memmove behavior
 * for both directions of overlap, including overlapping head and tail.
 */
template <::std::size_t Lanes>
FAST_IO_GNU_ALWAYS_INLINE inline void move_resident_vector_ends(
	unsigned char *destination, unsigned char const *source, ::std::size_t bytes) noexcept
{
	constexpr ::std::size_t width{32u};
	using Vector = ::fast_io::intrinsics::simd_vector<unsigned char, width>;
	auto const tail{bytes - Lanes * width};
	Vector a, b, c, d, e, f, g, h;
	a.load(source);
	e.load(source + tail);
	if constexpr (Lanes >= 2u)
	{
		b.load(source + width);
		f.load(source + tail + width);
	}
	if constexpr (Lanes >= 4u)
	{
		c.load(source + 2u * width);
		d.load(source + 3u * width);
		g.load(source + tail + 2u * width);
		h.load(source + tail + 3u * width);
	}
	a.store(destination);
	e.store(destination + tail);
	if constexpr (Lanes >= 2u)
	{
		b.store(destination + width);
		f.store(destination + tail + width);
	}
	if constexpr (Lanes >= 4u)
	{
		c.store(destination + 2u * width);
		d.store(destination + 3u * width);
		g.store(destination + tail + 2u * width);
		h.store(destination + tail + 3u * width);
	}
}

/** @brief Handles dynamic copy sizes outside the short inline vector tier.
 * @details Keep AVX2 register setup and larger-copy dispatch at one shared
 * call site. The tiled strategy is restricted to cache-resident sizes and
 * disjoint addresses. On the flat x86-64 address space, both unsigned
 * differences being at least the length proves disjointness.
 */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void move_resident_bytes_avx2(void *destination, void const *source, ::std::size_t bytes) noexcept
{
	auto *out{static_cast<unsigned char *>(destination)};
	auto const *in{static_cast<unsigned char const *>(source)};
#if !defined(__GNUC__) || defined(__clang__)
	// Clang leaves the wide fallback on a shorter path when it sees the upper bound first.
	if (bytes <= 256u)
#endif
	{
		if (bytes <= 64u)
		{
			if (bytes >= 33u)
				return ::fast_io::details::move_resident_vector_ends<1u>(out, in, bytes);
			if (bytes < 8u)
			{
				if (bytes >= 4u)
					return ::fast_io::details::copy_head_tail_bytes<4u>(out, out + bytes - 4u, in, in + bytes - 4u);
				if (bytes >= 2u)
					return ::fast_io::details::copy_head_tail_bytes<2u>(out, out + bytes - 2u, in, in + bytes - 2u);
				if (bytes)
					*out = *in;
				return;
			}
		}
		else if (bytes <= 128u)
			return ::fast_io::details::move_resident_vector_ends<2u>(out, in, bytes);
		else if (bytes <= 256u)
			return ::fast_io::details::move_resident_vector_ends<4u>(out, in, bytes);
	}
	constexpr ::std::size_t minimum_tile_bytes{1024u}, maximum_tile_bytes{8192u};
	if (static_cast<::std::size_t>(bytes - minimum_tile_bytes) <= maximum_tile_bytes - minimum_tile_bytes)
	{
		auto const distance{reinterpret_cast<::std::uintptr_t>(destination) - reinterpret_cast<::std::uintptr_t>(source)};
		if (bytes <= distance && bytes <= static_cast<::std::uintptr_t>(0u - distance))
			return ::fast_io::details::copy_disjoint_resident_bytes(out, in, bytes);
	}
	::fast_io::freestanding::my_memmove(destination, source, bytes);
}

#endif

/** @brief Moves one complete contiguous block with overlapping source and destination allowed.
 * @details Two machine words cover the short scalar tier and two vectors
 * cover the range from 16 to 32 bytes. The caller proves this block is
 * processed as one read followed by one write.
 * Each access stays inside the proved extent. A compiler-known extent is
 * given to memmove directly so the backend can select its fixed-size copy.
 * The exact-count Clang AVX2 path can cover 33 to 64 bytes with two
 * 32-byte snapshots. The range gap is proved before the wider accesses.
 * Partial and EOF transfers keep the narrow inline tier so the copy
 * does not displace inlining of their outer progress dispatch.
 * Dynamic sizes beyond the inline tier can use the target's resident-copy
 * strategy. The fallback uses the ordinary memmove implementation.
 */
template <bool Exact, typename OutputChar, typename InputChar>
FAST_IO_GNU_ALWAYS_INLINE inline void move_small_contiguous_block(
	OutputChar *destination, InputChar const *source, ::std::size_t count) noexcept
{
	static_assert(sizeof(OutputChar) == sizeof(InputChar));
	constexpr ::std::size_t width{16u};
	auto const bytes{count * sizeof(InputChar)};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if (__builtin_constant_p(bytes))
	{
		::fast_io::freestanding::my_memmove(destination, source, bytes);
		return;
	}
#endif
	if constexpr (width % sizeof(InputChar) == 0u &&
		::fast_io::details::can_simd_vector_run_with_cpu_instruction<width>)
	{
		constexpr ::std::size_t word{sizeof(::std::uint_least64_t)};
		constexpr bool supports_word{word * 2u == width && word % sizeof(InputChar) == 0u};
		constexpr auto minimum{supports_word ? word : width};
#if defined(__x86_64__) && !defined(__ILP32__) && defined(__AVX2__) && defined(__clang__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_vzeroupper)
		constexpr auto maximum{Exact ? 4u * width : 2u * width};
#else
		constexpr auto maximum{2u * width};
#endif
		if (static_cast<::std::size_t>(bytes - minimum) <= maximum - minimum)
		{
			// Test the word tier before dispatching either vector width.
			if constexpr (supports_word)
			{
				if (bytes < width)
				{
					auto const offset{count - word / sizeof(InputChar)};
					::fast_io::details::copy_head_tail_bytes<word>(
						destination, destination + offset, source, source + offset);
					return;
				}
			}
#if defined(__x86_64__) && !defined(__ILP32__) && defined(__AVX2__) && defined(__clang__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_vzeroupper)
			if (Exact && bytes > 2u * width)
			{
				auto const offset{count - 2u * width / sizeof(InputChar)};
				::fast_io::details::copy_head_tail_bytes<2u * width>(
					destination, destination + offset, source, source + offset);
				// Confine upper-register cleanup to the copy that used wide vectors.
				__builtin_ia32_vzeroupper();
				return;
			}
#endif
			auto const offset{count - width / sizeof(InputChar)};
			::fast_io::details::copy_head_tail_bytes<width>(destination, destination + offset, source, source + offset);
			return;
		}
	}
#if defined(__x86_64__) && !defined(__ILP32__) && defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_assume_aligned)
	::fast_io::details::move_resident_bytes_avx2(destination, source, bytes);
#else
	::fast_io::freestanding::my_memmove(destination, source, bytes);
#endif
}

} // namespace fast_io::details
