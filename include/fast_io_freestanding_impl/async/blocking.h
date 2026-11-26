#pragma once

namespace fast_io::async::details
{

/** @brief Couples an operation with a non-consuming test for issuer-local completion. */
template <typename Operation, typename Ready>
struct conditional_work
{
	Operation operation;
	Ready ready;

	/** @brief Executes the original operation once, after the scheduling decision. */
	decltype(auto) operator()() FAST_IO_ASYNC_THROWS
	{
		return operation();
	}
};

/** @brief Stores an opaque operation's result directly in its own promise. */
template <typename Function, typename Promise>
struct blocking_awaiter : work_request
{
	Function *function;
	Promise *promise{};
	file_activity activity{};

	/** @brief Captures the callable already owned by this coroutine frame. */
	explicit blocking_awaiter(Function &operation) noexcept
		: function(::fast_io::freestanding::addressof(operation))
	{
		execute = [](work_request &item) noexcept { static_cast<blocking_awaiter &>(item).invoke(); };
	}

	/** @brief Completes the operation and preserves the original exception state. */
	void invoke() noexcept
	{
		file_activity_scope observe{activity};
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try
#endif
		{
			if constexpr (::std::is_void_v<::std::invoke_result_t<Function &>>)
			{
				(*function)();
			}
			else
			{
				promise->return_value((*function)());
			}
		}
#if defined(__HERBCEPTIONS__)
		catch throws(::std::error failure)
		{
			promise->unhandled_herbception(failure.release());
		}
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		catch (...)
		{
			promise->unhandled_exception();
		}
#endif
	}

	/** @brief Enters await_suspend to obtain the promise even on threadless targets. */
	constexpr bool await_ready() const noexcept
	{
		return false;
	}

	/** @brief Dispatches the callable to the issuer's worker or executes it inline. */
	bool await_suspend(::std::coroutine_handle<Promise> handle) noexcept
	{
		continuation = handle;
		promise = ::fast_io::freestanding::addressof(handle.promise());
		if (current_loop != nullptr && current_loop->standalone_root != handle && current_loop->submit_work != nullptr)
		{
			if constexpr (requires { { function->ready() } noexcept -> ::std::same_as<bool>; })
			{
				if (function->ready())
				{
					invoke();
					return false;
				}
			}
			if (current_loop->submit_work(current_loop->context, *this))
			{
				return true;
			}
		}
		invoke();
		return false;
	}

	/** @brief Leaves errors in the promise for delivery at the public await boundary. */
	void await_resume() const noexcept
	{ promise->history.received(activity); }
};

/** @brief Runs the existing synchronous engine under the async completion protocol. */
template <typename Function>
inline task<::std::invoke_result_t<Function &>> invoke_blocking(Function function) FAST_IO_ASYNC_THROWS
{
	using result = ::std::invoke_result_t<Function &>;
	using promise = typename task<result>::promise_type;
	co_await blocking_awaiter<Function, promise>{function};
	if constexpr (::std::is_void_v<result>)
	{
		co_return;
	}
	else
	{
		co_return result_already_stored{};
	}
}

/** @brief Executes a proved immediate operation at first resume, preserving deferred arguments and task errors. */
template <typename Function>
inline task<::std::invoke_result_t<Function &>> invoke_immediate(Function function) FAST_IO_ASYNC_THROWS
{
	co_return function();
}

/**
 * @brief Executes a wholly immediate device operation at result consumption, preserving its error channel.
 * @details Readiness is independent of input data and has no side effects. A standalone get or an awaiting
 * parent calls the original scanner or transfer exactly once. Explicit root scheduling transfers the callable
 * to a real coroutine so its result and errors remain available after the event loop returns.
 */
template <typename Function>
struct immediate_call
{
	Function function;

	/** @brief Advertises that this device never waits. */
	constexpr bool operator()(::std::true_type) const noexcept
	{
		return true;
	}

	/** @brief Transfers explicit root ownership into the existing coroutine ABI. */
	auto operator()(::std::false_type) FAST_IO_ASYNC_THROWS
	{
		return invoke_immediate(static_cast<Function &&>(function));
	}

	/** @brief Executes only after the caller commits to observing success or failure. */
	FAST_IO_GNU_ALWAYS_INLINE decltype(auto) consume() FAST_IO_ASYNC_THROWS
	{
		return function();
	}
};

/** @brief Keeps an immediate call in an awaiting parent until explicit root scheduling needs a child frame. */
template <typename Function>
FAST_IO_GNU_ALWAYS_INLINE inline auto make_immediate_call(Function function)
{
	using work = immediate_call<Function>;
	return resident_operation<::std::invoke_result_t<Function &>, work>{
		::std::in_place, work{static_cast<Function &&>(function)}};
}

/** @brief Uses the device's direction-specific progress guarantee to select the coroutine's execution context. */
template <bool Immediate, typename Function>
inline auto invoke_device(Function function) FAST_IO_ASYNC_THROWS
{
	if constexpr (Immediate)
	{
		return invoke_immediate(static_cast<Function &&>(function));
	}
	else
	{
		return invoke_blocking(static_cast<Function &&>(function));
	}
}

} // namespace fast_io::async::details
