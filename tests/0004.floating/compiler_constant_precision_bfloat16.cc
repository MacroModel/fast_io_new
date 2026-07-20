#include "compiler_constant_precision_matrix.h"

int main()
{
#if defined(__GNUC__) && !defined(__clang__) && defined(__BFLT16_MANT_DIG__)
	return ::fast_io::tests::compiler_constant_precision::run<__bf16>() ? 0 : 1;
#elif defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	return ::fast_io::tests::compiler_constant_precision::run<__bf16>() ? 0 : 1;
#else
	return 0;
#endif
}
