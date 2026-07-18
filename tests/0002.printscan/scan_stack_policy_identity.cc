#include <cassert>
#include <string_view>

#include "scan_concept_support.h"

namespace
{

using disabled_stack_policy = ::fast_io::print_stack_policy<0u>;
using small_stack_policy = ::fast_io::print_stack_policy<3u>;
using inline_stack_policy = ::fast_io::print_stack_policy<4096u>;

static_assert(
	::fast_io::details::scan_precise_inline_staging_capacity<char, disabled_stack_policy>() == 0u);
static_assert(
	::fast_io::details::scan_precise_inline_staging_capacity<char, small_stack_policy>() == 3u);
static_assert(
	::fast_io::details::scan_precise_inline_staging_capacity<char, inline_stack_policy>() == 4096u);

using input_type = ::scan_concept_harness::bounded_refill_source_ref;
using proxy_type = ::scan_concept_harness::fixed_record_proxy<4u>;

inline constexpr auto dynamic_staging_entry =
	&::fast_io::details::scan_precise_reserve_staging<4u, disabled_stack_policy, input_type, proxy_type>;
inline constexpr auto inline_staging_entry =
	&::fast_io::details::scan_precise_reserve_staging<4u, inline_stack_policy, input_type, proxy_type>;

// The same scanner extent makes opposite allocation decisions under these policies. Distinct function addresses are
// a language-level check that the policy participates in specialization identity; an extent-only template could be
// emitted as one weak symbol and let COMDAT selection silently choose another configured build's body.
static_assert(dynamic_staging_entry != inline_staging_entry);
static_assert(4u >
	::fast_io::details::scan_precise_inline_staging_capacity<char, disabled_stack_policy>());
static_assert(4u <=
	::fast_io::details::scan_precise_inline_staging_capacity<char, inline_stack_policy>());

template <typename stack_policy>
inline void test_refill_staging_policy()
{
	::scan_concept_harness::bounded_refill_source source;
	source.reset("ABCD", 1u);
	::scan_concept_harness::fixed_record_target<4u> target;
	auto proxy{scan_alias_define(::fast_io::io_alias, target)};
	assert(::fast_io::operations::decay::scan_freestanding_decay<stack_policy>(
		input_stream_ref_define(source), proxy));
	assert(target.calls == 1u);
	assert(target.value[0] == 'A');
	assert(target.value[1] == 'B');
	assert(target.value[2] == 'C');
	assert(target.value[3] == 'D');
}

} // namespace

int main()
{
	// Exercise both policy-specialized scalar dispatchers. The one-character refill guarantees that neither case can
	// use the direct current-chunk path, so execution reaches the policy-dependent staging specialization above.
	test_refill_staging_policy<disabled_stack_policy>();
	test_refill_staging_policy<inline_stack_policy>();

	// Instantiate recursive pack propagation as well: both targets cross refill boundaries under the zero-stack policy.
	::scan_concept_harness::bounded_refill_source source;
	source.reset("ABCDEFGH", 1u);
	::scan_concept_harness::fixed_record_target<4u> first;
	::scan_concept_harness::fixed_record_target<4u> second;
	auto first_proxy{scan_alias_define(::fast_io::io_alias, first)};
	auto second_proxy{scan_alias_define(::fast_io::io_alias, second)};
	assert(::fast_io::operations::decay::scan_freestanding_decay<disabled_stack_policy>(
		input_stream_ref_define(source), first_proxy, second_proxy));
	assert(first.calls == 1u && second.calls == 1u);
}
