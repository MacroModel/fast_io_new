#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concatf_std<"%3$*1$.*4$f">(8, 99, 3.125, 2);
}
