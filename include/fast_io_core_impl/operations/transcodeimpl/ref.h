#pragma once

/**
 * @file
 * @brief Exposes stream-reference projections and CPOs for transcoder adapters.
 *
 * Refs are trivial borrowed handles. They expose only sequential buffered and
 * typed/byte operations whose semantics the adapter can preserve; scatter,
 * seek, positioned I/O, and native handles are intentionally not forwarded.
 * The duplex ref delegates each CPO to the corresponding independent child.
 */

namespace fast_io
{

/** @brief Trivial borrowed output observer for a standalone output adapter. */
template <typename owner>
struct basic_otranscoder_ref
{
	static constexpr bool output_secure_clear{owner::traits_type::secure_clear};
	using owner_type = owner;
	using decorated_stream_type = typename owner::decorated_stream_type;
	using output_char_type = typename owner_type::output_char_type;

	owner_type *ptr{};
};

/** @brief Trivial borrowed input observer for a standalone input adapter. */
template <typename owner>
struct basic_itranscoder_ref
{
	static constexpr bool input_secure_clear{owner::traits_type::secure_clear};
	using owner_type = owner;
	using decorated_stream_type = typename owner::decorated_stream_type;
	using input_char_type = typename owner_type::input_char_type;

	owner_type *ptr{};
};

/** @brief Trivial borrowed duplex observer projected to directional children. */
template <typename owner>
struct basic_iotranscoder_ref
{
	static constexpr bool input_secure_clear{owner::input_traits_type::secure_clear};
	static constexpr bool output_secure_clear{owner::output_traits_type::secure_clear};
	using owner_type = owner;
	using decorated_stream_type = typename owner::decorated_stream_type;
	using input_char_type = typename owner_type::input_char_type;
	using output_char_type = typename owner_type::output_char_type;

	owner_type *ptr{};
};

/** @brief Certifies that a byte-wide public buffer uses the same cursor for byte and typed transfers. */
template <typename C>
inline constexpr ::std::true_type transcode_byte_window_define(io_type_t<basic_itranscoder_ref<C>>) noexcept
{
	return {};
}

/** @brief Certifies that a byte-wide public buffer uses the same cursor for byte and typed transfers. */
template <typename C>
inline constexpr ::std::true_type transcode_byte_window_define(io_type_t<basic_otranscoder_ref<C>>) noexcept
{
	return {};
}

/** @brief Certifies that a byte-wide public buffer uses the same cursor for byte and typed transfers. */
template <typename C>
inline constexpr ::std::true_type transcode_byte_window_define(io_type_t<basic_iotranscoder_ref<C>>) noexcept
{
	return {};
}

/** @brief Gives this decorator instance its own observable path component. */
template <typename Owner>
inline constexpr auto async_layer_kind_define(io_type_t<basic_itranscoder_ref<Owner>>) noexcept
{ return ::fast_io::async::point_kind::decorator; }

/** @brief Gives this decorator instance its own observable path component. */
template <typename Owner>
inline constexpr auto async_layer_kind_define(io_type_t<basic_otranscoder_ref<Owner>>) noexcept
{ return ::fast_io::async::point_kind::decorator; }

/** @brief Gives this decorator instance its own observable path component. */
template <typename Owner>
inline constexpr auto async_layer_kind_define(io_type_t<basic_iotranscoder_ref<Owner>>) noexcept
{ return ::fast_io::async::point_kind::decorator; }

/** @brief Obtains the common physical child without bypassing either of the duplex engines. */
template <typename Owner>
inline constexpr auto async_stage_child_define(io_type_t<basic_iotranscoder_ref<Owner>>) noexcept
{ return ::std::type_identity<::std::remove_cvref_t<decltype(::std::declval<typename Owner::handle_storage_type &>().get())>>{}; }

/** @brief Identifies the underlying output for stage introspection without exposing a bypass around the encoder. */
template <typename Owner>
inline constexpr auto async_stage_child_define(io_type_t<basic_otranscoder_ref<Owner>>) noexcept
{
	return ::std::type_identity<::std::remove_cvref_t<decltype(::std::declval<typename Owner::handle_storage_type &>().get())>>{};
}

/** @brief Identifies the underlying input for stage introspection while preserving all decoding work. */
template <typename Owner>
inline constexpr auto async_stage_child_define(io_type_t<basic_itranscoder_ref<Owner>>) noexcept
{
	return ::std::type_identity<::std::remove_cvref_t<decltype(::std::declval<typename Owner::handle_storage_type &>().get())>>{};
}

/** @brief Borrows an lvalue output adapter as its lightweight stream observer. */
template <::std::integral public_char, typename handle_storage, typename engine,
		  typename traits>
inline constexpr auto output_stream_ref_define(
	basic_otranscoder<public_char, handle_storage, engine, traits> &value) noexcept
{
	return basic_otranscoder_ref<
		basic_otranscoder<public_char, handle_storage, engine, traits>>{
		__builtin_addressof(value)};
}

/** @brief Borrows a temporary output adapter for its enclosing guarded operation. */
template <::std::integral public_char, typename handle_storage, typename engine,
		  typename traits>
inline constexpr auto output_stream_ref_define(
	basic_otranscoder<public_char, handle_storage, engine, traits> &&value) noexcept
{
	return basic_otranscoder_ref<
		basic_otranscoder<public_char, handle_storage, engine, traits>>{
		__builtin_addressof(value)};
}

/** @brief Preserves an already normalized output adapter observer. */
template <typename owner>
inline constexpr basic_otranscoder_ref<owner> output_stream_ref_define(
	basic_otranscoder_ref<owner> ref) noexcept
{
	return ref;
}

/** @brief Declares trivial output observers safe for value transport. */
template <typename owner>
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	io_type_t<basic_otranscoder_ref<owner>>) noexcept
{
	// These refs contain one owner pointer and are safe to materialize by value
	// at public normalization boundaries.
	return {};
}

/** @brief Returns the beginning of the output adapter's public put area. */
template <typename owner>
inline typename owner::output_char_type *obuffer_begin(
	basic_otranscoder_ref<owner> ref)
{
	ref.ptr->require_typed_write_boundary();
	return ref.ptr->public_buffer.buffer_begin;
}

/** @brief Returns the committed cursor of the public put area. */
template <typename owner>
inline typename owner::output_char_type *obuffer_curr(
	basic_otranscoder_ref<owner> ref)
{
	ref.ptr->require_typed_write_boundary();
	return ref.ptr->public_buffer.buffer_curr;
}

/** @brief Returns the end of the currently available public put area. */
template <typename owner>
inline typename owner::output_char_type *obuffer_end(
	basic_otranscoder_ref<owner> ref)
{
	ref.ptr->require_typed_write_boundary();
	return ref.ptr->public_buffer.buffer_end;
}

/** @brief Commits a caller-advanced public put-area cursor. */
template <typename owner>
inline void obuffer_set_curr(basic_otranscoder_ref<owner> ref,
							 typename owner::output_char_type *current)
{
	ref.ptr->require_typed_write_boundary();
	auto const validated{
		::fast_io::details::validate_transcode_closed_range_offsets(
			ref.ptr->public_buffer.buffer_begin,
			ref.ptr->public_buffer.buffer_end, current)};
	if (!validated.valid) [[unlikely]]
	{
		// Public print CPO callers are outside the adapter's ownership boundary.
		// Reject their cursor unless an aligned closed-range offset is proven.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	ref.ptr->public_buffer.buffer_curr = validated.current;
}

/** @brief Reserves only already allocated public code units of an open, aligned encoder. */
template <typename Owner>
inline auto output_stream_try_reserve_define(basic_otranscoder_ref<Owner> ref, ::std::size_t n) noexcept -> typename Owner::output_char_type *
{
	auto &owner{*ref.ptr};
	auto &put{owner.public_buffer};
	if (owner.state != otranscoder_state::open || owner.source_remainder.size != 0u || put.buffer_curr == nullptr ||
		static_cast<::std::size_t>(put.buffer_end - put.buffer_curr) < n)
	{
		return nullptr;
	}
	return put.buffer_curr;
}

/** @brief Publishes the prefix produced in a previously reserved typed encoder window. */
template <typename Owner>
inline void output_stream_commit_reserved_define(basic_otranscoder_ref<Owner> ref, typename Owner::output_char_type *last) noexcept
{
	ref.ptr->public_buffer.buffer_curr = last;
}

/**
 * @brief Commits complete public code units to an open encoder without running the codec or flushing the sink.
 * @details A closed direction, pending partial object, or full buffer declines the request before any change.
 * The ordinary write protocol then supplies its original typed error or performs the required device work.
 * Only the outermost put area is inspected, so every inner layer retains its own state and finish ordering.
 */
template <typename Owner>
inline bool output_stream_try_write_define(basic_otranscoder_ref<Owner> ref,
										   typename Owner::output_char_type const *first, typename Owner::output_char_type const *last) noexcept
{
	auto &adapter{*ref.ptr};
	if (adapter.state != otranscoder_state::open || adapter.source_remainder.size != 0u)
	{
		return false;
	}
	if (first == last)
	{
		return true;
	}
	auto &put{adapter.public_buffer};
	if (put.buffer_curr == nullptr || put.buffer_end - put.buffer_curr < last - first)
	{
		return false;
	}
	put.buffer_curr = ::fast_io::details::non_overlapped_copy(first, last, put.buffer_curr);
	return true;
}

/** @brief Declares put-area pointer subtraction valid within one allocation. */
template <typename owner>
inline constexpr ::std::true_type obuffer_address_distance_safe_define(
	io_reserve_type_t<typename owner::output_char_type,
					  basic_otranscoder_ref<owner>>) noexcept
{
	return {};
}

template <typename Owner>
inline constexpr ::std::size_t output_stream_transcode_bytes_define(io_type_t<basic_otranscoder_ref<Owner>>) noexcept
{
	return Owner::traits_type::public_buffer_size * sizeof(typename Owner::output_char_type);
}

template <typename Owner>
inline constexpr ::std::size_t input_stream_transcode_bytes_define(io_type_t<basic_itranscoder_ref<Owner>>) noexcept
{
	return Owner::traits_type::public_buffer_size * sizeof(typename Owner::input_char_type);
}

/** @brief Returns the configured minimum public put-area capacity. */
template <typename owner>
inline constexpr ::std::size_t obuffer_minimum_size_define(
	io_reserve_type_t<typename owner::output_char_type,
					  basic_otranscoder_ref<owner>>) noexcept
{
	return owner::traits_type::public_buffer_size;
}

/** @brief Processes pending output and prepares a minimum-sized public put area. */
template <typename owner>
inline void obuffer_minimum_size_flush_prepare_define(
	basic_otranscoder_ref<owner> ref)
{
	// Allocation and prefix transformation may throw, so preparation remains a
	// regular CPO rather than being hidden in a noexcept cursor accessor.
	ref.ptr->prepare_put_area();
}

/** @brief Routes typed overflow through the output adapter's bounded engine loop. */
template <typename owner>
inline void write_all_overflow_define(
	basic_otranscoder_ref<owner> ref,
	typename owner::output_char_type const *first,
	typename owner::output_char_type const *last)
{
	ref.ptr->write_all_overflow(first, last);
}

/** @brief Routes byte overflow through representation-aware source binding. */
template <typename owner>
inline void write_all_bytes_overflow_define(
	basic_otranscoder_ref<owner> ref, ::std::byte const *first,
	::std::byte const *last)
{
	ref.ptr->write_all_bytes_overflow(first, last);
}

/** @brief Performs nonterminal output sync-flush through the adapter owner. */
template <typename owner>
inline void output_stream_buffer_flush_define(
	basic_otranscoder_ref<owner> ref)
{
	ref.ptr->flush_legacy();
}

/** @brief Performs checked terminal output finish through the adapter owner. */
template <typename owner>
inline void output_stream_finish_define(basic_otranscoder_ref<owner> ref)
{
	ref.ptr->finish_legacy();
}

/** @brief Borrows an lvalue input adapter as its lightweight stream observer. */
template <::std::integral public_char, typename handle_storage, typename engine,
		  typename traits>
inline constexpr auto input_stream_ref_define(
	basic_itranscoder<public_char, handle_storage, engine, traits> &value) noexcept
{
	return basic_itranscoder_ref<
		basic_itranscoder<public_char, handle_storage, engine, traits>>{
		__builtin_addressof(value)};
}

/** @brief Borrows a temporary input adapter without implying automatic drain. */
template <::std::integral public_char, typename handle_storage, typename engine,
		  typename traits>
inline constexpr auto input_stream_ref_define(
	basic_itranscoder<public_char, handle_storage, engine, traits> &&value) noexcept
{
	return basic_itranscoder_ref<
		basic_itranscoder<public_char, handle_storage, engine, traits>>{
		__builtin_addressof(value)};
}

/** @brief Preserves an already normalized input adapter observer. */
template <typename owner>
inline constexpr basic_itranscoder_ref<owner> input_stream_ref_define(
	basic_itranscoder_ref<owner> ref) noexcept
{
	return ref;
}

/** @brief Declares trivial input observers safe for value transport. */
template <typename owner>
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	io_type_t<basic_itranscoder_ref<owner>>) noexcept
{
	return {};
}

/** @brief Returns the beginning of the caller-visible input get area. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_begin(
	basic_itranscoder_ref<owner> ref)
{
	ref.ptr->prepare_get_area();
	return ref.ptr->public_buffer.buffer_begin;
}

/** @brief Returns the current caller-visible input cursor. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_curr(
	basic_itranscoder_ref<owner> ref)
{
	ref.ptr->prepare_get_area();
	return ref.ptr->public_buffer.buffer_curr;
}

/** @brief Returns the end of the caller-visible input get area. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_end(
	basic_itranscoder_ref<owner> ref)
{
	ref.ptr->prepare_get_area();
	return ref.ptr->public_buffer.buffer_end;
}

/** @brief Validates and commits a caller-advanced public get-area cursor. */
template <typename owner>
inline void ibuffer_set_curr(
	basic_itranscoder_ref<owner> ref,
	typename owner::input_char_type *current)
{
	// Scanner code normally supplies a valid cursor. Validate the public commit
	// nevertheless, because accepting an out-of-range pointer corrupts every
	// later byte-boundary calculation.
	ref.ptr->require_typed_read_boundary();
	auto const validated{
		::fast_io::details::validate_transcode_closed_range_offsets(
			ref.ptr->public_buffer.buffer_begin,
			ref.ptr->public_buffer.buffer_end, current)};
	if (!validated.valid) [[unlikely]]
	{
		// Scanner-provided cursors are not trusted to share allocation provenance;
		// validate integer offsets before rebuilding the committed cursor.
		::fast_io::throw_transcode_stream_error(
			::fast_io::transcode_stream_errc::protocol_violation);
	}
	ref.ptr->public_buffer.buffer_curr = validated.current;
}

/** @brief Refills the input adapter's public get area when possible. */
template <typename owner>
[[nodiscard]] inline bool ibuffer_underflow(
	basic_itranscoder_ref<owner> ref)
{
	return ref.ptr->underflow();
}

#if defined(__HERBCEPTIONS__)
namespace details
{
/** @brief Admits a checked decoder when its engine or child supplies deterministic errors. */
template <typename Owner>
concept transcode_input_checked =
	operations::decay::defines::has_transcode_process_checked_define<typename Owner::engine_ref_type> ||
	operations::decay::defines::has_transcode_finish_checked_define<typename Owner::engine_ref_type> ||
	requires(typename Owner::normalized_input_ref_type &child) {
		{ ibuffer_underflow_checked_define(child, ::std::size_t{}) } -> ::std::same_as<bool>;
	};

/** @brief Delivers the first checked failure after the decoder and child release their guards. */
template <typename Owner, typename Read>
inline auto transcode_input_checked_step(basic_itranscoder_ref<Owner> ref, Read read) throws
{
	transcode_input_error_slot steps;
	auto result{read(*ref.ptr, steps)};
	if (steps.failure.domain != nullptr) [[unlikely]]
	{
		ref.ptr->state = itranscoder_state::failed;
		throw throws steps.failure;
	}
	return result;
}
} // namespace details

/**
 * @brief Refills a scanner's get area and delivers the engine's native error after releasing the child lock.
 * @details A false result means logical EOF only. A failing process or finish publishes no speculative bytes
 * and poisons the affected input direction before control returns to the caller.
 */
template <typename Owner>
	requires details::transcode_input_checked<Owner>
[[nodiscard]] inline bool ibuffer_underflow_checked_define(basic_itranscoder_ref<Owner> ref, ::std::size_t reserve) throws
{
	return details::transcode_input_checked_step(ref, [reserve](Owner &input, auto &steps) {
		return input.underflow(reserve, steps);
	});
}

/** @brief Reads a typed or byte staging prefix while preserving checked decoder failures.
 * @details Byte reads retain the adapter's partial-object cursor, including on a wide public stream.
 * Returning the original destination means clean EOF only. An engine failure poisons the adapter.
 */
template <typename Owner, typename Unit>
	requires details::transcode_input_checked<Owner> &&
			 (::std::same_as<Unit, typename Owner::input_char_type> || ::std::same_as<Unit, ::std::byte>)
inline Unit *transcode_read_some_checked_define(basic_itranscoder_ref<Owner> ref, Unit *first, Unit *last) throws
{
	return details::transcode_input_checked_step(ref, [first, last](Owner &input, auto &steps) {
		if constexpr (::std::same_as<Unit, ::std::byte>)
			return input.read_some_bytes_impl(first, last, steps);
		else
			return input.read_some_impl(first, last, steps);
	});
}
#endif

/** @brief A scanner or transfer may request room for a known extent on its next logical refill.
 * @details Short reads remain valid. The published prefix stays live until the next refill.
 */
template <typename Owner>
[[nodiscard]] inline bool ibuffer_underflow_reserve_define(basic_itranscoder_ref<Owner> ref, ::std::size_t n)
{
	return ref.ptr->underflow(n);
}

/** @brief Declares that this stateful input may require underflow work. */
template <typename owner>
inline constexpr bool ibuffer_underflow_never(
	basic_itranscoder_ref<owner>) noexcept
{
	return false;
}

/** @brief Routes typed read_some underflow through the input adapter. */
template <typename owner>
inline typename owner::input_char_type *read_some_underflow_define(
	basic_itranscoder_ref<owner> ref,
	typename owner::input_char_type *first,
	typename owner::input_char_type *last)
{
	return ref.ptr->read_some(first, last);
}

/** @brief Routes byte read_some underflow through the input adapter. */
template <typename owner>
inline ::std::byte *read_some_bytes_underflow_define(
	basic_itranscoder_ref<owner> ref, ::std::byte *first,
	::std::byte *last)
{
	return ref.ptr->read_some_bytes(first, last);
}

/** @brief Projects a duplex observer to its independent input child observer. */
template <typename owner>
inline constexpr auto iotranscoder_input_ref(
	basic_iotranscoder_ref<owner> ref) noexcept
{
	// Directional projection preserves the child owner type, so ordinary input
	// concepts see exactly the same surface as a standalone basic_itranscoder.
	return basic_itranscoder_ref<typename owner::input_adapter_type>{
		__builtin_addressof(ref.ptr->input)};
}

/** @brief Projects a duplex observer to its independent output child observer. */
template <typename owner>
inline constexpr auto iotranscoder_output_ref(
	basic_iotranscoder_ref<owner> ref) noexcept
{
	// Output projection likewise prevents duplex-only state from leaking into
	// the primitive output dispatch graph.
	return basic_otranscoder_ref<typename owner::output_adapter_type>{
		__builtin_addressof(ref.ptr->output)};
}

/** @brief Borrows an lvalue duplex adapter as one joint stream observer. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto io_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &value) noexcept
{
	return basic_iotranscoder_ref<::std::remove_reference_t<decltype(value)>>{
		__builtin_addressof(value)};
}

/** @brief Borrows a temporary duplex adapter for its enclosing operation. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto io_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &&value) noexcept
{
	return basic_iotranscoder_ref<::std::remove_reference_t<decltype(value)>>{
		__builtin_addressof(value)};
}

/** @brief Preserves an already normalized duplex stream observer. */
template <typename owner>
inline constexpr basic_iotranscoder_ref<owner> io_stream_ref_define(
	basic_iotranscoder_ref<owner> ref) noexcept
{
	return ref;
}

/** @brief Projects a normalized duplex observer to its input direction. */
template <typename owner>
inline constexpr auto input_stream_ref_define(
	basic_iotranscoder_ref<owner> ref) noexcept
{
	return iotranscoder_input_ref(ref);
}

/** @brief Projects a normalized duplex observer to its output direction. */
template <typename owner>
inline constexpr auto output_stream_ref_define(
	basic_iotranscoder_ref<owner> ref) noexcept
{
	return iotranscoder_output_ref(ref);
}

/** @brief Directly obtains the input projection of an lvalue duplex adapter. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto input_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &value) noexcept
{
	return iotranscoder_input_ref(io_stream_ref_define(value));
}

/** @brief Directly obtains the input projection of a temporary duplex adapter. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto input_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &&value) noexcept
{
	return iotranscoder_input_ref(io_stream_ref_define(value));
}

/** @brief Directly obtains the output projection of an lvalue duplex adapter. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto output_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &value) noexcept
{
	return iotranscoder_output_ref(io_stream_ref_define(value));
}

/** @brief Directly obtains the output projection of a temporary duplex adapter. */
template <::std::integral input_public_char,
		  ::std::integral output_public_char, typename handle_storage,
		  typename input_engine, typename output_engine,
		  typename input_traits, typename output_traits>
inline constexpr auto output_stream_ref_define(
	basic_iotranscoder<input_public_char, output_public_char, handle_storage,
					   input_engine, output_engine, input_traits,
					   output_traits> &&value) noexcept
{
	return iotranscoder_output_ref(io_stream_ref_define(value));
}

/** @brief Declares trivial duplex observers safe for value transport. */
template <typename owner>
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	io_type_t<basic_iotranscoder_ref<owner>>) noexcept
{
	return {};
}

/** @brief Delegates duplex get-area begin to the input child. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_begin(
	basic_iotranscoder_ref<owner> ref)
{
	return ibuffer_begin(iotranscoder_input_ref(ref));
}

/** @brief Delegates duplex get-area current to the input child. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_curr(
	basic_iotranscoder_ref<owner> ref)
{
	return ibuffer_curr(iotranscoder_input_ref(ref));
}

/** @brief Delegates duplex get-area end to the input child. */
template <typename owner>
inline typename owner::input_char_type *ibuffer_end(
	basic_iotranscoder_ref<owner> ref)
{
	return ibuffer_end(iotranscoder_input_ref(ref));
}

/** @brief Delegates duplex input cursor commit to the input child. */
template <typename owner>
inline void ibuffer_set_curr(
	basic_iotranscoder_ref<owner> ref,
	typename owner::input_char_type *current)
{
	ibuffer_set_curr(iotranscoder_input_ref(ref), current);
}

/** @brief Delegates duplex underflow to the input child. */
template <typename owner>
[[nodiscard]] inline bool ibuffer_underflow(
	basic_iotranscoder_ref<owner> ref)
{
	return ibuffer_underflow(iotranscoder_input_ref(ref));
}

/** @brief Declares that duplex input may require stateful underflow. */
template <typename owner>
inline constexpr bool ibuffer_underflow_never(
	basic_iotranscoder_ref<owner>) noexcept
{
	return false;
}

/** @brief Delegates duplex typed read_some to the input child. */
template <typename owner>
inline typename owner::input_char_type *read_some_underflow_define(
	basic_iotranscoder_ref<owner> ref,
	typename owner::input_char_type *first,
	typename owner::input_char_type *last)
{
	return read_some_underflow_define(iotranscoder_input_ref(ref), first, last);
}

/** @brief Delegates duplex byte read_some to the input child. */
template <typename owner>
inline ::std::byte *read_some_bytes_underflow_define(
	basic_iotranscoder_ref<owner> ref, ::std::byte *first,
	::std::byte *last)
{
	return read_some_bytes_underflow_define(
		iotranscoder_input_ref(ref), first, last);
}

/** @brief Reserves output through the encoder projection of a bidirectional adapter. */
template <typename Owner>
inline auto output_stream_try_reserve_define(basic_iotranscoder_ref<Owner> ref, ::std::size_t n) noexcept
{
	return output_stream_try_reserve_define(iotranscoder_output_ref(ref), n);
}

/** @brief Publishes a previously reserved output prefix without touching the decoder cursor. */
template <typename Owner>
inline void output_stream_commit_reserved_define(basic_iotranscoder_ref<Owner> ref, typename Owner::output_char_type *last) noexcept
{
	output_stream_commit_reserved_define(iotranscoder_output_ref(ref), last);
}

/** @brief Tests only the encoder's public put area when this duplex observer occupies an output position. */
template <typename Owner>
inline bool output_stream_try_write_define(basic_iotranscoder_ref<Owner> ref,
										   typename Owner::output_char_type const *first, typename Owner::output_char_type const *last) noexcept
{
	return output_stream_try_write_define(iotranscoder_output_ref(ref), first, last);
}

/** @brief Delegates duplex put-area begin to the output child. */
template <typename owner>
inline typename owner::output_char_type *obuffer_begin(
	basic_iotranscoder_ref<owner> ref)
{
	return obuffer_begin(iotranscoder_output_ref(ref));
}

/** @brief Delegates duplex put-area current to the output child. */
template <typename owner>
inline typename owner::output_char_type *obuffer_curr(
	basic_iotranscoder_ref<owner> ref)
{
	return obuffer_curr(iotranscoder_output_ref(ref));
}

/** @brief Delegates duplex put-area end to the output child. */
template <typename owner>
inline typename owner::output_char_type *obuffer_end(
	basic_iotranscoder_ref<owner> ref)
{
	return obuffer_end(iotranscoder_output_ref(ref));
}

/** @brief Delegates duplex output cursor commit to the output child. */
template <typename owner>
inline void obuffer_set_curr(
	basic_iotranscoder_ref<owner> ref,
	typename owner::output_char_type *current)
{
	obuffer_set_curr(iotranscoder_output_ref(ref), current);
}

/** @brief Declares duplex put-area pointer subtraction safe. */
template <typename owner>
inline constexpr ::std::true_type obuffer_address_distance_safe_define(
	io_reserve_type_t<typename owner::output_char_type,
					  basic_iotranscoder_ref<owner>>) noexcept
{
	return {};
}

/** @brief Returns the output child's configured put-area minimum. */
template <typename owner>
inline constexpr ::std::size_t obuffer_minimum_size_define(
	io_reserve_type_t<typename owner::output_char_type,
					  basic_iotranscoder_ref<owner>>) noexcept
{
	return owner::output_traits_type::public_buffer_size;
}

/** @brief Prepares the duplex output child's minimum-sized put area. */
template <typename owner>
inline void obuffer_minimum_size_flush_prepare_define(
	basic_iotranscoder_ref<owner> ref)
{
	ref.ptr->output.prepare_put_area();
}

/** @brief Delegates duplex typed overflow to the output child. */
template <typename owner>
inline void write_all_overflow_define(
	basic_iotranscoder_ref<owner> ref,
	typename owner::output_char_type const *first,
	typename owner::output_char_type const *last)
{
	ref.ptr->output.write_all_overflow(first, last);
}

/** @brief Delegates duplex byte overflow to the output child. */
template <typename owner>
inline void write_all_bytes_overflow_define(
	basic_iotranscoder_ref<owner> ref, ::std::byte const *first,
	::std::byte const *last)
{
	ref.ptr->output.write_all_bytes_overflow(first, last);
}

/** @brief Sync-flushes only the duplex output child. */
template <typename owner>
inline void output_stream_buffer_flush_define(
	basic_iotranscoder_ref<owner> ref)
{
	ref.ptr->output.flush_legacy();
}

/** @brief Terminally finishes only the duplex output child. */
template <typename owner>
inline void output_stream_finish_define(basic_iotranscoder_ref<owner> ref)
{
	ref.ptr->finish_output();
}

} // namespace fast_io
