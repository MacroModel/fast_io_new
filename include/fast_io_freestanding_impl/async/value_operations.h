#pragma once

namespace fast_io::async::details
{
/** @brief Builds nontrivial values in caller-owned storage across both standard and deterministic exception ABIs. */
template <typename V>
struct issue_slot
{
	union { char empty; V value; };
	bool active{};
	issue_slot() noexcept : empty{} {}
	issue_slot(issue_slot const &)=delete;
	~issue_slot() {if (active) value.~V();}

	/** @brief Constructs a lazily needed object in place and registers it for destruction only after success. */
	template <typename T>
	V &get_or_construct(T &source)
	{
		if (!active)
		{
			::new (static_cast<void *>(::fast_io::freestanding::addressof(value))) V(source);
			active = true;
		}
		return value;
	}
};

template <typename Char,typename T,bool=annotated_operand<T> && async_print_producer<Char,unannotated_t<T>>> struct capture_field_type {using type=print_record<Char>;};
template <typename Char,typename T> struct capture_field_type<Char,T,true> {using type=print_capture_type<Char,unannotated_t<T>>;};

/** @brief Freezes an ordinary spelling while the operands of the issuing call are still alive. */
template <typename Char, typename T>
inline void freeze_print_record(print_record<Char> &record,T &&value) FAST_IO_ASYNC_THROWS
{
	auto &&source{value_operand(static_cast<T &&>(value))};
	using source_type = ::std::remove_reference_t<decltype(source)>;
	using traits = ::fast_io::details::decay::print_static_scatter_traits<Char, ::std::remove_cv_t<source_type>>;
	if constexpr (::std::same_as<::std::remove_cv_t<source_type>, ::fast_io::io_null_t>)
	{
		return;
	}
	else if constexpr (!::std::is_volatile_v<source_type> &&
					   requires { requires traits::available && traits::size <= inline_record_capacity; })
	{
		auto span{traits::define(source)};
		record.template spell_inline<traits::size>([span](Char *dest) noexcept {
			return ::fast_io::details::non_overlapped_copy_n(span.base, span.len, dest);
		});
	}
	else
	{
		typename print_record<Char>::base scratch;
		::fast_io::details::print_snapshot_fields<false>(scratch, text_operand<Char>(static_cast<decltype(source) &&>(source)));
		record.adopt(scratch);
	}
}

/** @brief Constructs each independent snapshot before issue returns and registers it for unwinding immediately. */
template <typename Char,typename T,typename Saved>
inline void freeze_field(issue_slot<Saved> &slot,T &&value) FAST_IO_ASYNC_THROWS
{
	auto *address{static_cast<void *>(::fast_io::freestanding::addressof(slot.value))};
	if constexpr (annotated_operand<T> && async_print_producer<Char,unannotated_t<T>>)
	{
		::new(address) Saved(async_print_capture_define(io_reserve_type<Char,::std::remove_cvref_t<unannotated_t<T>>>,value_operand(static_cast<T &&>(value))));
		slot.active=true;
	}
	else
	{
		// Render an unmarked field now so no borrowed formatting data reaches the coroutine.
		::new(address) Saved;
		slot.active=true;
		freeze_print_record(slot.value,static_cast<T &&>(value));
	}
}

/** @brief Keeps producer state separate from the captured value so a cursor may point into its final frame. */
template <typename Char,typename Saved>
struct print_field_cursor { using type = print_cursor_type<Char,Saved>; };
template <typename Char>
struct print_field_cursor<Char,print_record<Char>> { using type = ::std::size_t; };

/** @brief A prefix writer can report physical admission to the public record while retaining its own continuation. */
template <typename R>
concept forwards_prefix_admission = requires(R &out, ::std::byte const *data, ::std::size_t n, task<void>::promise_type &owner) {
	async_forward_admission_define(async_write_some_define(out, data, n), owner, true);
};

/** @brief Transfers a sealed prefix, retaining its storage through every short write before reusing the window. */
template <bool Async, typename R, typename Char>
inline task<void> write_value_prefix(R &out, Char const *first, Char const *last, task<void>::handle_type owner, bool run_ahead) FAST_IO_ASYNC_THROWS
{
	if constexpr (Async && awaitable_output<R>)
	{
		auto p{reinterpret_cast<::std::byte const *>(first)};
		auto bytes{static_cast<::std::size_t>(last - first) * sizeof(Char)};
		while (bytes)
		{
			auto request = [&]() -> decltype(auto) {
				if constexpr (forwards_prefix_admission<R>)
					return async_forward_admission_define(async_write_some_define(out, p, bytes), owner.promise(), run_ahead);
				else return async_write_some_define(out, p, bytes);
			};
			auto result{co_await request()};
			run_ahead = false;
			if (result.error.raise) result.error.raise(result.error.code);
			if (!result.transferred || result.transferred > bytes) raise_invalid_progress(0);
			p += result.transferred;
			bytes -= result.transferred;
		}
	}
	else
	{
		auto write = [&]() FAST_IO_ASYNC_THROWS { write_observed(out, first, last); };
		if constexpr (!Async || immediate_output<R>) write();
		else co_await invoke_blocking(write);
	}
}

/** @brief Holds the sole outstanding write of a prefix whose bytes already belong to the parent frame.
 * @details Both continuations run on the same issuer. Starting the child runs it through immediate short
 * writes up to its first actual wait. Joining a suspended child installs a continuation without resuming
 * its I/O request twice. The parent drains this slot before another write, any fallible work or return.
 */
struct pending_prefix_write
{
	task<void>::handle_type handle{};

	/** @brief True while this slot owns the writer, including an immediately available error. */
	explicit operator bool() const noexcept { return static_cast<bool>(handle); }

	/** @brief Transfers the fresh child into stable storage and starts it on the current issuer. */
	void start(task<void> &&child) noexcept
	{
		handle = child.release();
		handle.resume();
	}
	/** @brief A completed prefix can release its frame on this activation of the producer. */
	bool await_ready() const noexcept { return handle.done(); }
	/** @brief Leaves resumption of the writer exclusively to the outstanding device request. */
	void await_suspend(::std::coroutine_handle<> parent) const noexcept { handle.promise().continuation = parent; }
	/** @brief Releases the completed child's frame on both success and error delivery. */
	void await_resume() FAST_IO_ASYNC_THROWS
	{
		task<void> completed{::std::exchange(handle, {})};
		completed.frame.promise().rethrow();
	}
};

/** @brief Identifies the public record that owns the prefix writer's observations.
 * @details A caller waiting privately for a single start may leave the event loop at that start. A joined
 * or independently submitted root keeps its issuer and storage alive until its outstanding work completes.
 */
struct prefix_issuer
{
	task<void>::handle_type owner{};
	constexpr bool await_ready() const noexcept { return false; }
	bool await_suspend(task<void>::handle_type parent) noexcept
	{
		owner = parent;
		return false;
	}
	constexpr task<void>::handle_type await_resume() const noexcept { return owner; }
};

/** @brief Borrows whole windows from the first nonempty frozen field, skipping empty format literals.
 * @details The cursor advances only after the destination has granted permission to stream. Leaving the
 * tail for ordinary generation lets subsequent fields share its write. Every deferred producer retains
 * its original position and all borrowed bytes remain inside the operation's immutable field capture.
 */
template <typename Char, ::std::size_t Capacity, typename Values, typename Cursors, ::std::size_t... I>
inline auto leading_print_prefix(Values const &saved, Cursors &cursors, ::std::index_sequence<I...>) noexcept
{
	struct range { Char const *first{}; ::std::size_t size{}; ::std::size_t *cursor{}; } result;
	auto examine = [&]<::std::size_t N>() {
		auto &v{::fast_io::containers::get<N>(saved)};
		if constexpr (::std::same_as<::std::remove_cvref_t<decltype(v)>, print_record<Char>>)
		{
			auto n{static_cast<::std::size_t>(v.end() - v.begin())};
			if (!n) return false;
			if (n >= Capacity)
				result = {v.begin(), n - n % Capacity, ::fast_io::freestanding::addressof(::fast_io::containers::get<N>(cursors))};
		}
		return true;
	};
	(void)(examine.template operator()<I>() || ...);
	return result;
}

/** @brief Advances one selected producer without retaining a pointer into a previously offered window. */
template <typename Char,typename Cursor,typename Saved>
inline auto generate_field(Cursor &cursor,Saved const &saved,Char *first,Char *last) FAST_IO_ASYNC_THROWS
{
	if constexpr (::std::same_as<Saved,print_record<Char>>)
	{
		auto n{static_cast<::std::size_t>(last-first)};
		if (static_cast<::std::size_t>(saved.end()-saved.begin())-cursor < n) n=static_cast<::std::size_t>(saved.end()-saved.begin())-cursor;
		::fast_io::details::non_overlapped_copy_n(saved.begin()+cursor,n,first);
		cursor+=n;
		return context_print_result<Char *>{first+n,cursor==static_cast<::std::size_t>(saved.end()-saved.begin())};
	}
	else return async_print_step_define(io_reserve_type<Char,Saved>,cursor,saved,first,last);
}

/** @brief Detects an explicit dependency between the first physical write and an argument event.
 * @details Projection retains dependencies across unselected local starts. Ordinary streams and
 * providers that order only their own formatter steps need no run-time scheduling check.
 */
template <typename Graph, typename Binding>
consteval bool has_ordered_prefix_write() noexcept
{
	if constexpr (requires { async_point_dependencies_define(io_type<Graph>); })
	{
		constexpr auto order{publication_dependencies<observed_graph<Graph, Binding>>()};
		for (::std::size_t i{}; i != order.size; ++i)
		{
			auto edge{order.edges[i]};
			if ((edge.before == output_point<0> && edge.after.side == point_side::argument) ||
				(edge.after == output_point<0> && edge.before.side == point_side::argument))
				return true;
		}
	}
	return false;
}

/**
 * @brief Bounds the spelling actually stored or produced for one asynchronous print field.
 * @details An incremental producer owns its complete spelling bound. A frozen field uses the
 * same character representation as issue-time formatting. A custom alias or forwarding hook
 * may select a longer representation, so a bound on the original type cannot certify that path.
 */
template <typename C, typename A>
consteval ::std::size_t sealed_field_maximum() noexcept
{
	using source = ::std::remove_cvref_t<unannotated_t<A>>;
	using saved = typename capture_field_type<C, A>::type;
	if constexpr (annotated_operand<A> && async_print_producer<C, unannotated_t<A>>)
	{
		if constexpr (async_print_statically_bounded<C, saved>)
			return async_print_max_size_define(io_reserve_type<C, saved>);
		else
			return SIZE_MAX;
	}
	else if constexpr (::std::same_as<source, ::fast_io::io_null_t>)
		return 0;
	else if constexpr (::fast_io::details::decay::print_static_scatter_traits<C, source>::available)
		return ::fast_io::details::decay::print_static_scatter_traits<C, source>::size;
	else if constexpr (::std::is_bounded_array_v<source>)
		return record_argument_bound<C, decltype(text_operand<C>(::std::declval<unannotated_t<A>>()))>();
	else if constexpr (::fast_io::alias_printable<source &> || ::fast_io::alias_printable<source const &> ||
		::fast_io::alias_printable<source> || ::fast_io::status_io_print_forwardable<C, source &> ||
		::fast_io::status_io_print_forwardable<C, source const &> || ::fast_io::status_io_print_forwardable<C, source>)
		return SIZE_MAX;
	else if constexpr (annotated_operand<A> && ::fast_io::reserve_printable<C, source>)
		return print_reserve_size(io_reserve_type<C, source>);
	else
		return record_argument_bound<C, decltype(text_operand<C>(::std::declval<unannotated_t<A>>()))>();
}

/**
 * @brief Chooses the coroutine's private window without introducing an earlier transport publication.
 * @details If every partial sum is strictly below the inline capacity, no field can fill that window,
 * even when a final zero-byte step still needs a writable span. The newline has its own extra slot.
 * Equality, overflow and unknown bounds retain the full streaming window and its prefix boundaries.
 */
template <typename C, typename... A>
consteval ::std::size_t sealed_record_window() noexcept
{
	constexpr ::std::size_t full_window{(sizeof(::std::size_t) <= 2 ? 128u : 4096u) / sizeof(C)};
	if constexpr (full_window > inline_record_capacity)
	{
		constexpr ::std::size_t maxima[]{sealed_field_maximum<C, A>()..., 0};
		::std::size_t total{};
		for (auto bound : maxima)
		{
			// A small complete record must never trigger a new visible prefix boundary.
			if (bound >= inline_record_capacity - total)
				return full_window;
			total += bound;
		}
		return inline_record_capacity;
	}
	return full_window;
}

/** @brief Streams an ordered record through one producer frame, preserving short-write offsets and one final finish. */
template <bool Line,typename Binding,bool PrepareValues,::std::size_t Capacity,typename O,typename... Saved,::std::size_t... I,::std::size_t... Position,bool... Observed>
inline task<void> drive_print_values(capture<O> file,::fast_io::containers::tuple<Saved...> saved,
	::std::index_sequence<I...>,::std::index_sequence<Position...>,::std::integer_sequence<bool,Observed...>) FAST_IO_ASYNC_THROWS
{
	using Ref=output_ref_type<O>;
	using Char=typename Ref::output_char_type;
	using Guard = ::fast_io::operations::basic_output_operation_guard<decltype(captured_operand(file))>;
	using Graph = typename Binding::graph_type;
	constexpr bool layered_write{uses_selected_io_layers<false, O, Ref>};
	// A mutex protects one complete publication. No lock can migrate with a suspended coroutine.
	constexpr bool locked_record{::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<Ref>};
	constexpr bool stream_prefixes{!locked_record && !PrepareValues && prefix_output<Ref> &&
		(!layered_write || !requires { async_point_dependencies_define(io_type<Graph>); }) &&
		((::std::same_as<Saved,print_record<Char>> || prefix_print_producer<Char,Saved>) && ...)};
	constexpr bool transport_start{Binding::async_output && !layered_write && Graph::output::count == 1 &&
		Graph::starts_at(output_point<0>) && Graph::kind(output_point<0>) == point_kind::transport};
	constexpr bool ordered_write{stream_prefixes && transport_start && has_ordered_prefix_write<Graph, Binding>()};
	constexpr bool lookahead{stream_prefixes && !layered_write && Binding::async_output && issuer_prefix_output<Ref> &&
		(independent_print_producer<Char, Saved> || ...) &&
		((::std::same_as<Saved, print_record<Char>> || independent_print_producer<Char, Saved>) && ...) &&
		!requires { async_point_dependencies_define(io_type<Graph>); }};
	::std::conditional_t<lookahead, pending_prefix_write, no_point_selection> pending;
	::std::conditional_t<lookahead, ::fast_io::details::local_operator_new_array_ptr<Char, ::fast_io::native_global_allocator>,
		no_point_selection> alternate;
	::std::conditional_t<stream_prefixes, issue_slot<Guard>, no_point_selection> early_guard;
	bool prefixes_allowed{}, prefix_written{};
	::fast_io::containers::tuple<typename print_field_cursor<Char,Saved>::type...> cursors{};
	constexpr ::std::size_t capacity{Capacity};
	Char local[capacity+Line];
	Char *text{local};
	::std::size_t used{};
	typename print_record<Char>::base overflow;
	auto private_out{::fast_io::operations::output_stream_ref(overflow)};
	task<void>::handle_type issuer{};
	if constexpr (lookahead || ordered_write || (locked_record && Binding::async_output) ||
		(stream_prefixes && Binding::async_output && forwards_prefix_admission<Ref>))
		issuer = co_await prefix_issuer{};
	bool joined{};
	if constexpr (lookahead) joined = current_loop && (issuer.promise().continuation || submitted_root(issuer));
	if constexpr (lookahead && (::std::same_as<Saved, print_record<Char>> || ...))
	{
		auto prefix{leading_print_prefix<Char, capacity>(saved, cursors, ::std::index_sequence<I...>{})};
		if (prefix.size && joined)
		{
			auto &&owner{captured_operand(file)};
			auto &out{early_guard.get_or_construct(owner).ref()};
			prefixes_allowed = async_output_accepts_prefixes_define(out);
			if (prefixes_allowed)
			{
				if constexpr (transport_start) co_await start_point(output_point<0>);
				pending.start(write_value_prefix<true>(out, prefix.first, prefix.first + prefix.size, issuer, true));
				// An immediate failure still precedes every later formatter.
				if (pending.await_ready()) co_await pending;
				*prefix.cursor = prefix.size;
				prefix_written = true;
			}
		}
	}
	for (::std::size_t field{};field<sizeof...(Saved);)
	{
		context_print_result<Char *> step{};
		value_checkpoint checkpoint{};
		constexpr ::std::size_t positions[]{Position...,0};
		bool observable{}, selected_step{};
		auto advance=[&]<::std::size_t N>() FAST_IO_ASYNC_THROWS {
			auto &value{::fast_io::containers::get<N>(saved)};
			constexpr bool visible[]{Observed...,false};
			observable=visible[N];
			auto next{generate_field(::fast_io::containers::get<N>(cursors),value,text+used,text+capacity)};
			step={next.iter,next.done};
			if constexpr (requires { next.checkpoint; }) checkpoint=next.checkpoint;
			if constexpr (!::std::same_as<::std::remove_cvref_t<decltype(value)>,print_record<Char>>)
			{
				using selection = typename Binding::template value_selection<positions[N]>;
				// Literal spans inserted by fmt have no producer and never acquire a value's selection.
				selected_step = selection::all || (checkpoint.ordinal && selection::selects(checkpoint.ordinal-1)) ||
					(step.done && visible[N] && Binding::selected(argument_point<positions[N]>));
			}
		};
		(void)((field==I && (advance.template operator()<I>(),true)) || ...);
		// A new checkpoint also permits a CPU-only step with no encoded characters.
		::fast_io::details::decay::validate_context_print_result(text+used,text+capacity,step.iter,step.done || checkpoint.ordinal!=0);
		if constexpr (ordered_write)
		{
			// Bytes preceding this checkpoint are sealed. A successor may observe the first write here.
			if (!prefix_written && (!early_guard.active || prefixes_allowed) &&
				(step.iter != text || overflow.curr_ptr != overflow.begin_ptr))
			{
				auto &promise{issuer.promise()};
				if (promise.check_point(promise.history, output_point<0>, 1u, true) != static_cast<::std::size_t>(-1))
				{
					auto &&owner{captured_operand(file)};
					if (!early_guard.active)
						prefixes_allowed = async_output_accepts_prefixes_define(early_guard.get_or_construct(owner).ref());
					if (prefixes_allowed)
					{
						Char const *first{text}, *last{step.iter};
						if (overflow.curr_ptr != overflow.begin_ptr)
						{
							// Earlier full windows stayed private while the transport awaited its predecessor.
							if (first != last) ::fast_io::operations::decay::write_all_decay_dispatch(private_out, first, last);
							first = overflow.begin_ptr;
							last = overflow.curr_ptr;
						}
						co_await start_point(output_point<0>);
						co_await write_value_prefix<true>(early_guard.value.ref(), first, last, issuer, false);
						overflow.curr_ptr = overflow.begin_ptr;
						step.iter = text;
						prefix_written = true;
					}
				}
			}
		}
		if (checkpoint.ordinal)
		{
			observable_point point{point_side::argument,positions[field],checkpoint.ordinal};
			if (!checkpoint.starting || Binding::selected(point))
				co_await publish_point{point,checkpoint.skipped ? 2u : 1u,checkpoint.starting};
		}
		if (step.done)
		{
			if (observable) co_await complete_point({point_side::argument,positions[field]});
			if (++field==sizeof...(Saved))
			{
				if constexpr (Line) *step.iter++=::fast_io::char_literal_v<u8'\n',Char>;
			}
		}
		used=static_cast<::std::size_t>(step.iter-text);
		// A full sealed window can reach a byte stream while the next argument has yet to start.
		if (used==capacity && field!=sizeof...(Saved))
		{
			if constexpr (stream_prefixes)
			{
				if (!early_guard.active)
				{
					auto &&owner{captured_operand(file)};
					prefixes_allowed = async_output_accepts_prefixes_define(early_guard.get_or_construct(owner).ref());
				}
				if (prefixes_allowed && (!ordered_write || prefix_written))
				{
					// Only the issuer's private formatting window may advance ahead of the borrowed prefix.
					if constexpr (lookahead) if (pending) co_await pending;
					if constexpr (transport_start) if (!prefix_written) co_await start_point(output_point<0>);
					auto write = [&]() FAST_IO_ASYNC_THROWS {
						// All prefixes use the same codec state. Only the terminal span may drain an owned trailer.
						if constexpr (layered_write) return emit_decorated<O, false>(early_guard.value, text, text + used);
						else return write_value_prefix<Binding::async_output>(early_guard.value.ref(), text, text + used, issuer, joined);
					}();
					if constexpr (lookahead)
					{
						if (joined)
						{
							pending.start(static_cast<task<void> &&>(write));
							if (pending.await_ready()) co_await pending;
							else if (text != local) text = local;
							else
							{
								// Only a genuinely suspended write needs a second disjoint window.
								text = alternate.get();
								if (!text) text = alternate.allocate_new(capacity + Line);
							}
						}
						else co_await static_cast<task<void> &&>(write);
					}
					else co_await static_cast<task<void> &&>(write);
					prefix_written = true;
				}
			}
			if (!prefixes_allowed || (ordered_write && !prefix_written))
				::fast_io::operations::decay::write_all_decay_dispatch(private_out,text,text+used);
			used=0;
		}
		if constexpr (!PrepareValues)
			if (selected_step && current_loop && (current_loop->root_count>1 || !current_loop->ready.empty()))
				co_await ::fast_io::async::details::yield();
	}
	if constexpr (lookahead) if (pending) co_await pending;
	Char const *first{text},*last{text+used};
	if (overflow.curr_ptr!=overflow.begin_ptr)
	{
		if (used) ::fast_io::operations::decay::write_all_decay_dispatch(private_out,text,text+used);
		first=overflow.begin_ptr;
		last=overflow.curr_ptr;
	}
	// A kernel-only caller owns this first resume. The later consumer owns the file request.
	if constexpr (PrepareValues) co_await ::std::suspend_always{};
	auto &&owner{captured_operand(file)};
	// The first flush and the final suffix share the same normalization and temporary-owner finish.
	auto acquire = [&]() -> decltype(auto) {
		if constexpr (stream_prefixes)
		{
			return early_guard.get_or_construct(owner);
		}
		else return Guard(owner);
	};
	decltype(auto) guard{acquire()};
	if constexpr (layered_write)
	{
		co_await emit_decorated<O>(guard, first, last);
	}
	else
	{
		auto &out{guard.ref()};
		if constexpr (native_output<Ref>) if (last==first && !prefix_written) co_await unused_point(output_point<0>);
		if (last!=first)
		{
			if constexpr (locked_record)
			{
				// The ordinary writer acquires the chain of mutexes, writes the sealed record and unlocks on the same thread.
				auto write = [&]() FAST_IO_ASYNC_THROWS {
					::fast_io::operations::decay::write_all_decay_dispatch(out, first, last);
				};
				if constexpr (Binding::async_output)
				{
					// A private synchronous join is already waiting for this very frame. A submitted
					// record or a point waiter needs the issuer and retains the worker handoff.
					if (current_loop && current_loop->standalone_root == issuer &&
						current_loop->root_count == 1 && current_loop->ready.empty())
						write();
					else co_await invoke_blocking(write);
				}
				else write();
			}
			else if constexpr (Binding::async_output && awaitable_output<Ref>)
			{
				if constexpr (transport_start) if (!prefix_written) co_await start_point(output_point<0>);
				auto *p{reinterpret_cast<::std::byte const *>(first)};
				auto n{static_cast<::std::size_t>(last-first)*sizeof(Char)};
				while (n)
				{
					auto [size,err]{co_await async_write_some_define(out,p,n)};
					if (err.raise || !size || size>n) [[unlikely]]
					{
						if constexpr (!transport_start) co_await failed_point(output_point<0>);
						co_await store_native_error{err.raise ? err : native_error{raise_invalid_progress,0}};
						co_return;
					}
					n-=size; p+=size;
				}
			}
			else
			{
				bool accepted{};
				if constexpr (immediate_output_range<Ref> || immediate_output_reserve<Ref>)
					accepted=try_write_record(out,first,last);
				if (!accepted)
				{
					auto write=[&]() FAST_IO_ASYNC_THROWS { write_observed(out,first,last); };
					if constexpr (!Binding::async_output || immediate_output<Ref>) write();
					else co_await invoke_blocking(write);
				}
			}
		}
		if constexpr (Guard::checked_finish) co_await finish_output_guard(guard);
	}
}

/** @brief Takes all source snapshots at issue, destroying earlier captures if a later provider raises. */
template <typename Char, typename Issue, typename... Args>
inline auto with_captured_fields(Issue issue, Args &&...args) FAST_IO_ASYNC_THROWS
{
	return [&]<::std::size_t... N>(::std::index_sequence<N...>) FAST_IO_ASYNC_THROWS {
		[[maybe_unused]] ::fast_io::containers::tuple<issue_slot<typename capture_field_type<Char,Args>::type>...> slots{};
		(freeze_field<Char>(::fast_io::containers::get<N>(slots), static_cast<Args &&>(args)), ...);
		using values = ::fast_io::containers::tuple<typename capture_field_type<Char,Args>::type...>;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
		return issue(values{static_cast<typename capture_field_type<Char,Args>::type &&>(::fast_io::containers::get<N>(slots).value)...});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
	}(::std::index_sequence_for<Args...>{});
}

/** @brief Captures all fields left-to-right before transferring them to a lazily driven record. */
template <bool Line,typename Binding,bool PrepareValues=false,::std::size_t... Positions,typename O,typename... Args>
inline auto issue_mapped_print_values(::std::index_sequence<Positions...>,O &&out,Args &&...args)
{
	using Char = typename output_ref_type<O>::output_char_type;
	return make_io_operation<PrepareValues>([&]() FAST_IO_ASYNC_THROWS {
		return with_captured_fields<Char>([&](auto &&values) FAST_IO_ASYNC_THROWS {
			return drive_print_values<Line,Binding,PrepareValues,sealed_record_window<Char,Args...>()>(capture<O>{static_cast<O &&>(out)},
				static_cast<decltype(values)>(values), ::std::index_sequence_for<Args...>{}, ::std::index_sequence<Positions...>{},
				::std::integer_sequence<bool,decltype(describe_operand<false,Char,Args>())::observe_field...>{});
		}, static_cast<Args &&>(args)...);
	});
}

/** @brief Uses the argument's public position when no grammar expansion has inserted literal fields. */
template <bool Line,typename Binding,bool PrepareValues=false,typename O,typename... Args>
inline auto issue_print_values(O &&out,Args &&...args)
{
	return issue_mapped_print_values<Line,Binding,PrepareValues>(::std::index_sequence_for<Args...>{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Borrows an ordinary lvalue target or owns a temporary scan manipulator until the record completes. */
template <typename T>
struct synchronous_target : capture<T>
{
	using capture<T>::capture;
};

template <typename Char,typename T,bool=annotated_operand<T> && async_scan_consumer<Char,unannotated_t<T>>>
struct bind_field_type { using type=synchronous_target<unannotated_t<T>>; };
template <typename Char,typename T>
struct bind_field_type<Char,T,true> { using type=scan_binding_type<Char,unannotated_t<T>>; };

/** @brief Freezes the configuration of a selected target at issue, retaining its explicit destination loan. */
template <typename Char,typename T,typename B>
inline void bind_field(issue_slot<B> &slot,T &&target) FAST_IO_ASYNC_THROWS
{
	auto *address{static_cast<void *>(::fast_io::freestanding::addressof(slot.value))};
	if constexpr (annotated_operand<T> && async_scan_consumer<Char,unannotated_t<T>>)
		::new(address) B(async_scan_bind_define(io_reserve_type<Char,::std::remove_cvref_t<unannotated_t<T>>>,
			value_operand(static_cast<T &&>(target))));
	else
		::new(address) B(value_operand(static_cast<T &&>(target)));
	slot.active=true;
}

/** @brief An unmarked field delegates its whole parse to scan and needs no private incremental cursor. */
template <typename C,typename B> struct field_cursor { using type=scan_cursor_type<C,B>; };
template <typename C,typename T> struct field_cursor<C,synchronous_target<T>> { struct type {}; };

/** @brief Advances a bound value on a readable span or at EOF and validates the reported cursor before publication. */
template <typename Char, typename B>
inline value_scan_step<Char const *> advance_bound_scan(scan_cursor_type<Char,B> &state, B &binding,
	Char const *first, Char const *last, bool eof) FAST_IO_ASYNC_THROWS
{
	auto tag{io_reserve_type<Char,B>};
	if (eof)
	{
		auto terminal{async_scan_eof_define(tag,state,binding)};
		if constexpr (requires { terminal.checkpoint; }) return {first, terminal.code, terminal.checkpoint};
		else return {first, terminal, {}};
	}
	auto next{async_scan_step_define(tag,state,binding,first,last)};
	value_checkpoint checkpoint{};
	if constexpr (requires { next.checkpoint; }) checkpoint = next.checkpoint;
	// A checkpoint may advance private parser state while consuming no input.
	if (next.iter < first || next.iter > last ||
		(next.code == parse_code::partial && next.iter == first && !checkpoint.ordinal)) [[unlikely]]
		::fast_io::fast_terminate();
	return {next.iter, next.code, checkpoint};
}

/** @brief Delivers a scanner's terminal parse code through the selected exception ABI. */
[[noreturn]] inline void raise_field_error(::std::uint_least32_t code) FAST_IO_ASYNC_THROWS
{
	::fast_io::details::raise_scan_error(static_cast<parse_code>(code));
}

/** @brief Binds a contiguous fixed-width scanner to its ordinary apply function across short async refills. */
template <typename Path, typename R, typename Field, typename C>
inline task<void> scan_fixed_layer(R &in, Field &field, C *first, ::std::size_t extent, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	for (auto p{first}; p != first + extent;)
	{
		if (!(co_await refill_layer<Path, 0>(in, activity))) ::fast_io::details::raise_scan_error(parse_code::end_of_file);
		auto curr{ibuffer_curr(in)};
		auto n{static_cast<::std::size_t>(ibuffer_end(in) - curr)};
		auto left{static_cast<::std::size_t>(first + extent - p)};
		if (n > left) n = left;
		p = ::fast_io::details::non_overlapped_copy_n(curr, n, p);
		ibuffer_set_curr(in, curr + n);
	}
	if (!::fast_io::details::scan_precise_reserve_apply<C>(first, field))
		::fast_io::details::raise_scan_error(parse_code::end_of_file);
}

/** @brief Reuses a synchronous scanner's exact cursor, progress and EOF contracts with awaitable window acquisition. */
template <typename Path, typename R, typename T>
inline task<void> scan_layer_field(R &in, T &target, file_activity &activity) FAST_IO_ASYNC_THROWS
{
	using C = typename R::input_char_type;
	decltype(auto) field{::fast_io::io_scan_forward<C>(::fast_io::io_scan_alias(target))};
	using F = ::std::remove_cvref_t<decltype(field)>;
	auto tag{io_reserve_type<C, F>};
	if constexpr (::fast_io::precise_reserve_scannable<C, F>)
	{
		constexpr auto n{scan_precise_reserve_size(tag)};
		if constexpr (n <= ::fast_io::details::scan_precise_inline_staging_capacity<C>())
		{
			C units[n ? n : 1];
			co_await scan_fixed_layer<Path>(in, field, units, n, activity);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<C> units(n);
			co_await scan_fixed_layer<Path>(in, field, units.ptr, n, activity);
		}
	}
	else if constexpr (::fast_io::context_scannable<C, F>)
	{
		::fast_io::details::scan_context_state_t<C, F> state{};
		auto begin{ibuffer_curr(in)}, end{ibuffer_end(in)};
		for (;;)
		{
			auto first{ibuffer_curr(in)};
			if (first == end)
			{
				if (!(co_await refill_layer<Path, 0>(in, activity)))
				{
					auto code{scan_context_eof_define(tag, state, field)};
					if (code == parse_code::ok) co_return;
					if (code != parse_code::end_of_file && begin != end)
						::fast_io::details::scan_context_eof_rewind_if_available(in, state, field, begin, end);
					::fast_io::details::raise_scan_error(code == parse_code::partial ? parse_code::invalid : code);
				}
				begin = first = ibuffer_curr(in);
				end = ibuffer_end(in);
				if (first == end) ::fast_io::details::raise_scan_error(parse_code::invalid);
			}
			auto [next, code]{scan_context_define(tag, state, static_cast<C const *>(first), static_cast<C const *>(end), field)};
			if (!::fast_io::details::scan_commit_iterator_if_in_current_chunk(in, first, static_cast<C const *>(first),
				static_cast<C const *>(end), next)) ::fast_io::details::raise_scan_error(parse_code::invalid);
			if (code == parse_code::ok) co_return;
			if (code != parse_code::partial || next == first)
				::fast_io::details::raise_scan_error(code == parse_code::partial ? parse_code::invalid : code);
		}
	}
	else
	{
		// A contiguous-only scanner has the same terminal-window requirement as ordinary scan.
		scan_marked_ref(in, target);
	}
}

/** @brief Borrows the normalized buffer until each ordered field has completed its native input. */
template <typename Path, typename Source, typename... Targets>
inline task<void> scan_native_buffer(capture<Source> source, capture<Targets>... targets) FAST_IO_ASYNC_THROWS
{
	decltype(auto) in{::fast_io::operations::input_stream_ref(captured_operand(source))};
	layer_activity receipt{};
	co_await receipt;
	(co_await scan_layer_field<Path>(in, targets.get(), receipt.activity), ...);
}

/** @brief Parses the current field in a borrowed device window and publishes exactly the consumed prefix. */
template <typename Selection,typename I,typename... Bindings,::std::size_t... K,bool... Marked,bool... Observed>
inline task<void> drive_scan_values(capture<I> file,
	::fast_io::containers::tuple<Bindings...> bindings,::std::index_sequence<K...>,
	::std::integer_sequence<bool,Marked...>,::std::integer_sequence<bool,Observed...>) FAST_IO_ASYNC_THROWS
{
	using Ref=input_ref_type<I>;
	using Char=typename Ref::input_char_type;
	constexpr bool layered_refill{uses_selected_io_layers<true, I, Ref>};
	::fast_io::containers::tuple<typename field_cursor<Char,Bindings>::type...> cursors{};
	auto &&owner{captured_operand(file)};
	decltype(auto) in{::fast_io::operations::input_stream_ref(owner)};
	layer_activity receipt;
	if constexpr (layered_refill) co_await receipt;
	constexpr bool visible[]{Observed...,false};
	// Static storage keeps the marker pack available to nested coroutine instantiations on GCC.
	static constexpr bool adapted[]{Marked...,false};
	constexpr bool all_steps[]{Selection::template value_selection<K>::all..., false};
	bool eof{};
	for (::std::size_t field{};field<sizeof...(Bindings);)
	{
		if constexpr (!(Marked && ...))
		{
			if (!adapted[field])
			{
				if constexpr (layered_refill)
				{
					// Resolve the target before suspending. One await belongs to exactly one field and cursor.
					auto select = [&]<::std::size_t N>(auto &self) FAST_IO_ASYNC_THROWS -> task<void> {
						if constexpr (!adapted[N])
							if (field == N) return scan_layer_field<typename I::layer_path>(in,
								::fast_io::containers::get<N>(bindings).get(), receipt.activity);
						if constexpr (N+1u < sizeof...(Bindings)) return self.template operator()<N+1u>(self);
						else ::fast_io::fast_terminate();
					};
					co_await select.template operator()<0>(select);
					++field;
					continue;
				}
				// Preserve the ordinary scanner's status, aliasing, refill and cursor effects in argument order.
				auto scan_one=[&]<::std::size_t N>() FAST_IO_ASYNC_THROWS {
					if constexpr (!adapted[N])
						scan_marked_ref(in,::fast_io::containers::get<N>(bindings).get());
				};
				auto work=[&]() FAST_IO_ASYNC_THROWS {
					(void)((field==K && (scan_one.template operator()<K>(),true)) || ...);
				};
				if constexpr (Selection::async_input && !immediate_input<Ref>) co_await invoke_blocking(work);
				else work();
				++field;
				continue;
			}
		}
		auto first{ibuffer_curr(in)},last{ibuffer_end(in)};
		if (!eof && first==last)
		{
			auto refill=[&]() FAST_IO_ASYNC_THROWS { return ibuffer_underflow(in); };
			if constexpr (layered_refill) eof = !(co_await refill_layer<typename I::layer_path, 0>(in, receipt.activity));
			else if constexpr (!Selection::async_input || immediate_input<Ref>) eof=!refill();
			else eof=!(co_await invoke_blocking(refill));
			first=ibuffer_curr(in); last=ibuffer_end(in);
			if (!eof && first==last) [[unlikely]] ::fast_io::fast_terminate();
		}
		// Bound CPU work on resident input only when a competing root can use the issuer.
		bool const competing{current_loop && (current_loop->root_count>1 || !current_loop->ready.empty())};
		constexpr ::std::size_t window{(sizeof(::std::size_t)<=2 ? 128u : 4096u)/sizeof(Char)};
		if (!eof && competing && all_steps[field] && static_cast<::std::size_t>(last-first)>window) last=first+window;
		parse_code code{};
		value_checkpoint checkpoint{};
		bool observable{};
		auto advance=[&]<::std::size_t N>() FAST_IO_ASYNC_THROWS {
			if constexpr (adapted[N])
			{
				auto &binding{::fast_io::containers::get<N>(bindings)};
				auto &state{::fast_io::containers::get<N>(cursors)};
				observable=visible[N];
				auto next{advance_bound_scan<Char>(state,binding,first,last,eof)};
				checkpoint = next.checkpoint;
				code = next.code;
				if (!eof) ibuffer_set_curr(in,const_cast<Char *>(next.iter));
			}
		};
		(void)((field==K && (advance.template operator()<K>(),true)) || ...);
		auto const point = observable_point{point_side::argument,field,checkpoint.ordinal};
		if (checkpoint.ordinal && (!checkpoint.starting || Selection::selected(point)))
			co_await publish_point{point,checkpoint.skipped ? 2u : 1u,checkpoint.starting};
		if (competing && ((checkpoint.ordinal && Selection::selected(point)) ||
			(!checkpoint.ordinal && all_steps[field] && code==parse_code::partial && !eof)))
			co_await ::fast_io::async::details::yield();
		if (code==parse_code::partial && (!eof || checkpoint.ordinal)) continue;
		if (code==parse_code::ok)
		{
			auto commit=[&]<::std::size_t N>() FAST_IO_ASYNC_THROWS {
				if constexpr (adapted[N])
				{
					auto &binding{::fast_io::containers::get<N>(bindings)};
					async_scan_commit_define(io_reserve_type<Char,::std::remove_cvref_t<decltype(binding)>>,
						::fast_io::containers::get<N>(cursors),binding);
				}
			};
			(void)((field==K && (commit.template operator()<K>(),true)) || ...);
		}
		if (code!=parse_code::ok)
		{
			if (observable)
			{
				if (code==parse_code::end_of_file) co_await unused_point({point_side::argument,field});
				else co_await failed_point({point_side::argument,field});
			}
			// EOF skips the suffix. No later field can acquire or commit its target.
			if (code==parse_code::end_of_file) for (auto next{field+1};next<sizeof...(Bindings);++next)
				if (visible[next]) co_await unused_point({point_side::argument,next});
			co_await store_native_error{{raise_field_error,static_cast<unsigned>(code==parse_code::partial ? parse_code::invalid : code)}};
			co_return;
		}
		if (observable)
		{
			auto const released = observable_point{point_side::argument,field};
			co_await complete_point(released);
			if (competing && field+1 != sizeof...(Bindings) && Selection::selected(released))
				co_await ::fast_io::async::details::yield();
		}
		++field;
	}
	co_return;
}

/** @brief Fixes each target and its configuration at issue. Repeated targets retain their original commit order. */
template <typename Selection,typename I,typename... Args>
inline auto issue_scan_values(I &&in,Args &&...args)
{
	using Char=typename input_ref_type<I>::input_char_type;
	return make_io_operation([&]() FAST_IO_ASYNC_THROWS {
		return [&]<::std::size_t... N>(::std::index_sequence<N...>) FAST_IO_ASYNC_THROWS {
			::fast_io::containers::tuple<issue_slot<typename bind_field_type<Char,Args>::type>...> slots{};
			(bind_field<Char>(::fast_io::containers::get<N>(slots),static_cast<Args &&>(args)),...);
			using bindings=::fast_io::containers::tuple<typename bind_field_type<Char,Args>::type...>;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
			return drive_scan_values<Selection>(capture<I>{static_cast<I &&>(in)},
				bindings{static_cast<typename bind_field_type<Char,Args>::type &&>(::fast_io::containers::get<N>(slots).value)...},
				::std::index_sequence<N...>{},::std::integer_sequence<bool,(annotated_operand<Args> && async_scan_consumer<Char,unannotated_t<Args>>)...>{},
				::std::integer_sequence<bool,decltype(describe_operand<true,Char,Args>())::observe_field...>{});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
		}(::std::index_sequence_for<Args...>{});
	});
}
} // namespace fast_io::async::details
