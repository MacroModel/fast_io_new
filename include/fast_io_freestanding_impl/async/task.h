#pragma once

namespace fast_io::async
{

namespace details
{
template <typename T, typename Work>
class resident_operation;

struct conversion_issuer {};

/** @brief Reads the current coroutine's issuer without saving a suspend state. */
template <typename Owner>
struct ready_issuer : ::std::suspend_never
{
	Owner &issuer;
	Owner &await_resume() const noexcept { return issuer; }
};

struct run_t;
struct run_with_t;
struct pending_prefix_write;

/** @brief Remembers the allocation underlying a coroutine frame, including any alignment padding. */
struct frame_allocation
{
	void *base;
	::std::size_t bytes;
};

/** @brief Supplies alignment of private local spill types on coroutine ABIs that pass only allocation size. */
template <::std::size_t Alignment> struct frame_alignment_hint {};
template <typename T> inline constexpr ::std::size_t frame_argument_alignment{alignof(T)};
template <::std::size_t Alignment> inline constexpr ::std::size_t frame_argument_alignment<frame_alignment_hint<Alignment>>{Alignment};

/** @brief Combines actual argument alignments and any additional spill requirement at compile time. */
template <typename Promise, typename... Args>
consteval ::std::size_t required_frame_alignment() noexcept
{
	::std::size_t alignment{alignof(Promise) < alignof(::std::max_align_t) ? alignof(::std::max_align_t) : alignof(Promise)};
	((alignment = alignment < frame_argument_alignment<::std::remove_cv_t<Args>> ?
		frame_argument_alignment<::std::remove_cv_t<Args>> : alignment), ...);
	return alignment;
}

/**
 * @brief Reserves the alignment required by the promise, local spill slots and captured arguments.
 * @details A provider with a common unsized release can return its allocation directly. Other
 * providers retain the original pointer and exact size where every promise's deleter can find them.
 */
template <typename Allocator = ::fast_io::native_global_allocator>
inline void *allocate_frame(::std::size_t size, ::std::size_t alignment) noexcept
{
	if constexpr (requires { Allocator::allocate_aligned_for_deallocate(alignment, size); })
	{
		return Allocator::allocate_aligned_for_deallocate(alignment, size);
	}
	else
	{
		if (alignment < alignof(frame_allocation))
		{
			alignment = alignof(frame_allocation);
		}
		auto const overhead{sizeof(frame_allocation) + alignment - 1u};
		if (size > SIZE_MAX - overhead) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		auto const bytes{size + overhead};
		auto *base{static_cast<::std::byte *>(Allocator::allocate(bytes))};
		auto const candidate{reinterpret_cast<::std::uintptr_t>(base + sizeof(frame_allocation))};
		auto const padding{static_cast<::std::size_t>(-candidate) & (alignment - 1u)};
		auto *frame{base + sizeof(frame_allocation) + padding};
		::new (static_cast<void *>(frame - sizeof(frame_allocation))) frame_allocation{base, bytes};
		return frame;
	}
}

/** @brief Uses the same provider contract for every frame, including frames with over-aligned arguments. */
template <typename Allocator = ::fast_io::native_global_allocator>
inline void deallocate_frame(void *frame) noexcept
{
	if constexpr (requires { Allocator::allocate_aligned_for_deallocate(::std::size_t{}, ::std::size_t{}); })
	{
		Allocator::deallocate(frame);
	}
	else
	{
		auto const *header{reinterpret_cast<frame_allocation *>(static_cast<::std::byte *>(frame) - sizeof(frame_allocation))};
		Allocator::deallocate_n(header->base, header->bytes);
	}
}

/** @brief Owns the error state shared by all task result types. */
struct task_error
{
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
	::std::exception_ptr exception{};
#endif
	native_error native{};
#if defined(__HERBCEPTIONS__)
	::std::cxx_std_error herbception{};

	/** @brief Releases an unconsumed error domain payload when a completed task is discarded. */
	~task_error()
	{
		auto const *domain{static_cast<::std::error_domain_singleton const *>(herbception.domain)};
		if (domain != nullptr && domain->do_cleanup != nullptr)
		{
			domain->do_cleanup(herbception.code);
		}
	}
#endif

	/** @brief Captures the current standard exception without changing its dynamic type. */
	void unhandled_exception() noexcept
	{
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		exception = ::std::current_exception();
#else
		::fast_io::fast_terminate();
#endif
	}

#if defined(__HERBCEPTIONS__)
	/** @brief Retains the deterministic error channel of a throws coroutine by value. */
	void unhandled_herbception(::std::cxx_std_error failure) noexcept
	{
		herbception = failure;
	}
#endif

	/** @brief Tests all configured error channels without consuming the retained failure. */
	[[nodiscard]] bool has_error() const noexcept
	{
		return native.raise != nullptr
#if defined(__HERBCEPTIONS__)
			|| herbception.domain != nullptr
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			|| static_cast<bool>(exception)
#endif
			;
	}

	/** @brief Delivers the recorded failure at the caller's await or wait boundary. */
	void rethrow()
#if defined(__HERBCEPTIONS__)
		throws
#endif
	{
#if defined(__HERBCEPTIONS__)
		if (herbception.domain != nullptr) [[unlikely]]
		{
			auto const failure{herbception};
			herbception = {};
			throw throws failure;
		}
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		if (exception) [[unlikely]]
		{
			::std::rethrow_exception(exception);
		}
#endif
		if (native.raise != nullptr) [[unlikely]]
		{
			native.raise(native.code);
		}
	}
};

/** @brief Installs a native failure without making an intermediate C++ throw. */
struct store_native_error
{
	native_error error;

	/** @brief Keeps the promise accessible through await_suspend. */
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	/** @brief Records the failure and returns directly to the running coroutine. */
	template <typename Promise>
	constexpr bool await_suspend(::std::coroutine_handle<Promise> handle) const noexcept
	{
		handle.promise().native = error;
		return false;
	}

	/** @brief Completes the error assignment without a value. */
	constexpr void await_resume() const noexcept
	{}
};

/** @brief Signals that a blocking awaiter already wrote the result into the promise. */
struct result_already_stored
{};

/** @brief Delays one result's construction until its final promise storage is available. */
template <typename Factory>
struct task_value_factory
{
	Factory make;
};

/** @brief Stores a result until its unique consumer retrieves it. */
template <typename T>
struct task_value
{
	union
	{
		unsigned char empty{};
		T value;
	};
	bool engaged{};

	/** @brief Starts with the empty union member active, including for non-default-constructible results. */
	constexpr task_value() noexcept : empty{}
	{}

	/** @brief Destroys exactly the result constructed by co_return or by a completed worker. */
	~task_value()
	{
		if (engaged)
		{
			value.~T();
		}
	}

	/** @brief Constructs the result in place from the co_return operand.
	 * @details The selected constructor determines the exception channel. Publish engagement only
	 * after construction succeeds so failure cleanup never destroys an unconstructed result.
	 */
	template <typename U>
		requires ::std::constructible_from<T, U &&>
	void return_value(U &&result) FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::std::is_herbceptions_throws_constructible_v<T, U &&>), ::std::is_nothrow_constructible_v<T, U &&>)
	{
		::new (static_cast<void *>(::fast_io::freestanding::addressof(value))) T(static_cast<U &&>(result));
		engaged = true;
	}

	/** @brief Constructs the final object directly and publishes it only after successful construction. */
	template <typename Factory>
		requires requires(Factory &make) { { make() } -> ::std::same_as<T>; }
	void return_value(task_value_factory<Factory> factory)
		FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
			!FAST_IO_HERBCEPTIONS_NOTHROWS(factory.make()), noexcept(factory.make()))
	{
		::new (static_cast<void *>(::fast_io::freestanding::addressof(value))) T(factory.make());
		engaged = true;
	}

	/** @brief Leaves the result previously published by a worker untouched. */
	constexpr void return_value(result_already_stored) noexcept
	{}

	/** @brief Moves the stored result into its consumer through the move constructor's exception channel. */
	T take_value() FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::std::is_herbceptions_throws_constructible_v<T, T &&>), ::std::is_nothrow_move_constructible_v<T>)
	{
		return static_cast<T &&>(value);
	}
};

template <>
struct task_value<void>
{
	/** @brief Completes a void coroutine. */
	constexpr void return_void() const noexcept
	{}

	/** @brief Consumes the void result after the error check. */
	constexpr void take_value() const noexcept
	{}
};

/** @brief Transfers control to the awaiting coroutine after the final suspend. */
struct final_awaiter
{
	/** @brief Keeps the frame alive until its owning task consumes it. */
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	/** @brief Resumes the parent by symmetric transfer, without growing the resume stack. */
	template <typename Promise>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> handle) const noexcept
	{
		auto &promise{handle.promise()};
		if (promise.observers)
			if (auto next{notify_observers(promise.observers,promise.history,true)}) return next;
		auto const continuation{promise.continuation};
		return continuation ? continuation : ::std::noop_coroutine();
	}

	/** @brief Provides the required final-awaiter completion hook. */
	constexpr void await_resume() const noexcept
	{}
};

} // namespace details

/**
 * @brief Owns a lazy, single-consumer I/O coroutine.
 * @details An unstarted task may be discarded. Await a task by moving it, or call get() to run it on the
 * calling thread's I/O loop. A parent coroutine must remain alive until its child completes. This task
 * provides no detached execution or cancellation of submitted I/O. A directly awaited temporary owns
 * its child until await_resume or destruction of its parent. Compilers with coroutine allocation
 * elision support can therefore place that child's storage inside the parent for this expression.
 */
template <typename T = void>
class [[nodiscard]]
#if __has_cpp_attribute(clang::coro_await_elidable)
	[[clang::coro_await_elidable]]
#endif
	task
{
public:
	using result_type = T;
	struct promise_type : details::task_error, details::task_value<T>, details::publication_frame
	{
		/** @brief Includes parameter alignment on compilers that use the C++20 leading-allocator convention. */
		template <typename... Args>
		static void *operator new(::std::size_t size, Args &...)
		{
			return details::allocate_frame(size,details::required_frame_alignment<promise_type,Args...>());
		}

#if defined(__cpp_aligned_new) && !defined(_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION) && \
	(!defined(_LIBCPP_HAS_LIBRARY_ALIGNED_ALLOCATION) || _LIBCPP_HAS_LIBRARY_ALIGNED_ALLOCATION)
		/** @brief Honors implementation-provided frame alignment, including over-aligned local spill slots. */
		template <typename... Args>
		static void *operator new(::std::size_t size, ::std::align_val_t alignment, Args &...)
		{
			constexpr auto required{details::required_frame_alignment<promise_type,Args...>()};
			auto requested{static_cast<::std::size_t>(alignment)};
			return details::allocate_frame(size,requested < required ? required : requested);
		}
#endif

		/** @brief Returns frame storage independently of which leading-allocator overload created it. */
		static void operator delete(void *memory, ::std::size_t) noexcept
		{
			details::deallocate_frame(memory);
		}

		/** @brief Returns the only owner of the newly allocated coroutine frame. */
		task get_return_object() noexcept
		{
			return task{::std::coroutine_handle<promise_type>::from_promise(*this)};
		}

		/** @brief Defers all I/O until the task is awaited or driven. */
		constexpr ::std::suspend_always initial_suspend() const noexcept
		{
			return {};
		}

		/** @brief Exposes this conversion's issuer and continues in the same activation. */
		details::ready_issuer<details::publication_frame> yield_value(details::conversion_issuer) noexcept { return {{},*this}; }

		/** @brief Publishes the result before passing control to its consumer. */
		constexpr details::final_awaiter final_suspend() const noexcept
		{
			return {};
		}
	};

	using handle_type = ::std::coroutine_handle<promise_type>;

private:
	handle_type frame{};
	template <typename, typename>
	friend class details::resident_operation;

	/** @brief Takes ownership of a frame during get_return_object. */
	explicit task(handle_type handle) noexcept : frame(handle)
	{}

	/** @brief Transfers frame ownership out of the task, exactly once. */
	handle_type release() noexcept
	{
		auto const handle{frame};
		frame = {};
		return handle;
	}

	friend struct details::run_t;
	friend struct details::run_with_t;
	friend struct details::pending_prefix_write;

protected:
	/** @brief Preserves a creation failure in the same result channel as a running frame. */
	template <typename Owner = task, typename Factory, typename... Args>
	FAST_IO_GNU_ALWAYS_INLINE static Owner materialize(Factory &&create, Args &&...args)
	{
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try
#endif
		{
			return Owner{create(static_cast<Args &&>(args)...)};
		}
#if defined(__HERBCEPTIONS__)
		catch throws(::std::error failure) { return Owner{from_creation_error(failure)}; }
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		catch (...) { return Owner{from_creation_exception()}; }
#endif
	}

	/** @brief Enters a fresh producer whose issuer-owned prefix ends before any external request is submitted. */
	void resume_issue_prefix() noexcept { frame.resume(); }

#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
	/** @brief Builds a result-free frame that carries a failure detected before an operation could start. */
	static task failed_frame()
	{
		if constexpr (::std::is_void_v<T>)
		{
			co_return;
		}
		else
		{
			// Consumption checks the stored error before touching the unconstructed value.
			co_return details::result_already_stored{};
		}
	}

	#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
	/** @brief Preserves the original dynamic exception type when issue-time capture fails. */
	static task from_creation_exception() noexcept
	{
		auto job{failed_frame()};
		job.frame.promise().history.record(details::observation_history::capture_failed, 1);
		job.frame.promise().exception = ::std::current_exception();
		return job;
	}
#endif
#if defined(__HERBCEPTIONS__)
	/** @brief Moves ownership of an eagerly reported deterministic error into a new promise. */
	static task from_creation_error(::std::error &failure)
	{
		auto job{failed_frame()};
		job.frame.promise().history.record(details::observation_history::capture_failed, 1);
		job.frame.promise().unhandled_herbception(failure.release());
		return job;
	}
#endif

	/** @brief Recognizes an owned operation that still requires a scheduler to make progress. */
	bool pending() const noexcept
	{
		return frame && !frame.done();
	}

public:
	/** @brief Transfers ownership without starting or resuming the coroutine. */
	task(task &&other) noexcept : frame(other.release())
	{}

	task(task const &) = delete;
	task &operator=(task const &) = delete;

	/** @brief Destroys the frame and its captured operands. */
	~task()
	{
		if (frame)
		{
			frame.destroy();
		}
	}

	/** @brief Queries an executing frame without advancing it or observing an exception. */
	[[nodiscard]] details::observation_history const *observations() const noexcept
	{ return frame ? ::fast_io::freestanding::addressof(frame.promise().history) : nullptr; }
	/** @brief Installs a stateless checker for a provider that declares mandatory release dependencies. */
	void validate_publications(details::publication_frame::validator f, bool file = false) noexcept
	{
		frame.promise().check_point=f;
		frame.promise().owns_file=file;
	}
	/** @brief Reports terminal state independently of result consumption. */
	[[nodiscard]] bool finished() const noexcept { return !pending(); }
	/** @brief Reports an error without removing it from its original error domain. */
	[[nodiscard]] bool failed() const noexcept { return frame && frame.done() && !frame.promise().consumed && frame.promise().has_error(); }
	/** @brief Joins a submitted root or starts a privately driven lazy or safely parked producer. */
	::std::coroutine_handle<> observe(details::observation_wait &wait, ::std::coroutine_handle<> caller) noexcept
	{
		auto &p{frame.promise()};
		wait.continuation = caller;
		wait.independent = p.observers || details::submitted_root(frame) || static_cast<bool>(p.continuation);
		wait.sibling = p.observers;
		p.observers = ::fast_io::freestanding::addressof(wait);
		if (wait.independent) return ::std::noop_coroutine();
		return p.history.next_entry(frame);
	}

	/** @brief Installs a full-result continuation for a lazy or privately parked operation. */
	::std::coroutine_handle<> join_from(::std::coroutine_handle<> parent) noexcept
	{
		auto &p{frame.promise()};
		if (p.continuation) [[unlikely]] ::fast_io::fast_terminate();
		p.continuation = parent;
		if (details::submitted_root(frame)) return ::std::noop_coroutine();
		return p.history.next_entry(frame);
	}

	/** @brief A named task retains its handle for observation while its sole consumer is suspended. */
	class borrowed_awaiter
	{
		task &owner;
	public:
		explicit borrowed_awaiter(task &job) noexcept : owner(job) {}
		bool await_ready() const noexcept
		{
			if (!owner.frame) [[unlikely]] ::fast_io::fast_terminate();
			return owner.frame.done();
		}
		::std::coroutine_handle<> await_suspend(::std::coroutine_handle<> parent) const noexcept
		{
			return owner.join_from(parent);
		}
		T await_resume() FAST_IO_ASYNC_THROWS { return owner.get(); }
	};

	/** @brief Awaits a named task whose lifetime includes the pending operation. */
	borrowed_awaiter operator co_await() & noexcept { return borrowed_awaiter{*this}; }

	class awaiter
	{
		task owner;
		details::observation_history *parent_history{};

	public:
		/** @brief Holds the child until its result is consumed or its parent is destroyed. */
		explicit awaiter(task &&child) noexcept : owner(static_cast<task &&>(child))
		{}

		/** @brief Allows an already completed task to deliver its result immediately. */
		bool await_ready() const noexcept
		{
			if (!owner.frame) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			return owner.frame.done();
		}

		/** @brief Installs the continuation and starts the child by symmetric transfer. */
		template <typename Parent>
		::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Parent> continuation) noexcept
		{
			if constexpr (requires { continuation.promise().history; })
				parent_history = ::fast_io::freestanding::addressof(continuation.promise().history);
			auto &child{owner.frame.promise()};
			if constexpr (::std::derived_from<Parent,details::publication_frame>)
			{
				auto &parent{continuation.promise()};
				if (!child.forwards_file && !child.check_point && (parent.owns_file || parent.forwards_file))
				{
					child.file_owner = ::fast_io::freestanding::addressof(parent.publication_owner());
					child.forwards_file = true;
				}
			}
			child.continuation = continuation;
			return child.history.next_entry(owner.frame);
		}

		/** @brief Delivers the original error or transfers ownership of the stored result. */
		T await_resume()
#if defined(__HERBCEPTIONS__)
			throws
#endif
		{
			auto &promise{owner.frame.promise()};
			if (parent_history) parent_history->merge_activity(promise.history);
			if (promise.has_error()) [[unlikely]] promise.rethrow();
			return promise.take_value();
		}
	};

	/** @brief Consumes the task as a standard C++ awaitable. */
	awaiter operator co_await() && noexcept
	{
		return awaiter{static_cast<task &&>(*this)};
	}

	/** @brief Drives a standalone task to completion and consumes its result. */
	protected:
	template <bool IssuerOnly, typename Report>
	T consume_result(Report report)
#if defined(__HERBCEPTIONS__)
		throws
#endif
	{
		if (!frame) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if constexpr (IssuerOnly)
		{
			// Fileless conversion cursors cannot submit I/O. A private join has no peer or waiting device.
			if (!frame.done() && !details::current_loop && !frame.promise().observers && !frame.promise().continuation)
				frame.resume();
		}
		if (!frame.done())
		{
			::std::coroutine_handle<> root{frame};
			auto entry{frame.promise().history.next_entry(root)};
			details::run_roots(::fast_io::freestanding::addressof(root), 1, ::fast_io::freestanding::addressof(entry));
		}
		return consume_completed(report);
	}

	/** @brief Consumes a completed child without instantiating a scheduler in an await-resume path. */
	template <typename Report>
	FAST_IO_GNU_ALWAYS_INLINE T consume_completed(Report report) FAST_IO_ASYNC_THROWS
	{
		if (!frame || !frame.done()) [[unlikely]] ::fast_io::fast_terminate();
		if (frame.promise().consumed) [[unlikely]] ::fast_io::fast_terminate();
		report(observations(), failed(), false);
		if (details::submitted_root(frame))
		{
			frame.promise().consumed = true;
			// The enclosing batch still owns a handle in its root array until the issuer returns.
			if (frame.promise().has_error()) [[unlikely]] frame.promise().rethrow();
			return frame.promise().take_value();
		}
		return awaiter{static_cast<task &&>(*this)}.await_resume();
	}
public:
	/** @brief Joins through the configured issuer, retaining platform completion and error delivery. */
	template <typename Report = details::ignore_observations>
	T get(Report report = {}) FAST_IO_ASYNC_THROWS { return consume_result<false>(report); }

};

namespace details
{

/**
 * @brief Gives a public I/O expression a completion boundary when its result is discarded.
 * @details A named operation can be moved, awaited, or batched like any task. An unconsumed statement
 * drives its own coroutine at the end of the full expression, while temporary devices and payloads
 * are still alive. During standard exception unwinding a pending operation is simply released.
 */
template <typename T, bool PrepareValues = false, bool IssuerOnly = false>
class io_operation : public task<T>
{
public:
	/** @brief Takes the frame of the selected implementation without starting it. */
	explicit io_operation(task<T> &&job) noexcept : task<T>(static_cast<task<T> &&>(job))
	{}

	io_operation(io_operation &&) noexcept = default;

	/** @brief Elides the event pump for an isolated cursor-based conversion whose providers cannot await a device. */
	template <typename Report = ignore_observations>
	T get(Report report = {}) FAST_IO_ASYNC_THROWS { return this->template consume_result<IssuerOnly>(report); }

	/** @brief Runs the kernel-only value producer up to its staged write after installing the graph validator. */
	void prepare_values_at_issue() noexcept requires PrepareValues { this->resume_issue_prefix(); }

	/** @brief Creates the public completion boundary around the operation's task. */
	template <typename Factory, typename... Args>
	FAST_IO_GNU_ALWAYS_INLINE static io_operation from_factory(Factory &&create, Args &&...args)
	{
		return task<T>::template materialize<io_operation>(static_cast<Factory &&>(create), static_cast<Args &&>(args)...);
	}

	/** @brief Runs an outstanding statement and delivers any failure through the available destructor ABI. */
	~io_operation() noexcept(false)
	{
		if (!this->pending() && !this->failed())
		{
			return;
		}
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		if (::std::uncaught_exceptions() != 0) [[unlikely]]
		{
			// Unwinding must not start pending I/O or replace the active exception.
			return;
		}
#endif
#if defined(__HERBCEPTIONS__)
		try
		{
			this->get();
		}
		catch throws(::std::error failure)
		{
			// Herbceptions forbids a deterministic throws specification on a destructor.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			failure.throw_legacy_exception();
#else
			::fast_io::fast_terminate();
#endif
		}
#else
		this->get();
#endif
	}
};

/**
 * @brief Constructs the public result through the ordinary C++ return ABI.
 * @details An ordinary class prvalue is destroyed at the end of a discarded expression. Keeping
 * deterministic errors inside that result also delivers an early staging failure to an explicit await.
 */
template <bool PrepareValues = false, bool IssuerOnly = false, typename Factory, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_io_operation(Factory &&create, Args &&...args)
{
	using value_type = typename decltype(create(static_cast<Args &&>(args)...))::result_type;
	using result_type = io_operation<value_type, PrepareValues, IssuerOnly>;
	return result_type::from_factory(static_cast<Factory &&>(create), static_cast<Args &&>(args)...);
}

} // namespace details
} // namespace fast_io::async
