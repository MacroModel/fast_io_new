#pragma once

namespace fast_io::async
{
template <typename Graph, typename Job>
class async_operation;
}

namespace fast_io::async::details
{

/**
 * @brief Defers a device operation whose whole result can sometimes be committed by its consumer.
 * @details The operation owns its formatter or scan targets from construction. Awaiting probes the device
 * at the point of consumption. A successful probe stays in the parent coroutine. An incomplete probe
 * leaves the device unchanged and materializes the ordinary suspending task, including its error channel.
 * Unconditionally batching a named operation also materializes that task so other roots can make progress.
 */
template <typename T, typename Work>
class resident_operation
{
public:
	using result_type = T;
private:
	enum class phase : unsigned char
	{
		initial,
		framed,
		ready,
		local_error,
		consumed
	};
	Work work;
	task<T> child{typename task<T>::handle_type{}};
	phase state{phase::initial};

	/** @brief Returns the committed result or executes ready work at the throwing consumption boundary. */
	FAST_IO_GNU_ALWAYS_INLINE T success() FAST_IO_ASYNC_THROWS
	{
		if constexpr (requires { work.consume(); })
		{
			return work.consume();
		}
		else if constexpr (!::std::is_void_v<T>)
		{
			static_assert(::std::is_same_v<T, bool>);
			return true;
		}
	}

	/** @brief Lets the owning caller inline a ready provider and its receipt through the same error boundary. */
	template <typename Report>
	FAST_IO_GNU_ALWAYS_INLINE T consume_ready(Report report) FAST_IO_ASYNC_THROWS
	{
		if constexpr (requires { work.consume(report); }) return work.consume(report);
		else return work.consume();
	}

	/** @brief Records a possible inline error before calling a throwing consumer, then commits success exactly once. */
	template <typename Report>
	FAST_IO_GNU_ALWAYS_INLINE T inline_result(Report report) FAST_IO_ASYNC_THROWS
	{
		if constexpr (requires { work.consume(); })
		{
			// The resident phase is already part of the transaction. A throwing consumer leaves a stable failure there.
			state=phase::local_error;

			if constexpr (::std::is_void_v<T>)
			{
				consume_ready(report);
				state=phase::consumed;
			}
			else
			{
				auto value{consume_ready(report)};
				state=phase::consumed;
				return value;
			}
		}
		else
		{
			state=phase::consumed;
			return success();
		}
	}

	/** @brief Attempts one transaction before taking ownership of a fallback frame. */
	FAST_IO_GNU_ALWAYS_INLINE bool try_ready() noexcept
	{
		if (state == phase::initial && work(::std::true_type{}))
		{
			state = phase::ready;
			return true;
		}
		return state == phase::ready;
	}

	/**
	 * @brief Retains a named root's owned operands and tests its current device window on first resume.
	 * @details Between creation and submission another operation may refill or consume the same window.
	 * A successful nonthrowing transaction produces the result in this frame. A miss leaves all public
	 * state unchanged and transfers control to the usual suspending implementation.
	 */
	static task<T> run_transaction(Work owned) FAST_IO_ASYNC_THROWS
	{
		if (owned(::std::true_type{}))
		{
			co_await publish_observation{observation_history::resident,1};
			if constexpr (::std::is_void_v<T>)
			{
				co_return;
			}
			else
			{
				static_assert(::std::is_same_v<T, bool>);
				co_return true;
			}
		}
		co_return co_await owned(::std::false_type{});
	}

	/** @brief Supplies the result of an already committed window when a caller requests a concrete batch root. */
	static task<T> completed_transaction()
	{
		if constexpr (::std::is_void_v<T>)
			co_return;
		else
		{
			static_assert(::std::is_same_v<T, bool>);
			co_return true;
		}
	}

	/** @brief Builds a child after a consumer has already tried the current window or needs its ordinary task. */
	task<T> &frame_after_probe()
	{
		if (state == phase::initial)
		{
			auto issued{task<T>::materialize([this]() FAST_IO_ASYNC_THROWS { return work(::std::false_type{}); })};
			child.frame = issued.release();
			state = phase::framed;
		}
		if (state != phase::framed) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return child;
	}

	/** @brief Keeps a resumed provider's receipt readable until the actual result retires its child. */
	template <bool Completed = false, typename Report>
	FAST_IO_GNU_ALWAYS_INLINE T consume_child(Report report) FAST_IO_ASYNC_THROWS
	{
		auto retire = [&](observation_history const *history,bool error,bool local) noexcept {
			report(history,error,local);
			state = phase::consumed;
		};
		if constexpr (Completed)
			return child.consume_completed(retire);
		else if constexpr (requires { typename Work::publication_graph; })
			return child.template consume_result<Work::publication_graph::operation == algorithm::convert>(retire);
		else return child.get(retire);
	}

public:
	/** @brief A typed producer may check every publication directly against its fully bound graph. */
	template <typename Graph>
	static inline constexpr bool checks_publications = requires {
		typename Work::publication_graph;
		requires ::std::same_as<typename Work::publication_graph,Graph>;
	};

	/** @brief Constructs the operation directly in its result storage. */
	template <typename... Args>
	explicit resident_operation(::std::in_place_t, Args &&...args) : work(static_cast<Args &&>(args)...)
	{}

	/** @brief Transfers staged data and any already materialized task to a single new consumer. */
	resident_operation(resident_operation &&other) noexcept(::std::is_nothrow_move_constructible_v<Work>)
		: work(static_cast<Work &&>(other.work)), child(static_cast<task<T> &&>(other.child)), state(other.state)
	{
		other.state = phase::consumed;
	}
	resident_operation(resident_operation const &) = delete;

	/** @brief Returns a known terminal state without probing, consuming, or touching the device. */
	[[nodiscard]] bool finished() const noexcept
	{
		if (state == phase::framed) return child.finished();
		if constexpr (requires { work.joining_history; }) if (work.joining_history) return false;
		if constexpr (!requires { work.consume(); }) if (state == phase::ready) return true;
		return state == phase::consumed || state == phase::local_error;
	}

	/** @brief Borrows a nonterminal framed child's packed receipt during its issuer's current turn. */
	FAST_IO_GNU_ALWAYS_INLINE observation_history const *live_framed_observations() const noexcept
	{
		if (state == phase::framed && child.frame && !child.frame.done()) return child.observations();
		return nullptr;
	}

	/** @brief Exposes a materialized child's actual publications without forcing a frame. */
	[[nodiscard]] observation_history const *observations() const noexcept
	{
		if (state == phase::framed) return child.observations();
		if constexpr (requires { work.joining_history; }) return work.joining_history;
		else return nullptr;
	}
	/** @brief A buffered probe can succeed before the frame's result is consumed. */
	[[nodiscard]] bool local_ready() const noexcept
	{ return state == phase::consumed || (state == phase::ready && !requires { work.consume(); }); }

	/** @brief Commits a nonthrowing resident transaction for a return barrier. A miss preserves the file window. */
	FAST_IO_GNU_ALWAYS_INLINE bool try_commit_at_issue() noexcept
		requires (!requires(Work &w) { w.consume(); })
	{
		return try_ready();
	}

	/**
	 * @brief Executes an indivisible buffer overflow or refill required before the issuer may return.
	 * @details A failed reservation has made no change. Its fallback owns the same arguments and
	 * invokes the original synchronous device protocol. The outer file point becomes observable
	 * only when that call returns. A private issuer can execute it directly and retain the
	 * resulting frame, including the backing file activity and any exception, for a later join.
	 */
#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#endif
	void finish_buffer_at_issue() noexcept
		requires (!requires(Work &w) { w.consume(); })
	{
		auto &issued{frame_after_probe()};
		::std::coroutine_handle<> root{issued.frame};
		scheduler loop{};
		loop.standalone_root = root;
		drive(loop, ::fast_io::freestanding::addressof(root), 1);
	}
	/** @brief Inspects a completed child's error while keeping it available for get or await. */
	[[nodiscard]] bool failed() const noexcept
	{
		if constexpr (requires { work.joining_history; }) if (work.joining_history) return false;
		return state == phase::local_error || (state == phase::framed && child.failed());
	}

	/** @brief Supplies the work object to the formatter before the operation is published. */
	Work &staging() noexcept
	{
		return work;
	}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
	/** @brief Retains the current C++ exception from issue-time staging until the unique consuming join. */
	void creation_exception() noexcept
	{
		child.frame = task<T>::from_creation_exception().release();
		state = phase::framed;
	}
#endif
#if defined(__HERBCEPTIONS__)
	/** @brief Saves an error from formatting alongside errors reported by a submitted operation. */
	void creation_error(::std::error &failure) noexcept
	{
		child.frame = task<T>::from_creation_error(failure).release();
		state = phase::framed;
	}
#endif

	/** @brief Hands the original captured work to a coroutine for explicit batching or device backpressure. */
	task<T> &as_task()
	{
		if constexpr (!requires { work.consume(); })
		{
			if (state == phase::ready)
			{
				// A return barrier already committed this window. The batch only needs its completed result.
				child.frame = completed_transaction().release();
				child.frame.promise().history.record(observation_history::resident, 1);
				child.frame.resume();
				state = phase::framed;
				return child;
			}
			if (state == phase::initial)
			{
				// Conversion to task remains lazy. The new frame owns the operands after this object moves.
				auto issued{make_io_operation([this]() FAST_IO_ASYNC_THROWS {
					return run_transaction(static_cast<Work &&>(work));
				})};
				child.frame = issued.release();
				state = phase::framed;
			}
		}
		return frame_after_probe();
	}

	/** @brief Supports callers that store a public result in an ordinary task. */
	operator task<T>() &&
	{
		auto &issued{as_task()};
		state = phase::consumed;
		return static_cast<task<T> &&>(issued);
	}

	/** @brief Retains the task-reference interface for a caller with an explicitly typed parameter. */
	operator task<T> &() &
	{
		return as_task();
	}

	/** @brief Drives a standalone operation and observes the original result or error exactly once. */
	template <typename Report = ignore_observations>
	FAST_IO_GNU_ALWAYS_INLINE T get(Report report = {}) FAST_IO_ASYNC_THROWS
	{
		if constexpr (requires { work.consume(); })
		{
			// A ready device can complete or throw here. Only an explicitly submitted root owns a frame.
			if (state == phase::initial || state == phase::ready)
			{
				if constexpr (requires { work.can_join_inline(); })
					if (!work.can_join_inline())
					{
						frame_after_probe();
						return consume_child(report);
					}
				return inline_result(report);
			}
			if (state != phase::framed) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			return consume_child(report);
		}
		else
		{
			if (try_ready())
			{
				return inline_result(report);
			}
			frame_after_probe();
			return consume_child(report);
		}
	}

	/** @brief Resolves a resident record before the compiler needs to suspend the awaiting coroutine. */
	FAST_IO_GNU_ALWAYS_INLINE bool await_ready() noexcept
	{
		if constexpr (requires { work.consume(); })
		{
			if (state == phase::initial)
			{
				if constexpr (requires { work.can_join_inline(); })
					if (!work.can_join_inline()) return frame_after_probe().finished();
				state = phase::ready;
			}
			if (state == phase::ready)
			{
				return true;
			}
			if (state != phase::framed) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			return child.frame.done();
		}
		else
		{
			return try_ready() || frame_after_probe().frame.done();
		}
	}

	/** @brief Starts the fallback frame by symmetric transfer, keeping the awaiting caller alive. */
	auto await_suspend(::std::coroutine_handle<> parent) noexcept
	{
		return child.join_from(parent);
	}

	/** @brief Observes completion without running a completed child through the scheduler again. */
	template <typename Report = ignore_observations>
	FAST_IO_GNU_ALWAYS_INLINE T await_resume(Report report = {}) FAST_IO_ASYNC_THROWS
	{
		if (state == phase::ready)
		{
			return inline_result(report);
		}
		return consume_child<true>(report);
	}

	/** @brief Keeps the existing full-expression and unwinding behavior for a discarded public operation. */
	FAST_IO_GNU_ALWAYS_INLINE ~resident_operation()
#if defined(__HERBCEPTIONS__) && !defined(__cpp_exceptions) && !defined(_CPPUNWIND)
		noexcept
#else
		noexcept(false)
#endif
	{
		if (state != phase::initial && (state != phase::framed || (!child.pending() && !child.failed())))
		{
			return;
		}
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		if (::std::uncaught_exceptions() != 0) [[unlikely]]
		{
			return;
		}
#endif
#if defined(__HERBCEPTIONS__)
		try
		{
			get();
		}
		catch throws(::std::error failure)
		{
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			failure.throw_legacy_exception();
#else
			::fast_io::fast_terminate();
#endif
		}
#else
		get();
#endif
	}
};

/** @brief Returns an existing root coroutine by reference. */
template <typename T>
inline task<T> &root_task(task<T> &job) noexcept
{
	return job;
}

/** @brief Materializes a resident result when a batch needs a persistent root frame. */
template <typename T, typename Work>
inline task<T> &root_task(resident_operation<T, Work> &job)
{
	return job.as_task();
}

/** @brief Borrows a named I/O operation's execution frame while its owner retains the result and observations. */
template <typename Graph, typename Job>
inline auto &root_task(async_operation<Graph, Job> &operation)
{
	return ::fast_io::async::details::root_task(operation.job);
}

/** @brief Accepts library-owned tasks and I/O operations whose lifetimes cover the batch. */
template <typename Job>
concept scheduler_root = requires(Job &job) { ::fast_io::async::details::root_task(job); };

/** @brief Internal batch adapter. Its call operator keeps the scheduler out of operand ADL. */
struct run_t
{
	/** @brief Drives distinct roots on one issuer and leaves each result in its owner for a later join. */
	template <scheduler_root... Jobs>
	void operator()(Jobs &...jobs) const
	{
		if constexpr (sizeof...(jobs) != 0)
		{
			::std::coroutine_handle<> roots[]{::fast_io::async::details::root_task(jobs).frame...};
			::std::coroutine_handle<> entries[]{root_task(jobs).frame.promise().history.next_entry(root_task(jobs).frame)...};
			run_roots(roots, sizeof...(jobs),entries);
		}
	}
};

inline constexpr run_t run{};

/** @brief Internal adapter for an event pump supplied by a freestanding device or a test backend. */
struct run_with_t
{
	/**
	 * @brief Drives all roots until completion while their frames and the borrowed pump remain alive.
	 * @details A pump accepting a Boolean receives each scheduling turn and may block when the ready queue is
	 * empty. A nullary pump is invoked only when the issuer needs an external completion to make progress.
	 */
	template <typename Poll, scheduler_root... Jobs>
	void operator()(Poll &&poll, Jobs &...jobs) const
	{
		if constexpr (sizeof...(jobs) != 0)
		{
			scheduler loop{};
			// The reference adapter also accepts a free function or a const, noncopyable pump.
			auto step = [&poll](bool may_block) {
				if constexpr (requires { poll(may_block); })
				{
					poll(may_block);
				}
				else if (may_block)
				{
					poll();
				}
			};
			loop.context = ::fast_io::freestanding::addressof(step);
			loop.step = [](void *context, request_queue &ready) {
				(*static_cast<decltype(step) *>(context))(ready.empty());
			};
			::std::coroutine_handle<> roots[]{::fast_io::async::details::root_task(jobs).frame...};
			::std::coroutine_handle<> entries[]{root_task(jobs).frame.promise().history.next_entry(root_task(jobs).frame)...};
			drive(loop, roots, sizeof...(jobs),entries);
		}
	}
};

inline constexpr run_with_t run_with{};

} // namespace fast_io::async::details
