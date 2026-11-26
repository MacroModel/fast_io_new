#pragma once

/** @file @brief Directional decorators for wire-encoding conversion. */
#include "fast_io_utf.h"
#include "fast_io_unit/gb18030.h"

namespace fast_io
{
/** @brief Names physical byte encodings independently of the stream's public character width. */
enum class text_encoding : unsigned char
{
	utf8, gbk, utf16le, utf16be, utf32le, utf32be, gb18030, latin1, ebcdic037_ascii,
	utf32 = ::std::endian::native == ::std::endian::big ? utf32be : (::std::endian::native == ::std::endian::little ? static_cast<unsigned char>(utf32le) : 9u)
};

namespace transcoders
{
/** @brief Tests the assigned two-byte repertoire of Microsoft's published CP936 version 2.01.
 * @details The existing GB18030 two-byte table agrees on all assigned CP936 entries. These
 * unassigned index ranges exclude later GB18030 additions. A common CJK range needs no search.
 */
inline constexpr bool gbk_assigned(unsigned index) noexcept
{
	constexpr unsigned short excluded[][2]{
		{6080,6175},{6270,6365},{6376,6381},{6432,6433},{6444,6445},{6458,6555},{6650,6745},{6829,6935},
		{7022,7125},{7150,7157},{7182,7188},{7201,7202},{7208,7208},{7211,7315},{7349,7363},{7397,7409},
		{7495,7505},{7533,7533},{7536,7536},{7538,7541},{7579,7599},{7624,7624},{7627,7627},{7629,7631},
		{7672,7684},{7686,7698},{7775,7789},{7886,7979},{8076,8169},{8266,8359},{8456,8549},{8646,8739},
		{8836,8929},{16525,16529},{22706,22799},{22896,22989},{23086,23179},{23276,23369},{23466,23559},
		{23656,23749},{23766,23939}};
	if (index < 6080u || (index > 8929u && index < 16525u) || (index > 16529u && index < 22706u)) return true;
	unsigned first{}, last{sizeof(excluded)/sizeof(*excluded)};
	while (first < last)
	{
		auto mid{first + (last - first) / 2u};
		if (excluded[mid][1] < index) first = mid + 1;
		else last = mid;
	}
	return first == sizeof(excluded)/sizeof(*excluded) || index < excluded[first][0];
}

/** @brief Converts a well-formed GBK lead and trail into their shared dense two-byte table index. */
inline constexpr unsigned gbk_index(unsigned lead, unsigned trail) noexcept
{
	return (lead - 0x81u) * 190u + trail - 0x40u - (trail > 0x7fu);
}

/** @brief GBK and GB18030 share the original contiguous block of CJK unified ideographs. */
inline constexpr char32_t gbk_cjk_table_offset = [] {
	char32_t skipped{0x80u};
	for (auto const &range : ::fast_io::details::codecvt::gb18030::gb18030_ranges)
	{
		if (range[1] > 0x4e00u) break;
		skipped += range[1] - range[0];
	}
	return skipped;
}();

/** @brief Converts UTF-8 and strict GBK/CP936 with at most one incomplete scalar retained per stream.
 * @details The GBK variant accepts ASCII, the single-byte euro sign and assigned two-byte sequences.
 * Malformed UTF-8, unassigned GBK codes, unrepresentable scalars and a truncated final scalar fail.
 */
template <bool ToGbk>
struct utf8_gbk_engine
{
	/** @brief Permits the serial async driver to suspend between bounded steps while this engine retains its state. */
	template <bool Input>
	friend constexpr ::std::true_type async_transcode_step_define(::fast_io::transcode_reserve_t<utf8_gbk_engine>, ::std::bool_constant<Input>) noexcept
	{
		return {};
	}

	using from_value_type = ::std::byte;
	using to_value_type = ::std::byte;
	unsigned char pending[4]{};
	unsigned used{};
};

/** @brief Batch two native ASCII vectors on short fields while lending longer text as a single range. */
template <bool ToGbk>
inline constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<utf8_gbk_engine<ToGbk>>) noexcept
{
	constexpr auto width{::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr auto batch{width ? 2u * width : 4u * sizeof(::std::size_t)};
	return {batch, batch};
}

/** @brief Reserves enough room to publish any one representable scalar atomically. */
template <bool ToGbk>
inline constexpr ::std::size_t transcode_min_output_size_define(transcode_reserve_t<utf8_gbk_engine<ToGbk>>, transcode_phase) noexcept
{ return ToGbk ? 2u : 3u; }

/** @brief Converts complete scalars in stream order and retains an unfinished input prefix until refill. */
template <bool ToGbk>
inline basic_transcode_process_result<::std::byte, ::std::byte> transcode_process_define(utf8_gbk_engine<ToGbk> &s,
	::std::byte const *p, ::std::byte const *end, ::std::byte *out, ::std::byte *limit)
{
	namespace cv = ::fast_io::details::codecvt;
	while (p != end)
	{
		// The common ASCII prefix preserves code points and wire representations in both directions.
		if (!s.used && static_cast<unsigned>(*p) < 0x80u)
		{
			constexpr auto width{::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
			if constexpr (width != 0)
			{
				while (static_cast<::std::size_t>(end - p) >= width && static_cast<::std::size_t>(limit - out) >= width)
				{
					::fast_io::intrinsics::simd_vector<unsigned char, width ? width : 16u> block;
					block.load(p);
					if (!::fast_io::intrinsics::is_all_zeros(block >> 7u))
					{
						break;
					}
					block.store(out);
					p += width;
					out += width;
				}
			}
			while (p != end && out != limit && static_cast<unsigned>(*p) < 0x80u)
			{
				*out++ = *p++;
			}
		}
		if (p == end) break;
		if (static_cast<::std::size_t>(limit - out) < (ToGbk ? 2u : 3u))
			return {p, out, transcode_step_status::need_output};
		if constexpr (ToGbk)
		{
			// Three-byte UTF-8 for the CP936 unified ideographs is a dense two-byte lookup.
			// Check both continuation bytes before indexing the shared table.
			if (!s.used)
			{
				while (end - p >= 3 && limit - out >= 2)
				{
					auto const a{static_cast<unsigned>(p[0])};
					auto const b{static_cast<unsigned>(p[1])};
					auto const c{static_cast<unsigned>(p[2])};
					if (a - 0xe4u > 5u || ((b ^ 0x80u) | (c ^ 0x80u)) >= 64u) break;
					auto const cp{((a & 15u) << 12u) | ((b & 63u) << 6u) | (c & 63u)};
					if (cp - 0x4e00u >= 0x9fa6u - 0x4e00u) break;
					auto const code{cv::gb18030::lookup_uni_to_gb18030_tb[cp - gbk_cjk_table_offset]};
					out[0] = static_cast<::std::byte>(code & 0xffu);
					out[1] = static_cast<::std::byte>(code >> 8u);
					p += 3;
					out += 2;
				}
				if (p == end) break;
				if (limit - out < 2) return {p, out, transcode_step_status::need_output};
				if (static_cast<unsigned>(*p) < 0x80u) continue;
			}
			cv::unicode_scalar scalar{};
			if (s.used)
			{
				for (;;)
				{
					scalar = cv::unicode_decode<encoding_scheme::utf>(s.pending, s.used);
					if (scalar.status != cv::unicode_status::incomplete || p == end) break;
					s.pending[s.used++] = static_cast<unsigned char>(*p++);
				}
				if (scalar.status == cv::unicode_status::incomplete) break;
			}
			else
			{
				// Contiguous scalars are decoded once. Only a split scalar crosses the call boundary.
				auto const remaining{static_cast<::std::size_t>(end - p)};
				scalar = cv::unicode_decode<encoding_scheme::utf>(reinterpret_cast<unsigned char const *>(p), remaining);
				if (scalar.status == cv::unicode_status::incomplete)
				{
					::fast_io::freestanding::my_memcpy(s.pending, p, remaining);
					s.used = static_cast<unsigned>(remaining);
					p = end;
					break;
				}
				p += scalar.units;
			}
			if (scalar.status != cv::unicode_status::complete) throw_parse_code(parse_code::invalid);
			if (scalar.value >= 0x4e00u && scalar.value < 0x9fa6u)
			{
				auto const code{cv::gb18030::lookup_uni_to_gb18030_tb[scalar.value - gbk_cjk_table_offset]};
				*out++ = static_cast<::std::byte>(code & 0xffu);
				*out++ = static_cast<::std::byte>(code >> 8u);
			}
			else if (scalar.value == 0x20acu) *out++ = ::std::byte{0x80};
			else
			{
				char gb[4];
				if (scalar.value > 0xffffu || cv::gb18030::get_gb18030_code_units(static_cast<char32_t>(scalar.value), gb) != 2u ||
					!gbk_assigned(gbk_index(static_cast<unsigned char>(gb[0]), static_cast<unsigned char>(gb[1]))))
					throw_parse_code(parse_code::invalid);
				*out++ = static_cast<::std::byte>(static_cast<unsigned char>(gb[0]));
				*out++ = static_cast<::std::byte>(static_cast<unsigned char>(gb[1]));
			}
		}
		else
		{
			if (!s.used)
			{
				// Both GB2312 ideograph levels map to three-byte UTF-8. The last five
				// positions of row D7 are unassigned. Other CP936 rows use the general lookup.
				while (end - p >= 2 && limit - out >= 3)
				{
					auto const lead{static_cast<unsigned>(p[0])};
					auto const trail{static_cast<unsigned>(p[1])};
					if (lead - 0xb0u > 0xf7u - 0xb0u || trail - 0xa1u > 0xfeu - 0xa1u ||
						(lead == 0xd7u && trail >= 0xfau))
					{
						break;
					}
					auto const scalar{cv::gb18030::lookup_gb18030_to_uni2_tb[gbk_index(lead, trail)]};
					cv::unicode_emit<encoding_scheme::utf>(scalar, 3u, reinterpret_cast<char *>(out));
					p += 2;
					out += 3;
				}
				if (p == end) break;
				if (limit - out < 3) return {p, out, transcode_step_status::need_output};
				if (static_cast<unsigned>(*p) < 0x80u) continue;
			}
			if (!s.used) s.pending[s.used++] = static_cast<unsigned char>(*p++);
			auto const lead{s.pending[0]};
			char32_t scalar{0x20ac};
			if (lead != 0x80u)
			{
				if (lead < 0x81u || lead > 0xfeu) throw_parse_code(parse_code::invalid);
				if (p == end) break;
				auto trail{static_cast<unsigned>(*p++)};
				if (trail < 0x40u || trail == 0x7fu || trail > 0xfeu) throw_parse_code(parse_code::invalid);
				auto index{gbk_index(lead, trail)};
				if (!gbk_assigned(index)) throw_parse_code(parse_code::invalid);
				scalar = cv::gb18030::lookup_gb18030_to_uni2_tb[index];
			}
			auto width{cv::unicode_width<char>(scalar)};
			cv::unicode_emit<encoding_scheme::utf>(scalar, width, reinterpret_cast<char *>(out));
			out += width;
		}
		s.used = 0;
	}
	return {p, out, transcode_step_status::need_input};
}

/** @brief Rejects a final partial lead byte or UTF-8 scalar without synthesizing replacement text. */
template <bool ToGbk>
inline basic_transcode_drain_result<::std::byte> transcode_finish_define(utf8_gbk_engine<ToGbk> &s, ::std::byte *p, ::std::byte *)
{
	if (s.used) throw_parse_code(parse_code::invalid);
	return {p, transcode_drain_status::complete};
}
} // namespace transcoders

} // namespace fast_io

#include "fast_io_core_impl/codecvt/legacy_wire.h"

namespace fast_io
{
namespace details
{
template <text_encoding From, text_encoding To, transcoders::utf_error_policy Policy = transcoders::utf_error_policy::strict>
consteval auto wire_conversion()
{
	using namespace transcoders;
	if constexpr ((From == text_encoding::utf8 && To == text_encoding::gbk) ||
		(From == text_encoding::gbk && To == text_encoding::utf8))
		return utf8_gbk_engine<To == text_encoding::gbk>{};
	else if constexpr (is_unicode_wire<From> && is_unicode_wire<To>)
		return utf_wire<wire_unit<From>, wire_unit<To>, wire_order<From>, wire_order<To>, Policy>{};
	else
		return legacy_wire<From, To>{};
}
} // namespace details

/** @brief Applies From-to-To conversion in the direction in which data flows through the decorator.
 * @details Duplex handles keep independent encoder and decoder state. An output consumes From and
 * writes To to its child. An input reads From from its child and publishes To to the caller.
 * Unicode wire pairs accept an optional error policy. Strict conversion reports malformed input.
 * Replacement emits U+FFFD for each maximal subpart of an ill-formed sequence and resumes at the
 * next undecoded unit. A partial sequence remains pending until another block or terminal finish.
 */
template <text_encoding From, text_encoding To, transcoders::utf_error_policy Policy = transcoders::utf_error_policy::strict,
		  typename Handle>
	requires async::io_device<Handle> && (Policy == transcoders::utf_error_policy::strict ||
		(Policy == transcoders::utf_error_policy::replace && transcoders::is_unicode_wire<From> && transcoders::is_unicode_wire<To>))
inline auto cond(Handle &&handle)
{
	using engine = decltype(details::wire_conversion<From, To, Policy>());
	if constexpr (async::input_device<Handle> && async::output_device<Handle>)
		return make_iotranscoder(::std::forward<Handle>(handle), engine{}, engine{});
	else if constexpr (async::input_device<Handle>)
		return make_itranscoder(::std::forward<Handle>(handle), engine{});
	else return make_otranscoder(::std::forward<Handle>(handle), engine{});
}

/** @brief Spells the UTF-8-to-GBK specialization at the operand being decorated. */
template <typename Handle>
inline auto cond_utf8_gbk(Handle &&handle)
{ return cond<text_encoding::utf8, text_encoding::gbk>(::std::forward<Handle>(handle)); }

/** @brief Spells the GBK-to-UTF-8 specialization at the operand being decorated. */
template <typename Handle>
inline auto cond_gbk_utf8(Handle &&handle)
{ return cond<text_encoding::gbk, text_encoding::utf8>(::std::forward<Handle>(handle)); }

} // namespace fast_io
