#include <concepts>
#include <cstddef>
#include <cstdlib>

#include <fast_io_core.h>

namespace transcode_decay_transport_contract
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct counters
{
	unsigned process{};
	unsigned sync_flush{};
	unsigned finish{};
};

struct inline_observer
{
	using from_value_type = char;
	using to_value_type = char;
	counters calls{};
};

struct inline_engine
{
	inline_observer observer{};
};

inline constexpr inline_observer &transcode_ref_define(
	inline_engine &engine) noexcept
{
	return engine.observer;
}

struct substitutable_observer
{
	using from_value_type = char;
	using to_value_type = char;
	counters *calls{};
};

struct substitutable_engine
{
	counters calls{};
};

inline constexpr substitutable_observer transcode_ref_define(
	substitutable_engine &engine) noexcept
{
	return {__builtin_addressof(engine.calls)};
}

inline constexpr ::std::true_type transcode_ref_value_transport_safe_define(
	::fast_io::io_type_t<substitutable_observer>) noexcept
{
	// Copies share all transform state through the external control block.
	return {};
}

struct default_engine
{
	using from_value_type = char;
	using to_value_type = char;
	counters calls{};
};

/*
 * This observer is deliberately neither copyable nor movable. A reference
 * normalization must preserve its identity, whereas a prvalue normalization
 * must initialize its sole owner by guaranteed copy elision. An exception
 * specification must not instantiate the unselected by-value transport: a
 * conditional operator selects an evaluated operand, not a discarded template
 * branch, and therefore cannot hide the deleted copy constructor below.
 */
struct pinned_counters : counters
{
	unsigned normalizations{};
	unsigned constructions{};
	unsigned destructions{};
};

struct pinned_observer
{
	using from_value_type = char;
	using to_value_type = char;
	pinned_counters *calls{};
	pinned_observer const *identity{};

	explicit pinned_observer(pinned_counters *state) noexcept
		: calls(state), identity(this)
	{
		++calls->constructions;
	}

	pinned_observer(pinned_observer const &) = delete;
	pinned_observer &operator=(pinned_observer const &) = delete;
	pinned_observer(pinned_observer &&) = delete;
	pinned_observer &operator=(pinned_observer &&) = delete;

	~pinned_observer()
	{
		++calls->destructions;
	}
};

struct pinned_lvalue_engine
{
	pinned_counters calls{};
	pinned_observer observer{__builtin_addressof(calls)};
};

inline pinned_observer &transcode_ref_define(pinned_lvalue_engine &engine) noexcept
{
	++engine.calls.normalizations;
	return engine.observer;
}

struct pinned_prvalue_engine
{
	pinned_counters calls{};
};

inline pinned_observer transcode_ref_define(pinned_prvalue_engine &engine) noexcept
{
	++engine.calls.normalizations;
	return pinned_observer{__builtin_addressof(engine.calls)};
}

template <typename observer>
inline counters &observer_calls(observer &ref) noexcept
{
	if constexpr (::std::same_as<observer, inline_observer>)
	{
		return ref.calls;
	}
	else if constexpr (::std::same_as<observer, pinned_observer>)
	{
		require(ref.identity == __builtin_addressof(ref));
		return *ref.calls;
	}
	else if constexpr (::std::same_as<observer, substitutable_observer>)
	{
		return *ref.calls;
	}
	else
	{
		return ref.calls;
	}
}

template <typename observer>
	requires(::std::same_as<observer, inline_observer> ||
			 ::std::same_as<observer, pinned_observer> ||
			 ::std::same_as<observer, substitutable_observer> ||
			 ::std::same_as<observer, default_engine>)
inline ::fast_io::basic_transcode_process_result<char, char>
transcode_process_define(observer &ref, char const *, char const *from_last,
						 char *to_first, char *) noexcept
{
	++observer_calls(ref).process;
	return {from_last, to_first, ::fast_io::transcode_step_status::need_input};
}

template <typename observer>
	requires(::std::same_as<observer, inline_observer> ||
			 ::std::same_as<observer, pinned_observer> ||
			 ::std::same_as<observer, substitutable_observer> ||
			 ::std::same_as<observer, default_engine>)
inline ::fast_io::basic_transcode_drain_result<char>
transcode_sync_flush_define(observer &ref, char *to_first, char *) noexcept
{
	++observer_calls(ref).sync_flush;
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

template <typename observer>
	requires(::std::same_as<observer, inline_observer> ||
			 ::std::same_as<observer, pinned_observer> ||
			 ::std::same_as<observer, substitutable_observer> ||
			 ::std::same_as<observer, default_engine>)
inline ::fast_io::basic_transcode_drain_result<char>
transcode_finish_define(observer &ref, char *to_first, char *) noexcept
{
	++observer_calls(ref).finish;
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

template <typename observer>
	requires(::std::same_as<observer, inline_observer> ||
			 ::std::same_as<observer, pinned_observer> ||
			 ::std::same_as<observer, substitutable_observer> ||
			 ::std::same_as<observer, default_engine>)
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<observer>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

using process_result = ::fast_io::basic_transcode_process_result<char, char>;
using process_owner_entry = process_result (*)(
	inline_observer, char const *, char const *, char *, char *) noexcept;
using process_borrowed_entry = process_result (*)(
	inline_observer &, char const *, char const *, char *, char *) noexcept;
using substitutable_process_owner_entry = process_result (*)(
	substitutable_observer, char const *, char const *, char *, char *) noexcept;
using substitutable_process_borrowed_entry = process_result (*)(
	substitutable_observer &, char const *, char const *, char *, char *) noexcept;

/*
 * The type-level proposition is the portable part of the ABI contract: the
 * historical unsuffixed decay spelling owns a value, while only the explicit
 * borrowed spelling exposes a reference parameter. Runtime checks below prove
 * that the mandatory-inline selector does not discard inline engine state.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_process_decay<
					   inline_observer>),
			  process_owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_process_decay_borrowed<
					   inline_observer>),
			  process_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_process_decay<
					   substitutable_observer>),
			  substitutable_process_owner_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::transcode_process_decay_borrowed<
					   substitutable_observer>),
			  substitutable_process_borrowed_entry>);
static_assert(!::fast_io::operations::defines::
				  transcode_ref_value_transport_safe<inline_observer>);
static_assert(!::fast_io::operations::defines::
				  abi_value_transcode_ref_result_object<inline_observer &>());
static_assert(::fast_io::operations::defines::
				  transcode_ref_value_transport_safe<substitutable_observer>);
// Semantic copy substitution is portable, whereas direct aggregate transport
// is target-dependent. In particular, conservative i386/Wasm policies may
// borrow even a one-pointer aggregate; that must not invalidate its value ABI
// entry or the normalization contract tested above and below.
static_assert(::fast_io::operations::defines::
				  abi_value_transcode_ref_result_object<substitutable_observer &>() ==
	::fast_io::details::abi_small_trivial_argument_object<substitutable_observer>());
static_assert(::fast_io::operations::defines::
				  abi_value_transcode_ref_result_object<
					  ::fast_io::transcoder_ref<default_engine> &>() ==
	::fast_io::details::abi_small_trivial_argument_object<::fast_io::transcoder_ref<default_engine>>());

static_assert(::fast_io::transcoder<pinned_lvalue_engine>);
static_assert(::fast_io::transcoder<pinned_prvalue_engine>);
static_assert(!::fast_io::operations::defines::
				  abi_value_transcode_ref_result_object<pinned_observer &>());
static_assert(noexcept(::fast_io::operations::transcode_process(
	::std::declval<pinned_lvalue_engine &>(), nullptr, nullptr, nullptr, nullptr)));
static_assert(noexcept(::fast_io::operations::transcode_process(
	::std::declval<pinned_prvalue_engine &>(), nullptr, nullptr, nullptr, nullptr)));
static_assert(noexcept(::fast_io::operations::decay::transcode_finish_decay_dispatch(
	::std::declval<pinned_observer &>(), nullptr, nullptr)));

inline void verify_pinned_normalization() noexcept
{
	char input{'x'};
	char output{};
	pinned_lvalue_engine borrowed{};
	(void)::fast_io::operations::transcode_process(
		borrowed, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	require(borrowed.calls.normalizations == 1u);
	require(borrowed.calls.process == 1u);
	(void)::fast_io::operations::transcode_finish(
		borrowed, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(borrowed.calls.normalizations == 2u);
	require(borrowed.calls.finish == 1u);
	require(borrowed.calls.constructions == 1u);
	require(borrowed.calls.destructions == 0u);

	// Each public operation owns one distinct normalization result until all
	// selected CPO work completes; no intermediate move or copy is permitted.
	pinned_prvalue_engine owned{};
	(void)::fast_io::operations::transcode_process(
		owned, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	require(owned.calls.normalizations == 1u);
	require(owned.calls.process == 1u);
	require(owned.calls.constructions == 1u);
	require(owned.calls.destructions == 1u);
	(void)::fast_io::operations::transcode_finish(
		owned, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(owned.calls.normalizations == 2u);
	require(owned.calls.finish == 1u);
	require(owned.calls.constructions == 2u);
	require(owned.calls.destructions == 2u);
}

inline void verify_inline_identity() noexcept
{
	char input{'x'};
	char output{};
	inline_observer observer{};

	(void)::fast_io::operations::decay::transcode_process_decay(
		observer, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.process == 0u);
	(void)::fast_io::operations::decay::transcode_process_decay_dispatch(
		observer, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.process == 1u);

	(void)::fast_io::operations::decay::transcode_sync_flush_decay(
		observer, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.sync_flush == 0u);
	(void)::fast_io::operations::decay::transcode_sync_flush_decay_dispatch(
		observer, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.sync_flush == 1u);

	(void)::fast_io::operations::decay::transcode_finish_decay(
		observer, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.finish == 0u);
	(void)::fast_io::operations::decay::transcode_finish_decay_dispatch(
		observer, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(observer.calls.finish == 1u);

	inline_engine engine{};
	(void)::fast_io::operations::transcode_process(
		engine, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	(void)::fast_io::operations::transcode_sync_flush(
		engine, __builtin_addressof(output), __builtin_addressof(output) + 1);
	(void)::fast_io::operations::transcode_finish(
		engine, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(engine.observer.calls.process == 1u);
	require(engine.observer.calls.sync_flush == 1u);
	require(engine.observer.calls.finish == 1u);
}

inline void verify_substitutable_and_default() noexcept
{
	char input{'x'};
	char output{};
	substitutable_engine substitutable{};
	(void)::fast_io::operations::transcode_process(
		substitutable, __builtin_addressof(input),
		__builtin_addressof(input) + 1, __builtin_addressof(output),
		__builtin_addressof(output) + 1);
	(void)::fast_io::operations::transcode_finish(
		substitutable, __builtin_addressof(output),
		__builtin_addressof(output) + 1);
	require(substitutable.calls.process == 1u);
	require(substitutable.calls.finish == 1u);

	default_engine engine{};
	(void)::fast_io::operations::transcode_process(
		engine, __builtin_addressof(input), __builtin_addressof(input) + 1,
		__builtin_addressof(output), __builtin_addressof(output) + 1);
	(void)::fast_io::operations::transcode_finish(
		engine, __builtin_addressof(output), __builtin_addressof(output) + 1);
	require(engine.calls.process == 1u);
	require(engine.calls.finish == 1u);
}

} // namespace transcode_decay_transport_contract

extern "C"
{
	[[gnu::used]]
	::transcode_decay_transport_contract::process_owner_entry
		fast_io_transcode_decay_value_entry{
			&::fast_io::operations::decay::transcode_process_decay<
				::transcode_decay_transport_contract::inline_observer>};
	[[gnu::used]]
	::transcode_decay_transport_contract::process_borrowed_entry
		fast_io_transcode_decay_borrowed_entry{
			&::fast_io::operations::decay::transcode_process_decay_borrowed<
				::transcode_decay_transport_contract::inline_observer>};
	[[gnu::used]]
	::transcode_decay_transport_contract::substitutable_process_owner_entry
		fast_io_transcode_safe_decay_value_entry{
			&::fast_io::operations::decay::transcode_process_decay<
				::transcode_decay_transport_contract::substitutable_observer>};
	[[gnu::used]]
	::transcode_decay_transport_contract::substitutable_process_borrowed_entry
		fast_io_transcode_safe_decay_borrowed_entry{
			&::fast_io::operations::decay::transcode_process_decay_borrowed<
				::transcode_decay_transport_contract::substitutable_observer>};
}

int main()
{
	::transcode_decay_transport_contract::verify_inline_identity();
	::transcode_decay_transport_contract::verify_substitutable_and_default();
	::transcode_decay_transport_contract::verify_pinned_normalization();
}
