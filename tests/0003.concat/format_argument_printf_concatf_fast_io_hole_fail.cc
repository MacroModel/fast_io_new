#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concatf_fast_io<"%1$s%1$s">("x", "y");
}
