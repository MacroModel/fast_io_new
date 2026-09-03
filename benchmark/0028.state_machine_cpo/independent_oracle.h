#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fixture.h"

namespace fast_io_state_machine_cpo::oracle
{

[[nodiscard]] inline constexpr bool equal_bytes(
	char const *actual, ::std::size_t actual_size,
	char const *expected, ::std::size_t expected_size) noexcept
{
	if (actual_size != expected_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != actual_size; ++index)
	{
		if (actual[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline constexpr bool equal_bytes(
	char const *actual, ::std::size_t actual_size,
	text_record const &expected) noexcept
{
	return equal_bytes(
		actual, actual_size, expected.bytes.data(), expected.size);
}

[[nodiscard]] inline constexpr ::std::uint_least64_t digest_bytes(
	::std::uint_least64_t digest, char const *data,
	::std::size_t size) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		digest ^= static_cast<unsigned char>(data[index]);
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

[[nodiscard]] inline constexpr ::std::uint_least64_t double_bits(
	double value) noexcept
{
	return ::std::bit_cast<::std::uint_least64_t>(value);
}

[[nodiscard]] inline constexpr bool same_double(
	double actual, double expected) noexcept
{
	/*
	The generated grammar uses only integer multiples of one quarter.  Equality
	of object representations therefore checks sign-of-zero as well as exact
	value without introducing a tolerance oracle into parser measurements.
	*/
	return double_bits(actual) == double_bits(expected);
}

[[nodiscard]] inline bool scalar_corpus_is_self_consistent(
	scalar_corpus const &corpus) noexcept
{
	for (auto const &record : corpus)
	{
		text_record decimal{};
		make_signed_decimal(decimal, record.decimal_value);
		if (!equal_bytes(
				record.decimal.bytes.data(), record.decimal.size, decimal))
		{
			return false;
		}
		text_record hexadecimal{};
		make_unsigned_hexadecimal(hexadecimal, record.hexadecimal_value);
		if (!equal_bytes(
				record.hexadecimal.bytes.data(), record.hexadecimal.size,
				hexadecimal))
		{
			return false;
		}
		if (record.word.size == 0u ||
			record.word.size > record.word.bytes.size())
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline bool transcode_record_is_canonical(
	transcode_record const &record) noexcept
{
	::std::size_t encoded_position{};
	for (::std::size_t index{}; index != record.logical_size; ++index)
	{
		auto const character{record.logical[index]};
		if (character == '\r')
		{
			/*
			The fixture deliberately has no raw carriage return.  This makes the
			CRLF decoder's source grammar unambiguous at every chunk boundary.
			*/
			return false;
		}
		if (character == '\n')
		{
			if (encoded_position == record.encoded_size ||
				record.encoded[encoded_position++] != '\r')
			{
				return false;
			}
		}
		if (encoded_position == record.encoded_size ||
			record.encoded[encoded_position++] != character)
		{
			return false;
		}
	}
	return encoded_position == record.encoded_size;
}

} // namespace fast_io_state_machine_cpo::oracle
