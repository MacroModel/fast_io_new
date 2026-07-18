#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct lvalue_scatter
{
	char value{};
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, lvalue_scatter>, lvalue_scatter &value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, lvalue_scatter>) noexcept
{
	return {};
}

struct rvalue_scatter
{
	char value{};
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, rvalue_scatter>, rvalue_scatter &&value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, rvalue_scatter>) noexcept
{
	return {};
}

struct dual_category_scatter
{
	char value{};
};

struct tracked_scatter_storage
{
	char value{};
};

inline tracked_scatter_storage tracked_scatter_pool[32u]{};
inline ::std::size_t tracked_scatter_pool_position{};

inline tracked_scatter_storage *acquire_tracked_scatter_storage(char value) noexcept
{
	auto storage{__builtin_addressof(tracked_scatter_pool[tracked_scatter_pool_position++])};
	storage->value = value;
	return storage;
}

struct rvalue_tracked_scatter
{
	tracked_scatter_storage *storage{};
	bool active{};

	explicit rvalue_tracked_scatter(char value) noexcept
		: storage(acquire_tracked_scatter_storage(value)), active(true)
	{
	}

	rvalue_tracked_scatter(rvalue_tracked_scatter const &other) noexcept
		: storage(acquire_tracked_scatter_storage(other.storage->value)), active(true)
	{
	}

	rvalue_tracked_scatter(rvalue_tracked_scatter &&other) noexcept
		: storage(other.storage), active(other.active)
	{
		other.active = false;
	}

	~rvalue_tracked_scatter()
	{
		if (active)
		{
			// The backing cell deliberately outlives the object, but destruction poisons its observable sequence. This
			// makes an early strategy-local copy destruction deterministic without reading storage after its lifetime.
			storage->value = '!';
		}
	}
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_scatter>,
	rvalue_tracked_scatter &value) noexcept
{
	return {__builtin_addressof(value.storage->value), 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_scatter>) noexcept
{
	return {};
}

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, dual_category_scatter>, dual_category_scatter &value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, dual_category_scatter>, dual_category_scatter &&value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

static_assert(!::fast_io::scatter_printable<char, lvalue_scatter>);
static_assert(::fast_io::scatter_printable_for<char, lvalue_scatter &>);
static_assert(::fast_io::scatter_printable<char, rvalue_scatter>);
static_assert(!::fast_io::scatter_printable_for<char, rvalue_scatter &>);
static_assert(::fast_io::borrowed_scatter_source<char, lvalue_scatter>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<char, lvalue_scatter>);
static_assert(::fast_io::borrowed_scatter_source<char, rvalue_tracked_scatter>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<char, rvalue_tracked_scatter>);

using normalized_lvalue_scatter = decltype(::fast_io::io_print_forward<char>(::std::declval<lvalue_scatter &>()));
using normalized_bare_scatter = decltype(::fast_io::io_print_forward<char>(
	::std::declval<::fast_io::basic_io_scatter_t<char> &>()));
// The self-borrowing producer becomes one reference-wrapper value; the already-external bare descriptor retains the
// ABI-small value path through its stronger source-independence proof.
static_assert(::std::same_as<normalized_lvalue_scatter, ::fast_io::parameter<lvalue_scatter &>>);
static_assert(::std::same_as<normalized_bare_scatter, ::fast_io::basic_io_scatter_t<char>>);
static_assert(::fast_io::copy_stable_borrowed_print_source<char, normalized_lvalue_scatter>);
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_forward<char>(::std::declval<normalized_lvalue_scatter &>())),
			  normalized_lvalue_scatter>);

// The decayed dispatcher invokes its by-value copy as a named lvalue. These assertions prove that strategy admission
// follows that expression instead of inheriting the public T&& compatibility query in either direction.
static_assert(::fast_io::details::decay::print_freestanding_decay_param_okay_single<
			  char, lvalue_scatter>::value);
static_assert(!::fast_io::details::decay::print_freestanding_decay_param_okay_single<
			  char, rvalue_scatter>::value);
static_assert(::fast_io::details::decay::concat_retained_scatter_printable_v<
			  char, lvalue_scatter>);
static_assert(!::fast_io::details::decay::concat_retained_scatter_printable_v<
			  char, rvalue_scatter>);

static_assert(!::fast_io::range_two_pass_scatter_printable_v<char, lvalue_scatter>);
static_assert(::fast_io::range_two_pass_scatter_printable_v<char, dual_category_scatter>);

using lvalue_array = ::std::array<lvalue_scatter, 2u>;
using lvalue_range = decltype(::fast_io::mnp::rgvw(::std::declval<lvalue_array &>(), ","));
// Raw values cannot be copied safely for a retained two-pass plan, but range normalization carries each element as a
// compact reference parameter. Both passes therefore observe the same caller-owned element and the sized strategy is
// sound again; retaining this optimization is the purpose of identity-preserving entry decay.
static_assert(::std::same_as<lvalue_range, ::fast_io::sized_range_view_t<char, lvalue_scatter *>>);

struct capture_sink
{
	using output_char_type = char;
	::std::string *output{};
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(capture_sink sink, char const *first, char const *last)
{
	sink.output->append(first, last);
}

inline void test_named_lvalue_print_and_concat()
{
	lvalue_scatter value{'L'};
	::std::string output;
	::fast_io::print(capture_sink{__builtin_addressof(output)}, value);
	assert(output == "L");
	assert(::fast_io::concat_std(value) == "L");
}

inline void test_parameter_scatter_adapter_preserves_wrapper_storage()
{
	::fast_io::parameter<lvalue_scatter> owned{{'O'}};
	auto const owned_scatter{print_scatter_define(
		::fast_io::io_reserve_type<char, decltype(owned)>, owned)};
	assert(owned_scatter.base == __builtin_addressof(owned.reference.value));
	assert(owned_scatter.len == 1u);

	char external{'C'};
	::fast_io::parameter<::fast_io::basic_io_scatter_t<char>> const const_wrapper{
		{__builtin_addressof(external), 1u}};
	auto const const_scatter{print_scatter_define(
		::fast_io::io_reserve_type<char, ::std::remove_cv_t<decltype(const_wrapper)>>,
		const_wrapper)};
	assert(const_scatter.base == __builtin_addressof(external));
	assert(const_scatter.len == 1u);
}

inline void test_owned_rvalue_survives_retained_consumption()
{
	tracked_scatter_pool_position = 0u;
	::std::string output;
	::fast_io::print(capture_sink{__builtin_addressof(output)},
					 rvalue_tracked_scatter{'P'}, rvalue_tracked_scatter{'Q'});
	assert(output == "PQ");
	assert(::fast_io::concat_std(rvalue_tracked_scatter{'R'}) == "R");
}

inline void test_lvalue_only_range_uses_single_pass_fallback()
{
	lvalue_array values{{{'A'}, {'B'}}};
	auto range{::fast_io::mnp::rgvw(values, ",")};
	assert(::fast_io::concat_std(range) == "A,B");
}

inline void test_compiled_plan_direct_call_uses_named_arguments()
{
	constexpr auto plan{::fast_io::make_scatter_plan<char>(::fast_io::mnp::scatter_dynamic<0>)};
	lvalue_scatter value{'P'};
	::std::string output;
	plan.print(capture_sink{__builtin_addressof(output)}, value);
	assert(output == "P");
}

} // namespace

int main()
{
	test_named_lvalue_print_and_concat();
	test_parameter_scatter_adapter_preserves_wrapper_storage();
	test_owned_rvalue_survives_retained_consumption();
	test_lvalue_only_range_uses_single_pass_fallback();
	test_compiled_plan_direct_call_uses_named_arguments();
}
