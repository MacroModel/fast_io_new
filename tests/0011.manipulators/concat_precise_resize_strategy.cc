#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <fast_io.h>

namespace
{

struct strategy_trace
{
	::std::size_t resize_calls{};
	::std::size_t precise_size_calls{};
	::std::size_t precise_define_calls{};
};

inline strategy_trace *active_trace{};

struct precise_resize_text
{
	::std::string value;
	::std::size_t resize_calls{};
};

inline precise_resize_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, precise_resize_text>, char const *first, char const *last)
{
	return {::std::string(first, last), 0u};
}

inline char *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, precise_resize_text>, precise_resize_text &text,
	::std::size_t size)
{
	++text.resize_calls;
	if (active_trace != nullptr)
	{
		++active_trace->resize_calls;
	}
	text.value.resize(size);
	return text.value.data();
}

struct construct_only_text
{
	::std::string value;
};

inline construct_only_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, construct_only_text>, char const *first, char const *last)
{
	return {::std::string(first, last)};
}

struct wrong_const_pointer_text
{
	::std::string value;
};

inline wrong_const_pointer_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, wrong_const_pointer_text>, char const *first, char const *last)
{
	return {::std::string(first, last)};
}

inline char const *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, wrong_const_pointer_text>, wrong_const_pointer_text &text,
	::std::size_t size)
{
	text.value.resize(size);
	return text.value.data();
}

struct precise_pointer_leaf
{
	::std::string_view text;
};

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_pointer_leaf>, precise_pointer_leaf leaf) noexcept
{
	// This deliberately loose ordinary bound proves that concat selected the independent precise protocol rather than
	// treating a dynamic reserve capacity as an exact string size.
	return leaf.text.size() + 8u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_pointer_leaf>, char *destination,
	precise_pointer_leaf leaf) noexcept
{
	for (char ch : leaf.text)
	{
		*destination++ = ch;
	}
	return destination;
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_pointer_leaf>, precise_pointer_leaf leaf) noexcept
{
	if (active_trace != nullptr)
	{
		++active_trace->precise_size_calls;
	}
	return leaf.text.size();
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_pointer_leaf>, char *destination,
	[[maybe_unused]] ::std::size_t precise_size, precise_pointer_leaf leaf) noexcept
{
	if (active_trace != nullptr)
	{
		++active_trace->precise_define_calls;
	}
	return print_reserve_define(
		::fast_io::io_reserve_type<char, precise_pointer_leaf>, destination, leaf);
}

struct precise_void_leaf
{
	::std::string_view text;
};

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, precise_void_leaf>, precise_void_leaf leaf) noexcept
{
	return leaf.text.size();
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, precise_void_leaf>, char *destination,
	precise_void_leaf leaf) noexcept
{
	for (char ch : leaf.text)
	{
		*destination++ = ch;
	}
	return destination;
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, precise_void_leaf>, precise_void_leaf leaf) noexcept
{
	return leaf.text.size();
}

inline void print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, precise_void_leaf>, char *destination,
	[[maybe_unused]] ::std::size_t precise_size, precise_void_leaf leaf) noexcept
{
	(void)print_reserve_define(::fast_io::io_reserve_type<char, precise_void_leaf>, destination, leaf);
}

struct fixed_leaf
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, fixed_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_leaf>, char *destination, fixed_leaf) noexcept
{
	*destination = 'F';
	return destination + 1u;
}

struct dual_capability_text
{
	::std::array<char, 64u> storage{};
	::std::size_t size{};
	::std::size_t precise_resize_calls{};
};

inline dual_capability_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, char const *first, char const *last)
{
	dual_capability_text result;
	for (; first != last; ++first)
	{
		result.storage[result.size++] = *first;
	}
	return result;
}

inline constexpr char *strlike_begin(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text) noexcept
{
	return text.storage.data();
}

inline constexpr char *strlike_curr(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text) noexcept
{
	return text.storage.data() + text.size;
}

inline constexpr char *strlike_end(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text) noexcept
{
	return text.storage.data() + text.storage.size();
}

inline constexpr void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text,
	char *current) noexcept
{
	text.size = static_cast<::std::size_t>(current - text.storage.data());
}

inline void strlike_reserve(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text,
	::std::size_t size)
{
	if (text.storage.size() < size)
	{
		::std::abort();
	}
}

inline char *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, dual_capability_text>, dual_capability_text &text,
	::std::size_t size)
{
	++text.precise_resize_calls;
	text.size = size;
	return text.storage.data();
}

inline constexpr ::fast_io::io_strlike_reference_wrapper<char, dual_capability_text>
io_strlike_ref(::fast_io::io_alias_t, dual_capability_text &text) noexcept
{
	return {__builtin_addressof(text)};
}

enum class throw_stage
{
	size,
	define
};

struct throwing_precise_leaf
{
	throw_stage stage;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, throwing_precise_leaf>, throwing_precise_leaf) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_precise_leaf>, char *destination,
	throwing_precise_leaf) noexcept
{
	*destination = 'X';
	return destination + 1u;
}

struct wrong_endpoint_leaf
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_endpoint_leaf>, wrong_endpoint_leaf) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_endpoint_leaf>, char *destination,
	wrong_endpoint_leaf) noexcept
{
	*destination = 'E';
	return destination + 1u;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, wrong_endpoint_leaf>, wrong_endpoint_leaf) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, wrong_endpoint_leaf>, char *destination,
	::std::size_t, wrong_endpoint_leaf) noexcept
{
	*destination = 'E';
	// Returning the unchanged (and therefore still in-range) cursor avoids undefined pointer arithmetic in the test
	// while violating the exact-end postcondition that concat must enforce before advancing to the next leaf.
	return destination;
}

struct overflowing_precise_leaf
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, overflowing_precise_leaf>, overflowing_precise_leaf) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, overflowing_precise_leaf>, char *destination,
	overflowing_precise_leaf) noexcept
{
	*destination = 'O';
	return destination + 1u;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, overflowing_precise_leaf>, overflowing_precise_leaf) noexcept
{
	return SIZE_MAX;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, overflowing_precise_leaf>, char *destination,
	::std::size_t, overflowing_precise_leaf) noexcept
{
	return destination;
}

template <::std::size_t>
using repeated_precise_leaf = precise_pointer_leaf;

struct incomplete_precise_leaf;

template <::std::size_t>
using repeated_incomplete_precise_leaf = incomplete_precise_leaf;

template <::std::size_t... indexes>
consteval bool precise_resize_run_selected(::std::index_sequence<indexes...>)
{
	return ::fast_io::details::decay::basic_general_concat_precise_resize_run_v<
		char, repeated_precise_leaf<indexes>...>;
}

template <::std::size_t... indexes>
consteval bool oversized_incomplete_run_rejected(::std::index_sequence<indexes...>)
{
	return ::fast_io::details::decay::basic_general_concat_precise_resize_run_v<
		char, repeated_incomplete_precise_leaf<indexes>...>;
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, throwing_precise_leaf>, throwing_precise_leaf leaf)
{
	++active_trace->precise_size_calls;
	if (leaf.stage == throw_stage::size)
	{
		throw ::std::runtime_error("size");
	}
	return 1u;
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, throwing_precise_leaf>, char *destination,
	::std::size_t, throwing_precise_leaf leaf)
{
	++active_trace->precise_define_calls;
	*destination = 'X';
	if (leaf.stage == throw_stage::define)
	{
		throw ::std::runtime_error("define");
	}
	return destination + 1u;
}

static_assert(::fast_io::precise_resize_writable_strlike<char, ::std::string>);
static_assert(::fast_io::precise_resize_writable_strlike<wchar_t, ::std::wstring>);
static_assert(::fast_io::precise_resize_writable_strlike<char8_t, ::std::u8string>);
static_assert(::fast_io::precise_resize_writable_strlike<char16_t, ::std::u16string>);
static_assert(::fast_io::precise_resize_writable_strlike<char32_t, ::std::u32string>);
static_assert(::fast_io::precise_resize_writable_strlike<char, precise_resize_text>);
static_assert(!::fast_io::buffer_strlike<char, precise_resize_text>);
static_assert(!::fast_io::precise_resize_writable_strlike<char, construct_only_text>);
static_assert(!::fast_io::precise_resize_writable_strlike<char, wrong_const_pointer_text>);
static_assert(::fast_io::buffer_strlike<char, dual_capability_text>);
static_assert(::fast_io::precise_resize_writable_strlike<char, dual_capability_text>);
static_assert(::fast_io::precise_reserve_printable<char, precise_pointer_leaf>);
static_assert(::fast_io::precise_reserve_printable<char, precise_void_leaf>);
static_assert(precise_resize_run_selected(::std::make_index_sequence<16u>{}));
static_assert(!precise_resize_run_selected(::std::make_index_sequence<17u>{}));
static_assert(!oversized_incomplete_run_rejected(::std::make_index_sequence<17u>{}));

inline ::std::string_view view(dual_capability_text const &text) noexcept
{
	return {text.storage.data(), text.size};
}

void test_precise_resize_dispatch()
{
	strategy_trace trace;
	active_trace = __builtin_addressof(trace);
	auto output{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{"dynamic-precise"})};
	active_trace = nullptr;
	assert(output.value == "dynamic-precise");
	assert(output.resize_calls == 1u);
	assert(trace.resize_calls == 1u);
	assert(trace.precise_size_calls == 1u);
	assert(trace.precise_define_calls == 1u);

	auto line{::fast_io::basic_general_concat_checked<true, char, precise_resize_text>(
		precise_void_leaf{"void"})};
	assert(line.value == "void\n");
	assert(line.resize_calls == 1u);

	auto empty{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{""})};
	assert(empty.value.empty());
	assert(empty.resize_calls == 1u);
}

template <bool line = false, ::std::size_t... indexes>
precise_resize_text concat_repeated_precise_leaves(::std::index_sequence<indexes...>)
{
	return ::fast_io::basic_general_concat_checked<line, char, precise_resize_text>(
		(static_cast<void>(indexes), precise_pointer_leaf{"x"})...);
}

void test_multi_precise_resize_dispatch()
{
	strategy_trace two_trace;
	active_trace = __builtin_addressof(two_trace);
	auto two{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{"ab"}, precise_pointer_leaf{"cd"})};
	active_trace = nullptr;
	assert(two.value == "abcd");
	assert(two.resize_calls == 1u);
	assert(two_trace.resize_calls == 1u);
	assert(two_trace.precise_size_calls == 2u);
	assert(two_trace.precise_define_calls == 2u);

	auto four{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{"a"}, precise_void_leaf{"b"},
		precise_pointer_leaf{"c"}, precise_void_leaf{"d"})};
	assert(four.value == "abcd");
	assert(four.resize_calls == 1u);

	strategy_trace sixteen_trace;
	active_trace = __builtin_addressof(sixteen_trace);
	auto sixteen{concat_repeated_precise_leaves(::std::make_index_sequence<16u>{})};
	active_trace = nullptr;
	assert(sixteen.value == ::std::string(16u, 'x'));
	assert(sixteen.resize_calls == 1u);
	assert(sixteen_trace.resize_calls == 1u);
	assert(sixteen_trace.precise_size_calls == 16u);
	assert(sixteen_trace.precise_define_calls == 16u);

	// Newline ownership is a constant-size suffix, not another precise leaf. The measured sixteen-leaf line boundary
	// must therefore retain the same one-resize strategy and invoke every size/writer CPO exactly once.
	strategy_trace sixteen_line_trace;
	active_trace = __builtin_addressof(sixteen_line_trace);
	auto sixteen_line{concat_repeated_precise_leaves<true>(::std::make_index_sequence<16u>{})};
	active_trace = nullptr;
	assert(sixteen_line.value == ::std::string(16u, 'x') + '\n');
	assert(sixteen_line.resize_calls == 1u);
	assert(sixteen_line_trace.resize_calls == 1u);
	assert(sixteen_line_trace.precise_size_calls == 16u);
	assert(sixteen_line_trace.precise_define_calls == 16u);

	strategy_trace seventeen_trace;
	active_trace = __builtin_addressof(seventeen_trace);
	auto seventeen{concat_repeated_precise_leaves(::std::make_index_sequence<17u>{})};
	active_trace = nullptr;
	assert(seventeen.value == ::std::string(17u, 'x'));
	assert(seventeen.resize_calls == 0u);
	assert(seventeen_trace.resize_calls == 0u);
	assert(seventeen_trace.precise_size_calls == 0u);
	assert(seventeen_trace.precise_define_calls == 0u);

	// Adjacent zero-length slices are valid and must neither advance the cursor nor suppress the one logical resize.
	auto zero{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{""}, precise_void_leaf{""})};
	assert(zero.value.empty());
	assert(zero.resize_calls == 1u);

	// Exercise both the usual SSO-sized result and a payload beyond concat's former 2-KiB inline staging buffer.
	auto small{::fast_io::concat_std(
		precise_pointer_leaf{"sso"}, precise_pointer_leaf{"-run"})};
	assert(small == "sso-run");
	::std::string large_payload(4097u, 'L');
	auto large{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		precise_pointer_leaf{large_payload}, precise_pointer_leaf{"tail"})};
	assert(large.value.size() == large_payload.size() + 4u);
	assert(large.value.starts_with(large_payload));
	assert(large.value.ends_with("tail"));
	assert(large.resize_calls == 1u);

	auto line{::fast_io::basic_general_concat_checked<true, char, precise_resize_text>(
		precise_pointer_leaf{"a"}, precise_void_leaf{"b"},
		precise_pointer_leaf{"c"}, precise_void_leaf{"d"})};
	assert(line.value == "abcd\n");
	assert(line.resize_calls == 1u);
}

void test_earlier_strategies_keep_priority()
{
	auto scatter{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		::std::string_view{"scatter"})};
	assert(scatter.value == "scatter");
	assert(scatter.resize_calls == 0u);

	auto fixed{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(fixed_leaf{})};
	assert(fixed.value == "F");
	assert(fixed.resize_calls == 0u);

	// A destination exposing both capabilities must retain its genuine put-area path. The exact-resize CPO is a
	// portable fallback, not a reason to replace a stronger cursor protocol.
	auto dual{::fast_io::basic_general_concat_checked<false, char, dual_capability_text>(
		precise_pointer_leaf{"buffer-first"})};
	assert(view(dual) == "buffer-first");
	assert(dual.precise_resize_calls == 0u);
}

void test_semantic_precise_resize_dispatch()
{
	strategy_trace trace;
	active_trace = __builtin_addressof(trace);
	auto composition{::fast_io::mnp::pack(
		precise_pointer_leaf{"ab"},
		::fast_io::mnp::left(precise_pointer_leaf{"c"}, 3u, '.'),
		::fast_io::mnp::cond(
			true, precise_pointer_leaf{"D"}, precise_pointer_leaf{"inactive"}))};
	auto output{::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
		composition)};
	active_trace = nullptr;
	assert(output.value == "abc..D");
	assert(output.resize_calls == 1u);
	assert(trace.resize_calls == 1u);
	// Exact semantic emission measures each dynamic-precise leaf once for the whole allocation and once at its existing
	// leaf define boundary. The strategy removes staging/final-copy work; it does not silently change a leaf protocol.
	assert(trace.precise_size_calls == 6u);
	assert(trace.precise_define_calls == 3u);

	auto line{::fast_io::basic_general_concat_checked<true, char, precise_resize_text>(
		::fast_io::mnp::pack(
			::fast_io::mnp::left(precise_pointer_leaf{"line"}, 4u, '.'),
			precise_void_leaf{"-tail"}))};
	assert(line.value == "line-tail\n");
	assert(line.resize_calls == 1u);

	// This payload crosses the old inline staging buffer boundary. The public std::string adapter must now resize its
	// final logical range directly rather than placing a 2-KiB concat buffer plus a second construction in the caller.
	::std::string large_payload(4097u, 'x');
	auto large{::fast_io::concat_std(::fast_io::mnp::pack(
		precise_pointer_leaf{large_payload}, precise_pointer_leaf{"!"}))};
	assert(large.size() == large_payload.size() + 1u);
	assert(large.back() == '!');
}

void test_exception_boundaries()
{
	strategy_trace size_trace;
	active_trace = __builtin_addressof(size_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			throwing_precise_leaf{throw_stage::size});
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(size_trace.precise_size_calls == 1u);
	assert(size_trace.resize_calls == 0u);
	assert(size_trace.precise_define_calls == 0u);

	strategy_trace define_trace;
	active_trace = __builtin_addressof(define_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			throwing_precise_leaf{throw_stage::define});
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(define_trace.precise_size_calls == 1u);
	assert(define_trace.resize_calls == 1u);
	assert(define_trace.precise_define_calls == 1u);

	strategy_trace semantic_size_trace;
	active_trace = __builtin_addressof(semantic_size_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			::fast_io::mnp::pack(
				::fast_io::mnp::left(throwing_precise_leaf{throw_stage::size}, 1u, '.'),
				precise_pointer_leaf{"tail"}));
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(semantic_size_trace.resize_calls == 0u);

	strategy_trace semantic_define_trace;
	active_trace = __builtin_addressof(semantic_define_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			::fast_io::mnp::pack(
				::fast_io::mnp::left(throwing_precise_leaf{throw_stage::define}, 1u, '.'),
				precise_pointer_leaf{"tail"}));
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(semantic_define_trace.resize_calls == 1u);
	assert(semantic_define_trace.precise_define_calls == 1u);

	strategy_trace multi_size_trace;
	active_trace = __builtin_addressof(multi_size_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			precise_pointer_leaf{"first"}, throwing_precise_leaf{throw_stage::size},
			precise_pointer_leaf{"unmeasured"});
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(multi_size_trace.precise_size_calls == 2u);
	assert(multi_size_trace.resize_calls == 0u);
	assert(multi_size_trace.precise_define_calls == 0u);

	strategy_trace multi_define_trace;
	active_trace = __builtin_addressof(multi_define_trace);
	try
	{
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			precise_pointer_leaf{"first"}, throwing_precise_leaf{throw_stage::define},
			precise_pointer_leaf{"unwritten"});
		assert(false);
	}
	catch (::std::runtime_error const &)
	{}
	active_trace = nullptr;
	assert(multi_define_trace.precise_size_calls == 3u);
	assert(multi_define_trace.resize_calls == 1u);
	assert(multi_define_trace.precise_define_calls == 2u);
}

#if defined(__unix__) || defined(__APPLE__)
template <typename function>
void expect_child_failure(function &&fn)
{
	auto const child{::fork()};
	if (child < 0)
	{
		::std::abort();
	}
	if (child == 0)
	{
		fn();
		::_exit(0);
	}
	int status{};
	if (::waitpid(child, __builtin_addressof(status), 0) != child ||
		(WIFEXITED(status) && WEXITSTATUS(status) == 0))
	{
		::std::abort();
	}
}

void test_protocol_violation_boundaries()
{
	// These are death tests because checked arithmetic and a dishonest endpoint deliberately call fast_terminate.
	expect_child_failure([] {
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			overflowing_precise_leaf{}, precise_pointer_leaf{"overflow"});
	});
	expect_child_failure([] {
		(void)::fast_io::basic_general_concat_checked<false, char, precise_resize_text>(
			precise_pointer_leaf{"prefix"}, wrong_endpoint_leaf{},
			precise_pointer_leaf{"must-not-run"});
	});
}
#endif

} // namespace

int main()
{
	test_precise_resize_dispatch();
	test_multi_precise_resize_dispatch();
	test_earlier_strategies_keep_priority();
	test_semantic_precise_resize_dispatch();
	test_exception_boundaries();
#if defined(__unix__) || defined(__APPLE__)
	test_protocol_violation_boundaries();
#endif
}
