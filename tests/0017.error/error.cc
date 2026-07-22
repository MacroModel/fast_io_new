#include <fast_io.h>
#include <fast_io_device.h>

using namespace fast_io::io;

static_assert(::fast_io::status_io_print_forwardable<char, ::fast_io::freestanding::errc>);
static_assert(::fast_io::reserve_printable<char, ::fast_io::parse_code>);
static_assert(!::fast_io::scatter_printable<char, ::fast_io::parse_code>);

#if defined(_WIN32)
static_assert(::fast_io::reserve_printable<char, ::fast_io::win32_code>);
static_assert(!::fast_io::scatter_printable<char, ::fast_io::win32_code>);
#if !defined(_WIN32_WINDOWS)
static_assert(::fast_io::reserve_printable<char, ::fast_io::nt_code>);
static_assert(!::fast_io::scatter_printable<char, ::fast_io::nt_code>);
#endif
#endif

int main()
try
{
	fast_io::u8ibuf_file ibf("not_exist.txt");
}
catch (fast_io::error e)
{
	if (e == std::errc::no_such_file_or_directory)
	{
		perr("errc:no_such_file_or_directory\n");
	}
	return 1;
}
