#pragma once

namespace fast_io::async
{
namespace details
{
/** @brief Interprets publications for one fully bound graph. The inline producer and erased extension use the same rules. */
template <typename Graph>
struct operation_publications
{
	/** @brief Looks up a proof without running the device, including completed roots not yet consumed. */
	template <typename History>
	[[nodiscard]] static point_state inspect(observable_point p, History const *h,
									  bool terminal, bool error, bool local) noexcept
	{
		return inspect_known(p,h,terminal,error,local);
	}

	/** @brief Exposes the same classification to a snapshot with a compile-time graph index. */
	template <typename History>
	[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE static point_state inspect_known(observable_point p, History const *h,
									  bool terminal, bool error, bool local) noexcept
	{
		if (Graph::index(p) == Graph::count) return point_state::unavailable;
		if constexpr (Graph::has_start_points)
		{
			if (Graph::starts_at(p))
			{
				if (!Graph::selected_start(p)) return point_state::unavailable;
				if (h)
				{
					if (h->lookup(details::observation_history::capture_failed)) return point_state::unreached;
					auto slot{details::observation_history::key(p)};
					if constexpr (requires { Graph::local_slot(p); }) if (p.local) slot = Graph::local_slot(p);
					switch (h->lookup(slot))
					{
					case 1: return point_state::started;
					case 2: return point_state::unused;
					case 3: return point_state::unreached;
					}
				}
				if (!terminal) return point_state::pending;
				return error ? point_state::unreached : point_state::unused;
			}
		}
		if (h)
		{
			// A failed issue-time capture precedes every public value and device effect.
			if (h->lookup(details::observation_history::capture_failed)) return point_state::unreached;
			auto const slot = [&] {
				if constexpr (requires { Graph::local_slot(p); }) if (p.local) return Graph::local_slot(p);
				return details::observation_history::key(p);
			}();
			if (auto v{h->lookup(slot)})
				return v == 3 ? point_state::failed : v == 2 ? point_state::unused : point_state::complete;
			if constexpr (requires { Graph::local_slot(p); })
				if (p.local)
				{
					auto parent{h->lookup(details::observation_history::key({point_side::argument,p.depth}))};
					// A closed field has already classified its untaken branches, even when a later field fails.
					if (parent == 3u) return point_state::unreached;
					if (parent) return point_state::unused;
					if (terminal) return error ? point_state::unreached : point_state::unused;
					return point_state::pending;
				}
			local = local || h->lookup(details::observation_history::resident);
			if ((p.side == point_side::input || p.side == point_side::output) && p.depth &&
				(terminal || (p.side == point_side::input && h->lookup(details::observation_history::input_closed))) &&
				h->lookup(p.side == point_side::input ? details::observation_history::tracked_input : details::observation_history::tracked_output) &&
				!h->lookup(details::observation_history::key(p)+1u))
				return point_state::unused;
			if (p.side == point_side::input && h->lookup(details::observation_history::input_closed)) return point_state::complete;
			if (p.side == point_side::output && h->lookup(details::observation_history::output_closed)) return point_state::complete;
			if (!terminal && !p.local && Graph::kind(p)==point_kind::transport &&
				h->lookup(details::observation_history::key(p)+1u)) return point_state::submitted;
		}
		if (local && (p.side == point_side::input || p.side == point_side::output) && p.depth) return point_state::unused;
		if (terminal && error) return Graph::count == 1 ? point_state::failed : point_state::unreached;
		return terminal ? point_state::complete : point_state::pending;
	}

	/** @brief Checks membership, stage kind, selection and predecessors before issuing an irreversible receipt. */
	static ::std::size_t slot(observation_history const &history, observable_point p, unsigned value, bool starting) noexcept
	{
		constexpr auto invalid{static_cast<::std::size_t>(-1)};
		if (Graph::index(p) == Graph::count) return invalid;
		if (value == 1u && starting != Graph::starts_at(p)) return invalid;
		if constexpr (Graph::has_start_points) if (starting && !Graph::selected_start(p)) return invalid;
		if constexpr (requires { Graph::local_slot(p); })
			if (p.local && history.lookup(details::observation_history::key({point_side::argument,p.depth}))) return invalid;
		// An unselected middle stage cannot erase an earlier selected predecessor.
		static constexpr auto required{details::publication_dependencies<Graph>()};
		for (::std::size_t i{}; i != required.size; ++i)
		{
			auto e{required.edges[i]};
			if (e.after != p) continue;
			// Closing a field simultaneously retires its untaken local branches.
			if (p.side==point_side::argument && !p.local && e.before.local && e.before.depth==p.depth) continue;
			auto prior{inspect(e.before,&history,false,false,false)};
			if (prior==point_state::pending || prior==point_state::submitted) return invalid;
		}
		if constexpr (requires { Graph::local_slot(p); }) if (p.local) return Graph::local_slot(p);
		return details::observation_history::key(p);
	}
};
} // namespace details
/**
 * @brief Owns one logical operation and its independent observable points on a single issuer.
 * @details Ordinary consumption uses the file's inline or coroutine representation. Waiting on an
 * intermediate publication borrows a named operation without consuming its value or exception.
 * A private observer parks the producer at a resource-safe checkpoint. An explicitly submitted
 * root continues independently and may notify several observers in the same scheduling turn.
 */
template <typename Graph, typename Job>
class async_operation : private details::operation_publications<Graph>
{
	template <typename G, typename J>
	friend auto &details::root_task(async_operation<G, J> &);

	Job job;
	enum class receipt : unsigned char { live, local, local_error, framed, framed_error };
	static constexpr bool may_throw_on_delivery = [] {
		using T = typename Job::result_type;
		if constexpr (::std::is_void_v<T>) return false;
		else return !FAST_IO_HERBCEPTIONS_NOEXCEPT(T(::std::declval<T &&>()));
	}();
	/**
	 * @brief Determines whether a terminal transfer can be represented by the promise's first publication word.
	 * @details File observations can query the adjacent submission slot as well as the release slot.
	 * A local value stage can consult its own slot and its field's closure. If all reachable slots
	 * fit in the first word, copying that word preserves every later status query after frame
	 * destruction. This also covers unused branches, failed capture and release of a resident file.
	 * A deep graph materializes its per-point states while the full history is still alive.
	 */
	static inline constexpr bool fits_inline_receipt = []() constexpr noexcept
	{
		if constexpr (Graph::operation == algorithm::transmit || Graph::operation == algorithm::convert ||
			requires (observable_point p) { Graph::local_slot(p); })
		{
			for (::std::size_t i{}; i != Graph::count; ++i)
			{
				auto point{Graph::point(i)};
				auto last{details::observation_history::key(point)};
				if constexpr (requires { Graph::local_slot(point); })
					if (point.local)
					{
						if (last >= details::inline_publications::width) return false;
						last = Graph::local_slot(point);
					}
				if (point.side != point_side::argument) ++last;
				if (last >= details::inline_publications::width)
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}();
	::std::conditional_t<fits_inline_receipt, details::inline_publications, point_state[Graph::count ? Graph::count : 1u]> retired;
	receipt outcome{receipt::live};
	struct infallible_delivery {};
	/** @brief Remembers a failed result handoff separately from the provider's visited and skipped points.
	 * @details Only a result with a potentially throwing move needs a flag. Void and nonthrowing
	 * results keep the direct join and the original receipt layout.
	 */
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
	::std::conditional_t<may_throw_on_delivery, bool, infallible_delivery> delivery_failed{};

	/** @brief Drives only the observations still outstanding after issue-time capture or a failed buffer probe.
	 * @details Keeping the scheduler and its cleanup here lets small successful puts inline into their caller.
	 */
	template <typename Binding>
#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	void reach_issue_barrier()
	{
		auto filter = [](observable_point p) noexcept { return Binding::before_return(p); };
		if (!satisfied(filter))
		{
			auto barrier{reach(*this, filter)};
			::fast_io::async::details::run(barrier);
		}
	}

	using details::operation_publications<Graph>::inspect;
	using details::operation_publications<Graph>::inspect_known;

	/** @brief Uses one classifier for live and retired observations of a short staged graph.
	 * @details Only the first publication word can affect these queries. Its contents are stable
	 * during an issuer-side query. A missing history is passed through so immediate completion
	 * retains its own classification even when the graph contains an optional local stage.
	 */
	static point_state inspect(observable_point point, details::observation_history const *history,
		bool terminal, bool error, bool local) noexcept
		requires (fits_inline_receipt && requires { Graph::local_slot(point); })
	{
		details::inline_publications word{history ? history->inline_receipt(local) : details::inline_publications{}};
		return details::operation_publications<Graph>::inspect(point,
			history ? ::fast_io::freestanding::addressof(word) : nullptr, terminal, error, local);
	}

	/** @brief Captures the last state before result delivery destroys the coroutine or its error payload. */
	FAST_IO_GNU_ALWAYS_INLINE void remember(details::observation_history const *h, bool failed, bool local) noexcept
	{
		if (!h) {outcome=failed ? receipt::local_error : receipt::local;return;}
		if constexpr (fits_inline_receipt)
			retired = h->inline_receipt(local);
		else
			for (::std::size_t i{}; i!=Graph::count;++i) retired[i]=inspect(Graph::point(i),h,true,failed,local);
		outcome=failed ? receipt::framed_error : receipt::framed;
	}

	/** @brief Converts an immediate parser or custom formatter exception into an observable terminal failure. */
	template <bool Await>
	FAST_IO_GNU_ALWAYS_INLINE typename Job::result_type consume() FAST_IO_ASYNC_THROWS
	{
		auto report = [this](details::observation_history const *h,bool error,bool local) noexcept {
			remember(h,error,local);
		};
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
		if constexpr (may_throw_on_delivery)
		{
			// Construct the caller's value inside this boundary, including its last potentially throwing move.
			try
			{
				if constexpr (Await && requires { job.await_resume(report); }) return job.await_resume(report);
				else return job.get(report);
			}
#if defined(__HERBCEPTIONS__)
			catch throws(::std::error) { delivery_failed = true; throw throws; }
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			catch (...) { delivery_failed = true; throw; }
#endif
		}
		else
#endif
		{
			// Preserve the direct join for void and nonthrowing result types.
			if constexpr (Await && requires { job.await_resume(report); }) return job.await_resume(report);
			else return job.get(report);
		}
	}

	/** @brief Borrows the public operation through a full join without invalidating live issuer-side snapshots. */
	struct result_awaiter
	{
		async_operation &op;
		FAST_IO_GNU_ALWAYS_INLINE bool await_ready() noexcept
		{
			if constexpr (requires { op.job.await_ready(); }) return op.job.await_ready();
			else return op.job.finished();
		}
		auto await_suspend(::std::coroutine_handle<> parent) noexcept
		{
			if constexpr (requires { op.job.await_suspend(parent); }) return op.job.await_suspend(parent);
			else return op.job.join_from(parent);
		}
		FAST_IO_GNU_ALWAYS_INLINE decltype(auto) await_resume() FAST_IO_ASYNC_THROWS { return op.template consume<true>(); }
	};

public:
	using observable_points = Graph;
	using result_type = typename Job::result_type;
	/** @brief Constructs the selected work directly in its final owner and optionally enforces a return-time cut. */
	template <typename B, typename Make>
	FAST_IO_GNU_ALWAYS_INLINE explicit async_operation(io_type_t<B>, Make &&make) : job(make())
	{
		using order = decltype(details::operation_dependencies<Graph>());
		if constexpr ((order::count != 0 || Graph::has_start_points) &&
			!requires { requires Job::template checks_publications<Graph>; })
		{
			details::root_task(job).validate_publications(details::operation_publications<Graph>::slot, details::has_start_points<typename Graph::input> || details::has_start_points<typename Graph::output>);
		}
		if constexpr (requires { job.prepare_values_at_issue(); }) job.prepare_values_at_issue();
		if constexpr (details::has_barrier<B, Graph>())
		{
			if constexpr (requires { job.try_commit_at_issue(); })
			{
				if (job.try_commit_at_issue()) [[likely]] return;
				// The built-in resident drivers complete the outer buffer transaction as a single call.
				constexpr auto outer{Graph::operation == algorithm::scan ? input_point<0> : output_point<0>};
				if constexpr (order::count == 0)
					if constexpr (Graph::index(outer) != Graph::count && Graph::kind(outer) == point_kind::buffer &&
						B::before_return(outer))
					{
						job.finish_buffer_at_issue();
						return;
					}
			}
			reach_issue_barrier<B>();
		}
	}
	async_operation(async_operation const &) = delete;
	async_operation(async_operation &&other) noexcept(::std::is_nothrow_move_constructible_v<Job>)
		: job(static_cast<Job &&>(other.job)),outcome(other.outcome)
	{
		if constexpr (may_throw_on_delivery) delivery_failed = other.delivery_failed;
		if (outcome==receipt::framed || outcome==receipt::framed_error)
		{
			if constexpr (fits_inline_receipt) retired=other.retired;
			else for (::std::size_t i{};i!=Graph::count;++i) retired[i]=other.retired[i];
		}
	}

	/** @brief Inlines the contained job's completion boundary so an already joined resident operation has no out-of-line destructor. */
	FAST_IO_GNU_ALWAYS_INLINE ~async_operation() = default;

	/** @brief Reads a single point in constant time for inline-depth graphs. */
	[[nodiscard]] point_state state(observable_point p) const noexcept
	{
		if (outcome==receipt::local || outcome==receipt::local_error)
			return inspect(p,static_cast<details::observation_history const *>(nullptr),true,outcome==receipt::local_error,outcome==receipt::local);
		if (outcome!=receipt::live)
		{
			if constexpr (fits_inline_receipt)
				return inspect(p,::fast_io::freestanding::addressof(retired),true,outcome==receipt::framed_error,false);
			else
			{
				auto i{Graph::index(p)};
				return i==Graph::count ? point_state::unavailable : retired[i];
			}
		}
		bool local{};
		if constexpr (requires { job.local_ready(); }) local = job.local_ready();
		return inspect(p,job.observations(),job.finished(),job.failed(),local);
	}
private:
	/** @brief Classifies each point with a compile-time graph index. */
	template <typename History>
	FAST_IO_GNU_ALWAYS_INLINE static void inspect_status(observation_status<Graph> &snapshot, History const *history, bool local) noexcept
	{
		[&]<::std::size_t... I>(::std::index_sequence<I...>) {
			((snapshot.states[I] = inspect_known(Graph::point(I),history,snapshot.terminal,snapshot.error,local)), ...);
		}(::std::make_index_sequence<Graph::count>{});
	}

	/** @brief Takes an independent value snapshot. Failed operations keep their own exception channel. */
	template <bool Indexed>
	FAST_IO_GNU_ALWAYS_INLINE observation_status<Graph> make_status() const noexcept
	{
		observation_status<Graph> snapshot{};
		if constexpr (Indexed && fits_inline_receipt && !may_throw_on_delivery &&
			requires { job.live_framed_observations(); })
		{
			// A nonterminal framed conversion can read its whole receipt from the first publication word.
			if (outcome == receipt::live)
				if (auto *history{job.live_framed_observations()})
				{
					auto packed{history->inline_receipt(false)};
					inspect_status(snapshot,::fast_io::freestanding::addressof(packed),false);
					return snapshot;
				}
		}
		snapshot.terminal = outcome!=receipt::live || job.finished();
		snapshot.error = outcome==receipt::local_error || outcome==receipt::framed_error || (outcome==receipt::live && job.failed());
		if constexpr (may_throw_on_delivery) snapshot.error = snapshot.error || delivery_failed;
		if (outcome == receipt::live)
		{
			// No provider runs while the issuer copies a snapshot. Read its frame and phase just once.
			auto history{job.observations()};
			bool local{};
			if constexpr (requires { job.local_ready(); }) local = job.local_ready();
			// Each compile-time index follows the same point, slot and parent classification.
			if constexpr (Indexed)
				inspect_status(snapshot,history,local);
			else
				for (::std::size_t i{}; i != Graph::count; ++i)
					snapshot.states[i] = inspect(Graph::point(i),history,snapshot.terminal,snapshot.error,local);
		}
		else
			for (::std::size_t i{}; i != Graph::count; ++i) snapshot.states[i] = state(Graph::point(i));
		return snapshot;
	}

public:
	/** @brief Takes an independent value snapshot. Failed operations keep their own exception channel. */
	[[nodiscard]] observation_status<Graph> status() const noexcept { return make_status<false>(); }
	/** @brief Joins the operation and consumes its value or failure exactly once.
	 * @details A constructing operation delivers its completed value by value. For an in-place operation
	 * the join ends the operation's use of the caller's target, including when it reports failure. Each
	 * provider retains its own guarantees about writes already committed before that failure. A result
	 * move that fails during delivery sets the error flag while preserving the provider's recorded points.
	 */
	FAST_IO_GNU_ALWAYS_INLINE result_type get() FAST_IO_ASYNC_THROWS { return consume<false>(); }
	/** @brief Awaits the same consuming join while retaining the receipt on the operation for later inspection. */
	FAST_IO_GNU_ALWAYS_INLINE result_awaiter operator co_await() noexcept { return {*this}; }

	/** @brief Explicitly erases observable points while transferring the single result channel to a plain task. */
	operator task<result_type>() && { return static_cast<task<result_type> &&>(details::root_task(job)); }

private:
	/** @brief Checks the fixed receipt slot of a selected provider start without traversing the graph.
	 * @details Selection proves membership and the start kind at compile time. Once the producer has
	 * retired, failed capture, or classified that slot, waiting for its start can make no further progress.
	 */
	template <observable_point Point>
	bool start_settled() const noexcept
	{
		static_assert(Graph::selected_start(Point));
		if (outcome != receipt::live || job.finished()) return true;
		auto history{job.observations()};
		return history && (published_start<Point>(*history) || history->lookup(details::observation_history::capture_failed));
	}

	/** @brief Tests one prevalidated start using the notifying promise's own history. */
	template <observable_point Point>
	static bool published_start(details::observation_history const &history) noexcept
	{
		constexpr auto slot = [] {
			if constexpr (Point.local && requires { Graph::local_slot(Point); }) return Graph::local_slot(Point);
			else return details::observation_history::key(Point);
		}();
		return history.lookup(slot);
	}

	/** @brief Retains the type-level selection through subscription and each subsequent notification. */
	template <observable_point... Points>
	struct selected_starts
	{
		bool settled(async_operation const &op) const noexcept { return (op.template start_settled<Points>() && ...); }
		/** @brief Tests the selected starts in the history of the root that owns this observer.
		 * @details A nonterminal notification borrows that exact history. Terminal completion wakes
		 * the observer directly. A capture failure settles every start, so checking it once is sufficient.
		 */
		static bool notified(details::observation_history const &history) noexcept
		{
			return (published_start<Points>(history) && ...) || history.lookup(details::observation_history::capture_failed);
		}
	};

	template <bool Admission=false,typename Filter>
	bool satisfied(Filter const &filter) const noexcept
	{
		if constexpr (requires { filter.settled(*this); })
			return filter.settled(*this);
		else
		{
			for (::std::size_t i{}; i != Graph::count; ++i)
			{
				auto p{Graph::point(i)};
				if (!filter(p)) continue;
				auto current{state(p)};
				if (current==point_state::pending || (!Admission && current==point_state::submitted)) return false;
				if constexpr (Admission && Graph::has_start_points)
				{
					// Entering a provider may precede its first device request. Check the independent admission receipt.
					if (current == point_state::started && outcome == receipt::live && !job.finished())
					{
						auto history{job.observations()};
						if (!history || !history->lookup(details::observation_history::key(p) + 1u)) return false;
					}
				}
			}
			return true;
		}
	}

	template <typename Filter,bool Admission=false>
	struct point_awaiter : details::observation_wait
	{
		async_operation &op;
		Filter filter;
		details::scheduler *subscribed_loop{};
		point_awaiter(async_operation &owner, Filter test) : op(owner), filter(test) {}
		FAST_IO_GNU_ALWAYS_INLINE bool await_ready() const noexcept { return op.template satisfied<Admission>(filter); }
		FAST_IO_GNU_ALWAYS_INLINE auto await_suspend(::std::coroutine_handle<> parent)
		{
			if constexpr (Admission)
			{
				// A foreign coroutine can drive the file directly. Only a fast_io issuer can flush queued submissions.
				subscribed_loop = details::current_loop;
				if (subscribed_loop)
				{
					++subscribed_loop->admission_waiters;
				}
			}
			this->satisfied = [](details::observation_wait const &item, details::observation_history const &history) noexcept {
				if constexpr (!Admission && Graph::operation == algorithm::convert && requires { Filter::notified(history); })
					return Filter::notified(history);
				else
				{
					auto &self{static_cast<point_awaiter const &>(item)};
					return self.op.template satisfied<Admission>(self.filter);
				}
			};
			return details::root_task(op.job).observe(*this,parent);
		}
		FAST_IO_GNU_ALWAYS_INLINE auto await_resume() const noexcept
		{
			if constexpr (Admission) if (subscribed_loop) --subscribed_loop->admission_waiters;
			if constexpr (!Admission && Graph::operation == algorithm::convert && Graph::count <= 4 &&
				requires { filter.settled(op); })
			{
				return op.template make_status<true>();
			}
			else
				return op.status();
		}
	};

	template <typename Filter>
	static task<void> reach(async_operation &op, Filter filter)
	{
		(void)co_await point_awaiter<Filter>{op,filter};
	}

public:
	/** @brief Waits once for all selected applicable points. An empty or unavailable selection is ready immediately. */
	template <typename... Points> requires (::std::same_as<Points,observable_point> && ...)
	[[nodiscard]] auto when(Points... points) & noexcept
	{
		auto filter = [points...](observable_point p) noexcept { return ((p == points) || ...); };
		return point_awaiter<decltype(filter)>{*this,filter};
	}
	/** @brief Waits for an actual transport submission or a later terminal disposition of that same file point.
	 * @details An independently submitted root may keep running after this notification. A private caller or a
	 * blocking backend can observe completion in the same wakeup. Its buffer remains borrowed until completion.
	 */
	template <observable_point Point>
		requires (Graph::index(Point)!=Graph::count && Graph::kind(Point)==point_kind::transport)
	[[nodiscard]] auto when_submitted() & noexcept
	{
		auto select=[](observable_point p) noexcept {return p==Point;};
		return point_awaiter<decltype(select),true>{*this,select};
	}

	/** @brief Requests exact compile-time points. An absent point removes the overload from resolution. */
	template <observable_point... Points> requires (sizeof...(Points) != 0 && ((Graph::index(Points) != Graph::count) && ...))
	[[nodiscard]] auto when() & noexcept { return when(Points...); }

	/** @brief Waits for actual entry into selected work or a definitive reason why it could not start.
	 * @details A started state remains true after a later failure. The operation's result and borrowed
	 * memory stay with the producing coroutine until its separate completion contract is satisfied.
	 */
	template <observable_point... Points>
		requires (sizeof...(Points) != 0 && Graph::has_start_points && (Graph::selected_start(Points) && ...))
	[[nodiscard]] auto when_started() & noexcept { return point_awaiter<selected_starts<Points...>>{*this,{}}; }

};

} // namespace fast_io::async
