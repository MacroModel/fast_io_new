#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

template <::std::size_t extent>
struct fixed_target;

template <::std::size_t extent>
struct fixed_proxy
{
	fixed_target<extent> *target;
};

template <::std::size_t extent>
struct fixed_target
{
	::std::array<char, extent> value{};
	::std::size_t calls{};
};

template <::std::size_t extent>
inline constexpr fixed_proxy<extent> scan_alias_define(::fast_io::io_alias_t, fixed_target<extent> &target) noexcept
{
	return {__builtin_addressof(target)};
}

template <::std::size_t extent>
inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, fixed_proxy<extent>>) noexcept
{
	return extent;
}

template <::std::size_t extent>
inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_proxy<extent>>, char const *buffer, fixed_proxy<extent> &proxy) noexcept
{
	for (::std::size_t i{}; i != extent; ++i)
	{
		proxy.target->value[i] = buffer[i];
	}
	++proxy.target->calls;
}

struct fallible_target
{
	char value{};
	::std::size_t calls{};
};

struct fallible_proxy
{
	fallible_target *target;
};

inline constexpr fallible_proxy scan_alias_define(::fast_io::io_alias_t, fallible_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, fallible_proxy>) noexcept
{
	return 1u;
}

inline constexpr ::fast_io::parse_code scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, fallible_proxy>, char const *buffer, fallible_proxy &proxy) noexcept
{
	if (*buffer == '#')
	{
		return ::fast_io::parse_code::end_of_file;
	}
	if (*buffer == '!')
	{
		return ::fast_io::parse_code::invalid;
	}
	proxy.target->value = *buffer;
	++proxy.target->calls;
	return ::fast_io::parse_code::ok;
}

inline constexpr ::std::size_t large_extent{1024u * 1024u};

struct large_probe_target
{
	bool matched{};
	::std::size_t calls{};
};

struct large_probe_proxy
{
	large_probe_target *target;
};

inline constexpr large_probe_proxy scan_alias_define(::fast_io::io_alias_t, large_probe_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, large_probe_proxy>) noexcept
{
	return large_extent;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, large_probe_proxy>, char const *buffer, large_probe_proxy &proxy) noexcept
{
	proxy.target->matched = buffer[0] == 'x' && buffer[large_extent - 1u] == 'x';
	++proxy.target->calls;
}

struct reference_proxy;

struct reference_alias_target
{
	char value{};
	reference_proxy *proxy_ptr{};
};

struct reference_proxy
{
	reference_alias_target *target;

	inline explicit constexpr reference_proxy(reference_alias_target *target_address) noexcept
		: target(target_address)
	{}

	reference_proxy(reference_proxy const &) = delete;
	reference_proxy &operator=(reference_proxy const &) = delete;
	reference_proxy(reference_proxy &&) = delete;
	reference_proxy &operator=(reference_proxy &&) = delete;
};

inline reference_proxy &scan_alias_define(::fast_io::io_alias_t, reference_alias_target &target) noexcept
{
	return *target.proxy_ptr;
}

inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, reference_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, reference_proxy>, char const *buffer, reference_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
}

struct chunked_source
{
	using input_char_type = char;
	::std::string_view source;
	::std::size_t source_position{};
	::std::array<char, 3u> storage{};
	char const *current{storage.data()};
	char const *end{storage.data()};
	::std::size_t underflows{};
};

struct chunked_source_ref
{
	using input_char_type = char;
	chunked_source *source;
};

inline constexpr chunked_source_ref input_stream_ref_define(chunked_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

[[maybe_unused]] inline constexpr char const *ibuffer_begin(chunked_source_ref ref) noexcept
{
	return ref.source->storage.data();
}

inline constexpr char const *ibuffer_curr(chunked_source_ref ref) noexcept
{
	return ref.source->current;
}

inline constexpr char const *ibuffer_end(chunked_source_ref ref) noexcept
{
	return ref.source->end;
}

inline constexpr void ibuffer_set_curr(chunked_source_ref ref, char const *current) noexcept
{
	ref.source->current = current;
}

inline bool ibuffer_underflow(chunked_source_ref ref) noexcept
{
	++ref.source->underflows;
	auto const remaining{ref.source->source.size() - ref.source->source_position};
	auto const count{remaining < ref.source->storage.size() ? remaining : ref.source->storage.size()};
	if (count == 0u)
	{
		return false;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		ref.source->storage[i] = ref.source->source[ref.source->source_position + i];
	}
	ref.source->source_position += count;
	ref.source->current = ref.source->storage.data();
	ref.source->end = ref.source->storage.data() + count;
	return true;
}

template <bool hybrid>
struct text_target;

template <bool hybrid>
struct text_proxy
{
	text_target<hybrid> *target;
};

template <bool hybrid>
struct text_target
{
	::std::string value;
	::std::size_t contiguous_calls{};
	::std::size_t context_calls{};
	::std::size_t commits{};
};

template <bool hybrid>
inline constexpr text_proxy<hybrid> scan_alias_define(::fast_io::io_alias_t, text_target<hybrid> &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct text_context_state
{
	// No member initializer: value-initialization by the dispatcher is what establishes phase == 0.
	::std::size_t phase;
	::std::size_t characters;
};

template <bool hybrid>
inline constexpr ::fast_io::io_type_t<text_context_state>
scan_context_type(::fast_io::io_reserve_type_t<char, text_proxy<hybrid>>) noexcept
{
	return {};
}

template <bool hybrid>
inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, text_proxy<hybrid>>, text_context_state &state,
	char const *first, char const *last, text_proxy<hybrid> &proxy)
{
	++proxy.target->context_calls;
	if (state.phase > 1u)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	if (state.phase == 0u)
	{
		proxy.target->value.clear();
		state.phase = 1u;
	}
	if (first == last)
	{
		return {first, ::fast_io::parse_code::partial};
	}
	if (*first == '|')
	{
		++proxy.target->commits;
		return {first + 1, ::fast_io::parse_code::ok};
	}
	proxy.target->value.push_back(*first);
	++state.characters;
	// Consume deliberately one character at a time. Returning partial before `last` verifies that dispatch continues
	// on the suffix instead of discarding it through an eager underflow.
	return {first + 1, ::fast_io::parse_code::partial};
}

template <bool hybrid>
inline ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, text_proxy<hybrid>>, text_context_state &state,
	text_proxy<hybrid> &proxy) noexcept
{
	if (state.phase == 0u)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	++proxy.target->commits;
	return ::fast_io::parse_code::ok;
}

template <bool hybrid>
	requires(hybrid)
inline ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, text_proxy<hybrid>>, char const *first, char const *last,
	text_proxy<hybrid> &proxy)
{
	++proxy.target->contiguous_calls;
	if (first == last)
	{
		return {first, ::fast_io::parse_code::end_of_file};
	}
	auto current{first};
	for (; current != last && *current != '|'; ++current)
	{
	}
	proxy.target->value.assign(first, current);
	++proxy.target->commits;
	if (current != last)
	{
		return {current + 1, ::fast_io::parse_code::ok};
	}
	// For terminal buffers the entire remaining span is a complete token. On a refillable source the same result is
	// only a prefix; the dispatcher must therefore avoid this committing fast path there.
	return {last, ::fast_io::parse_code::ok};
}

template <bool hybrid>
	requires(hybrid)
inline constexpr ::std::true_type scan_context_terminal_contiguous_equivalent(
	::fast_io::io_reserve_type_t<char, text_proxy<hybrid>>) noexcept
{
	return {};
}

struct large_state_target
{
	::std::string value;
	bool aligned{true};
	bool zero_initialized{true};
};

struct large_state_proxy
{
	large_state_target *target;
};

inline constexpr large_state_proxy scan_alias_define(::fast_io::io_alias_t, large_state_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline ::std::size_t large_state_destructions{};

struct alignas(8192) large_context_state
{
	// This state is intentionally much larger and more strongly aligned than the inline scan-state budget. It has no
	// member initializer: observing phase == 0 proves that every active dispatcher uses value-initialization.
	::std::array<::std::byte, 64u * 1024u> working_set;
	::std::size_t phase;

	inline ~large_context_state()
	{
		++large_state_destructions;
	}
};

static_assert(!::fast_io::details::scan_context_state_inline_v<large_context_state>);

struct constexpr_large_context_state
{
	::std::array<::std::byte, 4097u> working_set;
	::std::size_t phase;
};

static_assert(!::fast_io::details::scan_context_state_inline_v<constexpr_large_context_state>);
static_assert(::fast_io::details::with_scan_context_state<constexpr_large_context_state>(
	[](constexpr_large_context_state &state) constexpr {
		return state.phase == 0u && state.working_set.front() == ::std::byte{};
	}));

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<large_context_state>
scan_context_type(::fast_io::io_reserve_type_t<char, large_state_proxy>) noexcept
{
	return {};
}

inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, large_state_proxy>, large_context_state &state,
	char const *first, char const *last, large_state_proxy &proxy)
{
	proxy.target->aligned = proxy.target->aligned &&
		(reinterpret_cast<::std::uintptr_t>(__builtin_addressof(state)) % alignof(large_context_state) == 0u);
	if (proxy.target->value.empty())
	{
		proxy.target->zero_initialized = proxy.target->zero_initialized && state.phase == 0u;
	}
	state.phase = 1u;
	auto current{first};
	for (; current != last && *current != '|'; ++current)
	{
		proxy.target->value.push_back(*current);
	}
	if (current != last)
	{
		return {current + 1, ::fast_io::parse_code::ok};
	}
	return {last, ::fast_io::parse_code::partial};
}

inline ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, large_state_proxy>, large_context_state &state,
	large_state_proxy &) noexcept
{
	return state.phase == 0u ? ::fast_io::parse_code::end_of_file : ::fast_io::parse_code::ok;
}

struct status_target
{
	bool value{};
};

struct status_proxy
{
	status_target *target;
};

inline constexpr status_proxy scan_alias_define(::fast_io::io_alias_t, status_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct status_source
{
	bool called{};
	bool *lock_observation{};
};

struct status_source_ref
{
	using input_char_type = char;
	status_source *source;
};

inline constexpr status_source_ref input_stream_ref_define(status_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline bool status_scan_define(status_source_ref source, status_proxy &proxy) noexcept
{
	if (source.source->lock_observation != nullptr)
	{
		assert(*source.source->lock_observation);
	}
	source.source->called = true;
	proxy.target->value = true;
	return true;
}

struct lock_state
{
	bool locked{};
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
};

struct mutex_ref
{
	lock_state *state;

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

struct locked_status_source
{
	status_source source;
	lock_state lock;
};

struct locked_status_ref
{
	using input_char_type = char;
	locked_status_source *source;
};

inline constexpr locked_status_ref input_stream_ref_define(locked_status_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline constexpr mutex_ref input_stream_mutex_ref_define(locked_status_ref source) noexcept
{
	return {__builtin_addressof(source.source->lock)};
}

inline constexpr status_source_ref input_stream_unlocked_ref_define(locked_status_ref source) noexcept
{
	return {__builtin_addressof(source.source->source)};
}

struct runtime_extent_proxy
{};

inline ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, runtime_extent_proxy>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, runtime_extent_proxy>, char const *, runtime_extent_proxy &) noexcept
{}

struct wrong_result_proxy
{};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_result_proxy>) noexcept
{
	return 1u;
}

[[maybe_unused]] inline int scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_result_proxy>, char const *, wrong_result_proxy &) noexcept
{
	return 0;
}

struct cv_qualified_context_state
{
	::std::size_t phase{};
};

struct cv_qualified_context_proxy
{};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<cv_qualified_context_state const>
scan_context_type(::fast_io::io_reserve_type_t<char, cv_qualified_context_proxy>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, cv_qualified_context_proxy>, cv_qualified_context_state const &,
	char const *first, char const *, cv_qualified_context_proxy &) noexcept
{
	return {first, ::fast_io::parse_code::ok};
}

[[maybe_unused]] inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, cv_qualified_context_proxy>, cv_qualified_context_state const &,
	cv_qualified_context_proxy &) noexcept
{
	return ::fast_io::parse_code::ok;
}

struct array_context_cell
{
	::std::byte value{};
};

struct array_context_proxy
{};

[[maybe_unused]] inline constexpr ::fast_io::io_type_t<array_context_cell[5000u]>
scan_context_type(::fast_io::io_reserve_type_t<char, array_context_proxy>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, array_context_proxy>, array_context_cell *,
	char const *first, char const *, array_context_proxy &) noexcept
{
	return {first, ::fast_io::parse_code::ok};
}

[[maybe_unused]] inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, array_context_proxy>, array_context_cell *, array_context_proxy &) noexcept
{
	return ::fast_io::parse_code::ok;
}

inline void test_precise_protocols()
{
	using ::fast_io::io::scan;

	::std::string_view empty;
	::fast_io::ibuffer_view empty_view(empty);
	fixed_target<0u> zero;
	assert(scan<true>(empty_view, zero));
	assert(zero.calls == 1u);

	::std::string_view record{"abcdZ"};
	::fast_io::ibuffer_view record_view(record);
	fixed_target<4u> fixed;
	assert(scan<true>(record_view, fixed));
	assert((fixed.value == ::std::array<char, 4u>{'a', 'b', 'c', 'd'}));
	assert(*record_view.curr_ptr == 'Z');

	::std::string_view batch_record{"abcdefghi"};
	::fast_io::ibuffer_view batch_view(batch_record);
	fixed_target<1u> a, b, c, d, e, f, g, h, i;
	assert(scan<true>(batch_view, a, b, c, d, e, f, g, h, i));
	assert(a.value[0] == 'a' && i.value[0] == 'i');
	assert(batch_view.curr_ptr == batch_view.end_ptr);

	::std::string short_record(large_extent - 1u, 'x');
	::fast_io::ibuffer_view short_view(short_record);
	large_probe_target short_probe;
	assert(!scan<true>(short_view, short_probe));
	assert(short_probe.calls == 0u);

	::std::string exact_record(large_extent, 'x');
	::fast_io::ibuffer_view exact_view(exact_record);
	large_probe_target exact_probe;
	assert(scan<true>(exact_view, exact_probe));
	assert(exact_probe.calls == 1u && exact_probe.matched);

	::std::string_view rejected{"#"};
	::fast_io::ibuffer_view rejected_view(rejected);
	fallible_target fallible;
	assert(!scan<true>(rejected_view, fallible));
	assert(rejected_view.curr_ptr == rejected_view.end_ptr);

	::std::string_view one{"Q"};
	::fast_io::ibuffer_view one_view(one);
	reference_alias_target reference_target;
	reference_proxy proxy{__builtin_addressof(reference_target)};
	reference_target.proxy_ptr = __builtin_addressof(proxy);
	assert(scan<true>(one_view, reference_target));
	assert(reference_target.value == 'Q');

	char parse_record[]{'R'};
	auto const parsed{::fast_io::parse_by_scan(parse_record, parse_record + 1u, reference_target)};
	assert(parsed.iter == parse_record + 1u && parsed.code == ::fast_io::parse_code::ok);
	assert(reference_target.value == 'R');
}

inline void test_context_and_hybrid_protocols()
{
	using ::fast_io::io::scan;

	chunked_source context_source{"abcdef|tail"};
	text_target<false> context_target;
	assert(scan<true>(context_source, context_target));
	assert(context_target.value == "abcdef");
	assert(context_target.commits == 1u);
	assert(context_target.context_calls >= 7u);
	assert(context_source.underflows == 3u);

	::std::string_view terminal_text{"terminal"};
	::fast_io::ibuffer_view terminal_view(terminal_text);
	text_target<true> terminal_target;
	assert(scan<true>(terminal_view, terminal_target));
	assert(terminal_target.value == "terminal");
	assert(terminal_target.contiguous_calls == 1u);
	assert(terminal_target.context_calls == 0u);
	assert(terminal_target.commits == 1u);

	chunked_source refillable_source{"cross-boundary"};
	text_target<true> refillable_target;
	assert(scan<true>(refillable_source, refillable_target));
	assert(refillable_target.value == "cross-boundary");
	assert(refillable_target.contiguous_calls == 0u);
	assert(refillable_target.context_calls != 0u);
	assert(refillable_target.commits == 1u);

	auto const destruction_base{large_state_destructions};
	chunked_source large_source{"large|tail"};
	large_state_target streamed;
	assert(scan<true>(large_source, streamed));
	assert(streamed.value == "large" && streamed.aligned && streamed.zero_initialized);
	assert(large_state_destructions == destruction_base + 1u);

	large_state_target parsed;
	char const parse_text[]{'p', 'a', 'r', 's', 'e'};
	auto const parse_result{::fast_io::parse_by_scan(parse_text, parse_text + 5u, parsed)};
	assert(parse_result.iter == parse_text + 5u && parse_result.code == ::fast_io::parse_code::ok);
	assert(parsed.value == "parse" && parsed.aligned && parsed.zero_initialized);
	assert(large_state_destructions == destruction_base + 2u);

	large_state_target converted;
	::fast_io::inplace_to(converted, "in", "place");
	assert(converted.value == "inplace" && converted.aligned && converted.zero_initialized);
	assert(large_state_destructions == destruction_base + 3u);
}

inline void test_status_mutex_and_exact_detection()
{
	using ::fast_io::io::scan;

	static_assert(::fast_io::operations::decay::defines::has_status_scan_define<
		status_source_ref, status_proxy &>);
	static_assert(!::fast_io::operations::decay::defines::has_status_scan_define<status_source_ref, int &>);
	static_assert(!::fast_io::precise_reserve_scannable<char, runtime_extent_proxy>);
	static_assert(!::fast_io::precise_reserve_scannable<char, wrong_result_proxy>);
	static_assert(!::fast_io::context_scannable<char, cv_qualified_context_proxy>);
	static_assert(!::fast_io::context_scannable<char, array_context_proxy>);

	status_source direct_source;
	status_target direct_target;
	assert(scan<true>(direct_source, direct_target));
	assert(direct_source.called && direct_target.value);

	locked_status_source locked_source;
	locked_source.source.lock_observation = __builtin_addressof(locked_source.lock.locked);
	status_target locked_target;
	assert(scan<true>(locked_source, locked_target));
	assert(locked_source.source.called && locked_target.value);
	assert(locked_source.lock.lock_calls == 1u);
	assert(locked_source.lock.unlock_calls == 1u);
	assert(!locked_source.lock.locked);
}

inline void test_memory_map_reference_semantics()
{
	using ::fast_io::io::scan;

	char input[]{'a', 'b', 'c'};
	::fast_io::basic_imemory_map<char> input_map;
	input_map.begin_ptr = input_map.curr_ptr = input;
	input_map.end_ptr = input + 3u;
	fixed_target<1u> first, second, third;
	assert(scan<true>(input_map, first, second));
	assert(first.value[0] == 'a' && second.value[0] == 'b');
	assert(input_map.curr_ptr == input + 2u);
	assert(scan<true>(input_map, third));
	assert(third.value[0] == 'c');
	assert(input_map.curr_ptr == input_map.end_ptr);

	char output[8u]{};
	::fast_io::basic_omemory_map<char> output_map;
	using output_map_ref = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(output_map))>;
	// A fixed mapped put area also supplies an all-or-terminate bulk completion CPO. The two proofs are independent:
	// cursor operations select the fitting fast path, while `writable` guarantees that every buffer-miss strategy has
	// a real terminal operation instead of accepting a put area that could silently drop the remainder.
	static_assert(::fast_io::operations::decay::defines::has_obuffer_basic_operations<output_map_ref>);
	static_assert(::fast_io::operations::decay::defines::writable<output_map_ref>);
	output_map.begin_ptr = output_map.curr_ptr = output;
	output_map.end_ptr = output + 8u;
	::fast_io::print(output_map, "ab");
	::fast_io::print(output_map, "cd");
	assert(output_map.written_bytes() == 4u);
	assert(::std::string_view(output, 4u) == "abcd");
}

} // namespace

int main()
{
	test_precise_protocols();
	test_context_and_hybrid_protocols();
	test_status_mutex_and_exact_detection();
	test_memory_map_reference_semantics();
}
