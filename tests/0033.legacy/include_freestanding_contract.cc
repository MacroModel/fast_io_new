#include <cstddef>

#if !((__STDC_HOSTED__ == 1 &&                                \
	   (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	   !defined(_LIBCPP_FREESTANDING)) ||                     \
	  defined(FAST_IO_ENABLE_HOSTED_FEATURES))
namespace fast_io
{

// A true freestanding inclusion supplies its allocation backend before the
// public package entry forms the native allocator aliases. Declarations are
// sufficient because this compile-only contract performs no allocation.
class custom_global_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

} // namespace fast_io
#endif

#include <fast_io_legacy.h>

#include <concepts>

// Inclusion proof: the legacy umbrella must expose the common protocol plane
// even when standard stream adapters are unavailable. Before the split this
// name was absent because the complete header body was hosted-only.
static_assert(::std::same_as<typename ::fast_io::io_type_t<int>::type, int>);

int main()
{
	return 0;
}
