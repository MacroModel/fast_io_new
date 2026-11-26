#pragma once
namespace fast_io::details::codecvt
{
/**
 * @brief Generates the exact-store packing controls required by one Latin-1 conversion direction.
 * @details A valid eight-position block produces between half a vector and a whole vector.
 * The first half of its shuffle holds the first output bytes. The second half holds the last
 * output bytes. Two overlapping, constant-width stores then end exactly at the reported cursor.
 * Each direction instantiates one table. Encoding uses 4 KiB and decoding uses 2 KiB.
 */
template <bool Encode>
inline constexpr auto latin_layouts = [] {
	constexpr unsigned width{Encode ? 16u : 8u};
	::fast_io::freestanding::array<::fast_io::freestanding::array<unsigned char,width>,256> result{};
	for (unsigned mask{};mask!=256u;++mask)
	{
		auto &entry{result[mask]};
		unsigned char packed[width]{};
		unsigned pos{};
		for (unsigned i{};i!=8u;++i)
		{
			if constexpr (Encode)
			{
				packed[pos++]=static_cast<unsigned char>(2u*i);
				if ((mask>>i)&1u) packed[pos++]=static_cast<unsigned char>(2u*i+1u);
			}
			else if (!((mask>>i)&1u)) packed[pos++]=static_cast<unsigned char>(i);
		}
		// Invalid lead masks never reach the decoder. Valid blocks produce at least half a vector.
		if (pos>=width/2u)
			for (unsigned i{};i!=width/2u;++i)
			{
				entry[i]=packed[i];
				entry[i+width/2u]=packed[pos-width/2u+i];
			}
	}
	return result;
}();

/** @brief Decodes an entire fixed-width run of UTF-8 Latin-1 pairs and checks all prefix and continuation bits. */
template <unsigned Bytes>
inline bool latin_decode_pairs(::std::byte const *p, ::std::byte *out) noexcept
{
	using wide = typename utf_vector_types<Bytes/2u>::halves;
	using narrow = typename utf_vector_types<Bytes/2u>::bytes;
	auto packed{utf_vector_load<wide>(p)};
	utf_order_lanes<encoding_scheme::utf_le>(packed);
	if (utf_any((packed & 0xc0feu) != 0x80c2u)) return false;
	auto value{__builtin_convertvector((packed >> 8u) | ((packed & 1u) << 6u),narrow)};
	__builtin_memcpy(out,&value,sizeof(value));
	return true;
}

/** @brief Emits sixteen non-ASCII Latin-1 values as two-byte UTF-8 words in memory order. */
inline void latin_encode_pairs(utf_b16 bytes, ::std::byte *out) noexcept
{
	auto put=[](auto v,auto dst) {
		auto halves{__builtin_convertvector(v,utf_h8)};
		auto packed{(0xc0u | (halves >> 6u)) | (((halves & 63u) | 128u) << 8u)};
		utf_order_lanes<encoding_scheme::utf_le>(packed);
		__builtin_memcpy(dst,&packed,sizeof(packed));
	};
	put(__builtin_shufflevector(bytes,bytes,0,1,2,3,4,5,6,7),out);
	put(__builtin_shufflevector(bytes,bytes,8,9,10,11,12,13,14,15),out+16);
}

/** @brief Processes complete, representable Latin-1 vectors. The scalar path handles the first failing or partial vector. */
template <bool FromLatin,typename Wide,encoding_scheme Order>
inline bool latin_wire_block(::std::byte const *&p, ::std::size_t n, ::std::byte *&out, ::std::size_t room,int) noexcept
{
	auto *src{p};
	auto *dst{out};
	if constexpr (sizeof(Wide) != 1)
	{
		constexpr unsigned lanes{utf_register_bytes / sizeof(Wide)};
		using narrow = typename utf_vector_types<lanes>::bytes;
		using wide = ::std::conditional_t<sizeof(Wide)==2,typename utf_vector_types<lanes>::halves,typename utf_vector_types<lanes>::words>;
		constexpr unsigned source{FromLatin ? lanes : lanes*sizeof(Wide)}, dest{FromLatin ? lanes*sizeof(Wide) : lanes};
		auto blocks{n / source < room / dest ? n / source : room / dest};
		if constexpr (FromLatin)
		{
			for (; blocks; --blocks,src += source,dst += dest)
			{
				auto value{__builtin_convertvector(utf_vector_load<narrow>(src),wide)};
				utf_order_lanes<Order>(value);
				__builtin_memcpy(dst,&value,sizeof(value));
			}
		}
		else
		{
			auto load = [](auto at) {
				auto v{utf_vector_load<wide>(at)};
				utf_order_lanes<Order>(v);
				return v;
			};
			for (; blocks >= 4; blocks -= 4,src += 4*source,dst += 4*dest)
			{
				auto a{load(src)}, b{load(src+source)}, c{load(src+2*source)}, d{load(src+3*source)};
				if (utf_any((a | b | c | d) > 255u)) break;
				if constexpr (sizeof(Wide) == 4)
				{
					using bytes = typename utf_vector_types<4*lanes>::bytes;
					auto value{__builtin_bit_cast(bytes,a | (b<<8u) | (c<<16u) | (d<<24u))};
					auto packed{[&]<::std::size_t... I>(::std::index_sequence<I...>) {
						return __builtin_shufflevector(value,value,(4*(I%lanes)+(std::endian::native==std::endian::big ? 3-I/lanes : I/lanes))...);
					}(::std::make_index_sequence<4*lanes>{})};
					__builtin_memcpy(dst,&packed,sizeof(packed));
				}
				else
				{
					auto put = [](auto at,wide v) {
						auto packed{__builtin_convertvector(v,narrow)};
						__builtin_memcpy(at,&packed,sizeof(packed));
					};
					put(dst,a); put(dst+dest,b); put(dst+2*dest,c); put(dst+3*dest,d);
				}
			}
			for (; blocks; --blocks,src += source,dst += dest)
			{
				auto v{load(src)};
				if (utf_any(v > 255u)) break;
				auto packed{__builtin_convertvector(v,narrow)};
				__builtin_memcpy(dst,&packed,sizeof(packed));
			}
		}
	}
	else
	{
		while (n >= 16 && room >= (FromLatin ? 16u : 8u))
		{
			auto bytes{utf_vector_load<utf_b16>(src)};
			auto mask{utf_mask(bytes)};
			if (!mask)
			{
				if (room<16u) break;
				// Four ASCII vectors can share one high-bit check before copying all 64 bytes.
				if (n>=64u && room>=64u)
				{
					auto b{utf_vector_load<utf_b16>(src+16)},c{utf_vector_load<utf_b16>(src+32)},d{utf_vector_load<utf_b16>(src+48)};
					if (!utf_mask(b|c|d))
					{
						__builtin_memcpy(dst,&bytes,16); __builtin_memcpy(dst+16,&b,16);
						__builtin_memcpy(dst+32,&c,16); __builtin_memcpy(dst+48,&d,16);
						src+=64; dst+=64; n-=64; room-=64;
						continue;
					}
				}
				__builtin_memcpy(dst,&bytes,16);
				src += 16; dst += 16; n -= 16; room -= 16;
				continue;
			}
			// A complete high-byte block can bypass the mixed-layout tables.
			if (mask==65535u)
			{
				if constexpr (FromLatin)
				{
					if (room>=32u)
					{
						latin_encode_pairs(bytes,dst);
						src+=16; dst+=32; n-=16; room-=32;
						continue;
					}
				}
				else
				{
					if constexpr (utf_register_bytes>=32u)
					{
						// A double-width load still requires a complete destination span.
						if (n>=32u && room>=16u && latin_decode_pairs<32>(src,dst))
						{
							src+=32; dst+=16; n-=32; room-=16;
							continue;
						}
					}
					if (latin_decode_pairs<16>(src,dst))
					{
						src+=16; dst+=8; n-=16; room-=8;
						continue;
					}
				}
			}
			unsigned read{}, written{};
			if constexpr (FromLatin)
			{
				auto first{__builtin_shufflevector(bytes,bytes,0,1,2,3,4,5,6,7)};
				auto v{__builtin_convertvector(first,utf_h8)};
				auto high{__builtin_bit_cast(utf_h8,v >= 128u)};
				auto pairs{(v & ~high) | (high & ((0xc0u | (v >> 6u)) | (((v & 63u) | 128u) << 8u)))};
				utf_order_lanes<encoding_scheme::utf_le>(pairs);
				auto const &layout{latin_layouts<FromLatin>[mask & 255u]};
				auto result{utf_shuffle(__builtin_bit_cast(utf_b16,pairs),utf_vector_load<utf_b16>(layout.data()))};
				read = 8; written = 8u + static_cast<unsigned>(__builtin_popcount(mask & 255u));
				// Two overlapping eight-byte stores cover exactly the eight to sixteen produced bytes.
				__builtin_memcpy(dst,&result,8);
				auto suffix{__builtin_shufflevector(result,result,8,9,10,11,12,13,14,15)};
				__builtin_memcpy(dst+written-8u,&suffix,8);
			}
			else
			{
				auto lead{__builtin_bit_cast(utf_b16,(bytes & 0xfeu) == 0xc2u)};
				auto continuation{__builtin_bit_cast(utf_b16,(bytes & 0xc0u) == 0x80u)};
				// One pair of masks proves that all sixteen bytes encode Latin-1.
				auto all_heads{utf_mask(lead)},all_tails{utf_mask(continuation)};
				auto prev{__builtin_shufflevector(bytes,bytes,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14)};
				auto value{bytes | ((prev & 1u) << 6u & continuation)};
				auto heads{all_heads&255u};
				auto const &layout{latin_layouts<FromLatin>[heads]};
				using half = typename utf_vector_types<8>::bytes;
				auto low{utf_vector_load<half>(layout.data())};
				auto first_len{8u-static_cast<unsigned>(__builtin_popcount(heads))};
				auto count{16u-static_cast<unsigned>(__builtin_popcount(all_heads))};
				if (room>=count && all_tails==((all_heads<<1u)&65535u) && mask==(all_heads|all_tails))
				{
					// One shuffle packs both halves, including a pair crossing their internal boundary.
					auto high{utf_vector_load<half>(latin_layouts<FromLatin>[all_heads>>8u].data())+8u};
					auto indexes{__builtin_shufflevector(low,high,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)};
					auto packed{utf_shuffle(value,indexes)};
					auto y{__builtin_shufflevector(packed,packed,4,5,6,7)};
					auto z{__builtin_shufflevector(packed,packed,8,9,10,11)};
					auto w{__builtin_shufflevector(packed,packed,12,13,14,15)};
					__builtin_memcpy(dst,&packed,4); __builtin_memcpy(dst+first_len-4u,&y,4);
					__builtin_memcpy(dst+first_len,&z,4); __builtin_memcpy(dst+count-4u,&w,4);
					read=16u-(all_heads>>15u); written=count;
				}
				else
				{
					// Commit a valid first half when a later error or a short destination stops the full block.
					auto tails{all_tails&255u};
					if (tails!=((heads<<1u)&255u) || ((mask&255u)!=(heads|tails))) break;
					auto indices{__builtin_shufflevector(low,low,0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7)};
					auto packed{utf_shuffle(value,indices)};
					auto suffix{__builtin_shufflevector(packed,packed,4,5,6,7)};
					read=8u-(heads>>7u); written=first_len;
					__builtin_memcpy(dst,&packed,4); __builtin_memcpy(dst+written-4u,&suffix,4);
				}
			}
			src += read; dst += written; n -= read; room -= written;
		}
	}
	auto advanced{src != p};
	p = src;
	out = dst;
	return advanced;
}
} // namespace fast_io::details::codecvt
