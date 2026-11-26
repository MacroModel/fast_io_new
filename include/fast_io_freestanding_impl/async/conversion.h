#pragma once
#include "conversion_storage.h"

namespace fast_io::async::details
{
/** @brief Binds a conversion's value providers without assigning a file point to its private character storage. */
template <typename... D>
struct conversion_points
{
	using base = with_value_points<operation_points<algorithm::convert,no_file_points,no_file_points>,D::observe_field...>;
	using type = typename compose_value_points<base,D...>::type;
};
/** @brief Names the observable graph after composing all public source and destination providers. */
template <typename... D>
using conversion_graph = typename conversion_points<D...>::type;

/** @brief Retains the public argument positions, including position zero for an existing destination. */
template <typename... A>
using conversion_request = operation_request<no_point_selection,no_point_selection,selection_of_t<A>...>;

/** @brief Checks a marked source in the conversion's actual character domain or an ordinary source at its capture boundary. */
template <typename C, typename A>
concept conversion_source = (annotated_operand<A> && async_printable<C,unannotated_t<A>>) ||
	(!annotated_operand<A> && !::fast_io::details::has_raw_print_arg<A> &&
	::fast_io::operations::defines::print_freestanding_okay_for_line<false,typename print_record<C>::base &,
		decltype(text_operand<C>(::std::declval<A>()))>);

/** @brief Selects the value-returning overload only when both the source captures and the final scanner exist. */
template <typename C, typename T, typename... A>
concept returning_conversion = (annotated_operand<A> || ...) && (conversion_source<C,A> && ...) &&
	::std::is_default_constructible_v<T> && ::std::is_move_constructible_v<T> &&
	::fast_io::details::can_do_inplace_to<C,T,basic_io_scatter_t<C>> &&
	binds_points<conversion_request<A...>,conversion_graph<decltype(describe_operand<false,C,A>())...>>;

/** @brief An ordinary target keeps its scanner; a marked target must provide a binding or explicitly permit borrowing. */
template <typename C, typename T, typename... A>
concept inplace_conversion = (annotated_operand<T> || (annotated_operand<A> || ...)) && (conversion_source<C,A> && ...) &&
	((annotated_operand<T> && async_scan_consumer<C,unannotated_t<T>>) ||
		((!annotated_operand<T> || async_scan_reusable<C,unannotated_t<T>>) &&
		capturable<unannotated_t<T>> && ::fast_io::details::can_do_inplace_to<C,unannotated_t<T>,basic_io_scatter_t<C>>)) &&
	binds_points<conversion_request<T,A...>,conversion_graph<decltype(describe_operand<true,C,T>()),decltype(describe_operand<false,C,A>())...>>;

/** @brief Requests whole-string materialization, with the optional line ending supplied by the concat protocol. */
template <bool Line>
struct concat_destination { static constexpr bool append_line{Line}; };

/** @brief Continues inline or passes control to the observer that borrowed this conversion. */
struct conversion_transfer
{
	::std::coroutine_handle<> observer{};
	bool await_ready() const noexcept { return !observer; }
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<>) const noexcept { return observer; }
	constexpr void await_resume() const noexcept {}
};

/** @brief Records a checked fact inline unless there is a waiter to notify.
 * @details A fact remains queryable through the result's lifetime.
 * An independently queued observer already has its own continuation. Only a private observer
 * returns a handle for a symmetric transfer.
 */
template <typename Graph, typename Owner>
struct conversion_publication
{
	Owner &owner;
	observable_point point;
	unsigned value;
	bool starting{};

	/** @brief Validates the provider and records the transition before making it visible. */
	FAST_IO_GNU_ALWAYS_INLINE void record() const noexcept
	{
		auto slot{operation_publications<Graph>::slot(owner.history,point,value,starting)};
		if (slot == static_cast<::std::size_t>(-1) || (point.local && owner.history.lookup(slot))) [[unlikely]]
			::fast_io::fast_terminate();
		owner.history.record(slot,value);
	}
	/** @brief Records before queuing independent observers or transferring to a private waiter. */
	FAST_IO_GNU_ALWAYS_INLINE conversion_transfer operator co_await() const noexcept
		requires requires { owner.observers; }
	{
		record();
		if (owner.observers)
			return {notify_observers(owner.observers,owner.history,false)};
		return {};
	}
};

/** @brief Snapshots a plain scalar whose library-owned formatter has no issue-time effects.
 * @details The fixed-source predicate admits non-volatile built-in integers and proves a reserve bound.
 * Copying the value fixes what will be formatted, even if the caller modifies its original before the join.
 */
template <typename T, ::std::size_t Bound>
struct conversion_scalar
{
	T value;
	static constexpr auto capacity{Bound};
};
template <typename T> inline constexpr bool is_conversion_scalar = false;
template <typename T, ::std::size_t N> inline constexpr bool is_conversion_scalar<conversion_scalar<T,N>> = true;

/** @brief Selects a value snapshot for plain bounded integers and the normal capture protocol for other sources. */
template <typename C, typename T>
struct conversion_capture : capture_field_type<C,T> {};
template <typename C, typename T>
	requires (!annotated_operand<T>) && ::std::integral<::std::remove_cvref_t<T>> &&
		(::fast_io::details::decay::print_fixed_public_source_available<C,T>()) &&
		(record_argument_bound<C,T>() > 0 && record_argument_bound<C,T>() <= inline_record_capacity)
struct conversion_capture<C,T>
{
	using type = conversion_scalar<::std::remove_cvref_t<T>,record_argument_bound<C,T>()>;
};

/** @brief Allocates provider cursor state only for operands that can actually publish a checkpoint. */
template <typename C, typename Saved>
struct conversion_print_cursor : print_field_cursor<C,Saved> {};
template <typename C, typename T, ::std::size_t N>
struct conversion_print_cursor<C,conversion_scalar<T,N>> { struct type {}; };

/** @brief Composes the maximum for a complete conversion spelling from its individual source contracts.
 * @details A captured step producer explicitly declares its total bound. Plain library-owned scalar and
 * character-array bounds already cover the actual ordinary spelling. All other shapes use a dynamic extent.
 */
template <typename C, typename A>
consteval ::std::size_t conversion_source_maximum() noexcept
{
	using saved = typename conversion_capture<C,A>::type;
	if constexpr (is_conversion_scalar<saved>)
	{
		return saved::capacity;
	}
	else if constexpr (annotated_operand<A> && async_print_producer<C,unannotated_t<A>>)
	{
		if constexpr (async_print_statically_bounded<C,saved>)
		{
			return async_print_max_size_define(io_reserve_type<C,saved>);
		}
	}
	else if constexpr (::std::is_bounded_array_v<::std::remove_reference_t<A>> &&
		::std::same_as<::std::remove_extent_t<::std::remove_reference_t<A>>, C const>)
	{
		// The same-domain array alias prints precisely N-1 units, including embedded nulls.
		return ::std::extent_v<::std::remove_reference_t<A>> - 1u;
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<A>, ::fast_io::io_null_t>)
	{
		return 0u;
	}
	else if constexpr (::fast_io::details::decay::print_static_scatter_traits<C, ::std::remove_cvref_t<A>>::available)
	{
		// Lowered literal nodes preserve their actual extent after format-string escapes are resolved.
		return ::fast_io::details::decay::print_static_scatter_traits<C, ::std::remove_cvref_t<A>>::size;
	}
	else if constexpr (::fast_io::details::decay::print_bounded_cstr_scatter_traits<::std::remove_cvref_t<A>>::available &&
		::fast_io::bounded_to_scatter_source<C, A>)
	{
		// This library carrier freezes exactly its descriptor's range in the matching character domain.
		return to_scatter_max_size(io_reserve_type<C, ::std::remove_cvref_t<A>>);
	}
	return static_cast<::std::size_t>(-1);
}

/** @brief Computes a complete spelling bound only for a synchronous scanner consuming a captured conversion.
 * @details A concat result and an explicitly marked destination have their own commit protocols.
 */
template <typename C, bool Needed, typename... A>
consteval ::std::size_t conversion_spelling_maximum() noexcept
{
	if constexpr (Needed)
	{
		return ::fast_io::details::to_spelling_maximum<conversion_source_maximum<C, A>()...>();
	}
	else
	{
		return SIZE_MAX;
	}
}

/** @brief Includes producer and scanner cursor alignment even when a coroutine ABI reports only its frame size. */
template <typename C, bool ScanTarget, typename Destination, typename... Saved>
consteval ::std::size_t conversion_frame_alignment() noexcept
{
	using cursors = ::fast_io::containers::tuple<typename conversion_print_cursor<C,Saved>::type...>;
	constexpr ::std::size_t alignments[]{alignof(Destination),alignof(::fast_io::containers::tuple<Saved...>),
		alignof(cursors),[] { if constexpr (ScanTarget) return alignof(scan_cursor_type<C,Destination>); else return alignof(C); }()};
	::std::size_t n{1};
	for (auto a : alignments) if (a > n) n = a;
	return n;
}

/** @brief Stores the complete spelling in a proved scalar reserve, including across a suspension. */
template <typename C, ::std::size_t Capacity>
struct scalar_conversion_record
{
	C units[Capacity ? Capacity : 1u];
	C *begin_ptr{units}, *curr_ptr{units};
};

/** @brief Advances a single source into the private character record and returns its provider checkpoint.
 * @details Frozen fragments have no provider state. An opted-in formatter receives a bounded writable
 * span and may publish a start without emitting any characters. The same validation covers both drivers.
 * Fits proves that the sum of the complete reserve budgets of every saved scalar fits the initial put
 * area in C units. Earlier scalars consume at most their own budgets, so each next writer has its
 * entire declared reservation available. Sources with a different emission protocol need their own proof.
 */
template <typename C, bool Fits, typename Record, typename Cursor, typename Saved>
FAST_IO_GNU_ALWAYS_INLINE inline value_print_step<C *> conversion_field_step(Record &text, Cursor &cursor, Saved &saved) FAST_IO_ASYNC_THROWS
{
	if constexpr (::std::same_as<Saved,print_record<C>>)
	{
		auto out{::fast_io::operations::output_stream_ref(text)};
		::fast_io::operations::decay::write_all_decay_dispatch(out,saved.begin(),saved.end());
		return {text.curr_ptr,true,{}};
	}
	else if constexpr (is_conversion_scalar<Saved>)
	{
		if constexpr (Fits)
			text.curr_ptr = emit_reserved_snapshot(text.curr_ptr,saved.value);
		else if (static_cast<::std::size_t>(text.end_ptr-text.curr_ptr) >= Saved::capacity) [[likely]]
			text.curr_ptr = emit_reserved_snapshot(text.curr_ptr,saved.value);
		else
		{
			// The actual spelling can still fit when the worst-case reservation does not.
			C units[Saved::capacity];
			auto *last{emit_reserved_snapshot(units,saved.value)};
			auto out{::fast_io::operations::output_stream_ref(text)};
			::fast_io::operations::decay::write_all_decay_dispatch(out,units,last);
		}
		return {text.curr_ptr,true,{}};
	}
	else
	{
		if (text.curr_ptr == text.end_ptr) output_stream_buffer_flush_define(::fast_io::operations::output_stream_ref(text));
		constexpr ::std::size_t window{(sizeof(::std::size_t) <= 2 ? 128u : 4096u) / sizeof(C)};
		auto *last{text.end_ptr};
		if (static_cast<::std::size_t>(last - text.curr_ptr) > window) last = text.curr_ptr + window;
		auto step{generate_field(cursor,saved,text.curr_ptr,last)};
		value_checkpoint checkpoint{};
		if constexpr (requires { step.checkpoint; }) checkpoint = step.checkpoint;
		::fast_io::details::decay::validate_context_print_result(text.curr_ptr,last,step.iter,step.done || checkpoint.ordinal);
		text.curr_ptr = step.iter;
		return {step.iter,step.done,checkpoint};
	}
}

/** @brief Caps a long scan window when the active issuer has peers to schedule. */
template <typename C>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline C const *conversion_scan_window_end(C const *first, C const *end, ::std::size_t window) noexcept
{
	return has_layer_peer() ? first + window : end;
}

/** @brief Formats directly into private growable storage and consumes its contiguous prefix with the selected scanner.
 * @details A single root owns every cursor, publication, byte and final result. A parked start observer can
 * resume the same frame before the producer finishes. Ordinary source values are fixed at issue. The internal
 * buffer survives suspension and supplies no file observation. Each bound target commits only on parse success.
 * The exclusive driver borrows its enclosing plan through both non-suspending endpoints. The independent
 * driver owns the plan members because an early subscriber can outlive the call that creates its frame.
 */
template <typename C, typename Result, typename Binding, bool ScanTarget, ::std::size_t Maximum, typename Destination, typename... Saved,
	typename Storage, ::std::size_t... I, ::std::size_t... Position>
inline auto drive_conversion(Storage &, frame_alignment_hint<conversion_frame_alignment<C,ScanTarget,Destination,Saved...>()>,
	::std::conditional_t<::std::same_as<::std::remove_cv_t<Storage>,independent_conversion_frame>,Destination,Destination &> destination,
	::std::conditional_t<::std::same_as<::std::remove_cv_t<Storage>,independent_conversion_frame>,::fast_io::containers::tuple<Saved...>,::fast_io::containers::tuple<Saved...> &> values,
	::std::index_sequence<I...>, ::std::index_sequence<Position...>) FAST_IO_ASYNC_THROWS
	-> ::std::conditional_t<::std::same_as<::std::remove_cv_t<Storage>,independent_conversion_frame>,task<Result>,
		scoped_conversion_task<Result,conversion_frame_alignment<C,ScanTarget,Destination,Saved...>()>>
{
	constexpr bool independent{::std::same_as<::std::remove_cv_t<Storage>,independent_conversion_frame>};
	using graph = typename Binding::graph_type;
	// Both promises return an lvalue. Name its type before evaluating the yield on older coroutine frontends.
	using issuer_type = ::std::conditional_t<independent, publication_frame, Storage>;
	issuer_type &issuer = co_yield conversion_issuer{};
	using publication = conversion_publication<observed_graph<graph,Binding>,issuer_type>;
	if constexpr (sizeof...(Saved) == 0 && ::std::is_void_v<Result>)
	{
		// An empty format has no components. Keep the destination untouched and release its binding.
		if constexpr (graph::index(argument_point<0>) != graph::count) co_await publication{issuer,argument_point<0>,2};
		co_return;
	}
	// When known, the complete reserve also bounds the readable span presented to a marked scanner.
	constexpr auto capacity = [] {
		if constexpr ((is_conversion_scalar<Saved> && ...))
			return ::fast_io::details::to_spelling_maximum<Saved::capacity...>();
		else
			return SIZE_MAX;
	}();
	constexpr bool fits{capacity <= inline_record_capacity};
	::std::conditional_t<fits, scalar_conversion_record<C,capacity>, typename print_record<C>::base> text;
	::fast_io::containers::tuple<typename conversion_print_cursor<C,Saved>::type...> cursors{};
	if constexpr (!independent)
	{
		// This exclusive join cannot suspend. Keep each position constant through its formatter and publications.
		[[maybe_unused]] auto finish = [&]<::std::size_t N, ::std::size_t P>() FAST_IO_ASYNC_THROWS {
			auto &saved{::fast_io::containers::get<N>(values)};
			using field_type = ::std::remove_cvref_t<decltype(saved)>;
			if constexpr (::std::same_as<field_type,print_record<C>> || is_conversion_scalar<field_type>)
			{
				// A frozen fragment, including a format literal, has no public argument slot.
				conversion_field_step<C,fits>(text,::fast_io::containers::get<N>(cursors),saved);
			}
			else
			{
				for (;;)
				{
					auto step{conversion_field_step<C,fits>(text,::fast_io::containers::get<N>(cursors),saved)};
					auto event{step.checkpoint};
					observable_point p{point_side::argument,P,event.ordinal};
					if (event.ordinal && (!event.starting || Binding::selected(p)))
						publication{issuer,p,event.skipped ? 2u : 1u,event.starting}.record();
					if (step.done) break;
				}
				if constexpr (graph::index(argument_point<P>) != graph::count)
					publication{issuer,argument_point<P>,1u}.record();
			}
		};
		(finish.template operator()<I,Position>(),...);
	}
	else if constexpr (sizeof...(Saved) <= 4 &&
		(0u + ... + static_cast<unsigned>(!::std::same_as<Saved,print_record<C>> && !is_conversion_scalar<Saved>)) <= 1u)
	{
		// Frozen fragments cannot suspend. Keep the sole provider's source occurrence constant across activations.
		constexpr ::std::size_t producer = [] {
			::std::size_t index{sizeof...(Saved)};
			((!::std::same_as<Saved,print_record<C>> && !is_conversion_scalar<Saved> ? index = I : index), ...);
			return index;
		}();
		auto finish = [&]<::std::size_t N>() FAST_IO_ASYNC_THROWS {
			if constexpr (N != producer)
				conversion_field_step<C,fits>(text,::fast_io::containers::get<N>(cursors),::fast_io::containers::get<N>(values));
		};
		([&]() FAST_IO_ASYNC_THROWS { if constexpr (I < producer) finish.template operator()<I>(); }(), ...);
		if constexpr (producer != sizeof...(Saved))
		{
			constexpr ::std::size_t positions[]{Position...,0};
			constexpr auto position{positions[producer]};
			for (;;)
			{
				auto step{conversion_field_step<C,fits>(text,::fast_io::containers::get<producer>(cursors),::fast_io::containers::get<producer>(values))};
				auto checkpoint{step.checkpoint};
				bool selected{Binding::template value_selection<position>::all ||
					(checkpoint.ordinal && Binding::selected({point_side::argument,position,checkpoint.ordinal})) ||
					(step.done && Binding::selected(argument_point<position>))};
				observable_point p{point_side::argument,position,checkpoint.ordinal};
				if (checkpoint.ordinal && (!checkpoint.starting || Binding::selected(p)))
					co_await publication{issuer,p,checkpoint.skipped ? 2u : 1u,checkpoint.starting};
				if (step.done)
				{
					if constexpr (graph::index(argument_point<position>) != graph::count)
						co_await publication{issuer,argument_point<position>,1};
				}
				if (selected && has_layer_peer()) co_await yield();
				if (step.done) break;
			}
		}
		([&]() FAST_IO_ASYNC_THROWS { if constexpr (I > producer) finish.template operator()<I>(); }(), ...);
	}
	else
	{
		// The independent frame must retain which field to resume while other roots or subscribers run.
		for (::std::size_t field{}; field != sizeof...(Saved); )
		{
			value_checkpoint checkpoint{};
			bool done{}, observable{}, selected{};
			constexpr ::std::size_t positions[]{Position...,0};
			[[maybe_unused]] auto advance = [&]<::std::size_t N, ::std::size_t P>() FAST_IO_ASYNC_THROWS {
				auto &saved{::fast_io::containers::get<N>(values)};
				using field_type = ::std::remove_cvref_t<decltype(saved)>;
				auto step{conversion_field_step<C,fits>(text,::fast_io::containers::get<N>(cursors),saved)};
				checkpoint = step.checkpoint;
				done = step.done;
				if constexpr (!::std::same_as<field_type,print_record<C>> && !is_conversion_scalar<field_type>)
				{
					observable = graph::index(argument_point<P>) != graph::count;
					selected = Binding::template value_selection<P>::all ||
						(checkpoint.ordinal && Binding::selected({point_side::argument,P,checkpoint.ordinal})) ||
						(done && Binding::selected(argument_point<P>));
				}
			};
			(void)((field == I && (advance.template operator()<I,Position>(),true)) || ...);
			auto position{positions[field]};
			observable_point p{point_side::argument,position,checkpoint.ordinal};
			if (checkpoint.ordinal && (!checkpoint.starting || Binding::selected(p)))
				co_await publication{issuer,p,checkpoint.skipped ? 2u : 1u,checkpoint.starting};
			if (done)
			{
				if (observable) co_await publication{issuer,{point_side::argument,position},1};
				++field;
			}
			if (selected && has_layer_peer()) co_await yield();
		}
	}
	basic_io_scatter_t<C> spelling{text.begin_ptr,static_cast<::std::size_t>(text.curr_ptr-text.begin_ptr)};
	if constexpr (requires { Destination::append_line; })
	{
		if constexpr (::std::is_nothrow_move_constructible_v<Result> && requires {
			{ strlike_concat_elide_result_move(io_strlike_type<C,Result>) } noexcept -> ::std::same_as<::std::true_type>;
		})
		{
			co_return task_value_factory{[&]() FAST_IO_ASYNC_THROWS {
				return ::fast_io::basic_general_concat<Destination::append_line,C,Result>(spelling);
			}};
		}
		else
			co_return ::fast_io::basic_general_concat<Destination::append_line,C,Result>(spelling);
	}
	else if constexpr (!::std::is_void_v<Result>)
	{
		co_return ::fast_io::basic_to<C, Result>(::fast_io::details::to_materialized_source<Result, Maximum>(spelling));
	}
	else if constexpr (!ScanTarget)
	{
		auto &target{destination.get()};
		::fast_io::basic_inplace_to<C>(target, ::fast_io::details::to_materialized_source<decltype(target), Maximum>(spelling));
	}
	else
	{
		constexpr ::std::size_t window{(sizeof(::std::size_t) <= 2 ? 128u : 4096u) / sizeof(C)};
		scan_cursor_type<C,Destination> cursor{};
		C const *first{spelling.base}, *end{first + spelling.len};
		constexpr bool observable{graph::index(argument_point<0>) != graph::count};
		for (;;)
		{
			bool const eof{first == end};
			auto step{[&]() FAST_IO_ASYNC_THROWS {
				if (eof)
				{
					return advance_bound_scan<C>(cursor,destination,first,first,true);
				}
				auto *last{end};
				if constexpr (Binding::template value_selection<0>::all && capacity > window)
				{
					if (static_cast<::std::size_t>(end-first) > window) [[unlikely]]
					{
						if constexpr (independent)
						{
							if (has_layer_peer()) last = first + window;
						}
						else
						{
							last = conversion_scan_window_end(first,end,window);
						}
					}
				}
				return advance_bound_scan<C>(cursor,destination,first,last,false);
			}()};
			first = step.iter;
			auto event{step.checkpoint};
			observable_point p{point_side::argument,0,event.ordinal};
			if (event.ordinal && (!event.starting || Binding::selected(p)))
				co_await publication{issuer,p,event.skipped ? 2u : 1u,event.starting};
			if (step.code == parse_code::partial)
			{
				if (!eof || event.ordinal)
				{
					if constexpr (independent)
						if ((Binding::template value_selection<0>::all || Binding::selected(p)) && has_layer_peer()) co_await yield();
					continue;
				}
				step.code = parse_code::invalid;
			}
			if (step.code != parse_code::ok)
			{
				if constexpr (observable)
				{
					if (step.code == parse_code::end_of_file) co_await publication{issuer,argument_point<0>,2};
					else co_await publication{issuer,argument_point<0>,3};
				}
				co_await store_native_error{{raise_field_error,static_cast<unsigned>(step.code)}};
				co_return;
			}
			async_scan_commit_define(io_reserve_type<C,Destination>,cursor,destination);
			if constexpr (observable) co_await publication{issuer,argument_point<0>,1};
			co_return;
		}
	}
}

/** @brief Owns all issue-time snapshots. Only ordinary C++ objects move before a driver starts. */
template <typename C, typename T, typename Binding, bool ScanTarget, typename D, typename Positions, typename Values, ::std::size_t Maximum>
struct conversion_plan;

template <typename C, typename T, typename Binding, bool ScanTarget, typename D, typename Positions, ::std::size_t Maximum, typename... Saved>
struct conversion_plan<C,T,Binding,ScanTarget,D,Positions,::fast_io::containers::tuple<Saved...>,Maximum>
{
	D destination;
	::fast_io::containers::tuple<Saved...> values;
	/** @brief Screens named spill types before permitting a compiler to inline a non-suspending coroutine.
	 * @details This also bounds the caller-owned result slot. The leading allocator handles the actual
	 * size of any non-elided frame, including padding and compiler-specific spills.
	 */
	static constexpr bool bounded = [] {
		using scanner = typename decltype([] {
			if constexpr (ScanTarget) return io_type<scan_cursor_type<C,D>>;
			else return io_type<char>;
		}())::type;
		struct working_set
		{
			task_value<T> result;
			typename print_record<C>::base text;
			::fast_io::containers::tuple<typename conversion_print_cursor<C,Saved>::type...> cursors;
			scanner target_cursor;
			D destination;
			::fast_io::containers::tuple<Saved...> values;
		};
		return ::fast_io::details::context_state_inline_v<working_set>;
	}();

	/** @brief Instantiates the same conversion body for the selected ownership and suspension contract. */
	template <typename Storage>
	auto operator()(Storage &storage) FAST_IO_ASYNC_THROWS
	{
		using hint = frame_alignment_hint<conversion_frame_alignment<C,ScanTarget,D,Saved...>()>;
		if constexpr (::std::same_as<::std::remove_cv_t<Storage>,independent_conversion_frame>)
			return drive_conversion<C,T,Binding,ScanTarget,Maximum,D,Saved...>(storage,hint{},static_cast<D &&>(destination),
				static_cast<decltype(values) &&>(values),::std::index_sequence_for<Saved...>{},Positions{});
		else
			return drive_conversion<C,T,Binding,ScanTarget,Maximum,D,Saved...>(storage,hint{},destination,values,
				::std::index_sequence_for<Saved...>{},Positions{});
	}
};

/** @brief Constructs a source directly in its final conversion tuple, left to right at issue time. */
template <typename C, typename T>
inline typename conversion_capture<C,T>::type capture_conversion_source(T &&value) FAST_IO_ASYNC_THROWS
{
	using saved = typename conversion_capture<C,T>::type;
	if constexpr (is_conversion_scalar<saved>)
		return saved{value};
	else if constexpr (annotated_operand<T> && async_print_producer<C,unannotated_t<T>>)
		return async_print_capture_define(io_reserve_type<C,::std::remove_cvref_t<unannotated_t<T>>>,value_operand(static_cast<T &&>(value)));
	else
	{
		saved result;
		freeze_print_record(result,static_cast<T &&>(value));
		return result;
	}
}

/** @brief Captures at issue, deferring the frame's storage decision until the operation's lifetime is known. */
template <typename C, typename Result, typename Graph, typename Request, typename T, typename Lower>
inline auto issue_conversion_with(T &&target, Lower lower)
{
	using binding = bound_points<Request,Graph>;
	using observed = observed_graph<Graph,binding>;
	auto prepare = [&]() FAST_IO_ASYNC_THROWS {
		auto start = [&](auto &&destination) FAST_IO_ASYNC_THROWS {
			return lower([&]<typename Positions, typename... A>(Positions, A &&...args) FAST_IO_ASYNC_THROWS {
				using values = ::fast_io::containers::tuple<typename conversion_capture<C,A>::type...>;
				using destination_type = ::std::remove_cvref_t<decltype(destination)>;
				constexpr bool scan_target{annotated_operand<T> && async_scan_consumer<C,unannotated_t<T>>};
				constexpr bool needs_bound{!scan_target && !requires { destination_type::append_line; }};
				using plan = conversion_plan<C,Result,binding,scan_target,destination_type,Positions,values,
					conversion_spelling_maximum<C, needs_bound, A...>()>;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
				return plan{static_cast<decltype(destination)>(destination),{capture_conversion_source<C>(static_cast<A &&>(args))...}};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
			});
		};
		if constexpr (::std::is_void_v<Result>)
		{
			using destination = typename bind_field_type<C,T>::type;
			issue_slot<destination> slot;
			bind_field<C>(slot,static_cast<T &&>(target));
			return start(static_cast<destination &&>(slot.value));
		}
		else return start(target);
	};
	using plan = decltype(prepare());
	using work = conversion_work<Result,observed,plan>;
	auto factory = [&] {
		if constexpr (plan::bounded && ::fast_io::details::context_state_inline_v<work> && ::std::is_nothrow_move_constructible_v<plan>)
		{
			resident_operation<Result,work> job{::std::in_place};
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
			try
#endif
			{ job.staging().stage(prepare); }
#if defined(__HERBCEPTIONS__)
			catch throws(::std::error failure) { job.creation_error(failure); }
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			catch (...) { job.creation_exception(); }
#endif
			return job;
		}
		else return make_io_operation<false,true>([&]() FAST_IO_ASYNC_THROWS { return prepare()(independent_conversion); });
	};
	return async_operation<observed,decltype(factory())>{io_type<binding>,factory};
}

/** @brief Passes a plain conversion pack directly to the common issue-time capture boundary. */
template <typename C, typename Result, typename Graph, typename Request, typename T, typename Positions, typename... A>
inline auto issue_conversion(T &&target, Positions positions, A &&...args)
{
	return issue_conversion_with<C,Result,Graph,Request>(static_cast<T &&>(target),[&](auto issue) FAST_IO_ASYNC_THROWS {
		return issue(positions,static_cast<A &&>(args)...);
	});
}

/** @brief Captures marked sources for a new result, numbering sources from zero.
 * @details The caller supplies a result type. Each observable field belongs to a source occurrence.
 * During execution the operation owns the storage for the result. Successful join transfers the value
 * to the caller. An intermediate publication can neither expose the result nor end a destination loan.
 */
template <typename C, typename Result, typename... A>
inline auto make_conversion(A &&...args)
{
	using graph = conversion_graph<decltype(describe_operand<false,C,A>())...>;
	return issue_conversion<C,Result,graph,conversion_request<A...>>(io_type<Result>,::std::index_sequence_for<A...>{},static_cast<A &&>(args)...);
}

/** @brief Preserves position zero for the existing destination and shifts only the public source indices.
 * @details Binding a writable reference keeps the caller's object identity through the operation.
 * With only marked sources its ordinary scanner receives the spelling. With a marked scan consumer the
 * driver keeps a private cursor and calls commit after parse success. A final join ends the borrow.
 */
template <typename C, typename T, typename... A>
inline auto make_inplace_conversion(T &&target,A &&...args)
{
	using graph = conversion_graph<decltype(describe_operand<true,C,T>()),decltype(describe_operand<false,C,A>())...>;
	return [&]<::std::size_t... I>(::std::index_sequence<I...>) {
		return issue_conversion<C,void,graph,conversion_request<T,A...>>(
			static_cast<T &&>(target),::std::index_sequence<(I+1u)...>{},static_cast<A &&>(args)...);
	}(::std::index_sequence_for<A...>{});
}
} // namespace fast_io::async::details

namespace fast_io::async
{
/** @brief Constructs a string from complete code units, including whitespace and embedded zeroes.
 * @details Only marked source fields publish progress. The private string materializer has no file points.
 */
template <typename C, typename T, bool Line, typename... A>
	requires ::fast_io::strlike<C,T> && (annotated_operand<A> || ...) && (details::conversion_source<C,A> && ...) &&
		details::binds_points<details::conversion_request<A...>,details::conversion_graph<decltype(details::describe_operand<false,C,A>())...>>
inline auto async_concat_define(io_reserve_type_t<C,T>,::std::bool_constant<Line>,A &&...args)
{
	using graph = details::conversion_graph<decltype(details::describe_operand<false,C,A>())...>;
	return details::issue_conversion<C,T,graph,details::conversion_request<A...>>(details::concat_destination<Line>{},
		::std::index_sequence_for<A...>{},static_cast<A &&>(args)...);
}
} // namespace fast_io::async

namespace fast_io
{
inline namespace io
{
/** @brief Creates a conversion operation from one or more marked sources in the requested character domain.
 * @return An operation whose successful final join delivers an independently usable T by value.
 * @details The sources supply the observable points. The result cannot retain a borrow into a private
 * spelling that expires with the operation. A scanner that wants to refer to external storage must
 * arrange the lifetime of that storage independently.
 */
template <::std::integral C, typename T, typename... A>
	requires async::details::returning_conversion<C,T,A...>
[[nodiscard]] inline auto basic_to(A &&...args)
{
	return async::details::make_conversion<C,T>(static_cast<A &&>(args)...);
}

/** @brief Creates an in-place conversion with a marked target, a marked source or both.
 * @return An operation whose final join returns void on success and delivers a conversion failure
 * through the configured exception ABI.
 * @pre Keep borrowed destination storage valid and exclusively available through the final join,
 * unless the provider explicitly releases its last use earlier. A temporary binding may own a
 * manipulator while borrowing the object to which the manipulator refers.
 * @details An ordinary target uses its existing scanner. A selected incremental target commits
 * once after parse success. A start observer can resume before either target has received a value.
 */
template <::std::integral C, typename T, typename... A>
	requires async::details::inplace_conversion<C,T,A...>
inline auto basic_inplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<C>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Constructs the result from marked and ordinary char source fragments. */
template <typename T, typename... A>
	requires async::details::returning_conversion<char,T,A...>
[[nodiscard]] inline auto to(A &&...args)
{
	return async::details::make_conversion<char,T>(static_cast<A &&>(args)...);
}

/** @brief Converts char source fragments into the bound destination at its public argument position. */
template <typename T, typename... A>
	requires async::details::inplace_conversion<char,T,A...>
inline auto inplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<char>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Constructs the result from marked and ordinary wchar_t source fragments. */
template <typename T, typename... A>
	requires async::details::returning_conversion<wchar_t,T,A...>
[[nodiscard]] inline auto wto(A &&...args)
{
	return async::details::make_conversion<wchar_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts wchar_t source fragments into the bound destination at its public argument position. */
template <typename T, typename... A>
	requires async::details::inplace_conversion<wchar_t,T,A...>
inline auto winplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<wchar_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Constructs the result from marked and ordinary char8_t source fragments. */
template <typename T, typename... A>
	requires async::details::returning_conversion<char8_t,T,A...>
[[nodiscard]] inline auto u8to(A &&...args)
{
	return async::details::make_conversion<char8_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts char8_t source fragments into the bound destination at its public argument position. */
template <typename T, typename... A>
	requires async::details::inplace_conversion<char8_t,T,A...>
inline auto u8inplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<char8_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Constructs the result from marked and ordinary char16_t source fragments. */
template <typename T, typename... A>
	requires async::details::returning_conversion<char16_t,T,A...>
[[nodiscard]] inline auto u16to(A &&...args)
{
	return async::details::make_conversion<char16_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts char16_t source fragments into the bound destination at its public argument position. */
template <typename T, typename... A>
	requires async::details::inplace_conversion<char16_t,T,A...>
inline auto u16inplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<char16_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

/** @brief Constructs the result from marked and ordinary char32_t source fragments. */
template <typename T, typename... A>
	requires async::details::returning_conversion<char32_t,T,A...>
[[nodiscard]] inline auto u32to(A &&...args)
{
	return async::details::make_conversion<char32_t,T>(static_cast<A &&>(args)...);
}

/** @brief Converts char32_t source fragments into the bound destination at its public argument position. */
template <typename T, typename... A>
	requires async::details::inplace_conversion<char32_t,T,A...>
inline auto u32inplace_to(T &&target, A &&...args)
{
	return async::details::make_inplace_conversion<char32_t>(static_cast<T &&>(target),static_cast<A &&>(args)...);
}

} // namespace io
} // namespace fast_io
