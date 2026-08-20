#pragma once

/**
 * @file
 * @brief Defines the bounded transform-protocol vocabulary.
 *
 * A transcoder engine only consumes and produces bounded ranges. Streams,
 * buffering, allocation, EOF, and operation lifetime belong to adapters above
 * this layer.
 */

namespace fast_io
{

/** @brief Restricts transcoder endpoints to integral code units or raw bytes. */
template <typename T>
concept transcode_unit =
	::std::integral<::std::remove_cv_t<T>> ||
	::std::same_as<::std::remove_cv_t<T>, ::std::byte>;

/** @brief Tag used to query buffer-size guarantees without constructing an engine. */
template <typename T>
struct transcode_reserve_t
{
	/** @brief Constructs the stateless reserve-query tag. */
	inline explicit constexpr transcode_reserve_t() noexcept = default;
};

template <typename T>
inline constexpr transcode_reserve_t<T> transcode_reserve{};

/** @brief Describes why a bounded process call returned to its caller. */
enum class transcode_step_status : ::std::uint_least8_t
{
	// The complete supplied source range was accepted; from_next equals from_last.
	need_input,
	// A fresh destination can make progress. The old destination need not be full.
	need_output
};

/** @brief Reports the exact source and destination cursors after a process step. */
template <transcode_unit from_type, transcode_unit to_type>
struct basic_transcode_process_result
{
	using from_value_type = from_type;
	using to_value_type = to_type;

	// First unconsumed source and first unwritten destination. Both must remain
	// inside the exact ranges supplied to the process call.
	from_value_type const *from_next{};
	to_value_type *to_next{};
	transcode_step_status status{};
};

/** @brief Describes whether a drain phase completed or needs another buffer. */
enum class transcode_drain_status : ::std::uint_least8_t
{
	// This drain phase currently has no more output.
	complete,
	// A fresh destination is required to continue the same drain phase.
	need_output
};

/** @brief Reports the destination cursor and status of a bounded drain step. */
template <transcode_unit to_type>
struct basic_transcode_drain_result
{
	using to_value_type = to_type;

	// First unwritten destination in the bounded drain buffer.
	to_value_type *to_next{};
	transcode_drain_status status{};
};

/** @brief Identifies the protocol phase for which output capacity is queried. */
enum class transcode_phase : ::std::uint_least8_t
{
	process,
	// Nonterminal visibility boundary; the engine remains usable afterward.
	sync_flush,
	// Terminal message boundary, including validation and trailers.
	finish
};

namespace details
{

/** @brief Supplies no endpoint aliases until an engine exposes both endpoints. */
template <typename T, typename = void>
struct transcoder_ref_endpoint_types
{};

/** @brief Copies endpoint aliases from a structurally valid engine type. */
template <typename T>
struct transcoder_ref_endpoint_types<T, ::std::void_t<
											typename T::from_value_type,
											typename T::to_value_type>>
{
	using from_value_type = typename T::from_value_type;
	using to_value_type = typename T::to_value_type;
};

} // namespace details

/** @brief Non-owning default observer used to dispatch transcoder CPOs. */
template <typename T>
struct transcoder_ref : ::fast_io::details::transcoder_ref_endpoint_types<T>
{
	// The default engine observer is deliberately a trivial borrow. Providers
	// with another stable observer shape customize transcode_ref_define.
	using value_type = T;
	using pointer = T *;

	pointer ptr{};

	/** @brief Constructs an empty observer; it must be assigned before use. */
	inline constexpr transcoder_ref() noexcept = default;
	/** @brief Borrows an existing engine without extending its lifetime. */
	inline explicit constexpr transcoder_ref(pointer value) noexcept : ptr(value)
	{}
};

namespace operations::defines
{

/** @brief Detects an engine-provided observer customization. */
template <typename T>
concept has_transcode_ref_define = requires(T &engine) {
	transcode_ref_define(engine);
};

} // namespace operations::defines

/** @brief Normalizes an engine to its customized or default borrowed observer. */
template <typename T>
inline constexpr decltype(auto) transcode_ref(T &engine) noexcept
{
	if constexpr (::fast_io::operations::defines::has_transcode_ref_define<T>)
	{
		// Preserve a provider-defined observer and its associated CPO surface.
		return transcode_ref_define(engine);
	}
	else
	{
		// Fall back to a trivial pointer observer for directly customized engines.
		return transcoder_ref<::std::remove_reference_t<T>>{__builtin_addressof(engine)};
	}
}

} // namespace fast_io
