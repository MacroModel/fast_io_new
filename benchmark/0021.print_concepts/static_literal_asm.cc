#include <fast_io_core.h>

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
	::fast_io::operations::print_freestanding<false>(
		static_literal_probe_sink{}, "a", "bbb", ::fast_io::mnp::chvw('c'));
}

/// Semantically identical control with one source literal.
extern "C" void fast_io_static_literal_whole_probe()
{
	::fast_io::operations::print_freestanding<false>(
		static_literal_probe_sink{}, "abbbc");
}

int main()
{
	fast_io_static_literal_split_probe();
	fast_io_static_literal_whole_probe();
}
