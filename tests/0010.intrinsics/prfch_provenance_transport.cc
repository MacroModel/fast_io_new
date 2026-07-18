#include <cstddef>

// Custom-allocation configuration tests supply the minimum protocol before fast_io forms its native aliases. These
// declarations are never called by this compile-time test; they merely model a valid user-provided backend.
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

#include <fast_io_dsal/string.h>

#include <concepts>
#include <type_traits>
#include <utility>

namespace
{

using raw_scatter = ::fast_io::basic_io_scatter_t<char>;
using proved_scatter = ::fast_io::basic_prfch_cacheable_io_scatter_t<char>;

struct custom_allocator
{};

struct wrong_marker_allocator
{};

struct explicitly_cacheable_allocator
{};

// Returning a merely bool-convertible value must not satisfy an exact-true marker protocol.
[[maybe_unused]] inline constexpr bool prfch_cacheable_allocator_provenance_define(
	::fast_io::io_type_t<wrong_marker_allocator>) noexcept
{
	return true;
}

[[maybe_unused]] inline constexpr ::std::true_type prfch_cacheable_allocator_provenance_define(
	::fast_io::io_type_t<explicitly_cacheable_allocator>) noexcept
{
	return {};
}

inline constexpr char long_literal[]{
	"This literal deliberately crosses the static-scatter boundary and therefore uses a two-word descriptor."};

using literal_alias = ::std::remove_cvref_t<decltype(::fast_io::io_print_alias(long_literal))>;
using explicit_small_scatter = ::std::remove_cvref_t<decltype(::fast_io::manipulators::small_scatter(long_literal))>;
using short_literal_alias = ::std::remove_cvref_t<decltype(::fast_io::io_print_alias("short"))>;

using native_string = ::fast_io::containers::basic_string<char, ::fast_io::native_global_allocator>;
using native_thread_local_string =
	::fast_io::containers::basic_string<char, ::fast_io::native_thread_local_allocator>;
using custom_string = ::fast_io::containers::basic_string<char, custom_allocator>;
using explicitly_cacheable_string =
	::fast_io::containers::basic_string<char, explicitly_cacheable_allocator>;
using string_view = ::fast_io::containers::basic_string_view<char>;

template <typename T>
using direct_alias_result = decltype(print_alias_define(
	::fast_io::io_alias, ::std::declval<T>()));

using native_string_alias = ::std::remove_cvref_t<direct_alias_result<native_string const &>>;
using native_thread_local_string_alias =
	::std::remove_cvref_t<direct_alias_result<native_thread_local_string const &>>;
using custom_string_alias = ::std::remove_cvref_t<direct_alias_result<custom_string const &>>;
using explicitly_cacheable_string_alias =
	::std::remove_cvref_t<direct_alias_result<explicitly_cacheable_string const &>>;
using string_view_alias = ::std::remove_cvref_t<direct_alias_result<string_view const &>>;

using safe_pack = ::fast_io::manipulators::pack_t<
	proved_scatter, ::fast_io::io_null_t, ::fast_io::parameter<proved_scatter const &>>;
using unsafe_pack = ::fast_io::manipulators::pack_t<proved_scatter, raw_scatter>;

using safe_condition =
	::fast_io::manipulators::condition<proved_scatter, ::fast_io::io_null_t>;
using unsafe_condition =
	::fast_io::manipulators::condition<proved_scatter, raw_scatter>;

using fixed_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left, proved_scatter>;
using fixed_width_fill = ::fast_io::manipulators::width_ch_t<
	::fast_io::manipulators::scalar_placement::right, proved_scatter, char>;
using runtime_width = ::fast_io::manipulators::width_runtime_t<proved_scatter>;
using runtime_width_fill = ::fast_io::manipulators::width_runtime_ch_t<proved_scatter, char>;
using unsafe_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left, raw_scatter>;

static_assert(sizeof(proved_scatter) == sizeof(raw_scatter));
static_assert(alignof(proved_scatter) == alignof(raw_scatter));
static_assert(offsetof(proved_scatter, base) == offsetof(raw_scatter, base));
static_assert(offsetof(proved_scatter, len) == offsetof(raw_scatter, len));
static_assert(::std::is_standard_layout_v<proved_scatter>);
static_assert(::std::is_trivially_copyable_v<proved_scatter>);
static_assert(!::std::is_convertible_v<raw_scatter, proved_scatter>);

static_assert(::fast_io::prfch_cacheable_read_provenance<proved_scatter>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<raw_scatter>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<proved_scatter>);
static_assert(::fast_io::scatter_printable<char, proved_scatter>);
static_assert(::fast_io::borrowed_scatter_source<char, proved_scatter>);
static_assert(::fast_io::scatter_output_state_independent<char, proved_scatter>);
static_assert(::fast_io::scatter_direct_print_equivalent<char, proved_scatter>);
static_assert(::fast_io::copy_stable_borrowed_print_source<char, proved_scatter>);

static_assert(::fast_io::prfch_cacheable_read_provenance<
			  ::fast_io::parameter<proved_scatter>>);
static_assert(::fast_io::prfch_cacheable_read_provenance<
			  ::fast_io::parameter<proved_scatter const &>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<
			  ::fast_io::parameter<raw_scatter &>>);

static_assert(::std::same_as<literal_alias, raw_scatter>);
static_assert(::std::same_as<explicit_small_scatter, raw_scatter>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<literal_alias>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<short_literal_alias>);
static_assert(::std::same_as<
			  native_string_alias,
			  ::std::conditional_t<::fast_io::details::prfch_native_global_allocator_cacheable,
								   proved_scatter, raw_scatter>>);
static_assert(::std::same_as<
			  native_thread_local_string_alias,
			  ::std::conditional_t<::fast_io::details::prfch_native_thread_local_allocator_cacheable,
								   proved_scatter, raw_scatter>>);
static_assert(::std::same_as<custom_string_alias, raw_scatter>);
static_assert(::std::same_as<explicitly_cacheable_string_alias, proved_scatter>);
static_assert(::std::same_as<string_view_alias, raw_scatter>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<custom_string_alias>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<string_view_alias>);

static_assert(::fast_io::prfch_cacheable_allocator_provenance<
				  ::fast_io::native_global_allocator> ==
			  ::fast_io::details::prfch_native_global_allocator_cacheable);
static_assert(::fast_io::prfch_cacheable_allocator_provenance<
				  ::fast_io::native_thread_local_allocator> ==
			  ::fast_io::details::prfch_native_thread_local_allocator_cacheable);
static_assert(!::fast_io::prfch_cacheable_allocator_provenance<custom_allocator>);
static_assert(!::fast_io::prfch_cacheable_allocator_provenance<wrong_marker_allocator>);
static_assert(::fast_io::prfch_cacheable_allocator_provenance<explicitly_cacheable_allocator>);

static_assert(::fast_io::prfch_cacheable_read_provenance<safe_pack>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_pack>);
static_assert(::fast_io::prfch_cacheable_read_provenance<safe_condition>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_condition>);
static_assert(::fast_io::prfch_cacheable_read_provenance<fixed_width>);
static_assert(::fast_io::prfch_cacheable_read_provenance<fixed_width_fill>);
static_assert(::fast_io::prfch_cacheable_read_provenance<runtime_width>);
static_assert(::fast_io::prfch_cacheable_read_provenance<runtime_width_fill>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<unsafe_width>);

inline constexpr bool scatter_projection_is_lossless() noexcept
{
	char const text[]{"proof"};
	proved_scatter const proved{text, 5u};
	auto const raw{::fast_io::print_scatter_define(
		::fast_io::io_reserve_type<char, proved_scatter>, proved)};
	return raw.base == text && raw.len == 5u;
}

static_assert(scatter_projection_is_lossless());

} // namespace

int main()
{
	return 0;
}
