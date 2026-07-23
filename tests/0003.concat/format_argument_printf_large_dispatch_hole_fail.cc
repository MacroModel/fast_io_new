#include <fast_io_format.h>

int main()
{
	// Nine replacement operations select the large-program dispatcher. Its
	// printf validator must reject the unreferenced first argument.
	char storage[16u]{};
	::fast_io::basic_obuffer_view<char> output{storage, storage + 16u};
	::fast_io::fmt::printf<
		"%2$d%2$d%2$d%2$d%2$d%2$d%2$d%2$d%2$d">(output, 0, 1);
}
