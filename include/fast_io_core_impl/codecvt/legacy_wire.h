#pragma once
#include "gb18030_wire_ranges.h"

namespace fast_io::details::codecvt
{
template <bool FromLatin,typename Wide,encoding_scheme Order>
inline bool latin_wire_block(::std::byte const *&,::std::size_t,::std::byte *&,::std::size_t,long) noexcept { return false; }
}

namespace fast_io::transcoders
{
template <text_encoding E>
inline constexpr bool is_unicode_wire{E == text_encoding::utf8 || E == text_encoding::utf16le ||
	E == text_encoding::utf16be || E == text_encoding::utf32le || E == text_encoding::utf32be || E == text_encoding::utf32};

template <text_encoding E>
using wire_unit = ::std::conditional_t<E == text_encoding::utf16le || E == text_encoding::utf16be, char16_t,
	::std::conditional_t<E == text_encoding::utf32le || E == text_encoding::utf32be || E == text_encoding::utf32, char32_t, char8_t>>;

template <text_encoding E>
inline constexpr encoding_scheme wire_order{E == text_encoding::utf16be || E == text_encoding::utf32be ?
	encoding_scheme::utf_be : E == text_encoding::utf32 ? encoding_scheme::utf : encoding_scheme::utf_le};

namespace wire_details
{
namespace cv = ::fast_io::details::codecvt;

// IBM CCSID 37, restricted to the 128 ASCII scalar values, including controls.
inline constexpr auto ascii_to_037 = [] {
	::fast_io::freestanding::array<unsigned char,128> table{};
	for (unsigned i{}; i != 128; ++i) table[i] = ::fast_io::details::bm_i8_to_ebcdic[i];
	// UTR #16 uses the 1047 invariant layout. These four ASCII entries differ in CCSID 37.
	table[0x0a] = 0x25;
	table[0x5b] = 0xba;
	table[0x5d] = 0xbb;
	table[0x5e] = 0xb0;
	return table;
}();
inline constexpr auto ebcdic_to_ascii = [] {
	::fast_io::freestanding::array<unsigned char,256> table{};
	for (auto &entry : table) entry = 0xff;
	for (unsigned i{}; i != 128; ++i) table[ascii_to_037[i]] = static_cast<unsigned char>(i);
	return table;
}();

inline constexpr char32_t gb_four_scalar(unsigned index) noexcept
{
	if (index >= 189000) return 0x10000u + index - 189000u;
	if (index == 7457) return 0xe7c7;
	::std::size_t first{}, last{sizeof(gb_ranges) / sizeof(*gb_ranges) - 1u};
	while (last - first > 1u)
	{
		auto mid{first + (last - first) / 2u};
		if (index < gb_ranges[mid].pointer) last = mid;
		else first = mid;
	}
	return gb_ranges[first].scalar + (index - gb_ranges[first].pointer);
}

inline constexpr char32_t gb_two_scalar(unsigned index) noexcept
{
	// The shared fast_io table has seven mappings which differ from the WHATWG two-byte index.
	if (index == 6555) return 0x3000;
	if (index >= 23767)
	{
		switch (index)
		{
		case 23767: return 0xe816;
		case 23768: return 0xe817;
		case 23769: return 0xe818;
		case 23794: return 0xe831;
		case 23804: return 0xe83b;
		case 23830: return 0xe855;
		}
	}
	return cv::gb18030::lookup_gb18030_to_uni2_tb[index];
}

template <text_encoding E>
inline cv::unicode_scalar decode(::std::byte const *p, ::std::size_t n) noexcept
{
	constexpr auto ok{cv::unicode_status::complete}, bad{cv::unicode_status::invalid}, more{cv::unicode_status::incomplete};
	if constexpr (is_unicode_wire<E>)
	{
		using unit = wire_unit<E>;
		if (n < sizeof(unit)) return {0,0,more};
		auto r{cv::unicode_decode<wire_order<E>>(cv::unicode_byte_cursor<unit>(p),n / sizeof(unit))};
		r.units *= sizeof(unit);
		return r;
	}
	else
	{
		auto a{static_cast<unsigned>(*p)};
		if constexpr (E == text_encoding::latin1) return {a,1,ok};
		else if constexpr (E == text_encoding::ebcdic037_ascii)
		{
			auto cp{ebcdic_to_ascii[a]};
			return {cp,1,cp == 0xff ? bad : ok};
		}
		else
		{
			if (a < 0x80) return {a,1,ok};
			if (a == 0x80) return {0x20ac,1,ok};
			if (a == 0xff) return {0,1,bad};
			if (n < 2) return {0,1,more};
			auto b{static_cast<unsigned>(p[1])};
			if (b >= 0x40 && b != 0x7f && b <= 0xfe)
			{
				auto index{gbk_index(a,b)};
				if constexpr (E == text_encoding::gbk)
					return {cv::gb18030::lookup_gb18030_to_uni2_tb[index],2,gbk_assigned(index) ? ok : bad};
				else return {gb_two_scalar(index),2,ok};
			}
			if constexpr (E == text_encoding::gb18030)
			{
				if (b - 0x30u > 9u) return {0,2,bad};
				if (n < 3) return {0,2,more};
				auto c{static_cast<unsigned>(p[2])};
				if (c - 0x81u > 0x7du) return {0,3,bad};
				if (n < 4) return {0,3,more};
				auto d{static_cast<unsigned>(p[3])};
				if (d - 0x30u > 9u) return {0,4,bad};
				auto index{((a - 0x81u) * 10u + (b - 0x30u)) * 1260u + (c - 0x81u) * 10u + (d - 0x30u)};
				if ((index > 39419u && index < 189000u) || index > 1237575u) return {0,4,bad};
				return {gb_four_scalar(index),4,ok};
			}
			else return {0,2,bad};
		}
	}
}

inline unsigned gb_encode(char32_t cp, ::std::byte *out) noexcept
{
	unsigned code{};
	if (cp >= 0x10000)
	{
		auto n{cp - 0x10000u + 189000u};
		out[3] = static_cast<::std::byte>(0x30 + n % 10); n /= 10;
		out[2] = static_cast<::std::byte>(0x81 + n % 126); n /= 126;
		out[1] = static_cast<::std::byte>(0x30 + n % 10);
		out[0] = static_cast<::std::byte>(0x81 + n / 10);
		return 4;
	}
	if (cp >= 0x4e00 && cp < 0x9fa6)
	{
		auto v{cv::gb18030::lookup_uni_to_gb18030_tb[cp - gbk_cjk_table_offset]};
		out[0] = static_cast<::std::byte>(v & 255u);
		out[1] = static_cast<::std::byte>(v >> 8u);
		return 2;
	}
	if (cp == 0xe5e5) return 0;
	if (cp == 0x1e3f) code = 0xa8bc;
	else if (cp >= 0x9fb4 && cp <= 0x9fbb)
	{
		constexpr unsigned char trails[]{0x59,0x61,0x66,0x67,0x6d,0x7e,0x90,0xa0};
		code = 0xfe00u + trails[cp-0x9fb4u];
	}
	else if (cp >= 0xfe10 && cp <= 0xfe19)
	{
		constexpr unsigned char trails[]{0xd9,0xdb,0xda,0xdc,0xdd,0xde,0xdf,0xec,0xed,0xf3};
		code = 0xa600u + trails[cp-0xfe10u];
	}
	else return static_cast<unsigned>(cv::gb18030::get_gb18030_code_units(cp,reinterpret_cast<char *>(out)));
	out[0] = static_cast<::std::byte>(code >> 8u);
	out[1] = static_cast<::std::byte>(code & 255u);
	return 2;
}

template <text_encoding E>
inline unsigned encode(char32_t cp, ::std::byte *out) noexcept
{
	if constexpr (is_unicode_wire<E>)
	{
		using unit = wire_unit<E>;
		auto width{cv::unicode_width<unit>(cp)};
		cv::unicode_emit<wire_order<E>>(cp,width,cv::unicode_byte_cursor<unit>(out));
		return width * sizeof(unit);
	}
	else if constexpr (E == text_encoding::ebcdic037_ascii)
	{
		if (cp >= 128) return 0;
		*out = static_cast<::std::byte>(ascii_to_037[cp]);
		return 1;
	}
	else if (cp < (E == text_encoding::latin1 ? 256u : 128u))
	{
		*out = static_cast<::std::byte>(cp);
		return 1;
	}
	else if constexpr (E == text_encoding::latin1) return 0;
	else if constexpr (E == text_encoding::gb18030) return gb_encode(cp,out);
	else
	{
		if (cp == 0x20ac) { *out = ::std::byte{0x80}; return 1; }
		char gb[4];
		if (cp > 0xffff || cv::gb18030::get_gb18030_code_units(cp,gb) != 2 ||
			!gbk_assigned(gbk_index(static_cast<unsigned char>(gb[0]),static_cast<unsigned char>(gb[1])))) return 0;
		::fast_io::freestanding::my_memcpy(out,gb,2);
		return 2;
	}
}
} // namespace wire_details

/** @brief Converts each scalar directly into the destination encoding, retaining at most three incomplete input bytes. */
template <text_encoding From, text_encoding To>
struct legacy_wire
{
	/** @brief Permits the serial async driver to suspend between bounded steps while this engine retains its state. */
	template <bool Input>
	friend constexpr ::std::true_type async_transcode_step_define(::fast_io::transcode_reserve_t<legacy_wire>, ::std::bool_constant<Input>) noexcept
	{
		return {};
	}

	using from_value_type = ::std::byte;
	using to_value_type = ::std::byte;
	::std::byte tail[4]{};
	unsigned used{};
};

template <text_encoding F, text_encoding T>
inline constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<legacy_wire<F,T>>) noexcept
{
	constexpr auto width{::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr auto batch{width ? 2u * width : 4u * sizeof(::std::size_t)};
	return {batch, batch * sizeof(wire_unit<T>) / sizeof(wire_unit<F>)};
}

template <text_encoding F, text_encoding T>
inline constexpr ::std::size_t transcode_min_output_size_define(transcode_reserve_t<legacy_wire<F,T>>,transcode_phase) noexcept
{
	if constexpr (T == text_encoding::gbk) return 2;
	if constexpr (T == text_encoding::latin1 || T == text_encoding::ebcdic037_ascii) return 1;
	return 4;
}

template <text_encoding F, text_encoding T>
inline basic_transcode_process_result<::std::byte,::std::byte> transcode_process_define(legacy_wire<F,T> &s,
	::std::byte const *p, ::std::byte const *end, ::std::byte *out, ::std::byte *limit)
{
	using namespace wire_details;
	while (p != end)
	{
		if (out == limit) return {p,out,transcode_step_status::need_output};
		auto n{static_cast<::std::size_t>(end-p)};
		auto room{static_cast<::std::size_t>(limit-out)};
		if constexpr ((F == text_encoding::latin1 && is_unicode_wire<T>) ||
			(T == text_encoding::latin1 && is_unicode_wire<F>))
		{
			constexpr auto unicode{F == text_encoding::latin1 ? T : F};
			if (!s.used && cv::latin_wire_block<F == text_encoding::latin1,wire_unit<unicode>,wire_order<unicode>>(p,n,out,room,0)) continue;
		}
		if constexpr (sizeof(wire_unit<F>) == 1 && sizeof(wire_unit<T>) == 1 &&
			F != text_encoding::ebcdic037_ascii && T != text_encoding::ebcdic037_ascii)
		{
			if (!s.used && static_cast<unsigned>(*p) < 128u)
			{
				constexpr auto width{::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
				if constexpr (width)
				{
					while (n >= width && room >= width)
					{
						::fast_io::intrinsics::simd_vector<unsigned char,width ? width : 16u> v;
						v.load(p);
						if (!::fast_io::intrinsics::is_all_zeros(v >> 7u)) break;
						v.store(out);
						p += width; out += width; n -= width; room -= width;
					}
				}
				while (p != end && out != limit && static_cast<unsigned>(*p) < 128u) *out++ = *p++;
				continue;
			}
		}
		auto *input{p};
		auto available{n};
		if (s.used)
		{
			auto addition{n < 4u - s.used ? n : 4u - s.used};
			::fast_io::freestanding::my_memcpy(s.tail + s.used,p,addition);
			input = s.tail;
			available = s.used + addition;
		}
		auto scalar{decode<F>(input,available)};
		if (scalar.status == cv::unicode_status::invalid) throw_parse_code(parse_code::invalid);
		if (scalar.status == cv::unicode_status::incomplete)
		{
			if (!s.used) ::fast_io::freestanding::my_memcpy(s.tail,p,n);
			s.used = static_cast<unsigned>(available);
			p = end;
			break;
		}
		::std::byte small[4];
		auto *dest{room >= sizeof(small) ? out : small};
		auto width{encode<T>(scalar.value,dest)};
		if (!width) throw_parse_code(parse_code::invalid);
		if (width > room) return {p,out,transcode_step_status::need_output};
		if (dest == small) ::fast_io::freestanding::my_memcpy(out,small,width);
		p += scalar.units - s.used;
		s.used = 0;
		out += width;
	}
	return {p,out,transcode_step_status::need_input};
}

template <text_encoding F, text_encoding T>
inline basic_transcode_drain_result<::std::byte> transcode_finish_define(legacy_wire<F,T> &s,::std::byte *p,::std::byte *)
{
	if (s.used) throw_parse_code(parse_code::invalid);
	return {p,transcode_drain_status::complete};
}
} // namespace fast_io::transcoders
