#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

using elements_type = ::std::array<::std::string_view, 3u>;
using range_type = decltype(::fast_io::mnp::rgvw(
	::std::declval<elements_type &>(), "::"));
using named_normalized_type = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<range_type &>())));
using temporary_normalized_type = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<range_type &&>())));

struct custom_traits : ::std::char_traits<char>
{};

using custom_traits_string = ::std::basic_string<char, custom_traits>;

template <typename T>
struct custom_allocator
{
	using value_type = T;

	constexpr custom_allocator() noexcept = default;
	template <typename U>
	constexpr custom_allocator(custom_allocator<U> const &) noexcept
	{}

	[[nodiscard]] T *allocate(::std::size_t n)
	{
		return ::std::allocator<T>{}.allocate(n);
	}

	void deallocate(T *ptr, ::std::size_t n) noexcept
	{
		::std::allocator<T>{}.deallocate(ptr, n);
	}

	template <typename U>
	friend constexpr bool operator==(custom_allocator, custom_allocator<U>) noexcept
	{
		return true;
	}
};

using custom_allocator_string =
	::std::basic_string<char, ::std::char_traits<char>, custom_allocator<char>>;

// These hooks intentionally model an otherwise admissible third-party exact-overwrite destination in every language
// mode. They prove that the generic concept itself owns the feature-test boundary: guarding only std::basic_string's
// adapter would let unrelated ADL hooks activate the new concat strategy when the standardized API is unavailable.
struct custom_exact_overwrite_probe
{};

[[maybe_unused]] inline constexpr custom_exact_overwrite_probe strlike_construct_define(
	::fast_io::io_strlike_type_t<char, custom_exact_overwrite_probe>, char const *, char const *) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	::fast_io::io_strlike_type_t<char, custom_exact_overwrite_probe>) noexcept
{
	return {};
}

template <typename operation>
[[maybe_unused]] inline constexpr void strlike_exact_resize_and_overwrite(
	::fast_io::io_strlike_type_t<char, custom_exact_overwrite_probe>, custom_exact_overwrite_probe &,
	::std::size_t, operation &) noexcept
{}

struct dummy_overwrite_operation
{
	inline constexpr ::std::size_t operator()(char *, ::std::size_t) const noexcept
	{
		return 0u;
	}
};

// These destinations isolate the second-stage CPO check. A marker is insufficient when the actual named operation
// cannot bind, and a non-void adapter must not become concept-true merely because its side effects would be usable.
struct rvalue_only_exact_overwrite_probe
{};

[[maybe_unused]] inline constexpr rvalue_only_exact_overwrite_probe strlike_construct_define(
	::fast_io::io_strlike_type_t<char, rvalue_only_exact_overwrite_probe>,
	char const *, char const *) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	::fast_io::io_strlike_type_t<char, rvalue_only_exact_overwrite_probe>) noexcept
{
	return {};
}

template <typename operation>
	requires(!::std::is_lvalue_reference_v<operation>)
[[maybe_unused]] inline constexpr void strlike_exact_resize_and_overwrite(
	::fast_io::io_strlike_type_t<char, rvalue_only_exact_overwrite_probe>,
	rvalue_only_exact_overwrite_probe &, ::std::size_t, operation &&) noexcept
{}

struct nonvoid_exact_overwrite_probe
{};

[[maybe_unused]] inline constexpr nonvoid_exact_overwrite_probe strlike_construct_define(
	::fast_io::io_strlike_type_t<char, nonvoid_exact_overwrite_probe>,
	char const *, char const *) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	::fast_io::io_strlike_type_t<char, nonvoid_exact_overwrite_probe>) noexcept
{
	return {};
}

template <typename operation>
[[maybe_unused]] inline constexpr int strlike_exact_resize_and_overwrite(
	::fast_io::io_strlike_type_t<char, nonvoid_exact_overwrite_probe>,
	nonvoid_exact_overwrite_probe &, ::std::size_t, operation &) noexcept
{
	return 0;
}

struct throwing_precise_size_source
{};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, throwing_precise_size_source>,
	throwing_precise_size_source) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_precise_size_source>, char *destination,
	throwing_precise_size_source) noexcept
{
	*destination = 'x';
	return destination + 1u;
}

[[maybe_unused]] inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, throwing_precise_size_source>,
	throwing_precise_size_source)
{
	throw 91;
}

[[maybe_unused]] inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, throwing_precise_size_source>, char *destination,
	[[maybe_unused]] ::std::size_t precise_size, throwing_precise_size_source value) noexcept
{
	return print_reserve_define(
		::fast_io::io_reserve_type<char, throwing_precise_size_source>, destination, value);
}

[[maybe_unused]] inline constexpr ::std::true_type print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<char, throwing_precise_size_source>) noexcept
{
	return {};
}

#if __cpp_lib_string_resize_and_overwrite >= 202110L
struct callback_observing_string
{
	::std::string storage;
};

inline bool overwrite_callback_entered{};

[[maybe_unused]] inline callback_observing_string strlike_construct_define(
	::fast_io::io_strlike_type_t<char, callback_observing_string>,
	char const *first, char const *last)
{
	return {::std::string(first, last)};
}

[[maybe_unused]] inline constexpr ::std::true_type strlike_exact_resize_and_overwrite_available(
	::fast_io::io_strlike_type_t<char, callback_observing_string>) noexcept
{
	return {};
}

template <typename operation>
[[maybe_unused]] inline void strlike_exact_resize_and_overwrite(
	::fast_io::io_strlike_type_t<char, callback_observing_string>, callback_observing_string &str,
	::std::size_t size, operation &op)
{
	overwrite_callback_entered = true;
	str.storage.resize_and_overwrite(size, op);
}
#endif

struct noexcept_void_precise_leaf
{};

[[maybe_unused]] inline constexpr ::std::size_t
	print_reserve_size(::fast_io::io_reserve_type_t<char, noexcept_void_precise_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, noexcept_void_precise_leaf>, char *destination,
	noexcept_void_precise_leaf) noexcept
{
	*destination = 'v';
	return destination + 1u;
}

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, noexcept_void_precise_leaf>, noexcept_void_precise_leaf) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr void print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, noexcept_void_precise_leaf>, char *destination,
	[[maybe_unused]] ::std::size_t precise_size, noexcept_void_precise_leaf) noexcept
{
	*destination = 'v';
}

struct throwing_copy_iterator
{
	using value_type = ::std::string_view;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::forward_iterator_tag;
	using iterator_category = ::std::forward_iterator_tag;

	value_type *current{};

	constexpr throwing_copy_iterator() noexcept = default;
	constexpr throwing_copy_iterator(throwing_copy_iterator const &other) noexcept(false)
		: current(other.current)
	{}
	constexpr throwing_copy_iterator &operator=(throwing_copy_iterator const &) noexcept = default;

	inline constexpr value_type &operator*() const noexcept
	{
		return *current;
	}

	inline constexpr throwing_copy_iterator &operator++() noexcept
	{
		++current;
		return *this;
	}

	inline constexpr throwing_copy_iterator operator++(int) noexcept(false)
	{
		auto copy{*this};
		++*this;
		return copy;
	}

	[[maybe_unused]] friend inline constexpr bool operator==(
		throwing_copy_iterator left, throwing_copy_iterator right) noexcept
	{
		return left.current == right.current;
	}
};

static_assert(::std::forward_iterator<throwing_copy_iterator>);
using throwing_copy_range = ::fast_io::sized_range_view_t<char, throwing_copy_iterator>;
using throwing_copy_parameter = ::fast_io::parameter<throwing_copy_range &>;
static_assert(!::fast_io::sized_range_view_nothrow_reserve_define_v<
			  char, throwing_copy_iterator>);
static_assert(::fast_io::precise_reserve_printable<char, noexcept_void_precise_leaf>);
static_assert(!::fast_io::nothrow_precise_reserve_printable<char, noexcept_void_precise_leaf>);
static_assert(!::fast_io::scatter_printable_for<char, named_normalized_type &>);

#if __cpp_lib_string_resize_and_overwrite >= 202110L
static_assert(::fast_io::exact_resize_and_overwrite_strlike<char, ::std::string>);
static_assert(::fast_io::exact_resize_and_overwrite_strlike<char, custom_exact_overwrite_probe>);
static_assert(::fast_io::exact_resize_and_overwrite_strlike<
			  char, rvalue_only_exact_overwrite_probe>);
static_assert(!::fast_io::exact_resize_and_overwrite_strlike_for<
			  char, rvalue_only_exact_overwrite_probe, dummy_overwrite_operation>);
static_assert(::fast_io::exact_resize_and_overwrite_strlike<
			  char, nonvoid_exact_overwrite_probe>);
static_assert(!::fast_io::exact_resize_and_overwrite_strlike_for<
			  char, nonvoid_exact_overwrite_probe, dummy_overwrite_operation>);
static_assert(!::fast_io::exact_resize_and_overwrite_strlike<char, custom_traits_string>);
static_assert(!::fast_io::exact_resize_and_overwrite_strlike<char, custom_allocator_string>);
static_assert(::fast_io::sized_range_view_nothrow_reserve_define_v<
			  char, typename range_type::iterator>);
static_assert(!::fast_io::nothrow_precise_reserve_printable<char, throwing_copy_parameter>);
static_assert(::fast_io::nothrow_precise_reserve_printable<char, range_type>);
static_assert(::fast_io::nothrow_precise_reserve_printable<char, named_normalized_type>);
static_assert(::fast_io::nothrow_precise_reserve_printable<char, temporary_normalized_type>);
static_assert(!::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
			  false, char, ::std::string, named_normalized_type>);
static_assert(::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
			  true, char, ::std::string, named_normalized_type>);
static_assert(::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
			  true, char, ::std::string, temporary_normalized_type>);
using throwing_normalized_type = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<throwing_precise_size_source &>())));
static_assert(::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
			  true, char, callback_observing_string, throwing_normalized_type>);
#else
// The standard destination CPO and every generic strategy admission point are unavailable when the feature-test macro
// is absent. This makes the library boundary executable documentation even though custom same-named hooks exist above.
static_assert(!::fast_io::exact_resize_and_overwrite_strlike<char, ::std::string>);
static_assert(!::fast_io::exact_resize_and_overwrite_strlike<char, custom_exact_overwrite_probe>);
static_assert(!::fast_io::nothrow_precise_reserve_printable<char, named_normalized_type>);
static_assert(!::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
			  false, char, custom_exact_overwrite_probe, named_normalized_type>);
#endif

template <::std::integral char_type>
void test_character_domain()
{
	using view_type = ::std::basic_string_view<char_type>;
	using string_type = ::std::basic_string<char_type>;
	char_type const alpha[]{static_cast<char_type>('a'), static_cast<char_type>('l'),
							static_cast<char_type>('p'), static_cast<char_type>('h'), static_cast<char_type>('a'), char_type{}};
	char_type const omega[]{static_cast<char_type>('o'), static_cast<char_type>('m'),
							static_cast<char_type>('e'), static_cast<char_type>('g'), static_cast<char_type>('a'), char_type{}};
	char_type const separator[]{static_cast<char_type>(':'), static_cast<char_type>(':'), char_type{}};
	::std::array<view_type, 3u> elements{view_type{alpha}, view_type{}, view_type{omega}};
	auto range{::fast_io::mnp::rgvw(elements, separator)};

	string_type expected{alpha};
	expected.append(separator, 2u);
	expected.append(separator, 2u);
	expected.append(omega);
	auto const named_result{
		::fast_io::basic_general_concat_checked<false, char_type, string_type>(range)};
	assert(named_result == expected);

	auto expected_line{expected};
	expected_line.push_back(static_cast<char_type>('\n'));
	auto const named_line_result{
		::fast_io::basic_general_concat_checked<true, char_type, string_type>(range)};
	assert(named_line_result == expected_line);
	auto const temporary_line_result{
		::fast_io::basic_general_concat_checked<true, char_type, string_type>(
			::fast_io::mnp::rgvw(elements, separator))};
	assert(temporary_line_result == expected_line);

	::std::array<view_type, 0u> empty{};
	auto empty_range{::fast_io::mnp::rgvw(empty, separator)};
	assert((::fast_io::basic_general_concat_checked<false, char_type, string_type>(empty_range).empty()));
	auto const empty_line{
		::fast_io::basic_general_concat_checked<true, char_type, string_type>(empty_range)};
	assert(empty_line.size() == 1u && empty_line.front() == static_cast<char_type>('\n'));
}

#if __cpp_lib_string_resize_and_overwrite >= 202110L
void test_sizing_exception_precedes_callback()
{
	overwrite_callback_entered = false;
	throwing_precise_size_source source;
	bool caught{};
	try
	{
		(void)::fast_io::basic_general_concat_checked<
			true, char, callback_observing_string>(source);
	}
	catch (int value)
	{
		caught = value == 91;
	}
	assert(caught);
	assert(!overwrite_callback_entered);
}
#endif

} // namespace

int main()
{
	test_character_domain<char>();
	test_character_domain<wchar_t>();
	test_character_domain<char8_t>();
	test_character_domain<char16_t>();
	test_character_domain<char32_t>();
#if __cpp_lib_string_resize_and_overwrite >= 202110L
	test_sizing_exception_precedes_callback();
#endif
}
