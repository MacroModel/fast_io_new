#include <fast_io_format.h>

#include <string_view>

int main()
{
	::std::string_view format{"{}"};
	(void)::fast_io::fmt::concat_std(format, 1);
}
