#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>

#include <fast_io.h>
#include <fast_io_unit/string.h>

namespace gcc_std_dynamic_chunk_test
{

struct dynamic_source
{
	char value{};
};

/*
The source deliberately exposes only the run-time-shaped dynamic protocol. It
initializes scratch inside its conservative bound but returns the one-character
logical cursor. No static-reserve, bounded, precise, scatter, context, or direct
CPO exists, so the type isolates the plain dynamic-reserve representation
admitted by the GCC std::string placement policy.
*/
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_source>,
	dynamic_source) noexcept
{
	return 4u;
}

[[nodiscard]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_source>, char *destination,
	dynamic_source source) noexcept
{
	*destination = source.value;
	destination[1] = '\0';
	destination[2] = '\0';
	destination[3] = '\0';
	return destination + 1;
}

template <bool line, ::std::size_t... indices>
[[nodiscard]] inline constexpr ::std::string make_result(
	::std::index_sequence<indices...>)
{
	if constexpr (line)
	{
		return ::fast_io::concatln_std(
			dynamic_source{static_cast<char>('a' + indices % 26u)}...);
	}
	else
	{
		return ::fast_io::concat_std(
			dynamic_source{static_cast<char>('a' + indices % 26u)}...);
	}
}

template <bool line, ::std::size_t count>
[[nodiscard]] inline constexpr bool verify_result(
	::std::string const &result) noexcept
{
	if (result.size() != count + static_cast<::std::size_t>(line))
	{
		return false;
	}
	for (::std::size_t index{}; index != count; ++index)
	{
		if (result[index] != static_cast<char>('a' + index % 26u))
		{
			return false;
		}
	}
	return !line || result.back() == '\n';
}

template <bool line, ::std::size_t count>
[[nodiscard]] inline constexpr bool verify_concat() noexcept
{
	auto const result{make_result<line>(::std::make_index_sequence<count>{})};
	return verify_result<line, count>(result);
}

template <bool line, ::std::size_t... indices>
inline void print_result(
	::std::string &result, ::std::index_sequence<indices...>)
{
	auto output{::fast_io::io_strlike_ref(::fast_io::io_alias, result)};
	if constexpr (line)
	{
		::fast_io::println(
			output,
			dynamic_source{static_cast<char>('a' + indices % 26u)}...);
	}
	else
	{
		::fast_io::print(
			output,
			dynamic_source{static_cast<char>('a' + indices % 26u)}...);
	}
}

template <bool line, ::std::size_t count>
[[nodiscard]] inline bool verify_print()
{
	::std::string result;
	print_result<line>(result, ::std::make_index_sequence<count>{});
	return verify_result<line, count>(result);
}

using std_output =
	::fast_io::io_strlike_reference_wrapper<char, ::std::string>;

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13 && defined(__linux__) && \
	defined(__x86_64__) && defined(__GLIBCXX__) && defined(_GLIBCXX_USE_CXX11_ABI) &&   \
	_GLIBCXX_USE_CXX11_ABI
static_assert(
	::fast_io::details::decay::print_control_single_out_of_line_traits<
		std_output>::value);
static_assert(
	::fast_io::details::decay::print_control_single_out_of_line_eligible<
		std_output, dynamic_source>);
#else
static_assert(
	!::fast_io::details::decay::print_control_single_out_of_line_traits<
		std_output>::value);
#endif

[[nodiscard]] inline bool verify_terminal_chunks()
{
	if constexpr (
		::fast_io::details::decay::print_control_single_out_of_line_traits<
			std_output>::value)
	{
		/*
		Only a destination which activates the placement policy instantiates its
		five-, six-, and seven-control terminal chunks. Non-GCC controls retain
		their ordinary code path without paying for irrelevant stress functions.
		*/
		return verify_concat<false, 53u>() && verify_print<true, 54u>() &&
			   verify_concat<true, 55u>();
	}
	else
	{
		return true;
	}
}

/*
Constant evaluation is admitted only when both the language mode and the
standard-library feature-test macro promise constexpr string support. The
compiler placement attribute has no run-time call boundary during constant
evaluation and therefore must preserve the same result.
*/
#if __cplusplus >= 202002L && defined(__cpp_lib_constexpr_string) && \
	__cpp_lib_constexpr_string >= 201907L
static_assert(verify_concat<false, 32u>());
static_assert(verify_concat<true, 32u>());
#endif

} // namespace gcc_std_dynamic_chunk_test

int main()
{
	/*
	The 32-control case exercises the placement threshold; 53, 54, and 55
	controls cover every non-multiple terminal chunk; and 64 controls prove that
	recursion cannot drop, duplicate, or misplace a leaf or the final newline
	after the first complete group.
	*/
	if (!gcc_std_dynamic_chunk_test::verify_concat<false, 32u>() ||
		!gcc_std_dynamic_chunk_test::verify_concat<true, 32u>() ||
		!gcc_std_dynamic_chunk_test::verify_print<false, 32u>() ||
		!gcc_std_dynamic_chunk_test::verify_print<true, 32u>() ||
		!gcc_std_dynamic_chunk_test::verify_terminal_chunks() ||
		!gcc_std_dynamic_chunk_test::verify_concat<false, 64u>() ||
		!gcc_std_dynamic_chunk_test::verify_concat<true, 64u>() ||
		!gcc_std_dynamic_chunk_test::verify_print<false, 64u>() ||
		!gcc_std_dynamic_chunk_test::verify_print<true, 64u>())
	{
		::std::abort();
	}
}
