#pragma once

#include "../../fast_io_freestanding.h"

#include "dom.h"
#include "error.h"
#include "escape.h"
#include "number.h"
#include "options.h"
#include "simd.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fast_io::json
{

template <typename iterator>
struct json_parse_result
{
	iterator iter{};
	::fast_io::json::json_errc code{};
	::std::size_t offset{};
	// Line and column are one-based diagnostics on failure and zero on success.
	::std::size_t line{};
	::std::size_t column{};

	/* A parse result follows the usual error-code convention: `none` is success. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return code == ::fast_io::json::json_errc::none;
	}

	[[nodiscard]] constexpr ::fast_io::error error() const noexcept
	{
		return {::fast_io::json::json_domain_value,
				static_cast<::std::size_t>(code)};
	}
};

namespace details
{

template <::std::integral char_type>
[[nodiscard]] inline constexpr json_parse_result<char_type const *>
json_make_parse_result(char_type const *first, char_type const *position,
					   ::fast_io::json::json_errc code) noexcept
{
	json_parse_result<char_type const *> result{};
	result.iter = position;
	result.code = code;
	result.offset = first == position ? 0u : static_cast<::std::size_t>(position - first);
	result.line = 1u;
	result.column = 1u;
	for (auto current{first}; current != position; ++current)
	{
		if (*current == ::fast_io::char_literal_v<u8'\n', char_type>)
		{
			++result.line;
			result.column = 1u;
		}
		else
		{
			++result.column;
		}
	}
	return result;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr json_parse_result<char_type const *>
json_make_success_parse_result(char_type const *first,
							   char_type const *last) noexcept
{
	// Successful callers need the consumed endpoint, not a second full pass to
	// rediscover a diagnostic line/column which is meaningful only on failure.
	return {last, ::fast_io::json::json_errc::none,
		first == last ? 0u : static_cast<::std::size_t>(last - first), 0u, 0u};
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr unsigned json_hex_value(char_type value) noexcept
{
	constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
	constexpr auto nine{::fast_io::char_literal_v<u8'9', char_type>};
	constexpr auto lower_a{::fast_io::char_literal_v<u8'a', char_type>};
	constexpr auto lower_f{::fast_io::char_literal_v<u8'f', char_type>};
	constexpr auto upper_a{::fast_io::char_literal_v<u8'A', char_type>};
	constexpr auto upper_f{::fast_io::char_literal_v<u8'F', char_type>};
	if (value >= zero && value <= nine)
	{
		return static_cast<unsigned>(value - zero);
	}
	if (value >= lower_a && value <= lower_f)
	{
		return static_cast<unsigned>(value - lower_a) + 10u;
	}
	if (value >= upper_a && value <= upper_f)
	{
		return static_cast<unsigned>(value - upper_a) + 10u;
	}
	return 0xFFu;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool json_read_hex_quad(char_type const *first,
													   char_type const *last,
													   ::std::uint_least32_t &value,
													   char_type const *&error) noexcept
{
	::std::uint_least32_t accumulator{};
	for (unsigned index{}; index != 4u; ++index)
	{
		if (first == last ||
			static_cast<::std::size_t>(last - first) <= index)
		{
			error = last;
			return false;
		}
		auto const digit{json_hex_value(first[index])};
		if (digit == 0xFFu)
		{
			error = first + index;
			return false;
		}
		accumulator = static_cast<::std::uint_least32_t>((accumulator << 4u) | digit);
	}
	value = accumulator;
	return true;
}

template <typename string_type>
inline void json_append_scalar(string_type &destination, ::std::uint_least32_t code_point)
{
	using destination_char_type = typename string_type::value_type;
	basic_json_escape_buffer<destination_char_type> encoded{};
	json_encode_scalar(encoded, code_point);
	destination.append(encoded.buffer, encoded.size);
}

/* Locate either JSON string sentinel in one pass.  The common unescaped path
   sees only the closing quote; escaped strings stop at the first reverse
   solidus and are decoded incrementally.  Using the same mask-to-bitset SIMD
   vocabulary as stage one avoids two independent memchr passes and, unlike a
   byte-at-a-time close scan, does not make short ASCII runs branch-heavy. */
template <::std::integral char_type>
[[nodiscard]] inline char_type const *json_find_string_special(
	char_type const *first, char_type const *last) noexcept
{
	if constexpr (sizeof(char_type) == 1u)
	{
		constexpr auto native_mask_width{
			::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size_with_mask_to_bitset};
		constexpr auto width{native_mask_width != 0u
			? native_mask_width
			: ::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
		if constexpr (width != 0u)
		{
			using simd_type = ::fast_io::intrinsics::simd_vector<
				::std::uint_least8_t, width>;
			auto const quotation_mark{json_simd_splat<width>(0x22u)};
			auto const reverse_solidus{json_simd_splat<width>(0x5Cu)};
			for (; static_cast<::std::size_t>(last - first) >= width;
				 first += width)
			{
				simd_type bytes{};
				bytes.load(static_cast<void const *>(first));
				auto const candidates{
					(bytes == quotation_mark) | (bytes == reverse_solidus)};
				if (!::fast_io::intrinsics::is_all_zeros(candidates))
				{
					auto const bits{
						::fast_io::intrinsics::vector_mask_to_bitset(candidates)};
					return first + static_cast<::std::size_t>(
						::std::countr_zero(bits));
				}
			}
		}
	}
	constexpr auto quotation_mark{::fast_io::char_literal_v<u8'"', char_type>};
	constexpr auto reverse_solidus{::fast_io::char_literal_v<u8'\\', char_type>};
	for (; first != last && *first != quotation_mark &&
		*first != reverse_solidus; ++first)
	{
	}
	return first;
}

template <::std::integral char_type>
[[nodiscard]] inline char_type *json_find_string_special(
	char_type *first, char_type *last) noexcept
{
	return const_cast<char_type *>(json_find_string_special(
		static_cast<char_type const *>(first),
		static_cast<char_type const *>(last)));
}

template <typename string_type, ::std::integral char_type>
[[nodiscard]] inline bool json_decode_string(char_type const *quote,
											 char_type const *last,
											 string_type &destination,
											 char_type const *&after,
											 char_type const *&error,
											 ::fast_io::json::json_errc &error_code,
											 bool &compact_direct)
{
	using destination_char_type = typename string_type::value_type;
	constexpr bool byte_compatible{
		sizeof(destination_char_type) == 1u && sizeof(char_type) == 1u};
	constexpr auto quotation_mark{::fast_io::char_literal_v<u8'"', char_type>};
	constexpr auto reverse_solidus{::fast_io::char_literal_v<u8'\\', char_type>};
	compact_direct = true;

	auto const content{quote + 1};
	destination.clear();
	auto const append_raw = [&](char_type const *raw_first,
		char_type const *raw_last) -> bool
	{
		if constexpr (::std::same_as<destination_char_type, char_type>)
		{
			destination.append(raw_first,
				static_cast<::std::size_t>(raw_last - raw_first));
			return true;
		}
		else if constexpr (byte_compatible)
		{
			auto const raw_size{static_cast<::std::size_t>(
				raw_last - raw_first)};
			auto const old_size{static_cast<::std::size_t>(destination.size())};
			destination.resize(old_size + raw_size);
			if (raw_size != 0u)
			{
				__builtin_memcpy(static_cast<void *>(destination.data() + old_size),
					static_cast<void const *>(raw_first), raw_size);
			}
			return true;
		}
		else
		{
			while (raw_first != raw_last)
			{
				auto const decoded{decode_json_code_point(raw_first,
					static_cast<::std::size_t>(raw_last - raw_first), 0u)};
				if (decoded.status != unicode_decode_status::ok)
				{
					error = raw_first;
					error_code = sizeof(char_type) == 1u
						? ::fast_io::json::json_errc::invalid_utf8
						: ::fast_io::json::json_errc::invalid_unicode;
					return false;
				}
				json_append_scalar(destination, decoded.code_point);
				raw_first += decoded.next;
			}
			return true;
		}
	};

	auto current{content};
	for (;;)
	{
		auto const special{json_find_string_special(current, last)};
		if (special == last)
		{
			error = last;
			error_code = ::fast_io::json::json_errc::unexpected_end;
			return false;
		}
		if constexpr (::std::same_as<destination_char_type, char_type> ||
			byte_compatible)
		{
			if (current == content && *special == quotation_mark)
			{
				if constexpr (::std::same_as<destination_char_type, char_type>)
				{
					destination.assign(content,
						static_cast<::std::size_t>(special - content));
				}
				else
				{
					auto const raw_size{static_cast<::std::size_t>(
						special - content)};
					destination.resize(raw_size);
					if (raw_size != 0u)
					{
						__builtin_memcpy(static_cast<void *>(destination.data()),
							static_cast<void const *>(content), raw_size);
					}
				}
				after = special + 1;
				return true;
			}
		}
		if (!append_raw(current, special))
		{
			return false;
		}
		if (*special == quotation_mark)
		{
			after = special + 1;
			return true;
		}
		current = special;
		auto const escape_start{current};
		++current;
		if (current == last)
		{
			error = escape_start;
			error_code = ::fast_io::json::json_errc::invalid_escape;
			return false;
		}
		::std::uint_least32_t code_point{};
		switch (json_code_unit(*current))
		{
		case 0x22u:
			code_point = 0x22u;
			++current;
			break;
		case 0x5Cu:
			code_point = 0x5Cu;
			++current;
			break;
		case 0x2Fu:
			code_point = 0x2Fu;
			++current;
			break;
		case 0x62u:
			code_point = 0x08u;
			++current;
			break;
		case 0x66u:
			code_point = 0x0Cu;
			++current;
			break;
		case 0x6Eu:
			code_point = 0x0Au;
			++current;
			break;
		case 0x72u:
			code_point = 0x0Du;
			++current;
			break;
		case 0x74u:
			code_point = 0x09u;
			++current;
			break;
		case 0x75u:
		{
			++current;
			char_type const *hex_error{};
			if (!json_read_hex_quad(current, last, code_point, hex_error))
			{
				error = hex_error;
				error_code = ::fast_io::json::json_errc::invalid_unicode_escape;
				return false;
			}
			current += 4;
			if (code_point >= 0xD800u && code_point <= 0xDBFFu)
			{
				if (static_cast<::std::size_t>(last - current) < 6u ||
					current[0] != reverse_solidus ||
					current[1] != ::fast_io::char_literal_v<u8'u', char_type>)
				{
					error = current;
					error_code = ::fast_io::json::json_errc::invalid_unicode_escape;
					return false;
				}
				::std::uint_least32_t low_surrogate{};
				if (!json_read_hex_quad(current + 2, last, low_surrogate, hex_error) ||
					low_surrogate < 0xDC00u || low_surrogate > 0xDFFFu)
				{
					error = hex_error == nullptr ? current + 2 : hex_error;
					error_code = ::fast_io::json::json_errc::invalid_unicode_escape;
					return false;
				}
				code_point = static_cast<::std::uint_least32_t>(
					0x10000u + ((code_point - 0xD800u) << 10u) + (low_surrogate - 0xDC00u));
				current += 6;
			}
			else if (code_point >= 0xDC00u && code_point <= 0xDFFFu)
			{
				error = escape_start;
				error_code = ::fast_io::json::json_errc::invalid_unicode_escape;
				return false;
			}
			break;
		}
		default:
			error = current;
			error_code = ::fast_io::json::json_errc::invalid_escape;
			return false;
		}
		compact_direct = compact_direct && code_point >= 0x20u &&
			code_point != 0x22u && code_point != 0x5cu;
		json_append_scalar(destination, code_point);
	}
}

template <typename json_type, ::fast_io::details::character char_type>
class basic_json_stage2_parser
{
	using access_type = ::fast_io::json::detail::basic_json_access;
	using node_type = typename json_type::node_type;
	using number_type = typename json_type::number_type;
	using integer_type = typename json_type::integer_type;
	using uinteger_type = typename json_type::uinteger_type;
	using string_type = typename json_type::string_type;
	using array_type = typename json_type::array_type;
	using object_type = typename json_type::object_type;
	using key_type = typename object_type::key_type;

	enum class frame_phase : char unsigned
	{
		array_value_or_end,
		array_value,
		array_comma_or_end,
		object_key_or_end,
		object_key,
		object_colon,
		object_comma_or_end
	};

	struct frame
	{
		frame_phase phase{};
		node_type *owner{};
		array_type *array{};
		object_type *object{};
		::std::optional<key_type> key{};
		char_type const *key_position{};
	};

public:
	basic_json_stage2_parser(char_type const *first, char_type const *last,
							 node_type &root, json_stage1_result const &stage1,
							 ::fast_io::json::json_parse_options options)
		: first_(first), last_(last), cursor_(first), root_(root),
		  structurals_(stage1.structurals), options_(options)
	{
		auto const reserve_size{(::std::min)(options_.max_depth, static_cast<::std::size_t>(64u))};
		frames_.reserve(reserve_size);
	}

	[[nodiscard]] bool parse()
	{
		node_type *target{::std::addressof(root_)};
		bool need_value{true};

		for (;;)
		{
			if (need_value)
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::unexpected_token))
				{
					return false;
				}
				++structural_index_;
				if (!parse_value(token, *target))
				{
					return false;
				}
				need_value = false;
				continue;
			}

			if (frames_.empty())
			{
				break;
			}

			auto &current{frames_.back()};
			switch (current.phase)
			{
			case frame_phase::array_value_or_end:
			case frame_phase::array_value:
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::unexpected_token))
				{
					return false;
				}
				if (*token == ::fast_io::char_literal_v<u8']', char_type>)
				{
					if (current.phase == frame_phase::array_value)
					{
						return fail(::fast_io::json::json_errc::unexpected_token, token);
					}
					consume_punctuation(token);
					frames_.pop_back();
					continue;
				}
				current.array->emplace_back(access_type::make_child(*current.owner));
				target = ::std::addressof(current.array->back());
				current.phase = frame_phase::array_comma_or_end;
				need_value = true;
				continue;
			}
			case frame_phase::array_comma_or_end:
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::expected_comma_or_end))
				{
					return false;
				}
				if (*token == ::fast_io::char_literal_v<u8',', char_type>)
				{
					consume_punctuation(token);
					current.phase = frame_phase::array_value;
					continue;
				}
				if (*token == ::fast_io::char_literal_v<u8']', char_type>)
				{
					consume_punctuation(token);
					frames_.pop_back();
					continue;
				}
				return fail(::fast_io::json::json_errc::expected_comma_or_end, token);
			}
			case frame_phase::object_key_or_end:
			case frame_phase::object_key:
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::unexpected_token))
				{
					return false;
				}
				if (*token == ::fast_io::char_literal_v<u8'}', char_type>)
				{
					if (current.phase == frame_phase::object_key)
					{
						return fail(::fast_io::json::json_errc::unexpected_token, token);
					}
					consume_punctuation(token);
					frames_.pop_back();
					continue;
				}
				if (*token != ::fast_io::char_literal_v<u8'"', char_type>)
				{
					return fail(::fast_io::json::json_errc::unexpected_token, token);
				}
				++structural_index_;
				/* The object's rebound allocator constructs the pair, but a plain
				   allocator does not propagate into basic_string.  Construct the
				   decoded key with the owning JSON node's rebound allocator before
				   insertion; otherwise a stateful allocator silently falls back to
				   the key type's default resource. */
				using key_allocator_type = typename key_type::allocator_type;
				if constexpr (::std::constructible_from<
							  key_allocator_type,
							  decltype(access_type::allocator(*current.owner))> &&
						  ::std::constructible_from<key_type,
										key_allocator_type>)
				{
					current.key.emplace(key_allocator_type{
						access_type::allocator(*current.owner)});
				}
				else
				{
					current.key.emplace();
				}
				char_type const *after{};
				char_type const *string_error{};
				auto string_error_code{::fast_io::json::json_errc::invalid_escape};
				bool compact_direct{};
				if (!json_decode_string(token, last_, *current.key, after,
									string_error, string_error_code, compact_direct))
				{
					return fail(string_error_code, string_error);
				}
				cursor_ = after;
				current.key_position = token;
				current.phase = frame_phase::object_colon;
				continue;
			}
			case frame_phase::object_colon:
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::expected_colon))
				{
					return false;
				}
				if (*token != ::fast_io::char_literal_v<u8':', char_type>)
				{
					return fail(::fast_io::json::json_errc::expected_colon, token);
				}
				consume_punctuation(token);
				target = prepare_object_value(current);
				if (target == nullptr)
				{
					return false;
				}
				current.phase = frame_phase::object_comma_or_end;
				need_value = true;
				continue;
			}
			case frame_phase::object_comma_or_end:
			{
				char_type const *token{};
				if (!next_token(token, ::fast_io::json::json_errc::unexpected_end,
								::fast_io::json::json_errc::expected_comma_or_end))
				{
					return false;
				}
				if (*token == ::fast_io::char_literal_v<u8',', char_type>)
				{
					consume_punctuation(token);
					current.phase = frame_phase::object_key;
					continue;
				}
				if (*token == ::fast_io::char_literal_v<u8'}', char_type>)
				{
					consume_punctuation(token);
					frames_.pop_back();
					continue;
				}
				return fail(::fast_io::json::json_errc::expected_comma_or_end, token);
			}
			}
		}

		return finish_root();
	}

	[[nodiscard]] ::fast_io::json::json_errc error_code() const noexcept
	{
		return error_code_;
	}
	[[nodiscard]] char_type const *error_position() const noexcept
	{
		return error_position_;
	}

private:
	[[nodiscard]] bool fail(::fast_io::json::json_errc code, char_type const *position) noexcept
	{
		if (error_code_ == ::fast_io::json::json_errc::none)
		{
			error_code_ = code;
			error_position_ = position;
		}
		return false;
	}

	[[nodiscard]] bool only_json_space(char_type const *begin, char_type const *end,
									   ::fast_io::json::json_errc code)
	{
		for (; begin != end; ++begin)
		{
			if (!json_ascii_space(json_code_unit(*begin)))
			{
				return fail(code, begin);
			}
		}
		return true;
	}

	[[nodiscard]] bool next_token(char_type const *&token, ::fast_io::json::json_errc eof_code,
								  ::fast_io::json::json_errc gap_code)
	{
		if (structural_index_ == structurals_.size())
		{
			if (!only_json_space(cursor_, last_, gap_code))
			{
				return false;
			}
			return fail(eof_code, last_);
		}
		auto const offset{structurals_[structural_index_]};
		token = first_ + offset;
		if (token < cursor_)
		{
			return fail(::fast_io::json::json_errc::syntax_error, token);
		}
		return only_json_space(cursor_, token, gap_code);
	}

	void consume_punctuation(char_type const *token) noexcept
	{
		++structural_index_;
		cursor_ = token + 1;
	}

	[[nodiscard]] bool finish_root()
	{
		if (structural_index_ != structurals_.size())
		{
			auto const *token{first_ + structurals_[structural_index_]};
			if (token < cursor_)
			{
				return fail(::fast_io::json::json_errc::syntax_error, token);
			}
			if (!only_json_space(cursor_, token, ::fast_io::json::json_errc::trailing_data))
			{
				return false;
			}
			return fail(::fast_io::json::json_errc::trailing_data, token);
		}
		return only_json_space(cursor_, last_,
			::fast_io::json::json_errc::trailing_data);
	}

	template <::std::size_t n>
	[[nodiscard]] bool consume_literal(char_type const *token, char8_t const (&literal)[n])
	{
		constexpr ::std::size_t length{n - 1u};
		for (::std::size_t index{}; index != length; ++index)
		{
			if (static_cast<::std::size_t>(last_ - token) <= index)
			{
				return fail(::fast_io::json::json_errc::unexpected_end, last_);
			}
			if (token[index] != ::fast_io::char_literal<char_type>(literal[index]))
			{
				return fail(::fast_io::json::json_errc::invalid_literal, token + index);
			}
		}
		auto const after{token + length};
		if (after != last_ && !json_number_is_delimiter(*after))
		{
			return fail(::fast_io::json::json_errc::invalid_literal, after);
		}
		cursor_ = after;
		return true;
	}

	[[nodiscard]] bool push_container(node_type &node, bool object)
	{
		if (frames_.size() >= options_.max_depth)
		{
			return fail(::fast_io::json::json_errc::depth_exceeded, cursor_ - 1);
		}
		if (object)
		{
			auto &container{access_type::emplace_object(node)};
			frames_.push_back(frame{frame_phase::object_key_or_end,
									::std::addressof(node),
									nullptr,
									::std::addressof(container),
									{},
									nullptr});
		}
		else
		{
			auto &container{access_type::emplace_array(node)};
			frames_.push_back(frame{frame_phase::array_value_or_end,
									::std::addressof(node),
									::std::addressof(container),
									nullptr,
									{},
									nullptr});
		}
		return true;
	}

	[[nodiscard]] bool parse_value(char_type const *token, node_type &node)
	{
		auto const value{json_code_unit(*token)};
		switch (value)
		{
		case 0x6Eu:
			if (!consume_literal(token, u8"null"))
			{
				return false;
			}
			access_type::set_null(node);
			return true;
		case 0x74u:
			if (!consume_literal(token, u8"true"))
			{
				return false;
			}
			access_type::set_boolean(node, true);
			return true;
		case 0x66u:
			if (!consume_literal(token, u8"false"))
			{
				return false;
			}
			access_type::set_boolean(node, false);
			return true;
		case 0x22u:
		{
			auto &string{access_type::emplace_string(node)};
			access_type::invalidate_string_metadata(node);
			char_type const *after{};
			char_type const *string_error{};
			auto string_error_code{::fast_io::json::json_errc::invalid_escape};
			bool compact_direct{};
			if (!json_decode_string(token, last_, string, after, string_error,
									string_error_code, compact_direct))
			{
				return fail(string_error_code, string_error);
			}
			access_type::set_validated_string_metadata(node, compact_direct);
			cursor_ = after;
			return true;
		}
		case 0x5Bu:
			cursor_ = token + 1;
			return push_container(node, false);
		case 0x7Bu:
			cursor_ = token + 1;
			return push_container(node, true);
		default:
			break;
		}

		if (value == 0x2Du || (value >= 0x30u && value <= 0x39u))
		{
			auto const scanned{scan_json_number(token, last_)};
			if (scanned.code != ::fast_io::parse_code::ok)
			{
				return fail(scanned.code == ::fast_io::parse_code::end_of_file
								? ::fast_io::json::json_errc::unexpected_end
								: ::fast_io::json::json_errc::invalid_number,
							scanned.iter);
			}
			if (!assign_number(node, scanned.token))
			{
				return false;
			}
			cursor_ = scanned.token.last;
			return true;
		}
		return fail(::fast_io::json::json_errc::unexpected_token, token);
	}

	template <typename value_type>
	[[nodiscard]] ::fast_io::parse_code convert_number(
		basic_json_number_token<char_type const *> const &token, value_type &value)
	{
		if constexpr ((::fast_io::json_signed_integer<value_type> ||
					   ::fast_io::json_unsigned_integer<value_type> ||
					   ::fast_io::json_floating_point<value_type>) &&
					  ::std::default_initializable<value_type>)
		{
			return parse_json_number_into(token, value);
		}
		else
		{
			return ::fast_io::parse_code::invalid;
		}
	}

	[[nodiscard]] ::fast_io::parse_code try_signed(node_type &node,
												   basic_json_number_token<char_type const *> const &token)
	{
		if constexpr (::fast_io::json_signed_integer<integer_type> &&
					  ::std::default_initializable<integer_type>)
		{
			integer_type value{};
			auto const code{convert_number(token, value)};
			if (code == ::fast_io::parse_code::ok)
			{
				access_type::set_integer(node, ::std::move(value));
			}
			return code;
		}
		else
		{
			return ::fast_io::parse_code::invalid;
		}
	}

	[[nodiscard]] ::fast_io::parse_code try_unsigned(node_type &node,
													 basic_json_number_token<char_type const *> const &token)
	{
		if constexpr (::fast_io::json_unsigned_integer<uinteger_type> &&
					  ::std::default_initializable<uinteger_type>)
		{
			uinteger_type value{};
			auto const code{convert_number(token, value)};
			if (code == ::fast_io::parse_code::ok)
			{
				access_type::set_uinteger(node, ::std::move(value));
			}
			return code;
		}
		else
		{
			return ::fast_io::parse_code::invalid;
		}
	}

	[[nodiscard]] ::fast_io::parse_code try_floating(node_type &node,
													 basic_json_number_token<char_type const *> const &token)
	{
		if constexpr (::fast_io::json_floating_point<number_type> &&
					  ::std::default_initializable<number_type>)
		{
			number_type value{};
			auto const code{convert_number(token, value)};
			if (code == ::fast_io::parse_code::ok)
			{
				access_type::set_number(node, ::std::move(value));
			}
			return code;
		}
		else
		{
			return ::fast_io::parse_code::invalid;
		}
	}

	[[nodiscard]] bool assign_number(node_type &node,
									 basic_json_number_token<char_type const *> const &token)
	{
		if (token.kind == json_number_token_kind::floating ||
			options_.integer_preference == ::fast_io::json::json_integer_preference::prefer_floating)
		{
			auto const code{try_floating(node, token)};
			return finish_number_conversion(code, ::fast_io::json::json_errc::number_overflow, token.first);
		}

		constexpr bool signed_available{
			::fast_io::json_signed_integer<integer_type> && ::std::default_initializable<integer_type>};
		constexpr bool unsigned_available{
			::fast_io::json_unsigned_integer<uinteger_type> && ::std::default_initializable<uinteger_type>};

		if (token.negative)
		{
			if constexpr (signed_available)
			{
				auto const code{try_signed(node, token)};
				return finish_number_conversion(code, ::fast_io::json::json_errc::integer_overflow, token.first);
			}
			else
			{
				auto const code{try_floating(node, token)};
				return finish_number_conversion(code, ::fast_io::json::json_errc::number_overflow, token.first);
			}
		}

		if (options_.integer_preference == ::fast_io::json::json_integer_preference::prefer_unsigned)
		{
			if constexpr (unsigned_available)
			{
				auto const code{try_unsigned(node, token)};
				return finish_number_conversion(code, ::fast_io::json::json_errc::uinteger_overflow, token.first);
			}
			else if constexpr (signed_available)
			{
				auto const code{try_signed(node, token)};
				return finish_number_conversion(code, ::fast_io::json::json_errc::integer_overflow, token.first);
			}
			else
			{
				auto const code{try_floating(node, token)};
				return finish_number_conversion(code, ::fast_io::json::json_errc::number_overflow, token.first);
			}
		}

		if constexpr (signed_available)
		{
			auto const signed_code{try_signed(node, token)};
			if (signed_code == ::fast_io::parse_code::ok)
			{
				return true;
			}
			if (signed_code != ::fast_io::parse_code::overflow)
			{
				return fail(::fast_io::json::json_errc::invalid_number, token.first);
			}
			if constexpr (unsigned_available)
			{
				auto const unsigned_code{try_unsigned(node, token)};
				return finish_number_conversion(unsigned_code,
												::fast_io::json::json_errc::uinteger_overflow, token.first);
			}
			return fail(::fast_io::json::json_errc::integer_overflow, token.first);
		}
		else if constexpr (unsigned_available)
		{
			auto const code{try_unsigned(node, token)};
			return finish_number_conversion(code, ::fast_io::json::json_errc::uinteger_overflow, token.first);
		}
		else
		{
			auto const code{try_floating(node, token)};
			return finish_number_conversion(code, ::fast_io::json::json_errc::number_overflow, token.first);
		}
	}

	[[nodiscard]] bool finish_number_conversion(::fast_io::parse_code code,
												::fast_io::json::json_errc overflow_code, char_type const *position)
	{
		if (code == ::fast_io::parse_code::ok)
		{
			return true;
		}
		return fail(code == ::fast_io::parse_code::overflow ? overflow_code
															: ::fast_io::json::json_errc::invalid_number,
					position);
	}

	[[nodiscard]] node_type *prepare_object_value(frame &current)
	{
		auto &object{*current.object};
		auto found{object.find(*current.key)};
		if (found != object.end())
		{
			switch (options_.duplicate_keys)
			{
			case ::fast_io::json::json_duplicate_key_policy::reject:
				static_cast<void>(fail(::fast_io::json::json_errc::duplicate_key, current.key_position));
				return nullptr;
			case ::fast_io::json::json_duplicate_key_policy::keep_first:
				discarded_.emplace_back(access_type::allocator(*current.owner));
				current.key.reset();
				return ::std::addressof(access_type::node(discarded_.back()));
			case ::fast_io::json::json_duplicate_key_policy::keep_last:
				access_type::reset(found->second);
				current.key.reset();
				return ::std::addressof(found->second);
			}
		}

		auto child{access_type::make_child(*current.owner)};
		auto insertion{::fast_io::json::detail::json_object_emplace_known_absent(
			object, ::std::move(*current.key), ::std::move(child))};
		current.key.reset();
		return ::std::addressof(insertion->second);
	}

	char_type const *first_{};
	char_type const *last_{};
	char_type const *cursor_{};
	node_type &root_;
	::std::vector<::std::size_t> const &structurals_;
	::fast_io::json::json_parse_options options_{};
	::std::size_t structural_index_{};
	::std::vector<frame> frames_{};
	::std::deque<json_type> discarded_{};
	::fast_io::json::json_errc error_code_{::fast_io::json::json_errc::none};
	char_type const *error_position_{};
};

} // namespace details

template <typename json_type, ::fast_io::details::character char_type>
[[nodiscard]] inline json_parse_result<char_type const *>
try_parse_json(json_type &destination, char_type const *first, char_type const *last,
			   ::fast_io::json::json_parse_options options = {})
{
	auto stage1{::fast_io::json::details::json_build_structural_index(first, last)};
	if (!stage1)
	{
		::fast_io::json::json_errc code{};
		switch (stage1.error)
		{
		case ::fast_io::json::details::json_stage1_errc::none:
			code = ::fast_io::json::json_errc::syntax_error;
			break;
		case ::fast_io::json::details::json_stage1_errc::invalid_unicode:
			code = sizeof(char_type) == 1u ? ::fast_io::json::json_errc::invalid_utf8
										   : ::fast_io::json::json_errc::invalid_unicode;
			break;
		case ::fast_io::json::details::json_stage1_errc::unescaped_control_character:
			code = ::fast_io::json::json_errc::unescaped_control_character;
			break;
		case ::fast_io::json::details::json_stage1_errc::unterminated_string:
			code = ::fast_io::json::json_errc::unexpected_end;
			break;
		}
		return ::fast_io::json::details::json_make_parse_result(
			first, first + stage1.error_offset, code);
	}

	using access_type = ::fast_io::json::detail::basic_json_access;
	auto const allocator{access_type::allocator(access_type::node(destination))};
	json_type temporary{allocator};
	::fast_io::json::details::basic_json_stage2_parser<json_type, char_type> parser{
		first, last, access_type::node(temporary), stage1, options};
	if (!parser.parse())
	{
		return ::fast_io::json::details::json_make_parse_result(
			first, parser.error_position(), parser.error_code());
	}
	destination.swap(temporary);
	return ::fast_io::json::details::json_make_success_parse_result(first, last);
}

template <typename json_type, typename char_type, typename traits_type>
	requires ::fast_io::details::character<char_type>
[[nodiscard]] inline json_parse_result<char_type const *>
try_parse_json(json_type &destination, ::std::basic_string_view<char_type, traits_type> input,
			   ::fast_io::json::json_parse_options options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return ::fast_io::json::try_parse_json(destination, first, last, options);
}

template <typename json_type, ::fast_io::details::character char_type>
	requires ::std::default_initializable<json_type>
[[nodiscard]] inline json_type parse_json(char_type const *first, char_type const *last,
										  ::fast_io::json::json_parse_options options = {})
{
	json_type result{};
	auto const parsed{::fast_io::json::try_parse_json(result, first, last, options)};
	if (!parsed)
	{
		::fast_io::json::throw_json_error(parsed.code);
	}
	return result;
}

template <typename json_type, typename char_type, typename traits_type>
	requires(::fast_io::details::character<char_type> && ::std::default_initializable<json_type>)
[[nodiscard]] inline json_type parse_json(::std::basic_string_view<char_type, traits_type> input,
										  ::fast_io::json::json_parse_options options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return ::fast_io::json::parse_json<json_type>(first, last, options);
}

template <typename json_type>
struct json_scan_proxy
{
	using manip_tag = ::fast_io::manip_tag_t;

	json_type *value{};
	::fast_io::json::json_parse_options options{};
};

template <typename json_type>
[[nodiscard]] inline constexpr json_scan_proxy<json_type>
scan_json(json_type &value, ::fast_io::json::json_parse_options options = {}) noexcept
{
	return {::std::addressof(value), options};
}

/*
Only the terminal contiguous CPO is provided.  A refillable context parser must
retain the structural-index carry, a partial UTF sequence/string escape/number,
and every open DOM frame while preserving the destination's strong guarantee.
Silently buffering an unbounded document or advertising a context state that
cannot make forward progress would violate fast_io's context-scanner contract,
so that distinct facility is intentionally not approximated here.
*/
template <::fast_io::details::character char_type, typename json_type>
[[nodiscard]] inline ::fast_io::parse_result<char_type const *>
scan_contiguous_define(
	::fast_io::io_reserve_type_t<char_type, json_scan_proxy<json_type>>,
	char_type const *first, char_type const *last, json_scan_proxy<json_type> proxy)
{
	auto const result{::fast_io::json::try_parse_json(*proxy.value, first, last, proxy.options)};
	if (result)
	{
		return {result.iter, ::fast_io::parse_code::ok};
	}
	switch (result.code)
	{
	case ::fast_io::json::json_errc::number_overflow:
	case ::fast_io::json::json_errc::integer_overflow:
	case ::fast_io::json::json_errc::uinteger_overflow:
		return {result.iter, ::fast_io::parse_code::overflow};
	case ::fast_io::json::json_errc::unexpected_end:
		return {result.iter, ::fast_io::parse_code::end_of_file};
	default:
		return {result.iter, ::fast_io::parse_code::invalid};
	}
}

} // namespace fast_io::json

namespace fast_io::json
{

template <typename node_type>
[[nodiscard]] inline constexpr auto scan_alias_define(
	::fast_io::io_alias_t, ::fast_io::json::basic_json<node_type> &value) noexcept
{
	return ::fast_io::json::json_scan_proxy<::fast_io::json::basic_json<node_type>>{
		::std::addressof(value), {}};
}

} // namespace fast_io::json
