#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "byte_oracle.h"
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
	auto const result{compare_bytes(
		actual, actual_size, expected.bytes.data(), expected.size)};
	return {result.equal, result.mismatch};
}

} // namespace fast_io_cpo_matrix::oracle
