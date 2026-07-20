#include <fast_io_format.h>

int main()
{
	char storage[2u]{};
	::fast_io::obuffer_view output{storage, storage + 2u};
	::fast_io::fmt::wprint<L"{}">(output, L'x');
}
