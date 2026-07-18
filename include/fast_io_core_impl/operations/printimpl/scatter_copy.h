#pragma once

namespace fast_io::details::decay
{

/// @brief Copies a type-level fixed scatter without losing its extent to a run-time function parameter.
/// @details This helper is intentionally separate from `small_scatter_copy_n`. GCC recognizes a short loop whose count
///          arrives as a function argument as an independent `memcpy`, even after constant propagation. Adjacent
///          literal fragments then remain separate stores and cannot be combined with neighboring one-character
///          manipulators. Keeping `count` in the template exposes every small assignment before loop-distribution and
///          lets the back end merge, for example, `"a", "bbb", chvw('c')` into the same payload stores as `"abbbc"`.
///          Only a `static_scatter_t<count>` may select this route, so the exact source extent is a type-level proof;
///          general run-time scatters continue through the bounded policy below. Larger fixed extents deliberately use
///          the memcpy-shaped primitive to avoid multiplying instructions at delimiter-heavy call sites.
/// @tparam count      exact number of source and destination elements
/// @tparam value_type the trivially addressable element type carried by the scatter
/// @param first       first source element of an extent containing at least `count` elements
/// @param result      first destination element of an extent containing at least `count` elements
/// @return value_type* one past the copied destination
template <::std::size_t count, typename value_type>
inline constexpr value_type *static_scatter_copy_n(
	value_type const *first, value_type *result) noexcept
{
	if constexpr (count <= 16u)
	{
		// An index expansion, rather than a counted loop, prevents GCC from recreating a separate memcpy before the
		// static-scatter pointer itself has propagated to the literal object.
		[]<::std::size_t... index>(value_type const *source, value_type *destination,
			::std::index_sequence<index...>) constexpr noexcept {
			((destination[index] = source[index]), ...);
		}(first, result, ::std::make_index_sequence<count>{});
		return result + count;
	}
	else
	{
		return ::fast_io::details::non_overlapped_copy_n(first, count, result);
	}
}

/// @brief Copies a run-time scatter payload into an already-proved contiguous destination.
/// @details Repeated tiny descriptors, especially range separators, otherwise become out-of-line `memcpy` calls on
///          GCC even when a payload is only a few characters. The 16-element cutoff is shared by print coalescing,
///          concat, and range materialization so those strategy layers cannot silently acquire different lowering
///          policies. It is a measured cost threshold, not a capacity or lifetime proof; payloads above it continue
///          through the general non-overlapping copy routine. Increasing the cutoff requires both throughput and code-
///          size evidence because a compiler may emit one branch for every possible run-time length. GCC 15 measurements
///          showed the current cutoff removing per-separator calls, while also growing one N=128 range hot symbol from
///          roughly 526 to 1,150 bytes; this explicit trade-off must remain visible at the shared policy boundary.
/// @tparam value_type the trivially addressable element type carried by the scatter
/// @param first       first source element
/// @param count       number of elements to copy
/// @param result      first destination element
/// @return value_type* one past the copied destination
template <typename value_type>
inline constexpr value_type *small_scatter_copy_n(
	value_type const *first, ::std::size_t count, value_type *result) noexcept
{
	if (count <= 16u)
	{
		for (::std::size_t i{}; i != count; ++i)
		{
			result[i] = first[i];
		}
		return result + count;
	}
	return ::fast_io::details::non_overlapped_copy_n(first, count, result);
}

} // namespace fast_io::details::decay
