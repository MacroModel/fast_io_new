#pragma once

namespace fast_io::async
{

namespace details
{
/** @brief Determines whether either direction of a device can require a hosted issuer. */
template <typename T>
consteval bool needs_scheduler() noexcept
{
	using stream = ::std::remove_reference_t<T>;
	if constexpr (input_device<T>)
	{
		using ref = decltype(::fast_io::operations::input_stream_ref(::std::declval<stream &>()));
		if constexpr (!immediate_input<ref>)
		{
			return true;
		}
	}
	if constexpr (output_device<T>)
	{
		using ref = decltype(::fast_io::operations::output_stream_ref(::std::declval<stream &>()));
		if constexpr (!immediate_output<ref>)
		{
			return true;
		}
	}
	return false;
}

/** @brief Finds the issuer supplied by the included device package without creating a global startup initializer. */
template <typename T>
consteval auto stream_scheduler() noexcept
{
	if constexpr (needs_scheduler<T>() && requires { async_scheduler_define(::fast_io::io_type<::std::remove_cvref_t<T>>); })
	{
		return async_scheduler_define(::fast_io::io_type<::std::remove_cvref_t<T>>);
	}
	else
	{
		return nullptr;
	}
}
} // namespace details

namespace details
{
/** @brief Installs a lazy hosted driver for a real file while retaining the original endpoint's ownership. */
template <bool Selected, typename Path = void, typename T>
constexpr decltype(auto) select_endpoint(T &&stream)
{
	if constexpr (Selected)
	{
		constexpr auto enable{stream_scheduler<T>()};
		if constexpr (!::std::is_same_v<::std::remove_cv_t<decltype(enable)>, ::std::nullptr_t>)
			if (!::std::is_constant_evaluated() && enable != nullptr) enable();
		return device<T, Path>{static_cast<T &&>(stream)};
	}
	else return static_cast<T &&>(stream);
}
} // namespace details

namespace details
{

/** @brief Restores an endpoint's original ownership category for the existing output-finish guard. */
template <typename T>
constexpr decltype(auto) operand(T &&value) noexcept
{
	if constexpr (is_device<::std::remove_cvref_t<T>>)
	{
		using category = typename ::std::remove_cvref_t<T>::stream_category;
		if constexpr (!::std::is_lvalue_reference_v<T> && !::std::is_lvalue_reference_v<category>)
		{
			return static_cast<::std::remove_reference_t<decltype(value.get())> &&>(value.get());
		}
		else
		{
			return value.get();
		}
	}
	else
	{
		return static_cast<T &&>(value);
	}
}

/** @brief Returns the frame-owned or borrowed endpoint in its original public category. */
template <typename T>
constexpr decltype(auto) captured_operand(capture<T> &stored) noexcept
{
	return operand(static_cast<T &&>(stored.get()));
}

template <typename T>
using operand_type = decltype(operand(::std::declval<T>()));

template <typename T>
using input_ref_type = ::std::remove_reference_t<decltype(::fast_io::operations::input_stream_ref(
	::std::declval<::std::remove_reference_t<operand_type<T>> &>()))>;

template <typename T>
using output_ref_type = ::fast_io::operations::defines::print_freestanding_named_output_t<operand_type<T>>;

/** @brief Preserves the value category of the operand when transferring its annotation into a request. */
template <typename T>
using unwrapped_ref_t = decltype(value_operand(::std::declval<T>()));
template <typename T>
using unannotated_t = ::std::conditional_t<::std::is_rvalue_reference_v<unwrapped_ref_t<T>>,
	::std::remove_reference_t<unwrapped_ref_t<T>>, unwrapped_ref_t<T>>;

/** @brief Only a marked occurrence contributes its value provider's milestones to this invocation. */
template <bool Scan, typename C, typename T>
consteval auto describe_operand()
{
	if constexpr (annotated_operand<T>)
		return describe_value<Scan,C,unannotated_t<T>>();
	else
		return default_value_points<false>{};
}

/** @brief Proves logical input and output can transfer complete, equal-width public elements. */
template <typename I, typename O>
concept transfer_pair = input_device<operand_type<I>> && output_device<operand_type<O>> &&
						(sizeof(typename input_ref_type<I>::input_char_type) == sizeof(typename output_ref_type<O>::output_char_type));

/** @brief Diagnoses a malformed progress report from a byte-completion provider. */
[[noreturn]] inline void raise_invalid_progress(::std::uint_least32_t) FAST_IO_ASYNC_THROWS
{
#if defined(__HERBCEPTIONS__)
	throw throws ::std::parse_errc::invalid;
#else
	::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
#endif
}

/** @brief Reports physical EOF partway through one typed input element. */
[[noreturn]] inline void raise_truncated_element(::std::uint_least32_t) FAST_IO_ASYNC_THROWS
{
#if defined(__HERBCEPTIONS__)
	throw throws ::std::parse_errc::end_of_file;
#else
	::fast_io::throw_parse_code(::fast_io::parse_code::end_of_file);
#endif
}

 } // namespace details
} // namespace fast_io::async
#include "decorated_io.h"
namespace fast_io::async::details
{
/** @brief Finishes an owned temporary only after all its writes succeed, allowing a worker to flush a blocking sink. */
template <typename Guard>
inline auto finish_output_guard(Guard &guard) FAST_IO_ASYNC_THROWS
{
	return invoke_device<immediate_output<typename Guard::normalized_ref_type>>([&guard]() FAST_IO_ASYNC_THROWS { guard.commit(); });
}

/** @brief Selects the native completion object when available, preserving task-based device customizations. */
template <typename I, typename O>
inline decltype(auto) direct_transfer_request(I &input, O &output) FAST_IO_ASYNC_THROWS
{
	if constexpr (native_transfer_request<I, O>)
	{
		return async_transmit_until_eof_request_define(input, output);
	}
	else
	{
		return async_transmit_until_eof_define(input, output);
	}
}

/**
 * @brief Drains native and synchronous endpoints in public element units.
 * @details One normalization and one success-only finish guard cover the entire operation. Each native read finishes
 * any partial final code unit before publishing the block. Writes retain their source storage across short completions.
 */
template <typename I, typename O>
inline task<::fast_io::transmit_result> transfer_native(capture<I> input, capture<O> output) FAST_IO_ASYNC_THROWS
{
	auto &&source{captured_operand(input)};
	auto &&destination{captured_operand(output)};
	decltype(auto) in{::fast_io::operations::input_stream_ref(source)};
	::fast_io::operations::basic_output_operation_guard<decltype(destination)> guard{destination};
	auto &out{guard.ref()};
	using input_char = typename input_ref_type<I>::input_char_type;
	using output_char = typename output_ref_type<O>::output_char_type;
	constexpr auto capacity{::fast_io::details::transmit_buffer_size_cache<sizeof(input_char)>};
	::fast_io::transmit_result result{};
	layer_activity receipt;
	if constexpr (uses_selected_io_layers<true, I, input_ref_type<I>> ||
		uses_selected_io_layers<false, O, output_ref_type<O>>) co_await receipt;
	if constexpr (direct_transfer<decltype(in), decltype(out)>)
	{
		auto const direct{co_await direct_transfer_request(in, out)};
		result = direct.progress;
		if (direct.error.raise != nullptr) [[unlikely]]
		{
			co_await store_native_error{direct.error};
			co_return result;
		}
		if (direct.complete)
		{
			if constexpr (decltype(guard)::checked_finish) co_await finish_output_guard(guard);
			co_return result;
		}
	}
	::fast_io::details::local_operator_new_array_ptr<input_char> buffer(capacity);
	decltype(auto) reader{[&]() -> decltype(auto) {
		if constexpr (marked_device<I> && native_input_sequence<input_ref_type<I>>)
		{
			return async_input_sequence_define(in);
		}
		else
		{
			return (in);
		}
	}()};
	decltype(auto) writer{[&]() -> decltype(auto) {
		if constexpr (marked_device<O> && native_output_sequence<output_ref_type<O>>)
		{
			return async_output_sequence_define(out);
		}
		else
		{
			return (out);
		}
	}()};
	::fast_io::uintfpos_transmit_reference_wrapper progress{::fast_io::freestanding::addressof(result.transmitted)};
	[[maybe_unused]] bool destination_can_coalesce{true};
	for (;;)
	{
		::std::size_t elements{};
		if constexpr (uses_selected_io_layers<true, I, input_ref_type<I>>)
		{
			auto *p{reinterpret_cast<::std::byte *>(buffer.ptr)};
			::std::size_t n{};
			if constexpr (!marked_device<O> && !layered_device<O> &&
				::std::same_as<input_char, output_char> && verified_transfer_window<::std::remove_cvref_t<decltype(in)>> &&
				!requires { async_layer_read_define(io_type<typename I::layer_path>,
					::std::integral_constant<::std::size_t, 0>{}, in, p, capacity, receipt.activity); } &&
				coalescible_transmit_output<output_ref_type<O>>)
			{
				if (destination_can_coalesce && current_loop && current_loop->standalone_root &&
					current_loop->ready.empty() && !transfer_input_tie_active(source, in))
				{
					if (!(co_await refill_layer<typename I::layer_path, 0, true>(in, receipt.activity))) break;
					if (current_loop && current_loop->standalone_root && current_loop->ready.empty() &&
						!transfer_input_tie_active(source, in))
					{
						auto const block{prepare_verified_transfer_block(*in.ptr, capacity)};
						if (block.size) destination_can_coalesce = async_output_can_coalesce_transmit_define(out);
						if (block.size && destination_can_coalesce)
						{
							::fast_io::freestanding::my_memcpy(p, in.ptr->public_buffer.buffer_curr, block.first);
							::fast_io::freestanding::my_memcpy(p + block.first, in.ptr->transform_buffer.buffer_curr,
								block.size - block.first);
							auto done{async_write_transmit_window_define(out, p, block.size)};
							// Reject an inconsistent completion before using its byte count as a cursor.
							if (done.error.raise ? done.transferred >= block.size : done.transferred != block.size)
								raise_invalid_progress(0);
							transmit_integer_add_define(progress, commit_verified_transfer_block(*in.ptr, block, done));
							if (done.error.raise)
							{
								co_await store_native_error{done.error};
								co_return result;
							}
							continue;
						}
					}
					n = static_cast<::std::size_t>(in.ptr->copy_public_bytes(p, p + capacity) - p);
				}
			}
			if (!n)
				n = co_await read_layer<typename I::layer_path, 0, true>(in, p, capacity * sizeof(input_char), receipt.activity);
			while (n % sizeof(input_char))
			{
				auto next{co_await read_layer<typename I::layer_path, 0>(in, p + n, sizeof(input_char) - n % sizeof(input_char), receipt.activity)};
				if (!next) raise_truncated_element(0);
				n += next;
			}
			elements = n / sizeof(input_char);
		}
		else if constexpr (marked_device<I> && awaitable_input<input_ref_type<I>>)
		{
			auto *bytes{reinterpret_cast<::std::byte *>(buffer.ptr)};
			::std::size_t received{}, requested{capacity * sizeof(input_char)};
			for (;;)
			{
				auto const [count, error]{co_await async_read_some_define(reader, bytes + received, requested)};
				if (error.raise != nullptr || count > requested) [[unlikely]]
				{
					co_await store_native_error{error.raise != nullptr ? error : native_error{raise_invalid_progress, 0u}};
					co_return result;
				}
				if (count == 0 && received != 0) [[unlikely]]
				{
					co_await store_native_error{{raise_truncated_element, 0u}};
					co_return result;
				}
				received += count;
				auto const partial{received % sizeof(input_char)};
				if (partial == 0)
				{
					break;
				}
				// Complete precisely the suffix of the final element.
				requested = sizeof(input_char) - partial;
			}
			elements = received / sizeof(input_char);
		}
		else
		{
			auto read_block = [&] {
				auto *end{::fast_io::operations::decay::read_some_decay_dispatch(in, buffer.ptr, buffer.ptr + capacity)};
				return static_cast<::std::size_t>(end - buffer.ptr);
			};
			// The unmarked endpoint retains its synchronous protocol. A blocking read must still let the issuer run a producer.
			if constexpr (immediate_input<input_ref_type<I>>)
			{
				elements = read_block();
			}
			else
			{
				elements = co_await invoke_blocking(read_block);
			}
		}
		if (elements == 0)
		{
			break;
		}
		if constexpr (uses_selected_io_layers<false, O, output_ref_type<O>>)
		{
			co_await write_layer<typename O::layer_path, 0, layer_action::write>(out,
				reinterpret_cast<::std::byte const *>(buffer.ptr), elements * sizeof(input_char), receipt.activity);
		}
		else if constexpr (marked_device<O> && awaitable_output<output_ref_type<O>>)
		{
			auto const *data{reinterpret_cast<::std::byte const *>(buffer.ptr)};
			auto remaining{elements * sizeof(input_char)};
			do
			{
				auto const [count, error]{co_await async_write_some_define(writer, data, remaining)};
				if (error.raise != nullptr || count == 0 || count > remaining) [[unlikely]]
				{
					co_await store_native_error{error.raise != nullptr ? error : native_error{raise_invalid_progress, 0u}};
					co_return result;
				}
				data += count;
				remaining -= count;
			} while (remaining != 0);
		}
		else
		{
			auto write_block = [&] {
				if constexpr (::std::same_as<input_char, output_char>)
				{
					write_observed(out, buffer.ptr, buffer.ptr + elements);
				}
				else
				{
					// Equal-width distinct characters have the same element boundaries.
					using alias_pointer
#if __has_cpp_attribute(__gnu__::__may_alias__)
						[[__gnu__::__may_alias__]]
#endif
						= output_char const *;
					write_observed(out, reinterpret_cast<alias_pointer>(buffer.ptr),
																		   reinterpret_cast<alias_pointer>(buffer.ptr + elements));
				}
			};
			// Backpressure on an unmarked synchronous sink can require a consumer on this same issuer.
			if constexpr (immediate_output<output_ref_type<O>>)
			{
				write_block();
			}
			else
			{
				bool committed{};
				if constexpr (!marked_device<O> && ::std::same_as<input_char, output_char> &&
					issuer_range_output<output_ref_type<O>, output_char>)
				{
					if (current_loop && current_loop->standalone_root && current_loop->ready.empty())
					{
						// This owned window is disjoint from the sink. Probe the live endpoint on each
						// write. A decline leaves the complete range available to the blocking fallback.
						native_error failure{};
						committed = async_write_range_on_issuer_define(out, buffer.ptr, buffer.ptr + elements, failure);
						if (failure.raise) [[unlikely]]
						{
							co_await store_native_error{failure};
							co_return result;
						}
					}
				}
				if (!committed) co_await invoke_blocking(write_block);
			}
		}
		transmit_integer_add_define(progress, elements);
	}
	co_await publish_observation{observation_history::input_closed,1};
	if constexpr (decltype(guard)::checked_finish && uses_selected_io_layers<false, O, output_ref_type<O>>)
		co_await write_layer<typename O::layer_path, 0, layer_action::finish>(out, nullptr, 0, receipt.activity);
	else if constexpr (decltype(guard)::checked_finish) co_await finish_output_guard(guard);
	co_return result;
}

/** @brief Selects completion I/O only when it preserves both endpoints' synchronization protocols. */
template <typename I, typename O>
inline constexpr bool native_transfer_pair =
	(direct_transfer<input_ref_type<I>, output_ref_type<O>> ||
	 (marked_device<I> && awaitable_input<input_ref_type<I>>) || (marked_device<O> && awaitable_output<output_ref_type<O>>)) &&
	!::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<input_ref_type<I>> &&
	!::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<output_ref_type<O>>;

/** @brief Bounds inline code units for a staged record before dynamic storage is needed. */
inline constexpr ::std::size_t inline_record_capacity{64u};

/**
 * @brief Retains a private spelling that can be moved into another operation or coroutine frame.
 * @details The inline form stores its length. A heap-backed record transfers ownership of the allocation
 * and its extent. The growable writer is needed only while generating the spelling.
 */
template <::std::integral Char>
class print_record
{
	Char *allocation{};
	::std::size_t size{}, capacity{};
	Char local[inline_record_capacity];

public:
	using base = ::fast_io::basic_generic_dynamic_output_buffer<Char, inline_record_capacity, ::fast_io::native_global_allocator>;

	print_record() noexcept = default;
	print_record(print_record const &) = delete;

	/** @brief Spells an independently bounded record directly into its final inline array.
	 * @details The caller proves the sum of the fields' maximum lengths, including the
	 * line ending, before selecting this overload. Only the initialized prefix is published.
	 */
	template <::std::size_t Bound, typename Emit>
		requires (Bound <= inline_record_capacity)
	FAST_IO_GNU_ALWAYS_INLINE void spell_inline(Emit emit) noexcept
	{
		size = static_cast<::std::size_t>(emit(local) - local);
	}

	/** @brief Transfers an allocation or copies precisely the initialized inline prefix. */
	print_record(print_record &&source) noexcept : allocation(source.allocation), size(source.size), capacity(source.capacity)
	{
		if (allocation == nullptr)
		{
			::fast_io::details::non_overlapped_copy_n(source.local, size, local);
		}
		source.allocation = nullptr;
		source.size = 0;
	}

	/** @brief Releases the allocation with the same allocator and extent used by the staging buffer. */
	~print_record()
	{
		if (allocation != nullptr)
		{
			using allocator = ::fast_io::typed_generic_allocator_adapter<::fast_io::native_global_allocator, Char>;
			if constexpr (allocator::has_deallocate)
			{
				allocator::deallocate(allocation);
			}
			else
			{
				allocator::deallocate_n(allocation, capacity);
			}
		}
	}

	/** @brief Adopts a newly formatted record into an empty owner after successful formatting. */
	void adopt(base &scratch) noexcept
	{
		size = static_cast<::std::size_t>(scratch.curr_ptr - scratch.begin_ptr);
		if (scratch.begin_ptr == scratch.buffer)
		{
			::fast_io::details::non_overlapped_copy_n(scratch.buffer, size, local);
		}
		else
		{
			allocation = scratch.begin_ptr;
			capacity = static_cast<::std::size_t>(scratch.end_ptr - scratch.begin_ptr);
			scratch.begin_ptr = scratch.buffer;
		}
	}

	/** @brief Rebases inline storage to this object's current address at consumption. */
	Char const *begin() const noexcept
	{
		return allocation == nullptr ? local : allocation;
	}
	/** @brief Returns the end of the exact formatted record in the same allocation as begin(). */
	Char const *end() const noexcept
	{
		return begin() + size;
	}
};

/**
 * @brief Formats directly into a resident operation's private put area.
 * @details Immediate joins consume this range in place. Moving a staged operation copies its initialized
 * inline prefix or transfers ownership of its allocation.
 */
template <::std::integral Char>
class resident_print_record
{
public:
	typename print_record<Char>::base staging;

	resident_print_record() noexcept = default;
	resident_print_record(resident_print_record const &) = delete;

	/** @brief Transfers the unique allocation or copies only the initialized inline range. */
	resident_print_record(resident_print_record &&source) noexcept
	{
		auto &from{source.staging};
		if (from.begin_ptr == from.buffer)
			staging.curr_ptr = ::fast_io::details::non_overlapped_copy(from.buffer, from.curr_ptr, staging.buffer);
		else
		{
			staging.begin_ptr = from.begin_ptr;
			staging.curr_ptr = from.curr_ptr;
			staging.end_ptr = from.end_ptr;
			from.begin_ptr = from.buffer;
			from.end_ptr = from.buffer + inline_record_capacity;
		}
		from.curr_ptr = from.buffer;
	}

	/** @brief Returns the initialized range for a short write or the consuming join. */
	Char const *begin() const noexcept { return staging.begin_ptr; }
	Char const *end() const noexcept { return staging.curr_ptr; }
};

/** @brief Proves that a complete normalized print record is represented by destination-independent formatting CPOs. */
template <bool Line, typename Output, typename... Args>
consteval bool can_stage(::fast_io::operations::defines::print_freestanding_named_normalized_run<Args...> *)
{
	using Char = typename Output::output_char_type;
	return !::fast_io::operations::decay::defines::has_indivisible_status_print<Line, Output, Args...> &&
		   ((::fast_io::reserve_printable<Char, ::std::remove_cvref_t<Args>> ||
			 ::fast_io::dynamic_reserve_printable<Char, ::std::remove_cvref_t<Args>> ||
			 ::fast_io::scatter_printable<Char, ::std::remove_cvref_t<Args>> ||
			 ::fast_io::reserve_scatters_printable<Char, ::std::remove_cvref_t<Args>> ||
			 ::fast_io::dynamic_reserve_scatters_printable<Char, ::std::remove_cvref_t<Args>>) &&
			...);
}

/** @brief Proves a staged record can be formatted independently and submitted to a completion-based output. */
template <bool Line, typename O, typename... Args>
concept staged_print = (awaitable_output<output_ref_type<O>> || immediate_output_range<output_ref_type<O>> || immediate_output_reserve<output_ref_type<O>> || ::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_operations<output_ref_type<O>>) &&
					   !::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<output_ref_type<O>> &&
					   can_stage<Line, output_ref_type<O>>(static_cast<::fast_io::operations::defines::print_freestanding_named_normalized_run_t<
															   typename output_ref_type<O>::output_char_type, Args...> *>(nullptr)) &&
					   ::fast_io::operations::defines::print_freestanding_okay_for_line<Line,
																						typename print_record<typename output_ref_type<O>::output_char_type>::base &, Args...>;

/** @brief Accepts a stream-specific print protocol or a record proved printable through the staging buffer. */
template <bool Line, typename O, typename... Args>
concept printable_operation = output_device<operand_type<O>> &&
							  (::fast_io::operations::defines::print_freestanding_okay_for_line<Line, operand_type<O>, Args...> ||
							   staged_print<Line, O, Args...>);

/** @brief Adapts a narrow literal when the selected output uses another code-unit domain. */
template <typename Char, typename T>
constexpr decltype(auto) text_operand(T &&value) noexcept
{
#if !defined(FAST_IO_DISABLE_CODECVT)
	using array_type = ::std::remove_reference_t<T>;
	if constexpr (!::std::same_as<Char, char> && ::std::is_bounded_array_v<array_type> &&
				  ::std::same_as<::std::remove_extent_t<array_type>, char const>)
	{
		return ::fast_io::mnp::code_cvt(value);
	}
	else
#endif
	{
		return static_cast<T &&>(value);
	}
}

/** @brief Recognizes an original device-specific record or its standard cross-code-unit literal form. */
template <bool Line, typename O, typename... Args>
concept marked_printable = printable_operation<Line, O, Args...> ||
						   (output_device<operand_type<O>> && printable_operation<Line, O,
																				  decltype(text_operand<typename output_ref_type<O>::output_char_type>(::std::declval<Args>()))...>);

/** @brief Uses a device's whole-range commit or composes its reservation and publication operations. */
template <typename Output, typename Char>
inline bool try_write_record(Output &out, Char const *first, Char const *last) noexcept
{
	if constexpr (immediate_output_range<Output>)
	{
		return output_stream_try_write_define(out, first, last);
	}
	else
	{
		if (first == last) { return true; }
		auto *dest{output_stream_try_reserve_define(out, static_cast<::std::size_t>(last - first))};
		if (dest == nullptr) { return false; }
		output_stream_commit_reserved_define(out, ::fast_io::details::non_overlapped_copy(first, last, dest));
		return true;
	}
}



/** @brief Hands an ordered span to the selected layers and finishes a temporary owner only after the last span. */
template <typename O, bool Final = true, typename Guard, typename C>
inline task<void> emit_decorated(Guard &guard, C const *first, C const *last) FAST_IO_ASYNC_THROWS
{
	layer_activity receipt;
	co_await receipt;
	using path = typename O::layer_path;
	auto &out{guard.ref()};
	co_await write_layer<path, 0, layer_action::write>(out, reinterpret_cast<::std::byte const *>(first),
		static_cast<::std::size_t>(last - first) * sizeof(C), receipt.activity);
	if constexpr (Final && Guard::checked_finish)
		co_await write_layer<path, 0, layer_action::finish>(out, nullptr, 0, receipt.activity);
}

/** @brief Emits an owned record and checked-finishes the destination only after complete output. */
template <bool AsyncDevice = true, typename O, ::std::integral Char>
inline task<void> emit_record(capture<O> output, print_record<Char> record) FAST_IO_ASYNC_THROWS
{
	auto &&stream{captured_operand(output)};
	::fast_io::operations::basic_output_operation_guard<decltype(stream)> guard{stream};
	if constexpr (uses_selected_io_layers<false, O, output_ref_type<O>>)
	{
		co_await emit_decorated<O>(guard, record.begin(), record.end());
	}
	else
	{
		auto &out{guard.ref()};
		// An empty raw record has no write request. Checked decorator finish is still honored below.
		if constexpr (native_output<output_ref_type<O>>)
			if (record.begin()==record.end()) co_await unused_point(output_point<0>);
		bool accepted{};
		if constexpr (immediate_output_range<output_ref_type<O>> || immediate_output_reserve<output_ref_type<O>>)
		{
			accepted = try_write_record(out, record.begin(), record.end());
		}
		if constexpr (AsyncDevice && awaitable_output<output_ref_type<O>>)
		{
			using points = decltype(describe_file<false, output_ref_type<O>>());
			if constexpr (points::count == 1 && describes_start<points>(0) && points::kind(0) == point_kind::transport)
			{
				// Ordinary fields were captured at issue. Enter this file's selected transport only on a miss.
				if (!accepted && record.begin() != record.end()) co_await start_point(output_point<0>);
			}
		}
		if constexpr (AsyncDevice && issuer_record_output<output_ref_type<O>, Char>)
		{
			if (!accepted && current_loop != nullptr && current_loop->standalone_root && current_loop->ready.empty())
			{
				// The backend must prove the current destination can complete as this isolated root.
				native_error failure{};
				accepted = async_write_record_on_issuer_define(out, record.begin(), record.end(), failure);
				if (failure.raise != nullptr) [[unlikely]]
				{
					co_await store_native_error{failure};
					co_return;
				}
			}
		}
		if (!accepted)
		{
			if constexpr (!AsyncDevice)
			{
				// The value-only binding commits the frozen record through the synchronous file protocol on this issuer.
				write_observed(out, record.begin(), record.end());
			}
			else if constexpr (awaitable_output<output_ref_type<O>>)
			{
				// Keep the range and its short-write cursor in the record's own suspended frame.
				auto const *data{reinterpret_cast<::std::byte const *>(record.begin())};
				auto remaining{static_cast<::std::size_t>(record.end() - record.begin()) * sizeof(Char)};
				while (remaining != 0)
				{
					auto const [count, error]{co_await async_write_some_define(out, data, remaining)};
					if (error.raise != nullptr || count == 0 || count > remaining) [[unlikely]]
					{
						// A device error has priority over validation of its accompanying byte count.
						co_await store_native_error{error.raise != nullptr ? error : native_error{raise_invalid_progress, 0u}};
						co_return;
					}
					data += count;
					remaining -= count;
				}
			}
			else
			{
				// The ordinary overflow path receives the whole record when the current put area is short.
				co_await invoke_blocking([&] { write_observed(out, record.begin(), record.end()); });
			}
		}
		if constexpr (decltype(guard)::checked_finish) co_await finish_output_guard(guard);
	}
}

/** @brief Finishes a staged record after a put-area miss using the selected backing protocol. */
template <typename NativePath = void, typename Ref, typename Record>
inline task<void> emit_buffered_record(capture<Ref> ref, Record record) FAST_IO_ASYNC_THROWS
{
	auto &out{ref.get()};
	if (!try_write_record(out, record.begin(), record.end()))
	{
		if constexpr (::std::same_as<NativePath, void>)
			co_await invoke_device<immediate_output<::std::remove_reference_t<Ref>>>([&] { write_observed(out, record.begin(), record.end()); });
		else
		{
			layer_activity receipt{};
			co_await receipt;
			co_await write_layer<NativePath, 0, layer_action::write>(out, reinterpret_cast<::std::byte const *>(record.begin()),
				static_cast<::std::size_t>(record.end() - record.begin()) * sizeof(*record.begin()), receipt.activity);
		}
	}
}

/** @brief Holds a normalized borrowed destination and the formatted record until its unique consumer is ready. */
template <typename Ref, bool InlineStaging = true, typename NativePath = void>
struct buffered_record
{
	using char_type = typename ::std::remove_reference_t<Ref>::output_char_type;
	using record_type = ::std::conditional_t<InlineStaging, resident_print_record<char_type>, print_record<char_type>>;
	capture<Ref> sink;
	record_type record;

	/** @brief Takes the stable normalized observer without touching its output cursor. */
	explicit buffered_record(Ref &&ref) : sink(static_cast<Ref &&>(ref)) {}

	/** @brief Commits all staged code units if they fit in the device's available put area. */
	bool operator()(::std::true_type) noexcept
	{
		return try_write_record(sink.get(), record.begin(), record.end());
	}

	/** @brief Transfers the observer and data into the coroutine that can wait for space. */
	task<void> operator()(::std::false_type) FAST_IO_ASYNC_THROWS
	{
		return emit_buffered_record<NativePath>(static_cast<capture<Ref> &&>(sink), static_cast<record_type &&>(record));
	}
};

template <typename O>
using normalized_output = decltype(::fast_io::operations::output_stream_ref(::std::declval<operand_type<O>>()));

template <typename I>
using normalized_input = decltype(::fast_io::operations::input_stream_ref(::std::declval<operand_type<I>>()));

/** @brief Recognizes a borrowed resident output whose original slow path accepts the complete record. */
template <typename O>
concept buffered_record_output = ::std::is_lvalue_reference_v<operand_type<O>> && (immediate_output_range<output_ref_type<O>> || immediate_output_reserve<output_ref_type<O>>) &&
	!awaitable_output<output_ref_type<O>> && ::std::is_nothrow_move_constructible_v<capture<normalized_output<O>>> && requires {
		{ ::fast_io::operations::output_stream_ref(::std::declval<operand_type<O>>()) } noexcept;
	};

/** @brief Stores one bounded, value-semantic formatting operand, including an array's complete code units. */
template <typename Value>
struct record_argument { Value value; };

/** @brief Takes the snapshot required by a named output job before its input operands may change. */
template <typename T>
inline constexpr auto capture_record_argument(T &&value) noexcept
{
	using V = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_array_v<V>)
	{
		record_argument<V> result;
		::fast_io::details::non_overlapped_copy_n(value, ::std::extent_v<V>, result.value);
		return result;
	}
	else
	{
		return record_argument<V>{value};
	}
}

/** @brief Bounds the text of a scalar, copied code-unit array or opted-in self-contained formatter. */
template <typename Char, typename Arg>
consteval ::std::size_t record_argument_bound() noexcept
{
	using V = ::std::remove_cvref_t<Arg>;
	constexpr auto outside{inline_record_capacity + 1u};
	if constexpr (::std::is_volatile_v<::std::remove_all_extents_t<::std::remove_reference_t<Arg>>>)
	{
		return outside;
	}
	else if constexpr (::std::is_bounded_array_v<V> && ::std::same_as<::std::remove_extent_t<V>, Char> &&
					   ::std::is_const_v<::std::remove_extent_t<::std::remove_reference_t<Arg>>>)
	{
		return ::std::extent_v<V>;
	}
	else if constexpr (requires { requires ::fast_io::manipulators::is_static_arg_v<V>;
								  requires ::std::integral<::std::remove_cvref_t<decltype(V::get())>>; })
	{
		// The structural node retains its syntax identity; its stored integer supplies a conservative bound.
		return record_argument_bound<Char, decltype(V::get())>();
	}
	else if constexpr (::fast_io::async::snapshot_printable<Char, V> && requires(Char *p, V const &v) {
						   typename ::std::integral_constant<::std::size_t, print_reserve_size(io_reserve_type<Char, V>)>;

#if defined(__HERBCEPTIONS__)
						   requires (!throws((print_reserve_define(io_reserve_type<Char, V>, p, v))));
#endif
						   { print_reserve_define(io_reserve_type<Char, V>, p, v) } noexcept -> ::std::same_as<Char *>;
					   })
	{
		return print_reserve_size(io_reserve_type<Char, V>);
	}
	else if constexpr (::std::integral<V> && requires(Char *first, V value) {
		{ print_reserve_size(io_reserve_type<Char, V>) } -> ::std::convertible_to<::std::size_t>;

#if defined(__HERBCEPTIONS__)
		requires (!throws((print_reserve_define(io_reserve_type<Char, V>, first, value))));
#endif
		{ print_reserve_define(io_reserve_type<Char, V>, first, value) } noexcept;
	})
	{
		// The ordinary formatter supplies the upper bound, including its sign, base and character policies.
		return print_reserve_size(io_reserve_type<Char, V>);
	}
	else
	{
		return outside;
	}
}

/** @brief Recognizes a captured operand with a nonthrowing value-dependent length or scatter extent. */
template <typename Char, typename Arg>
concept sized_record_snapshot = !::std::is_volatile_v<::std::remove_reference_t<Arg>> &&
								::fast_io::async::snapshot_printable<Char, ::std::remove_cvref_t<Arg>> &&
								(requires(Char *p, ::std::remove_cvref_t<Arg> const &v) {

#if defined(__HERBCEPTIONS__)
		requires (!throws((print_reserve_size(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, v))));
#endif
		{ print_reserve_size(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, v) } noexcept -> ::std::same_as<::std::size_t>;

#if defined(__HERBCEPTIONS__)
		requires (!throws((print_reserve_define(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, p, v))));
#endif
		{ print_reserve_define(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, p, v) } noexcept -> ::std::same_as<Char *>;
	} || requires(::std::remove_cvref_t<Arg> const &v) {

#if defined(__HERBCEPTIONS__)
		requires (!throws((print_scatter_define(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, v))));
#endif
		{ print_scatter_define(io_reserve_type<Char, ::std::remove_cvref_t<Arg>>, v) } noexcept -> ::std::same_as<::fast_io::basic_io_scatter_t<Char>>; });

/** @brief Limits total captured operand storage independently from the number of emitted code units. */
template <typename Char, typename... Args>
consteval bool record_capture_fits() noexcept
{
	if constexpr ((requires { sizeof(Args); } && ...))
	{
		::std::size_t remaining{inline_record_capacity * sizeof(Char)};
		return ((sizeof(Args) <= remaining && (remaining -= sizeof(Args), true)) && ...);
	}
	return false;
}

/** @brief Admits value-dependent records only when every operand has a stable snapshot and a usable bound. */
template <typename Char, typename... Args>
concept sized_record = (sized_record_snapshot<Char, Args> || ...) &&
					   ((record_argument_bound<Char, Args>() <= inline_record_capacity || sized_record_snapshot<Char, Args>) && ...) &&
					   record_capture_fits<Char, Args...>();

/** @brief Measures one owned snapshot without executing its formatter or publishing device effects. */
template <typename Char, typename T>
inline constexpr ::std::size_t record_size(T const &v) noexcept
{
	constexpr auto bound{record_argument_bound<Char, T const &>()};
	if constexpr (bound <= inline_record_capacity)
	{
		return bound;
	}
	else if constexpr (requires(Char *p) {

#if defined(__HERBCEPTIONS__)
						   requires (!throws((print_reserve_size(io_reserve_type<Char, T>, v))));
#endif
						   { print_reserve_size(io_reserve_type<Char, T>, v) } noexcept -> ::std::same_as<::std::size_t>;

#if defined(__HERBCEPTIONS__)
						   requires (!throws((print_reserve_define(io_reserve_type<Char, T>, p, v))));
#endif
						   { print_reserve_define(io_reserve_type<Char, T>, p, v) } noexcept -> ::std::same_as<Char *>;
					   })
	{
		return print_reserve_size(io_reserve_type<Char, T>, v);
	}
	else
	{
		return print_scatter_define(io_reserve_type<Char, T>, v).len;
	}
}

/** @brief Sums admissible field bounds without overflow and caps inline storage at the existing record capacity. */
template <bool Line, typename Char, typename... Args>
consteval ::std::size_t inline_record_bound() noexcept
{
	::std::size_t size{Line};
	auto add = [&](::std::size_t amount) { size = size > inline_record_capacity || amount > inline_record_capacity - size ? inline_record_capacity + 1u : size + amount; };
	(add(record_argument_bound<Char, Args>()), ...);
	if constexpr ((::fast_io::async::snapshot_printable<Char, ::std::remove_cvref_t<Args>> || ...) && !record_capture_fits<Char, Args...>())
	{
		return inline_record_capacity + 1u;
	}
	return size;
}

/** @brief Combines the destination reservation proof with the static or captured value-dependent record bounds. */
template <bool Line, typename O, typename... Args>
concept reservable_record = immediate_output_reserve<output_ref_type<O>> &&
							(inline_record_bound<Line, typename output_ref_type<O>::output_char_type, Args...>() <= inline_record_capacity ||
							 sized_record<typename output_ref_type<O>::output_char_type, Args...>);

/**
 * @brief Emits one owned built-in operand into an exclusive reservation made by its enclosing record.
 * @details The destination is disjoint from the operation's snapshots. The built-in array alias
 * formats the first extent minus one code units, including embedded zeroes. Its final array element
 * is outside that range regardless of its value. Integral fields fit their own reserve bounds.
 */
template <typename Char, typename T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr Char *emit_reserved_snapshot(Char *dest, T const &field) noexcept
{
	decltype(auto) normalized{::fast_io::io_print_forward<Char>(::fast_io::io_print_alias(field))};
	using N = ::std::remove_cvref_t<decltype(normalized)>;
	// A captured static scatter borrows the operation's own array. Its exact
	// width and the disjoint output reservation permit one fixed-size copy,
	// including embedded zeroes, without changing the ordinary alias's extent.
	if constexpr (::std::is_bounded_array_v<T> && requires { normalized.base; } &&
				  ::std::same_as<N, ::fast_io::manipulators::static_scatter_t<Char, ::std::extent_v<T> - 1u>>)
	{
		return ::fast_io::details::non_overlapped_copy_n(normalized.base, ::std::extent_v<T> - 1u, dest);
	}
	else if constexpr (::fast_io::scatter_printable<Char, N>)
	{
		auto scatter{print_scatter_define(io_reserve_type<Char, N>, normalized)};
		::fast_io::details::non_overlapped_copy_n(scatter.base, scatter.len, dest);
		return dest + scatter.len;
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
		// The compiler can fold every quotient of a known decimal field into literal stores.
		// The fixed public-source contract excludes character, boolean and customized formats.
		if constexpr (::std::integral<T> && ::fast_io::details::decay::print_fixed_public_source_available<Char, T>())
		{
			if (__builtin_constant_p(field))
			{
				auto *end{dest + print_reserve_precise_size(io_reserve_type<Char, N>, normalized)};
				auto *digit{end};
				using U = ::std::make_unsigned_t<T>;
				auto magnitude{static_cast<U>(field)};
				if constexpr (::std::signed_integral<T>)
					if (field < 0)
					{
						*dest = ::fast_io::char_literal_v<u8'-', Char>;
						magnitude = static_cast<U>(U{} - magnitude);
					}
				do
				{
					*--digit = static_cast<Char>(::fast_io::char_literal_v<u8'0', Char> + magnitude % 10u);
					magnitude = static_cast<U>(magnitude / 10u);
				} while (magnitude);
				return end;
			}
		}
#endif
		return print_reserve_define(io_reserve_type<Char, N>, dest, normalized);
	}
}

/** @brief Writes each snapshot in order and publishes only its initialized prefix, including a requested newline. */
template <bool Line, typename Ref, typename Char, typename Fields>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void commit_reserved_snapshots(Ref &out, Char *dest, Fields &fields) noexcept
{
	fields([&](auto const &...values) { ((dest = emit_reserved_snapshot(dest, values)), ...); });
	if constexpr (Line) *dest++ = ::fast_io::char_literal_v<u8'\n', Char>;
	output_stream_commit_reserved_define(out, dest);
}

/** @brief Lets a bounded record format directly into an exclusively reserved put area at the consuming await. */
template <bool Line, typename O, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_reserved_print(O &&device, Args &&...args)
{
	using Ref = normalized_output<O>;
	using Char = typename output_ref_type<O>::output_char_type;
	using NativePath = native_buffer_path_t<false, O, output_ref_type<O>>;
	constexpr auto capacity{inline_record_bound<Line, Char, Args...>()};
	constexpr bool fixed_snapshots{capacity <= inline_record_capacity &&
		(::fast_io::details::decay::print_fixed_public_source_available<Char, Args>() && ...)};
	auto fields = [... items = capture_record_argument(static_cast<Args &&>(args))](auto function) -> decltype(auto) {
		return function(items.value...);
	};
	if constexpr (immediate_output<::std::remove_reference_t<Ref>> && ::fast_io::direct_obuffer_copy_safe<Char, Ref> &&
				  ::fast_io::deferred_obuffer_commit_safe<Char, Ref> &&
				  ::fast_io::operations::defines::print_freestanding_okay_for_line<Line, operand_type<O>, Args...>)
	{
		// Each captured operand owns its value. The complete print can consume this device inline.
		return make_immediate_call([out = capture<Ref>(::fast_io::operations::output_stream_ref(operand(static_cast<O &&>(device)))),
									fields]() mutable FAST_IO_ASYNC_THROWS {
			auto &ref{out.get()};
			if constexpr (fixed_snapshots)
			{
				if (auto *dest = output_stream_try_reserve_define(ref, capacity))
				{
					commit_reserved_snapshots<Line>(ref, dest, fields);
					return;
				}
			}
			// A short remaining area can still fit the actual record through the ordinary exact-length planner.
			fields([&](auto const &...v) { ::fast_io::operations::decay::print_freestanding_compiler_constant_pre_normalization<Line>(ref, v...); });
		});
	}
	else
	{
		auto work = [out = capture<Ref>(::fast_io::operations::output_stream_ref(operand(static_cast<O &&>(device)))),
					 fields]<bool Probe>(::std::bool_constant<Probe>) mutable
#if defined(__HERBCEPTIONS__)
			throws(!Probe)
#else
			noexcept(Probe)
#endif
		{
			if constexpr (Probe)
			{
				auto &ref{out.get()};
				auto required{capacity};
				if constexpr (capacity > inline_record_capacity)
				{
					// Saturating the sum makes overflow and oversized records ordinary reservation misses.
					required = Line;
					fields([&](auto const &...values) {
						auto add = [&](::std::size_t n) {
							if (required <= inline_record_capacity && n <= inline_record_capacity - required)
							{
								required += n;
							}
							else
							{
								required = inline_record_capacity + 1u;
							}
						};
						(add(record_size<Char>(values)), ...);
					});
					if (required > inline_record_capacity)
					{
						return false;
					}
				}
				auto *dest{output_stream_try_reserve_define(ref, required)};
				if (dest == nullptr)
				{
					return false;
				}
				if constexpr (fixed_snapshots)
				{
					commit_reserved_snapshots<Line>(ref, dest, fields);
				}
				else
				{
					::fast_io::basic_obuffer_view<Char> view{dest, dest + required};
					fields([&](auto const &...v) { ::fast_io::operations::print_freestanding<Line>(view, v...); });
					output_stream_commit_reserved_define(ref, dest + view.size());
				}
				return true;
			}
			else if constexpr (!::std::same_as<NativePath, void>)
			{
				// The staged record must outlive an overlapped buffer flush.
				resident_print_record<Char> record;
				fields([&](auto const &...v) { ::fast_io::operations::print_freestanding<Line>(record.staging, v...); });
				return emit_buffered_record<NativePath>(static_cast<capture<Ref> &&>(out), static_cast<resident_print_record<Char> &&>(record));
			}
			else
			{
				return invoke_device<immediate_output<::std::remove_reference_t<Ref>>>([ref = static_cast<capture<Ref> &&>(out), fields]() mutable FAST_IO_ASYNC_THROWS {
					if constexpr (capacity <= inline_record_capacity)
					{
						Char local[capacity ? capacity : 1u];
						::fast_io::basic_obuffer_view<Char> view{local, local + capacity};
						fields([&](auto const &...v) { ::fast_io::operations::print_freestanding<Line>(view, v...); });
						// The formatter initialized [begin, curr). GCC can treat the one-past pointer as a separate read.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
						write_observed(ref.get(), view.begin_ptr, view.curr_ptr);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
					}
					else
					{
						// The record may exceed the inline budget. The staging buffer retains the complete slow path.
						typename print_record<Char>::base staging;
						fields([&](auto const &...v) { ::fast_io::operations::print_freestanding<Line>(staging, v...); });
						write_observed(ref.get(), staging.begin_ptr, staging.curr_ptr);
					}
				});
			}
		};
		return resident_operation<void, decltype(work)>{::std::in_place, static_cast<decltype(work) &&>(work)};
	}
}


/** @brief Stages at the call site and leaves device effects to await, get, or the full-expression boundary. */
template <bool Line, typename O, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_buffered_print(O &&device, Args &&...args)
{
	using work_type = buffered_record<normalized_output<O>, true, native_buffer_path_t<false, O, output_ref_type<O>>>;
	resident_operation<void, work_type> op{::std::in_place, ::fast_io::operations::output_stream_ref(operand(static_cast<O &&>(device)))};
	auto &record{op.staging().record};
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
	try
#endif
	{
		::fast_io::details::print_snapshot_fields<Line>(record.staging, static_cast<Args &&>(args)...);
	}
#if defined(__HERBCEPTIONS__)
	catch throws(::std::error failure) { op.creation_error(failure); }
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
	catch (...) { op.creation_exception(); }
#endif
	return op;
}


/** @brief Supplies a result after an indivisible custom print has already consumed all its arguments. */
inline task<void> printed_during_issue() { co_return; }

/** @brief Selects the existing print planner before a native submission or inside an opaque stream operation. */
template <bool Line, typename O, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline task<void> print_operation(O &&output, Args &&...args) FAST_IO_ASYNC_THROWS
{
	using ref = output_ref_type<O>;
	using Char = typename ref::output_char_type;
	if constexpr (staged_print<Line, O, Args...>)
	{
		// Format while compiler-known values and the caller's temporary payloads are still visible.
		print_record<Char> record;
		constexpr auto bound{inline_record_bound<Line, Char, Args...>()};
		if constexpr (kernel_byte_output<ref> && bound <= inline_record_capacity &&
					  (::fast_io::details::decay::print_fixed_public_source_available<Char, Args>() && ...))
		{
			record.template spell_inline<bound>([&](Char *cursor) noexcept {
				((cursor = emit_reserved_snapshot(cursor, args)), ...);
				if constexpr (Line) *cursor++ = ::fast_io::char_literal_v<u8'\n', Char>;
				return cursor;
			});
		}
		else
		{
			typename print_record<Char>::base staging;
			::fast_io::details::print_snapshot_fields<Line>(staging, static_cast<Args &&>(args)...);
			record.adopt(staging);
		}
		return emit_record(capture<O>(static_cast<O &&>(output)), static_cast<print_record<Char> &&>(record));
	}
	else
	{
		// Keep the whole destination-specific protocol. Proven value snapshots can cross suspension.
		if constexpr (((record_argument_bound<Char,Args>() <= inline_record_capacity) && ...))
			return invoke_device<immediate_output<ref>>([stream = capture<O>(static_cast<O &&>(output)),
					... values = capture_record_argument(static_cast<Args &&>(args))]() mutable FAST_IO_ASYNC_THROWS {
				::fast_io::details::print_snapshot_fields<Line>(captured_operand(stream),values.value...);
			});
		else
		{
			// A value with writable aliases cannot be retained by an opaque customization.
			// Execute the canonical group while its arguments still refer to the issue-time state.
			::fast_io::details::print_snapshot_fields<Line>(operand(static_cast<O &&>(output)),static_cast<Args &&>(args)...);
			return printed_during_issue();
		}
	}
}

/** @brief Scans a normalized asynchronous input and preserves the first uncommitted field's failure. */
template <typename Input, typename... Targets>
inline void scan_marked_ref(Input &in, Targets &...targets) FAST_IO_ASYNC_THROWS
{
	if constexpr (parse_code_input_scan<Input, Targets...>)
	{
		// The provider has already advanced its cursor and committed every preceding field.
		auto const code{input_stream_scan_code_define(in, targets...)};
		if (code != ::fast_io::parse_code::ok) [[unlikely]]
		{
			::fast_io::details::raise_scan_error(code);
		}
	}
	else
	{
		::fast_io::operations::scan_explicit_ref(in, targets...);
	}
}

/** @brief Runs the canonical scanner once against a resident prefix or on the device's worker. */
template <typename I, typename... Args>
inline task<void> scan_buffered(capture<I> source, capture<Args>... values) FAST_IO_ASYNC_THROWS
{
	auto &&owner{captured_operand(source)};
	decltype(auto) in{::fast_io::operations::input_stream_ref(owner)};
	auto op = [&]() FAST_IO_ASYNC_THROWS { scan_marked_ref(in, values.get()...); };
	auto ready = [&]() noexcept { return input_stream_scan_ready_define(in, ::fast_io::io_type<::std::remove_cvref_t<Args>>...); };
	conditional_work<decltype(op), decltype(ready)> work{op, ready};
	co_await blocking_awaiter<decltype(work), task<void>::promise_type>{work};
}

/** @brief Preserves an exact stream-specific overload before applying ordinary literal transcoding. */
template <bool Line, typename O, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto print_marked(O &&output, Args &&...args) FAST_IO_ASYNC_THROWS
{
	if constexpr (printable_operation<Line, O, Args...>)
	{
		return print_operation<Line>(static_cast<O &&>(output), static_cast<Args &&>(args)...);
	}
	else
	{
		using char_type = typename output_ref_type<O>::output_char_type;
		return print_operation<Line>(static_cast<O &&>(output), text_operand<char_type>(static_cast<Args &&>(args))...);
	}
}

/** @brief Builds the existing print strategy once for every return policy and line-ending choice. */
template <bool Line, typename O, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_print(O &&output, Args &&...args)
{
	if constexpr (!layered_device<O> && buffered_record_output<O> && staged_print<Line, O, Args...>)
	{
		if constexpr (reservable_record<Line,O,Args...>)
			return make_reserved_print<Line,O,Args...>(static_cast<O &&>(output),static_cast<Args &&>(args)...);
		else return make_buffered_print<Line,O,Args...>(static_cast<O &&>(output),static_cast<Args &&>(args)...);
	}
	else return make_io_operation(
		print_marked<Line,O,Args...>, static_cast<O &&>(output), static_cast<Args &&>(args)...);
}

/** @brief Scans a captured buffer through its selected native refill, preserving field order. */
template <typename Path, typename Source, typename... Targets>
inline task<void> scan_native_buffer(capture<Source> source, capture<Targets>... targets) FAST_IO_ASYNC_THROWS;

/** @brief Keeps a resident field's probe in the awaiting caller and transfers its loans on a miss. */
template <typename Ref, typename Target, typename NativePath = void>
struct resident_scan_field
{
	capture<Ref> source;
	capture<Target> argument;

	FAST_IO_GNU_ALWAYS_INLINE bool operator()(::std::true_type) noexcept
	{
		return input_stream_try_scan_define(source.get(), argument.get());
	}

	auto operator()(::std::false_type) FAST_IO_ASYNC_THROWS
	{
		if constexpr (::std::same_as<NativePath, void>)
			return invoke_device<::fast_io::async::immediate_input<::std::remove_reference_t<Ref>>>(
				[work = static_cast<resident_scan_field &&>(*this)]() mutable FAST_IO_ASYNC_THROWS {
					return scan_marked_ref(work.source.get(), work.argument.get());
				});
		else
			return scan_native_buffer<NativePath>(static_cast<capture<Ref> &&>(source), static_cast<capture<Target> &&>(argument));
	}
};

/** @brief Scans a marked input through its original normalized scanner and buffering protocols. */
template <::fast_io::async::details::marked_device I, typename... Args>
	requires ::fast_io::async::input_device<::fast_io::async::details::operand_type<I>>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_scan(I &&input, Args &&...args)
{
	using namespace ::fast_io::async::details;
	using Path = typename ::std::remove_cvref_t<I>::layer_path;
	constexpr bool native_refill{buffer_needs_native_completion<true, Path, input_ref_type<I>>};
	if constexpr (::fast_io::async::immediate_input<input_ref_type<I>> && ::std::is_lvalue_reference_v<operand_type<I>> &&
					   ((::std::is_lvalue_reference_v<Args> || ::std::is_trivially_copyable_v<Args>) && ...))
	{
		return make_immediate_call([source = capture<I>(static_cast<I &&>(input)),
									... values = capture<Args>(static_cast<Args &&>(args))]() mutable FAST_IO_ASYNC_THROWS {
			auto &&device{captured_operand(source)};
			decltype(auto) ref{::fast_io::operations::input_stream_ref(device)};
			if constexpr (::fast_io::async::immediate_input_scan<input_ref_type<I>, Args...> &&
				(sizeof...(Args) > 1 || !::fast_io::async::parse_code_input_scan<input_ref_type<I>, ::std::remove_reference_t<Args>...>))
			{
				// A declined transaction leaves the cursor and targets available to the canonical scanner.
				if (input_stream_try_scan_define(ref, values.get()...))
				{
					return;
				}
			}
			return scan_marked_ref(ref, values.get()...);
		});
	}
	else if constexpr (::std::is_lvalue_reference_v<operand_type<I>> && ::fast_io::async::immediate_input_scan<input_ref_type<I>, Args...> &&
				  ::std::is_nothrow_move_constructible_v<capture<normalized_input<I>>> &&
				  (::std::is_nothrow_move_constructible_v<capture<Args>> && ...) && requires {
		{ ::fast_io::operations::input_stream_ref(::std::declval<operand_type<I>>()) } noexcept;
	})
	{
		using Ref = normalized_input<I>;
		if constexpr (sizeof...(Args) == 1)
		{
			using Work = resident_scan_field<Ref, Args..., ::std::conditional_t<native_refill, Path, void>>;
			return resident_operation<void, Work>{::std::in_place,
				Work{capture<Ref>(::fast_io::operations::input_stream_ref(operand(static_cast<I &&>(input)))),
					capture<Args>(static_cast<Args &&>(args))...}};
		}
		else
		{
			auto work = [source = capture<Ref>(::fast_io::operations::input_stream_ref(operand(static_cast<I &&>(input)))),
						 ... values = capture<Args>(static_cast<Args &&>(args))]<bool Probe>(::std::bool_constant<Probe>) mutable
#if defined(__HERBCEPTIONS__)
				throws(!Probe)
#else
				noexcept(Probe)
#endif
			{
				if constexpr (Probe)
				{
					return input_stream_try_scan_define(source.get(), values.get()...);
				}
				else
				{
					// The fallback owns the references independently of the public awaitable's lifetime.
					if constexpr (native_refill)
						return scan_native_buffer<Path>(static_cast<capture<Ref> &&>(source), static_cast<capture<Args> &&>(values)...);
					else
						return invoke_device<::fast_io::async::immediate_input<input_ref_type<I>>>([input = static_cast<capture<Ref> &&>(source),
																						 ... args = static_cast<capture<Args> &&>(values)]() mutable FAST_IO_ASYNC_THROWS {
							return scan_marked_ref(input.get(), args.get()...);
						});
				}
			};
			return resident_operation<void, decltype(work)>{::std::in_place, static_cast<decltype(work) &&>(work)};
		}
	}
	else if constexpr (native_refill)
	{
		return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
			return scan_native_buffer<Path>(capture<I>(static_cast<I &&>(input)), capture<Args>(static_cast<Args &&>(args))...);
		});
	}
	else if constexpr (requires(input_ref_type<I> &ref) {
					  { input_stream_scan_ready_define(ref, ::fast_io::io_type<::std::remove_cvref_t<Args>>...) } noexcept -> ::std::same_as<bool>;
				  })
	{
		return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
			return scan_buffered(capture<I>(static_cast<I &&>(input)), capture<Args>(static_cast<Args &&>(args))...);
		});
	}

	else
	{
		return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
			return invoke_device<::fast_io::async::immediate_input<input_ref_type<I>>>([source = capture<I>(static_cast<I &&>(input)),
																						... values = capture<Args>(static_cast<Args &&>(args))]() mutable FAST_IO_ASYNC_THROWS {
				auto &&device{captured_operand(source)};
				decltype(auto) ref{::fast_io::operations::input_stream_ref(device)};
				return scan_marked_ref(ref, values.get()...);
			});
		});
	}
}

/**
 * @brief Transfers public elements from input to output until logical EOF.
 * @details Unmarked endpoints enter the synchronous transmit engine directly. Marking either endpoint selects a
 * coroutine. An opaque adapter retains its own cursor, transformations, locking and terminal-finish protocols.
 */
template <typename I, typename O>
	requires ::fast_io::async::details::transfer_pair<I, O>
inline constexpr auto make_transmit(I &&input, O &&output)
{
	using namespace ::fast_io::async::details;
	if constexpr (native_transfer_pair<I, O> || uses_selected_io_layers<true, I, input_ref_type<I>> ||
		uses_selected_io_layers<false, O, output_ref_type<O>> ||
		(describe_file<true,input_ref_type<I>>().count > 1 && describe_file<true,input_ref_type<I>>().kind(0) == point_kind::decorator) ||
		(describe_file<false,output_ref_type<O>>().count > 1 && describe_file<false,output_ref_type<O>>().kind(0) == point_kind::decorator))
	{
		return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
			return transfer_native(capture<I>(static_cast<I &&>(input)), capture<O>(static_cast<O &&>(output)));
		});
	}
	else if constexpr (::fast_io::async::immediate_input<input_ref_type<I>> && ::fast_io::async::immediate_output<output_ref_type<O>> &&
					   ::std::is_lvalue_reference_v<operand_type<I>> && ::std::is_lvalue_reference_v<operand_type<O>>)
	{
		return make_immediate_call([source = capture<I>(static_cast<I &&>(input)),
									destination = capture<O>(static_cast<O &&>(output))]() mutable FAST_IO_ASYNC_THROWS {
			return ::fast_io::operations::transmit_until_eof(captured_operand(destination), captured_operand(source));
		});
	}
	else
	{
		return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
			return invoke_device<::fast_io::async::immediate_input<input_ref_type<I>> && ::fast_io::async::immediate_output<output_ref_type<O>>>([source = capture<I>(static_cast<I &&>(input)),
																																				  destination = capture<O>(static_cast<O &&>(output))]() mutable FAST_IO_ASYNC_THROWS {
				return ::fast_io::operations::transmit_until_eof(captured_operand(destination), captured_operand(source));
			});
		});
	}
}


} // namespace fast_io::async::details

#include "value_operations.h"

namespace fast_io::async::details
{
template <typename O>
using out_levels = decltype(describe_file<false,output_ref_type<O>>());
template <typename I>
using in_levels = decltype(describe_file<true,input_ref_type<I>>());
/** @brief Adds only value providers whose owned protocol can publish the selected argument independently. */
template <typename Base,bool... Fields>
using with_value_points=::std::conditional_t<(Fields || ...),argument_points<Base,Fields...>,Base>;
template <typename Base,typename C,bool Scan,typename... Args>
using value_graph=typename compose_value_points<
	with_value_points<Base,decltype(describe_operand<Scan,C,Args>())::observe_field...>,
	decltype(describe_operand<Scan,C,Args>())...>::type;
template <typename O,typename... Args>
using print_graph=value_graph<operation_points<algorithm::print,no_file_points,out_levels<O>>,
	typename output_ref_type<O>::output_char_type,false,Args...>;
template <typename O,typename... Args>
using format_graph=value_graph<operation_points<algorithm::format,no_file_points,out_levels<O>>,
	typename output_ref_type<O>::output_char_type,false,Args...>;
template <typename I,typename... Args>
using scan_graph=value_graph<operation_points<algorithm::scan,in_levels<I>,no_file_points>,
	typename input_ref_type<I>::input_char_type,true,Args...>;

/** @brief Checks the observer requirement of every field that a synchronous semantic object can select.
 *  @details Expanding a pack, choosing a condition arm, or adding padding can expose a direct
 *  formatter. Each possible child uses the same named-source normalization as ordinary print.
 *  Examining both arm types at compile time leaves the run-time evaluation of the predicate
 *  and the inactive arm untouched.
 */
template <typename Ref, typename Field>
consteval bool field_needs_real_output()
{
	namespace d = ::fast_io::details::decay;
	using Char = typename Ref::output_char_type;
	if constexpr (::fast_io::print_requires_exact_ref<Char, Ref, Field>)
		return true;
	else if constexpr (d::print_semantic_node<Field>)
	{
		using Node = ::std::remove_reference_t<decltype(d::print_semantic_node_ref(::std::declval<Field &>()))>;
		auto child = []<typename T>() {
			using normalized = decltype(d::print_semantic_input_forward<Char>(::std::declval<T &>()));
			return field_needs_real_output<Ref, normalized>();
		};
		if constexpr (::fast_io::details::print_pack<Node>)
			return [&]<::std::size_t... I>(::std::index_sequence<I...>) {
				return (child.template operator()<decltype(::fast_io::containers::get<I>(::std::declval<Node &>().storage))>() || ...);
			}(::std::make_index_sequence<Node::size>{});
		else if constexpr (d::print_semantic_condition_v<::std::remove_cv_t<Node>>)
			return child.template operator()<decltype((::std::declval<Node &>().t1))>() ||
				child.template operator()<decltype((::std::declval<Node &>().t2))>();
		else
			return child.template operator()<decltype((::std::declval<Node &>().reference))>();
	}
	else return false;
}

/**
 * @brief Checks whether the ordinary record contract permits publishing privately formatted fields.
 * @details A lockable observer receives the complete spelling in one ordinary write. Following its
 * unlocked projection at compile time proves both that this write exists and that it preserves
 * the record's representation. An indivisible status hook owns the entire record. An unmarked
 * field which explicitly requires its observer cannot be redirected to a private formatting buffer.
 * A marked field instead supplies its own capture and spelling contract.
 */
template <bool Line, typename Ref, bool UnderLock = false, typename... Original, typename... Normalized>
consteval bool accepts_staged_fields(::fast_io::operations::defines::print_freestanding_named_normalized_run<Original...> *original,
	::fast_io::operations::defines::print_freestanding_named_normalized_run<Normalized...> *normalized)
{
	namespace caps = ::fast_io::operations::decay::defines;
	if constexpr (caps::has_indivisible_status_print<Line, Ref, Normalized...> ||
		((!annotated_operand<Original> && field_needs_real_output<Ref, Normalized>()) || ...))
		return false;
	else if constexpr (caps::has_output_or_io_stream_mutex_ref_define<Ref>)
	{
		if constexpr (caps::has_complete_output_stream_mutex_protocol<Ref>)
		{
			using unlocked = ::std::remove_reference_t<decltype(
				::fast_io::operations::decay::output_stream_unlocked_ref_decay(::std::declval<Ref &>()))>;
			return accepts_staged_fields<Line, unlocked, true>(original, normalized);
		}
		else return false;
	}
	else return (!UnderLock && awaitable_output<Ref>) || caps::writable<Ref> || caps::bytes_writable<Ref>;
}

/** @brief Admits ordinary fields through their synchronous formatter and marked fields through their owned producer. */
template <bool Line,typename O,typename... Args>
consteval bool async_print_pack()
{
	using R=output_ref_type<O>;
	using C=typename R::output_char_type;
	if constexpr (!(annotated_operand<Args> || ...))
		return marked_printable<Line,device<O>,Args...>;
	else if constexpr (!((annotated_operand<Args> ? async_printable<C,unannotated_t<Args>> :
		::fast_io::operations::defines::print_freestanding_okay_for_line<false,typename print_record<C>::base &,
			decltype(text_operand<C>(::std::declval<Args>()))>) && ...))
		return false;
	else
	{
		using normalized_run=::fast_io::operations::defines::print_freestanding_named_normalized_run_t<C,unannotated_t<Args>...>;
		using sources = ::fast_io::operations::defines::print_freestanding_named_normalized_run<Args...>;
		return accepts_staged_fields<Line, R>(static_cast<sources *>(nullptr), static_cast<normalized_run *>(nullptr));
	}
}

/** @brief Names the ordinary scanner proxy for the same named target used by synchronous scan. */
template <typename C,typename T>
using sync_scan_proxy = ::std::remove_reference_t<decltype(::fast_io::io_scan_forward<C>(
	::fast_io::io_scan_alias(::std::declval<T &>())))>;

/** @brief Checks the actual target protocol and rejects arithmetic temporaries with no destination to update. */
template <typename C,typename T>
consteval bool accepts_scan_field()
{
	if constexpr (annotated_operand<T>) return async_scannable<C,unannotated_t<T>>;
	else if constexpr (::std::is_arithmetic_v<::std::remove_cvref_t<T>>)
		return ::std::is_lvalue_reference_v<T> && !::std::is_const_v<::std::remove_reference_t<T>>;
	else if constexpr (async_scan_reusable<C,T>) return true;
	else if constexpr (requires { typename sync_scan_proxy<C,T>; })
	{
		using S=sync_scan_proxy<C,T>;
		return ::fast_io::precise_reserve_scannable<C,S> || ::fast_io::context_scannable<C,S> ||
			::fast_io::contiguous_scannable<C,S> || ::fast_io::contiguous_padding_scannable_protocol<C,S>;
	}
	else return false;
}

/** @brief A mixed record binds marked targets and invokes each ordinary scanner when its field is reached. */
template <typename I,typename... Args>
consteval bool async_scan_pack()
{
	using R=input_ref_type<I>;
	using C=typename R::input_char_type;
	constexpr bool incremental{((annotated_operand<Args> && async_scan_consumer<C,unannotated_t<Args>>) || ...)};
	constexpr bool group_status{::fast_io::operations::decay::defines::has_status_scan_define<R,sync_scan_proxy<C,unannotated_t<Args>> &...>};
	if constexpr (!incremental && group_status)
		return ((!annotated_operand<Args> || async_scan_reusable<C,unannotated_t<Args>>) && ...);
	else if constexpr (!(accepts_scan_field<C,Args>() && ...)) return false;
	else if constexpr (incremental)
		// A per-field parser cannot bypass a mutex or split an indivisible group-status customization.
		return !group_status && !::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<R> &&
			::fast_io::operations::decay::defines::has_ibuffer_basic_operations<R>;
	else return true;
}
template <typename I,typename O>
using transfer_graph = operation_points<algorithm::transmit,in_levels<I>,out_levels<O>>;

/** @brief Creates the file-selected work for one record under its ADL-selected binding. */
template <bool Line,typename P,typename O,typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto issue_print(O &&out, Args &&...args)
{
	using L = print_graph<O,Args...>;
	using B = bound_points<P,L>;
	auto factory = [&] {
		if constexpr (requires { async_print_work_define(::std::bool_constant<Line>{},io_type<B>,static_cast<O &&>(out),static_cast<Args &&>(args)...); })
			return make_io_operation([&]() FAST_IO_ASYNC_THROWS { return async_print_work_define(::std::bool_constant<Line>{},io_type<B>,static_cast<O &&>(out),static_cast<Args &&>(args)...); });
		else if constexpr ((annotated_operand<Args> || ...))
			return issue_print_values<Line,B,kernel_only_output<B>>(select_endpoint<B::async_output,typename B::output>(static_cast<O &&>(out)),static_cast<Args &&>(args)...);
		else return make_print<Line>(select_endpoint<true,typename B::output>(static_cast<O &&>(out)),static_cast<Args &&>(args)...);
	};
	return async_operation<observed_graph<L,B>,decltype(factory())>{io_type<B>,factory};
}
} // namespace fast_io::async::details

namespace fast_io::async
{
namespace details
{
/** @brief Resolves unmarked operands to the empty selection used by an operation-local request. */
template <typename T,bool Annotated=annotated_operand<T>>
struct selection_of
{
	using type=no_point_selection;
};

/** @brief Extracts the selected points from a marked operand. */
template <typename T>
struct selection_of<T,true>
{
	using type=typename ::std::remove_cvref_t<T>::selection_type;
};

template <typename T>
using selection_of_t=typename selection_of<T>::type;

/** @brief Binds only this object's named points, then recursively binds its stored child in the same direction. */
template <bool Input, typename T>
consteval auto endpoint_selection()
{
	using owner = ::std::remove_cvref_t<unannotated_t<T>>;
	using local = selection_of_t<T>;
	using child = typename decltype([] {
		if constexpr (requires { typename owner::decorated_stream_type; })
			return ::std::type_identity<typename owner::decorated_stream_type>{};
		else if constexpr (requires { typename owner::buffered_stream_type; })
		{
			using underlying = typename owner::buffered_stream_type;
			if constexpr (contains_annotation<underlying>() || requires { typename underlying::decorated_stream_type; })
				return ::std::type_identity<underlying>{};
			else return ::std::type_identity<void>{};
		}
		else return ::std::type_identity<void>{};
	}())::type;
	if constexpr (!::std::is_void_v<child>)
	{
		using nested = decltype(endpoint_selection<Input, child>());
		constexpr auto size = []<typename File> {
			if constexpr (Input) return in_levels<File>::count;
			else return out_levels<File>::count;
		};
		constexpr auto child_size{size.template operator()<unannotated_t<child>>()};
		using description = typename decltype([] {
			if constexpr (Input) return ::std::type_identity<in_levels<owner>>{};
			else return ::std::type_identity<out_levels<owner>>{};
		}())::type;
		static_assert(description::wrapped_child::count == child_size);
		return decorated_point_selection<local, description::local_count, nested, child_size>{};
	}
	else return local{};
}

template <typename O, typename... Args>
using output_request = operation_request<no_point_selection,decltype(endpoint_selection<false,O>()),selection_of_t<Args>...>;
template <typename I, typename... Args>
using input_request = operation_request<decltype(endpoint_selection<true,I>()),no_point_selection,selection_of_t<Args>...>;
template <typename I, typename O>
using transmit_request = operation_request<decltype(endpoint_selection<true,I>()),decltype(endpoint_selection<false,O>())>;
/** @brief Detects an opt-in annotation on an endpoint or any operation argument. */
template <typename Endpoint,typename... Values>
inline constexpr bool has_annotation=contains_annotation<Endpoint>() || (annotated_operand<Values> || ... || false);

/** @brief Checks the value provider and its local selectors for one character domain and use.
 * @details An explicit capture or binding may supply zero independently observable points. It
 * still accepts the empty selection. An explicit index must name a published start or release.
 */
template <bool Input, typename C, typename T, typename Selection>
consteval bool accepts_value_marker() noexcept
{
	if constexpr (Input ? !async_scannable<C,T> : !async_printable<C,T>) return false;
	else if constexpr (Selection::all) return true;
	else
	{
		using D = decltype(describe_value<Input,C,T>());
		return valid_point_selection<Selection, D::count + D::observe_field>() && valid_selection_domain<Selection,D>();
	}
}

/** @brief Tests an endpoint marker without constructing an operation graph or instantiating its coroutine. */
template <bool Input, typename T, typename Selection>
consteval bool accepts_file_marker() noexcept
{
	if constexpr (Input ? !input_device<T> : !output_device<T>) return false;
	else
	{
		using path = decltype(endpoint_selection<Input, basic_async<Selection,T>>());
		if constexpr (Input) return valid_file_selection<path,in_levels<T>>();
		else return valid_file_selection<path,out_levels<T>>();
	}
}

/** @brief Admits a marker if one possible role supports the selection. The consuming operation resolves the actual role. */
template <typename T, typename Selection, typename C>
concept value_marker_in = accepts_value_marker<false,C,T,Selection>() || accepts_value_marker<true,C,T,Selection>();

/** @brief Stops at the first supported role and character domain without instantiating an I/O coroutine. */
template <typename T, typename Selection>
concept selectable_operand = accepts_file_marker<false,T,Selection>() || accepts_file_marker<true,T,Selection>() ||
	value_marker_in<T,Selection,char> || value_marker_in<T,Selection,wchar_t> || value_marker_in<T,Selection,char8_t> ||
	value_marker_in<T,Selection,char16_t> || value_marker_in<T,Selection,char32_t> ||
	value_marker_in<T,Selection,unsigned char> || value_marker_in<T,Selection,signed char> ||
	value_marker_in<T,Selection,unsigned short> || value_marker_in<T,Selection,short> ||
	value_marker_in<T,Selection,unsigned int> || value_marker_in<T,Selection,int> ||
	value_marker_in<T,Selection,unsigned long> || value_marker_in<T,Selection,long> ||
	value_marker_in<T,Selection,unsigned long long> || value_marker_in<T,Selection,long long> || value_marker_in<T,Selection,bool>
#if defined(__SIZEOF_INT128__)
	|| (::std::integral<__int128_t> && value_marker_in<T,Selection,__int128_t>) ||
	(::std::integral<__uint128_t> && value_marker_in<T,Selection,__uint128_t>)
#endif
	;
}

/** @brief Marks a locally supported asynchronous operand; an empty selector pack selects all its work.
 * @details The marker borrows a named object or owns a temporary. An explicit numeric index must exist
 * in one possible role; an enum must belong to that provider. The I/O algorithm checks the chosen
 * direction, character type and value category again when it binds the complete expression.
 */
template <auto... Indices, typename T>
	requires (valid_point_selector<Indices> && ...) && (!annotated_operand<T>) && details::capturable<T> && point_selection<Indices...>::well_formed &&
		details::selectable_operand<T, point_selection<Indices...>>
[[nodiscard]] inline constexpr auto async(T &&value)
{
	return basic_async<point_selection<Indices...>,T>{static_cast<T &&>(value)};
}
} // namespace fast_io::async

namespace fast_io::async::details
{
/**
 * @brief Scans through an asynchronous endpoint and reports parse failures through the configured exception ABI.
 * @details There is no Boolean reporting mode for asynchronous scan. EOF, invalid input, and overflow propagate as
 * C++ exceptions in a conventional C++ build and as deterministic Herbceptions in an HC build.
 */
template <typename Request, typename I,typename... Args>
	requires binds_points<Request,scan_graph<I,Args...>> &&
		(async_scan_pack<I,Args...>())
FAST_IO_GNU_ALWAYS_INLINE inline auto issue_scan(I &&in,Args &&...args)
{
	namespace op = ::fast_io::async::details;
	using L=op::scan_graph<I,Args...>;
	using B=op::bound_points<Request,L>;
	auto factory = [&] {
		if constexpr (requires { async_scan_work_define(io_type<B>,static_cast<I &&>(in),static_cast<Args &&>(args)...); })
			return op::make_io_operation([&]() FAST_IO_ASYNC_THROWS { return async_scan_work_define(io_type<B>,static_cast<I &&>(in),static_cast<Args &&>(args)...); });
		else if constexpr ((requires { typename B::input::child_selection; } && B::async_input && (requires(input_ref_type<I> &r) { r.ptr->transcode_engine;
	} || requires(input_ref_type<I> &r) { r.iobptr->input_buffer.buffer_curr; })) || ((annotated_operand<Args> && async_scan_consumer<typename input_ref_type<I>::input_char_type,unannotated_t<Args>>) || ...))
			return op::issue_scan_values<B>(op::select_endpoint<B::async_input,typename B::input>(static_cast<I &&>(in)),static_cast<Args &&>(args)...);
		else if constexpr (!B::async_input)
			return op::make_io_operation([&]() FAST_IO_ASYNC_THROWS {
				return op::invoke_immediate([source=op::capture<I>(static_cast<I &&>(in)),... vals=op::capture<unannotated_t<Args>>(value_operand(static_cast<Args &&>(args)))]() mutable FAST_IO_ASYNC_THROWS {
					decltype(auto) ref=::fast_io::operations::input_stream_ref(source.get());
					return op::scan_marked_ref(ref,vals.get()...);
				});
			});
		else
			return op::make_scan(op::select_endpoint<true,typename B::input>(static_cast<I &&>(in)),value_operand(static_cast<Args &&>(args))...);
	};
	return ::fast_io::async::async_operation<observed_graph<L,B>,decltype(factory())>{io_type<B>,factory};
}

/**
 * @brief Transfers bytes between selected input and output endpoints until logical EOF.
 * @details The operation retains independent endpoint selections and obeys only dependencies declared by their
 * point graph. Errors propagate through the conventional C++ or Herbceptions ABI selected for the build.
 */
template <typename Request,typename I,typename O>
	requires async::details::transfer_pair<I,O> && binds_points<Request,async::details::transfer_graph<I,O>>
inline auto issue_transmit(I &&in,O &&out)
{
	namespace op = ::fast_io::async::details;
	using L=op::transfer_graph<I,O>;
	using B=op::bound_points<Request,L>;
	constexpr bool custom=requires { async_transmit_work_define(io_type<B>,static_cast<I &&>(in),static_cast<O &&>(out)); };
	auto factory = [&] {
		if constexpr (custom)
			return op::make_io_operation([&]() FAST_IO_ASYNC_THROWS { return async_transmit_work_define(io_type<B>,static_cast<I &&>(in),static_cast<O &&>(out)); });
		else
			return op::make_transmit(op::select_endpoint<B::async_input,typename B::input>(static_cast<I &&>(in)),
				op::select_endpoint<B::async_output,typename B::output>(static_cast<O &&>(out)));
	};
	return ::fast_io::async::async_operation<observed_graph<L,B>,decltype(factory())>{io_type<B>,factory};
}

} // namespace fast_io::async::details

namespace fast_io
{
inline namespace io
{
/** @brief Preserves the original synchronous transmit call when neither endpoint is annotated. */
template <typename I,typename O>
	requires (!async::details::contains_annotation<I>()) && (!async::details::contains_annotation<O>()) && async::details::transfer_pair<I,O>
inline constexpr auto transmit(I &&in,O &&out)
#if defined(__HERBCEPTIONS__)
	throws(::fast_io::details::uses_checked_transmit_eof<false, true, O, I>)
#endif
{
	return ::fast_io::operations::transmit_until_eof(static_cast<O &&>(out),static_cast<I &&>(in));
}

/** @brief Formats a record using the selections attached to its file and original values. */
template <typename O, typename... Args>
	requires async::details::has_annotation<O,Args...> && async::output_device<async::details::unannotated_t<O>> &&
		async::details::binds_points<async::details::output_request<O,Args...>,async::details::print_graph<async::details::unannotated_t<O>,Args...>> &&
		(async::details::async_print_pack<false,async::details::unannotated_t<O>,Args...>())
inline auto print(O &&out,Args &&...args)
{
	return async::details::issue_print<false,async::details::output_request<O,Args...>>(
		async::details::value_operand(static_cast<O &&>(out)),static_cast<Args &&>(args)...);
}

/** @brief Appends the newline to the same owned record before submitting it to the output. */
template <typename O, typename... Args>
	requires async::details::has_annotation<O,Args...> && async::output_device<async::details::unannotated_t<O>> &&
		async::details::binds_points<async::details::output_request<O,Args...>,async::details::print_graph<async::details::unannotated_t<O>,Args...>> &&
		(async::details::async_print_pack<true,async::details::unannotated_t<O>,Args...>())
inline auto println(O &&out,Args &&...args)
{
	return async::details::issue_print<true,async::details::output_request<O,Args...>>(
		async::details::value_operand(static_cast<O &&>(out)),static_cast<Args &&>(args)...);
}

/** @brief Binds scan targets at issue. Joining the result completes with void or raises the first failure. */
template <typename I,typename... Args>
	requires async::details::has_annotation<I,Args...> && async::input_device<async::details::unannotated_t<I>> &&
		async::details::binds_points<async::details::input_request<I,Args...>,async::details::scan_graph<async::details::unannotated_t<I>,Args...>> &&
		(async::details::async_scan_pack<async::details::unannotated_t<I>,Args...>())
inline auto scan(I &&in,Args &&...args)
{
	return async::details::issue_scan<async::details::input_request<I,Args...>>(
		async::details::value_operand(static_cast<I &&>(in)),static_cast<Args &&>(args)...);
}

/** @brief Drains the input until EOF with independently selected input and output drivers. */
template <typename I,typename O>
	requires (async::details::contains_annotation<I>() || async::details::contains_annotation<O>()) &&
		async::details::transfer_pair<async::details::unannotated_t<I>,async::details::unannotated_t<O>> &&
		async::details::binds_points<async::details::transmit_request<I,O>,async::details::transfer_graph<async::details::unannotated_t<I>,async::details::unannotated_t<O>>>
inline auto transmit(I &&in,O &&out)
{
	return async::details::issue_transmit<async::details::transmit_request<I,O>>(
		async::details::value_operand(static_cast<I &&>(in)),async::details::value_operand(static_cast<O &&>(out)));
}
} // namespace io
} // namespace fast_io
