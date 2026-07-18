#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

// Pulling floating formatting into a scanner-protocol harness both increases build noise and violates the benchmark's
// isolation claim.  Preserve an embedding build's macro state while asking the public header for its non-floating API.
#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_SCAN_CONCEPT_HARNESS_RESTORE_FLOATING_MACRO
#endif
#include <fast_io.h>
#if defined(FAST_IO_SCAN_CONCEPT_HARNESS_RESTORE_FLOATING_MACRO)
#undef FAST_IO_SCAN_CONCEPT_HARNESS_RESTORE_FLOATING_MACRO
#undef FAST_IO_DISABLE_FLOATING_POINT
#endif

// This support header deliberately models only scanner protocols and stream capabilities.  Its tokens are fixed
// records or literal byte sequences; no decimal, floating-point, address, or other conversion algorithm participates
// in the measurements.  Keeping the semantic work trivial makes a regression attributable to concept recognition,
// alias/forward composition, buffer dispatch, packing, or locking rather than to a parser implementation.
namespace scan_concept_harness
{

template <::std::size_t extent>
struct fixed_record_target;

template <::std::size_t extent>
struct fixed_record_proxy
{
	fixed_record_target<extent> *target;
};

template <::std::size_t extent>
struct fixed_record_target
{
	::std::array<char, extent> value{};
	::std::size_t calls{};
};

template <::std::size_t extent>
inline constexpr fixed_record_proxy<extent>
scan_alias_define(::fast_io::io_alias_t, fixed_record_target<extent> &target) noexcept
{
	return {__builtin_addressof(target)};
}

template <::std::size_t extent>
inline constexpr ::std::size_t
	scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, fixed_record_proxy<extent>>) noexcept
{
	return extent;
}

template <::std::size_t extent>
inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_record_proxy<extent>>, char const *buffer,
	fixed_record_proxy<extent> &proxy) noexcept
{
	for (::std::size_t i{}; i != extent; ++i)
	{
		proxy.target->value[i] = buffer[i];
	}
	++proxy.target->calls;
}

// This fixed-record CPO depends only on its supplied character span and target. It cannot reach the input observer, so
// delaying a pack's cursor publication is observationally equivalent to the scalar commit-before-CPO schedule.
template <::std::size_t extent>
inline constexpr ::std::true_type scan_precise_reserve_aggregate_commit_safe(
	::fast_io::io_reserve_type_t<char, fixed_record_proxy<extent>>) noexcept
{
	return {};
}

#if !defined(FAST_IO_SCAN_CONCEPT_FORCE_REFERENCE_PROXY_TRANSPORT)
template <::std::size_t extent>
inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, fixed_record_proxy<extent>>) noexcept
{
	// The benchmark proxy contains only a pointer to externally owned target state. Its scan CPO ignores the proxy
	// address and performs every observable mutation through that pointer, so copying the descriptor is equivalent to
	// retaining its original normalization temporary. The opt-out macro creates an otherwise identical reference-path
	// benchmark; it is evidence machinery, not a second production policy.
	return {};
}
#endif

struct reference_alias_target;

// Copy and move are both deleted so this proxy proves that scan aliasing preserves an lvalue result.  A dispatcher
// that materializes the result of scan_alias_define cannot compile this otherwise valid protocol.
struct noncopyable_reference_proxy
{
	reference_alias_target *target;

	explicit constexpr noncopyable_reference_proxy(reference_alias_target *address) noexcept
		: target(address)
	{}

	noncopyable_reference_proxy(noncopyable_reference_proxy const &) = delete;
	noncopyable_reference_proxy &operator=(noncopyable_reference_proxy const &) = delete;
	noncopyable_reference_proxy(noncopyable_reference_proxy &&) = delete;
	noncopyable_reference_proxy &operator=(noncopyable_reference_proxy &&) = delete;
};

struct reference_alias_target
{
	char value{};
	noncopyable_reference_proxy proxy{this};

	inline constexpr reference_alias_target() noexcept = default;

	// Moving or copying the target constructs a new proxy bound to the destination. The proxy itself remains strictly
	// noncopyable, so a successful scan still proves that normalization retained the alias result as a reference.
	inline constexpr reference_alias_target(reference_alias_target const &other) noexcept
		: value(other.value), proxy(this)
	{}

	inline constexpr reference_alias_target(reference_alias_target &&other) noexcept
		: value(other.value), proxy(this)
	{}

	inline constexpr reference_alias_target &operator=(reference_alias_target const &other) noexcept
	{
		value = other.value;
		return *this;
	}

	inline constexpr reference_alias_target &operator=(reference_alias_target &&other) noexcept
	{
		value = other.value;
		return *this;
	}
};

inline constexpr noncopyable_reference_proxy &
scan_alias_define(::fast_io::io_alias_t, reference_alias_target &target) noexcept
{
	return target.proxy;
}

inline constexpr ::std::size_t
	scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>, char const *buffer,
	noncopyable_reference_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>, char const *first, char const *last,
	noncopyable_reference_proxy &proxy) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::end_of_file};
	}
	proxy.target->value = *first;
	return {first + 1, ::fast_io::parse_code::ok};
}

inline constexpr ::std::size_t refill_storage_size{8u};

// A bounded refill source provides the smallest meaningful nonterminal ibuffer protocol.  `chunk_size` is a runtime
// property on purpose: one scanner type is exercised across every boundary placement without changing its concepts.
struct bounded_refill_source
{
	using input_char_type = char;

	::std::string_view source{};
	::std::size_t source_position{};
	::std::size_t chunk_size{1u};
	::std::array<char, refill_storage_size> storage{};
	char *current{storage.data()};
	char *end{storage.data()};
	::std::size_t underflows{};

	inline constexpr void reset(::std::string_view text, ::std::size_t requested_chunk_size) noexcept
	{
		source = text;
		source_position = 0u;
		chunk_size = requested_chunk_size == 0u
						 ? 1u
						 : (requested_chunk_size < storage.size() ? requested_chunk_size : storage.size());
		current = end = storage.data();
		underflows = 0u;
	}
};

struct bounded_refill_source_ref
{
	using input_char_type = char;
	bounded_refill_source *source;
};

inline constexpr bounded_refill_source_ref input_stream_ref_define(bounded_refill_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

[[maybe_unused]] inline constexpr char *ibuffer_begin(bounded_refill_source_ref ref) noexcept
{
	return ref.source->storage.data();
}

inline constexpr char *ibuffer_curr(bounded_refill_source_ref ref) noexcept
{
	return ref.source->current;
}

inline constexpr char *ibuffer_end(bounded_refill_source_ref ref) noexcept
{
	return ref.source->end;
}

inline constexpr void ibuffer_set_curr(bounded_refill_source_ref ref, char *current) noexcept
{
	ref.source->current = current;
}

inline bool ibuffer_underflow(bounded_refill_source_ref ref) noexcept
{
	++ref.source->underflows;
	auto const remaining{ref.source->source.size() - ref.source->source_position};
	auto const count{remaining < ref.source->chunk_size ? remaining : ref.source->chunk_size};
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

inline constexpr ::std::size_t literal_capacity{256u};

template <bool contiguous_enabled>
struct literal_target;

template <bool contiguous_enabled>
struct literal_proxy
{
	literal_target<contiguous_enabled> *target;
};

template <bool contiguous_enabled>
struct literal_target
{
	::std::array<char, literal_capacity> value{};
	::std::size_t size{};
	::std::size_t contiguous_calls{};
	::std::size_t context_calls{};
	::std::size_t commits{};
};

template <bool contiguous_enabled>
inline constexpr literal_proxy<contiguous_enabled>
scan_alias_define(::fast_io::io_alias_t, literal_target<contiguous_enabled> &target) noexcept
{
	return {__builtin_addressof(target)};
}

#if !defined(FAST_IO_SCAN_CONCEPT_FORCE_REFERENCE_PROXY_TRANSPORT)
template <bool contiguous_enabled>
inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>) noexcept
{
	// The descriptor stores only an externally owned target address. Context state is owned separately by dispatch,
	// and no context or contiguous CPO observes or mutates the descriptor object or its address.
	return {};
}
#endif

struct literal_context_state
{
	// No member initializer is intentional: a correct dispatcher value-initializes protocol state before first use.
	::std::size_t characters;
	bool started;
};

template <bool contiguous_enabled>
inline constexpr ::fast_io::io_type_t<literal_context_state>
	scan_context_type(::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>) noexcept
{
	return {};
}

template <bool contiguous_enabled>
inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>, literal_context_state &state,
	char const *first, char const *last, literal_proxy<contiguous_enabled> &proxy) noexcept
{
	++proxy.target->context_calls;
	if (!state.started)
	{
		state.started = true;
		state.characters = 0u;
		proxy.target->size = 0u;
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
	if (state.characters == proxy.target->value.size())
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	proxy.target->value[state.characters++] = *first;
	proxy.target->size = state.characters;
	// Consuming one character while returning partial proves that dispatch reuses the unconsumed suffix of a refill
	// before asking for another chunk.  This is the boundary case that exposes lost-input and no-progress loops.
	return {first + 1, ::fast_io::parse_code::partial};
}

template <bool contiguous_enabled>
inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>, literal_context_state &state,
	literal_proxy<contiguous_enabled> &proxy) noexcept
{
	if (!state.started)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	++proxy.target->commits;
	return ::fast_io::parse_code::ok;
}

template <bool contiguous_enabled>
	requires(contiguous_enabled)
inline ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>, char const *first,
	char const *last, literal_proxy<contiguous_enabled> &proxy) noexcept
{
	++proxy.target->contiguous_calls;
	proxy.target->size = 0u;
	if (first == last)
	{
		// Match the unstarted context state's EOF result; this equality is part of the terminal-equivalence proof below.
		return {first, ::fast_io::parse_code::end_of_file};
	}
	auto current{first};
	for (; current != last && *current != '|'; ++current)
	{
		if (proxy.target->size == proxy.target->value.size())
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		proxy.target->value[proxy.target->size++] = *current;
	}
	++proxy.target->commits;
	return {current == last ? current : current + 1, ::fast_io::parse_code::ok};
}

template <bool contiguous_enabled>
	requires(contiguous_enabled)
inline constexpr ::std::true_type scan_context_terminal_contiguous_equivalent(
	::fast_io::io_reserve_type_t<char, literal_proxy<contiguous_enabled>>) noexcept
{
	return {};
}

template <bool contiguous_enabled>
[[nodiscard]] inline constexpr bool literal_equals(
	literal_target<contiguous_enabled> const &target, ::std::string_view expected) noexcept
{
	if (target.size != expected.size())
	{
		return false;
	}
	for (::std::size_t i{}; i != expected.size(); ++i)
	{
		if (target.value[i] != expected[i])
		{
			return false;
		}
	}
	return true;
}

struct stalled_context_target;

struct stalled_context_proxy
{
	stalled_context_target *target;
};

struct stalled_context_target
{
	::std::size_t context_calls{};
	::std::size_t eof_calls{};
};

inline constexpr stalled_context_proxy
scan_alias_define(::fast_io::io_alias_t, stalled_context_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct stalled_context_state
{
};

inline constexpr ::fast_io::io_type_t<stalled_context_state>
	scan_context_type(::fast_io::io_reserve_type_t<char, stalled_context_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, stalled_context_proxy>, stalled_context_state &, char const *first,
	char const *, stalled_context_proxy &proxy) noexcept
{
	++proxy.target->context_calls;
	// Deliberately violate the progress rule. A terminal dispatcher must reject this result instead of repeatedly
	// presenting the same available suffix to a state machine that cannot become productive without a refill.
	return {first, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, stalled_context_proxy>, stalled_context_state &,
	stalled_context_proxy &proxy) noexcept
{
	++proxy.target->eof_calls;
	return ::fast_io::parse_code::end_of_file;
}

struct escaped_context_target;

struct escaped_context_proxy
{
	escaped_context_target *target;
};

struct escaped_context_target
{
	char const *reported_iterator{};
	::fast_io::parse_code reported_code{::fast_io::parse_code::partial};
	::std::size_t context_calls{};
};

inline constexpr escaped_context_proxy
scan_alias_define(::fast_io::io_alias_t, escaped_context_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

struct escaped_context_state
{
};

inline constexpr ::fast_io::io_type_t<escaped_context_state>
	scan_context_type(::fast_io::io_reserve_type_t<char, escaped_context_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, escaped_context_proxy>, escaped_context_state &, char const *, char const *,
	escaped_context_proxy &proxy) noexcept
{
	++proxy.target->context_calls;
	// The caller selects a pointer in the same backing array but outside the advertised subrange. This makes the
	// negative test well-defined while proving that dispatch validates the iterator independently of the parse code.
	return {proxy.target->reported_iterator, proxy.target->reported_code};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, escaped_context_proxy>, escaped_context_state &,
	escaped_context_proxy &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

struct status_target;

struct status_proxy
{
	status_target *target;
};

struct status_target
{
	bool value{};
};

inline constexpr status_proxy scan_alias_define(::fast_io::io_alias_t, status_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

#if !defined(FAST_IO_SCAN_CONCEPT_FORCE_REFERENCE_PROXY_TRANSPORT)
inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, status_proxy>) noexcept
{
	// The high-level status CPO performs every observable write through `target`; copying this descriptor cannot change
	// target identity or scanner state. Whether value transport is profitable for a status source remains a separate,
	// source-aware scheduling question measured by the benchmark opt-out above.
	return {};
}
#endif

struct status_source
{
	::std::size_t calls{};
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

// Compatibility probe used only when the concept benchmark is compiled against the frozen pre-fix baseline. The old
// detector asked whether `status_scan_define<true>(source, 0)` existed, even though execution called the non-template
// overload below with the real proxy pack. Providing this non-deducible template makes the historical detector reach
// the same runtime CPO without changing current recognition or admitting an integer to the actual scan operation.
template <bool>
inline bool status_scan_define(status_source_ref, int) noexcept
{
	return false;
}

template <typename... proxies>
	requires(sizeof...(proxies) != 0u &&
			 (::std::same_as<::std::remove_cvref_t<proxies>, status_proxy> && ...))
inline bool status_scan_define(status_source_ref source, proxies &&...proxy) noexcept
{
	if (source.source->lock_observation != nullptr && !*source.source->lock_observation)
	{
		return false;
	}
	source.source->calls += sizeof...(proxies);
	((proxy.target->value = true), ...);
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
		if (state->locked)
		{
			__builtin_trap();
		}
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		if (!state->locked)
		{
			__builtin_trap();
		}
		state->locked = false;
		++state->unlock_calls;
	}
};

struct locked_status_source
{
	status_source source{};
	lock_state lock{};
};

struct locked_status_source_ref
{
	using input_char_type = char;
	locked_status_source *source;
};

inline constexpr locked_status_source_ref input_stream_ref_define(locked_status_source &source) noexcept
{
	return {__builtin_addressof(source)};
}

inline constexpr mutex_ref input_stream_mutex_ref_define(locked_status_source_ref source) noexcept
{
	return {__builtin_addressof(source.source->lock)};
}

inline constexpr status_source_ref input_stream_unlocked_ref_define(locked_status_source_ref source) noexcept
{
	return {__builtin_addressof(source.source->source)};
}

template <typename input_type, ::std::size_t extent, ::std::size_t count, ::std::size_t... indices>
inline bool scan_fixed_pack_impl(
	input_type &input, ::std::array<fixed_record_target<extent>, count> &targets,
	::std::index_sequence<indices...>)
{
	return ::fast_io::io::scan<true>(input, targets[indices]...);
}

template <typename input_type, ::std::size_t extent, ::std::size_t count>
inline bool scan_fixed_pack(
	input_type &input, ::std::array<fixed_record_target<extent>, count> &targets)
{
	return scan_fixed_pack_impl(input, targets, ::std::make_index_sequence<count>{});
}

template <typename source_type, ::std::size_t count, ::std::size_t... indices>
inline bool scan_status_pack_impl(source_type &source, ::std::array<status_target, count> &targets,
								  ::std::index_sequence<indices...>)
{
	return ::fast_io::io::scan<true>(source, targets[indices]...);
}

template <typename source_type, ::std::size_t count>
inline bool scan_status_pack(source_type &source, ::std::array<status_target, count> &targets)
{
	return scan_status_pack_impl(source, targets, ::std::make_index_sequence<count>{});
}

} // namespace scan_concept_harness
