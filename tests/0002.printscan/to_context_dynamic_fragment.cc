#include "scan_concept_support.h"

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
}
