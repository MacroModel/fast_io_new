#include "common_outputs.h"

namespace probe = ::fast_io_cpo_matrix;
namespace dispatch = ::fast_io::details::decay;

using f = probe::fixed_reserve_source;
using d = probe::dynamic_reserve_source;
using p = probe::precise_reserve_source;
using s = probe::scatter_source;
using b = probe::borrowed_scatter_source;
using bd = probe::bounded_dynamic_source;
using pp = probe::precise_preferred_source;
using ss = probe::stable_scatter_source;
using o = probe::ring_output_ref;

static_assert(!::fast_io::borrowed_scatter_source<char, s>);
static_assert(::fast_io::borrowed_scatter_source<char, b>);

/*
Each assertion below names one independent planner premise.  Their conjunction
is intentionally established only by the proof-rich controls; no assertion may
be derived from reserve/scatter shape or a `noexcept` producer alone.
*/
static_assert(
	::fast_io::single_pass_bounded_materialization_source<char, bd>);
static_assert(::fast_io::eager_materialization_safe_printable<char, bd>);
static_assert(::fast_io::cached_precise_reserve_printable<char, pp>);
static_assert(
	::fast_io::output_growth_independent_precise_reserve_printable<char, pp>);
static_assert(
	::fast_io::concat_fresh_precise_resize_printable_preferred<char, pp>);
static_assert(::fast_io::eager_materialization_safe_printable<char, pp>);
static_assert(::fast_io::borrowed_scatter_source<char, ss>);
static_assert(::fast_io::scatter_output_state_independent<char, ss>);
static_assert(::fast_io::scatter_direct_print_equivalent<char, ss>);
static_assert(::fast_io::copy_stable_borrowed_print_source<char, ss>);
static_assert(::fast_io::eager_materialization_safe_printable<char, ss>);

/*
The heterogeneous public recipe F/D/P/S/A/F/D/P normalizes A to F.  The
unmarked scatter is a deliberate semantic cut: the first three reserve leaves
may be materialized together, but retaining S while later producers execute is
not justified.  Its companion B designates immutable corpus storage and
publishes the independent-lifetime proof, so F/D/P/B/F/F/D/P must form one
finite dynamic/scatter prefix.  These assertions distinguish a required
provenance boundary from a performance regression without relying on timing.
*/
static_assert(!dispatch::print_runtime_scatter_plan_fast_entry_available_v<
			  char, o, f &, d &, p &, s &, f &, f &, d &, p &>);
inline constexpr auto unmarked_prefix{
	dispatch::find_continuous_scatters_n<char, f, d, p, s, f, f, d, p>()};
static_assert(unmarked_prefix.position == 3u);
static_assert(unmarked_prefix.hasreserve && unmarked_prefix.hasdynamicreserve);
static_assert(!unmarked_prefix.hasscatters);
static_assert(!dispatch::print_controls_dynamic_scatters_reserve_fast_entry_available<
			  char, f, d, p, s, f, f, d, p>());

inline constexpr auto borrowed_prefix{
	dispatch::find_continuous_scatters_n<char, f, d, p, b, f, f, d, p>()};
static_assert(borrowed_prefix.position == 8u);
static_assert(borrowed_prefix.hasscatters && borrowed_prefix.hasreserve &&
			  borrowed_prefix.hasdynamicreserve);
static_assert(dispatch::print_controls_dynamic_scatters_reserve_fast_entry_available<
			  char, f, d, p, b, f, f, d, p>());

extern "C" void fast_io_cpo_matrix_mixed_n8_dispatch(
	o &output, f &a0, d &a1, p &a2, s &a3,
	f &a4, f &a5, d &a6, p &a7)
{
	::fast_io::operations::decay::print_freestanding_decay_impl<false>(
		output, a0, a1, a2, a3, a4, a5, a6, a7);
}

int main()
{
	return 0;
}
