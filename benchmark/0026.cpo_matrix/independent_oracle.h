#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common_sources.h"

namespace fast_io_cpo_matrix::oracle
{

inline constexpr ::std::size_t maximum_emitted_size{
	selected_pack_count * maximum_token_size + (selected_line ? 1u : 0u)};
inline constexpr ::std::size_t maximum_output_capacity{
	selected_pack_count * maximum_protocol_bound + 1u};

struct expected_record
{
	::std::array<char, maximum_emitted_size == 0u ? 1u : maximum_emitted_size>
		bytes{};
	::std::size_t size{};
};

[[nodiscard]] inline expected_record make_expected(
	corpus_record const &record) noexcept
{
	expected_record expected;
	for (::std::size_t argument{}; argument != selected_pack_count; ++argument)
	{
		auto const &token{record.tokens[argument]};
		for (::std::size_t index{}; index != token.size; ++index)
		{
			expected.bytes[expected.size++] = token.bytes[index];
		}
	}
	if constexpr (selected_line)
	{
		expected.bytes[expected.size++] = '\n';
	}
	return expected;
}

struct comparison_result
{
	bool equal{};
	::std::size_t mismatch{};
};

/*
The oracle consumes only corpus bytes and the public line flag.  It never calls
an alias, reserve, precise, scatter, concat, or output CPO, so a shared defect
in old/new normalization cannot make the preflight self-validating.
*/
[[nodiscard]] inline comparison_result compare(
	char const *actual, ::std::size_t actual_size,
	expected_record const &expected) noexcept
{
	auto const common_size{
		actual_size < expected.size ? actual_size : expected.size};
	for (::std::size_t index{}; index != common_size; ++index)
	{
		if (actual[index] != expected.bytes[index])
		{
			return {false, index};
		}
	}
	if (actual_size != expected.size)
	{
		return {false, common_size};
	}
	return {true, actual_size};
}

[[nodiscard]] inline constexpr ::std::uint_least64_t digest_bytes(
	::std::uint_least64_t digest, char const *first,
	::std::size_t size) noexcept
{
	for (::std::size_t index{}; index != size; ++index)
	{
		digest ^= static_cast<unsigned char>(first[index]);
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

} // namespace fast_io_cpo_matrix::oracle
