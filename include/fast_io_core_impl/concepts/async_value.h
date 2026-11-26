#pragma once

namespace fast_io::async
{
/** @brief One optional milestone reached during a bounded value step. Zero-initialization publishes nothing. */
struct value_checkpoint
{
	::std::size_t ordinal{};
	bool skipped{};
	bool starting{};

	/** @brief Returns a publication for a resource or semantic boundary the provider has actually released. */
	static constexpr value_checkpoint reached(::std::size_t index) noexcept { return {index+1u,false}; }
	/** @brief Declares that a conditional stage will never be taken in this invocation. */
	static constexpr value_checkpoint unused(::std::size_t index) noexcept { return {index+1u,true}; }

	/** @brief Reports entry into selected asynchronous work. The following provider steps own its completion.
	 * @details Return this checkpoint as control enters the stage, before running the work whose start
	 * the observer may await. The current output prefix and parser cursor must already be valid.
	 */
	template <typename Index> requires (::std::is_enum_v<Index> || ::std::integral<Index>)
	static constexpr value_checkpoint start(Index index) noexcept
	{ return {static_cast<::std::size_t>(index) + 1u, false, true}; }
};
/** @brief Print progression including an optional independently observable resource release. */
template <typename Iter>
struct value_print_step { Iter iter; bool done; value_checkpoint checkpoint{}; };
/** @brief Scan progression including an optional milestone of the private parser. Commit remains a separate action. */
template <typename Iter>
struct value_scan_step { Iter iter; parse_code code; value_checkpoint checkpoint{}; };
/** @brief Allows an EOF drain to report one new checkpoint before a later terminal result. */
struct value_eof_step { parse_code code; value_checkpoint checkpoint{}; };

/** @brief Requires local point before to retire before local point after may retire. Identifiers start at zero. */
struct value_dependency
{
	::std::size_t before,after;
	friend constexpr bool operator==(value_dependency,value_dependency) noexcept = default;
};
/** @brief Defines a value's finite set of named-by-index milestones and only its mandatory order constraints. */
template <::std::size_t Count,value_dependency... Edges>
struct value_points
{
	static constexpr ::std::size_t count{Count};
	// Explicit zero opts out even though the value still has an owned incremental protocol.
	static constexpr bool observe_field{Count!=0};
	static constexpr ::std::size_t edge_count{sizeof...(Edges)};
	static constexpr value_dependency edges[sizeof...(Edges) + 1]{Edges...,{}};
};
namespace details
{
template <typename R,typename I>
concept print_step_result = ::std::same_as<R,context_print_result<I>> || ::std::same_as<R,value_print_step<I>>;
template <typename R,typename I>
concept scan_step_result = ::std::same_as<R,parse_result<I>> || ::std::same_as<R,value_scan_step<I>>;
template <typename R>
concept eof_step_result = ::std::same_as<R,parse_code> || ::std::same_as<R,value_eof_step>;

template <typename Char, typename T>
using print_capture_type = decltype(async_print_capture_define(
	io_reserve_type<Char, ::std::remove_cvref_t<T>>, ::std::declval<T>()));
template <typename Char, typename Saved>
using print_cursor_type = typename decltype(async_print_cursor_type(io_reserve_type<Char,Saved>))::type;
template <typename Char, typename T>
using scan_binding_type = decltype(async_scan_bind_define(
	io_reserve_type<Char, ::std::remove_cvref_t<T>>, ::std::declval<T>()));
template <typename Char, typename Binding>
using scan_cursor_type = typename decltype(async_scan_cursor_type(io_reserve_type<Char,Binding>))::type;
} // namespace details

/**
 * @brief Identifies an independently captured print value with an incrementally resumable character producer.
 * @details Capture happens before issue returns and must detach all mutable source aliases. A cursor is
 * constructed once after its saved value reaches stable storage. A nonterminal step must either
 * produce a nonempty prefix or advance its private state and report a checkpoint. A start can therefore
 * be observed before any character is produced. A provider can finish with zero characters. Window
 * pointers expire on return. Different argument positions get distinct captures and cursors.
 */
template <typename Char, typename T>
concept async_print_producer = ::std::integral<Char> && requires {
	typename details::print_capture_type<Char,T>;
	requires ::std::is_object_v<details::print_capture_type<Char,T>>;
	requires ::std::is_move_constructible_v<details::print_capture_type<Char,T>>;
	typename details::print_cursor_type<Char,details::print_capture_type<Char,T>>;
	requires ::std::is_default_constructible_v<details::print_cursor_type<Char,details::print_capture_type<Char,T>>>;
} && requires(details::print_capture_type<Char,T> const &saved,
	details::print_cursor_type<Char,details::print_capture_type<Char,T>> &cursor, Char *p) {
	{ async_print_step_define(io_reserve_type<Char,details::print_capture_type<Char,T>>, cursor, saved, p, p) }
		-> details::print_step_result<Char *>;
};

/** @brief Supplies a captured producer's maximum total spelling in code units at compile time.
 * @details This bound covers all steps combined, including every possible emitted prefix and suffix.
 * It describes the asynchronous producer identified by its capture type, independently of any synchronous formatter.
 */
template <typename Char, typename Saved>
concept async_print_statically_bounded = ::std::integral<Char> && requires {
	{ async_print_max_size_define(io_reserve_type<Char,Saved>) } noexcept -> ::std::same_as<::std::size_t>;
	typename ::std::integral_constant<::std::size_t,async_print_max_size_define(io_reserve_type<Char,Saved>)>;
	requires (async_print_max_size_define(io_reserve_type<Char,Saved>) != SIZE_MAX);
};

/**
 * @brief Identifies a bound scan target with private incremental parsing state and an explicit commit boundary.
 * @details Binding fixes the destination and parsing configuration before issue returns. A step may consume
 * only its reported prefix. A partial step on a nonempty window must consume input or advance its
 * private state and report a checkpoint. At EOF the finalizer may expose another checkpoint before
 * returning a terminal code. Every subsequent finalization step sees the same end of input. Only an
 * ok result authorizes commit. The binding may borrow the target until the operation releases the last
 * occurrence of that target. A committed result must outlive the temporary input window it consumed.
 * The driver commits fields in argument order.
 */
template <typename Char, typename T>
concept async_scan_consumer = ::std::integral<Char> && requires {
	typename details::scan_binding_type<Char,T>;
	requires ::std::is_object_v<details::scan_binding_type<Char,T>>;
	requires ::std::is_move_constructible_v<details::scan_binding_type<Char,T>>;
	typename details::scan_cursor_type<Char,details::scan_binding_type<Char,T>>;
	requires ::std::is_default_constructible_v<details::scan_cursor_type<Char,details::scan_binding_type<Char,T>>>;
} && requires(details::scan_binding_type<Char,T> const &binding,
	details::scan_cursor_type<Char,details::scan_binding_type<Char,T>> &cursor, Char const *p) {
	{ async_scan_step_define(io_reserve_type<Char,details::scan_binding_type<Char,T>>, cursor, binding, p, p) }
		-> details::scan_step_result<Char const *>;
	{ async_scan_eof_define(io_reserve_type<Char,details::scan_binding_type<Char,T>>, cursor, binding) }
		-> details::eof_step_result;
	{ async_scan_commit_define(io_reserve_type<Char,details::scan_binding_type<Char,T>>, cursor, binding) }
		-> ::std::same_as<void>;
};

/**
 * @brief Certifies that formatting this value into private storage during issue preserves its complete visible contract.
 * @details The provider promises destination-independent character generation, stable configuration during capture,
 * and no externally ordered side effect or exception prefix that precomputing the field would move. The driver
 * copies the actual initialized characters, including all scatter backing ranges, before returning to the caller.
 * This is a per-character-domain customization. Copyability and reservation bounds alone confer no such permission.
 */
struct materialize_at_issue_t {};
inline constexpr materialize_at_issue_t materialize_at_issue{};

/** @brief Recognizes an explicit semantic equivalence proof, independently of ordinary reserve or scatter support. */
template <typename Char,typename T>
concept async_print_materializable = ::std::integral<Char> && requires {
	{ async_print_reuse_define(io_reserve_type<Char,::std::remove_cvref_t<T>>) } noexcept -> ::std::same_as<materialize_at_issue_t>;
} && (::fast_io::reserve_printable<Char,::std::remove_cvref_t<T>> || ::fast_io::dynamic_reserve_printable<Char,::std::remove_cvref_t<T>> ||
	::fast_io::scatter_printable<Char,::std::remove_cvref_t<T>> || ::fast_io::reserve_scatters_printable<Char,::std::remove_cvref_t<T>> ||
	::fast_io::dynamic_reserve_scatters_printable<Char,::std::remove_cvref_t<T>> || ::fast_io::context_printable<Char,::std::remove_cvref_t<T>>);

/**
 * @brief Certifies whole-operation reuse of the canonical synchronous scanner with an exclusive lvalue target.
 * @details The target identity is the entire configuration, it remains valid through the final result, and
 * no additional borrowed descriptors are read. The original scan controls field order, partial assignment,
 * mutexes and errors. This proof exposes no separate early-commit point for the individual argument.
 */
struct borrow_scan_target_t {};
inline constexpr borrow_scan_target_t borrow_scan_target{};
template <typename Char,typename T>
concept async_scan_reusable = ::std::is_lvalue_reference_v<T> && !::std::is_const_v<::std::remove_reference_t<T>> &&
	requires { { async_scan_reuse_define(io_reserve_type<Char,::std::remove_cvref_t<T>>) } noexcept -> ::std::same_as<borrow_scan_target_t>; };

/** @brief Selects a value protocol in the actual output domain independently of synchronous printability. */
template <typename Char,typename T>
concept async_printable = ::std::integral<Char> && (async_print_producer<Char,T> || async_print_materializable<Char,T>);

/** @brief Selects a target protocol in the actual input domain independently of synchronous scannability. */
template <typename Char,typename T>
concept async_scannable = ::std::integral<Char> && (async_scan_consumer<Char,T> || async_scan_reusable<Char,T>);

/** @brief Certifies that each initialized prefix of this captured formatter can become visible immediately.
 * @details The producer seals all characters preceding its returned cursor. A later formatting failure
 * may leave those characters in the output. A producer that still needs to authenticate, validate or
 * revise them must retain them in private storage until it can give this guarantee.
 */
template <typename Char, typename Saved>
concept prefix_print_producer = requires {
	{ async_print_prefixes_define(io_reserve_type<Char, Saved>) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief Allows this captured formatter to progress while an earlier, immutable output prefix is in flight.
 * @details Its cursor and destination window are private. Advancing the cursor cannot depend on the
 * earlier write result or cause an external effect that requires its success. Selected starts may be
 * observed even if that write subsequently fails. A nonthrowing step lets the issuer join the borrowed
 * prefix at its next output boundary on every path. Other producers can still stream sealed prefixes.
 */
template <typename Char, typename Saved>
concept independent_print_producer = prefix_print_producer<Char, Saved> &&
	requires(details::print_cursor_type<Char, Saved> &cursor, Saved const &value, Char *p) {
		{ async_print_lookahead_define(io_reserve_type<Char, Saved>) } noexcept -> ::std::same_as<::std::true_type>;
		{ async_print_step_define(io_reserve_type<Char, Saved>, cursor, value, p, p) } noexcept;
	};
} // namespace fast_io::async
