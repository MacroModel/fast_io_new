#pragma once

#include "../../fast_io_device.h"
#include <array>
#include <atomic>
#include <optional>
#include <utility>

namespace fast_io::async::details
{

/**
 * @brief Drives one batch of independent, unstarted root tasks on a reusable hosted thread.
 * @details One producer owns the issuer and submits the next batch after taking the previous one.
 * Moving the roots transfers frame ownership. References captured inside a root must remain valid
 * and safe to use on the chosen thread through completion. Issue-time work already performed by
 * a public I/O call stays on its original thread. A factory that needs a background open therefore
 * places the open in the body of its lazy root. Each root retains its original result and error ABI.
 *
 * Publishing queued releases the initialized frames to the worker. Publishing idle releases the
 * completed promises to the producer. Only the producer changes the storage between these two
 * transfers. Destruction joins an outstanding batch before releasing its frames.
 */
template <typename Thread, typename T, ::std::size_t Count>
requires (Count != 0)
class native_batch_issuer
{
	using batch_type = ::std::array<::fast_io::async::task<T>, Count>;
	enum class state : unsigned { idle, queued, stopped };
	::std::atomic<state> phase{state::idle};
	::std::optional<batch_type> batch;
	Thread worker;

	/** @brief Drives all roots on the same native event loop until every promise has reached final suspend. */
	void serve() noexcept
	{
		for (;;)
		{
			phase.wait(state::idle, ::std::memory_order_acquire);
			if (phase.load(::std::memory_order_relaxed) == state::stopped)
			{
				return;
			}
			[&]<::std::size_t... I>(::std::index_sequence<I...>) {
				run((*batch)[I]...);
			}(::std::make_index_sequence<Count>{});
			phase.store(state::idle, ::std::memory_order_release);
			phase.notify_one();
		}
	}

public:
	native_batch_issuer() = default;
	native_batch_issuer(native_batch_issuer const &) = delete;
	native_batch_issuer &operator=(native_batch_issuer const &) = delete;

	/** @brief Publishes one owned batch. First use charges native thread creation to the producer. */
	void submit(batch_type &&roots)
	{
		if (batch.has_value()) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if (!worker.joinable())
		{
			// A failure to create the thread leaves the caller's roots intact.
			worker = Thread{[this]() noexcept { serve(); }};
		}
		batch.emplace(static_cast<batch_type &&>(roots));
		phase.store(state::queued, ::std::memory_order_release);
		phase.notify_one();
	}

	/** @brief Returns completed roots to their owner for ordinary result and exception consumption. */
	[[nodiscard]] batch_type take()
	{
		if (!batch) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		phase.wait(state::queued, ::std::memory_order_acquire);
		batch_type ready{static_cast<batch_type &&>(*batch)};
		batch.reset();
		return ready;
	}

	/** @brief Drains an unconsumed batch before stopping the thread and destroying its promises. */
	~native_batch_issuer()
	{
		if (worker.joinable())
		{
			phase.wait(state::queued, ::std::memory_order_acquire);
			phase.store(state::stopped, ::std::memory_order_release);
			phase.notify_one();
			worker.join();
		}
	}
};

} // namespace fast_io::async::details
