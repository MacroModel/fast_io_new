#include <fast_io.h>
#include <fast_io_i18n.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace locale_strategy_test
{

struct protocol_counts
{
	::std::size_t locale_size_calls{};
	::std::size_t locale_define_calls{};
	::std::size_t locale_scatter_calls{};
	::std::size_t actual_direct_calls{};
	::std::size_t concat_direct_calls{};
	::std::size_t dummy_direct_calls{};
};

struct const_protocol_counts
{
	::std::size_t sizes{};
	::std::size_t defines{};
	::std::size_t directs{};
	::std::size_t shifts{};
};

struct capture_sink
{
	using char_type = char;
	using output_char_type = char;
	::std::string *storage;
	::std::size_t *write_calls;
	::std::size_t *scatter_calls;
};

struct identity_observer
{
	using output_char_type = char;
	::std::size_t direct_calls{};

	identity_observer() = default;
	identity_observer(identity_observer const &) = delete;
	identity_observer &operator=(identity_observer const &) = delete;
};

struct identity_output
{
	identity_observer observer;
};

struct lock_state
{
	::std::size_t locks{};
	::std::size_t unlocks{};
	bool locked{};
};

struct locked_capture_sink
{
	using char_type = char;
	using output_char_type = char;
	capture_sink unlocked;
	lock_state *state;
};

struct lock_proxy
{
	lock_state *state;

	inline void lock() noexcept
	{
		if (state->locked)
		{
			::std::abort();
		}
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		if (!state->locked)
		{
			::std::abort();
		}
		state->locked = false;
		++state->unlocks;
	}
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline constexpr identity_observer &output_stream_ref_define(identity_output &output) noexcept
{
	return output.observer;
}

inline constexpr locked_capture_sink output_stream_ref_define(locked_capture_sink sink) noexcept
{
	return sink;
}

inline constexpr lock_proxy output_stream_mutex_ref_define(locked_capture_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr capture_sink output_stream_unlocked_ref_define(locked_capture_sink sink) noexcept
{
	return sink.unlocked;
}

inline void write_all_overflow_define(
	capture_sink sink, char const *first, char const *last) noexcept
{
	++*sink.write_calls;
	sink.storage->append(first, last);
}

inline void scatter_write_all_overflow_define(
	capture_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count) noexcept
{
	++*sink.scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.storage->append(scatters[index].base, scatters[index].len);
	}
}

struct locale_dynamic_text
{
	protocol_counts *counts;
	char const *text;
	::std::size_t size;
};

struct locale_const_text
{
	const_protocol_counts *counts;
};

inline ::std::size_t print_reserve_size(
	::fast_io::lc_all const *, locale_const_text const &value) noexcept
{
	++value.counts->sizes;
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::lc_all const *, char *destination, locale_const_text const &value) noexcept
{
	++value.counts->defines;
	*destination++ = 'K';
	return destination;
}

inline void print_define(
	::fast_io::lc_all const *, capture_sink sink, locale_const_text const &value) noexcept
{
	++value.counts->directs;
	static constexpr char character{'K'};
	write_all_overflow_define(sink, __builtin_addressof(character),
						  __builtin_addressof(character) + 1u);
}

inline ::std::size_t print_define_internal_shift(
	::fast_io::lc_all const *, locale_const_text const &value) noexcept
{
	++value.counts->shifts;
	return 1u;
}

struct locale_ordered_dynamic
{
	::std::size_t ordinal;
	::std::size_t *size_order;
	::std::size_t *define_order;
	char value;
};

inline ::std::size_t print_reserve_size(
	::fast_io::lc_all const *, locale_ordered_dynamic const &value) noexcept
{
	if (*value.size_order != value.ordinal)
	{
		::std::abort();
	}
	++*value.size_order;
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::lc_all const *, char *destination,
	locale_ordered_dynamic const &value) noexcept
{
	if (*value.define_order != value.ordinal)
	{
		::std::abort();
	}
	++*value.define_order;
	*destination++ = value.value;
	return destination;
}

inline ::std::size_t print_reserve_size(
	::fast_io::lc_all const *, locale_dynamic_text value) noexcept
{
	++value.counts->locale_size_calls;
	return value.size;
}

inline char *print_reserve_define(
	::fast_io::lc_all const *, char *destination, locale_dynamic_text value) noexcept
{
	++value.counts->locale_define_calls;
	for (::std::size_t index{}; index != value.size; ++index)
	{
		destination[index] = value.text[index];
	}
	return destination + value.size;
}

struct locale_scatter_text
{
	protocol_counts *counts;
	char const *text;
	::std::size_t size;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::lc_all const *, locale_scatter_text value) noexcept
{
	++value.counts->locale_scatter_calls;
	return {value.text, value.size};
}

inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, locale_scatter_text>) noexcept
{
	// `locale_scatter_text` points at caller-owned immutable text whose lifetime covers every synchronous test call.
	return {};
}

struct locale_scratch_scatter
{
	char value;
	char *shared_scratch;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::lc_all const *, locale_scratch_scatter value) noexcept
{
	*value.shared_scratch = value.value;
	return {value.shared_scratch, 1u};
}

struct locale_rotating_stable_scatter
{
	char const *first;
	char const *second;
	::std::size_t *observations;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::lc_all const *, locale_rotating_stable_scatter value) noexcept
{
	char const *selected{((*value.observations)++ & 1u) == 0u ? value.first : value.second};
	return {selected, 1u};
}

struct locale_owned_self_scatter
{
	::std::array<char, 3u> text;
	char const **observed_base;

	inline constexpr locale_owned_self_scatter(
		::std::array<char, 3u> value, char const **observed) noexcept
		: text(value), observed_base(observed)
	{}
	locale_owned_self_scatter(locale_owned_self_scatter const &) = delete;
	locale_owned_self_scatter &operator=(locale_owned_self_scatter const &) = delete;
	locale_owned_self_scatter(locale_owned_self_scatter &&) = default;
	locale_owned_self_scatter &operator=(locale_owned_self_scatter &&) = default;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::lc_all const *, locale_owned_self_scatter &value) noexcept
{
	*value.observed_base = value.text.data();
	return {value.text.data(), value.text.size()};
}

inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, locale_owned_self_scatter>) noexcept
{
	// The descriptor names the unchanged owner's fixed array. Re-observation returns the identical range, and the
	// enclosing pack/parameter owns that array through both contiguous passes.
	return {};
}

struct ordinary_move_only_text
{
	::std::array<char, 3u> text;
	ordinary_move_only_text const **observed_owner;
	::std::size_t *size_calls;
	::std::size_t *define_calls;

	ordinary_move_only_text(
		::std::array<char, 3u> value, ordinary_move_only_text const **observed,
		::std::size_t *sizes, ::std::size_t *defines) noexcept
		: text(value), observed_owner(observed), size_calls(sizes), define_calls(defines)
	{}
	ordinary_move_only_text(ordinary_move_only_text const &) = delete;
	ordinary_move_only_text &operator=(ordinary_move_only_text const &) = delete;
	ordinary_move_only_text(ordinary_move_only_text &&) = default;
	ordinary_move_only_text &operator=(ordinary_move_only_text &&) = default;
};

inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, ordinary_move_only_text>,
	ordinary_move_only_text &value) noexcept
{
	*value.observed_owner = __builtin_addressof(value);
	++*value.size_calls;
	return value.text.size();
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, ordinary_move_only_text>, char *destination,
	ordinary_move_only_text &value) noexcept
{
	*value.observed_owner = __builtin_addressof(value);
	++*value.define_calls;
	for (char character : value.text)
	{
		*destination++ = character;
	}
	return destination;
}

struct ordinary_scratch_scatter
{
	char value;
	char *scratch;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordinary_scratch_scatter>,
	ordinary_scratch_scatter &value) noexcept
{
	*value.scratch = value.value;
	return {value.scratch, 1u};
}

struct ordinary_stable_scatter
{
	char const *text;
	::std::size_t size;
};

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, ordinary_stable_scatter>,
	ordinary_stable_scatter &value) noexcept
{
	return {value.text, value.size};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, ordinary_stable_scatter>) noexcept
{
	return {};
}

struct locale_status_counts
{
	::std::size_t forwards{};
	::std::size_t sizes{};
	::std::size_t defines{};
	void const *measured_owner{};
};

struct locale_status_proxy
{
	locale_status_counts *counts;
	::std::array<char, 3u> text;

	locale_status_proxy(locale_status_counts *value, ::std::array<char, 3u> characters) noexcept
		: counts(value), text(characters)
	{}
	locale_status_proxy(locale_status_proxy const &) = delete;
	locale_status_proxy &operator=(locale_status_proxy const &) = delete;
	locale_status_proxy(locale_status_proxy &&) = default;
	locale_status_proxy &operator=(locale_status_proxy &&) = default;
};

inline ::std::size_t print_reserve_size(
	::fast_io::lc_all const *, locale_status_proxy &value) noexcept
{
	++value.counts->sizes;
	value.counts->measured_owner = __builtin_addressof(value);
	return value.text.size();
}

inline char *print_reserve_define(
	::fast_io::lc_all const *, char *destination, locale_status_proxy &value) noexcept
{
	if (value.counts->measured_owner != __builtin_addressof(value))
	{
		::std::abort();
	}
	++value.counts->defines;
	for (char character : value.text)
	{
		*destination++ = character;
	}
	return destination;
}

struct locale_status_source
{
	locale_status_counts *counts;

	locale_status_source(locale_status_counts *value) noexcept : counts(value) {}
	locale_status_source(locale_status_source const &) = delete;
	locale_status_source &operator=(locale_status_source const &) = delete;
	locale_status_source(locale_status_source &&) = default;
	locale_status_source &operator=(locale_status_source &&) = default;
};

inline locale_status_proxy status_io_print_forward(
	::fast_io::io_alias_type_t<char>, locale_status_source &source) noexcept
{
	++source.counts->forwards;
	return {source.counts, {'F', 'W', 'D'}};
}

struct locale_status_lvalue_source
{
	locale_status_counts *counts;
	locale_status_proxy *proxy;
};

inline locale_status_proxy &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, locale_status_lvalue_source &source) noexcept
{
	++source.counts->forwards;
	return *source.proxy;
}

struct actual_direct_text
{
	protocol_counts *counts;
};

// This overload intentionally accepts only the concrete test destination. It proves that locale dispatch does not
// require the historical dummy sink when the expression used by the real output object is valid.
inline void print_define(
	::fast_io::lc_all const *, capture_sink sink, actual_direct_text value) noexcept
{
	++value.counts->actual_direct_calls;
	static constexpr char text[]{'D', 'I', 'R'};
	write_all_overflow_define(sink, text, text + sizeof(text));
}

struct identity_direct_text
{
	identity_direct_text() = default;
	identity_direct_text(identity_direct_text const &) = delete;
	identity_direct_text &operator=(identity_direct_text const &) = delete;
};

inline void print_define(
	::fast_io::lc_all const *, identity_observer &out, identity_direct_text &) noexcept
{
	++out.direct_calls;
}

struct concat_direct_text
{
	protocol_counts *counts;
};

using string_output_ref = decltype(
	::fast_io::io_strlike_ref(::fast_io::io_alias, ::std::declval<::std::string &>()));

// Concat is also an output-specific direct protocol: its customization is valid for the actual string reference and
// deliberately absent for both the dummy sink and unrelated output objects.
inline void print_define(
	::fast_io::lc_all const *, string_output_ref out, concat_direct_text value)
{
	++value.counts->concat_direct_calls;
	static constexpr char text[]{'C', 'D'};
	::fast_io::operations::decay::write_all_decay(out, text, text + sizeof(text));
}

struct dummy_only_text
{
	protocol_counts *counts;
};

// A dummy-only locale overload must remain visible through the compatibility concept but must never pre-empt the
// ordinary reserve protocol when the concrete output cannot call it.
inline void print_define(
	::fast_io::lc_all const *, ::fast_io::details::dummy_buffer_output_stream<char>,
	dummy_only_text value) noexcept
{
	++value.counts->dummy_direct_calls;
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dummy_only_text>) noexcept
{
	return 3u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dummy_only_text>, char *destination,
	dummy_only_text) noexcept
{
	*destination++ = 'O';
	*destination++ = 'R';
	*destination++ = 'D';
	return destination;
}

struct ordinary_reserve_text
{};

struct construct_only_string
{
	using value_type = char;
	::std::string value;
};

struct cursor_only_string
{
	using value_type = char;
	::std::array<char, 64u> storage{};
	::std::size_t size{};
};

inline constexpr char *strlike_begin(
	::fast_io::io_strlike_type_t<char, cursor_only_string>, cursor_only_string &text) noexcept
{
	return text.storage.data();
}

inline constexpr char *strlike_curr(
	::fast_io::io_strlike_type_t<char, cursor_only_string>, cursor_only_string &text) noexcept
{
	return text.storage.data() + text.size;
}

inline constexpr char *strlike_end(
	::fast_io::io_strlike_type_t<char, cursor_only_string>, cursor_only_string &text) noexcept
{
	return text.storage.data() + text.storage.size();
}

inline constexpr void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, cursor_only_string>, cursor_only_string &text,
	char *current) noexcept
{
	text.size = static_cast<::std::size_t>(current - text.storage.data());
}

inline constexpr void strlike_reserve(
	::fast_io::io_strlike_type_t<char, cursor_only_string>, cursor_only_string &text,
	::std::size_t requested) noexcept
{
	if (requested > text.storage.size())
	{
		::std::abort();
	}
}

struct misleading_string_ref
{
	using output_char_type = char;
};

struct construct_with_misleading_ref
{
	using value_type = char;
	::std::string value;
};

inline constexpr misleading_string_ref io_strlike_ref(
	::fast_io::io_alias_t, construct_with_misleading_ref &) noexcept
{
	return {};
}

inline construct_with_misleading_ref strlike_construct_define(
	::fast_io::io_strlike_type_t<char, construct_with_misleading_ref>,
	char const *first, char const *last)
{
	return {::std::string(first, last)};
}

template <typename T>
concept has_char_range_construction = requires(char const *first) {
	strlike_construct_define(::fast_io::io_strlike_type<char, T>, first, first);
};

struct staging_locale_only_text
{};

using locale_concat_buffer_ref = ::fast_io::io_strlike_reference_wrapper<
	char, ::fast_io::details::basic_concat_buffer<char>>;

inline void print_define(
	::fast_io::lc_all const *, locale_concat_buffer_ref output,
	staging_locale_only_text)
{
	static constexpr char text[]{'S', 'T', 'G'};
	::fast_io::operations::decay::write_all_decay(output, text, text + sizeof(text));
}

inline construct_only_string strlike_construct_define(
	::fast_io::io_strlike_type_t<char, construct_only_string>,
	char const *first, char const *last)
{
	return {::std::string(first, last)};
}

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, ordinary_reserve_text>) noexcept
{
	return 3u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, ordinary_reserve_text>, char *destination,
	ordinary_reserve_text) noexcept
{
	*destination++ = 'R';
	*destination++ = 'S';
	*destination++ = 'V';
	return destination;
}

static_assert(::fast_io::lc_dynamic_reserve_printable<char, locale_dynamic_text>);
static_assert(::fast_io::lc_scatter_printable<char, locale_scatter_text>);
static_assert(::fast_io::lc_borrowed_scatter_source<char, locale_scatter_text>);
static_assert(::fast_io::lc_scatter_printable<char, locale_scratch_scatter>);
static_assert(!::fast_io::lc_borrowed_scatter_source<char, locale_scratch_scatter>);
static_assert(::fast_io::lc_scatter_printable<char, locale_rotating_stable_scatter>);
static_assert(!::fast_io::lc_borrowed_scatter_source<char, locale_rotating_stable_scatter>);
static_assert(::fast_io::lc_scatter_printable<char, locale_owned_self_scatter &>);
static_assert(::fast_io::lc_borrowed_scatter_source<char, locale_owned_self_scatter &>);
using scratch_pack_type = ::fast_io::manipulators::pack_t<
	locale_scratch_scatter, locale_scratch_scatter>;
using rotating_pack_type = ::fast_io::manipulators::pack_t<
	locale_rotating_stable_scatter, locale_rotating_stable_scatter>;
using scratch_condition_type = ::fast_io::manipulators::condition<
	locale_scratch_scatter, locale_scratch_scatter>;
using rotating_condition_type = ::fast_io::manipulators::condition<
	locale_rotating_stable_scatter, locale_rotating_stable_scatter>;
using stable_condition_type = ::fast_io::manipulators::condition<
	locale_scatter_text, locale_scatter_text>;
using ordinary_scratch_pack_type = ::fast_io::manipulators::pack_t<
	ordinary_scratch_scatter, ordinary_scratch_scatter>;
using ordinary_stable_pack_type = ::fast_io::manipulators::pack_t<
	ordinary_stable_scatter, ordinary_stable_scatter>;
static_assert(!::fast_io::lc_dynamic_reserve_printable<char, scratch_pack_type &>);
static_assert(!::fast_io::lc_dynamic_reserve_printable<char, rotating_pack_type &>);
static_assert(::fast_io::lc_scatter_printable<char, scratch_condition_type &>);
static_assert(::fast_io::lc_scatter_printable<char, rotating_condition_type &>);
static_assert(!::fast_io::lc_borrowed_scatter_source<char, scratch_condition_type &>);
static_assert(!::fast_io::lc_borrowed_scatter_source<char, rotating_condition_type &>);
static_assert(!::fast_io::lc_dynamic_reserve_printable<char, scratch_condition_type &>);
static_assert(!::fast_io::lc_dynamic_reserve_printable<char, rotating_condition_type &>);
static_assert(::fast_io::lc_borrowed_scatter_source<char, stable_condition_type &>);
static_assert(::fast_io::lc_dynamic_reserve_printable<char, stable_condition_type &>);
static_assert(!::fast_io::lc_dynamic_reserve_printable<char, ordinary_scratch_pack_type &>);
static_assert(::fast_io::lc_dynamic_reserve_printable<char, ordinary_stable_pack_type &>);
using const_parameter_type = ::fast_io::parameter<locale_const_text>;
static_assert(::fast_io::lc_dynamic_reserve_printable<char, const_parameter_type const &>);
static_assert(::fast_io::details::lc_direct_printable_to<
	char, capture_sink, const_parameter_type const &>);
static_assert(::fast_io::lc_printable_internal_shift<char, const_parameter_type const &>);
static_assert(::fast_io::dynamic_reserve_printable<char, ordinary_move_only_text &>);
using locale_boolalpha_type = decltype(::fast_io::mnp::boolalpha(true));
using locale_am_pm_type = decltype(::fast_io::mnp::am_pm(false));
static_assert(::fast_io::lc_borrowed_scatter_source<char, locale_boolalpha_type>);
static_assert(::fast_io::lc_borrowed_scatter_source<char, locale_am_pm_type>);
static_assert(!::fast_io::lc_printable<char, actual_direct_text>);
static_assert(::fast_io::details::lc_direct_printable_to<char, capture_sink, actual_direct_text>);
static_assert(::fast_io::details::lc_direct_printable_to<
	char, identity_observer, identity_direct_text>);
static_assert(::fast_io::lc_printable<char, dummy_only_text>);
static_assert(!::fast_io::details::lc_direct_printable_to<char, capture_sink, dummy_only_text>);
static_assert(!::fast_io::lc_printable<char, concat_direct_text>);
static_assert(
	::fast_io::details::lc_direct_printable_to<char, string_output_ref, concat_direct_text>);
static_assert(::fast_io::strlike<char, construct_only_string>);
static_assert(!::fast_io::auxiliary_strlike<char, construct_only_string>);
static_assert(::fast_io::buffer_strlike<char, cursor_only_string>);
static_assert(!has_char_range_construction<cursor_only_string>);
static_assert(::fast_io::strlike<char, construct_with_misleading_ref>);
static_assert(!::fast_io::details::decay::lc_concat_custom_destination_ok<
	false, char, construct_with_misleading_ref, staging_locale_only_text>);
static_assert(!::fast_io::details::decay::lc_concat_custom_destination_ok<
	false, char, construct_with_misleading_ref, ordinary_reserve_text>);
static_assert(::fast_io::details::decay::lc_status_print_output_run_okay<
	false, char, locale_concat_buffer_ref, staging_locale_only_text>);
static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_capture_sink>);

[[noreturn]] inline void fail() noexcept
{
	::std::abort();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		fail();
	}
}

} // namespace locale_strategy_test

int main()
{
	using namespace locale_strategy_test;

	::fast_io::lc_object locale{};
	locale.all.messages.yesstr = ::fast_io::basic_scatter<char>::append_range(
		locale.data_storage.chars, ::std::string_view{"yes"});
	locale.all.messages.nostr = ::fast_io::basic_scatter<char>::append_range(
		locale.data_storage.chars, ::std::string_view{"no"});
	locale.all.time.am_pm[0u] = ::fast_io::basic_scatter<char>::append_range(
		locale.data_storage.chars, ::std::string_view{"AM"});
	locale.all.time.am_pm[1u] = ::fast_io::basic_scatter<char>::append_range(
		locale.data_storage.chars, ::std::string_view{"PM"});

	// Locale facets store relative offsets. Every complete-object copy or move must rebind the explicit storage owner;
	// otherwise resolution would silently read the source object's vectors after the destination had been detached.
	::fast_io::lc_object copied_locale{locale};
	require(copied_locale.all.data_storage == __builtin_addressof(copied_locale.data_storage));
	require(copied_locale.all.data_storage != __builtin_addressof(locale.data_storage));
	auto const copied_yes{print_scatter_define(
		__builtin_addressof(copied_locale.all), ::fast_io::mnp::boolalpha(true))};
	require(copied_yes.base == copied_locale.data_storage.chars.data() +
							 copied_locale.all.messages.yesstr.rva);
	require(copied_yes.base != locale.data_storage.chars.data() +
						 locale.all.messages.yesstr.rva);

	::fast_io::lc_object copy_assigned_locale;
	copy_assigned_locale = locale;
	require(copy_assigned_locale.all.data_storage ==
			__builtin_addressof(copy_assigned_locale.data_storage));
	require(copy_assigned_locale.all.data_storage != __builtin_addressof(locale.data_storage));

	::fast_io::lc_object move_source{locale};
	::fast_io::lc_object moved_locale{::std::move(move_source)};
	require(moved_locale.all.data_storage == __builtin_addressof(moved_locale.data_storage));
	require(moved_locale.all.data_storage != __builtin_addressof(move_source.data_storage));
	auto const moved_yes{print_scatter_define(
		__builtin_addressof(moved_locale.all), ::fast_io::mnp::boolalpha(true))};
	require(moved_yes.base == moved_locale.data_storage.chars.data() +
						   moved_locale.all.messages.yesstr.rva);

	::fast_io::lc_object move_assign_source{locale};
	::fast_io::lc_object move_assigned_locale;
	move_assigned_locale = ::std::move(move_assign_source);
	require(move_assigned_locale.all.data_storage ==
			__builtin_addressof(move_assigned_locale.data_storage));
	require(move_assigned_locale.all.data_storage !=
			__builtin_addressof(move_assign_source.data_storage));
	protocol_counts counts{};
	locale_dynamic_text dynamic{__builtin_addressof(counts), "DYN", 3u};
	locale_scatter_text scatter{__builtin_addressof(counts), "SCT", 3u};
	dummy_only_text dummy{__builtin_addressof(counts)};

	::std::string output;
	::std::size_t write_calls{};
	::std::size_t scatter_calls{};
	capture_sink sink{__builtin_addressof(output), __builtin_addressof(write_calls),
					  __builtin_addressof(scatter_calls)};
	lock_state synchronization{};
	locked_capture_sink locked_sink{sink, __builtin_addressof(synchronization)};

	// Locale parameter adapters must mirror the const member expression of their sole owner across reserve, direct,
	// and internal-placement protocols. A mutable-only bridge here would make the concept probe and executed call
	// disagree and could mutate a formatter that the enclosing strategy deliberately exposed as const.
	const_protocol_counts const_counts{};
	const const_parameter_type const_parameter{locale_const_text{__builtin_addressof(const_counts)}};
	char const_buffer[1u]{};
	require(print_reserve_size(__builtin_addressof(locale.all), const_parameter) == 1u);
	require(print_reserve_define(
			__builtin_addressof(locale.all), const_buffer, const_parameter) ==
		const_buffer + 1u);
	require(const_buffer[0u] == 'K');
	require(print_define_internal_shift(
			__builtin_addressof(locale.all), const_parameter) == 1u);
	print_define(__builtin_addressof(locale.all), sink, const_parameter);
	require(output == "K");
	require(const_counts.sizes == 1u && const_counts.defines == 1u &&
			const_counts.directs == 1u && const_counts.shifts == 1u);
	output.clear();

	// Locale dispatch receives already-normalized ordinary leaves as well as locale bridges. The unchanged move-only
	// owner must be borrowed through the final ordinary strategy entry; copying it is neither an ABI optimization nor a
	// lifetime operation. The lvalue address additionally proves that parameter unwrapping preserves identity.
	ordinary_move_only_text const *observed_ordinary{};
	::std::size_t ordinary_sizes{};
	::std::size_t ordinary_defines{};
	ordinary_move_only_text ordinary_lvalue{
		{'L', 'V', 'L'}, __builtin_addressof(observed_ordinary),
		__builtin_addressof(ordinary_sizes), __builtin_addressof(ordinary_defines)};
	::fast_io::print(::fast_io::imbue(locale, sink), ordinary_lvalue);
	require(output == "LVL");
	require(observed_ordinary == __builtin_addressof(ordinary_lvalue));
	require(ordinary_sizes != 0u && ordinary_defines != 0u);
	output.clear();

	observed_ordinary = nullptr;
	ordinary_sizes = ordinary_defines = 0u;
	::fast_io::print(
		::fast_io::imbue(locale, sink),
		ordinary_move_only_text{
			{'R', 'V', 'L'}, __builtin_addressof(observed_ordinary),
			__builtin_addressof(ordinary_sizes), __builtin_addressof(ordinary_defines)});
	require(output == "RVL");
	require(observed_ordinary != nullptr);
	output.clear();

	// `basic_lc_concat_decay` is itself an ownership boundary. Its implementation phase must borrow that owner instead
	// of passing the named argument pack into a second by-value helper.
	observed_ordinary = nullptr;
	ordinary_sizes = ordinary_defines = 0u;
	auto normalized_move_only{::fast_io::io_print_forward<char>(
		::fast_io::io_print_alias(ordinary_move_only_text{
			{'D', 'E', 'C'}, __builtin_addressof(observed_ordinary),
			__builtin_addressof(ordinary_sizes), __builtin_addressof(ordinary_defines)}))};
	require(::fast_io::basic_lc_concat_decay<::std::string>(
			__builtin_addressof(locale.all), ::std::move(normalized_move_only)) == "DEC");
	require(observed_ordinary != nullptr);

	// A nested pack stores aliases before character-aware status forwarding. Forwarding its move-only locale proxy in
	// both reserve passes would create two owners; delaying one forwarding operation into an owned locale bridge keeps
	// locale context and makes size/define observe the identical proxy.
	locale_status_counts status_counts{};
	auto status_nested{::fast_io::mnp::right(
		::fast_io::mnp::pack(locale_status_source{__builtin_addressof(status_counts)}),
		8u, '.')};
	::fast_io::print(::fast_io::imbue(locale, sink), status_nested);
	require(output == ".....FWD");
	require(status_counts.forwards == 1u && status_counts.sizes != 0u &&
			status_counts.defines != 0u);
	output.clear();

	// A forwarding CPO may instead return a stable noncopyable lvalue proxy. The deferred adapter propagates that exact
	// identity through a pointer bridge and never attempts to own or copy the referent.
	locale_status_counts lvalue_status_counts{};
	locale_status_proxy stable_status_proxy{
		__builtin_addressof(lvalue_status_counts), {'R', 'E', 'F'}};
	auto lvalue_status_nested{::fast_io::mnp::right(
		::fast_io::mnp::pack(locale_status_lvalue_source{
			__builtin_addressof(lvalue_status_counts), __builtin_addressof(stable_status_proxy)}),
		8u, '.')};
	::fast_io::print(::fast_io::imbue(locale, sink), lvalue_status_nested);
	require(output == ".....REF");
	require(lvalue_status_counts.forwards == 1u);
	require(lvalue_status_counts.measured_owner == __builtin_addressof(stable_status_proxy));
	output.clear();

	// The two audited built-in locale scatter families can form one retained descriptor run. This is the positive
	// counterpart to the rotating-source negative case: one native scatter call emits four stable locale descriptors.
	write_calls = 0u;
	::fast_io::print(
		::fast_io::imbue(locale, sink), ::fast_io::mnp::boolalpha(true),
		::fast_io::mnp::boolalpha(false), ::fast_io::mnp::am_pm(false),
		::fast_io::mnp::am_pm(true));
	require(output == "yesnoAMPM");
	require(scatter_calls == 1u);
	require(write_calls == 0u);
	output.clear();
	scatter_calls = 0u;

	// `imbue` must preserve a stable noncopyable observer reference through its status continuation and locale direct
	// bridge. Any value reconstruction in that chain would either be ill-formed or update a discarded surrogate.
	identity_output identity;
	::fast_io::print(::fast_io::imbue(locale, identity), identity_direct_text{});
	require(identity.observer.direct_calls == 1u);

	// Owning parameter adapters must not return a descriptor into an adapter-local copy of the formatter.
	char const *observed_owned_base{};
	::fast_io::parameter<locale_owned_self_scatter> owned_scatter{
		locale_owned_self_scatter{{'O', 'W', 'N'}, __builtin_addressof(observed_owned_base)}};
	auto const owned_descriptor{print_scatter_define(__builtin_addressof(locale.all), owned_scatter)};
	require(observed_owned_base == owned_scatter.reference.text.data());
	require(owned_descriptor.base == owned_scatter.reference.text.data());

	// Locale condition and nested-pack adapters also borrow the semantic owner. These move-only branches made the old
	// by-value adapters ill-formed; the address checks additionally prove that returned scatters name the stored branch.
	char const *condition_first_base{};
	char const *condition_second_base{};
	::fast_io::manipulators::condition<locale_owned_self_scatter, locale_owned_self_scatter>
		owned_condition{
			true,
			locale_owned_self_scatter{{'C', 'N', 'D'}, __builtin_addressof(condition_first_base)},
			locale_owned_self_scatter{{'B', 'A', 'D'}, __builtin_addressof(condition_second_base)}};
	auto const condition_descriptor{
		print_scatter_define(__builtin_addressof(locale.all), owned_condition)};
	require(condition_descriptor.base == owned_condition.t1.text.data());
	require(condition_first_base == owned_condition.t1.text.data());
	require(condition_second_base == nullptr);

	char const *pack_first_base{};
	char const *pack_second_base{};
	auto owned_pack{::fast_io::mnp::pack(
		locale_owned_self_scatter{{'P', 'A', 'K'}, __builtin_addressof(pack_first_base)},
		locale_owned_self_scatter{{'T', 'W', 'O'}, __builtin_addressof(pack_second_base)})};
	char pack_output[6u]{};
	require(print_reserve_size(__builtin_addressof(locale.all), owned_pack) == 6u);
	auto const pack_end{print_reserve_define(
		__builtin_addressof(locale.all), pack_output, owned_pack)};
	require(pack_end == pack_output + 6u);
	require(::std::string_view(pack_output, 6u) == "PAKTWO");
	auto &stored_pack_first{::fast_io::containers::get<0u>(owned_pack.storage)};
	auto &stored_pack_second{::fast_io::containers::get<1u>(owned_pack.storage)};
	require(pack_first_base == stored_pack_first.text.data());
	require(pack_second_base == stored_pack_second.text.data());

	// Size observation order is part of a stateful producer's semantics. A single expression that passes head and tail
	// sizes as two function arguments leaves their evaluation order unspecified; the locale pack implementation instead
	// completes each head observation before recursing into the tail.
	::std::size_t ordered_sizes{};
	::std::size_t ordered_defines{};
	auto ordered_pack{::fast_io::mnp::pack(
		locale_ordered_dynamic{
			0u, __builtin_addressof(ordered_sizes), __builtin_addressof(ordered_defines), 'A'},
		locale_ordered_dynamic{
			1u, __builtin_addressof(ordered_sizes), __builtin_addressof(ordered_defines), 'B'})};
	char ordered_output[2u]{};
	require(print_reserve_size(__builtin_addressof(locale.all), ordered_pack) == 2u);
	require(ordered_sizes == 2u);
	require(print_reserve_define(
			__builtin_addressof(locale.all), ordered_output, ordered_pack) ==
		ordered_output + 2u);
	require(ordered_defines == 2u);
	require(::std::string_view(ordered_output, 2u) == "AB");

	// The ordinary-scatter half of a mixed locale pack obeys the same proof rule. Immutable external storage retains the
	// two-pass optimization, while `ordinary_scratch_pack_type` above is concept-negative because another observation
	// can overwrite its shared byte.
	ordinary_stable_pack_type ordinary_stable_pack{
		typename ordinary_stable_pack_type::storage_type{
			ordinary_stable_scatter{"S", 1u}, ordinary_stable_scatter{"T", 1u}}};
	char ordinary_stable_output[2u]{};
	require(print_reserve_size(
			__builtin_addressof(locale.all), ordinary_stable_pack) == 2u);
	require(print_reserve_define(
			__builtin_addressof(locale.all), ordinary_stable_output,
			ordinary_stable_pack) == ordinary_stable_output + 2u);
	require(::std::string_view(ordinary_stable_output, 2u) == "ST");

	// Both rotating buffers outlive the operation, but observation is deliberately non-repeatable. The negative marker
	// assertion above proves that lifetime alone cannot admit retained two-pass locale composition.
	::std::size_t rotating_observations{};
	locale_rotating_stable_scatter rotating{"A", "B", __builtin_addressof(rotating_observations)};
	auto const rotating_first{print_scatter_define(__builtin_addressof(locale.all), rotating)};
	auto const rotating_second{print_scatter_define(__builtin_addressof(locale.all), rotating)};
	require(rotating_first.base != rotating_second.base);

	::fast_io::print(::fast_io::imbue(locale, sink), dynamic, scatter,
		actual_direct_text{__builtin_addressof(counts)}, dummy, ordinary_reserve_text{});
	require(output == "DYNSCTDIRORDRSV");

	output.clear();
	::fast_io::print(::fast_io::imbue(locale, locked_sink), ordinary_reserve_text{});
	require(output == "RSV");
	require(synchronization.locks == 1u && synchronization.unlocks == 1u &&
			!synchronization.locked);

	output.clear();
	char locale_scratch{};
	locale_scratch_scatter scratch_a{'A', __builtin_addressof(locale_scratch)};
	locale_scratch_scatter scratch_b{'B', __builtin_addressof(locale_scratch)};
	::fast_io::print(::fast_io::imbue(locale, sink), scratch_a, scratch_b);
	require(output == "AB");
	require(::fast_io::lc_concat(__builtin_addressof(locale.all), scratch_a, scratch_b) == "AB");
	output.clear();
	// Nesting does not turn shared scratch into a repeatable producer. The negative pack/condition concepts above force
	// the semantic engine to retain the immediate one-observation leaves rather than inventing a size/define pair.
	auto scratch_nested{::fast_io::mnp::right(
		::fast_io::mnp::pack(scratch_a, scratch_b), 2u, '.')};
	::fast_io::print(::fast_io::imbue(locale, sink), scratch_nested);
	require(output == "AB");
	output.clear();
	::fast_io::print(
		::fast_io::imbue(locale, sink), ::fast_io::mnp::cond(true, scratch_a, scratch_b));
	require(output == "A");

	output.clear();
	auto semantic_record{::fast_io::mnp::pack(
		"<", ::fast_io::mnp::cond(true, dynamic, locale_dynamic_text{__builtin_addressof(counts), "BAD", 3u}),
		"|", ::fast_io::mnp::right(scatter, 5u, '.'), ">")};
	::fast_io::print(::fast_io::imbue(locale, sink), semantic_record);
	require(output == "<DYN|..SCT>");

	output.clear();
	// The pack is nested below width and therefore cannot be flattened without changing padding semantics. The locale
	// pack reserve protocol measures and materializes its named children in the same order.
	auto nested_width{::fast_io::mnp::right(::fast_io::mnp::pack(dynamic, scatter), 8u, '.')};
	::fast_io::println(::fast_io::imbue(locale, sink), nested_width);
	require(output == "..DYNSCT\n");

	auto const concatenated{::fast_io::lc_concat(
		__builtin_addressof(locale.all), "{", dynamic, scatter,
		concat_direct_text{__builtin_addressof(counts)}, dummy, ordinary_reserve_text{}, "}")};
	require(concatenated == "{DYNSCTCDORDRSV}");
	require(::fast_io::lc_concat(__builtin_addressof(locale.all), semantic_record) == "<DYN|..SCT>");
	require(::fast_io::lc_concatln(__builtin_addressof(locale.all), nested_width) == "..DYNSCT\n");
	auto const construct_only{::fast_io::basic_lc_concat<construct_only_string>(
		__builtin_addressof(locale.all), "[", dynamic, scatter, ordinary_reserve_text{}, "]")};
	require(construct_only.value == "[DYNSCTRSV]");

	// Cursor syntax and range construction are independent destination proofs. The first result deliberately provides
	// no `io_strlike_ref` and no construct CPO, so locale concat must instantiate the maintained generic cursor adapter.
	// The second advertises an unusable custom adapter; exact run admission must reject it and retain the staging route.
	auto const cursor_only{::fast_io::basic_lc_concat<cursor_only_string>(
		__builtin_addressof(locale.all), "CUR")};
	require(::std::string_view(cursor_only.storage.data(), cursor_only.size) == "CUR");
	auto const staged_misleading{
		::fast_io::basic_lc_concat<construct_with_misleading_ref>(
			__builtin_addressof(locale.all), staging_locale_only_text{})};
	require(staged_misleading.value == "STG");
	auto const staged_misleading_ordinary{
		::fast_io::basic_lc_concat<construct_with_misleading_ref>(
			__builtin_addressof(locale.all), ordinary_reserve_text{})};
	require(staged_misleading_ordinary.value == "RSV");

	require(counts.locale_size_calls != 0u);
	require(counts.locale_define_calls != 0u);
	require(counts.locale_scatter_calls != 0u);
	require(counts.actual_direct_calls == 1u);
	require(counts.concat_direct_calls == 1u);
	require(counts.dummy_direct_calls == 0u);
	require(write_calls + scatter_calls != 0u);
}
