#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concatf_std<"%3$*1$.*2$f">(8, 2, 3.125, 99);
}
