#pragma once

namespace fast_io::async::details
{
/**
 * @brief Spreads simultaneously runnable local file copies across the CPU set inherited by the worker.
 * @details A batch narrower than its allowed set leaves placement to the scheduler. A saturated set
 * gives each worker a distinct starting CPU before wrapping. Successive file jobs reuse that CPU.
 * The worker restores its inherited set before executing a different operation. An unavailable syscall or unusually large mask
 * declines the optimization without affecting the file operation.
 */
class file_cpu_placement
{
	using word = unsigned long;
	static inline constexpr ::std::size_t words{128u / sizeof(word)};
	word original[words];
	::std::size_t restore_bytes{}, available{};

public:
	/** @brief Binds this file operation only if the batch can occupy every CPU the kernel reported. */
	void select(::std::size_t roots, ::std::size_t worker) noexcept
	{
		if (restore_bytes && available && roots >= available)
		{
			return;
		}
		if (restore_bytes)
		{
			release();
			if (restore_bytes)
			{
				// A failed restoration must not overwrite the only copy of the original mask.
				return;
			}
		}
		if (available == 1 || roots < 2)
		{
			return;
		}
		auto bytes{::fast_io::system_call<__NR_sched_getaffinity, ::std::ptrdiff_t>(0, sizeof(original), original)};
		if (bytes <= 0 || static_cast<::std::size_t>(bytes) > sizeof(original))
		{
			return;
		}
		auto const n{static_cast<::std::size_t>(bytes) / sizeof(word)};
		::std::size_t count{};
		for (::std::size_t i{}; i != n; ++i)
		{
			count += static_cast<unsigned>(::std::popcount(original[i]));
		}
		available = count;
		if (count < 2 || roots < count)
		{
			return;
		}
		auto index{worker % count};
		word selected[words]{};
		for (::std::size_t i{}; i != n; ++i)
		{
			auto mask{original[i]};
			auto const bits{static_cast<unsigned>(::std::popcount(mask))};
			if (index >= bits)
			{
				index -= bits;
				continue;
			}
			while (index != 0)
			{
				mask &= mask - 1u;
				--index;
			}
			selected[i] = mask & (0ul - mask);
			if (::fast_io::system_call<__NR_sched_setaffinity, int>(0, bytes, selected) == 0)
			{
				restore_bytes = static_cast<::std::size_t>(bytes);
			}
			return;
		}
	}

	file_cpu_placement() noexcept = default;
	file_cpu_placement(file_cpu_placement const &) = delete;
	file_cpu_placement &operator=(file_cpu_placement const &) = delete;

	/** @brief Restores the worker's original placement before it runs opaque code or loses its parallel file workload. */
	void release() noexcept
	{
		available = 0;
		if (restore_bytes)
		{
			// The kernel intersects the saved mask with any newer cpuset restriction.
			if (::fast_io::system_call<__NR_sched_setaffinity, int>(0, restore_bytes, original) == 0)
			{
				restore_bytes = 0;
			}
		}
	}
};
} // namespace fast_io::async::details
