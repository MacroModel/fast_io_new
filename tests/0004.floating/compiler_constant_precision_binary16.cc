#include "compiler_constant_precision_matrix.h"

int main()
{
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	return ::fast_io::tests::compiler_constant_precision::run<_Float16>() ? 0 : 1;
#else
	return 0;
#endif
}
