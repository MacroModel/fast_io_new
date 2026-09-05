#include <cstddef>

#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 0
#error "This contract must be compiled with the compiler's real freestanding mode."
#endif

namespace fast_io
{

// The compile contract supplies the allocation customization required by a
// genuinely freestanding package; no definition is needed because this test
// deliberately exercises allocation-free conversion paths only.
class custom_global_allocator
{
public:
	static void *allocate(::std::size_t) noexcept;
	static void deallocate(void *) noexcept;
};

} // namespace fast_io

#include <fast_io_core.h>

#include <type_traits>

using narrow_to_result = ::fast_io::basic_to_chars_result<char>;
using narrow_from_result = ::fast_io::basic_from_chars_result<char>;

// Both the standard and local result paths are two-member aggregate protocols.
// These properties make pointer/error transport independent of padding and of
// the implementation-defined numeric representation of the error enumeration.
static_assert(::std::is_aggregate_v<narrow_to_result>);
static_assert(::std::is_standard_layout_v<narrow_to_result>);
static_assert(::std::is_trivially_copyable_v<narrow_to_result>);
static_assert(::std::is_aggregate_v<narrow_from_result>);
static_assert(::std::is_standard_layout_v<narrow_from_result>);
static_assert(::std::is_trivially_copyable_v<narrow_from_result>);
static_assert(offsetof(narrow_to_result, ptr) == 0u);
static_assert(offsetof(narrow_from_result, ptr) == 0u);
static_assert(offsetof(narrow_to_result, ec) >= sizeof(char *));
static_assert(offsetof(narrow_from_result, ec) >= sizeof(char const *));
static_assert(::fast_io::charconv_errc{} !=
			  ::fast_io::charconv_errc::invalid_argument);

constexpr bool integer_charconv_contract() noexcept
{
	char buffer[2]{};
	auto const printed{::fast_io::to_chars(buffer, buffer + 2, 255u, 16)};
	if (printed.ptr != buffer + 2 || printed.ec != ::fast_io::charconv_errc{} ||
		buffer[0] != 'f' || buffer[1] != 'f')
	{
		return false;
	}

	unsigned parsed{};
	auto const scanned{::fast_io::from_chars(buffer, buffer + 2, parsed, 16)};
	if (scanned.ptr != buffer + 2 || scanned.ec != ::fast_io::charconv_errc{} ||
		parsed != 255u)
	{
		return false;
	}

	char short_buffer[1]{};
	auto const short_result{
		::fast_io::to_chars(short_buffer, short_buffer + 1, 42u)};
	if (short_result.ptr != short_buffer + 1 ||
		short_result.ec != ::fast_io::charconv_errc::value_too_large)
	{
		return false;
	}

	char const invalid_text[]{'x'};
	auto const invalid_result{
		::fast_io::from_chars(invalid_text, invalid_text + 1, parsed)};
	if (invalid_result.ec != ::fast_io::charconv_errc::invalid_argument)
	{
		return false;
	}

	char const overflow_text[]{"999999999999999999999999999999999999"};
	auto const overflow_result{::fast_io::from_chars(
		overflow_text, overflow_text + sizeof(overflow_text) - 1u, parsed)};
	return overflow_result.ec ==
		   ::fast_io::charconv_errc::result_out_of_range;
}

static_assert(integer_charconv_contract());

int main()
{
	return integer_charconv_contract() ? 0 : 1;
}
