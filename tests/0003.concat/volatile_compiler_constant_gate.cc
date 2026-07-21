#include <fast_io.h>
#include <fast_io_device.h>
#include <fast_io_format.h>

#include <string_view>

int main()
{
	volatile int integer{-42};
	volatile double floating{3.5};

	char io_storage[64]{};
	::fast_io::obuffer_view io_output{io_storage, io_storage + sizeof(io_storage)};
	::fast_io::io::print(io_output, integer, "|", floating);
	if (::std::string_view(io_storage, io_output.curr_ptr) != "-42|3.5")
	{
		return 1;
	}

	auto io_concat{::fast_io::concat_std(integer, "|", floating)};
	if (io_concat != "-42|3.5")
	{
		return 2;
	}

	char format_storage[64]{};
	::fast_io::obuffer_view format_output{
		format_storage, format_storage + sizeof(format_storage)};
	::fast_io::fmt::print<"{}|{}">(format_output, integer, floating);
	if (::std::string_view(format_storage, format_output.curr_ptr) != "-42|3.5")
	{
		return 3;
	}

	auto format_concat{
		::fast_io::fmt::concat_std<"{}|{}">(integer, floating)};
	if (format_concat != "-42|3.5")
	{
		return 4;
	}
}
