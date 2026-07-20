#include <fast_io_format.h>

#include <array>

inline constexpr ::std::array<int, 256u> values{};

int main()
{
	// 256 fields of width 64 plus the range delimiters and separators produce
	// 16896 code units.  A known static result above 16 KiB is a contract error,
	// not permission to silently select the dynamic path.
	(void)::fast_io::fmt::concat_std<"{::064}">(
		::fast_io::fmt::static_arg<values>);
}
