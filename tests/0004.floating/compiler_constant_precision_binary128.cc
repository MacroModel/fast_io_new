#include "compiler_constant_precision_matrix.h"

int main()
{
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	return ::fast_io::tests::compiler_constant_precision::run<__float128>() ? 0 : 1;
#else
	return 0;
#endif
}
