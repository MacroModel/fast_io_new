#include <fast_io_format.h>

int main()
{
	// Static replacement selection is downstream of the same large-program
	// proof and must not bypass rejection of an unreferenced static argument.
	(void)::fast_io::fmt::concat_std<
		"{1}{1}{1}{1}{1}{1}{1}{1}{1}">(
		::fast_io::mnp::static_arg<0>,
		::fast_io::mnp::static_arg<1>);
}
