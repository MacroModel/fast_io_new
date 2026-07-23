#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

#if defined(__has_builtin)
#define FAST_IO_TEST_HAS_BUILTIN_CONSTANT_P \
	__has_builtin(__builtin_constant_p)
#else
#define FAST_IO_TEST_HAS_BUILTIN_CONSTANT_P 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
static_assert(!FAST_IO_TEST_HAS_BUILTIN_CONSTANT_P,
	"native MSVC must not enter the GNU optimizer-query protocol");

namespace
{

using boolean_source = decltype(::fast_io::mnp::boolalpha(true));
using precision_source = decltype(::fast_io::mnp::fixed<
	::fast_io::manipulators::floating_precision::
		fractional_preserve_trailing_zero>(12.44, 6u));

// Without a native optimizer query, ordinary value sources must fail before
// replacement aliases, materializers, or discarded true-arm consumers exist.
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<char, int>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<char, double>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<char, ::std::string>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, ::std::string_view>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, boolean_source>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, ::fast_io::unix_timestamp>);
static_assert(!::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, precision_source>);

#if _MSC_VER >= 1932
using static_integer_source = decltype(::fast_io::mnp::static_arg<42>);
using static_text_source = decltype(
	::fast_io::mnp::static_arg<"static-text">);

// A static argument carries its value in the type and is therefore independent
// from __builtin_constant_p. Its complete consteval provider contract admits
// the static replacement while ordinary sources above remain query-free.
static_assert(::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, static_integer_source>);
static_assert(::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_candidate_v<
		char, static_text_source>);
static_assert(::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_gate<char>(
		::fast_io::mnp::static_arg<42>));
static_assert(::fast_io::operations::decay::
	print_compiler_constant_pre_normalization_gate<char>(
		::fast_io::mnp::static_arg<"static-text">));
#endif

} // namespace
#endif

int main() noexcept
{
	return 0;
}

#undef FAST_IO_TEST_HAS_BUILTIN_CONSTANT_P
