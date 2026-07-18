#include <cassert>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

inline constexpr char payload[]{"ab"};
using bounded_scatter = ::fast_io::manipulators::small_scatter_t<char, 2u>;

static_assert(::fast_io::reserve_printable<char, bounded_scatter>);
static_assert(!::std::is_aggregate_v<bounded_scatter>);
static_assert(!::fast_io::reserve_printable<
	char, ::fast_io::manipulators::small_scatter_t<char, 0u>>);

inline constexpr bounded_scatter full_extent{payload, 2u};
inline constexpr bounded_scatter empty_extent{payload, 0u};
static_assert(full_extent.len == 2u && empty_extent.len == 0u);

} // namespace

int main()
{
	assert(::fast_io::concat_std(full_extent) == "ab");
	assert(::fast_io::concat_std(empty_extent).empty());
	auto converted{::fast_io::mnp::code_cvt(full_extent)};
	assert(converted.reference.base == payload && converted.reference.len == 2u);
}
