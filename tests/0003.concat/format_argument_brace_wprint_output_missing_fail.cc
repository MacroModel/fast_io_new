#include <fast_io_format.h>

int main()
{
	wchar_t storage[2u]{};
	::fast_io::basic_obuffer_view<wchar_t> output{storage, storage + 2u};
	::fast_io::fmt::wprint<L"{}{}">(output, L'x');
}
