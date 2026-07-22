#include <string>
#include <string_view>
#include <deque>
#include <set>
#include <unordered_set>
#include <fast_io.h>
#include <fast_io_dsal/vector.h>

int main()
{
	using namespace fast_io::io;
	using namespace fast_io::mnp;
	using namespace std::literals;
	fast_io::vector<int> i{1};
	std::deque d{1., 2.};
	std::set s{1.f, 2.f, 3.f};
	std::unordered_set u{1ull, 2ull, 3ull, 4ull};
	auto os_separator{fast_io::mnp::os_c_str(" ")};
	auto string_separator{"&"s};
	println(rgvw(i, " "));
	println(rgvw(d, os_separator));
	println(rgvw(s, string_separator));
	println(rgvw(u, " | "sv));
}
