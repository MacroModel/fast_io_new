#pragma once

/**
 * @file
 * @brief Implements buffered output over bounded stateful transform engines.
 *
 * The public put area aggregates printable fragments before invoking the
 * engine. Engine output is then committed through the destination bridge,
 * either directly into a compatible underlying obuffer or through typed/byte
 * scratch. Flush is nonterminal; finish is checked, terminal, and idempotent.
 */

#if defined(__HERBCEPTIONS__)
#if defined(__cpp_exceptions)
#define FAST_IO_TRANSCODE_OUTPUT_EFFECT throws
#else
#define FAST_IO_TRANSCODE_OUTPUT_EFFECT throws(Checked)
#endif
#else
#define FAST_IO_TRANSCODE_OUTPUT_EFFECT
#endif

namespace fast_io
{

/** @brief Tracks the terminal and failure state of an output adapter. */
enum class otranscoder_state : ::std::uint_least8_t
{
	// The engine accepts process/sync-flush/finish operations.
	open,
	// Terminal finish completed and all generated output was committed.
	finished,
	// Destruction or explicit cancellation abandoned the message.
	cancelled,
	// An engine or underlying-stream operation threw.
	failed
};

/**
 * @brief Adapts a bounded stateful transform engine to a buffered output stream.
 *
 * Public units are buffered independently from transformed units. Exact-unit
 * endpoints can use underlying put areas directly; byte/object bindings use
 * representation-safe scratch and preserve incomplete byte-written objects.
 */
template <::std::integral public_char, typename handle_storage,
		  typename engine,
		  typename traits = ::fast_io::details::transcode_output_traits_for<engine, ::fast_io::basic_otranscoder_traits<
			  public_char,
			  ::fast_io::transcode_to_value_t<
				  ::fast_io::operations::transcode_engine_ref_t<engine>>>, handle_storage>>
	requires(::fast_io::bounded_transcoder<engine> &&
			 ::fast_io::transcode_automatically_bindable<
				 ::fast_io::transcode_from_value_t<
					 ::fast_io::operations::transcode_engine_ref_t<engine>>,
				 public_char>)
class basic_otranscoder
{
public:
	using output_char_type = public_char;
	using handle_storage_type = handle_storage;
	/** @brief Original child operand retained for operation-level decorator traversal. */
	using decorated_stream_type = ::std::remove_reference_t<decltype(::std::declval<handle_storage &>().get())>;
	using engine_type = engine;
	using traits_type = traits;
	using allocator_type = typename traits_type::allocator_type;
	using engine_ref_type = ::fast_io::operations::transcode_engine_ref_t<engine_type>;
	using source_unit_type =
		::fast_io::transcode_from_value_t<engine_ref_type>;
	using transformed_unit_type =
		::fast_io::transcode_to_value_t<engine_ref_type>;
	using normalized_output_ref_type = ::fast_io::details::transcode_endpoint_ref_t<false, ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(
		::std::declval<handle_storage_type &>().get()))>>;

#if defined(__HERBCEPTIONS__)
	static inline constexpr bool checked_output{
		::fast_io::operations::decay::defines::has_transcode_process_checked_define<engine_ref_type> ||
		::fast_io::operations::decay::defines::has_transcode_finish_checked_define<engine_ref_type> ||
		::fast_io::details::sync_transcode_output_checked<normalized_output_ref_type>};
#endif

	static inline constexpr transcode_unit_binding source_binding{
		::fast_io::transcode_unit_binding_for<source_unit_type,
											  output_char_type>()};
	static inline constexpr transcode_unit_binding destination_binding{
		::fast_io::transcode_unit_binding_for<
			transformed_unit_type,
			typename normalized_output_ref_type::output_char_type>()};

	using wire_char_type = typename normalized_output_ref_type::output_char_type;
	// A bare sink needs a large put area to gather short fields. A contiguous range
	// occupying a substantial part of it can go straight from the caller to the engine.
	static inline constexpr ::std::size_t direct_write_size{[] {
		constexpr auto capacity{traits_type::public_buffer_size};
		if constexpr (details::transcode_preferred_sizes<engine_ref_type>.input_bytes != 0u &&
					  !operations::decay::defines::has_obuffer_basic_operations<normalized_output_ref_type> &&
					  details::output_transcode_bytes<normalized_output_ref_type> == 0u)
		{
			constexpr auto batch{details::transcode_buffer_units<output_char_type>(
				details::transcode_preferred_sizes<engine_ref_type>.input_bytes, capacity)};
			constexpr auto quarter{capacity / 4u};
			return batch < quarter ? quarter : batch;
		}
		else
		{
			return capacity;
		}
	}()};
	static inline constexpr ::std::size_t wire_buffer_size{
		::fast_io::details::transcode_buffer_units<transformed_unit_type>(
			::fast_io::details::output_transcode_bytes<normalized_output_ref_type>, traits_type::transform_buffer_size)};
	static constexpr bool assemble_wire_units{destination_binding == transcode_unit_binding::object_bytes &&
		!::fast_io::operations::decay::defines::bytes_writable<normalized_output_ref_type>};
	static_assert([]() consteval {
		if constexpr (destination_binding == transcode_unit_binding::exact_units)
		{
			// Exact transformed units require ordinary typed output support.
			return ::fast_io::operations::decay::defines::
				writable<normalized_output_ref_type>;
		}
		else
		{
			// A typed-only sink can receive complete, representation-safe wire units.
			return ::fast_io::operations::decay::defines::
				bytes_writable<normalized_output_ref_type> ||
				(::fast_io::transcode_wire_character<wire_char_type> && ::fast_io::operations::decay::defines::writable<normalized_output_ref_type>);
		}
	}());
	static_assert(traits_type::public_buffer_size != 0u);
	static_assert(traits_type::transform_buffer_size != 0u);

	// public_buffer is a put area. transform_buffer is used only when the
	// underlying destination cannot safely be handed to the engine directly.
	::fast_io::details::basic_transcode_buffer_pointers<output_char_type> public_buffer{};
	::fast_io::details::basic_transcode_buffer_pointers<transformed_unit_type> transform_buffer{};
	// Only exact-unit engines need this remainder when callers write bytes that
	// stop inside one PublicChar representation.
	::fast_io::details::basic_transcode_source_remainder<output_char_type> source_remainder{};
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#else
	[[no_unique_address]]
#endif
	::std::conditional_t<assemble_wire_units, ::fast_io::details::basic_transcode_source_remainder<wire_char_type>,
		::fast_io::details::empty> wire_remainder{};

	handle_storage_type handle;
	engine_type transcode_engine;
	otranscoder_state state{otranscoder_state::open};

	/** @brief Constructs an open adapter with no allocated buffers. */
	inline explicit basic_otranscoder(handle_storage_type handle_value,
									  engine_type engine_value)
		: handle(::std::move(handle_value)),
		  transcode_engine(::std::move(engine_value))
	{}

	/** @brief Disables copying of engine, cursor, and allocation ownership. */
	basic_otranscoder(basic_otranscoder const &) = delete;
	/** @brief Disables copy assignment of stateful message ownership. */
	basic_otranscoder &operator=(basic_otranscoder const &) = delete;
	/** @brief Disables move assignment so allocations retain one clear owner. */
	basic_otranscoder &operator=(basic_otranscoder &&) = delete;

	/** @brief Transfers all engine, buffer, remainder, and lifecycle state. */
	inline basic_otranscoder(basic_otranscoder &&other) noexcept(
		::std::is_nothrow_move_constructible_v<handle_storage_type> &&
		::std::is_nothrow_move_constructible_v<engine_type>)
		: public_buffer(other.public_buffer),
		  transform_buffer(other.transform_buffer),
		  source_remainder(other.source_remainder), wire_remainder(other.wire_remainder),
		  handle(::std::move(other.handle)),
		  transcode_engine(::std::move(other.transcode_engine)), state(other.state)
	{
		// Buffer ownership transfers without moving their contents. Marking the
		// source cancelled prevents either destructor from finishing or freeing
		// the same message state twice.
		other.public_buffer = {};
		other.transform_buffer = {};
		other.source_remainder.size = 0u;
		if constexpr (assemble_wire_units)
		{
			other.wire_remainder.size = 0;
			if constexpr (traits_type::secure_clear) { ::fast_io::secure_clear(other.wire_remainder.storage, sizeof(wire_char_type)); }
		}
		if constexpr (traits_type::secure_clear)
		{
			// Erase the moved-from inline remainder before abandoning its state.
			::fast_io::secure_clear(other.source_remainder.storage,
									sizeof(other.source_remainder.storage));
		}
		other.state = otranscoder_state::cancelled;
	}

	/** @brief Cancels unfinished work and releases all owned allocations. */
	inline ~basic_otranscoder()
	{
		// Destruction is deliberately nonterminal: finish may throw and must be
		// requested explicitly or by a checked temporary-operation guard.
		cancel();
		destroy_buffers();
	}

	/** @brief Rejects operations after finish, cancellation, or failure. */
	inline void require_open() const
	{
		if (state != otranscoder_state::open) [[unlikely]]
		{
			// Closed directions cannot safely resume a stateful engine.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::invalid_state);
		}
	}

	/** @brief Requires an open adapter aligned to a complete public object. */
	inline void require_typed_write_boundary() const
	{
		// Typed writes cannot resume in the middle of an object assembled by a
		// preceding byte write. Further byte writes may still complete it.
		require_open();
		if (source_remainder.size != 0u) [[unlikely]]
		{
			// Typed writes cannot follow a byte write stopped inside one object.
			::fast_io::throw_transcode_stream_error(
				::fast_io::transcode_stream_errc::incomplete_unit);
		}
	}

	/** @brief Lazily allocates the public put area. */
	inline void ensure_public_buffer()
	{
		if (public_buffer.buffer_begin != nullptr)
		{
			// Preserve the existing allocation and its current buffered prefix.
			return;
		}
		using typed_allocator = ::fast_io::typed_generic_allocator_adapter<
			allocator_type, output_char_type>;
		output_char_type *buffer{
			typed_allocator::allocate(traits_type::public_buffer_size)};
		public_buffer = {buffer, buffer,
						 buffer + traits_type::public_buffer_size};
	}

	/** @brief Processes an exact bounded engine-source range to completion. */
	template <bool Checked = false, typename output_ref>
	inline void process_source(engine_ref_type &engine_ref, output_ref &outref,
							   source_unit_type const *first,
							   source_unit_type const *last) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// Retain one normalized engine ref and one normalized stream ref across
		// the complete retry loop. need_input must consume the supplied range;
		// need_output must make progress with the guaranteed minimum capacity.
		if (first == last)
		{
			// Avoid invoking providers with an empty process range.
			return;
		}
		for (;;)
		{
			// Retry bounded process steps until this source range is fully consumed.
			::std::size_t minimum{
				::fast_io::operations::decay::transcode_min_output_size_decay(
					::fast_io::transcode_reserve<engine_ref_type>,
					::fast_io::transcode_phase::process)};
			auto destination{prepare_destination<Checked>(outref, minimum)};
			auto process = [&]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
#if defined(__HERBCEPTIONS__)
				if constexpr (Checked && ::fast_io::operations::decay::defines::has_transcode_process_checked_define<engine_ref_type>)
					return transcode_process_checked_define(engine_ref, first, last, destination.first, destination.last);
				else
#endif
					return ::fast_io::operations::decay::transcode_process_decay_dispatch(
						engine_ref, first, last, destination.first, destination.last);
			};
			auto result{process()};
			if (result.status != ::fast_io::transcode_step_status::need_input &&
				result.status != ::fast_io::transcode_step_status::need_output)
			{
				// Only the two protocol-defined process statuses are admissible.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			auto const validated_from{
				::fast_io::details::validate_transcode_closed_range_offsets(
					first, last, result.from_next)};
			if (!validated_from.valid) [[unlikely]]
			{
				// The engine result is untrusted: prove a closed-range integer offset
				// before rebuilding or comparing its source cursor.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			auto const validated_to{
				commit_destination<Checked>(
					outref, destination, result.to_next, wire_remainder)};
			bool const made_progress{validated_from.current_offset != 0u ||
								 validated_to != destination.first};
			first = validated_from.current;
			if (result.status == ::fast_io::transcode_step_status::need_input)
			{
				// Need-input is legal only after the entire bounded source is consumed.
				if (first != last)
				{
					// Early need-input would silently lose an unconsumed source suffix.
					::fast_io::throw_transcode_stream_error(
						::fast_io::transcode_stream_errc::protocol_violation);
				}
				return;
			}
			if (!made_progress)
			{
				// Need-output must consume or produce with guaranteed capacity.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
		}
	}

	/** @brief Maps public units to the engine's typed or object-byte source. */
	template <bool Checked = false, typename output_ref>
	inline void process_public_units_impl(engine_ref_type &engine_ref,
									 output_ref &outref,
									 output_char_type const *first,
									 output_char_type const *last) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// Object-byte binding exposes the representation of PublicChar through
		// std::byte. Equal-sized but semantically different integer types are
		// intentionally never reinterpreted as exact units.
		if constexpr (source_binding == transcode_unit_binding::exact_units)
		{
			// Matching source units can be passed to the engine directly.
			process_source<Checked>(engine_ref, outref, first, last);
		}
		else
		{
			// Byte engines observe complete public-object representations.
			auto const byte_first{
				reinterpret_cast<::std::byte const *>(first)};
			auto const byte_last{
				reinterpret_cast<::std::byte const *>(last)};
			process_source<Checked>(engine_ref, outref, byte_first, byte_last);
		}
	}

	/** @brief Runs sync-flush or finish until its bounded drain phase completes. */
	template <::fast_io::transcode_phase phase, bool Checked = false, typename output_ref>
	inline void drain(engine_ref_type &engine_ref, output_ref &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		if constexpr (::fast_io::outputless_transcode_drain<engine_ref_type, phase>)
		{
#if defined(__HERBCEPTIONS__)
			if constexpr (Checked && phase == ::fast_io::transcode_phase::finish &&
				::fast_io::operations::decay::defines::has_transcode_finish_checked_define<engine_ref_type>)
			{
				transformed_unit_type unit;
				auto result{transcode_finish_checked_define(engine_ref, &unit, &unit)};
				if (result.to_next != &unit || result.status != ::fast_io::transcode_drain_status::complete) [[unlikely]]
					::fast_io::throw_transcode_stream_error(::fast_io::transcode_stream_errc::protocol_violation);
			}
			else
#endif
				::fast_io::operations::decay::transcode_drain_empty<phase>(engine_ref);
			return;
		}
		// Both sync-flush and finish use the same bounded drain loop, but their
		// engine semantics remain distinct through the compile-time phase.
		for (;;)
		{
			// Supply fresh bounded destinations until the selected drain completes.
			::std::size_t minimum{
				::fast_io::operations::decay::transcode_min_output_size_decay(
					::fast_io::transcode_reserve<engine_ref_type>, phase)};
			auto destination{prepare_destination<Checked>(outref, minimum)};
			::fast_io::basic_transcode_drain_result<transformed_unit_type> result;
			if constexpr (phase == ::fast_io::transcode_phase::sync_flush)
			{
				// Nonterminal drain preserves the engine for subsequent processing.
				result = ::fast_io::operations::decay::transcode_sync_flush_decay_dispatch(
					engine_ref, destination.first, destination.last);
			}
#if defined(__HERBCEPTIONS__)
			else if constexpr (Checked && ::fast_io::operations::decay::defines::has_transcode_finish_checked_define<engine_ref_type>)
			{
				result = transcode_finish_checked_define(engine_ref, destination.first, destination.last);
			}
#endif
			else
			{
				// Terminal drain validates and closes the logical output message.
				result = ::fast_io::operations::decay::transcode_finish_decay_dispatch(
					engine_ref, destination.first, destination.last);
			}
			if (result.status != ::fast_io::transcode_drain_status::complete &&
				result.status != ::fast_io::transcode_drain_status::need_output)
			{
				// Reject drain statuses outside the bounded protocol vocabulary.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
			auto const validated_to{
				commit_destination<Checked>(
					outref, destination, result.to_next, wire_remainder)};
			bool const made_progress{validated_to != destination.first};
			if (result.status == ::fast_io::transcode_drain_status::complete)
			{
				// The phase is complete after its produced prefix has been committed.
				return;
			}
			if (!made_progress)
			{
				// Need-output without producing anything would make the loop livelock.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::protocol_violation);
			}
		}
	}

	/** @brief Transforms and clears the currently buffered public prefix. */
	template <bool Checked = false, typename output_ref>
	inline void process_public_prefix_impl(engine_ref_type &engine_ref,
									  output_ref &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		if (public_buffer.buffer_begin != public_buffer.buffer_curr)
		{
			// Process only the committed prefix; unused put-area capacity is ignored.
			process_public_units_impl<Checked>(engine_ref, outref, public_buffer.buffer_begin,
								 public_buffer.buffer_curr);
			public_buffer.buffer_curr = public_buffer.buffer_begin;
		}
	}

#if defined(__HERBCEPTIONS__)
	struct checked_output_transaction
	{
		otranscoder_state &state;
		bool committed{};
		~checked_output_transaction() { if (!committed) state = otranscoder_state::failed; }
	};
#endif

	/** @brief Flushes the underlying stream only when its protocol supports it. */
	template <bool Checked, typename output_ref>
	inline static void flush_underlying(output_ref &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
#if defined(__HERBCEPTIONS__)
		if constexpr (Checked && details::sync_transcode_output_checked<output_ref>)
		{
			details::transcode_output_owner(outref).template flush_impl<true>();
		}
		else
#endif
		if constexpr (::fast_io::output_stream_publishable<output_ref>)
		{
			// Some observers make committed bytes visible without requesting a larger writable range.
			output_stream_publish_define(outref);
		}
		else if constexpr (::fast_io::operations::decay::defines::
						  output_stream_buffer_flush_dispatchable<output_ref>)
		{
			// Propagate visibility through the normalized underlying output observer. Policy dispatch preserves a
			// stateful adapter's identity while retaining value ABI for explicitly substitutable descriptors.
			::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(
				outref);
		}
	}

	/** @brief Marks the adapter failed before propagating an operation exception. */
	template <bool Checked = false, typename function>
	inline void guarded_impl(function &&operation) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// An engine or device failure can leave partial state in either ABI.
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			operation();
		}
		catch throws (::std::error failure)
		{
			state = otranscoder_state::failed;
			throw throws failure;
		}
		catch (...)
		{
			state = otranscoder_state::failed;
			throw;
		}
#elif defined(__HERBCEPTIONS__)
		if constexpr (Checked)
		{
			checked_output_transaction transaction{state};
			operation();
			transaction.committed = true;
		}
		else
		{
			operation();
		}
#elif defined(__cpp_exceptions)
		try
		{
			operation();
		}
		catch (...)
		{
			state = otranscoder_state::failed;
			throw;
		}
#else
		operation();
#endif
	}

	template <bool Checked, typename Ref, typename Operation>
	inline static void with_output(Ref &ref, Operation &&operation) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
#if defined(__HERBCEPTIONS__)
		if constexpr (
#if defined(__cpp_exceptions)
			true
#else
			Checked
#endif
		) ::fast_io::details::with_transcode_output_checked(ref, operation);
		else
#endif
			::fast_io::details::with_transcode_endpoint<false>(ref, operation);
	}

	template <bool Checked, typename Ref>
	inline auto prepare_destination(Ref &ref, ::std::size_t minimum) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		namespace cv = ::fast_io::details;
#if defined(__HERBCEPTIONS__)
		if constexpr (Checked && cv::sync_transcode_output_checked<Ref> &&
			::fast_io::transcode_output_window<transformed_unit_type, Ref>)
		{
			auto range{cv::acquire_transcode_obuffer_range(ref)};
			minimum = minimum ? minimum : 1u;
			constexpr auto guarantee{obuffer_minimum_size_define(
				::fast_io::io_reserve_type<typename Ref::output_char_type, Ref>)};
			if (range.end_offset - range.current_offset < minimum && minimum <= guarantee)
				cv::transcode_output_owner(ref).template prepare_put_area_impl<true>();
			return cv::prepare_transcode_destination<allocator_type, traits_type::secure_clear, false>(
				ref, transform_buffer, minimum, wire_buffer_size);
		}
		else
#endif
			return cv::prepare_transcode_destination<allocator_type, traits_type::secure_clear>(
				ref, transform_buffer, minimum, wire_buffer_size);
	}

	template <bool Checked, typename Ref>
	inline transformed_unit_type *commit_destination(
		Ref &ref, details::basic_transcode_destination<transformed_unit_type> destination,
		transformed_unit_type *next, decltype(wire_remainder) &remainder) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
#if defined(__HERBCEPTIONS__)
		if constexpr (Checked && details::sync_transcode_output_checked<Ref>)
		{
			if (!destination.direct)
			{
				auto result{details::validate_transcode_closed_range_offsets(destination.first, destination.last, next)};
				if (!result.valid) [[unlikely]]
					::fast_io::throw_transcode_stream_error(::fast_io::transcode_stream_errc::protocol_violation);
				if (result.current_offset)
				{
					auto &child{details::transcode_output_owner(ref)};
					using char_type = typename Ref::output_char_type;
					if constexpr (::std::same_as<transformed_unit_type, char_type>)
						child.write_checked(destination.first, result.current);
					else
					{
						static_assert(::std::same_as<transformed_unit_type, ::std::byte>);
						if constexpr (sizeof(char_type) == 1)
						{
							child.write_checked(reinterpret_cast<char_type const *>(destination.first),
								reinterpret_cast<char_type const *>(result.current));
						}
						else
							child.template write_all_bytes_overflow_impl<true>(destination.first, result.current);
					}
				}
				return result.current;
			}
		}
#endif
		return details::commit_transcode_destination<traits_type::secure_clear>(ref, destination, next, remainder);
	}

#if defined(__HERBCEPTIONS__)
	/** @brief Preserves the direct put-area write before a checked overflow. */
	inline void write_checked(output_char_type const *first, output_char_type const *last) throws
	{
		require_typed_write_boundary();
		if (public_buffer.buffer_curr != nullptr && last - first <= public_buffer.buffer_end - public_buffer.buffer_curr)
			public_buffer.buffer_curr = details::non_overlapped_copy(first, last, public_buffer.buffer_curr);
		else
			write_all_overflow_impl<true>(first, last);
	}
#endif

	/** @brief Publishes a fresh put area after processing any committed prefix. */
	template <bool Checked = false>
	inline void prepare_put_area_impl(::std::size_t reserve = 1u) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// The output CPO layer calls this cold hook before exposing a fresh put
		// area. Any committed public prefix is transformed first.
		require_typed_write_boundary();
		guarded_impl<Checked>([this, reserve]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
			if (public_buffer.buffer_begin != public_buffer.buffer_curr)
			{
				// Normalize refs only when a committed prefix actually needs processing.
				decltype(auto) engine_ref{
					::fast_io::transcode_ref(transcode_engine)};
				decltype(auto) child_ref{
					::fast_io::operations::output_stream_ref(handle.get())};
				with_output<Checked>(child_ref,[&](auto &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT {
					process_public_prefix_impl<Checked>(engine_ref, outref);
				});
			}
			::fast_io::details::ensure_transcode_scratch<allocator_type, traits_type::secure_clear>(
				public_buffer, reserve, traits_type::public_buffer_size);
		});
	}

	/** @brief Writes a public-unit range, bypassing buffering for large inputs. */
	template <bool Checked = false>
	inline void write_all_overflow_impl(output_char_type const *first,
								   output_char_type const *last) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// Large ranges bypass the public put area after its existing prefix has
		// been processed; small ranges stay buffered to coalesce print fragments.
		require_typed_write_boundary();
		guarded_impl<Checked>([this, first, last]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
			::std::size_t const incoming_size{
				static_cast<::std::size_t>(last - first)};
			bool const has_public_prefix{
				public_buffer.buffer_begin != public_buffer.buffer_curr};
			if (has_public_prefix || incoming_size >= direct_write_size)
			{
				// Normalize once when flushing a prefix or processing a large range.
				decltype(auto) engine_ref{
					::fast_io::transcode_ref(transcode_engine)};
				decltype(auto) child_ref{
					::fast_io::operations::output_stream_ref(handle.get())};
				with_output<Checked>(child_ref,[&](auto &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT {
					process_public_prefix_impl<Checked>(engine_ref, outref);
					if (incoming_size >= direct_write_size)
					{
						// Large ranges go directly through the engine after prefix ordering.
						process_public_units_impl<Checked>(engine_ref, outref, first, last);
					}
				});
				if (incoming_size >= direct_write_size) return;
			}
			ensure_public_buffer();
			auto current{first};
			for (; current != last; ++current)
			{
				*public_buffer.buffer_curr++ = *current;
			}
		});
	}

	/** @brief Writes object bytes in stream order and retains an unfinished public unit.
	 *  @details Byte-native engines can consume immediately. An exact-unit engine with a
	 *  one-byte public character may append a fitting checked write inside the locked put
	 *  area. Filling its last slot leaves transformation to the next overflow or finish.
	 */
	template <bool Checked = false>
	inline void write_all_bytes_overflow_impl(::std::byte const *first,
										 ::std::byte const *last) FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// A byte-native engine consumes immediately. An exact typed engine first
		// assembles complete PublicChar objects and retains a partial suffix.
		require_open();
		guarded_impl<Checked>([this, first, last]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) child_ref{
				::fast_io::operations::output_stream_ref(handle.get())};
			with_output<Checked>(child_ref,[&](auto &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT {
				if constexpr (source_binding == transcode_unit_binding::object_bytes)
				{
					// A native byte source can consume the caller's byte range immediately.
					process_public_prefix_impl<Checked>(engine_ref, outref);
					process_source<Checked>(engine_ref, outref, first, last);
				}
				else
				{
					// An exact typed source assembles bytes into complete public objects.
					ensure_public_buffer();
					if constexpr (Checked && sizeof(output_char_type) == 1u &&
								  (::std::same_as<output_char_type, char> || ::std::same_as<output_char_type, unsigned char>))
					{
						// A fitting byte range consists of complete objects and has no partial unit to retain.
						auto const bytes{static_cast<::std::size_t>(last - first)};
						if (!source_remainder.size &&
							bytes <= static_cast<::std::size_t>(public_buffer.buffer_end - public_buffer.buffer_curr))
						{
							if (bytes != 0u)
							{
								::fast_io::freestanding::my_memcpy(public_buffer.buffer_curr, first, bytes);
								public_buffer.buffer_curr += bytes;
							}
							return;
						}
					}
					auto current{first};
					if (source_remainder.size != 0u)
					{
						// Complete the prior partial object before accepting new full objects.
						while (current != last &&
							   source_remainder.size != sizeof(output_char_type))
						{
							// Append bytes until the retained public object becomes complete.
							source_remainder.storage[source_remainder.size] = *current;
							++source_remainder.size;
							++current;
						}
						if (source_remainder.size == sizeof(output_char_type))
						{
							// Publish the newly completed object into the public put area.
							if (public_buffer.buffer_curr == public_buffer.buffer_end)
							{
								// Free public put-area capacity by transforming its full prefix.
								process_public_prefix_impl<Checked>(engine_ref, outref);
							}
							::fast_io::freestanding::my_memcpy(
								public_buffer.buffer_curr,
								source_remainder.storage, sizeof(output_char_type));
							++public_buffer.buffer_curr;
							if constexpr (traits_type::secure_clear)
							{
								// Erase the inline assembly buffer after publishing the object.
								::fast_io::secure_clear(source_remainder.storage,
														sizeof(source_remainder.storage));
							}
							source_remainder.size = 0u;
						}
					}
					while (static_cast<::std::size_t>(last - current) >=
						   sizeof(output_char_type))
					{
						// Copy as many complete public objects as available space permits.
						if (public_buffer.buffer_curr == public_buffer.buffer_end)
						{
							// Transform a full put area before copying another object batch.
							process_public_prefix_impl<Checked>(engine_ref, outref);
						}
						::std::size_t const available_units{
							static_cast<::std::size_t>(public_buffer.buffer_end -
													   public_buffer.buffer_curr)};
						::std::size_t const incoming_units{
							static_cast<::std::size_t>(last - current) /
							sizeof(output_char_type)};
						::std::size_t const units{available_units < incoming_units
													  ? available_units
													  : incoming_units};
						::std::size_t const bytes{units * sizeof(output_char_type)};
						::fast_io::freestanding::my_memcpy(
							public_buffer.buffer_curr, current, bytes);
						public_buffer.buffer_curr += units;
						current += bytes;
					}
					while (current != last)
					{
						// Retain the final incomplete object representation for later bytes.
						source_remainder.storage[source_remainder.size] = *current;
						++source_remainder.size;
						++current;
					}
				}
			});
		});
	}

	/** @brief Makes current output visible without terminally closing the engine. */
	template <bool Checked = false>
	inline void flush_impl() FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// Sync flush makes current output visible while preserving an open engine
		// for later print/write calls. It must never emit terminal padding/tag.
		require_open();
		guarded_impl<Checked>([this]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) child_ref{
				::fast_io::operations::output_stream_ref(handle.get())};
			with_output<Checked>(child_ref,[&](auto &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT {
				process_public_prefix_impl<Checked>(engine_ref, outref);
				drain<::fast_io::transcode_phase::sync_flush, Checked>(engine_ref, outref);
				flush_underlying<Checked>(outref);
			});
		});
	}

	/** @brief Terminally finishes once, validating complete source-unit alignment. */
	template <bool Checked = false>
	inline void finish_impl() FAST_IO_TRANSCODE_OUTPUT_EFFECT
	{
		// Successful terminal finish is idempotent. A partial typed source object
		// is rejected before the engine can observe an ambiguous unit boundary.
		if (state == otranscoder_state::finished)
		{
			// A previously successful finish is deliberately idempotent.
			return;
		}
		require_open();
		guarded_impl<Checked>([this]() FAST_IO_TRANSCODE_OUTPUT_EFFECT {
			if (source_remainder.size != 0u)
			{
				// Terminal finish cannot invent missing bytes of a public object.
				::fast_io::throw_transcode_stream_error(
					::fast_io::transcode_stream_errc::incomplete_unit);
			}
			decltype(auto) engine_ref{
				::fast_io::transcode_ref(transcode_engine)};
			decltype(auto) child_ref{
				::fast_io::operations::output_stream_ref(handle.get())};
			with_output<Checked>(child_ref,[&](auto &outref) FAST_IO_TRANSCODE_OUTPUT_EFFECT {
				process_public_prefix_impl<Checked>(engine_ref, outref);
				drain<::fast_io::transcode_phase::finish, Checked>(engine_ref, outref);
				if constexpr (assemble_wire_units)
				{
					if (wire_remainder.size != 0u) { ::fast_io::throw_transcode_stream_error(::fast_io::transcode_stream_errc::incomplete_unit); }
				}
				if constexpr (requires { handle.finish_owned_output(outref); })
				{
					// Our trailer is input to the next owned encoder.
#if defined(__HERBCEPTIONS__)
					if constexpr (Checked && details::sync_transcode_output_checked<::std::remove_reference_t<decltype(outref)>>)
						details::transcode_output_owner(outref).template finish_impl<true>();
					else
#endif
						handle.finish_owned_output(outref);
				}
				else
				{
					// A borrowed encoder may serve another outer message and retains its own finish boundary.
					flush_underlying<Checked>(outref);
				}
				state = otranscoder_state::finished;
			});
		});
	}

	inline void prepare_put_area(::std::size_t reserve = 1u)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			prepare_put_area_impl<false>(reserve);
		}
		catch throws (::std::error failure)
		{
			failure.throw_legacy_exception();
		}
#else
		prepare_put_area_impl<false>(reserve);
#endif
	}

	inline void write_all_overflow(output_char_type const *first, output_char_type const *last)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			write_all_overflow_impl<false>(first, last);
		}
		catch throws (::std::error failure)
		{
			failure.throw_legacy_exception();
		}
#else
		write_all_overflow_impl<false>(first, last);
#endif
	}

	inline void write_all_bytes_overflow(::std::byte const *first, ::std::byte const *last)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			write_all_bytes_overflow_impl<false>(first, last);
		}
		catch throws (::std::error failure)
		{
			failure.throw_legacy_exception();
		}
#else
		write_all_bytes_overflow_impl<false>(first, last);
#endif
	}

#if defined(__HERBCEPTIONS__)
	inline void flush() throws requires(checked_output)
	{
		flush_impl<true>();
	}
#endif
	inline void flush()
#if defined(__HERBCEPTIONS__)
		requires (!checked_output)
#endif
	{
		flush_legacy();
	}

	inline void flush_legacy()
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			flush_impl<false>();
		}
		catch throws (::std::error failure)
		{
			failure.throw_legacy_exception();
		}
#else
		flush_impl<false>();
#endif
	}

#if defined(__HERBCEPTIONS__)
	inline void finish() throws requires(checked_output)
	{
		finish_impl<true>();
	}
#endif
	inline void finish()
#if defined(__HERBCEPTIONS__)
		requires (!checked_output)
#endif
	{
		finish_legacy();
	}

	inline void finish_legacy()
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try
		{
			finish_impl<false>();
		}
		catch throws (::std::error failure)
		{
			failure.throw_legacy_exception();
		}
#else
		finish_impl<false>();
#endif
	}

	template <typename output_ref>
	inline void process_public_prefix(engine_ref_type &engine_ref, output_ref &outref)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try { process_public_prefix_impl<false>(engine_ref, outref); }
		catch throws (::std::error failure) { failure.throw_legacy_exception(); }
#else
		process_public_prefix_impl<false>(engine_ref, outref);
#endif
	}

	template <typename output_ref>
	inline void process_public_units(engine_ref_type &engine_ref, output_ref &outref, output_char_type const *first, output_char_type const *last)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try { process_public_units_impl<false>(engine_ref, outref, first, last); }
		catch throws (::std::error failure) { failure.throw_legacy_exception(); }
#else
		process_public_units_impl<false>(engine_ref, outref, first, last);
#endif
	}

	template <typename function>
	inline void guarded(function &&operation)
	{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
		try { guarded_impl<false>(::std::forward<function>(operation)); }
		catch throws (::std::error failure) { failure.throw_legacy_exception(); }
#else
		guarded_impl<false>(::std::forward<function>(operation));
#endif
	}

	/** @brief Abandons an open message without invoking engine or stream CPOs. */
	inline void cancel() noexcept
	{
		// Cancellation discards buffered logical input without invoking process,
		// sync-flush, finish, or any potentially throwing underlying operation.
		if (state == otranscoder_state::open)
		{
			// Only an active message transitions to the explicit cancelled state.
			state = otranscoder_state::cancelled;
		}
		public_buffer.buffer_curr = public_buffer.buffer_begin;
		if constexpr (traits_type::secure_clear)
		{
			// Erase inline partial-object bytes even though they were never processed.
			::fast_io::secure_clear(source_remainder.storage,
									sizeof(source_remainder.storage));
		}
		if constexpr (assemble_wire_units)
		{
			wire_remainder.size = 0;
			if constexpr (traits_type::secure_clear) { ::fast_io::secure_clear(wire_remainder.storage, sizeof(wire_char_type)); }
		}
		source_remainder.size = 0u;
	}

private:
	/** @brief Securely clears and deallocates one optional scratch allocation. */
	template <typename unit_type>
	inline static void destroy_buffer(
		::fast_io::details::basic_transcode_buffer_pointers<unit_type> &buffer) noexcept
	{
		unit_type *begin{buffer.buffer_begin};
		if (begin == nullptr)
		{
			// A lazily unused buffer has no allocation to release.
			return;
		}
		::std::size_t const count{
			static_cast<::std::size_t>(buffer.buffer_end - begin)};
		if constexpr (traits_type::secure_clear)
		{
			// Apply the traits policy before returning storage to the allocator.
			::fast_io::secure_clear(begin, count * sizeof(unit_type));
		}
		::fast_io::typed_generic_allocator_adapter<
			allocator_type, unit_type>::deallocate_n(begin, count);
		buffer = {};
	}

	/** @brief Releases both public and transformed adapter allocations. */
	inline void destroy_buffers() noexcept
	{
		destroy_buffer(public_buffer);
		destroy_buffer(transform_buffer);
	}
};

// Stream adapters explicitly opt in because a temporary represents one
// complete output message. Borrowed refs do not specialize this trait.
template <::std::integral public_char, typename handle_storage,
		  typename engine, typename traits>
inline constexpr bool temporary_output_finish_enabled<
	::fast_io::basic_otranscoder<public_char, handle_storage, engine, traits>> =
	true;

} // namespace fast_io

#undef FAST_IO_TRANSCODE_OUTPUT_EFFECT
