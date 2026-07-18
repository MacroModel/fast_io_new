#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_BORROWED_ALIAS_MATRIX_RESTORE_FLOATING_MACRO
#endif
#include <fast_io.h>
#if defined(FAST_IO_BORROWED_ALIAS_MATRIX_RESTORE_FLOATING_MACRO)
#undef FAST_IO_DISABLE_FLOATING_POINT
#undef FAST_IO_BORROWED_ALIAS_MATRIX_RESTORE_FLOATING_MACRO
#endif
#include <fast_io_driver/mangling.h>

namespace
{

inline constexpr char long_literal[] =
	"a deliberately long literal whose alias crosses the sixty-four character bare-scatter threshold";

// This type satisfies the legacy shape-based alias extension point but intentionally supplies no lifetime marker.
// Its name documents the adversarial implementation permitted by that interface: data() could designate shared
// scratch whose contents are replaced when the next view is produced.
struct scratch_substring_view
{
	char *first{};
	::std::size_t count{};

	inline constexpr char *begin() const noexcept
	{
		return first;
	}
	inline constexpr char *end() const noexcept
	{
		return first + count;
	}
	inline constexpr char *data() const noexcept
	{
		return first;
	}
	inline constexpr ::std::size_t size() const noexcept
	{
		return count;
	}
	inline constexpr scratch_substring_view substr() const noexcept
	{
		return *this;
	}
};

struct void_alias_source
{};

[[maybe_unused]] inline void print_alias_define(::fast_io::io_alias_t, void_alias_source &) noexcept
{}

struct void_status_source
{};

[[maybe_unused]] inline void status_io_print_forward(
	::fast_io::io_alias_type_t<char>, void_status_source &) noexcept
{}

using long_literal_source = decltype(long_literal);
using long_literal_alias = decltype(::fast_io::print_alias_define(::fast_io::io_alias, long_literal));

static_assert(::std::same_as<long_literal_alias, ::fast_io::basic_io_scatter_t<char>>);
static_assert(::fast_io::borrowed_scatter_source<char, long_literal_source>);
static_assert(::fast_io::alias_printable<long_literal_source &>);

static_assert(::fast_io::borrowed_scatter_source<char, ::std::string>);
static_assert(::fast_io::borrowed_scatter_source<char, ::std::string_view>);
static_assert(::fast_io::alias_printable<::std::string &>);
static_assert(::fast_io::alias_printable<::std::string_view &>);

using fast_string = ::fast_io::containers::basic_string<char, ::fast_io::native_global_allocator>;
using fast_string_view = ::fast_io::containers::basic_string_view<char>;
using fast_cstring_view = ::fast_io::containers::basic_cstring_view<char>;

static_assert(::fast_io::borrowed_scatter_source<char, fast_string>);
static_assert(::fast_io::borrowed_scatter_source<char, fast_string_view>);
static_assert(::fast_io::borrowed_scatter_source<char, fast_cstring_view>);
static_assert(::fast_io::alias_printable<fast_string &>);
static_assert(::fast_io::alias_printable<fast_string_view &>);
static_assert(::fast_io::alias_printable<fast_cstring_view &>);

using c_str = ::fast_io::manipulators::basic_os_c_str<char>;
using sized_c_str = ::fast_io::manipulators::basic_os_c_str_with_known_size<char>;
using sized_text = ::fast_io::manipulators::basic_os_str_known_size_without_null_terminated<char>;

static_assert(::fast_io::borrowed_scatter_source<char, c_str>);
static_assert(::fast_io::borrowed_scatter_source<char, sized_c_str>);
static_assert(::fast_io::borrowed_scatter_source<char, sized_text>);
static_assert(::fast_io::alias_printable<c_str>);
static_assert(::fast_io::alias_printable<sized_c_str>);
static_assert(::fast_io::alias_printable<sized_text>);

static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::basic_obuffer_view<char>>);
static_assert(::fast_io::alias_printable<::fast_io::basic_obuffer_view<char> &>);
static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::basic_http_header_buffer<char>>);
static_assert(::fast_io::alias_printable<::fast_io::basic_http_header_buffer<char> &>);

static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::allocation_file_loader>);
static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::native_file_loader>);
static_assert(::fast_io::alias_printable<::fast_io::allocation_file_loader &>);
static_assert(::fast_io::alias_printable<::fast_io::native_file_loader &>);

#if defined(_WIN32) || defined(__CYGWIN__)
static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::nt_file_loader>);
static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::win32_file_loader>);
static_assert(::fast_io::alias_printable<::fast_io::nt_file_loader &>);
static_assert(::fast_io::alias_printable<::fast_io::win32_file_loader &>);
#endif

static_assert(::fast_io::borrowed_scatter_source<char, ::fast_io::cxa_demangle>);
static_assert(::fast_io::alias_printable<::fast_io::cxa_demangle &>);

static_assert(::std::ranges::contiguous_range<scratch_substring_view>);
static_assert(::std::same_as<
			  decltype(::fast_io::print_alias_define(::fast_io::io_alias, ::std::declval<scratch_substring_view &>())),
			  ::fast_io::basic_io_scatter_t<char>>);
static_assert(!::fast_io::borrowed_scatter_source<char, scratch_substring_view>);
static_assert(!::fast_io::alias_printable<scratch_substring_view &>);
static_assert(!::fast_io::alias_printable<void_alias_source &>);
static_assert(!::fast_io::status_io_print_forwardable<char, void_status_source &>);

} // namespace

int main()
{}
