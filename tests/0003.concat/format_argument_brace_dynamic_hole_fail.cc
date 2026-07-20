#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concat_std<"{0:{2}.{3}f}">(3.125, 99, 8, 2);
}
