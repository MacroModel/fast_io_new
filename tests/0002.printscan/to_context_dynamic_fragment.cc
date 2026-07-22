#include "scan_concept_support.h"

namespace
{

inline ::std::size_t direct_tail_emissions{};
inline ::std::size_t dynamic_tail_emissions{};

struct direct_tail
{};

/// @brief Gives the direct tail a fixed one-character reserve bound.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, direct_tail>) noexcept
{
	return 1u;
}

/// @brief Records an attempted direct-tail emission before writing its sentinel byte.
inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, direct_tail>, char *iterator,
	direct_tail &) noexcept
{
	++direct_tail_emissions;
	*iterator++ = 'X';
	return iterator;
}

struct dynamic_tail
{};

using dynamic_output = ::fast_io::basic_dynamic_output_buffer_ref<
	::fast_io::basic_dynamic_output_buffer<char>>;

/// @brief Records an attempted dynamic-tail emission before writing its sentinel byte.
inline void print_define(::fast_io::io_reserve_type_t<char, dynamic_tail>,
						 dynamic_output output, dynamic_tail &) noexcept
{
	++dynamic_tail_emissions;
	constexpr char value[]{'Y'};
	::fast_io::operations::write_all(output, value, value + 1u);
}

} // namespace

/// @brief Verifies that completed context scans suppress all later direct and dynamic formatters.
int main()
{
	using namespace ::scan_concept_harness;

	literal_target<false> inplace_target;
	::fast_io::inplace_to(inplace_target, ::fast_io::mnp::left("abc|", 4u));
	if (!literal_equals(inplace_target, "abc") || inplace_target.commits != 1u)
	{
		::fast_io::fast_terminate();
	}

	auto converted{
		::fast_io::to<literal_target<false>>(::fast_io::mnp::left("xyz|", 4u))};
	if (!literal_equals(converted, "xyz") || converted.commits != 1u)
	{
		::fast_io::fast_terminate();
	}

	// A context scanner owns completion. Once the delimiter in the first fragment returns `ok`, neither the direct
	// reserve strategy nor the dynamic-output fallback may format a later argument. The public boundary may still alias
	// that argument; these counters intentionally observe only its formatter, which is the work this optimization skips.
	direct_tail_emissions = 0u;
	auto direct_short_circuit{
		::fast_io::to<literal_target<false>>("direct|", direct_tail{})};
	if (!literal_equals(direct_short_circuit, "direct") ||
		direct_tail_emissions != 0u)
	{
		::fast_io::fast_terminate();
	}

	dynamic_tail_emissions = 0u;
	literal_target<false> dynamic_short_circuit;
	::fast_io::inplace_to(dynamic_short_circuit, "dynamic|", dynamic_tail{});
	if (!literal_equals(dynamic_short_circuit, "dynamic") ||
		dynamic_tail_emissions != 0u)
	{
		::fast_io::fast_terminate();
	}
}
