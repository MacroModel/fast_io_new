#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concat_std<"{}{1}">(1, 2);
}
