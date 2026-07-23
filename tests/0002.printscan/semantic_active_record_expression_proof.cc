#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace semantic_active_record_expression_proof
{

template <typename output, typename... Args>
inline constexpr bool nonline_print_okay =
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, output, Args...>;

struct capture_state
{
	char bytes[8]{};
	::std::size_t size{};
};

struct writable_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr writable_sink output_stream_ref_define(
	writable_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(
	writable_sink sink, char const *first, char const *last) noexcept
{
	for (; first != last; ++first)
	{
		assert(sink.state->size != sizeof(sink.state->bytes));
		sink.state->bytes[sink.state->size++] = *first;
	}
}

struct mutable_only_large_leaf
{
	char value{};
	char force_reference_transport[128]{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, mutable_only_large_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, mutable_only_large_leaf>, char *iter,
	mutable_only_large_leaf &leaf) noexcept
{
	*iter = leaf.value;
	return iter + 1;
}

using mutable_only_pack = decltype(::fast_io::mnp::pack(::std::declval<mutable_only_large_leaf>()));
using normalized_const_pack = decltype(::fast_io::io_print_forward<char>(
	::fast_io::io_print_alias(
		::std::declval<mutable_only_pack const &>())));

// A large const pack crosses public normalization as an exact const reference.
// Its named element is therefore const at expansion time; a mutable-only leaf
// formatter is not a proof for that executable expression.
static_assert(::std::same_as<
			  normalized_const_pack,
			  ::fast_io::parameter<mutable_only_pack const &>>);
static_assert(!nonline_print_okay<
			  writable_sink &, mutable_only_pack const &>);
static_assert(nonline_print_okay<
			  writable_sink &, mutable_only_pack &>);

struct phase_source
{};

struct phase_runtime_leaf
{};

struct phase_direct_replacement
{};

using phase_alias_pack = decltype(::fast_io::mnp::pack(phase_runtime_leaf{}));

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, phase_direct_replacement>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, phase_direct_replacement>, char *iter,
	phase_direct_replacement) noexcept
{
	*iter = 'D';
	return iter + 1;
}

inline constexpr ::std::size_t print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<char, phase_direct_replacement>) noexcept
{
	return 1u;
}

inline constexpr phase_alias_pack print_alias_define(
	::fast_io::io_alias_t, phase_source &) noexcept
{
	return ::fast_io::mnp::pack(phase_runtime_leaf{});
}

inline constexpr phase_direct_replacement status_io_print_forward(
	::fast_io::io_alias_type_t<char>, phase_alias_pack &&) noexcept
{
	return {};
}

using phase_condition =
	::fast_io::manipulators::condition<phase_source, phase_source>;
using phase_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left, phase_source>;
using phase_raw_pack = ::fast_io::manipulators::pack_t<phase_source>;
using phase_direct_condition = ::fast_io::manipulators::condition<
	phase_direct_replacement, phase_direct_replacement>;
using phase_direct_width = ::fast_io::manipulators::width_t<
	::fast_io::manipulators::scalar_placement::left,
	phase_direct_replacement>;
using phase_named_direct_forward =
	::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<
		char, phase_source>;
using phase_named_input_forward =
	::fast_io::details::decay::
		print_semantic_named_member_input_forwarded_arg_t<char, phase_source>;

struct phase_replacement_only_sink
{
	using output_char_type = char;
};

inline constexpr phase_replacement_only_sink output_stream_ref_define(
	phase_replacement_only_sink sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(
	phase_replacement_only_sink, char const *, char const *) noexcept
{}

template <bool line>
inline void status_print_define(
	phase_replacement_only_sink, phase_direct_replacement &) noexcept
{}

struct phase_runtime_sink
{
	using output_char_type = char;
	::std::size_t *calls{};
};

inline constexpr phase_runtime_sink output_stream_ref_define(
	phase_runtime_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(
	phase_runtime_sink sink, phase_runtime_leaf &) noexcept
{
	++*sink.calls;
}

inline void print_define(
	::fast_io::io_reserve_type_t<char, phase_runtime_leaf>,
	phase_runtime_sink sink, phase_runtime_leaf &) noexcept
{
	++*sink.calls;
}

// Direct forwarding of the alias pack selects the replacement CPO. Semantic
// input forwarding deliberately transports an alias that is already a semantic
// node and must bypass that CPO. A condition-arm proof must model the latter.
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_forward<char>(
				  ::std::declval<phase_alias_pack>())),
			  phase_direct_replacement>);
static_assert(::std::same_as<
			  decltype(::fast_io::details::decay::print_semantic_input_forward<char>(
				  ::std::declval<phase_source &>())),
			  phase_alias_pack>);
static_assert(::std::same_as<
			  phase_named_direct_forward, phase_direct_replacement>);
static_assert(::std::same_as<
			  phase_named_input_forward, phase_alias_pack>);
static_assert(::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_named_direct_forward>::available);
static_assert(!::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_named_input_forward>::available);
static_assert(::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_named_direct_forward>::available);
static_assert(!::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_named_input_forward>::available);

// Condition arms and width children execute semantic-input forwarding. The
// transported alias pack contains an unsupported leaf, so every structural,
// static-size, and replay-safety trait must reject it even though direct status
// forwarding would produce a statically precise printable replacement.
static_assert(!::fast_io::details::decay::print_semantic_params_okay<
			  char, phase_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_condition>::available);
static_assert(!::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_condition>::available);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, phase_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, phase_condition>::value);
static_assert(!::fast_io::details::decay::print_semantic_params_okay<
			  char, phase_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_width>::available);
static_assert(!::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_width>::available);
static_assert(!::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, phase_width>::value);
static_assert(!::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, phase_width>::value);

// Replacing the children themselves with the direct-forward result makes each
// applicable trait positive. These counterfactuals ensure that the negative
// source-node assertions above test resolver choice rather than an unrelated
// absence of formatting protocols.
static_assert(::fast_io::details::decay::print_semantic_params_okay<
			  char, phase_direct_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_direct_condition>::available);
static_assert(::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_direct_condition>::available);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, phase_direct_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, phase_direct_condition>::value);
static_assert(::fast_io::details::decay::print_semantic_params_okay<
			  char, phase_direct_width>::value);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, phase_direct_width>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, phase_direct_width>::value);

// A raw pack member intentionally uses direct forwarding. Its replacement is
// therefore a valid one-byte static producer for all five trait families.
static_assert(::fast_io::details::decay::print_semantic_params_okay<
			  char, phase_raw_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_static_precise_size<
			  char, phase_raw_pack>::available);
static_assert(::fast_io::details::decay::print_semantic_static_precise_size<
				  char, phase_raw_pack>::size == 1u);
static_assert(::fast_io::details::decay::print_semantic_static_bounded_size<
			  char, phase_raw_pack>::available);
static_assert(::fast_io::details::decay::print_semantic_static_bounded_size<
				  char, phase_raw_pack>::size == 1u);
static_assert(::fast_io::details::decay::print_semantic_precise_size_ok<
			  char, phase_raw_pack>::value);
static_assert(::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char, phase_raw_pack>::value);

// Public admission makes one phase decision for the complete source run. The
// alias result is already a semantic pack, so execution bypasses its status
// forwarding replacement. All three public compatibility projections must
// therefore prove the runtime leaf rather than the directly printable decoy.
static_assert(!::fast_io::operations::defines::
				  print_freestanding_params_okay<char, phase_source &>);
static_assert(!::fast_io::operations::defines::print_freestanding_okay<
			  phase_replacement_only_sink &, phase_source &>);
static_assert(::fast_io::operations::defines::print_freestanding_okay<
			  phase_runtime_sink &, phase_source &>);
static_assert(!nonline_print_okay<
			  phase_replacement_only_sink &, phase_source &>);
static_assert(nonline_print_okay<
			  phase_runtime_sink &, phase_source &>);
static_assert(!nonline_print_okay<
			  phase_replacement_only_sink &, phase_condition &>);
static_assert(nonline_print_okay<
			  phase_runtime_sink &, phase_condition &>);

struct wrapped_null_source
{
	::fast_io::io_null_t *value{};
};

using wrapped_null = ::fast_io::parameter<::fast_io::io_null_t &>;

inline constexpr wrapped_null status_io_print_forward(
	::fast_io::io_alias_type_t<char>, wrapped_null_source &source) noexcept
{
	return {*source.value};
}

using empty_pack = decltype(::fast_io::mnp::pack());

struct unobservable_sink
{
	using output_char_type = char;
};

inline constexpr unobservable_sink output_stream_ref_define(
	unobservable_sink sink) noexcept
{
	return sink;
}

struct wrapped_null_status_sink
{
	using output_char_type = char;
	::std::size_t *calls{};
};

inline constexpr wrapped_null_status_sink output_stream_ref_define(
	wrapped_null_status_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(
	wrapped_null_status_sink sink, wrapped_null &) noexcept
{
	++*sink.calls;
}

// Runtime null filtering recognizes only a raw io_null expression. Recursively
// unwrapping parameter<io_null_t&> in the type proof would erase an active
// argument which the no-pack dispatcher still has to consume.
static_assert(::std::same_as<
			  decltype(::fast_io::io_print_forward<char>(
				  ::fast_io::io_print_alias(
					  ::std::declval<wrapped_null_source &>()))),
			  wrapped_null>);
static_assert(!nonline_print_okay<
			  unobservable_sink &, wrapped_null_source &, empty_pack &>);
static_assert(nonline_print_okay<
			  wrapped_null_status_sink &, wrapped_null_source &, empty_pack &>);

struct nested_leaf
{};

using nested_condition =
	::fast_io::manipulators::condition<nested_leaf, nested_leaf>;
using inner_condition_parameter =
	::fast_io::parameter<nested_condition &>;
using outer_condition_parameter =
	::fast_io::parameter<inner_condition_parameter &>;

struct nested_parameter_source
{
	outer_condition_parameter *value{};
};

inline constexpr outer_condition_parameter status_io_print_forward(
	::fast_io::io_alias_type_t<char>, nested_parameter_source &source) noexcept
{
	return *source.value;
}

struct nested_leaf_only_sink
{
	using output_char_type = char;
};

inline constexpr nested_leaf_only_sink output_stream_ref_define(
	nested_leaf_only_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(
	nested_leaf_only_sink, nested_leaf &) noexcept
{}

struct nested_wrapper_status_sink
{
	using output_char_type = char;
	::std::size_t *calls{};
};

inline constexpr nested_wrapper_status_sink output_stream_ref_define(
	nested_wrapper_status_sink sink) noexcept
{
	return sink;
}

template <bool line>
inline void status_print_define(
	nested_wrapper_status_sink sink, outer_condition_parameter &) noexcept
{
	++*sink.calls;
}

// Semantic-node recognition intentionally unwraps one parameter layer. The
// recursively unwrapping node_ref helper is an execution accessor, not evidence
// that a doubly wrapped condition enters top-level condition selection.
static_assert(!::fast_io::details::decay::print_semantic_node<
			  outer_condition_parameter>);
static_assert(!nonline_print_okay<
			  nested_leaf_only_sink &, nested_parameter_source &, empty_pack &>);
static_assert(nonline_print_okay<
			  nested_wrapper_status_sink &, nested_parameter_source &, empty_pack &>);

struct recursively_wrapped_pack_leaf
{
	char value{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, recursively_wrapped_pack_leaf>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, recursively_wrapped_pack_leaf>,
	char *iter, recursively_wrapped_pack_leaf &leaf) noexcept
{
	*iter = leaf.value;
	return iter + 1;
}

using recursively_wrapped_pack = decltype(::fast_io::mnp::pack(recursively_wrapped_pack_leaf{}));
using inner_pack_parameter =
	::fast_io::parameter<recursively_wrapped_pack &>;
using outer_pack_parameter =
	::fast_io::parameter<inner_pack_parameter &>;

template <typename T>
concept no_pack_dispatch_callable = requires(writable_sink &sink, T &value) {
	::fast_io::operations::decay::print_freestanding_decay_no_pack<false>(
		sink, value);
};

struct recursively_wrapped_pack_source
{
	outer_pack_parameter *value{};
};

inline constexpr outer_pack_parameter &status_io_print_forward(
	::fast_io::io_alias_type_t<char>,
	recursively_wrapped_pack_source &source) noexcept
{
	return *source.value;
}

// Pack expansion deliberately supports recursively wrapped parameter objects.
// That exception must participate in every execution-routing predicate even
// though ordinary semantic-node admission unwraps only one parameter layer.
static_assert(!::fast_io::details::decay::print_semantic_node<
			  outer_pack_parameter &>);
static_assert(::fast_io::details::decay::print_semantic_pack_argument_v<
			  outer_pack_parameter &>);
static_assert(::fast_io::details::decay::print_semantic_execution_node_v<
			  outer_pack_parameter &>);
static_assert(!no_pack_dispatch_callable<outer_pack_parameter>);
static_assert(no_pack_dispatch_callable<recursively_wrapped_pack_leaf>);
static_assert(nonline_print_okay<
			  writable_sink &, recursively_wrapped_pack_source &>);

struct mutex_state
{
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	::std::size_t status_calls{};
	bool locked{};
};

struct mutable_status_tag
{};

struct const_status_tag
{};

struct wrapped_null_mutex_tag
{};

template <typename tag>
struct mutex_terminal
{
	using output_char_type = char;
	mutex_state *state{};
};

template <typename tag>
struct mutex_locked_sink
{
	using output_char_type = char;
	mutex_state *state{};
	mutex_terminal<tag> *terminal{};
};

struct mutex_proxy
{
	mutex_state *state{};

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

template <typename tag>
inline constexpr mutex_locked_sink<tag> output_stream_ref_define(
	mutex_locked_sink<tag> sink) noexcept
{
	return sink;
}

template <typename tag>
inline constexpr mutex_proxy output_stream_mutex_ref_define(
	mutex_locked_sink<tag> sink) noexcept
{
	return {sink.state};
}

template <typename tag>
inline constexpr mutex_terminal<tag> const &
output_stream_unlocked_ref_define(mutex_locked_sink<tag> sink) noexcept
{
	return *sink.terminal;
}

struct mutex_active_leaf
{};

using mutex_active_pack = decltype(::fast_io::mnp::pack(mutex_active_leaf{}));

template <bool line>
inline void status_print_define(
	mutex_terminal<mutable_status_tag> &sink,
	mutex_active_leaf &) noexcept
{
	assert(sink.state->locked);
	++sink.state->status_calls;
}

template <bool line>
	requires(!line)
inline void status_print_define(
	mutex_terminal<mutable_status_tag> &sink) noexcept
{
	assert(sink.state->locked);
	++sink.state->status_calls;
}

template <bool line>
inline void status_print_define(
	mutex_terminal<const_status_tag> const &sink,
	mutex_active_leaf &) noexcept
{
	assert(sink.state->locked);
	++sink.state->status_calls;
}

template <bool line>
	requires(!line)
inline void status_print_define(
	mutex_terminal<const_status_tag> const &sink) noexcept
{
	assert(sink.state->locked);
	++sink.state->status_calls;
}

template <bool line>
inline void status_print_define(
	mutex_terminal<wrapped_null_mutex_tag> const &sink,
	wrapped_null &) noexcept
{
	assert(!line);
	assert(sink.state->locked);
	++sink.state->status_calls;
}

using mutable_status_locked_sink = mutex_locked_sink<mutable_status_tag>;
using const_status_locked_sink = mutex_locked_sink<const_status_tag>;
using wrapped_null_locked_sink = mutex_locked_sink<wrapped_null_mutex_tag>;

struct public_mutable_terminal
{
	using output_char_type = char;
	::std::size_t *calls{};
};

struct public_const_terminal
{
	using output_char_type = char;
	::std::size_t *calls{};
};

template <typename terminal_type>
struct public_const_ref_owner
{
	terminal_type *terminal{};
};

template <typename terminal_type>
inline constexpr terminal_type const &output_stream_ref_define(
	public_const_ref_owner<terminal_type> &owner) noexcept
{
	return *owner.terminal;
}

template <bool line>
inline void status_print_define(
	public_mutable_terminal &sink, mutex_active_leaf &) noexcept
{
	++*sink.calls;
}

template <bool line>
	requires(!line)
inline void status_print_define(public_mutable_terminal &sink) noexcept
{
	++*sink.calls;
}

template <bool line>
inline void status_print_define(
	public_const_terminal const &sink, mutex_active_leaf &) noexcept
{
	++*sink.calls;
}

template <bool line>
	requires(!line)
inline void status_print_define(public_const_terminal const &sink) noexcept
{
	++*sink.calls;
}

using public_mutable_owner =
	public_const_ref_owner<public_mutable_terminal>;
using public_const_owner = public_const_ref_owner<public_const_terminal>;

// Mutex recursion binds a const-reference unlocked result with decltype(auto).
// Removing cv-qualification in admission invents a mutable terminal which the
// runtime call never owns.
static_assert(
	::fast_io::operations::decay::defines::
		has_complete_output_stream_mutex_protocol<mutable_status_locked_sink>);
static_assert(!nonline_print_okay<
			  mutable_status_locked_sink &, mutex_active_pack &>);
static_assert(nonline_print_okay<
			  const_status_locked_sink &, mutex_active_pack &>);
static_assert(
	!::fast_io::operations::decay::defines::
		empty_print_observable<mutable_status_locked_sink>);
static_assert(
	::fast_io::operations::decay::defines::
		empty_print_observable<const_status_locked_sink>);
static_assert(nonline_print_okay<
			  wrapped_null_locked_sink &, wrapped_null_source &, empty_pack &>);

// Public admission follows the result of output_stream_ref, not the provider's
// raw CPO spelling. Core intentionally materializes a const CPO lvalue into one
// mutable owned observer; both proof and execution therefore see that value.
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, public_mutable_owner &, mutex_active_leaf &>);
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, public_const_owner &, mutex_active_leaf &>);
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, public_mutable_owner &>);
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, public_const_owner &>);

} // namespace semantic_active_record_expression_proof

int main()
{
	using namespace semantic_active_record_expression_proof;

	capture_state capture;
	writable_sink output{__builtin_addressof(capture)};
	auto mutable_record{
		::fast_io::mnp::pack(mutable_only_large_leaf{'M'})};
	::fast_io::io::print(output, mutable_record);
	assert(capture.size == 1u && capture.bytes[0] == 'M');

	::std::size_t phase_calls{};
	phase_runtime_sink phase_output{__builtin_addressof(phase_calls)};
	phase_condition first_phase{true, {}, {}};
	phase_condition second_phase{false, {}, {}};
	::fast_io::io::print(phase_output, first_phase);
	::fast_io::io::print(phase_output, second_phase);
	phase_source direct_phase{};
	::fast_io::io::print(phase_output, direct_phase);
	assert(phase_calls == 3u);

	::fast_io::io_null_t null_value{};
	wrapped_null_source null_source{__builtin_addressof(null_value)};
	auto no_children{::fast_io::mnp::pack()};
	::std::size_t wrapped_null_calls{};
	wrapped_null_status_sink null_output{
		__builtin_addressof(wrapped_null_calls)};
	::fast_io::io::print(null_output, null_source, no_children);
	assert(wrapped_null_calls == 1u);

	nested_condition condition{true, {}, {}};
	inner_condition_parameter inner{condition};
	outer_condition_parameter outer{inner};
	nested_parameter_source nested_source{__builtin_addressof(outer)};
	::std::size_t nested_calls{};
	nested_wrapper_status_sink nested_output{
		__builtin_addressof(nested_calls)};
	::fast_io::io::print(nested_output, nested_source, no_children);
	assert(nested_calls == 1u);

	recursively_wrapped_pack recursive_record{
		recursively_wrapped_pack_leaf{'P'}};
	inner_pack_parameter recursive_inner{recursive_record};
	outer_pack_parameter recursive_outer{recursive_inner};
	recursively_wrapped_pack_source recursive_source{
		__builtin_addressof(recursive_outer)};
	::fast_io::io::print(output, recursive_source);
	assert(capture.size == 2u && capture.bytes[1] == 'P');

	mutex_state state;
	mutex_terminal<const_status_tag> terminal{__builtin_addressof(state)};
	const_status_locked_sink locked{
		__builtin_addressof(state), __builtin_addressof(terminal)};
	auto active_record{::fast_io::mnp::pack(mutex_active_leaf{})};
	::fast_io::io::print(locked, active_record);
	::fast_io::io::println(locked, active_record);
	assert(state.lock_calls == 2u && state.unlock_calls == 2u &&
		   state.status_calls == 2u && !state.locked);

	mutex_state ignored_empty_state;
	mutex_terminal<mutable_status_tag> ignored_empty_terminal{
		__builtin_addressof(ignored_empty_state)};
	mutable_status_locked_sink ignored_empty_locked{
		__builtin_addressof(ignored_empty_state),
		__builtin_addressof(ignored_empty_terminal)};
	::fast_io::io::print(ignored_empty_locked, no_children);
	assert(ignored_empty_state.lock_calls == 0u &&
		   ignored_empty_state.unlock_calls == 0u &&
		   ignored_empty_state.status_calls == 0u &&
		   !ignored_empty_state.locked);

	mutex_state observed_empty_state;
	mutex_terminal<const_status_tag> observed_empty_terminal{
		__builtin_addressof(observed_empty_state)};
	const_status_locked_sink observed_empty_locked{
		__builtin_addressof(observed_empty_state),
		__builtin_addressof(observed_empty_terminal)};
	::fast_io::io::print(observed_empty_locked, no_children);
	assert(observed_empty_state.lock_calls == 1u &&
		   observed_empty_state.unlock_calls == 1u &&
		   observed_empty_state.status_calls == 1u &&
		   !observed_empty_state.locked);

	// A parameter-wrapped null is an active argument; the adjacent empty pack
	// merely enables structural inspection and must not turn that wrapper into
	// a zero-argument record before mutex acquisition.
	mutex_state wrapped_mutex_state;
	mutex_terminal<wrapped_null_mutex_tag> wrapped_terminal{
		__builtin_addressof(wrapped_mutex_state)};
	wrapped_null_locked_sink wrapped_locked{
		__builtin_addressof(wrapped_mutex_state),
		__builtin_addressof(wrapped_terminal)};
	::fast_io::io::print(wrapped_locked, null_source, no_children);
	assert(wrapped_mutex_state.lock_calls == 1u &&
		   wrapped_mutex_state.unlock_calls == 1u &&
		   wrapped_mutex_state.status_calls == 1u &&
		   !wrapped_mutex_state.locked);

	::std::size_t public_const_calls{};
	public_const_terminal public_terminal{
		__builtin_addressof(public_const_calls)};
	public_const_owner public_owner{__builtin_addressof(public_terminal)};
	mutex_active_leaf public_leaf;
	::fast_io::io::print(public_owner, public_leaf);
	::fast_io::io::print(public_owner);
	assert(public_const_calls == 2u);

	::std::size_t public_mutable_calls{};
	public_mutable_terminal public_mutable_terminal_value{
		__builtin_addressof(public_mutable_calls)};
	public_mutable_owner public_mutable_owner_value{
		__builtin_addressof(public_mutable_terminal_value)};
	::fast_io::io::print(public_mutable_owner_value, public_leaf);
	::fast_io::io::print(public_mutable_owner_value);
	assert(public_mutable_calls == 2u);
}
