#include <fast_io_format.h>

int main()
{
	// Nine replacement operations select the large-program dispatcher. Its
	// argument-domain proof must reject the unreferenced first argument.
	(void)::fast_io::fmt::concat_std<
		"{1}{1}{1}{1}{1}{1}{1}{1}{1}">(0, 1);
}
