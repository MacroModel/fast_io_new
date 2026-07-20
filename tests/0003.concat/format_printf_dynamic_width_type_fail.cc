#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concatf_std<"%*d">(3.5, 42);
}
