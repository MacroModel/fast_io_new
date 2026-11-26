#pragma once

namespace fast_io::async::details
{
/** @brief Records a sparse publication only when its depth exceeds the promise's inline capacity. */
struct observation_block
{
	observation_block *next;
	::std::size_t index;
	::std::uintptr_t bits;
	::std::coroutine_handle<> parked{};
};

/** @brief Stores the first group of two-bit publications by value after a short operation releases its frame. */
struct inline_publications
{
	static constexpr unsigned width{::std::numeric_limits<::std::uintptr_t>::digits / 2u};
	::std::uintptr_t bits;

	/** @brief Reads a slot proved to fit by the operation schema. */
	constexpr unsigned lookup(::std::size_t slot) const noexcept
	{
		return static_cast<unsigned>((bits >> (2u * slot)) & 3u);
	}
};

/**
 * @brief Stores early retirements in the producer promise. Common graphs use one machine word.
 * @details Slots belong to a path, not a process-wide enum. Additional words are allocated on demand
 * only when a deep provider actually publishes early. Final completion requires no publication per point.
 */
class observation_history
{
	using allocator = ::fast_io::typed_generic_allocator_adapter<::fast_io::native_global_allocator, observation_block>;
	static constexpr unsigned width{inline_publications::width};
	::std::uintptr_t first{};
	observation_block *rest{};

public:
	static constexpr ::std::size_t capture_failed{0}, input_closed{1}, output_closed{2}, resident{3}, tracked_input{4}, tracked_output{5};
	/** @brief Maps two equal-depth endpoint instances to distinct slots without an ABI limit on layer count. */
	static constexpr ::std::size_t key(observable_point p) noexcept
	{
		return first_point_slot + 5u * p.depth + (p.side == point_side::argument ? 4u : 2u * (p.side == point_side::output));
	}
	observation_history() noexcept = default;
	observation_history(observation_history const &) = delete;
	/** @brief Copies the short receipt before frame destruction without retaining ownership of spill blocks. */
	[[nodiscard]] inline_publications inline_receipt(bool local) const noexcept
	{
		return {first | (static_cast<::std::uintptr_t>(local) << (2u * resident))};
	}
	/** @brief Parks the exact child at a private observation. Only a nested stop allocates off the hot path. */
	void park(::std::coroutine_handle<> child) noexcept
	{
		if (!rest)
		{
			auto *p{allocator::allocate(1)};
			rest = ::new(static_cast<void *>(p)) observation_block{nullptr,1,0};
		}
		rest->parked = child;
	}
	/** @brief Returns the next permitted resume address while retaining the public root for completion checks. */
	::std::coroutine_handle<> next_entry(::std::coroutine_handle<> root) noexcept
	{
		if (!rest || !rest->parked) return root;
		auto next{rest->parked};
		rest->parked = {};
		return next;
	}
	/** @brief Releases only spill words that the producer actually populated. */
	~observation_history()
	{
		while (rest)
		{
			auto *block{rest};
			rest = block->next;
			allocator::deallocate_n(block, 1);
		}
	}
	/** @brief Returns zero for an unpublished point, one for a release, or two for a proved unused point. */
	[[nodiscard]] unsigned lookup(::std::size_t slot) const noexcept
	{
		auto const index{slot / width};
		auto word{first};
		if (index)
		{
			auto *p{rest};
			while (p && p->index != index) p = p->next;
			if (!p) return 0;
			word = p->bits;
		}
		return static_cast<unsigned>((word >> (2u * (slot % width))) & 3u);
	}
	/** @brief Publishes one irreversible fact. A provider may repeat the same fact. */
	void record(::std::size_t slot, unsigned value) noexcept
	{
		auto const index{slot / width};
		auto *word{::fast_io::freestanding::addressof(first)};
		if (index)
		{
			auto **at{::fast_io::freestanding::addressof(rest)};
			while (*at && (*at)->index != index) at = ::fast_io::freestanding::addressof((*at)->next);
			if (!*at)
			{
				auto *p{allocator::allocate(1)};
				::new (static_cast<void *>(p)) observation_block{nullptr, index, 0};
				*at = p;
			}
			word = ::fast_io::freestanding::addressof((*at)->bits);
		}
		auto const shift{2u * (slot % width)};
		auto const previous{(*word >> shift) & 3u};
		if (previous && previous != value) [[unlikely]] ::fast_io::fast_terminate();
		*word |= static_cast<::std::uintptr_t>(value) << shift;
	}
	/** @brief Merges a worker's private receipt only after its issuer has acquired the completion. */
	void received(file_activity const &activity) noexcept
	{
		for (unsigned s{}; s!=2; ++s)
		{
			if (!activity.tracked[s]) continue;
			record(s ? tracked_output : tracked_input,1);
			for (::std::size_t d{1};d<=activity.reached[s];++d)
				record(key({s ? point_side::output : point_side::input,d})+1u,1);
		}
	}
	/** @brief Carries real backing participation from an internal child to the same operation's issuer. */
	void merge_activity(observation_history const &child) noexcept
	{
		for (auto side : {point_side::input,point_side::output})
		{
			auto name{side == point_side::input ? tracked_input : tracked_output};
			if (!child.lookup(name)) continue;
			record(name,1);
			for (::std::size_t d{1};;++d)
			{
				auto slot{key({side,d})+1u};
				if (!child.lookup(slot)) break;
				record(slot,1);
			}
		}
	}

};

/** @brief Holds a non-consuming subscription entirely in the awaiting caller's frame. */
struct observation_wait : request_link
{
	observation_wait *sibling{};
	bool (*satisfied)(observation_wait const &, observation_history const &) noexcept {};
	bool independent{};
};

/** @brief Retains the graph checker on a public root or borrows that root for nested device publications. */
struct publication_frame
{
	::std::coroutine_handle<> continuation{};
	observation_wait *observers{};
	observation_history history{};
	using validator = ::std::size_t (*)(observation_history const &,observable_point,unsigned,bool) noexcept;
	union { validator check_point{}; publication_frame *file_owner; };
	bool consumed{}, forwards_file{}, owns_file{};

	/** @brief Resolves the address at which a selected provider must publish a file start. */
	publication_frame &publication_owner() noexcept { return forwards_file ? *file_owner : *this; }
};
/** @brief Wakes each satisfied observer exactly once; a privately driven producer may park at a safe boundary. */
inline ::std::coroutine_handle<> notify_observers(observation_wait *&head, observation_history const &history, bool terminal) noexcept
{
	::std::coroutine_handle<> driver{};
	auto **at{::fast_io::freestanding::addressof(head)};
	while (*at)
	{
		auto *item{*at};
		if (!terminal && !item->satisfied(*item, history))
		{
			at = ::fast_io::freestanding::addressof(item->sibling);
			continue;
		}
		*at = item->sibling;
		if (item->independent) current_loop->ready.push(*item);
		else driver = item->continuation;
	}
	return driver;
}

/** @brief Records actual kernel admission on the issuer. Only independently running operations can wake a peer mid-request. */
template <typename Promise>
inline void native_admitted(Promise &promise,bool write,bool wake) noexcept
{
	auto slot{observation_history::key({write ? point_side::output : point_side::input,0})+1u};
	if (promise.history.lookup(slot)) return;
	promise.history.record(slot,1);
	if (!wake) return;
	auto **at{::fast_io::freestanding::addressof(promise.observers)};
	while (*at)
	{
		auto *wait{*at};
		if (!wait->independent || !wait->satisfied(*wait,promise.history)) {at=::fast_io::freestanding::addressof(wait->sibling);continue;}
		// The kernel still owns the submitted request. Resume only its observer, on the issuer queue.
		*at=wait->sibling;
		current_loop->ready.push(*wait);
	}
}

/** @brief Publishes from the issuer after the provider has relinquished the advertised resource. */
struct publish_observation
{
	::std::size_t slot;
	unsigned value;
	constexpr bool await_ready() const noexcept { return false; }
	template <typename P>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<P> handle) const noexcept
	{
		auto &p{handle.promise()};
		p.history.record(slot, value);
		if (p.observers)
			if (auto driver{notify_observers(p.observers,p.history,false)}) return driver;
		return handle;
	}
	constexpr void await_resume() const noexcept {}
};

/** @brief Checks a provider publication against the bound operation before releasing any observer. */
struct publish_point
{
	observable_point point;
	unsigned value;
	bool starting{};
	constexpr bool await_ready() const noexcept { return false; }
	template <typename Promise>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> h) const noexcept
	{
		auto &p{h.promise()};
		auto const check{p.forwards_file ? nullptr : p.check_point};
		auto const slot{check ? check(p.history,point,value,starting) : observation_history::key(point)};
		if (slot==static_cast<::std::size_t>(-1) || (starting && !check) ||
			(point.local && (!check || p.history.lookup(slot)))) [[unlikely]]
			::fast_io::fast_terminate();
		return publish_observation{slot,value}.await_suspend(h);
	}
	constexpr void await_resume() const noexcept {}
};

/** @brief Reports a protocol stage on the public root the first time the provider reaches or skips it.
 * @details When a private observer parks a nested request, the root retains the leaf continuation.
 * A later record can revisit the same stage without changing the first receipt.
 */
struct layer_start
{
	observable_point point;
	bool used;
	constexpr bool await_ready() const noexcept { return false; }
	template <typename Promise>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> h) const noexcept
	{
		auto &owner{h.promise().publication_owner()};
		if (!owner.owns_file || !owner.check_point) ::fast_io::fast_terminate();
		auto slot{observation_history::key(point)};
		if (owner.history.lookup(slot)) return h;
		unsigned const value{used ? 1u : 2u};
		if (owner.check_point(owner.history,point,value,used) != slot) ::fast_io::fast_terminate();
		owner.history.record(slot,value);
		if (owner.observers)
			if (auto driver{notify_observers(owner.observers,owner.history,false)})
			{
				if (&owner != static_cast<publication_frame *>(&h.promise())) owner.history.park(h);
				return driver;
			}
		return h;
	}
	constexpr void await_resume() const noexcept {}
};

/** @brief Discards diagnostic progress when an ordinary task has no public observation contract. */
struct ignore_observations
{
	constexpr void operator()(observation_history const *, bool, bool) const noexcept {}
};
} // namespace fast_io::async::details

namespace fast_io::async
{
/** @brief Publishes the first entry into a selected stage of a custom I/O coroutine on its issuer.
 * @details The provider has begun the work and retains all outstanding resource leases. Subsequent
 * suspensions, retries and completion belong to that provider. A waiter can resume before any result exists.
 */
[[nodiscard]] inline constexpr auto start_point(observable_point point) noexcept
{
	return details::publish_point{point,1,true};
}
/** @brief Publishes an independently released resource in a custom producer coroutine. */
[[nodiscard]] inline constexpr auto complete_point(observable_point point) noexcept
{
	return details::publish_point{point,1};
}
/** @brief Publishes a proved untaken branch without reporting a successful device request. */
[[nodiscard]] inline constexpr auto unused_point(observable_point point) noexcept
{
	return details::publish_point{point,2};
}
/** @brief Records the failing boundary without consuming its original error. The provider must still deliver that error. */
[[nodiscard]] inline constexpr auto failed_point(observable_point point) noexcept
{
	return details::publish_point{point,3};
}
} // namespace fast_io::async
