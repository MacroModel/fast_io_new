#include <cstddef>
#include <cstdlib>

#include <fast_io.h>
#include <fast_io_format.h>

namespace
{

[[noreturn]] void fail() noexcept
{
	::std::abort();
}

void validate(char const *first, char const *last)
{
	if (last - first != 96)
	{
		fail();
	}

	for (::std::size_t index{}; index != 5u; ++index)
	{
		if (first[index] != "short"[index])
		{
			fail();
		}
	}
	if (first[5] != '|')
	{
		fail();
	}
	for (::std::size_t index{}; index != 8u; ++index)
	{
		if (first[6u + index] != static_cast<char>('1' + index))
		{
			fail();
		}
	}
	if (first[14] != '|' || first[15] != '|')
	{
		fail();
	}
	for (::std::size_t index{}; index != 80u; ++index)
	{
		if (first[16u + index] != static_cast<char>('a' + index % 26u))
		{
			fail();
		}
	}
}

} // namespace

int main()
{
	char short_terminated[8]{'s', 'h', 'o', 'r', 't'};
	char short_full[8]{'1', '2', '3', '4', '5', '6', '7', '8'};
	char empty[1]{};
	char long_terminated[96]{};
	for (::std::size_t index{}; index != 80u; ++index)
	{
		long_terminated[index] = static_cast<char>('a' + index % 26u);
	}

	char const(&short_terminated_view)[8]{short_terminated};
	char const(&short_full_view)[8]{short_full};
	char const(&empty_view)[1]{empty};
	char const(&long_terminated_view)[96]{long_terminated};

	// Exercise both format frontends: each must preserve the source array extent
	// until the measured C-string is materialized by the core print layer.
	{
		char storage[128]{};
		::fast_io::obuffer_view output{storage, storage + sizeof(storage)};
		::fast_io::fmt::print<"{:s}|{:s}|{:s}|{:s}">(
			output, short_terminated_view, short_full_view, empty_view,
			long_terminated_view);
		validate(storage, output.curr_ptr);
	}
	{
		char storage[128]{};
		::fast_io::obuffer_view output{storage, storage + sizeof(storage)};
		::fast_io::fmt::printf<"%s|%s|%s|%s">(
			output, short_terminated_view, short_full_view, empty_view,
			long_terminated_view);
		validate(storage, output.curr_ptr);
	}
}
