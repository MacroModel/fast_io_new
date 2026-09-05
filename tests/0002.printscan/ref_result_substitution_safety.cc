#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>

#include <fast_io_core.h>

namespace ref_result_substitution
{

struct incomplete_stream_proxy;
struct incomplete_mutex_proxy;
struct incomplete_scan_alias_proxy;

// These queries are intentionally formed while both proxy classes are incomplete. A forward-declared class is still
// an object type, but no standard constructibility trait may be instantiated until completeness has been established.
static_assert(!::fast_io::operations::defines::storable_input_stream_ref_result<
	incomplete_stream_proxy>);
static_assert(!::fast_io::operations::defines::storable_input_stream_ref_result<
	incomplete_stream_proxy &>);
static_assert(!::fast_io::operations::defines::storable_output_stream_ref_result<
	incomplete_stream_proxy>);
static_assert(!::fast_io::operations::defines::storable_output_stream_ref_result<
	incomplete_stream_proxy const &>);
static_assert(!::fast_io::operations::defines::abi_value_input_stream_ref_result<
	incomplete_stream_proxy>);
static_assert(!::fast_io::operations::defines::abi_value_output_stream_ref_result<
	incomplete_stream_proxy &>);
static_assert(!::fast_io::operations::decay::defines::storable_mutex_ref_result<
	incomplete_mutex_proxy>);
static_assert(!::fast_io::operations::decay::defines::storable_mutex_ref_result<
	incomplete_mutex_proxy &>);

struct incomplete_scan_alias_value_target
{};

struct incomplete_scan_alias_xvalue_target
{};

incomplete_scan_alias_proxy scan_alias_define(
	::fast_io::io_alias_t, incomplete_scan_alias_value_target &) noexcept;
incomplete_scan_alias_proxy &&scan_alias_define(
	::fast_io::io_alias_t, incomplete_scan_alias_xvalue_target &) noexcept;

// Unlike a stable lvalue alias, either expression must be materialized into owned storage. The completeness check has
// to precede the construction trait: standard-library implementations may diagnose constructibility queries on a
// forward declaration rather than converting that invalid query into a false atomic constraint.
static_assert(!::fast_io::alias_scannable<incomplete_scan_alias_value_target &>);
static_assert(!::fast_io::alias_scannable<incomplete_scan_alias_xvalue_target &>);

struct incomplete_input_value_source
{};

struct incomplete_input_reference_source
{};

struct incomplete_output_value_source
{};

struct incomplete_output_reference_source
{};

incomplete_stream_proxy input_stream_ref_define(
	incomplete_input_value_source &&) noexcept;
incomplete_stream_proxy &input_stream_ref_define(
	incomplete_input_reference_source &) noexcept;
incomplete_stream_proxy output_stream_ref_define(
	incomplete_output_value_source &&) noexcept;
incomplete_stream_proxy &output_stream_ref_define(
	incomplete_output_reference_source &) noexcept;

template <typename T>
concept can_normalize_input_ref = requires {
	::fast_io::operations::input_stream_ref(::std::declval<T>());
};

template <typename T>
concept can_normalize_output_ref = requires {
	::fast_io::operations::output_stream_ref(::std::declval<T>());
};

static_assert(!::fast_io::operations::defines::has_input_stream_ref_define<
	incomplete_input_value_source>);
static_assert(!::fast_io::operations::defines::has_input_stream_ref_define<
	incomplete_input_reference_source &>);
static_assert(!::fast_io::operations::defines::has_output_stream_ref_define<
	incomplete_output_value_source>);
static_assert(!::fast_io::operations::defines::has_output_stream_ref_define<
	incomplete_output_reference_source &>);
static_assert(!can_normalize_input_ref<incomplete_input_value_source>);
static_assert(!can_normalize_input_ref<incomplete_input_reference_source &>);
static_assert(!can_normalize_output_ref<incomplete_output_value_source>);
static_assert(!can_normalize_output_ref<incomplete_output_reference_source &>);

struct copyable_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *identity{};
};

struct externally_stateful_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *const state{};
};

// This opt-in is intentionally absent for `copyable_io_proxy`: mutating its observer object can change which state it
// denotes. Copies of this test proxy, by contrast, are specified to operate only on the shared pointed-to state; its
// descriptor field is immutable under the hypothetical stream protocol. The marker proves semantics, not ABI lowering.
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<externally_stateful_io_proxy>) noexcept
{
	return {};
}

struct move_only_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *identity{};

	move_only_io_proxy() = default;
	inline explicit constexpr move_only_io_proxy(int *value) noexcept : identity(value) {}
	move_only_io_proxy(move_only_io_proxy const &) = delete;
	move_only_io_proxy &operator=(move_only_io_proxy const &) = delete;
	inline constexpr move_only_io_proxy(move_only_io_proxy &&other) noexcept
		: identity(::std::exchange(other.identity, nullptr))
	{}
	move_only_io_proxy &operator=(move_only_io_proxy &&) = delete;
};

struct immovable_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *identity{};

	inline explicit constexpr immovable_io_proxy(int *value) noexcept : identity(value) {}
	immovable_io_proxy(immovable_io_proxy const &) = delete;
	immovable_io_proxy &operator=(immovable_io_proxy const &) = delete;
	immovable_io_proxy(immovable_io_proxy &&) = delete;
	immovable_io_proxy &operator=(immovable_io_proxy &&) = delete;
};

struct nontrivial_copyable_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *identity{};
	unsigned *copy_count{};

	inline constexpr nontrivial_copyable_io_proxy(int *value, unsigned *copies) noexcept
		: identity(value), copy_count(copies)
	{}

	inline nontrivial_copyable_io_proxy(nontrivial_copyable_io_proxy const &other) noexcept
		: identity(other.identity), copy_count(other.copy_count)
	{
		++*copy_count;
	}

	inline nontrivial_copyable_io_proxy &operator=(
		nontrivial_copyable_io_proxy const &other) noexcept
	{
		identity = other.identity;
		copy_count = other.copy_count;
		++*copy_count;
		return *this;
	}
};

// Primitive dispatch must be well-formed for a named move-only or immovable
// observer. These CPOs touch only external state, so the test can distinguish a
// valid borrowed invocation without granting either type value-copy admission.
inline char *read_some_underflow_define(
	move_only_io_proxy &input, char *first, char *last) noexcept
{
	if (first != last)
	{
		*first++ = 'm';
		++*input.identity;
	}
	return first;
}

inline void write_all_overflow_define(
	move_only_io_proxy &output, char const *, char const *) noexcept
{
	++*output.identity;
}

inline char *read_some_underflow_define(
	immovable_io_proxy &input, char *first, char *last) noexcept
{
	if (first != last)
	{
		*first++ = 'i';
		++*input.identity;
	}
	return first;
}

inline void write_all_overflow_define(
	immovable_io_proxy &output, char const *, char const *) noexcept
{
	++*output.identity;
}

static_assert(::fast_io::operations::defines::storable_input_stream_ref_result<
	copyable_io_proxy &>);
static_assert(::fast_io::operations::defines::storable_output_stream_ref_result<
	copyable_io_proxy const &>);
static_assert(::std::is_trivially_copyable_v<copyable_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	move_only_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	move_only_io_proxy &>);
static_assert(!::std::is_trivially_copyable_v<nontrivial_copyable_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	nontrivial_copyable_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	nontrivial_copyable_io_proxy &>);
static_assert(!::fast_io::operations::defines::stream_ref_value_transport_safe<
	copyable_io_proxy>);
static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
	externally_stateful_io_proxy>);
static_assert(::fast_io::operations::defines::stream_ref_result_borrows_lvalue<
	copyable_io_proxy &>);
static_assert(!::fast_io::operations::defines::stream_ref_result_borrows_lvalue<
	copyable_io_proxy const &>);
static_assert(!::fast_io::operations::defines::stream_ref_result_borrows_lvalue<
	copyable_io_proxy &&>);

// One-time normalization and repeated primitive propagation are intentionally independent proofs. A move-only prvalue,
// a non-trivial copyable prvalue, and an unmarked compact descriptor can each create or denote the normalized observer,
// but none may enter a helper that repeatedly copies it. Even an explicit semantic marker remains subject to the native
// argument-cost policy, so this relation is portable to scalar-only and unmodelled ABI families.
static_assert(!::fast_io::operations::defines::abi_value_io_stream_ref_result<
	copyable_io_proxy &>);
static_assert(
	::fast_io::operations::defines::abi_value_io_stream_ref_result<
		externally_stateful_io_proxy> ==
	::fast_io::details::abi_small_trivial_argument_object<externally_stateful_io_proxy>());
static_assert(!::fast_io::operations::defines::abi_value_io_stream_ref_result<
	move_only_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	immovable_io_proxy>);
static_assert(::fast_io::operations::defines::storable_io_stream_ref_result<
	immovable_io_proxy &>);
static_assert(!::fast_io::operations::defines::storable_io_stream_ref_result<
	immovable_io_proxy &&>);
static_assert(!::fast_io::operations::defines::abi_value_io_stream_ref_result<
	immovable_io_proxy>);
static_assert(!::fast_io::operations::defines::abi_value_io_stream_ref_result<
	nontrivial_copyable_io_proxy>);

struct primitive_direct_input_cursor
{
	using input_char_type = char;
	char const *current{};
};

struct primitive_direct_output_cursor
{
	using output_char_type = char;
	char *current{};
};

// These compact cursors deliberately keep mutable position in the observer
// object. They are trivial and register-sized, but copying either would publish
// progress only into a discarded parameter. The missing semantic marker is the
// formal reason primitive dispatch must retain their identity.
static_assert(::std::is_trivially_copyable_v<primitive_direct_input_cursor>);
static_assert(::std::is_trivially_copyable_v<primitive_direct_output_cursor>);
static_assert(!::fast_io::operations::defines::abi_value_input_stream_ref_result<
	primitive_direct_input_cursor &>);
static_assert(!::fast_io::operations::defines::abi_value_output_stream_ref_result<
	primitive_direct_output_cursor &>);

inline char *read_some_underflow_define(
	primitive_direct_input_cursor &input, char *first, char *last) noexcept
{
	for (char *iter{first}; iter != last; ++iter, ++input.current)
		*iter = *input.current;
	return last;
}

inline void write_all_overflow_define(
	primitive_direct_output_cursor &output, char const *first,
	char const *last) noexcept
{
	for (; first != last; ++first, ++output.current)
		*output.current = *first;
}

struct primitive_shared_state
{
	char const *input_current{};
	char *output_current{};
	void const *observed_input_proxy{};
	void const *observed_output_proxy{};
};

struct primitive_shared_input_proxy
{
	using input_char_type = char;
	primitive_shared_state *state{};
};

struct primitive_shared_output_proxy
{
	using output_char_type = char;
	primitive_shared_state *state{};
};

// Both proxies contain only a pointer to shared mutable control state. The ADL
// declarations are explicit substitution proofs: copying the descriptor cannot
// fork either cursor, so a target-supported primitive may restore value ABI.
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<primitive_shared_input_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<primitive_shared_output_proxy>) noexcept
{
	return {};
}

static_assert(
	::fast_io::operations::defines::abi_value_input_stream_ref_result<
		primitive_shared_input_proxy &> ==
	::fast_io::details::abi_small_trivial_argument_object<
		primitive_shared_input_proxy>());
static_assert(
	::fast_io::operations::defines::abi_value_output_stream_ref_result<
		primitive_shared_output_proxy &> ==
	::fast_io::details::abi_small_trivial_argument_object<
		primitive_shared_output_proxy>());

inline char *read_some_underflow_define(
	primitive_shared_input_proxy &input, char *first, char *last) noexcept
{
	input.state->observed_input_proxy = __builtin_addressof(input);
	for (char *iter{first}; iter != last; ++iter, ++input.state->input_current)
		*iter = *input.state->input_current;
	return last;
}

inline void write_all_overflow_define(
	primitive_shared_output_proxy &output, char const *first,
	char const *last) noexcept
{
	output.state->observed_output_proxy = __builtin_addressof(output);
	for (; first != last; ++first, ++output.state->output_current)
		*output.state->output_current = *first;
}

struct copyable_input_source
{
	copyable_io_proxy proxy;
};

inline constexpr copyable_io_proxy &input_stream_ref_define(
	copyable_input_source &source) noexcept
{
	return source.proxy;
}

inline constexpr copyable_io_proxy const &input_stream_ref_define(
	copyable_input_source const &source) noexcept
{
	return source.proxy;
}

struct xvalue_input_source
{
	copyable_io_proxy proxy;
};

inline constexpr copyable_io_proxy &&input_stream_ref_define(
	xvalue_input_source &&source) noexcept
{
	return ::std::move(source.proxy);
}

struct volatile_materializable_io_proxy
{
	using input_char_type = char;
	using output_char_type = char;
	int *identity{};

	volatile_materializable_io_proxy() = default;
	inline volatile_materializable_io_proxy(
		volatile_materializable_io_proxy volatile &other) noexcept
		: identity(other.identity)
	{}
};

struct volatile_input_source
{
	volatile_materializable_io_proxy proxy;
};

inline volatile_materializable_io_proxy volatile &input_stream_ref_define(
	volatile_input_source &source) noexcept
{
	return source.proxy;
}

struct copyable_output_source
{
	copyable_io_proxy proxy;
};

inline constexpr copyable_io_proxy &output_stream_ref_define(
	copyable_output_source &source) noexcept
{
	return source.proxy;
}

struct copyable_io_source
{
	copyable_io_proxy proxy;
};

inline constexpr copyable_io_proxy &io_stream_ref_define(
	copyable_io_source &source) noexcept
{
	return source.proxy;
}

struct move_only_input_source
{
	int *identity{};
};

inline constexpr move_only_io_proxy input_stream_ref_define(
	move_only_input_source &&source) noexcept
{
	return move_only_io_proxy{source.identity};
}

struct nontrivial_input_source
{
	int *identity{};
	unsigned *copy_count{};
};

struct referenced_move_only_input_source
{
	move_only_io_proxy proxy;
};

inline constexpr move_only_io_proxy &input_stream_ref_define(
	referenced_move_only_input_source &source) noexcept
{
	return source.proxy;
}

struct referenced_nontrivial_input_source
{
	nontrivial_copyable_io_proxy proxy;
};

inline constexpr nontrivial_copyable_io_proxy &input_stream_ref_define(
	referenced_nontrivial_input_source &source) noexcept
{
	return source.proxy;
}

struct immovable_input_source
{
	int *identity{};
};

inline constexpr immovable_io_proxy input_stream_ref_define(
	immovable_input_source &&source) noexcept
{
	return immovable_io_proxy{source.identity};
}

inline constexpr nontrivial_copyable_io_proxy input_stream_ref_define(
	nontrivial_input_source &&source) noexcept
{
	return {source.identity, source.copy_count};
}

static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	copyable_input_source &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	copyable_input_source const &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	xvalue_input_source>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	volatile_input_source &>);
static_assert(::fast_io::operations::defines::has_output_stream_ref_define<
	copyable_output_source &>);
static_assert(::fast_io::operations::defines::has_io_stream_ref_define<
	copyable_io_source &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	move_only_input_source>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	nontrivial_input_source>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	immovable_input_source>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	referenced_move_only_input_source &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	referenced_nontrivial_input_source &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<copyable_input_source &>())),
	copyable_io_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<copyable_input_source const &>())),
	copyable_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<xvalue_input_source>())),
	copyable_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<volatile_input_source &>())),
	volatile_materializable_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::output_stream_ref(
		::std::declval<copyable_output_source &>())),
	copyable_io_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::io_stream_ref(
		::std::declval<copyable_io_source &>())),
	copyable_io_proxy &>);
static_assert(can_normalize_input_ref<move_only_input_source>);
static_assert(can_normalize_input_ref<nontrivial_input_source>);
static_assert(can_normalize_input_ref<immovable_input_source>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<move_only_input_source>())),
	move_only_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<nontrivial_input_source>())),
	nontrivial_copyable_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<immovable_input_source>())),
	immovable_io_proxy>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<referenced_move_only_input_source &>())),
	move_only_io_proxy &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<referenced_nontrivial_input_source &>())),
	nontrivial_copyable_io_proxy &>);

struct copyable_mutex_proxy
{
	int *lock_count{};

	inline void lock() noexcept
	{
		++*lock_count;
	}

	inline void unlock() noexcept
	{
		++*lock_count;
	}
};

struct move_only_mutex_proxy
{
	int *lock_count{};

	move_only_mutex_proxy() = default;
	inline explicit constexpr move_only_mutex_proxy(int *value) noexcept : lock_count(value) {}
	move_only_mutex_proxy(move_only_mutex_proxy const &) = delete;
	move_only_mutex_proxy &operator=(move_only_mutex_proxy const &) = delete;
	inline constexpr move_only_mutex_proxy(move_only_mutex_proxy &&other) noexcept
		: lock_count(::std::exchange(other.lock_count, nullptr))
	{}
	move_only_mutex_proxy &operator=(move_only_mutex_proxy &&) = delete;

	inline void lock() noexcept
	{
		++*lock_count;
	}

	inline void unlock() noexcept
	{
		++*lock_count;
	}
};

static_assert(::fast_io::operations::decay::defines::storable_mutex_ref_result<
	copyable_mutex_proxy &>);
static_assert(::fast_io::operations::decay::defines::storable_mutex_ref_result<
	move_only_mutex_proxy>);
static_assert(!::fast_io::operations::decay::defines::storable_mutex_ref_result<
	move_only_mutex_proxy &>);

template <typename unlocked_type>
struct locked_output_with
{
	using output_char_type = char;
	int *identity{};
	unsigned *copy_count{};
};

template <typename unlocked_type>
inline constexpr copyable_mutex_proxy output_stream_mutex_ref_define(
	locked_output_with<unlocked_type> output) noexcept
{
	return {output.identity};
}

inline constexpr copyable_io_proxy output_stream_unlocked_ref_define(
	locked_output_with<copyable_io_proxy> output) noexcept
{
	return {output.identity};
}

inline constexpr move_only_io_proxy output_stream_unlocked_ref_define(
	locked_output_with<move_only_io_proxy> output) noexcept
{
	return move_only_io_proxy{output.identity};
}

inline constexpr nontrivial_copyable_io_proxy output_stream_unlocked_ref_define(
	locked_output_with<nontrivial_copyable_io_proxy> output) noexcept
{
	return {output.identity, output.copy_count};
}

// Mutex ownership is independent: the guard may own a move-only lock proxy. The unlocked observer is likewise
// materialized once by high-level dispatch, so move-only and non-trivial prvalues are valid protocol results. A
// primitive that intends to copy one of them must impose the separate ABI-value refinement at that primitive boundary.
static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_output_with<copyable_io_proxy>>);
static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_output_with<move_only_io_proxy>>);
static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_output_with<nontrivial_copyable_io_proxy>>);

struct unlocked_output
{
	using output_char_type = char;
};

struct incomplete_value_mutex_output
{
	using output_char_type = char;
};

struct incomplete_reference_mutex_output
{
	using output_char_type = char;
};

incomplete_mutex_proxy output_stream_mutex_ref_define(
	incomplete_value_mutex_output) noexcept;
incomplete_mutex_proxy &output_stream_mutex_ref_define(
	incomplete_reference_mutex_output) noexcept;

inline constexpr unlocked_output output_stream_unlocked_ref_define(
	incomplete_value_mutex_output) noexcept
{
	return {};
}

inline constexpr unlocked_output output_stream_unlocked_ref_define(
	incomplete_reference_mutex_output) noexcept
{
	return {};
}

static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	incomplete_value_mutex_output>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	incomplete_reference_mutex_output>);

inline void run_reference_and_prvalue_tests()
{
	int identity{};
	copyable_input_source referenced{{__builtin_addressof(identity)}};
	decltype(auto) input_reference{::fast_io::operations::input_stream_ref(referenced)};
	static_assert(::std::same_as<decltype(input_reference), copyable_io_proxy &>);
	assert(__builtin_addressof(input_reference) == __builtin_addressof(referenced.proxy));
	input_reference.identity = nullptr;
	assert(referenced.proxy.identity == nullptr);
	referenced.proxy.identity = __builtin_addressof(identity);

	copyable_output_source output_source{{__builtin_addressof(identity)}};
	decltype(auto) output_reference{::fast_io::operations::output_stream_ref(output_source)};
	static_assert(::std::same_as<decltype(output_reference), copyable_io_proxy &>);
	assert(__builtin_addressof(output_reference) == __builtin_addressof(output_source.proxy));
	output_reference.identity = nullptr;
	assert(output_source.proxy.identity == nullptr);

	copyable_io_source joint_source{{__builtin_addressof(identity)}};
	decltype(auto) joint_reference{::fast_io::operations::io_stream_ref(joint_source)};
	static_assert(::std::same_as<decltype(joint_reference), copyable_io_proxy &>);
	assert(__builtin_addressof(joint_reference) == __builtin_addressof(joint_source.proxy));
	joint_reference.identity = nullptr;
	assert(joint_source.proxy.identity == nullptr);

	copyable_input_source const const_source{{__builtin_addressof(identity)}};
	auto const_materialized{::fast_io::operations::input_stream_ref(const_source)};
	static_assert(::std::same_as<decltype(const_materialized), copyable_io_proxy>);
	assert(__builtin_addressof(const_materialized) != __builtin_addressof(const_source.proxy));
	assert(const_materialized.identity == __builtin_addressof(identity));

	xvalue_input_source xvalue_source{{__builtin_addressof(identity)}};
	auto xvalue_materialized{
		::fast_io::operations::input_stream_ref(::std::move(xvalue_source))};
	static_assert(::std::same_as<decltype(xvalue_materialized), copyable_io_proxy>);
	assert(__builtin_addressof(xvalue_materialized) != __builtin_addressof(xvalue_source.proxy));
	assert(xvalue_materialized.identity == __builtin_addressof(identity));

	volatile_input_source volatile_source;
	volatile_source.proxy.identity = __builtin_addressof(identity);
	auto volatile_materialized{::fast_io::operations::input_stream_ref(volatile_source)};
	static_assert(::std::same_as<decltype(volatile_materialized),
		volatile_materializable_io_proxy>);
	assert(__builtin_addressof(volatile_materialized) !=
		   __builtin_addressof(volatile_source.proxy));
	assert(volatile_materialized.identity == __builtin_addressof(identity));

	move_only_input_source unique_source{__builtin_addressof(identity)};
	auto unique{::fast_io::operations::input_stream_ref(::std::move(unique_source))};
	assert(unique.identity == __builtin_addressof(identity));

	unsigned copy_count{};
	nontrivial_input_source observable_source{__builtin_addressof(identity), __builtin_addressof(copy_count)};
	auto observable{::fast_io::operations::input_stream_ref(::std::move(observable_source))};
	assert(observable.identity == __builtin_addressof(identity));
	assert(copy_count == 0u);

	immovable_input_source immovable_source{__builtin_addressof(identity)};
	auto immovable{::fast_io::operations::input_stream_ref(::std::move(immovable_source))};
	assert(immovable.identity == __builtin_addressof(identity));

	referenced_move_only_input_source referenced_unique{
		move_only_io_proxy{__builtin_addressof(identity)}};
	decltype(auto) unique_reference{::fast_io::operations::input_stream_ref(referenced_unique)};
	static_assert(::std::same_as<decltype(unique_reference), move_only_io_proxy &>);
	assert(__builtin_addressof(unique_reference) == __builtin_addressof(referenced_unique.proxy));

	referenced_nontrivial_input_source referenced_observable{
		nontrivial_copyable_io_proxy{__builtin_addressof(identity), __builtin_addressof(copy_count)}};
	decltype(auto) observable_reference{
		::fast_io::operations::input_stream_ref(referenced_observable)};
	static_assert(::std::same_as<decltype(observable_reference), nontrivial_copyable_io_proxy &>);
	assert(__builtin_addressof(observable_reference) == __builtin_addressof(referenced_observable.proxy));
	assert(copy_count == 0u);

	int lock_count{};
	copyable_mutex_proxy copyable_mutex{__builtin_addressof(lock_count)};
	copyable_mutex_proxy copied_mutex{copyable_mutex};
	copied_mutex.lock();
	copied_mutex.unlock();
	move_only_mutex_proxy moved_mutex{__builtin_addressof(lock_count)};
	move_only_mutex_proxy stored_mutex{::std::move(moved_mutex)};
	stored_mutex.lock();
	stored_mutex.unlock();
	assert(lock_count == 4);

	char const input_text[]{'i', 'n'};
	char input_result[2]{};
	primitive_direct_input_cursor direct_input{input_text};
	::fast_io::operations::decay::read_some_decay_dispatch(
		direct_input, input_result, input_result + 2);
	assert(direct_input.current == input_text + 2);
	assert(input_result[0] == 'i' && input_result[1] == 'n');

	char direct_output_storage[2]{};
	char const output_text[]{'o', 'k'};
	primitive_direct_output_cursor direct_output{direct_output_storage};
	::fast_io::operations::decay::write_all_decay_dispatch(
		direct_output, output_text, output_text + 2);
	assert(direct_output.current == direct_output_storage + 2);
	assert(direct_output_storage[0] == 'o' && direct_output_storage[1] == 'k');

	char shared_input_result[2]{};
	char shared_output_storage[2]{};
	primitive_shared_state shared_state{
		input_text, shared_output_storage, nullptr, nullptr};
	primitive_shared_input_proxy shared_input{__builtin_addressof(shared_state)};
	primitive_shared_output_proxy shared_output{__builtin_addressof(shared_state)};
	::fast_io::operations::decay::read_some_decay_dispatch(
		shared_input, shared_input_result, shared_input_result + 2);
	::fast_io::operations::decay::write_all_decay_dispatch(
		shared_output, output_text, output_text + 2);
	assert(shared_state.input_current == input_text + 2);
	assert(shared_state.output_current == shared_output_storage + 2);
	assert(shared_input_result[0] == 'i' && shared_input_result[1] == 'n');
	assert(shared_output_storage[0] == 'o' && shared_output_storage[1] == 'k');
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<
				  primitive_shared_input_proxy &>)
		assert(shared_state.observed_input_proxy != __builtin_addressof(shared_input));
	else
		assert(shared_state.observed_input_proxy == __builtin_addressof(shared_input));
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<
				  primitive_shared_output_proxy &>)
		assert(shared_state.observed_output_proxy != __builtin_addressof(shared_output));
	else
		assert(shared_state.observed_output_proxy == __builtin_addressof(shared_output));

	int move_only_activity{};
	move_only_io_proxy move_only_primitive{__builtin_addressof(move_only_activity)};
	char move_only_character{};
	::fast_io::operations::decay::read_some_decay_dispatch(
		move_only_primitive, __builtin_addressof(move_only_character),
		__builtin_addressof(move_only_character) + 1);
	::fast_io::operations::decay::write_all_decay_dispatch(
		move_only_primitive, output_text, output_text + 1);
	assert(move_only_activity == 2 && move_only_character == 'm');

	int immovable_activity{};
	immovable_io_proxy immovable_primitive{__builtin_addressof(immovable_activity)};
	char immovable_character{};
	::fast_io::operations::decay::read_some_decay_dispatch(
		immovable_primitive, __builtin_addressof(immovable_character),
		__builtin_addressof(immovable_character) + 1);
	::fast_io::operations::decay::write_all_decay_dispatch(
		immovable_primitive, output_text, output_text + 1);
	assert(immovable_activity == 2 && immovable_character == 'i');
}

} // namespace ref_result_substitution

int main()
{
	::ref_result_substitution::run_reference_and_prvalue_tests();
}
