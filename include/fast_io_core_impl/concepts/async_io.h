#pragma once

namespace fast_io::async
{

template <typename T> class task;

/** @brief Authorizes suspending between ordinary bounded steps of one converter in the specified direction.
 * @details The engine retains ownership of its mutable state and must relinquish each borrowed window
 * before a step returns. The driver serializes subsequent steps and finalization on the issuing thread.
 * A converter can supply a separate local description and layer driver for an internal multistage protocol.
 */
template <typename Engine, bool Input>
concept resumable_transcode_step = requires {
	{ async_transcode_step_define(::fast_io::transcode_reserve<Engine>, ::std::bool_constant<Input>{}) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief Allows the concrete output to authorise independent writes of successive sealed record prefixes.
 * @details A true answer covers ordered visibility, partial failure, and splitting this record into multiple
 * writes. The probe has no output effect. Message transports and whole-record handlers may require staging.
 */
template <typename T>
concept prefix_output = requires(T &out) {
	{ async_output_accepts_prefixes_define(out) } noexcept -> ::std::convertible_to<bool>;
};

/** @brief Carries a native completion failure until the result is consumed. */
struct native_error
{
	using raise_type = void (*)(::std::uint_least32_t) FAST_IO_ASYNC_THROWS;
	raise_type raise{};
	::std::uint_least32_t code{};
};

/**
 * @brief Reports one completed byte request on any platform.
 * @details On success, transferred is at most the requested length and zero on input denotes EOF.
 * On failure the provider records a domain-specific raising function and its code. The caller raises it
 * at the result-consumption boundary, after the device has relinquished the range.
 */
struct byte_completion
{
	::std::size_t transferred{};
	native_error error{};
};

/**
 * @brief Permits one isolated issuer to combine successive public windows of a byte stream.
 * @details The caller probes the live sink immediately before the write. A successful probe
 * guarantees that grouping these windows preserves byte order and the prefix accepted on failure.
 * The writer returns the full length on success, or the exact shorter prefix and the first error.
 * The caller uses that prefix to restore the decoder's last attempted public window.
 */
template <typename T>
concept coalescible_transmit_output = requires(T &out, ::std::byte const *data, ::std::size_t size) {
	{ async_output_can_coalesce_transmit_define(out) } noexcept -> ::std::same_as<bool>;
	{ async_write_transmit_window_define(out, data, size) } noexcept -> ::std::same_as<byte_completion>;
};

/**
 * @brief Recognizes a frame-owned native request whose one completion can be delivered without throwing.
 * @details A false suspension decision completes on the issuer. A true decision transfers the right to
 * resume the continuation exactly once. The enclosing coroutine owns the request until await_resume.
 */
template <typename T, typename Result>
concept completion_awaiter = requires(T &request, ::std::coroutine_handle<> continuation) {
	{ request.await_ready() } noexcept -> ::std::same_as<bool>;
	{ request.await_suspend(continuation) } noexcept -> ::std::same_as<bool>;
	{ request.await_resume() } noexcept -> ::std::same_as<Result>;
};

/**
 * @brief Recognizes an output that can conditionally commit an owned record on an isolated issuer.
 * @details A false return leaves the entire range untouched. A true return consumes the full range
 * or publishes the first native failure after its committed prefix. The caller supplies live,
 * nonoverlapping storage and guarantees a standalone root with no queued peer or foreign pump.
 */
template <typename Output, typename Char>
concept issuer_record_output = requires(Output &out, Char const *first, Char const *last, native_error &failure) {
	{ async_write_record_on_issuer_define(out, first, last, failure) } noexcept -> ::std::same_as<bool>;
};

/**
 * @brief Recognizes an issuer-side write of an arbitrary contiguous part of a stream.
 * @details The caller owns the nonoverlapping range through completion and drives an isolated
 * root with an empty ready queue. A false return preserves the sink and its position. A true
 * return has the same bytes, ordering, position and first-error behavior as the ordinary range
 * write on this sink. In particular, chunk boundaries cannot introduce a message or flush.
 * A device whose eligibility depends on its live handle checks that handle on every attempt.
 */
template <typename Output, typename Char>
concept issuer_range_output = requires(Output &out, Char const *first, Char const *last, native_error &failure) {
	{ async_write_range_on_issuer_define(out, first, last, failure) } noexcept -> ::std::same_as<bool>;
};

/** @brief Proves a backend can suspend and report progress for one contiguous byte range. */
template <typename T>
concept byte_io_awaiter = completion_awaiter<T, byte_completion>;

/**
 * @brief Proves device input can finish on the issuer without waiting for an external I/O event.
 * @details The normalized observer opts in for all cursor positions, including refill and logical EOF.
 * A custom scanner still controls its own computation. A buffer that may refill from a file cannot opt in.
 */
template <typename T>
concept immediate_input = requires {
	{ input_stream_immediate_define(::fast_io::io_type<::std::remove_cvref_t<T>>) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief Proves every write, overflow and device finish can run without waiting on external I/O. */
template <typename T>
concept immediate_output = requires {
	{ output_stream_immediate_define(::fast_io::io_type<::std::remove_cvref_t<T>>) } noexcept -> ::std::same_as<::std::true_type>;
};

/** @brief Proves prepared bytes go directly to a physical output with no remaining user-defined transformation. */
template <typename T>
concept kernel_byte_output = requires(T &stream, ::std::byte const *p, ::std::size_t n) {
	{ async_kernel_output_define(::fast_io::io_type<T>) } noexcept -> ::std::same_as<::std::true_type>;
	{ async_write_some_define(stream, p, n) } noexcept -> byte_io_awaiter;
};

/** @brief Recognizes completion-based input on a normalized stream observer. */
template <typename T>
concept native_input = requires(T &stream, ::std::byte *first, ::std::size_t bytes) {
	{ async_read_some_define(stream, first, bytes) } noexcept -> byte_io_awaiter;
};

/**
 * @brief Recognizes a destination that can accept a whole record immediately when its current put area permits.
 * @details Returning false leaves the destination untouched. Returning true commits the complete typed range.
 * The operation runs on the issuer at first resume, so an earlier task can change the available capacity safely.
 */
template <typename T>
concept immediate_output_range = requires(T &out, typename T::output_char_type const *first, typename T::output_char_type const *last) {
	{ output_stream_try_write_define(out, first, last) } noexcept -> ::std::same_as<bool>;
};

/**
 * @brief Recognizes an observer that can parse and commit a complete group of scan targets without waiting.
 * @details Success advances the cursor and assigns targets in argument order. A false result preserves the
 * cursor and every target, including on partial input, so the ordinary scanner retains its EOF and error rules.
 */
template <typename Input, typename... Targets>
concept immediate_input_scan = requires(Input &in, Targets &...targets) {
	{ input_stream_try_scan_define(in, targets...) } noexcept -> ::std::same_as<bool>;
};

/**
 * @brief Recognizes an ordinary, potentially refilling scan that returns its exact terminal parse code.
 * @details This operation executes once. Earlier target writes and cursor movement on failure match public scan.
 * It supplies the parse outcome independently of the exception ABI that eventually observes it.
 */
template <typename Input, typename... Targets>
concept parse_code_input_scan = requires(Input &in, Targets &...targets) {
	{ input_stream_scan_code_define(in, targets...) } -> ::std::same_as<::fast_io::parse_code>;
};

/**
 * @brief Proves a destination can lend contiguous space to a bounded, nonthrowing formatter without waiting.
 * @details A null result leaves the device untouched. A nonnull result grants the requested number of writable
 * code units. The issuer immediately publishes the used prefix with the companion commit operation. No other
 * use of the device or suspension is permitted between reservation and publication.
 */
template <typename Output>
concept immediate_output_reserve = requires(Output &out, ::std::size_t n, typename Output::output_char_type *last) {
	{ output_stream_try_reserve_define(out, n) } noexcept -> ::std::same_as<typename Output::output_char_type *>;
	{ output_stream_commit_reserved_define(out, last) } noexcept -> ::std::same_as<void>;
};

/**
 * @brief Identifies a value that may be snapshotted for bounded, deferred formatting in a character domain.
 * @details Copying the complete value must capture everything affecting its text. After the call all
 * selected formatting and normalization steps must be nonthrowing, independent of device state and free
 * of observable effects other than the emitted text. A pointer-bearing provider may opt in only if the
 * pointee remains immutable and alive throughout the job. Sizing and emitting protocols independently determine whether a reservation is available. Repeating a size or scatter query must have no observable effect.
 */
template <typename Char, typename T>
concept snapshot_printable = requires { sizeof(T); } && ::std::is_trivially_copyable_v<T> &&
							 ::std::is_nothrow_copy_constructible_v<T> && requires {
								 { print_async_snapshot_safe(io_reserve_type<Char, T>) } -> ::std::same_as<::std::true_type>;
							 };

/** @brief Recognizes completion-based output on a normalized stream observer. */
template <typename T>
concept native_output = requires(T &stream, ::std::byte const *first, ::std::size_t bytes) {
	{ async_write_some_define(stream, first, bytes) } noexcept -> byte_io_awaiter;
};

/**
 * @brief Requires a selected backing stream to complete an input refill or output overflow natively.
 * @details An observer can represent a handle whose ordinary byte operation reports submission status
 * while the kernel still owns its range. Moving that operation to a worker cannot complete it.
 * A resident buffer can still accept or consume public code units without calling the backing observer.
 */
template <typename T, bool Input>
concept native_buffer_completion_required = ((Input && native_input<T>) || (!Input && native_output<T>)) && requires {
	{ async_native_buffer_completion_define(io_type<T>) } noexcept -> ::std::same_as<::std::true_type>;
};

/**
 * @brief Recognizes a composed device read implemented by a coroutine.
 * @details A composed operation may need both transport directions before it
 * returns a byte completion. Kernel-only optimizations require their separate
 * physical capability and cannot bypass the protocol or authentication.
 */
template <typename T>
concept composed_input = requires(T &s, ::std::byte *p, ::std::size_t n) {
	{ async_read_some_define(s, p, n) } -> ::std::same_as<task<byte_completion>>;
};

/** @brief Recognizes an asynchronous application write, including multi-step protocol processing. */
template <typename T>
concept composed_output = requires(T &s, ::std::byte const *p, ::std::size_t n) {
	{ async_write_some_define(s, p, n) } -> ::std::same_as<task<byte_completion>>;
};

/** @brief Accepts a native read request or an application-level composed read. */
template <typename T>
concept awaitable_input = native_input<T> || composed_input<T>;

/** @brief Accepts a native write request or an application-level composed write. */
template <typename T>
concept awaitable_output = native_output<T> || composed_output<T>;

/** @brief Permits the issuer to advance an independent formatter while a sealed prefix is borrowed.
 * @details The device returns each native completion to the same serial issuer. While the issuer
 * is executing a formatting step, another thread may use the immutable bytes but cannot resume the
 * writing coroutine. The live destination must separately accept a split of this particular record.
 */
template <typename T>
concept issuer_prefix_output = prefix_output<T> && native_output<T> && requires {
	{ async_output_prefix_on_issuer_define(io_type<T>) } noexcept -> ::std::same_as<::std::true_type>;
};

/**
 * @brief Obtains an input observer scoped to one sequential asynchronous operation.
 * @details Reads on the sequence use the same cursor and report the same short
 * reads, EOF, and errors as the original observer. The caller retains the original
 * endpoint and the sequence until all borrowed storage has been released.
 */
template <typename T>
concept native_input_sequence = requires(T &stream) {
	{ async_input_sequence_define(stream) } noexcept -> native_input;
};

/**
 * @brief Obtains an output observer scoped to one sequential asynchronous operation.
 * @details A provider may retain properties that remain true throughout the borrow. Its writes must
 * have the same effects, short completions and errors as writes to the original observer. The result
 * lives in the issuing coroutine until every outstanding request has completed.
 */
template <typename T>
concept native_output_sequence = requires(T &stream) {
	{ async_output_sequence_define(stream) } noexcept -> native_output;
};

} // namespace fast_io::async

namespace fast_io::async
{

/**
 * @brief Describes progress made by an optional device-pair transfer operation.
 * @details An incomplete success hands the advanced public cursors back to the generic transfer loop.
 * A complete success denotes logical EOF. The count uses public elements, exactly as transmit_result does.
 */
struct transfer_completion
{
	::fast_io::transmit_result progress{};
	bool complete{};
	native_error error{};
};

/**
 * @brief Recognizes a directed device pair that can keep one native request in the calling frame.
 * @details Construction must be nonthrowing. A provider declining the request publishes an incomplete
 * result with its exact committed prefix, so the public transfer can continue at both current cursors.
 */
template <typename I, typename O>
concept native_transfer_request = requires(I &input, O &output) {
	{ async_transmit_until_eof_request_define(input, output) } noexcept -> completion_awaiter<transfer_completion>;
};

/** @brief Recognizes either a frame-owned native transfer or an existing task-returning pair provider. */
template <typename I, typename O>
concept direct_transfer = native_transfer_request<I, O> || requires(I &input, O &output) {
	{ async_transmit_until_eof_define(input, output) } -> ::std::same_as<task<transfer_completion>>;
};

} // namespace fast_io::async
