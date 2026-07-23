#include <fast_io_format.h>

#include <cstddef>
#include <type_traits>

namespace
{

struct empty_entry_state
{
	::std::size_t output_references{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t effective_status_calls{};
	::std::size_t outer_status_calls{};
	::std::size_t writes{};
	bool locked{};
};

template <typename char_type, bool observable>
struct empty_entry_locked_sink
{
	using output_char_type = char_type;
	empty_entry_state *state{};
};

template <typename char_type, bool observable>
struct empty_entry_unlocked_sink
{
	using output_char_type = char_type;
	empty_entry_state *state{};
};

struct empty_entry_lock
{
	empty_entry_state *state{};

	inline void lock() noexcept
	{
		if (state->locked)
		{
			::fast_io::fast_terminate();
		}
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		if (!state->locked)
		{
			::fast_io::fast_terminate();
		}
		state->locked = false;
		++state->unlocks;
	}
};

template <typename char_type, bool observable>
inline empty_entry_locked_sink<char_type, observable>
output_stream_ref_define(
	empty_entry_locked_sink<char_type, observable> sink) noexcept
{
	++sink.state->output_references;
	return sink;
}

template <typename char_type, bool observable>
inline constexpr empty_entry_lock output_stream_mutex_ref_define(
	empty_entry_locked_sink<char_type, observable> sink) noexcept
{
	return {sink.state};
}

template <typename char_type, bool observable>
inline constexpr empty_entry_unlocked_sink<char_type, observable>
output_stream_unlocked_ref_define(
	empty_entry_locked_sink<char_type, observable> sink) noexcept
{
	return {sink.state};
}

template <bool line, typename char_type, bool observable>
	requires(!line)
inline void status_print_define(
	empty_entry_locked_sink<char_type, observable> sink) noexcept
{
	// This decoy proves that observability belongs to the effective destination,
	// not to a mutex wrapper which execution can never use for status output.
	++sink.state->outer_status_calls;
}

template <bool line, typename char_type>
	requires(!line)
inline void status_print_define(
	empty_entry_unlocked_sink<char_type, true> sink) noexcept
{
	if (!sink.state->locked)
	{
		::fast_io::fast_terminate();
	}
	++sink.state->effective_status_calls;
}

template <typename char_type, bool observable>
inline void write_all_overflow_define(
	empty_entry_unlocked_sink<char_type, observable> sink,
	char_type const *, char_type const *) noexcept
{
	if (!sink.state->locked)
	{
		::fast_io::fast_terminate();
	}
	++sink.state->writes;
}

template <typename char_type, typename sink_type>
inline void exercise_format_facades(sink_type &sink)
{
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"">(sink);
		::fast_io::fmt::printf<"">(sink);
		::fast_io::fmt::details::print_with_rule<"">(
			::fast_io::fmt::brace_fmt_t{}, sink);
		::fast_io::fmt::details::print_with_rule<"">(
			::fast_io::fmt::printf_fmt_t{}, sink);
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"">(sink);
		::fast_io::fmt::wprintf<L"">(sink);
		::fast_io::fmt::details::print_with_rule<L"">(
			::fast_io::fmt::brace_fmt_t{}, sink);
		::fast_io::fmt::details::print_with_rule<L"">(
			::fast_io::fmt::printf_fmt_t{}, sink);
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"">(sink);
		::fast_io::fmt::u8printf<u8"">(sink);
		::fast_io::fmt::details::print_with_rule<u8"">(
			::fast_io::fmt::brace_fmt_t{}, sink);
		::fast_io::fmt::details::print_with_rule<u8"">(
			::fast_io::fmt::printf_fmt_t{}, sink);
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"">(sink);
		::fast_io::fmt::u16printf<u"">(sink);
		::fast_io::fmt::details::print_with_rule<u"">(
			::fast_io::fmt::brace_fmt_t{}, sink);
		::fast_io::fmt::details::print_with_rule<u"">(
			::fast_io::fmt::printf_fmt_t{}, sink);
	}
	else
	{
		static_assert(::std::same_as<char_type, char32_t>);
		::fast_io::fmt::u32print<U"">(sink);
		::fast_io::fmt::u32printf<U"">(sink);
		::fast_io::fmt::details::print_with_rule<U"">(
			::fast_io::fmt::brace_fmt_t{}, sink);
		::fast_io::fmt::details::print_with_rule<U"">(
			::fast_io::fmt::printf_fmt_t{}, sink);
	}
}

template <typename sink_type>
inline void exercise_legacy_explicit_facades(sink_type &sink)
{
	::fast_io::io::print(sink);
	::fast_io::io::perr(sink);
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
	::fast_io::io::debug_print(sink);
	::fast_io::io::debug_perr(sink);
#endif
	::fast_io::operations::print_freestanding<false>(sink);
}

template <typename sink_type>
inline void exercise_normalized_zero_source_bridges(sink_type &sink)
{
	// These are the distinct hot, cold, borrowed, and compiler-constant
	// ownership boundaries which legacy/default facades can select after output
	// normalization. Every zero-source route must delegate to the same gate.
	::fast_io::operations::decay::print_freestanding_empty_run(sink);
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization<false>(sink);
	::fast_io::operations::decay::
		print_freestanding_compiler_constant_pre_normalization_cold<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay_impl<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay_no_pack<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay_borrowed_output<false>(sink);
	::fast_io::operations::decay::
		print_freestanding_decay_borrowed_output_and_arguments<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay_cold<false>(sink);
	::fast_io::operations::decay::
		print_freestanding_decay_cold_borrowed_output<false>(sink);
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(sink);
	::fast_io::operations::decay::
		print_freestanding_decay_cold_unforwarded<false>(sink);
	::fast_io::operations::decay::
		print_freestanding_decay_compiler_constant_dispatch<false>(sink);
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_true_emit_after_lock<false>(sink);
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_true_emit<false>(sink);
}

inline constexpr ::std::size_t format_entry_count{4u};
inline constexpr ::std::size_t base_legacy_entry_count{3u};
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
inline constexpr ::std::size_t debug_legacy_entry_count{2u};
#else
inline constexpr ::std::size_t debug_legacy_entry_count{};
#endif
inline constexpr ::std::size_t public_entry_count{
	format_entry_count + base_legacy_entry_count + debug_legacy_entry_count};
inline constexpr ::std::size_t normalized_bridge_count{15u};

template <typename char_type, bool observable>
[[nodiscard]] inline bool run_typed_matrix()
{
	using sink_type = empty_entry_locked_sink<char_type, observable>;
	static_assert(
		::fast_io::operations::decay::defines::has_status_print_define<
			false, sink_type>);
	static_assert(
		::fast_io::operations::decay::defines::empty_print_observable<
			sink_type> == observable);

	empty_entry_state state{};
	sink_type sink{__builtin_addressof(state)};
	exercise_format_facades<char_type>(sink);
	exercise_legacy_explicit_facades(sink);
	exercise_normalized_zero_source_bridges(sink);

	constexpr ::std::size_t total_entry_count{
		public_entry_count + normalized_bridge_count};
	if (state.output_references != public_entry_count || state.writes != 0u ||
		state.outer_status_calls != 0u || state.locked)
	{
		return false;
	}
	if constexpr (observable)
	{
		return state.locks == total_entry_count &&
			   state.unlocks == total_entry_count &&
			   state.effective_status_calls == total_entry_count;
	}
	else
	{
		return state.locks == 0u && state.unlocks == 0u &&
			   state.effective_status_calls == 0u;
	}
}

inline void exercise_fixed_default_empty_routes()
{
	// Default-output facades deliberately do not accept an injectable stream.
	// Their explicit-output siblings above prove the exact lock/status contract,
	// while these calls prove that every fixed-device overload remains connected
	// to the same source-free lowering and compiles for its selected code-unit
	// domain. The default library streams are silent providers, so no bytes are
	// emitted by these non-line calls.
	::fast_io::fmt::print<"">();
	::fast_io::fmt::printf<"">();
	::fast_io::fmt::wprint<L"">();
	::fast_io::fmt::wprintf<L"">();
	::fast_io::fmt::u8print<u8"">();
	::fast_io::fmt::u8printf<u8"">();
	::fast_io::fmt::u16print<u"">();
	::fast_io::fmt::u16printf<u"">();
	::fast_io::fmt::u32print<U"">();
	::fast_io::fmt::u32printf<U"">();

	// These legacy helpers own fixed stdout/stderr selection and likewise cannot
	// be supplied a test sink. Their immediate continuations are the hot/cold
	// normalized bridges measured above, so invoking the source-free forms here
	// closes the fixed-device wiring check without pretending that a default
	// stream can expose the observable-status provider used by the matrix.
	::fast_io::details::print_after_source_pre_normalization<false>();
	::fast_io::details::print_after_io_print_forward<false>();
	::fast_io::details::perr_after_source_pre_normalization<false>();
	::fast_io::details::perr_after_io_print_forward<false>();
#ifndef FAST_IO_DISABLE_DEBUG_PRINT
	if (::fast_io::details::debug_print_try_default_compiler_constant<false>())
	{
		::fast_io::fast_terminate();
	}
	::fast_io::details::debug_print_after_source_pre_normalization<false>();
	::fast_io::details::debug_print_after_io_print_forward<false>();
#endif
}

} // namespace

int main()
{
	bool const passed{
		run_typed_matrix<char, false>() &&
		run_typed_matrix<char, true>() &&
		run_typed_matrix<wchar_t, false>() &&
		run_typed_matrix<wchar_t, true>() &&
		run_typed_matrix<char8_t, false>() &&
		run_typed_matrix<char8_t, true>() &&
		run_typed_matrix<char16_t, false>() &&
		run_typed_matrix<char16_t, true>() &&
		run_typed_matrix<char32_t, false>() &&
		run_typed_matrix<char32_t, true>()};
	if (!passed)
	{
		return 1;
	}
	exercise_fixed_default_empty_routes();
	return 0;
}
