// Compile-only coverage for the exact-status graph. No ordinary emitter is instantiated here.
#include <fast_io.h>

namespace fast_io::operations::decay
{
#include <fast_io_core_impl/operations/printimpl/print_semantic_status_probe.h>
}

namespace semantic_status_probe_contract
{
using static_two = ::fast_io::manipulators::static_scatter_t<char, 2u>;
using static_three = ::fast_io::manipulators::static_scatter_t<char, 3u>;
using character = ::fast_io::manipulators::chvw_t<char>;
using optional_two = ::fast_io::manipulators::condition<static_two, ::fast_io::io_null_t>;
using optional_three = ::fast_io::manipulators::condition<static_three, ::fast_io::io_null_t>;
using optional_character = ::fast_io::manipulators::condition<character, ::fast_io::io_null_t>;
using nested_optional = ::fast_io::manipulators::condition<optional_two, optional_three>;

// Intentionally lacks any standalone print formatter: only exact whole-record status lookup is relevant.
struct marker
{};

struct without_status
{
	using output_char_type = char;
};

struct sparse_status
{
	using output_char_type = char;
};

template <bool line>
void status_print_define(sparse_status, static_two &, marker const &, static_three &) noexcept;

struct empty_status
{
	using output_char_type = char;
};

template <bool line>
	requires(!line)
void status_print_define(empty_status) noexcept;

struct const_argument_status
{
	using output_char_type = char;
};

template <bool line, typename T>
	requires(::std::same_as<T, marker const>)
void status_print_define(const_argument_status, T &) noexcept;

struct const_output_status
{
	using output_char_type = char;
};

template <bool line, typename Out>
	requires(::std::same_as<Out, const_output_status const>)
void status_print_define(Out &, marker &) noexcept;

struct character_status
{
	using output_char_type = char;
};

template <bool line>
void status_print_define(character_status, character &, marker &) noexcept;

struct nested_status
{
	using output_char_type = char;
};

template <bool line>
void status_print_define(nested_status, static_three &, marker &) noexcept;

template <bool line, typename Output, typename... Args>
inline constexpr bool any{
	::fast_io::operations::decay::print_semantic_any_exact_status<line, char, Output, Args...>()};

static_assert(!any<false, without_status, optional_two, marker const, optional_three>);
static_assert(!any<true, without_status, optional_two const, marker, nested_optional const>);

// The customization exists only for the all-selected record, not the source condition pack or either singleton.
static_assert(any<false, sparse_status, optional_two, marker const, optional_three>);
static_assert(any<true, sparse_status, optional_two const, marker const, optional_three const>);
static_assert(!any<false, sparse_status, optional_three, marker const, optional_two>);

// Exact empty-record ownership remains line-specific, even when every source condition disappears.
static_assert(any<false, empty_status, optional_two, optional_three>);
static_assert(!any<true, empty_status, optional_two, optional_three>);
static_assert(any<false, empty_status>);
static_assert(!any<true, empty_status>);

// Mandatory named arguments and the output preserve const qualification; selected literal forwarding owns mutable
// named values even when its original condition object is const.
static_assert(any<false, const_argument_status, optional_two, marker const>);
static_assert(!any<false, const_argument_status, optional_two, marker>);
static_assert(any<false, const_output_status const, marker>);
static_assert(!any<false, const_output_status, marker>);
static_assert(any<false, character_status, optional_character const, marker>);
static_assert(any<false, nested_status, nested_optional const, marker>);

using unsupported_arm = ::fast_io::manipulators::condition<marker, ::fast_io::io_null_t>;
static_assert(any<false, without_status, unsupported_arm>);
static_assert(any<false, without_status, optional_two volatile>);

// A supplied explicit null is erased by the same selector whenever this semantic path is selected.
static_assert(any<false, empty_status, ::fast_io::io_null_t>);
static_assert(!any<false, sparse_status, ::fast_io::io_null_t, marker>);

template <::std::size_t index>
struct separator
{};

template <::std::size_t extent>
using factory_optional = decltype(::fast_io::details::decay::print_semantic_input_forward<char>(
	::fast_io::mnp::cond(false, ::fast_io::manipulators::static_scatter_t<char, extent>{})));

struct mask_ten_status
{
	using output_char_type = char;
};

// Only mask 1010 (the second and fourth conditions active) owns status. Physical factory arm ordering is deliberately
// not assumed: the proof must find the shape irrespective of which branch it traverses first.
template <bool line>
void status_print_define(mask_ten_status,
						 separator<0u> &, static_three &, separator<1u> &, separator<2u> &,
						 ::fast_io::manipulators::static_scatter_t<char, 5u> &, separator<3u> &) noexcept;

static_assert(any<false, mask_ten_status,
				  factory_optional<2u>, separator<0u>, factory_optional<3u>, separator<1u>,
				  factory_optional<4u>, separator<2u>, factory_optional<5u>, separator<3u>>);
static_assert(any<true, mask_ten_status,
				  factory_optional<2u> const, separator<0u>, factory_optional<3u> const, separator<1u>,
				  factory_optional<4u> const, separator<2u>, factory_optional<5u> const, separator<3u>>);
static_assert(!any<false, without_status,
				   factory_optional<2u>, separator<0u>, factory_optional<3u>, separator<1u>,
				   factory_optional<4u>, separator<2u>, factory_optional<5u>, separator<3u>>);

namespace base_associated_status
{
struct base_leaf
{
	template <bool line>
	friend void status_print_define(without_status, static_three &, base_leaf &) noexcept;
};

struct derived_leaf : base_leaf
{};

struct base_output
{
	template <bool line>
	friend void status_print_define(base_output &, static_three &, marker &) noexcept;
};

struct derived_output : base_output
{
	using output_char_type = char;
};
} // namespace base_associated_status

// Hidden friends introduced only through a source or destination base class remain part of exact ADL lookup.
static_assert(any<false, without_status, optional_three, base_associated_status::derived_leaf>);
static_assert(!any<false, without_status, optional_two, base_associated_status::derived_leaf>);
static_assert(any<true, base_associated_status::derived_output, optional_three, marker>);
static_assert(!any<true, base_associated_status::derived_output, optional_two, marker>);

template <bool line, typename Output, typename... Args>
consteval bool matches_recursive_reference()
{
	return any<line, Output, Args...> ==
		   ::fast_io::operations::decay::print_semantic_status_probe_any_impl<line, char, Output,
																			  ::fast_io::operations::decay::print_semantic_status_probe_types<>,
																			  ::fast_io::operations::decay::print_semantic_status_probe_types<
																				  ::fast_io::operations::decay::print_semantic_status_probe_input<false, Args &>...>>::value;
}

static_assert(matches_recursive_reference<false, mask_ten_status,
										  factory_optional<2u>, separator<0u>, factory_optional<3u>, separator<1u>,
										  factory_optional<4u>, separator<2u>, factory_optional<5u>, separator<3u>>());
static_assert(matches_recursive_reference<true, without_status,
										  factory_optional<2u> const, separator<0u>, factory_optional<3u>, separator<1u>,
										  factory_optional<4u> const, separator<2u>, factory_optional<5u>, separator<3u>>());
static_assert(matches_recursive_reference<false, empty_status, optional_two const, optional_three>());
static_assert(matches_recursive_reference<true, empty_status, optional_two const, optional_three>());
static_assert(matches_recursive_reference<false, character_status, optional_character const, marker>());
static_assert(matches_recursive_reference<true, base_associated_status::derived_output, optional_three, marker>());
static_assert(matches_recursive_reference<false, without_status, optional_two, base_associated_status::derived_leaf>());
static_assert(matches_recursive_reference<false, const_argument_status, optional_two, marker const>());
static_assert(matches_recursive_reference<false, const_argument_status, optional_two, marker>());

template <typename T>
struct exact_mask_leaf_code
	: ::std::integral_constant<unsigned, 32u>
{};

template <::std::size_t extent>
struct exact_mask_leaf_code<::fast_io::manipulators::static_scatter_t<char, extent>>
	: ::std::integral_constant<unsigned, (extent >= 2u && extent <= 5u ? 1u << (extent - 2u) : 32u)>
{};

template <typename... Args>
consteval unsigned exact_mask_pack_code()
{
	constexpr unsigned codes[]{exact_mask_leaf_code<Args>::value..., 0u};
	unsigned previous{};
	unsigned result{};
	for (::std::size_t index{}; index != sizeof...(Args); ++index)
	{
		if (codes[index] <= previous || codes[index] >= 32u)
		{
			return 32u;
		}
		previous = codes[index];
		result |= previous;
	}
	return result;
}

template <unsigned mask>
struct exact_mask_owner
{
	using output_char_type = char;

	template <bool line, typename... Args>
		requires(!line && exact_mask_pack_code<Args...>() == mask)
	friend void status_print_define(exact_mask_owner, Args &...) noexcept
	{}
};

template <::std::size_t... mask>
consteval bool every_single_mask_owner(::std::index_sequence<mask...>)
{
	return ((any<false, exact_mask_owner<mask>,
				 factory_optional<2u>, factory_optional<3u>, factory_optional<4u>, factory_optional<5u>> &&
			 !any<true, exact_mask_owner<mask>,
				  factory_optional<2u>, factory_optional<3u>, factory_optional<4u>, factory_optional<5u>> &&
			 matches_recursive_reference<false, exact_mask_owner<mask>,
										 factory_optional<2u>, factory_optional<3u>, factory_optional<4u>, factory_optional<5u>>()) &&
			...);
}

// Each destination owns precisely one of all sixteen masks. This catches lost branches even if traversal order or
// early positive short circuit changes; line=true verifies that an owner's other policy is not accidentally reused.
static_assert(every_single_mask_owner(::std::make_index_sequence<16u>{}));

#if __cplusplus > 202302L && __cpp_pack_indexing >= 202311L
using identical_arms = ::fast_io::manipulators::condition<static_two, static_two>;
using identical_null_arms = ::fast_io::manipulators::condition<::fast_io::io_null_t, ::fast_io::io_null_t const>;
static_assert(::fast_io::operations::decay::print_semantic_status_probe_mask_graph<
				  false, char, without_status, identical_arms, identical_null_arms>::choice_count == 0u);
static_assert(matches_recursive_reference<false, sparse_status,
										  identical_arms const, marker const, optional_three, identical_null_arms>());
static_assert(matches_recursive_reference<true, without_status,
										  identical_arms, marker const, optional_three, identical_null_arms>());

using canonical_graph = ::fast_io::operations::decay::print_semantic_status_probe_mask_graph<
	false, char, without_status, optional_two, marker, optional_three>;
using const_condition_graph = ::fast_io::operations::decay::print_semantic_status_probe_mask_graph<
	false, char, without_status, optional_two const, marker, optional_three const>;
using parameter_condition_graph = ::fast_io::operations::decay::print_semantic_status_probe_mask_graph<
	false, char, without_status, ::fast_io::parameter<optional_two const &>, marker,
	::fast_io::parameter<optional_three &>>;
static_assert(::std::same_as<canonical_graph, const_condition_graph>);
static_assert(::std::same_as<canonical_graph, parameter_condition_graph>);
static_assert(!::std::same_as<canonical_graph,
							  ::fast_io::operations::decay::print_semantic_status_probe_mask_graph<
								  false, char, without_status, optional_two, marker const, optional_three>>);
#endif

struct adjusted_array_status
{
	using output_char_type = char;
};

template <bool line>
void status_print_define(adjusted_array_status, char *&) noexcept;

struct adjusted_function_status
{
	using output_char_type = char;
};

template <bool line>
void status_print_define(adjusted_function_status, void (*&)()) noexcept;

// Named requires-parameters retain the original concept's array and function adjustment rules.
static_assert(any<false, adjusted_array_status, char[4u]>);
static_assert(any<false, adjusted_function_status, void()>);
static_assert(matches_recursive_reference<false, adjusted_array_status, char[4u]>());
static_assert(matches_recursive_reference<false, adjusted_function_status, void()>());
} // namespace semantic_status_probe_contract

int main()
{}
