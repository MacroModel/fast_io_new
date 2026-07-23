#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace semantic_const_pack_expression_proof
{

struct leaf
{
	char value{'x'};
	char padding[128]{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, leaf>, leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, leaf>, char *destination, leaf &value) noexcept
{
	*destination = value.value;
	return destination + 1;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, leaf>, leaf &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, leaf>, char *destination, ::std::size_t,
	leaf &value) noexcept
{
	*destination = value.value;
	return destination + 1;
}

using pack = decltype(::fast_io::mnp::pack(leaf{}));

struct source
{
	pack const *value{};
};

inline constexpr pack const &print_alias_define(
	::fast_io::io_alias_t, source &value) noexcept
{
	return *value.value;
}

struct sink
{
	using output_char_type = char;
	int *calls{};
};

inline constexpr sink output_stream_ref_define(sink output) noexcept
{
	return output;
}

template <bool line>
inline void status_print_define(
	sink output, ::fast_io::parameter<leaf const &> &) noexcept
{
	static_assert(!line);
	++*output.calls;
}

using condition = ::fast_io::manipulators::condition<source, source>;
using mutable_pack_parameter = ::fast_io::parameter<pack &>;
using const_pack_parameter = ::fast_io::parameter<pack const &>;

static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, mutable_pack_parameter>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, mutable_pack_parameter>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, const_pack_parameter>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, const_pack_parameter>::value);
static_assert(::fast_io::details::decay::print_semantic_params_okay<
			  char, mutable_pack_parameter &>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<
			  char, const_pack_parameter &>::value);
static_assert(
	::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
		false, sink &, condition &>);

struct lvalue_scatter_leaf
{};

inline constexpr char lvalue_scatter_bytes[]{"L"};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, lvalue_scatter_leaf>,
	lvalue_scatter_leaf &) noexcept
{
	return {lvalue_scatter_bytes, 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, lvalue_scatter_leaf>) noexcept
{
	return {};
}

struct rvalue_scatter_leaf
{};

inline constexpr char rvalue_scatter_bytes[]{"R"};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, rvalue_scatter_leaf>,
	rvalue_scatter_leaf &&) noexcept
{
	return {rvalue_scatter_bytes, 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, rvalue_scatter_leaf>) noexcept
{
	return {};
}

struct const_result_scatter_leaf
{
	char force_reference_transport[128]{};
};

inline constexpr char const_result_scatter_bytes[]{"C"};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, const_result_scatter_leaf>,
	const_result_scatter_leaf &) noexcept
{
	return {const_result_scatter_bytes, 1u};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, const_result_scatter_leaf>) noexcept
{
	return {};
}

struct const_result_source
{
	const_result_scatter_leaf const *value{};
};

inline constexpr const_result_source &print_alias_define(
	::fast_io::io_alias_t, const_result_source &value) noexcept
{
	return value;
}

inline constexpr const_result_scatter_leaf const &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, const_result_source &value) noexcept
{
	return *value.value;
}

struct lvalue_result_source
{};

inline constexpr lvalue_scatter_leaf print_alias_define(
	::fast_io::io_alias_t, lvalue_result_source &) noexcept
{
	return {};
}

struct rvalue_result_source
{};

inline constexpr rvalue_scatter_leaf print_alias_define(
	::fast_io::io_alias_t, rvalue_result_source &) noexcept
{
	return {};
}

using lvalue_result_condition =
	::fast_io::manipulators::condition<lvalue_result_source, lvalue_result_source>;
using rvalue_result_condition =
	::fast_io::manipulators::condition<rvalue_result_source, rvalue_result_source>;
using lvalue_result_pack =
	::fast_io::manipulators::pack_t<lvalue_result_source>;
using rvalue_result_pack =
	::fast_io::manipulators::pack_t<rvalue_result_source>;
using lvalue_result_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left, lvalue_result_source>;
using rvalue_result_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left, rvalue_result_source>;

using lvalue_forwarding_result =
	::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
		char, lvalue_result_source &>;
using rvalue_forwarding_result =
	::fast_io::details::decay::print_semantic_input_forwarded_arg_t<
		char, rvalue_result_source &>;
using stable_lvalue_forwarding_expression =
	::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
		char, lvalue_result_source &>;
using stable_rvalue_forwarding_expression =
	::fast_io::details::decay::print_semantic_stable_input_forwarded_arg_t<
		char, rvalue_result_source &>;
using const_forwarding_result = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(::std::declval<const_result_source &>())));

// Both forwarding CPOs return values. Runtime binds each value to a named local,
// so the strategy proof must test the resulting mutable lvalue expression.
static_assert(::std::is_same_v<lvalue_forwarding_result, lvalue_scatter_leaf>);
static_assert(::std::is_same_v<rvalue_forwarding_result, rvalue_scatter_leaf>);
static_assert(::std::is_same_v<stable_lvalue_forwarding_expression,
							   lvalue_scatter_leaf &>);
static_assert(::std::is_same_v<stable_rvalue_forwarding_expression,
							   rvalue_scatter_leaf &>);
static_assert(::fast_io::scatter_printable_for<char, lvalue_scatter_leaf &>);
static_assert(!::fast_io::scatter_printable_for<char, lvalue_scatter_leaf &&>);
static_assert(!::fast_io::scatter_printable_for<char, rvalue_scatter_leaf &>);
static_assert(::fast_io::scatter_printable_for<char, rvalue_scatter_leaf &&>);
static_assert(::std::is_same_v<
			  const_forwarding_result,
			  ::fast_io::parameter<const_result_scatter_leaf const &>>);
static_assert(
	::fast_io::scatter_printable_for<char, const_result_scatter_leaf &>);
static_assert(
	!::fast_io::scatter_printable_for<char, const_result_scatter_leaf const &>);
static_assert(!::fast_io::details::decay::
				  print_freestanding_decay_param_okay_single<
					  char, const_forwarding_result>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, lvalue_result_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, lvalue_result_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, lvalue_result_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, lvalue_result_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, lvalue_result_width>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, lvalue_result_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, rvalue_result_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, rvalue_result_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, rvalue_result_pack>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, rvalue_result_pack>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, rvalue_result_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, rvalue_result_width>::value);

struct scatter_sink
{
	using output_char_type = char;
	char *value{};
};

inline constexpr scatter_sink output_stream_ref_define(
	scatter_sink output) noexcept
{
	return output;
}

inline void write_all_overflow_define(
	scatter_sink output, char const *first, char const *last) noexcept
{
	assert(last - first == 1);
	*output.value = *first;
}

} // namespace semantic_const_pack_expression_proof

int main()
{
	using namespace semantic_const_pack_expression_proof;

	pack stored_pack{leaf{}};
	source selected{__builtin_addressof(stored_pack)};
	int calls{};
	condition value{true, selected, selected};
	::fast_io::io::print(sink{__builtin_addressof(calls)}, value);
	assert(calls == 1);

	char byte{};
	lvalue_result_condition lvalue_condition{true, {}, {}};
	::fast_io::io::print(
		scatter_sink{__builtin_addressof(byte)}, lvalue_condition);
	assert(byte == 'L');
}
