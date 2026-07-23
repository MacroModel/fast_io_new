#include <cassert>
#include <concepts>
#include <cstddef>
#include <string>
#include <utility>

#include <fast_io.h>
#include <fast_io_i18n.h>

namespace
{

struct capture_state
{
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t nonline_status_calls{};
	::std::size_t line_status_calls{};
	::std::size_t locale_print_calls{};
	::std::size_t writes{};
	::std::size_t bytes{};
	bool locked{};
};

struct lock_proxy
{
	capture_state *state{};

	inline void lock() const noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct silent_locked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct silent_unlocked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct observable_locked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

struct observable_unlocked_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr silent_locked_sink output_stream_ref_define(
	silent_locked_sink sink) noexcept
{
	return sink;
}

inline constexpr observable_locked_sink output_stream_ref_define(
	observable_locked_sink sink) noexcept
{
	return sink;
}

inline constexpr lock_proxy output_stream_mutex_ref_define(
	silent_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr lock_proxy output_stream_mutex_ref_define(
	observable_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr silent_unlocked_sink output_stream_unlocked_ref_define(
	silent_locked_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr observable_unlocked_sink output_stream_unlocked_ref_define(
	observable_locked_sink sink) noexcept
{
	return {sink.state};
}

inline void write_all_overflow_define(
	silent_unlocked_sink sink, char const *first, char const *last) noexcept
{
	assert(sink.state->locked);
	++sink.state->writes;
	sink.state->bytes += static_cast<::std::size_t>(last - first);
}

[[maybe_unused]] inline void write_all_overflow_define(
	observable_unlocked_sink sink, char const *first, char const *last) noexcept
{
	assert(sink.state->locked);
	++sink.state->writes;
	sink.state->bytes += static_cast<::std::size_t>(last - first);
}

template <bool line>
inline void status_print_define(observable_unlocked_sink sink) noexcept
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

using silent_locale_output = ::fast_io::lc_imbuer<silent_locked_sink>;
using observable_locale_output = ::fast_io::lc_imbuer<observable_locked_sink>;

struct mutable_locale_status_sink
{
	using output_char_type = char;
};

struct const_locale_status_sink
{
	using output_char_type = char;
};

struct locale_const_leaf
{};

struct locale_only_leaf
{};

struct forwarding_inactive_branch
{};

using forwarding_replaced_condition =
	::fast_io::manipulators::condition<
		::fast_io::io_null_t, forwarding_inactive_branch>;

struct forwarding_replacement_leaf
{
	char value{'L'};
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

static_assert(::std::same_as<
			  normalized_forwarding_replacement,
			  forwarding_replacement_leaf>);

struct impossible_locale_sink
{
	using output_char_type = char;
};

struct impossible_locale_leaf
{};

template <bool line>
	requires(!line)
inline void status_print_define(mutable_locale_status_sink &) noexcept
{}

template <bool line>
inline void status_print_define(mutable_locale_status_sink &, int &) noexcept
{}

template <bool line>
inline void status_print_define(
	mutable_locale_status_sink &, locale_const_leaf &) noexcept
{}

template <bool line>
	requires(!line)
inline void status_print_define(const_locale_status_sink const &) noexcept
{}

template <bool line>
inline void status_print_define(const_locale_status_sink const &, int &) noexcept
{}

inline void print_define(
	::fast_io::basic_lc_all<char> const *, observable_unlocked_sink sink,
	locale_only_leaf &) noexcept
{
	assert(sink.state->locked);
	++sink.state->locale_print_calls;
}

using mutable_status_const_locale_output =
	::fast_io::lc_imbuer<mutable_locale_status_sink const &>;
using const_status_const_locale_output =
	::fast_io::lc_imbuer<const_locale_status_sink const &>;
using const_locale_leaf_parameter =
	::fast_io::parameter<locale_const_leaf const &>;
using mutable_locale_leaf_parameter =
	::fast_io::parameter<locale_const_leaf &>;
using impossible_locale_output =
	::fast_io::lc_imbuer<impossible_locale_sink>;

// A locale adapter has no zero-source work of its own. It may expose the
// non-line empty-record capability exactly when its normalized handle proves
// the same capability through the complete mutex protocol.
static_assert(
	!::fast_io::operations::decay::defines::empty_print_observable<
		silent_locale_output>);
static_assert(
	::fast_io::operations::decay::defines::empty_print_observable<
		observable_locale_output>);

// Locale binding receives the named handle expression. A const handle cannot
// borrow a mutable-only status customization during either ordinary admission
// or zero-record observability discovery.
static_assert(!::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, mutable_locale_status_sink const, int>);
static_assert(::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, const_locale_status_sink const, int>);
static_assert(
	!::fast_io::operations::decay::defines::empty_print_observable<
		mutable_status_const_locale_output>);
static_assert(
	::fast_io::operations::decay::defines::empty_print_observable<
		const_status_const_locale_output>);

// Locale binding unwraps parameter transport but retains the referent's const
// qualification. Admission must test that exact leaf expression rather than a
// mutable surrogate.
static_assert(::std::same_as<
			  decltype(::fast_io::details::decay::lc_bind_one<
					   char, mutable_locale_status_sink>(
				  static_cast<::fast_io::basic_lc_all<char> const *>(nullptr),
				  ::std::declval<const_locale_leaf_parameter &>())),
			  locale_const_leaf const &>);
static_assert(!::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, mutable_locale_status_sink, const_locale_leaf_parameter>);
static_assert(::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, mutable_locale_status_sink, mutable_locale_leaf_parameter>);

// Locale output-specific CPO selection occurs only after mutex unwrapping.
// Conversely, the outer locale status provider must not certify a nonempty
// record which the effective output cannot consume after binding.
static_assert(::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, observable_locked_sink, locale_only_leaf>);
static_assert(!::fast_io::details::decay::lc_status_print_output_run_okay<
			  false, char, impossible_locale_sink, impossible_locale_leaf>);
static_assert(
	!::fast_io::operations::decay::defines::has_status_print_define<
		false, impossible_locale_output, impossible_locale_leaf>);

} // namespace

int main()
{
	::fast_io::basic_lc_object<char> locale;
	auto const *const locale_all{__builtin_addressof(locale.all)};

	// `lc_status_print_define_decay` is itself a complete-record entry used by
	// locale concat. Its zero-source branch must therefore apply the same
	// empty-record proof before locale selection or mutex acquisition.
	capture_state direct_silent_state;
	silent_locked_sink direct_silent_output{__builtin_addressof(direct_silent_state)};
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_silent_output);
	assert(direct_silent_state.locks == 0u);
	assert(direct_silent_state.unlocks == 0u);
	assert(direct_silent_state.writes == 0u);
	auto empty_record{::fast_io::mnp::pack()};
	auto nested_empty_record{
		::fast_io::mnp::pack(::fast_io::mnp::pack())};
	auto inactive_condition{
		::fast_io::mnp::cond(false, "not emitted")};
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_silent_output, empty_record);
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_silent_output, nested_empty_record);
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_silent_output, inactive_condition);
	assert(direct_silent_state.locks == 0u);
	assert(direct_silent_state.unlocks == 0u);
	assert(direct_silent_state.writes == 0u);

	capture_state direct_observable_state;
	observable_locked_sink direct_observable_output{
		__builtin_addressof(direct_observable_state)};
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_observable_output);
	assert(direct_observable_state.locks == 1u);
	assert(direct_observable_state.unlocks == 1u);
	assert(direct_observable_state.nonline_status_calls == 1u);
	assert(direct_observable_state.line_status_calls == 0u);
	assert(direct_observable_state.writes == 0u);
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_observable_output, empty_record);
	assert(direct_observable_state.locks == 2u);
	assert(direct_observable_state.unlocks == 2u);
	assert(direct_observable_state.nonline_status_calls == 2u);
	assert(direct_observable_state.line_status_calls == 0u);
	assert(direct_observable_state.writes == 0u);
	assert(!direct_observable_state.locked);

	locale_only_leaf locale_leaf;
	::fast_io::operations::decay::lc_status_print_define_decay<false>(
		locale_all, direct_observable_output, locale_leaf);
	assert(direct_observable_state.locks == 3u);
	assert(direct_observable_state.unlocks == 3u);
	assert(direct_observable_state.locale_print_calls == 1u);
	assert(!direct_observable_state.locked);

	// The public locale concat facade enters the same record boundary directly.
	// A non-line source-free record constructs an empty string, while line mode
	// remains observable through its required newline.
	auto const empty_concat{::fast_io::lc_concat(locale_all)};
	auto const empty_concatln{::fast_io::lc_concatln(locale_all)};
	assert(empty_concat.empty());
	assert(empty_concatln == "\n");

	capture_state silent_state;
	auto silent_output{
		::fast_io::imbue(locale, silent_locked_sink{__builtin_addressof(silent_state)})};
	::fast_io::io::print(silent_output);
	assert(silent_state.locks == 0u);
	assert(silent_state.unlocks == 0u);
	assert(silent_state.writes == 0u);
	::fast_io::io::print(silent_output, empty_record);
	::fast_io::io::perr(silent_output, nested_empty_record);
	::fast_io::io::debug_print(silent_output, inactive_condition);
	assert(silent_state.locks == 0u);
	assert(silent_state.unlocks == 0u);
	assert(silent_state.writes == 0u);

	// Locale status owns the source record, but its mutex precheck shares the
	// ordinary semantic phase proof. A condition stored in a pack is still a
	// raw member here: its ADL forwarding replacement must be formed under the
	// output lock and then pass through locale binding as one nonempty leaf.
	forwarding_replaced_condition forwarding_condition{
		true, ::fast_io::io_null, forwarding_inactive_branch{}};
	auto forwarding_record{::fast_io::mnp::pack(forwarding_condition)};
	assert(!::fast_io::details::decay::print_semantic_run_provably_empty(
		forwarding_record));
	capture_state forwarding_locale_state;
	auto forwarding_locale_output{::fast_io::imbue(
		locale,
		silent_locked_sink{__builtin_addressof(forwarding_locale_state)})};
	::fast_io::io::print(forwarding_locale_output, forwarding_record);
	assert(forwarding_locale_state.locks == 1u);
	assert(forwarding_locale_state.unlocks == 1u);
	assert(forwarding_locale_state.writes == 1u);
	assert(forwarding_locale_state.bytes == 1u);
	assert(!forwarding_locale_state.locked);

	// A line record is not empty: the locale adapter must retain the underlying
	// synchronization boundary while the ordinary printer emits its newline.
	::fast_io::io::println(silent_output);
	assert(silent_state.locks == 1u);
	assert(silent_state.unlocks == 1u);
	assert(silent_state.writes == 1u);
	assert(silent_state.bytes == 1u);
	assert(!silent_state.locked);

	capture_state observable_state;
	auto observable_output{::fast_io::imbue(
		locale, observable_locked_sink{__builtin_addressof(observable_state)})};
	::fast_io::io::print(observable_output);
	assert(observable_state.locks == 1u);
	assert(observable_state.unlocks == 1u);
	assert(observable_state.nonline_status_calls == 1u);
	assert(observable_state.line_status_calls == 0u);
	assert(observable_state.writes == 0u);
	::fast_io::io::print(observable_output, empty_record);
	assert(observable_state.locks == 2u);
	assert(observable_state.unlocks == 2u);
	assert(observable_state.nonline_status_calls == 2u);
	assert(observable_state.line_status_calls == 0u);
	assert(observable_state.writes == 0u);

	::fast_io::io::println(observable_output);
	assert(observable_state.locks == 3u);
	assert(observable_state.unlocks == 3u);
	assert(observable_state.nonline_status_calls == 2u);
	assert(observable_state.line_status_calls == 1u);
	assert(observable_state.writes == 0u);
	assert(!observable_state.locked);
}
