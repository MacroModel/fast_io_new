#include <cstddef>

// The configured-alias variants of this compile-time test provide the minimum allocator protocol before fast_io forms
// `native_*_allocator`. No test object calls these declarations; they exist solely to model a valid custom backend.
#if defined(FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR) || (__STDC_HOSTED__ == 0)
namespace fast_io
{
class custom_global_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};
} // namespace fast_io
#endif

#if defined(FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR)
namespace fast_io
{
class custom_thread_local_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};
} // namespace fast_io
#endif

#include <fast_io_device.h>

#include <span>
#include <type_traits>

namespace
{

struct dummy_handle
{};

struct unmarked_allocator
{
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

struct explicitly_cacheable_allocator
{
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

[[maybe_unused]] inline constexpr ::std::true_type prfch_cacheable_allocator_provenance_define(
	::fast_io::io_type_t<explicitly_cacheable_allocator>) noexcept
{
	return {};
}

template <typename allocator_type>
using input_traits = ::fast_io::basic_io_buffer_traits<
	::fast_io::buffer_mode::in, allocator_type, char, void>;

template <typename allocator_type>
using output_traits = ::fast_io::basic_io_buffer_traits<
	::fast_io::buffer_mode::out, allocator_type, void, char>;

template <typename allocator_type>
using input_output_traits = ::fast_io::basic_io_buffer_traits<
	::fast_io::buffer_mode::in | ::fast_io::buffer_mode::out | ::fast_io::buffer_mode::tie,
	allocator_type, char, char>;

template <typename allocator_type>
using input_buffer = ::fast_io::basic_io_buffer<dummy_handle, input_traits<allocator_type>>;

template <typename allocator_type>
using output_buffer = ::fast_io::basic_io_buffer<dummy_handle, output_traits<allocator_type>>;

template <typename allocator_type>
using input_output_buffer =
	::fast_io::basic_io_buffer<dummy_handle, input_output_traits<allocator_type>>;

template <typename owner_type>
using io_buffer_ref = ::fast_io::basic_io_buffer_ref<owner_type>;

using safe_input_buffer = input_buffer<explicitly_cacheable_allocator>;
using safe_output_buffer = output_buffer<explicitly_cacheable_allocator>;
using safe_input_output_buffer = input_output_buffer<explicitly_cacheable_allocator>;
using unsafe_input_buffer = input_buffer<unmarked_allocator>;
using unsafe_output_buffer = output_buffer<unmarked_allocator>;
using unsafe_input_output_buffer = input_output_buffer<unmarked_allocator>;

static_assert(::fast_io::details::prfch_cacheable_input_io_buffer_traits<
			  input_traits<explicitly_cacheable_allocator>>);
static_assert(!::fast_io::details::prfch_cacheable_output_io_buffer_traits<
			  input_traits<explicitly_cacheable_allocator>>);
static_assert(!::fast_io::details::prfch_cacheable_input_io_buffer_traits<
			  output_traits<explicitly_cacheable_allocator>>);
static_assert(::fast_io::details::prfch_cacheable_output_io_buffer_traits<
			  output_traits<explicitly_cacheable_allocator>>);
static_assert(::fast_io::details::prfch_cacheable_input_io_buffer_traits<
			  input_output_traits<explicitly_cacheable_allocator>>);
static_assert(::fast_io::details::prfch_cacheable_output_io_buffer_traits<
			  input_output_traits<explicitly_cacheable_allocator>>);
static_assert(!::fast_io::details::prfch_cacheable_input_io_buffer_traits<
			  input_traits<unmarked_allocator>>);
static_assert(!::fast_io::details::prfch_cacheable_output_io_buffer_traits<
			  output_traits<unmarked_allocator>>);

// Direction is a property of the buffered interface, not merely of mutable allocation. An input owner lends only its
// get area, an output owner lends only its put area, and an input/output owner proves the two independently.
static_assert(::fast_io::prfch_cacheable_read_provenance<safe_input_buffer>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<safe_input_buffer>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<safe_output_buffer>);
static_assert(::fast_io::prfch_cacheable_write_provenance<safe_output_buffer>);
static_assert(::fast_io::prfch_cacheable_read_write_provenance<safe_input_output_buffer>);

static_assert(::fast_io::prfch_cacheable_read_provenance<io_buffer_ref<safe_input_buffer>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<io_buffer_ref<safe_input_buffer>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<io_buffer_ref<safe_output_buffer>>);
static_assert(::fast_io::prfch_cacheable_write_provenance<io_buffer_ref<safe_output_buffer>>);
static_assert(::fast_io::prfch_cacheable_read_write_provenance<io_buffer_ref<safe_input_output_buffer>>);

static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_input_buffer>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<unsafe_output_buffer>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_input_output_buffer>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<unsafe_input_output_buffer>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<io_buffer_ref<unsafe_input_buffer>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<io_buffer_ref<unsafe_output_buffer>>);

// A transcode reference selects/rebuilds a decorated handle; it does not expose the owner's get/put cursor protocol.
// Leaving it unmarked prevents a control wrapper from being mistaken for a cacheable character range.
static_assert(!::fast_io::prfch_cacheable_read_provenance<
			  ::fast_io::basic_io_buffer_transcode_ref<safe_input_output_buffer>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<
			  ::fast_io::basic_io_buffer_transcode_ref<safe_input_output_buffer>>);

using safe_dynamic_output = ::fast_io::basic_generic_dynamic_output_buffer<
	char, 4096u, explicitly_cacheable_allocator>;
using unsafe_dynamic_output = ::fast_io::basic_generic_dynamic_output_buffer<
	char, 4096u, unmarked_allocator>;
using safe_dynamic_output_ref = ::fast_io::basic_dynamic_output_buffer_ref<safe_dynamic_output>;
using unsafe_dynamic_output_ref = ::fast_io::basic_dynamic_output_buffer_ref<unsafe_dynamic_output>;

// Even a large inline array can overflow, so the mixed inline/heap owner is not proved by its current stack state.
static_assert(!::fast_io::prfch_cacheable_read_provenance<safe_dynamic_output>);
static_assert(::fast_io::prfch_cacheable_write_provenance<safe_dynamic_output>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<safe_dynamic_output_ref>);
static_assert(::fast_io::prfch_cacheable_write_provenance<safe_dynamic_output_ref>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_dynamic_output>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<unsafe_dynamic_output>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<unsafe_dynamic_output_ref>);

using concat_buffer = ::fast_io::details::basic_concat_buffer<char>;
using concat_buffer_ref = ::fast_io::io_strlike_reference_wrapper<char, concat_buffer>;
static_assert(!::fast_io::prfch_cacheable_read_provenance<concat_buffer>);
static_assert(::fast_io::prfch_cacheable_write_provenance<concat_buffer> ==
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable);
static_assert(!::fast_io::prfch_cacheable_read_provenance<concat_buffer_ref>);
static_assert(::fast_io::prfch_cacheable_write_provenance<concat_buffer_ref> ==
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable);

using native_dynamic_output = ::fast_io::basic_dynamic_output_buffer<char>;
using native_dynamic_output_ref = ::fast_io::basic_dynamic_output_buffer_ref<native_dynamic_output>;
static_assert(::fast_io::prfch_cacheable_write_provenance<native_dynamic_output> ==
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable);
static_assert(::fast_io::prfch_cacheable_write_provenance<native_dynamic_output_ref> ==
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable);

// These objects expose caller-owned addresses or mappings. Ownership of the descriptor/view object is not ownership of
// ordinary cacheable storage, and a file mapping can have platform-specific cache/device semantics.
static_assert(!::fast_io::prfch_cacheable_read_provenance<char *>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<char *>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::std::span<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::std::span<char>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::basic_io_scatter_t<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::basic_io_scatter_t<char>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::basic_io_buffer_pointers<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::basic_io_buffer_pointers<char>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::basic_ibuffer_view<char>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::basic_ibuffer_view_ref<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::basic_obuffer_view<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::basic_obuffer_view_ref<char>>);

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && \
	  !defined(_LIBCPP_FREESTANDING)) ||                                             \
	 defined(FAST_IO_ENABLE_HOSTED_FEATURES)) &&                                     \
	!defined(__AVR__)
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::native_io_observer>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::native_io_observer>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::native_file_loader>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::native_file_loader>);

// Hosted buffered-file aliases are the same proved owner template around an unmarked OS handle. The buffer layer gains
// only the direction backed by its allocator; the raw observer above remains unmarked.
static_assert(::fast_io::prfch_cacheable_read_provenance<::fast_io::ibuf_file> ==
			  ::fast_io::details::prfch_native_global_allocator_cacheable);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::ibuf_file>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::obuf_file>);
static_assert(::fast_io::prfch_cacheable_write_provenance<::fast_io::obuf_file> ==
			  ::fast_io::details::prfch_native_global_allocator_cacheable);
static_assert(::fast_io::prfch_cacheable_read_write_provenance<::fast_io::iobuf_file> ==
			  ::fast_io::details::prfch_native_global_allocator_cacheable);
#endif

} // namespace

int main()
{}
