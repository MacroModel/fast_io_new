#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct capture_state
{
	::std::size_t nonline_status_calls{};
	::std::size_t line_status_calls{};
	::std::size_t explicit_null_status_calls{};
	::std::size_t scalar_write_calls{};
	::std::size_t scatter_write_calls{};
	::std::size_t bytes{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	bool locked{};
	bool newline_only{true};
};

inline void observe_range(
	capture_state &state, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		state.newline_only &= *first == '\n';
		++state.bytes;
	}
}

struct scalar_status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct scatter_status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct plain_scalar_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct active_record_status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct unlocked_status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct locked_status_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct silent_unlocked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct silent_locked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct source_graph_unlocked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct source_graph_locked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct mutex_ref
{
	capture_state *state{};

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

inline constexpr scalar_status_sink
output_stream_ref_define(scalar_status_sink sink) noexcept
{
	return sink;
}

inline constexpr scatter_status_sink
output_stream_ref_define(scatter_status_sink sink) noexcept
{
	return sink;
}

inline constexpr plain_scalar_sink
output_stream_ref_define(plain_scalar_sink sink) noexcept
{
	return sink;
}

inline constexpr active_record_status_sink
output_stream_ref_define(active_record_status_sink sink) noexcept
{
	return sink;
}

inline constexpr locked_status_sink
output_stream_ref_define(locked_status_sink sink) noexcept
{
	return sink;
}

inline constexpr silent_locked_sink
output_stream_ref_define(silent_locked_sink sink) noexcept
{
	return sink;
}

inline constexpr source_graph_locked_sink
output_stream_ref_define(source_graph_locked_sink sink) noexcept
{
	return sink;
}

inline constexpr mutex_ref
output_stream_mutex_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr mutex_ref
output_stream_mutex_ref_define(silent_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr mutex_ref
output_stream_mutex_ref_define(source_graph_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr unlocked_status_sink
output_stream_unlocked_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr silent_unlocked_sink
output_stream_unlocked_ref_define(silent_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr source_graph_unlocked_sink
output_stream_unlocked_ref_define(source_graph_locked_sink sink) noexcept
{
	return {sink.state};
}

template <bool line, typename sink_type>
	requires(::std::same_as<sink_type, scalar_status_sink> ||
			 ::std::same_as<sink_type, scatter_status_sink> ||
			 ::std::same_as<sink_type, unlocked_status_sink>)
inline void status_print_define(sink_type sink) noexcept
{
	if constexpr (::std::same_as<sink_type, unlocked_status_sink>)
	{
		assert(sink.state->locked);
	}
	if constexpr (line)
	{
		++sink.state->line_status_calls;
	}
	else
	{
		++sink.state->nonline_status_calls;
	}
}

template <bool line>
inline void status_print_define(
	scalar_status_sink sink, ::fast_io::io_null_t &) noexcept
{
	++sink.state->explicit_null_status_calls;
}

using static_integer =
	::std::remove_cvref_t<decltype(::fast_io::mnp::static_arg<42>)>;
using normalized_static_integer = ::std::remove_cvref_t<decltype(::fast_io::details::decay::print_semantic_input_forward<char>(
	::std::declval<static_integer &>()))>;

struct forwarding_inactive_branch
{};

using forwarding_replaced_condition =
	::fast_io::manipulators::condition<
		::fast_io::io_null_t, forwarding_inactive_branch>;

struct forwarding_replacement_leaf
{
	char value{'F'};
};

inline constexpr forwarding_replacement_leaf status_io_print_forward(
	::fast_io::io_alias_type_t<char>,
	forwarding_replaced_condition &) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, forwarding_replacement_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, forwarding_replacement_leaf>,
	char *iter, forwarding_replacement_leaf const &leaf) noexcept
{
	*iter++ = leaf.value;
	return iter;
}

using normalized_forwarding_replacement =
	::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<
		char, forwarding_replaced_condition>;

// A raw pack member crosses alias/status forwarding before condition
// selection. This exact type proof makes the regression sensitive to that
// phase boundary instead of merely observing a later write count.
static_assert(::std::same_as<
			  normalized_forwarding_replacement,
			  forwarding_replacement_leaf>);

template <bool line>
inline void status_print_define(
	active_record_status_sink sink, normalized_static_integer) noexcept
{
	if constexpr (line)
	{
		++sink.state->line_status_calls;
	}
	else
	{
		++sink.state->nonline_status_calls;
	}
}

template <bool line, typename record_type>
	requires ::fast_io::details::print_pack<record_type>
inline void status_print_define(
	source_graph_unlocked_sink sink, record_type &) noexcept
{
	assert(sink.state->locked);
	if constexpr (line)
	{
		++sink.state->line_status_calls;
	}
	else
	{
		++sink.state->nonline_status_calls;
	}
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, scalar_status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, plain_scalar_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, active_record_status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char, unlocked_status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, scatter_status_sink>) noexcept
{
	return {};
}

template <typename sink_type>
	requires(::std::same_as<sink_type, scalar_status_sink> ||
			 ::std::same_as<sink_type, plain_scalar_sink> ||
			 ::std::same_as<sink_type, active_record_status_sink> ||
			 ::std::same_as<sink_type, unlocked_status_sink> ||
			 ::std::same_as<sink_type, silent_unlocked_sink>)
inline void write_all_overflow_define(
	sink_type sink, char const *first, char const *last) noexcept
{
	if constexpr (
		::std::same_as<sink_type, unlocked_status_sink> ||
		::std::same_as<sink_type, silent_unlocked_sink>)
	{
		assert(sink.state->locked);
	}
	++sink.state->scalar_write_calls;
	observe_range(*sink.state, first, last);
}

[[maybe_unused]] inline void scatter_write_all_overflow_define(
	scatter_status_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++sink.state->scatter_write_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const &scatter{scatters[index]};
		observe_range(*sink.state, scatter.base, scatter.base + scatter.len);
	}
}

template <typename sink_type, typename record_type>
inline void check_status_owner(sink_type sink, record_type &empty_record)
{
	::fast_io::io::print(sink, empty_record);
	::fast_io::io::println(sink, empty_record);
	::fast_io::io::perr(sink, empty_record);
	::fast_io::io::perrln(sink, empty_record);
	::fast_io::io::debug_print(sink, empty_record);
	::fast_io::io::debug_println(sink, empty_record);
	::fast_io::io::debug_perr(sink, empty_record);
	::fast_io::io::debug_perrln(sink, empty_record);

	assert(sink.state->nonline_status_calls == 4u);
	assert(sink.state->line_status_calls == 4u);
	assert(sink.state->scalar_write_calls == 0u);
	assert(sink.state->scatter_write_calls == 0u);
	assert(sink.state->bytes == 0u);
}

using empty_pack = decltype(::fast_io::mnp::pack());

static_assert(
	::fast_io::details::decay::print_semantic_input_argument_v<
		char, empty_pack &>);
static_assert(
	::fast_io::operations::decay::
		print_pre_normalization_semantic_source_run_v<
			active_record_status_sink, static_integer const &, empty_pack &>);
static_assert(!::fast_io::operations::decay::
				  print_compiler_constant_pre_normalization_available<
					  false, active_record_status_sink,
					  static_integer const &, empty_pack &>());

// A top-level semantic node cannot prove the active normalized record before
// expansion. In particular, pack<> has one source node but zero active leaves,
// so both the shared compiler-constant proof and the fragment shortcut must
// fail closed and let the exact zero-argument status owner run after flattening.
static_assert(!::fast_io::operations::decay::
				  print_compiler_constant_pre_normalization_fragment_run_available<
					  false, scalar_status_sink, empty_pack &>());
static_assert(!::fast_io::operations::decay::
				  print_compiler_constant_pre_normalization_fragment_run_available<
					  true, scalar_status_sink, empty_pack &>());
static_assert(!::fast_io::operations::decay::
				  print_compiler_constant_pre_normalization_fragment_run_available<
					  true, scatter_status_sink, empty_pack &>());

} // namespace

int main()
{
	auto empty_record{::fast_io::mnp::pack()};
	auto null_record{::fast_io::mnp::pack(::fast_io::io_null)};

	capture_state scalar_status_state;
	check_status_owner(
		scalar_status_sink{__builtin_addressof(scalar_status_state)},
		empty_record);

	capture_state scatter_status_state;
	check_status_owner(
		scatter_status_sink{__builtin_addressof(scatter_status_state)},
		empty_record);

	capture_state locked_status_state;
	check_status_owner(
		locked_status_sink{__builtin_addressof(locked_status_state)},
		empty_record);
	assert(locked_status_state.lock_calls == 8u);
	assert(locked_status_state.unlock_calls == 8u);
	assert(!locked_status_state.locked);

	// Recursive pack expansion must preserve the same zero-component record;
	// no intermediate semantic node may become an observable output operation.
	auto nested_empty_record{::fast_io::mnp::pack(::fast_io::mnp::pack())};
	capture_state nested_status_state;
	check_status_owner(
		scalar_status_sink{__builtin_addressof(nested_status_state)},
		nested_empty_record);

	// A null selected inside semantic structure contributes no active leaf and
	// therefore reaches the zero-argument record owner. A standalone null keeps
	// its explicit argument identity and may select status<io_null_t> instead.
	capture_state packed_null_state;
	check_status_owner(
		scalar_status_sink{__builtin_addressof(packed_null_state)},
		null_record);
	assert(packed_null_state.explicit_null_status_calls == 0u);
	capture_state standalone_null_state;
	scalar_status_sink standalone_null_sink{
		__builtin_addressof(standalone_null_state)};
	::fast_io::io::print(standalone_null_sink, ::fast_io::io_null);
	assert(standalone_null_state.explicit_null_status_calls == 1u);
	assert(standalone_null_state.nonline_status_calls == 0u);

	// Condition selection is another active-record boundary: an inactive
	// one-branch condition and a selected empty pack both normalize to zero
	// leaves, so neither source-level arity may bypass the zero-argument status
	// owner or manufacture a primitive output operation.
	auto inactive_condition{::fast_io::mnp::cond(false, "not emitted")};
	capture_state inactive_condition_state;
	check_status_owner(
		scalar_status_sink{__builtin_addressof(inactive_condition_state)},
		inactive_condition);
	auto selected_empty_pack{
		::fast_io::mnp::cond(true, ::fast_io::mnp::pack())};
	capture_state selected_empty_pack_state;
	check_status_owner(
		scalar_status_sink{__builtin_addressof(selected_empty_pack_state)},
		selected_empty_pack);

	// The source arity is nonzero at this boundary, but the selected active
	// record is empty. A silent destination must therefore be ignored before
	// its mutex is acquired for every facade that enters the print level.
	capture_state silent_locked_state;
	silent_locked_sink silent_locked{
		__builtin_addressof(silent_locked_state)};
	::fast_io::io::print(silent_locked, empty_record);
	::fast_io::io::perr(silent_locked, nested_empty_record);
	::fast_io::io::debug_print(silent_locked, inactive_condition);
	::fast_io::io::debug_perr(silent_locked, selected_empty_pack);
	::fast_io::io::print(silent_locked, null_record);
	assert(silent_locked_state.lock_calls == 0u);
	assert(silent_locked_state.unlock_calls == 0u);
	assert(silent_locked_state.scalar_write_calls == 0u);
	assert(silent_locked_state.bytes == 0u);

	// An apparently empty condition stored in a pack is not structural evidence
	// before raw member forwarding. ADL replaces the complete condition with a
	// nonempty leaf, so the ordinary path emits one byte and the mutex path must
	// perform the same replacement while holding its lock. A lock-free empty
	// precheck here would erase both the provider call and the output.
	forwarding_replaced_condition forwarding_condition{
		true, ::fast_io::io_null, forwarding_inactive_branch{}};
	auto forwarding_record{::fast_io::mnp::pack(forwarding_condition)};
	assert(!::fast_io::details::decay::print_semantic_run_provably_empty(
		forwarding_record));
	capture_state forwarding_plain_state;
	::fast_io::io::print(
		plain_scalar_sink{__builtin_addressof(forwarding_plain_state)},
		forwarding_record);
	assert(forwarding_plain_state.scalar_write_calls == 1u);
	assert(forwarding_plain_state.bytes == 1u);
	capture_state forwarding_locked_state;
	::fast_io::io::print(
		silent_locked_sink{__builtin_addressof(forwarding_locked_state)},
		forwarding_record);
	assert(forwarding_locked_state.lock_calls == 1u);
	assert(forwarding_locked_state.unlock_calls == 1u);
	assert(forwarding_locked_state.scalar_write_calls == 1u);
	assert(forwarding_locked_state.bytes == 1u);
	assert(!forwarding_locked_state.locked);

	// Line ownership makes the otherwise empty active record observable as one
	// newline, so the same destination must retain its synchronization boundary.
	::fast_io::io::println(silent_locked, empty_record);
	assert(silent_locked_state.lock_calls == 1u);
	assert(silent_locked_state.unlock_calls == 1u);
	assert(silent_locked_state.scalar_write_calls == 1u);
	assert(silent_locked_state.bytes == 1u);
	assert(silent_locked_state.newline_only);
	assert(!silent_locked_state.locked);

	// A provider may own the exact pre-expansion record. That operation has
	// precedence over the derived zero-argument record and still executes under
	// the effective output's mutex.
	capture_state source_graph_state;
	source_graph_locked_sink source_graph{
		__builtin_addressof(source_graph_state)};
	::fast_io::io::print(source_graph, empty_record);
	::fast_io::io::println(source_graph, empty_record);
	assert(source_graph_state.lock_calls == 2u);
	assert(source_graph_state.unlock_calls == 2u);
	assert(source_graph_state.nonline_status_calls == 1u);
	assert(source_graph_state.line_status_calls == 1u);
	assert(source_graph_state.scalar_write_calls == 0u);
	assert(!source_graph_state.locked);

	capture_state plain_state;
	plain_scalar_sink plain{__builtin_addressof(plain_state)};
	::fast_io::io::print(plain, empty_record);
	::fast_io::io::println(plain, empty_record);
	::fast_io::io::perr(plain, empty_record);
	::fast_io::io::perrln(plain, empty_record);
	::fast_io::io::debug_print(plain, empty_record);
	::fast_io::io::debug_println(plain, empty_record);
	::fast_io::io::debug_perr(plain, empty_record);
	::fast_io::io::debug_perrln(plain, empty_record);
	assert(plain_state.scalar_write_calls == 4u);
	assert(plain_state.scatter_write_calls == 0u);
	assert(plain_state.bytes == 4u);
	assert(plain_state.newline_only);

	// The empty semantic suffix also changes a larger source graph: after
	// flattening, the active record contains only static_arg<42>. A shortcut
	// proved against the pre-expansion pair must not bypass that exact status
	// operation merely because the first source has an immutable spelling.
	capture_state active_record_state;
	active_record_status_sink active{
		__builtin_addressof(active_record_state)};
	::fast_io::io::print(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::println(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::perr(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::perrln(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::debug_print(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::debug_println(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::debug_perr(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	::fast_io::io::debug_perrln(
		active, ::fast_io::mnp::static_arg<42>, empty_record);
	assert(active_record_state.nonline_status_calls == 4u);
	assert(active_record_state.line_status_calls == 4u);
	assert(active_record_state.scalar_write_calls == 0u);
	assert(active_record_state.bytes == 0u);
}
