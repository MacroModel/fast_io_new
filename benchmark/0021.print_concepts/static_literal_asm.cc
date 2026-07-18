#include <fast_io.h>

namespace
{

struct static_literal_probe_sink
{
	using output_char_type = char;
};

inline constexpr static_literal_probe_sink
output_stream_ref_define(static_literal_probe_sink sink) noexcept
{
	return sink;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
inline void write_all_overflow_define(
	static_literal_probe_sink, char const *first, char const *last) noexcept
{
	// The barrier observes only the final contiguous range. It prevents dead-output elimination without measuring a
	// syscall, checksum, or payload copy inside the boundary, so differences in the exported wrappers are exclusively
	// print composition and materialization code.
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(first), "r"(last) : "memory");
#else
	(void)first;
	(void)last;
#endif
}

} // namespace

/// Split spelling used by macro-selected and delimiter-heavy output sites.
/// The optimized wrapper should contain the same payload-store shape as the consolidated spelling below.
extern "C" void fast_io_static_literal_split_probe()
{
	::fast_io::print(
		static_literal_probe_sink{}, "a", "bbb", ::fast_io::mnp::chvw('c'));
}

/// Semantically identical control with one source literal.
extern "C" void fast_io_static_literal_whole_probe()
{
	::fast_io::print(static_literal_probe_sink{}, "abbbc");
}

/// Line-owned spelling used by `println`.  The type-level literal extent and the line policy should remain visible
/// through the reserve materializer so the back end can merge the final newline with the adjacent literal stores.
extern "C" void fast_io_static_literal_line_probe()
{
	::fast_io::println(static_literal_probe_sink{}, "abc");
}

/// Semantically identical control with the newline already present in the source literal.
extern "C" void fast_io_static_literal_embedded_line_probe()
{
	::fast_io::print(static_literal_probe_sink{}, "abc\n");
}

int main()
{
	fast_io_static_literal_split_probe();
	fast_io_static_literal_whole_probe();
	fast_io_static_literal_line_probe();
	fast_io_static_literal_embedded_line_probe();
}
