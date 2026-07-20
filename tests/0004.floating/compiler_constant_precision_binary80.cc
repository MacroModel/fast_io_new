#include <limits>

#include "compiler_constant_precision_matrix.h"

int main()
{
	if constexpr (::std::numeric_limits<long double>::digits == 64 &&
		::std::numeric_limits<long double>::max_exponent == 16384)
	{
		return ::fast_io::tests::compiler_constant_precision::run<long double>()
			? 0
			: 1;
	}
	return 0;
}
