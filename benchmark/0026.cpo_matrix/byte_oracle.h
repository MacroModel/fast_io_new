#pragma once

#include <cstddef>
#include <cstdint>

namespace fast_io_cpo_matrix::oracle
{

struct byte_comparison_result
{
	bool equal{};
	::std::size_t mismatch{};
};

/// @brief Compares two complete byte intervals without consulting a formatting CPO.
/// @details The caller constructs the expected interval directly from fixture storage. Keeping this primitive
///          independent from alias, reserve, scatter, print, and concat customization points prevents a shared
///          dispatcher defect from validating itself. `mismatch` is the first differing byte, or the common extent
///          when only the lengths differ.
[[nodiscard]] inline constexpr byte_comparison_result compare_bytes(
	char const *actual, ::std::size_t actual_size, char const *expected,
	::std::size_t expected_size) noexcept
{
	auto const common_size{actual_size < expected_size ? actual_size : expected_size};
	for (::std::size_t index{}; index != common_size; ++index)
	{
		if (actual[index] != expected[index])
		{
			return {false, index};
		}
	}
	if (actual_size != expected_size)
	{
		return {false, common_size};
	}
	return {true, actual_size};
}

/// @brief Extends an FNV-1a digest over an explicitly supplied byte interval.
/// @details Digesting is reserved for untimed validation and post-measurement state. The benchmark hot path exposes
///          its produced interval through a compiler barrier instead, so hashing cannot hide formatting regressions.
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
