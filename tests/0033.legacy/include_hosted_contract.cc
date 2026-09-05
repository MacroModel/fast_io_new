#include <fast_io_legacy.h>

#include <concepts>
#include <streambuf>
#include <string>

// Hosted first-inclusion proof: the standard stream headers must precede the
// package entry so its feature-sensitive standard-library adapters are
// installed, while the guarded legacy phase must still expose streambuf and
// filebuf observers.
static_assert(::std::same_as<
			  typename ::fast_io::streambuf_io_observer::streambuf_type,
			  ::std::streambuf>);
static_assert(::std::same_as<
			  typename ::fast_io::filebuf_io_observer::streambuf_type,
			  ::std::filebuf>);
static_assert(::fast_io::alias_printable<::std::string &>);

int main()
{
	return 0;
}
