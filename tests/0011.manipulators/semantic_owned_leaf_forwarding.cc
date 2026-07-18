#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace semantic_owned_leaf_forwarding
{

struct protocol_counts
{
	::std::size_t forward_calls{};
	::std::size_t move_calls{};
	::std::size_t live_owners{};
	::std::size_t maximum_live_owners{};
	::std::size_t precise_size_calls{};
	::std::size_t precise_define_calls{};
	::std::size_t reserve_size_calls{};
	::std::size_t reserve_define_calls{};
	::std::size_t internal_shift_calls{};
};

struct owned_leaf
{
	protocol_counts *counts{};
	char prefix{};
	char suffix{};
	bool owns{};

	inline explicit owned_leaf(protocol_counts *state, char first, char second) noexcept
		: counts(state), prefix(first), suffix(second), owns(true)
	{
		++counts->live_owners;
		counts->maximum_live_owners = ::std::max(
			counts->maximum_live_owners, counts->live_owners);
	}

	owned_leaf(owned_leaf const &) = delete;
	owned_leaf &operator=(owned_leaf const &) = delete;

	inline owned_leaf(owned_leaf &&other) noexcept
		: counts(::std::exchange(other.counts, nullptr)),
		  prefix(other.prefix), suffix(other.suffix), owns(::std::exchange(other.owns, false))
	{
		++counts->move_calls;
	}

	owned_leaf &operator=(owned_leaf &&) = delete;

	inline ~owned_leaf()
	{
		if (owns)
		{
			assert(counts != nullptr && counts->live_owners == 1u);
			--counts->live_owners;
		}
	}
};

inline void observe_owned_leaf(owned_leaf &leaf) noexcept
{
	// Every protocol below intentionally accepts only `owned_leaf&`. The run-time ownership check complements that
	// compile-time category proof: status forwarding may move the prvalue through its transport helper, but there must
	// remain exactly one live logical owner when sizing, emission, or internal-placement metadata observes the leaf.
	assert(leaf.owns && leaf.counts != nullptr);
	assert(leaf.counts->live_owners == 1u);
}

struct semantic_source
{
	protocol_counts *counts{};
	char prefix{};
	char suffix{};
};

inline owned_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, semantic_source &source) noexcept
{
	++source.counts->forward_calls;
	return owned_leaf{source.counts, source.prefix, source.suffix};
}

inline owned_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, semantic_source &&source) noexcept
{
	// A plain top-level temporary reaches status forwarding before it is stored; members of width/condition/pack nodes
	// reach the lvalue overload above. Both routes create the same owned leaf, after which all leaf CPOs are lvalue-only.
	++source.counts->forward_calls;
	return owned_leaf{source.counts, source.prefix, source.suffix};
}

// The leaf deliberately exposes no const, value, or rvalue formatting overload. These mutable-lvalue-only signatures
// are the executable specification of the semantic one-owner model: after forwarding creates owned storage, every
// deeper strategy must use that named storage rather than reconstructing an xvalue or copying the formatter.
inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, owned_leaf>, owned_leaf &leaf) noexcept
{
	observe_owned_leaf(leaf);
	++leaf.counts->reserve_size_calls;
	return 2u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, owned_leaf>, char *iter, owned_leaf &leaf) noexcept
{
	observe_owned_leaf(leaf);
	++leaf.counts->reserve_define_calls;
	*iter++ = leaf.prefix;
	*iter++ = leaf.suffix;
	return iter;
}

inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, owned_leaf>, owned_leaf &leaf) noexcept
{
	observe_owned_leaf(leaf);
	++leaf.counts->precise_size_calls;
	return 2u;
}

inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, owned_leaf>, char *iter,
	::std::size_t size, owned_leaf &leaf) noexcept
{
	observe_owned_leaf(leaf);
	assert(size == 2u);
	++leaf.counts->precise_define_calls;
	*iter++ = leaf.prefix;
	*iter++ = leaf.suffix;
	return iter;
}

inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, owned_leaf>, owned_leaf &leaf) noexcept
{
	observe_owned_leaf(leaf);
	++leaf.counts->internal_shift_calls;
	return 1u;
}

static_assert(!::std::copy_constructible<owned_leaf>);
static_assert(::std::move_constructible<owned_leaf>);
static_assert(::fast_io::dynamic_reserve_printable<char, owned_leaf>);
static_assert(::fast_io::precise_reserve_printable<char, owned_leaf>);
static_assert(::fast_io::printable_internal_shift<char, owned_leaf>);
static_assert(::std::same_as<
			  decltype(::fast_io::details::decay::print_semantic_input_forward<char>(
				  ::std::declval<semantic_source &>())),
			  owned_leaf>);

using positive_width = decltype(::fast_io::mnp::internal(::std::declval<semantic_source>(), 5u, '.'));
using positive_condition = decltype(::fast_io::mnp::cond(true, ::std::declval<semantic_source>()));
using positive_pack = decltype(::fast_io::mnp::pack(::std::declval<semantic_source>(),
													::std::declval<semantic_source>()));
using positive_nested = decltype(::fast_io::mnp::pack(
	::fast_io::mnp::internal(
		::fast_io::mnp::cond(true, ::std::declval<semantic_source>()), 5u, '.'),
	::std::string_view{}));

static_assert(::fast_io::details::decay::print_semantic_params_okay<char, positive_width>::value);
static_assert(::fast_io::details::decay::print_semantic_params_okay<char, positive_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_params_okay<char, positive_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_params_okay<char, positive_nested>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<char, positive_width>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<char, positive_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<char, positive_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<char, positive_nested>::value);

struct rvalue_only_leaf
{};

struct rvalue_only_source
{};

inline rvalue_only_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>, rvalue_only_source &) noexcept
{
	return {};
}

// These overloads make the negative probe meaningful: the leaf has a coherent formatting vocabulary, but only for a
// discarded xvalue. A stored semantic member is a named lvalue, so structural admission must not use these overloads
// as evidence for a call that the emitter cannot make.
[[maybe_unused]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, rvalue_only_leaf>, rvalue_only_leaf &&) noexcept
{
	return 2u;
}

[[maybe_unused]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, rvalue_only_leaf>, char *iter,
	rvalue_only_leaf &&) noexcept
{
	return iter + 2;
}

[[maybe_unused]] inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, rvalue_only_leaf>, rvalue_only_leaf &&) noexcept
{
	return 2u;
}

[[maybe_unused]] inline char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, rvalue_only_leaf>, char *iter,
	::std::size_t, rvalue_only_leaf &&) noexcept
{
	return iter + 2;
}

[[maybe_unused]] inline ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char, rvalue_only_leaf>, rvalue_only_leaf &&) noexcept
{
	return 1u;
}

template <typename T>
concept has_rvalue_only_leaf_vocabulary = requires(char *iter, T &&value) {
	{
		print_reserve_size(
			::fast_io::io_reserve_type<char, ::std::remove_cvref_t<T>>,
			::std::forward<T>(value))
	} -> ::std::same_as<::std::size_t>;
	{
		print_reserve_define(
			::fast_io::io_reserve_type<char, ::std::remove_cvref_t<T>>, iter,
			::std::forward<T>(value))
	} -> ::std::same_as<char *>;
	{
		print_reserve_precise_size(
			::fast_io::io_reserve_type<char, ::std::remove_cvref_t<T>>,
			::std::forward<T>(value))
	} -> ::std::same_as<::std::size_t>;
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char, ::std::remove_cvref_t<T>>, iter, 2u,
			::std::forward<T>(value))
	} -> ::std::same_as<char *>;
	{
		print_define_internal_shift(
			::fast_io::io_reserve_type<char, ::std::remove_cvref_t<T>>,
			::std::forward<T>(value))
	} -> ::std::same_as<::std::size_t>;
};

static_assert(has_rvalue_only_leaf_vocabulary<rvalue_only_leaf>);
static_assert(!::fast_io::dynamic_reserve_printable<char, rvalue_only_leaf>);
static_assert(!::fast_io::precise_reserve_printable<char, rvalue_only_leaf>);
static_assert(!::fast_io::printable_internal_shift<char, rvalue_only_leaf>);

using negative_width = decltype(::fast_io::mnp::internal(::std::declval<rvalue_only_source>(), 5u, '.'));
using negative_condition = decltype(::fast_io::mnp::cond(true, ::std::declval<rvalue_only_source>()));
using negative_pack = decltype(::fast_io::mnp::pack(::std::declval<rvalue_only_source>(),
													::std::declval<rvalue_only_source>()));
using negative_nested = decltype(::fast_io::mnp::pack(
	::fast_io::mnp::internal(
		::fast_io::mnp::cond(true, ::std::declval<rvalue_only_source>()), 5u, '.'),
	::std::string_view{}));

static_assert(!::fast_io::details::decay::print_semantic_params_okay<char, negative_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<char, negative_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<char, negative_pack>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<char, negative_nested>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<char, negative_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<char, negative_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<char, negative_pack>::value);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<char, negative_nested>::value);

template <typename T>
inline ::std::string render_print(T &&value)
{
	::std::string result;
	::fast_io::ostring_ref_std output{__builtin_addressof(result)};
	::fast_io::print(output, ::std::forward<T>(value));
	return result;
}

inline void require_drained(protocol_counts const &counts) noexcept
{
	assert(counts.live_owners == 0u);
	assert(counts.maximum_live_owners == 1u);
}

inline void test_one_named_owner_directly()
{
	protocol_counts counts;
	semantic_source source{__builtin_addressof(counts), '-', 'H'};
	{
		auto leaf{
			::fast_io::details::decay::print_semantic_input_forward<char>(source)};
		assert(counts.live_owners == 1u);
		assert(::fast_io::operations::decay::print_semantic_precise_size<char>(leaf) == 2u);
		assert(::fast_io::operations::decay::print_semantic_internal_shift<char>(leaf) == 1u);

		char buffer[2];
		char *const end{
			::fast_io::operations::decay::print_semantic_emit_prepared_width_child<char>(
				buffer, 2u, leaf)};
		assert(end == buffer + 2u);
		assert(::std::string_view(buffer, 2u) == "-H");
		assert(counts.live_owners == 1u);
	}

	require_drained(counts);
	assert(counts.forward_calls == 1u);
	assert(counts.precise_size_calls == 1u);
	assert(counts.precise_define_calls == 1u);
	assert(counts.internal_shift_calls == 1u);
}

inline void test_top_level_concat_and_print()
{
	protocol_counts concat_counts;
	assert(::fast_io::concat_std(
			   semantic_source{__builtin_addressof(concat_counts), '-', 'C'}) == "-C");
	require_drained(concat_counts);
	assert(concat_counts.forward_calls != 0u);
	assert(concat_counts.precise_define_calls + concat_counts.reserve_define_calls != 0u);

	protocol_counts print_counts;
	assert(render_print(
			   semantic_source{__builtin_addressof(print_counts), '+', 'P'}) == "+P");
	require_drained(print_counts);
	assert(print_counts.forward_calls != 0u);
	assert(print_counts.precise_define_calls + print_counts.reserve_define_calls != 0u);
}

inline void test_prepared_and_internal_width()
{
	protocol_counts prepared_counts;
	semantic_source prepared{__builtin_addressof(prepared_counts), '-', 'D'};
	::std::string prepared_output;
	::fast_io::ostring_ref_std prepared_sink{__builtin_addressof(prepared_output)};
	auto prepared_ref{::fast_io::operations::output_stream_ref(prepared_sink)};
	::fast_io::operations::decay::print_semantic_emit_width_direct<false, char>(
		prepared_ref, prepared, 5u, '.', 4u, 2u);
	assert(prepared_output == "-...D");
	require_drained(prepared_counts);
	assert(prepared_counts.precise_define_calls == 1u);
	assert(prepared_counts.internal_shift_calls == 1u);

	protocol_counts concat_width_counts;
	assert(::fast_io::concat_std(::fast_io::mnp::right(
			   semantic_source{__builtin_addressof(concat_width_counts), '-', 'R'},
			   5u, '.')) == "...-R");
	require_drained(concat_width_counts);
	assert(concat_width_counts.precise_size_calls != 0u);
	assert(concat_width_counts.precise_define_calls != 0u);

	protocol_counts print_width_counts;
	assert(render_print(::fast_io::mnp::internal(
			   semantic_source{__builtin_addressof(print_width_counts), '+', 'I'},
			   5u, '.')) == "+...I");
	require_drained(print_width_counts);
	assert(print_width_counts.internal_shift_calls != 0u);
}

inline void test_condition_pack_and_nested_composition()
{
	protocol_counts selected_counts;
	protocol_counts inactive_counts;
	auto selected{::fast_io::mnp::cond(
		true,
		semantic_source{__builtin_addressof(selected_counts), '-', 'T'},
		semantic_source{__builtin_addressof(inactive_counts), '-', 'F'})};
	assert(::fast_io::concat_std(selected) == "-T");
	require_drained(selected_counts);
	assert(inactive_counts.forward_calls == 0u && inactive_counts.live_owners == 0u);

	protocol_counts first_pack_counts;
	protocol_counts second_pack_counts;
	auto packed{::fast_io::mnp::pack(
		semantic_source{__builtin_addressof(first_pack_counts), 'A', '1'},
		semantic_source{__builtin_addressof(second_pack_counts), 'B', '2'})};
	assert(render_print(packed) == "A1B2");
	require_drained(first_pack_counts);
	require_drained(second_pack_counts);

	protocol_counts nested_counts;
	protocol_counts nested_inactive_counts;
	auto nested{::fast_io::mnp::pack(
		::std::string_view{"["},
		::fast_io::mnp::internal(
			::fast_io::mnp::cond(
				true,
				semantic_source{__builtin_addressof(nested_counts), '-', 'N'},
				semantic_source{__builtin_addressof(nested_inactive_counts), '-', 'X'}),
			5u, '.'),
		::std::string_view{"]"})};
	assert(::fast_io::concat_std(nested) == "[-...N]");
	require_drained(nested_counts);
	assert(nested_counts.internal_shift_calls != 0u);
	assert(nested_inactive_counts.forward_calls == 0u && nested_inactive_counts.live_owners == 0u);

	protocol_counts nested_print_counts;
	protocol_counts nested_print_inactive_counts;
	auto nested_print{::fast_io::mnp::pack(
		::std::string_view{"<"},
		::fast_io::mnp::cond(
			true,
			::fast_io::mnp::pack(
				semantic_source{__builtin_addressof(nested_print_counts), '+', 'Q'},
				::std::string_view{"!"}),
			::fast_io::mnp::pack(
				semantic_source{__builtin_addressof(nested_print_inactive_counts), '-', 'X'},
				::std::string_view{"?"})),
		::std::string_view{">"})};
	assert(render_print(nested_print) == "<+Q!>");
	require_drained(nested_print_counts);
	assert(nested_print_inactive_counts.forward_calls == 0u &&
		   nested_print_inactive_counts.live_owners == 0u);
}

} // namespace semantic_owned_leaf_forwarding

int main()
{
	::semantic_owned_leaf_forwarding::test_one_named_owner_directly();
	::semantic_owned_leaf_forwarding::test_top_level_concat_and_print();
	::semantic_owned_leaf_forwarding::test_prepared_and_internal_width();
	::semantic_owned_leaf_forwarding::test_condition_pack_and_nested_composition();
}
