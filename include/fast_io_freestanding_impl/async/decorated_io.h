#pragma once

namespace fast_io
{
/** @brief Allows early delivery only when the encoder and its concrete destination both accept prefixes.
 * @details Querying a stored child preserves its direction and ownership. A stateful encoder keeps the
 * same instance and its pending units for every write. A nested converter repeats the same check down
 * to the destination, which may decline splitting for this particular file or message.
 */
template <typename Owner>
	requires prefix_transcoder<typename Owner::engine_type> && async::prefix_output<typename Owner::normalized_output_ref_type> &&
		requires(Owner &owner) { { ::fast_io::operations::output_stream_ref(owner.handle.get()) } noexcept; }
inline constexpr bool async_output_accepts_prefixes_define(basic_otranscoder_ref<Owner> out) noexcept
{
	decltype(auto) child{::fast_io::operations::output_stream_ref(out.ptr->handle.get())};
	return async_output_accepts_prefixes_define(child);
}
}

namespace fast_io::async::details
{
/** @brief Projects a built-in buffered file's local selection onto its owned backing handle. */
template <typename Path>
struct backing_selection
{
	static constexpr bool selects(::std::size_t i) noexcept { return Path::selects(i + 1); }
};

/** @brief Obtains the child's binding for an annotated wrapper or an intrinsic file buffer. */
template <typename Path>
consteval auto buffer_child_selection() noexcept
{
	if constexpr (requires { typename Path::child_selection; }) return typename Path::child_selection{};
	else return backing_selection<Path>{};
}

/** @brief Uses the selected backing observer's native completion contract on a buffer miss. */
template <bool Input, typename Path, typename Ref>
inline constexpr bool buffer_needs_native_completion = [] {
	if constexpr (requires { typename Ref::io_buffer_type::handle_type; } &&
				  (Input ? requires(Ref &input) { input.iobptr->input_buffer.buffer_curr; }
				   : requires(Ref &output) { output.iobptr->output_buffer.buffer_curr; }))
	{
		using Backing = typename Ref::io_buffer_type::handle_type;
		using ChildPath = decltype(buffer_child_selection<Path>());
		if constexpr (!ChildPath::selects(0)) return false;
		else if constexpr (Input)
			return native_buffer_completion_required<::std::remove_cvref_t<decltype(
				::fast_io::operations::input_stream_ref(::std::declval<Backing &>()))>, true>;
		else
			return native_buffer_completion_required<::std::remove_cvref_t<decltype(
				::fast_io::operations::output_stream_ref(::std::declval<Backing &>()))>, false>;
	}
	else return false;
}();

/** @brief Identifies an operation whose device contains independently selected decorator and transport work. */
template <typename Device>
concept layered_device = requires {
	typename ::std::remove_cvref_t<Device>::layer_path::child_selection;
};

/** @brief Returns the device path only when a backing buffer miss must wait natively. */
template <bool Input, typename Device, typename Ref>
consteval auto native_buffer_path() noexcept
{
	if constexpr (requires { typename ::std::remove_cvref_t<Device>::layer_path; })
	{
		using Path = typename ::std::remove_cvref_t<Device>::layer_path;
		return ::std::type_identity<::std::conditional_t<buffer_needs_native_completion<Input, Path, Ref>, Path, void>>{};
	}
	else return ::std::type_identity<void>{};
}

template <bool Input, typename Device, typename Ref>
using native_buffer_path_t = typename decltype(native_buffer_path<Input, Device, Ref>())::type;

/** @brief Keeps explicitly layered I/O and selected native buffer misses on the completion path. */
template <bool Input, typename Device, typename Ref>
inline constexpr bool uses_selected_io_layers = layered_device<Device> ||
	!::std::same_as<native_buffer_path_t<Input, Device, Ref>, void>;

/** @brief Selects the lifetime action following the last accepted public unit. */
enum class layer_action : unsigned char { write, publish, finish };

/** @brief Records actual layer visits including an exception during a nested I/O request. */
struct layer_activity
{
	file_activity activity{};
	observation_history *history{};

	/** @brief Attaches this operation-local receipt to the coroutine that owns it. */
	bool await_ready() const noexcept { return false; }
	template <typename Promise>
	bool await_suspend(::std::coroutine_handle<Promise> handle) noexcept
	{
		history = ::fast_io::freestanding::addressof(handle.promise().history);
		return false;
	}
	void await_resume() const noexcept {}

	/** @brief Merges only the path reached before success, failure or cancellation. */
	~layer_activity() { if (history) history->received(activity); }
};

/** @brief Records participation without claiming that a streaming stage has already finished. */
inline void visit_layer(file_activity &activity, bool output, ::std::size_t depth) noexcept
{
	auto const index{static_cast<unsigned>(output)};
	activity.tracked[index] = true;
	if (activity.reached[index] < depth) activity.reached[index] = depth;
}

/** @brief Keeps a partially advanced codec unusable after an exception or frame destruction. */
template <typename State>
struct layer_transaction
{
	State &state;
	bool committed{};
	~layer_transaction() { if (!committed) state = State::failed; }
};

/** @brief Yields CPU work only where an explicitly selected stage has a runnable peer. */
inline bool has_layer_peer() noexcept
{
	return current_loop && (current_loop->root_count > 1 || !current_loop->ready.empty());
}

template <typename Path, ::std::size_t Depth, layer_action Action, typename R>
inline task<void> write_layer(R &out, ::std::byte const *data, ::std::size_t size, file_activity &activity) FAST_IO_ASYNC_THROWS;

/** @brief Requires complete public bytes that remain in one non-sensitive, successfully decoded range. */
template <typename R>
concept verified_transfer_window = requires {
	requires R::owner_type::direct_public_units;
	requires (sizeof(typename R::owner_type::input_char_type) == 1);
	requires ::std::same_as<typename R::owner_type::transformed_unit_type, ::std::byte>;
	requires (R::owner_type::traits_type::publication_mode == transcode_input_publication_mode::streaming_unverified);
	requires (!R::owner_type::traits_type::secure_clear);
	requires ::fast_io::coalescible_transcode_result<typename R::owner_type::engine_type>;
};

/** @brief Preserves each provider-defined refill callback on an opaque handle. */
template <typename Parent, typename Ref>
inline bool transfer_input_tie_active(Parent const &, Ref &ref) noexcept
{
	return requires { ref.ptr->handle.before_input_refill(); };
}

/** @brief Lets the duplex owner inspect its own pending output before skipping empty input ties. */
template <typename... Parts, typename Owner>
inline bool transfer_input_tie_active(::fast_io::basic_iotranscoder<Parts...> const &parent,
	::fast_io::basic_itranscoder_ref<Owner> &ref) noexcept
{
	if constexpr (::std::same_as<Owner, typename ::fast_io::basic_iotranscoder<Parts...>::input_adapter_type>)
		return parent.input_tie_has_effect(*ref.ptr);
	else return requires { ref.ptr->handle.before_input_refill(); };
}

/** @brief Describes the first unread public prefix, subsequent window width and total write length in bytes. */
struct verified_transfer_block
{
	::std::size_t first{}, window{}, size{};
};

/** @brief Bounds a contiguous range to whole logical public windows. */
template <typename Owner>
inline verified_transfer_block prepare_verified_transfer_block(Owner &owner, ::std::size_t room) noexcept
{
	auto &pub{owner.public_buffer};
	auto &raw{owner.transform_buffer};
	if (raw.buffer_curr == raw.buffer_end || owner.byte_boundary_offset || owner.target_remainder.size)
		return {};
	auto first{static_cast<::std::size_t>(pub.buffer_end - pub.buffer_curr)};
	auto window{static_cast<::std::size_t>(pub.buffer_capacity_end - pub.buffer_begin)};
	if (!first || first >= room || !window || window > room) return {};
	auto bytes{static_cast<::std::size_t>(raw.buffer_end - raw.buffer_curr)};
	auto available{room - first};
	if (bytes > available) bytes = available / window * window;
	if (!bytes) return {};
	return {first, window, first + bytes};
}

/** @brief Reconstructs the last attempted public window and consumes exactly the baseline's input. */
template <typename Owner>
inline ::std::size_t commit_verified_transfer_block(Owner &owner, verified_transfer_block block, byte_completion result) noexcept
{
	auto &pub{owner.public_buffer};
	pub.buffer_curr = pub.buffer_end;
	if (result.error.raise && result.transferred < block.first) return 0;
	auto completed{block.size};
	auto attempted{block.size};
	if (result.error.raise)
	{
		// A failed ordinary write has already consumed its source window. Successful earlier
		// windows count as transmitted, including the first one if it was partly read before entry.
		completed = block.first + (result.transferred - block.first) / block.window * block.window;
		auto rest{block.size - completed};
		attempted = completed + (rest < block.window ? rest : block.window);
	}
	// Rebuild the precise last get area so rewinding it after a short write exposes the same bytes.
	auto consumed{attempted - block.first};
	auto last_offset{(consumed - 1) / block.window * block.window};
	auto &raw{owner.transform_buffer};
	auto last_count{consumed - last_offset};
	::fast_io::freestanding::my_memcpy(pub.buffer_begin, raw.buffer_curr + last_offset, last_count);
	pub.buffer_end = pub.buffer_curr = pub.buffer_begin + last_count;
	raw.buffer_curr += consumed;
	return completed;
}

template <typename Path, ::std::size_t Depth, bool Transfer = false, typename R>
inline task<::std::size_t> read_layer(R &in, ::std::byte *data, ::std::size_t size, file_activity &activity) FAST_IO_ASYNC_THROWS;

/** @brief Assembles a byte stream into a typed-only child's whole objects across conversion calls.
 * @details A typed-only child may require an object split across engine calls. The adapter owns
 * the residual bytes and validates the final boundary. Complete objects may be passed immediately.
 */
template <typename Path, ::std::size_t Depth, typename Owner, typename Child>
inline task<void> write_partial_wire_units(Owner &owner, Child &child, ::std::byte const *p, ::std::size_t n, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	auto &carry{owner.wire_remainder};
	constexpr auto width{sizeof(typename Owner::wire_char_type)};
	if (carry.size)
	{
		while (n && carry.size < width) { carry.storage[carry.size++] = *p++; --n; }
		if (carry.size < width) co_return;
		co_await write_layer<Path, Depth, layer_action::write>(child, carry.storage, width, activity);
		carry.size = 0;
	}
	auto whole{n - n % width};
	if (whole) co_await write_layer<Path, Depth, layer_action::write>(child, p, whole, activity);
	for (; whole != n; ++whole) carry.storage[carry.size++] = p[whole];
}

/** @brief Drives one process or drain phase with validated cursors and a single outstanding child write.
 * @details An engine can borrow a compatible child put area. Otherwise the adapter's reusable
 * scratch remains alive until the child accepts its last byte. Trailers use the same path as payload.
 */
template <transcode_phase Phase, typename Path, ::std::size_t Depth, typename Owner, typename Child>
inline task<void> encode_layer(Owner &owner, Child &child, typename Owner::source_unit_type const *first,
	typename Owner::source_unit_type const *last, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	namespace cv = ::fast_io::details;
	namespace decay = ::fast_io::operations::decay;
	using engine_type = typename Owner::engine_ref_type;
	using unit = typename Owner::transformed_unit_type;
	using traits = typename Owner::traits_type;
	decltype(auto) engine{::fast_io::transcode_ref(owner.transcode_engine)};
	if constexpr (Phase == transcode_phase::process) if (first == last) co_return;
	for (;;)
	{
		if constexpr (Path::local_selection::selects(0))
			if (has_layer_peer()) co_await yield();
		if constexpr (::fast_io::outputless_transcode_drain<engine_type, Phase>)
		{
			// The selected stage still yields to peers and validates pending engine state.
			if constexpr (Phase == transcode_phase::finish &&
				decay::defines::has_transcode_finish_checked_define<engine_type>)
			{
				unit scratch;
				auto result{transcode_finish_checked_define(engine, &scratch, &scratch)};
				if (result.to_next != &scratch || result.status != transcode_drain_status::complete) [[unlikely]]
					throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
			}
			else
			{
				decay::transcode_drain_empty<Phase>(engine);
			}
			co_return;
		}
		auto minimum{decay::transcode_min_output_size_decay(transcode_reserve<engine_type>, Phase)};
		// Borrow available space directly. Route each overflow through the child's selected stages.
		auto window{cv::prepare_transcode_destination<typename Owner::allocator_type, traits::secure_clear, false>(
			child, owner.transform_buffer, minimum, Owner::wire_buffer_size)};
		unit *next{};
		bool consumed{}, done{};
		if constexpr (Phase == transcode_phase::process)
		{
			auto result = [&]() FAST_IO_ASYNC_THROWS {
				if constexpr (decay::defines::has_transcode_process_checked_define<engine_type>)
					return transcode_process_checked_define(engine, first, last, window.first, window.last);
				else
					return decay::transcode_process_decay_dispatch(engine, first, last, window.first, window.last);
			}();
			auto cursor{cv::validate_transcode_closed_range_offsets(first, last, result.from_next)};
			done = result.status == transcode_step_status::need_input;
			if (!cursor.valid || (done ? cursor.current != last : result.status != transcode_step_status::need_output))
				throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
			consumed = cursor.current != first;
			first = cursor.current;
			next = result.to_next;
		}
		else
		{
			auto result = [&]() FAST_IO_ASYNC_THROWS {
				if constexpr (Phase == transcode_phase::finish &&
					decay::defines::has_transcode_finish_checked_define<engine_type>)
					return transcode_finish_checked_define(engine, window.first, window.last);
				else if constexpr (Phase == transcode_phase::finish)
					return decay::transcode_finish_decay_dispatch(engine, window.first, window.last);
				else return decay::transcode_sync_flush_decay_dispatch(engine, window.first, window.last);
			}();
			done = result.status == transcode_drain_status::complete;
			if (!done && result.status != transcode_drain_status::need_output)
				throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
			next = result.to_next;
		}
		auto produced{cv::validate_transcode_closed_range_offsets(window.first, window.last, next)};
		if (!produced.valid || (!done && !consumed && !produced.current_offset))
			throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
		if (produced.current_offset)
		{
			if constexpr (::fast_io::transcode_output_window<unit, Child>)
				if (window.direct)
				{
					visit_layer(activity, true, Depth + Path::local_count);
					obuffer_set_curr(child, reinterpret_cast<typename Child::output_char_type *>(produced.current));
					if (done) co_return;
					continue;
				}
			using child_path = typename Path::child_selection;
			constexpr auto child_depth{Depth + Path::local_count};
			auto bytes{reinterpret_cast<::std::byte const *>(window.first)};
			auto length{produced.current_offset * sizeof(unit)};
			// Only a typed-only child can require a separate coroutine to assemble one object across calls.
			if constexpr (Owner::assemble_wire_units)
				co_await write_partial_wire_units<child_path, child_depth>(owner, child, bytes, length, activity);
			else
				co_await write_layer<child_path, child_depth, layer_action::write>(child, bytes, length, activity);
		}
		if (done) co_return;
	}
}

/** @brief Applies the public-operation plan to one output layer, preserving buffering, ownership and message order. */
template <typename Path, ::std::size_t Depth, layer_action Action, typename R>
inline task<void> write_layer(R &out, ::std::byte const *data, ::std::size_t size, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	using C = typename R::output_char_type;
	if constexpr (Action == layer_action::write) if (!size) co_return;
	visit_layer(activity, true, Depth);
	if constexpr (requires { async_layer_write_define(io_type<Path>, ::std::integral_constant<::std::size_t, Depth>{},
		::std::integral_constant<layer_action, Action>{}, out, data, size, activity); })
	{
		co_await async_layer_write_define(io_type<Path>, ::std::integral_constant<::std::size_t, Depth>{},
			::std::integral_constant<layer_action, Action>{}, out, data, size, activity);
	}
	else if constexpr (requires { typename Path::child_selection; out.ptr->transcode_engine; })
	{
		auto &owner{*out.ptr};
		using owner_type = typename R::owner_type;
		using source = typename owner_type::source_unit_type;
		if constexpr (Action == layer_action::finish)
			if (owner.state == otranscoder_state::finished) co_return;
		owner.require_open();
		layer_transaction transaction{owner.state};
		auto &put{owner.public_buffer};
		decltype(auto) child{::fast_io::operations::output_stream_ref(owner.handle.get())};
		auto drain_public = [&]() FAST_IO_ASYNC_THROWS {
			return encode_layer<transcode_phase::process, Path, Depth>(owner, child,
				reinterpret_cast<source const *>(put.buffer_begin), reinterpret_cast<source const *>(put.buffer_curr), activity);
		};
		if constexpr (Action == layer_action::write)
		{
			// Only a typed engine needs complete public objects. A byte engine accepts every prefix,
			// even when a prior short request ended inside a wide public character.
			if constexpr (::std::same_as<source, ::std::byte>)
			{
				if (size && (sizeof(C) > 1u || size >= owner_type::traits_type::public_buffer_size * sizeof(C)))
				{
					if (put.buffer_begin != put.buffer_curr) { co_await drain_public(); put.buffer_curr = put.buffer_begin; }
					co_await encode_layer<transcode_phase::process, Path, Depth>(owner, child, data, data + size, activity);
					transaction.committed = true;
					co_return;
				}
			}
			{
				owner.ensure_public_buffer();
				while (size)
				{
					if (put.buffer_curr == put.buffer_end) { co_await drain_public(); put.buffer_curr = put.buffer_begin; }
					auto &partial{owner.source_remainder};
					if (partial.size || size < sizeof(C))
					{
						while (partial.size < sizeof(C) && size) { partial.storage[partial.size++] = *data++; --size; }
						if (partial.size == sizeof(C))
						{
							::fast_io::freestanding::my_memcpy(put.buffer_curr++, partial.storage, sizeof(C));
							if constexpr (owner_type::traits_type::secure_clear) ::fast_io::secure_clear(partial.storage, sizeof(C));
							partial.size = 0;
						}
						continue;
					}
					auto count{static_cast<::std::size_t>(put.buffer_end - put.buffer_curr)};
					if (count > size / sizeof(C)) count = size / sizeof(C);
					auto bytes{count * sizeof(C)};
					::fast_io::freestanding::my_memcpy(put.buffer_curr, data, bytes);
					put.buffer_curr += count;
					data += bytes;
					size -= bytes;
					if (put.buffer_curr == put.buffer_end) { co_await drain_public(); put.buffer_curr = put.buffer_begin; }
				}
			}
		}
		else
		{
			if (owner.source_remainder.size && Action == layer_action::finish)
				throw_transcode_stream_error(transcode_stream_errc::incomplete_unit);
			if (put.buffer_begin != put.buffer_curr) { co_await drain_public(); put.buffer_curr = put.buffer_begin; }
			constexpr auto phase{Action == layer_action::finish ? transcode_phase::finish : transcode_phase::sync_flush};
			co_await encode_layer<phase, Path, Depth>(owner, child, nullptr, nullptr, activity);
			if constexpr (Action == layer_action::finish && owner_type::assemble_wire_units)
				if (owner.wire_remainder.size) throw_transcode_stream_error(transcode_stream_errc::incomplete_unit);
			constexpr bool finish{Action == layer_action::finish && requires { owner.handle.finish_owned_output(child); }};
			co_await write_layer<typename Path::child_selection, Depth + Path::local_count,
				finish ? layer_action::finish : layer_action::publish>(child, nullptr, 0, activity);
			if constexpr (Action == layer_action::finish) owner.state = otranscoder_state::finished;
		}
		transaction.committed = true;
	}
	else if constexpr (requires { out.iobptr->output_buffer.buffer_begin; out.iobptr->handle; })
	{
		using buffer = typename R::io_buffer_type;
		using child_path = decltype(buffer_child_selection<Path>());
		auto &put{out.iobptr->output_buffer};
		decltype(auto) child{::fast_io::operations::output_stream_ref(out.iobptr->handle)};
		auto capacity = buffer::traits_type::output_buffer_size;
		if constexpr (Action == layer_action::write)
		{
			if (size % sizeof(C)) raise_invalid_progress(0);
			if (!put.buffer_begin && size / sizeof(C) < capacity)
			{
				using alloc = ::fast_io::typed_generic_allocator_adapter<typename buffer::traits_type::allocator_type, C>;
				put.buffer_begin = put.buffer_curr = alloc::allocate(capacity);
				put.buffer_end = put.buffer_begin + capacity;
			}
			if (put.buffer_curr && size / sizeof(C) <= static_cast<::std::size_t>(put.buffer_end - put.buffer_curr))
			{
				::fast_io::freestanding::my_memcpy(put.buffer_curr, data, size);
				put.buffer_curr += size / sizeof(C);
				co_return;
			}
		}
		if (put.buffer_curr != put.buffer_begin)
		{
			co_await write_layer<child_path, Depth + 1, layer_action::write>(child,
				reinterpret_cast<::std::byte const *>(put.buffer_begin),
				static_cast<::std::size_t>(put.buffer_curr - put.buffer_begin) * sizeof(C), activity);
			put.buffer_curr = put.buffer_begin;
		}
		if constexpr (Action == layer_action::write)
			co_await write_layer<child_path, Depth + 1, Action>(child, data, size, activity);
	}
	else if constexpr (Action != layer_action::write)
	{
		auto publish = [&]() FAST_IO_ASYNC_THROWS {
			if constexpr (Action == layer_action::finish && requires { output_stream_finish_define(out); })
				output_stream_finish_define(out);
			else if constexpr (::fast_io::output_stream_publishable<R>) output_stream_publish_define(out);
			else if constexpr (::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<R>)
				::fast_io::operations::decay::output_stream_buffer_flush_decay_dispatch(out);
		};
		if constexpr (requires { output_stream_finish_define(out); } || ::fast_io::output_stream_publishable<R> ||
			::fast_io::operations::decay::defines::output_stream_buffer_flush_dispatchable<R>)
		{
			if constexpr (Path::selects(0)) co_await invoke_device<immediate_output<R>>(publish);
			else publish();
		}
	}
	else if constexpr (Path::selects(0) && awaitable_output<R>)
	{
		while (size)
		{
			auto result{co_await async_write_some_define(out, data, size)};
			if (result.error.raise) result.error.raise(result.error.code);
			if (!result.transferred || result.transferred > size) raise_invalid_progress(0);
			data += result.transferred;
			size -= result.transferred;
		}
	}
	else
	{
		auto write = [&]() FAST_IO_ASYNC_THROWS {
			if constexpr (::fast_io::operations::decay::defines::bytes_writable<R>)
				::fast_io::operations::decay::write_all_bytes_decay_dispatch(out, data, data + size);
			else if constexpr (sizeof(C) == 1 && ::std::same_as<C, char>)
				write_observed(out, reinterpret_cast<char const *>(data), reinterpret_cast<char const *>(data + size));
			else
			{
				if (size % sizeof(C)) raise_invalid_progress(0);
				C units[64];
				while (size)
				{
					auto n{size < sizeof(units) ? size : sizeof(units)};
					::fast_io::freestanding::my_memcpy(units, data, n);
					::fast_io::operations::decay::write_all_decay_dispatch(out, units, units + n / sizeof(C));
					data += n;
					size -= n;
				}
			}
		};
		// A mark on the encoder gives no permission to move an unmarked sink to a worker.
		if constexpr (Path::selects(0)) co_await invoke_device<immediate_output<R>>(write);
		else write();
	}
}

/** @brief Refills a decoder's persistent public window without consuming lookahead owned by the next scan. */
template <typename Path, ::std::size_t Depth, bool Transfer = false, typename R>
inline task<bool> refill_layer(R &in, file_activity &activity, ::std::size_t output_bytes = 0) FAST_IO_ASYNC_THROWS
{
	visit_layer(activity, false, Depth);
	if constexpr (requires { in.iobptr->input_buffer.buffer_begin; })
	{
		using buffer = typename R::io_buffer_type;
		using traits = typename buffer::traits_type;
		using C = typename buffer::input_char_type;
		using child_path = decltype(buffer_child_selection<Path>());
		auto &window{in.iobptr->input_buffer};
		if (window.buffer_curr != window.buffer_end) co_return true;
		if constexpr ((traits::mode & buffer_mode::out) == buffer_mode::out && (traits::mode & buffer_mode::tie) == buffer_mode::tie)
			co_await write_layer<Path, Depth, layer_action::publish>(in, nullptr, 0, activity);
		if (!window.buffer_begin)
		{
			using alloc = ::fast_io::typed_generic_allocator_adapter<typename traits::allocator_type, C>;
			window.buffer_begin = window.buffer_curr = window.buffer_end = alloc::allocate(traits::input_buffer_size);
		}
		decltype(auto) child{::fast_io::operations::input_stream_ref(in.iobptr->handle)};
		auto p{reinterpret_cast<::std::byte *>(window.buffer_begin)};
		auto got{co_await read_layer<child_path, Depth + 1>(child, p, traits::input_buffer_size * sizeof(C), activity)};
		if (!got) co_return false;
		while (got % sizeof(C))
		{
			auto part{co_await read_layer<child_path, Depth + 1>(child, p + got, sizeof(C) - got % sizeof(C), activity)};
			if (!part) raise_truncated_element(0);
			got += part;
		}
		window.buffer_curr = window.buffer_begin;
		window.buffer_end = window.buffer_begin + got / sizeof(C);
		co_return true;
	}
	else
	{
		auto &owner{*in.ptr};
		using owner_type = typename R::owner_type;
		using unit = typename owner_type::source_unit_type;
		constexpr bool batch_public_bytes{Transfer && owner_type::direct_public_units &&
			sizeof(typename owner_type::input_char_type) == 1 && sizeof(unit) == 1 &&
			owner_type::traits_type::publication_mode == transcode_input_publication_mode::streaming_unverified &&
			::fast_io::batchable_transcode_input<typename owner_type::engine_type>};
		if (owner.public_buffer.buffer_curr != owner.public_buffer.buffer_end) co_return true;
		if (owner.state == itranscoder_state::finished) co_return false;
		owner.require_open();
		layer_transaction transaction{owner.state};
		owner.before_source_refill();
		decltype(auto) child{::fast_io::operations::input_stream_ref(owner.handle.get())};
		decltype(auto) engine{::fast_io::transcode_ref(owner.transcode_engine)};
		auto &source{owner.source_buffer};
		::fast_io::details::supplied_transcode_input window;
		for (;;)
		{
			if constexpr (Path::local_selection::selects(0))
				if (has_layer_peer()) co_await yield();
			if constexpr (batch_public_bytes)
			{
				// Size the initial empty window from bytes already available. A short physical read
				// publishes its first decoded prefix without waiting to fill the requested window.
				constexpr ::std::size_t batch{4096u};
				if (output_bytes >= batch && source.buffer_curr != source.buffer_end &&
					source.buffer_end - source.buffer_curr >= static_cast<::std::ptrdiff_t>(batch) &&
					owner.public_buffer.buffer_begin == owner.public_buffer.buffer_end)
				{
					::fast_io::details::ensure_transcode_input_buffer<typename owner_type::allocator_type,
						owner_type::traits_type::secure_clear>(owner.public_buffer, batch, batch);
				}
			}
			bool ready;
			if constexpr (owner_type::target_binding == transcode_unit_binding::exact_units)
				ready = owner.refill_exact(engine, window);
			else if constexpr (batch_public_bytes)
			{
				// A preceding scan may already have decoded bytes beyond its public get area.
				// Publish that lookahead before requesting the next block from the engine.
				if (owner.transform_buffer.buffer_curr != owner.transform_buffer.buffer_end)
					ready = owner.refill_typed_from_bytes(engine, window);
				else
					ready = owner.refill_exact(engine, window);
			}
			else ready = owner.refill_typed_from_bytes(engine, window);
#if defined(__HERBCEPTIONS__)
			if (window.failure.domain != nullptr) [[unlikely]] throw throws window.failure;
#endif
			if (ready || owner.source_eof)
			{
				transaction.committed = true;
				co_return ready;
			}
			::fast_io::details::ensure_transcode_input_buffer<typename owner_type::allocator_type, owner_type::traits_type::secure_clear>(
				source, sizeof(typename owner_type::normalized_input_ref_type::input_char_type), owner_type::traits_type::source_buffer_size);
			auto p{reinterpret_cast<::std::byte *>(source.buffer_begin)};
			auto n{static_cast<::std::size_t>(source.buffer_capacity_end - source.buffer_begin) * sizeof(unit)};
			auto got{co_await read_layer<typename Path::child_selection, Depth + Path::local_count>(child, p, n, activity)};
			while (got % sizeof(unit))
			{
				auto suffix{co_await read_layer<typename Path::child_selection, Depth + Path::local_count>(
					child, p + got, sizeof(unit) - got % sizeof(unit), activity)};
				if (!suffix) raise_truncated_element(0);
				got += suffix;
			}
			source.buffer_curr = source.buffer_begin;
			source.buffer_end = source.buffer_begin + got / sizeof(unit);
			owner.source_eof = !got;
		}
	}
}

/** @brief Reads at most one logical prefix, retaining undecoded tails and physical EOF in the original adapter.
 * @details The codec's synchronous refill state machine validates authentication, cursor progress and
 * final object boundaries. A supplied window lets it request another async read without treating a
 * short chunk as EOF or restarting an incomplete character or record.
 */
template <typename Path, ::std::size_t Depth, bool Transfer, typename R>
inline task<::std::size_t> read_layer(R &in, ::std::byte *data, ::std::size_t size, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	if (!size) co_return ::std::size_t{};
	visit_layer(activity, false, Depth);
	if constexpr (requires { async_layer_read_define(io_type<Path>, ::std::integral_constant<::std::size_t, Depth>{},
		in, data, size, activity); })
		co_return co_await async_layer_read_define(io_type<Path>, ::std::integral_constant<::std::size_t, Depth>{},
			in, data, size, activity);
	else if constexpr (requires { typename Path::child_selection; in.ptr->transcode_engine; })
	{
		if (!(co_await refill_layer<Path, Depth, Transfer>(in, activity, size))) co_return ::std::size_t{};
		co_return static_cast<::std::size_t>(in.ptr->copy_public_bytes(data, data + size) - data);
	}
	else if constexpr (requires { in.iobptr->input_buffer.buffer_curr; })
	{
		using C = typename R::input_char_type;
		if (size < sizeof(C)) raise_invalid_progress(0);
		if (!(co_await refill_layer<Path, Depth>(in, activity))) co_return ::std::size_t{};
		auto first{ibuffer_curr(in)};
		auto units{static_cast<::std::size_t>(ibuffer_end(in) - first)};
		if (units > size / sizeof(C)) units = size / sizeof(C);
		::fast_io::freestanding::my_memcpy(data, first, units * sizeof(C));
		ibuffer_set_curr(in, first + units);
		co_return units * sizeof(C);
	}
	else if constexpr (Path::selects(0) && awaitable_input<R>)
	{
		auto result{co_await async_read_some_define(in, data, size)};
		if (result.error.raise) result.error.raise(result.error.code);
		if (result.transferred > size) raise_invalid_progress(0);
		co_return result.transferred;
	}
	else
	{
		auto read = [&]() FAST_IO_ASYNC_THROWS -> ::std::size_t {
			using C = typename R::input_char_type;
			if constexpr (::fast_io::operations::decay::defines::bytes_readable<R>)
				return static_cast<::std::size_t>(::fast_io::operations::decay::read_some_bytes_decay_dispatch(in, data, data + size) - data);
			else
			{
				C units[256];
				auto n{size < sizeof(units) ? size / sizeof(C) : 256u};
				if (!n) raise_invalid_progress(0);
				auto end{::fast_io::operations::decay::read_some_decay_dispatch(in, units, units + n)};
				auto bytes{static_cast<::std::size_t>(end - units) * sizeof(C)};
				::fast_io::freestanding::my_memcpy(data, units, bytes);
				return bytes;
			}
		};
		// The decoder borrows its unmarked source on this activation, including thread-local state.
		if constexpr (Path::selects(0)) co_return co_await invoke_device<immediate_input<R>>(read);
		else co_return read();
	}
}
} // namespace fast_io::async::details
