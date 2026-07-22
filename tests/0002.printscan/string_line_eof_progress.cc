#include <cassert>
#include <string>
#include <string_view>

#include <fast_io.h>
#include <fast_io_dsal/string.h>

namespace
{

template <typename string_type>
void check_line_get_eof()
{
	using namespace ::fast_io::io;

	{
		::std::string_view source{"last line"};
		::fast_io::ibuffer_view input{source};
		string_type line{"stale"};

		assert(scan<true>(input, ::fast_io::mnp::line_get(line)));
		assert(line.size() == source.size());
		assert((::std::string_view{line.data(), line.size()} == source));
		assert(!scan<true>(input, ::fast_io::mnp::line_get(line)));
	}

	{
		::std::string_view source{"\n"};
		::fast_io::ibuffer_view input{source};
		string_type line{"stale"};

		assert(scan<true>(input, ::fast_io::mnp::line_get(line)));
		assert(line.empty());
		assert(!scan<true>(input, ::fast_io::mnp::line_get(line)));
	}
}

void check_strlike_line_get_eof()
{
	using namespace ::fast_io::io;

	::std::string_view source{"last line"};
	::fast_io::ibuffer_view input{source};
	::fast_io::string line{"stale"};

	assert(scan<true>(input, ::fast_io::mnp::strlike_line_get(line)));
	assert((::std::string_view{line.data(), line.size()} == source));
	assert(!scan<true>(input, ::fast_io::mnp::strlike_line_get(line)));
}

} // namespace

int main()
{
	check_line_get_eof<::std::string>();
	check_line_get_eof<::fast_io::string>();
	check_strlike_line_get_eof();
}
