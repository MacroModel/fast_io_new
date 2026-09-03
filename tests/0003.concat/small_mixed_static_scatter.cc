#include <fast_io.h>
#include <fast_io_dsal/string.h>

#include <cstdlib>
#include <string_view>

namespace
{

inline void require_equal(::std::string_view actual, ::std::string_view expected) noexcept
{
	if (actual != expected)
	{
		::std::abort();
	}
}

template <typename string_type>
inline void verify_result(string_type const &result, ::std::string_view expected) noexcept
{
	require_equal({result.data(), result.size()}, expected);
}

} // namespace

int main()
{
	constexpr char static_text[]{'s', 't', 'a', 't', 'i', 'c'};
	constexpr char retained_text[]{'|', 'v', 'i', 'e', 'w', '|'};
	auto const static_scatter{
		::fast_io::manipulators::static_scatter_t<char, sizeof(static_text)>{static_text}};
	auto const retained_scatter{
		::fast_io::basic_io_scatter_t<char>{retained_text, sizeof(retained_text)}};

	// This three-leaf pack is the formal boundary exercised here: the static leaf is admitted only by
	// print_static_scatter_traits, the raw descriptor is retained verbatim, and the integer is a reserve leaf.  Their
	// exact concatenation proves that the small mixed planner measures and emits each admitted representation through
	// the same protocol and preserves source order.
	verify_result(::fast_io::concat_std(static_scatter, retained_scatter, 42u), "static|view|42");
	verify_result(::fast_io::concatln_std(static_scatter, retained_scatter, 42u), "static|view|42\n");
	verify_result(::fast_io::concat_fast_io(static_scatter, retained_scatter, 42u), "static|view|42");
	verify_result(::fast_io::concatln_fast_io(static_scatter, retained_scatter, 42u), "static|view|42\n");
}
