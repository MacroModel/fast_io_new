#include <fast_io_format.h>

int main()
{
	(void)::fast_io::fmt::concat_std<"{left}">(
		::fast_io::mnp::static_arg<"left", 1>,
		::fast_io::mnp::static_arg<"right", 2>);
}
