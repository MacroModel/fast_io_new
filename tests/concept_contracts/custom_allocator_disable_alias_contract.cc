#define FAST_IO_DISABLE_CUSTOM_THREAD_LOCAL_ALLOCATOR
#define FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR

#include <cstddef>

namespace fast_io
{

// The public allocation umbrella requires a complete selected custom backend.
// Declarations are sufficient because this compile-only contract never
// performs an allocation.
class custom_global_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

} // namespace fast_io

#include <fast_io_core.h>

#include <concepts>

// Configuration proof: disabling the separate thread-local implementation and
// selecting the thread-local policy must route both the public alias and the
// native adapter to the caller-provided global allocator type.
static_assert(::std::same_as<::fast_io::custom_thread_local_allocator,
							 ::fast_io::custom_global_allocator>);
static_assert(::std::same_as<
			  typename ::fast_io::native_thread_local_allocator::allocator_type,
			  ::fast_io::custom_global_allocator>);

int main()
{}
