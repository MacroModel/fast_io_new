#include <cstdlib>
#include <type_traits>
#include <utility>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io.h>

namespace scan_named_source_category
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

/*
 * Public scan borrows its named input parameter throughout one synchronous
 * operation. The incoming expression category must not change which CPO is
 * detected: both an existing source and a temporary source are named lvalues
 * before normalization. Deleted special members also prove that this boundary
 * never constructs a surrogate owner merely to obtain that lvalue expression.
 */
template <bool shared_projection>
struct named_source
{
	::fast_io::basic_ibuffer_view<char> view;
	unsigned *normalizations;

	named_source(char const *first, char const *last, unsigned *count) noexcept
		: view(first, last), normalizations(count)
	{}

	named_source(named_source const &) = delete;
	named_source &operator=(named_source const &) = delete;
	named_source(named_source &&) = delete;
	named_source &operator=(named_source &&) = delete;
};

inline ::fast_io::basic_ibuffer_view_ref<char> input_stream_ref_define(
	named_source<false> &source) noexcept
{
	++*source.normalizations;
	return {__builtin_addressof(source.view)};
}

inline ::fast_io::basic_ibuffer_view_ref<char> io_stream_ref_define(
	named_source<true> &source) noexcept
{
	++*source.normalizations;
	return {__builtin_addressof(source.view)};
}

struct rvalue_only_source
{
	::fast_io::basic_ibuffer_view<char> view;
};

inline ::fast_io::basic_ibuffer_view_ref<char> input_stream_ref_define(
	rvalue_only_source &&source) noexcept
{
	return {__builtin_addressof(source.view)};
}

struct const_source
{
	::fast_io::basic_ibuffer_view<char> *view;
	unsigned *normalizations;
};

inline ::fast_io::basic_ibuffer_view_ref<char> input_stream_ref_define(
	const_source const &source) noexcept
{
	// Constness applies to the handle, not the separate mutable input state.
	// The facade must preserve this cv-qualified normalization expression.
	++*source.normalizations;
	return {source.view};
}

struct dual_role_target
{
	::fast_io::basic_ibuffer_view<char> view;
	unsigned *value;
};

inline ::fast_io::basic_ibuffer_view_ref<char> input_stream_ref_define(
	dual_role_target &&source) noexcept
{
	return {__builtin_addressof(source.view)};
}

inline auto scan_alias_define(::fast_io::io_alias_t, dual_role_target &target) noexcept
{
	return ::fast_io::io_scan_alias(*target.value);
}

// The general CPO correctly distinguishes categories. The scan facade must
// query the named-source category, not weaken or reinterpret this CPO contract.
static_assert(::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<named_source<false> &>);
static_assert(!::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<named_source<false>>);
static_assert(::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<named_source<true> &>);
static_assert(!::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<named_source<true>>);
static_assert(::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<rvalue_only_source>);
static_assert(!::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<rvalue_only_source &>);
static_assert(::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<const_source const &>);
static_assert(::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<dual_role_target>);
static_assert(!::fast_io::operations::defines::
				  has_input_or_io_stream_ref_define<dual_role_target &>);

/*
 * These retained, non-template wrappers instantiate the real stdin fallback
 * bodies without executing them. main deliberately never calls them: default
 * input must not make a correctness test wait for user input or consume data.
 */
[[gnu::used]] bool compile_only_stdin_report(unsigned &value)
{
	return ::fast_io::io::scan<true>(value);
}

[[gnu::used]] void compile_only_stdin_throw(unsigned &value)
{
	::fast_io::io::scan<false>(value);
}

[[gnu::used]] bool compile_only_dual_role_report(dual_role_target &target)
{
	// The xvalue has an input CPO, but the named facade parameter does not.
	// Its valid scan alias must therefore remain a default-input scan target.
	return ::fast_io::io::scan<true>(::std::move(target));
}

[[gnu::used]] void compile_only_dual_role_throw(dual_role_target &target)
{
	::fast_io::io::scan<false>(::std::move(target));
}

inline void verify_const_source()
{
	constexpr char input[]{'2', '3'};
	::fast_io::basic_ibuffer_view<char> view{input, input + 2u};
	unsigned normalizations{};
	unsigned value{};
	const_source const source{
		__builtin_addressof(view), __builtin_addressof(normalizations)};
	require(::fast_io::io::scan<true>(source, value));
	require(value == 23u && normalizations == 1u);
	require(view.curr_ptr == view.end_ptr);
	view.curr_ptr = view.begin_ptr;
	value = 0u;
	::fast_io::io::scan<false>(::std::move(source), value);
	require(value == 23u && normalizations == 2u);
	require(view.curr_ptr == view.end_ptr);
}

template <bool shared_projection>
inline void verify_named_source_category()
{
	constexpr char input[]{'1', '7'};
	unsigned normalizations{};
	unsigned value{};
	named_source<shared_projection> source{
		input, input + 2u, __builtin_addressof(normalizations)};
	require(::fast_io::io::scan<true>(source, value));
	require(value == 17u && normalizations == 1u);
	require(source.view.curr_ptr == source.view.end_ptr);

	// An xvalue argument is still the same named object inside scan. Its cursor
	// mutation must remain visible in the original source after the call.
	source.view.curr_ptr = source.view.begin_ptr;
	value = 0u;
	require(::fast_io::io::scan<true>(::std::move(source), value));
	require(value == 17u && normalizations == 2u);
	require(source.view.curr_ptr == source.view.end_ptr);

	// Full-expression lifetime keeps this immovable temporary source alive
	// through normalization and scanning; no rvalue-specific CPO is required.
	value = 0u;
	require(::fast_io::io::scan<true>(
		named_source<shared_projection>{
			input, input + 2u, __builtin_addressof(normalizations)}, value));
	require(value == 17u && normalizations == 3u);
	value = 0u;
	::fast_io::io::scan<false>(
		named_source<shared_projection>{
			input, input + 2u, __builtin_addressof(normalizations)}, value);
	require(value == 17u && normalizations == 4u);
}

} // namespace scan_named_source_category

int main()
{
	::scan_named_source_category::verify_named_source_category<false>();
	::scan_named_source_category::verify_named_source_category<true>();
	::scan_named_source_category::verify_const_source();
}
