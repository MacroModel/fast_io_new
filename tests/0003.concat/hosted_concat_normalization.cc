#include <cassert>
#include <cstddef>

#include <fast_io.h>

namespace hosted_concat_normalization
{

struct concat_only_source
{};

struct unsupported_semantic_leaf
{};

struct concat_leaf
{};

using rejected_print_pack = decltype(::fast_io::mnp::pack(unsupported_semantic_leaf{}));

inline constexpr rejected_print_pack print_alias_define(
	::fast_io::io_alias_t, concat_only_source &) noexcept
{
	return ::fast_io::mnp::pack(unsupported_semantic_leaf{});
}

inline constexpr concat_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, rejected_print_pack &&) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, concat_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, concat_leaf>, char *iter,
	concat_leaf) noexcept
{
	*iter = 'C';
	return iter + 1;
}

using result_type = ::fast_io::details::basic_ct_string<char>;

// Public print detects the alias-produced semantic pack and deliberately
// bypasses its rvalue status replacement. Concat instead owns a raw
// alias/status phase, so using the print proof to admit this helper was a real
// false negative even though concat's executable replacement is printable.
static_assert(!::fast_io::operations::defines::
				  print_freestanding_params_okay<char, concat_only_source &>);
static_assert(::fast_io::basic_general_concat_checked_available<
			  false, char, result_type, concat_only_source &>());

struct print_only_source
{};

struct printable_semantic_leaf
{};

struct unsupported_concat_leaf
{};

using accepted_print_pack = decltype(::fast_io::mnp::pack(printable_semantic_leaf{}));

inline constexpr accepted_print_pack print_alias_define(
	::fast_io::io_alias_t, print_only_source &) noexcept
{
	return ::fast_io::mnp::pack(printable_semantic_leaf{});
}

inline constexpr unsupported_concat_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, accepted_print_pack &&) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, printable_semantic_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, printable_semantic_leaf>, char *iter,
	printable_semantic_leaf) noexcept
{
	*iter = 'P';
	return iter + 1;
}

// This is the converse mismatch: print's semantic-input rule accepts the
// transported pack, while concat's actual raw status replacement is not
// printable to any selected destination. The hosted helpers must use the
// latter proof and fail closed before selecting a concat body.
static_assert(::fast_io::operations::defines::
				  print_freestanding_params_okay<char, print_only_source &>);
static_assert(!::fast_io::basic_general_concat_checked_available<
			  false, char, result_type, print_only_source &>());

void test_filesystem_concat_helper()
{
	concat_only_source source;
	auto result{::fast_io::details::concat_ct<char>(source)};
	assert(result.size() == 1u);
	assert(result.data()[0] == 'C');
	(void)result;
}

#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSDOS__) && \
	!defined(__NEWLIB__) && !defined(__wasi__) && !defined(_PICOLIBC_)
void test_posix_process_arg_helper()
{
	using guard_type = ::fast_io::details::cstr_guard<char>;
	::fast_io::containers::vector<
		guard_type, ::fast_io::native_global_allocator>
		arguments;
	::fast_io::details::construct_posix_process_argenvs_decay_singal<0u>(
		arguments, concat_only_source{});
	assert(arguments.size() == 1u);
	assert(arguments[0u].cstr != nullptr);
	assert(arguments[0u].cstr[0u] == 'C');
	assert(arguments[0u].cstr[1u] == '\0');
	(void)arguments;
}
#endif

} // namespace hosted_concat_normalization

int main()
{
	hosted_concat_normalization::test_filesystem_concat_helper();
#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSDOS__) && \
	!defined(__NEWLIB__) && !defined(__wasi__) && !defined(_PICOLIBC_)
	hosted_concat_normalization::test_posix_process_arg_helper();
#endif
}
