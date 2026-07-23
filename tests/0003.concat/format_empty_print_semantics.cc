#include <fast_io_format.h>

#include <cstddef>

namespace
{

struct empty_status_state
{
	::std::size_t output_references{};
	::std::size_t status_calls{};
};

struct empty_status_sink
{
	using output_char_type = char;
	empty_status_state *state{};
};

struct immovable_empty_status_sink
{
	using output_char_type = char;
	empty_status_state *state{};

	inline explicit immovable_empty_status_sink(empty_status_state *value) noexcept
		: state(value)
	{}

	immovable_empty_status_sink(immovable_empty_status_sink const &) = delete;
	immovable_empty_status_sink &operator=(immovable_empty_status_sink const &) = delete;
};

struct active_status_only_leaf
{};

struct unsupported_active_leaf
{};

inline empty_status_sink output_stream_ref_define(
	empty_status_sink sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

inline immovable_empty_status_sink &output_stream_ref_define(
	immovable_empty_status_sink &sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

template <bool line>
inline void status_print_define(empty_status_sink sink) noexcept
{
	// This zero-argument CPO is the destination's explicit proof that an
	// otherwise empty logical print record remains observable.
	++sink.state->status_calls;
}

template <bool line>
inline void status_print_define(immovable_empty_status_sink &sink) noexcept
{
	++sink.state->status_calls;
}

template <bool line>
inline void status_print_define(
	immovable_empty_status_sink &sink, active_status_only_leaf &) noexcept
{
	// This leaf intentionally has no standalone formatter. It is valid only as
	// the exact active record selected after semantic pack/condition expansion.
	++sink.state->status_calls;
}

struct empty_plain_state
{
	::std::size_t output_references{};
	::std::size_t writes{};
};

struct empty_plain_sink
{
	using output_char_type = char;
	empty_plain_state *state{};
};

inline empty_plain_sink output_stream_ref_define(
	empty_plain_sink sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

[[maybe_unused]] inline void write_all_overflow_define(
	empty_plain_sink sink, char const *, char const *) noexcept
{
	++sink.state->writes;
}

struct empty_locked_state
{
	::std::size_t output_references{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t status_calls{};
	bool locked{};
};

struct empty_locked_sink
{
	using output_char_type = char;
	empty_locked_state *state{};
};

struct empty_unlocked_sink
{
	using output_char_type = char;
	empty_locked_state *state{};
};

struct empty_lock_proxy
{
	empty_locked_state *state{};

	inline void lock() noexcept
	{
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		state->locked = false;
		++state->unlocks;
	}
};

inline empty_locked_sink output_stream_ref_define(
	empty_locked_sink sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

inline constexpr empty_lock_proxy output_stream_mutex_ref_define(
	empty_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr empty_unlocked_sink output_stream_unlocked_ref_define(
	empty_locked_sink sink) noexcept
{
	return {sink.state};
}

template <bool line>
	requires(!line)
inline void status_print_define(empty_unlocked_sink sink) noexcept
{
	if (!sink.state->locked)
	{
		::fast_io::fast_terminate();
	}
	++sink.state->status_calls;
}

struct empty_silent_locked_sink
{
	using output_char_type = char;
	empty_locked_state *state{};
};

struct empty_silent_unlocked_sink
{
	using output_char_type = char;
	empty_locked_state *state{};
};

inline empty_silent_locked_sink output_stream_ref_define(
	empty_silent_locked_sink sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

inline constexpr empty_lock_proxy output_stream_mutex_ref_define(
	empty_silent_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr empty_silent_unlocked_sink output_stream_unlocked_ref_define(
	empty_silent_locked_sink sink) noexcept
{
	return {sink.state};
}

template <bool line>
	requires(!line)
inline void status_print_define(empty_silent_locked_sink sink) noexcept
{
	// A mutex wrapper's outer status CPO is intentionally unreachable. The print
	// level must discover empty-record observability only after following the
	// complete mutex protocol to the effective unlocked destination.
	++sink.state->status_calls;
}

[[maybe_unused]] inline void write_all_overflow_define(
	empty_silent_unlocked_sink sink, char const *, char const *) noexcept
{
	++sink.state->status_calls;
}

template <typename sink_type>
inline void exercise_public_empty_print_entries(sink_type &sink)
{
	::fast_io::io::print(sink);
	::fast_io::io::perr(sink);
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
	::fast_io::io::debug_print(sink);
	::fast_io::io::debug_perr(sink);
#endif
	::fast_io::operations::print_freestanding<false>(sink);
	::fast_io::fmt::print<"">(sink);
	::fast_io::fmt::printf<"">(sink);
}

// Five non-line front doors are always present: print, perr, the operations
// entry, and the two format grammars. Line mode has three corresponding
// front doors because the format layer deliberately exposes no println API.
inline constexpr ::std::size_t base_public_entry_count{5u};
inline constexpr ::std::size_t base_line_entry_count{3u};
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
inline constexpr ::std::size_t debug_entry_count{2u};
#else
inline constexpr ::std::size_t debug_entry_count{};
#endif
inline constexpr ::std::size_t public_entry_count{
	base_public_entry_count + debug_entry_count};
inline constexpr ::std::size_t line_entry_count{
	base_line_entry_count + debug_entry_count};
inline constexpr ::std::size_t normalized_entry_count{20u};

template <bool line, typename normalized_output>
inline void exercise_normalized_empty_print_entries(
	normalized_output &output)
{
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization<line>(output);
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization_cold<line>(output);
	::fast_io::operations::decay::print_freestanding_decay_impl<line>(output);
	::fast_io::operations::decay::print_freestanding_decay_no_pack<line>(output);
	::fast_io::operations::decay::print_freestanding_decay<line>(output);
	::fast_io::operations::decay::print_freestanding_decay_borrowed_output<line>(output);
	::fast_io::operations::decay::
		print_freestanding_decay_borrowed_output_and_arguments<line>(output);
	::fast_io::operations::decay::print_freestanding_decay_cold<line>(output);
	::fast_io::operations::decay::
		print_freestanding_decay_cold_borrowed_output<line>(output);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<line>(output);
	::fast_io::operations::decay::
		print_freestanding_decay_cold_unforwarded<line>(output);
	::fast_io::operations::decay::
		print_freestanding_decay_compiler_constant_dispatch<line>(output);
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_true_emit_after_lock<line>(output);
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_true_emit<line>(output);
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_flat_continuation<
			line, typename normalized_output::output_char_type,
			normalized_output>{output}();
	::fast_io::operations::decay::print_semantic_emit<
		line, true, typename normalized_output::output_char_type>(output);
	::fast_io::operations::decay::print_semantic_emit<
		line, false, typename normalized_output::output_char_type>(output);
	::fast_io::operations::decay::print_semantic_emit_flat_fallback<
		line, typename normalized_output::output_char_type>(output);
	::fast_io::operations::decay::
		print_semantic_emit_freestanding_continuation<normalized_output>{output}
			.template operator()<line>();
	::fast_io::operations::decay::print_semantic_emit_flat_continuation<
		line, typename normalized_output::output_char_type,
		normalized_output>{output}();
}

static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, empty_status_sink>);
static_assert(
	!::fast_io::operations::decay::defines::has_status_print_define<
		false, empty_plain_sink>);
static_assert(
	::fast_io::operations::decay::defines::empty_print_observable<
		empty_status_sink>);
static_assert(
	!::fast_io::operations::decay::defines::empty_print_observable<
		empty_plain_sink>);
static_assert(
	::fast_io::operations::decay::defines::empty_print_observable<
		empty_locked_sink>);
static_assert(
	::fast_io::operations::decay::defines::has_status_print_define<
		false, empty_silent_locked_sink>);
static_assert(
	!::fast_io::operations::decay::defines::empty_print_observable<
		empty_silent_locked_sink>);

using active_status_only_pack = decltype(::fast_io::mnp::pack(active_status_only_leaf{}));
using partially_supported_condition = decltype(::fast_io::mnp::cond(
	true, active_status_only_leaf{}, unsupported_active_leaf{}));
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, immovable_empty_status_sink &, active_status_only_pack &>);
static_assert(
	::fast_io::operations::defines::print_freestanding_okay_for_line<
		true, immovable_empty_status_sink &, active_status_only_pack &>);
static_assert(
	!::fast_io::operations::defines::print_freestanding_okay_for_line<
		false, immovable_empty_status_sink &, partially_supported_condition &>);
static_assert(
	!::fast_io::operations::defines::print_freestanding_okay_for_line<
		true, immovable_empty_status_sink &, partially_supported_condition &>);

} // namespace

int main()
{
	empty_status_state immovable_status{};
	immovable_empty_status_sink immovable_status_sink{
		__builtin_addressof(immovable_status)};
	// The terminal semantic flattener merely carries the already-normalized
	// observer to its continuation. A filtered-to-empty record must not copy that
	// observer before the shared zero-record dispatcher can invoke exact status.
	::fast_io::operations::decay::print_semantic_emit_flat_fallback<false, char>(
		immovable_status_sink);
	::fast_io::operations::decay::print_semantic_emit_flat_fallback<true, char>(
		immovable_status_sink);
	auto empty_semantic_record{::fast_io::mnp::pack()};
	::fast_io::io::print(immovable_status_sink, empty_semantic_record);
	::fast_io::io::println(immovable_status_sink, empty_semantic_record);
	auto active_status_record{
		::fast_io::mnp::pack(active_status_only_leaf{})};
	::fast_io::io::print(immovable_status_sink, active_status_record);
	::fast_io::io::println(immovable_status_sink, active_status_record);
	auto inactive_status_condition{
		::fast_io::mnp::cond(false, active_status_only_leaf{})};
	::fast_io::io::print(immovable_status_sink, inactive_status_condition);
	::fast_io::io::println(immovable_status_sink, inactive_status_condition);
	auto active_status_condition{
		::fast_io::mnp::cond(true, active_status_only_leaf{})};
	::fast_io::io::print(immovable_status_sink, active_status_condition);
	::fast_io::io::println(immovable_status_sink, active_status_condition);
	if (immovable_status.output_references != 8u ||
		immovable_status.status_calls != 10u)
	{
		return 12;
	}

	empty_status_state status{};
	empty_status_sink status_sink{__builtin_addressof(status)};
	exercise_public_empty_print_entries(status_sink);
	if (status.output_references != public_entry_count ||
		status.status_calls != public_entry_count)
	{
		return 1;
	}
	status = {};
	exercise_normalized_empty_print_entries<false>(status_sink);
	if (status.output_references != 0u ||
		status.status_calls != normalized_entry_count)
	{
		return 2;
	}
	status = {};
	if (::fast_io::details::panic_try_compiler_constant_pre_normalization<false>(
			status_sink) ||
		status.output_references != 0u || status.status_calls != 0u)
	{
		// Panic's speculative constant arm must leave normalization entirely to
		// its perr fallback when the device has an empty payload.
		return 9;
	}
	status = {};
	::fast_io::io::println(status_sink);
	::fast_io::io::perrln(status_sink);
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
	::fast_io::io::debug_println(status_sink);
	::fast_io::io::debug_perrln(status_sink);
#endif
	::fast_io::operations::print_freestanding<true>(status_sink);
	if (status.output_references != line_entry_count ||
		status.status_calls != line_entry_count)
	{
		return 10;
	}
	status = {};
	exercise_normalized_empty_print_entries<true>(status_sink);
	if (status.output_references != 0u ||
		status.status_calls != normalized_entry_count)
	{
		return 11;
	}

	empty_locked_state locked{};
	empty_locked_sink locked_sink{__builtin_addressof(locked)};
	exercise_public_empty_print_entries(locked_sink);
	if (locked.output_references != public_entry_count ||
		locked.locks != public_entry_count ||
		locked.unlocks != public_entry_count ||
		locked.status_calls != public_entry_count || locked.locked)
	{
		return 3;
	}
	locked = {};
	exercise_normalized_empty_print_entries<false>(locked_sink);
	if (locked.output_references != 0u ||
		locked.locks != normalized_entry_count ||
		locked.unlocks != normalized_entry_count ||
		locked.status_calls != normalized_entry_count || locked.locked)
	{
		return 4;
	}

	empty_plain_state plain{};
	empty_plain_sink plain_sink{__builtin_addressof(plain)};
	exercise_public_empty_print_entries(plain_sink);
	if (plain.output_references != public_entry_count || plain.writes != 0u)
	{
		return 5;
	}
	plain = {};
	exercise_normalized_empty_print_entries<false>(plain_sink);
	if (plain.output_references != 0u || plain.writes != 0u)
	{
		return 6;
	}

	empty_locked_state silent{};
	empty_silent_locked_sink silent_sink{__builtin_addressof(silent)};
	exercise_public_empty_print_entries(silent_sink);
	if (silent.output_references != public_entry_count || silent.locks != 0u ||
		silent.unlocks != 0u || silent.status_calls != 0u || silent.locked)
	{
		return 7;
	}
	exercise_normalized_empty_print_entries<false>(silent_sink);
	// Output-reference normalization belongs to the public facade. Once the
	// normalized record reaches the print level, an unobservable empty run is
	// discarded before mutex acquisition or primitive output on every entry.
	return silent.output_references == public_entry_count && silent.locks == 0u &&
				   silent.unlocks == 0u && silent.status_calls == 0u && !silent.locked
			   ? 0
			   : 8;
}
