#define FAST_IO_ENABLE_HOSTED_FEATURES

#include <fast_io_legacy.h>

#include <concepts>
#include <streambuf>

// Override proof: FAST_IO_ENABLE_HOSTED_FEATURES intentionally selects both
// guarded phases even when the compiler reports a freestanding translation
// unit, so the public streambuf and filebuf adapters remain a single coherent
// hosted package.
static_assert(::std::same_as<
			  typename ::fast_io::streambuf_io_observer::streambuf_type,
			  ::std::streambuf>);
static_assert(::std::same_as<
			  typename ::fast_io::filebuf_io_observer::streambuf_type,
			  ::std::filebuf>);

int main()
{
	return 0;
}
