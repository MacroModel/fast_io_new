#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concatf_std<"%.*f">('2', 3.125);
}
