#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct alias_proxy
{
	char value{};

	inline explicit constexpr alias_proxy(char ch) noexcept : value(ch) {}
	alias_proxy(alias_proxy const &) = delete;
	alias_proxy &operator=(alias_proxy const &) = delete;
	alias_proxy(alias_proxy &&) = delete;
	alias_proxy &operator=(alias_proxy &&) = delete;
};

struct mutable_alias_source
{
	alias_proxy proxy;
};

// This lvalue-only customization is the cv/ref regression case. A width factory must probe the expression it actually
// receives and retain this exact noncopyable mutable reference; probing `mutable_alias_source` as an invented rvalue or
// adding const would either reject the valid protocol or make the reserve customization unusable.
inline constexpr alias_proxy &print_alias_define(
	::fast_io::io_alias_t, mutable_alias_source &source) noexcept
{
	return source.proxy;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, alias_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, alias_proxy>, char *iter, alias_proxy &proxy) noexcept
{
	*iter = proxy.value;
	return iter + 1;
}

struct copied_alias_proxy
{
	char value{};
};

struct temporary_alias_source
{
	copied_alias_proxy proxy;
};

// The returned reference designates a subobject of an rvalue source. It is valid only while the width factory is still
// evaluating that source, so the factory must copy the proxy into its node before the full-expression ends.
inline constexpr copied_alias_proxy &print_alias_define(
	::fast_io::io_alias_t, temporary_alias_source &&source) noexcept
{
	return source.proxy;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, copied_alias_proxy>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, copied_alias_proxy>, char *iter,
	copied_alias_proxy const &proxy) noexcept
{
	*iter = proxy.value;
	return iter + 1;
}

struct noncopyable_temporary_alias_source
{
	alias_proxy proxy;
};

inline constexpr alias_proxy &print_alias_define(
	::fast_io::io_alias_t, noncopyable_temporary_alias_source &&source) noexcept
{
	return source.proxy;
}

struct large_mutable_leaf
{
	char value{};
	char make_larger_than_the_value_transport[64]{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, large_mutable_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, large_mutable_leaf>, char *iter,
	large_mutable_leaf &leaf) noexcept
{
	*iter = leaf.value;
	return iter + 1;
}

struct small_trivial_leaf
{
	char value{};
};

[[maybe_unused]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, small_trivial_leaf>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, small_trivial_leaf>, char *iter,
	small_trivial_leaf leaf) noexcept
{
	*iter = leaf.value;
	return iter + 1;
}

struct move_only_leaf
{
	char value{};

	inline explicit constexpr move_only_leaf(char ch) noexcept : value(ch) {}
	move_only_leaf(move_only_leaf const &) = delete;
	move_only_leaf &operator=(move_only_leaf const &) = delete;
	inline constexpr move_only_leaf(move_only_leaf &&) noexcept = default;
	inline constexpr move_only_leaf &operator=(move_only_leaf &&) noexcept = default;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, move_only_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, move_only_leaf>, char *iter,
	move_only_leaf const &leaf) noexcept
{
	*iter = leaf.value;
	return iter + 1;
}

struct semantic_mutex_state
{
	::std::string output;
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	bool locked{};
};

struct semantic_mutex_ref
{
	semantic_mutex_state *state;

	inline void lock() const noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

struct unlocked_semantic_sink
{
	using output_char_type = char;
	semantic_mutex_state *state;
};

inline void write_all_overflow_define(
	unlocked_semantic_sink sink, char const *first, char const *last)
{
	assert(sink.state->locked);
	sink.state->output.append(first, last);
}

struct locked_semantic_sink
{
	using output_char_type = char;
	semantic_mutex_state *state;
};

inline constexpr locked_semantic_sink
output_stream_ref_define(locked_semantic_sink sink) noexcept
{
	return sink;
}

inline constexpr semantic_mutex_ref
output_stream_mutex_ref_define(locked_semantic_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr unlocked_semantic_sink
output_stream_unlocked_ref_define(locked_semantic_sink sink) noexcept
{
	return {sink.state};
}

struct throwing_alias_source
{
	alias_proxy proxy;
};

inline alias_proxy &print_alias_define(::fast_io::io_alias_t, throwing_alias_source &)
{
	throw 37;
}

template <typename T>
concept can_make_right_width = requires(T &&value) {
	::fast_io::mnp::right(::std::forward<T>(value), 3u, '.');
};

using borrowed_alias_width = decltype(
	::fast_io::mnp::right(::std::declval<mutable_alias_source &>(), 3u));
using borrowed_alias_width_with_fill = decltype(
	::fast_io::mnp::left(::std::declval<mutable_alias_source &>(), 3u, '.'));
using borrowed_alias_middle_width_with_fill = decltype(
	::fast_io::mnp::middle(::std::declval<mutable_alias_source &>(), 3u, '.'));
using borrowed_alias_internal_width_with_fill = decltype(
	::fast_io::mnp::internal(::std::declval<mutable_alias_source &>(), 3u, '.'));
using runtime_borrowed_alias_width = decltype(::fast_io::mnp::width(
	::fast_io::mnp::scalar_placement::middle, ::std::declval<mutable_alias_source &>(), 3u));
using runtime_borrowed_alias_width_with_fill = decltype(::fast_io::mnp::width(
	::fast_io::mnp::scalar_placement::internal, ::std::declval<mutable_alias_source &>(), 3u, '.'));
using owned_temporary_alias_width = decltype(
	::fast_io::mnp::right(::std::declval<temporary_alias_source>(), 3u, '.'));
using borrowed_large_width = decltype(
	::fast_io::mnp::middle(::std::declval<large_mutable_leaf &>(), 3u));
using borrowed_const_large_width = decltype(
	::fast_io::mnp::internal(::std::declval<large_mutable_leaf const &>(), 3u));
using copied_small_width = decltype(
	::fast_io::mnp::left(::std::declval<small_trivial_leaf &>(), 3u));
using owned_move_only_width = decltype(
	::fast_io::mnp::left(::std::declval<move_only_leaf>(), 3u, '.'));

static_assert(::std::same_as<decltype(::std::declval<borrowed_alias_width &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<borrowed_alias_width_with_fill &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<borrowed_alias_middle_width_with_fill &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<borrowed_alias_internal_width_with_fill &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<runtime_borrowed_alias_width &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<runtime_borrowed_alias_width_with_fill &>().reference), alias_proxy &>);
static_assert(::std::same_as<
	decltype(::std::declval<owned_temporary_alias_width &>().reference), copied_alias_proxy>);
static_assert(::std::same_as<
	decltype(::std::declval<borrowed_large_width &>().reference), large_mutable_leaf &>);
static_assert(::std::same_as<
	decltype(::std::declval<borrowed_const_large_width &>().reference), large_mutable_leaf const &>);
static_assert(::std::same_as<
	decltype(::std::declval<copied_small_width &>().reference), small_trivial_leaf>);
static_assert(::std::same_as<
	decltype(::std::declval<owned_move_only_width &>().reference), move_only_leaf>);

// Owning a borrowed subobject is mandatory for an rvalue source. When that subobject cannot be copied or moved, cleanly
// rejecting the factory is safer than constructing a node whose reference becomes invalid at the semicolon.
static_assert(!::fast_io::details::width_storable<noncopyable_temporary_alias_source>);
static_assert(!can_make_right_width<noncopyable_temporary_alias_source>);

static_assert(noexcept(::fast_io::mnp::right(
	::std::declval<mutable_alias_source &>(), 3u, '.')));
static_assert(!noexcept(::fast_io::mnp::right(
	::std::declval<throwing_alias_source &>(), 3u, '.')));
static_assert(!noexcept(::fast_io::mnp::width(
	::fast_io::mnp::scalar_placement::left, ::std::declval<throwing_alias_source &>(), 3u)));

} // namespace

int main()
{
	mutable_alias_source alias_source{alias_proxy{'A'}};
	auto borrowed_alias{::fast_io::mnp::right(alias_source, 3u, '.')};
	assert(__builtin_addressof(borrowed_alias.reference) == __builtin_addressof(alias_source.proxy));
	alias_source.proxy.value = 'B';
	assert(::fast_io::concat_std(borrowed_alias) == "..B");

	large_mutable_leaf large_leaf{'L'};
	auto borrowed_large{::fast_io::mnp::left(large_leaf, 3u, '.')};
	assert(__builtin_addressof(borrowed_large.reference) == __builtin_addressof(large_leaf));
	large_leaf.value = 'M';
	assert(::fast_io::concat_std(borrowed_large) == "M..");

	// Both sources below are destroyed before concat is called. The stored child types above prove that the width nodes
	// own their normalized values; these assertions exercise the same lifetime boundary through the semantic dispatcher.
	auto owned_alias{::fast_io::mnp::right(temporary_alias_source{{'T'}}, 3u, '.')};
	assert(::fast_io::concat_std(owned_alias) == "..T");
	auto owned_move_only{::fast_io::mnp::left(move_only_leaf{'R'}, 3u, '.')};
	assert(::fast_io::concat_std(owned_move_only) == "R..");

	// Temporary semantic nodes exercise the stricter ownership case. Concat moves the normalized value into phase 1
	// exactly once; sizing, destination selection, and emission must borrow that owner rather than trying to copy the
	// named move-only width or an element exposed while flattening a move-only pack.
	assert(::fast_io::concat_std(
		::fast_io::mnp::left(move_only_leaf{'V'}, 3u, '.')) == "V..");
	assert(::fast_io::concat_std(
		::fast_io::mnp::pack(move_only_leaf{'P'})) == "P");
	assert(::fast_io::concat_std(move_only_leaf{'D'}) == "D");

	// The same one-owner rule applies to a non-semantic formatter. The no-pack/control dispatcher receives a reference
	// to the normalized owner; neither ordinary output nor mutex recursion may require the formatter to be copyable.
	semantic_mutex_state direct_state;
	::fast_io::print(
		locked_semantic_sink{__builtin_addressof(direct_state)}, move_only_leaf{'N'});
	assert(direct_state.output == "N");
	assert(direct_state.lock_calls == 1u && direct_state.unlock_calls == 1u && !direct_state.locked);

	semantic_mutex_state direct_cold_state;
	::fast_io::operations::decay::print_freestanding_decay_cold<false>(
		locked_semantic_sink{__builtin_addressof(direct_cold_state)}, move_only_leaf{'K'});
	assert(direct_cold_state.output == "K");
	assert(direct_cold_state.lock_calls == 1u && direct_cold_state.unlock_calls == 1u &&
		   !direct_cold_state.locked);

	// The owning decay entry materializes this move-only semantic node exactly once. Mutex recursion and the cold
	// wrapper must retain a reference to that same graph; either boundary taking the named node by value fails to compile.
	semantic_mutex_state mutex_state;
	::fast_io::print(
		locked_semantic_sink{__builtin_addressof(mutex_state)},
		::fast_io::mnp::left(move_only_leaf{'Q'}, 3u, '.'));
	assert(mutex_state.output == "Q..");
	assert(mutex_state.lock_calls == 1u && mutex_state.unlock_calls == 1u && !mutex_state.locked);

	semantic_mutex_state cold_state;
	::fast_io::operations::decay::print_freestanding_decay_cold<false>(
		locked_semantic_sink{__builtin_addressof(cold_state)},
		::fast_io::mnp::left(move_only_leaf{'C'}, 3u, '.'));
	assert(cold_state.output == "C..");
	assert(cold_state.lock_calls == 1u && cold_state.unlock_calls == 1u && !cold_state.locked);

	bool propagated{};
	throwing_alias_source throwing_source{alias_proxy{'X'}};
	try
	{
		(void)::fast_io::mnp::right(throwing_source, 3u, '.');
	}
	catch (int value)
	{
		propagated = value == 37;
	}
	assert(propagated);
}
