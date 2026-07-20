#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concat_std<"{value}">(
		::fast_io::fmt::arg<"value">(1),
		::fast_io::fmt::arg<"value">(2));
}
