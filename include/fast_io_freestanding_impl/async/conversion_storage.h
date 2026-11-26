#pragma once

namespace fast_io::async::details
{
/** @brief Selects independent ownership when a conversion can outlive the current consumer. */
struct independent_conversion_frame {};
inline constexpr independent_conversion_frame independent_conversion{};

/** @brief Places the live receipt at the beginning of the private workspace before result and error storage. */
struct conversion_observer
{
	observation_history history{};
};

/** @brief Lends one caller-owned slot for the full dynamic extent of a conversion join.
 * @details The compiler supplies the actual coroutine frame size, including its own spill slots.
 * Alignment padding is charged against the common incremental-state budget. The fallback is
 * released by this owner after the coroutine destroys its parameters, locals and promise.
 */
template <typename T, typename Policy = ::fast_io::details::default_print_stack_policy>
struct conversion_frame_storage : conversion_observer, task_error, task_value<T>
{
	static constexpr auto capacity{::fast_io::details::context_inline_state_budget_bytes<Policy>()};
	alignas(::std::max_align_t) ::std::byte bytes[capacity ? capacity : 1u];
	void *spill{};

	conversion_frame_storage() noexcept = default;
	conversion_frame_storage(conversion_frame_storage const &) = delete;

	/** @brief Grants correctly aligned storage only if the complete compiler request fits. */
	void *acquire(::std::size_t size, ::std::size_t alignment) noexcept
	{
		auto const address{reinterpret_cast<::std::uintptr_t>(bytes)};
		auto const padding{static_cast<::std::size_t>(-address) & (alignment - 1u)};
		if (padding <= capacity && size <= capacity - padding) [[likely]] return bytes + padding;
		return spill = allocate_frame(size,alignment);
	}

	/** @brief Reclaims just the possible alignment or size overflow after the join returns. */
	~conversion_frame_storage() { if (spill) deallocate_frame(spill); }
};

/** @brief Routes co_return into the caller's result slot, including move-only and void results. */
template <typename T>
struct conversion_return
{
	conversion_frame_storage<T> &storage;
	template <typename V>
	void return_value(V &&v) FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		!FAST_IO_HERBCEPTIONS_NOTHROWS(storage.return_value(static_cast<V &&>(v))),
		noexcept(storage.return_value(static_cast<V &&>(v))))
	{ storage.return_value(static_cast<V &&>(v)); }
};
template <>
struct conversion_return<void>
{
	conversion_frame_storage<void> &storage;
	void return_void() const noexcept {}
};

template <typename Graph, typename Owner> struct conversion_publication;

/** @brief Executes the ordinary coroutine body with statically ready awaiters for an exclusive, complete join.
 * @details A private result has a single issuer and no outstanding subscription. Every typed publication
 * is still validated and saved. With both end suspensions ready, even the handle never escapes into
 * the returned token. The leading allocator also supports compilers that retain a physical frame.
 */
template <typename T, ::std::size_t RequiredAlignment>
struct scoped_conversion_task
{
	struct promise_type : conversion_return<T>
	{
		/** @brief Binds result, error and observation lifetimes to the consumer enclosing this call. */
		template <typename... Args>
		explicit promise_type(conversion_frame_storage<T> &storage, Args &...) noexcept : conversion_return<T>{storage} {}
		/** @brief Includes alignments of cursor locals even on compilers that only report frame size. */
		template <typename... Args>
		static void *operator new(::std::size_t bytes, conversion_frame_storage<T> &storage, Args &...)
		{
			constexpr auto alignment{alignof(promise_type) > RequiredAlignment ? alignof(promise_type) : RequiredAlignment};
			return storage.acquire(bytes,alignment < alignof(::std::max_align_t) ? alignof(::std::max_align_t) : alignment);
		}
#if defined(__cpp_aligned_new) && !defined(_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION) && \
	(!defined(_LIBCPP_HAS_LIBRARY_ALIGNED_ALLOCATION) || _LIBCPP_HAS_LIBRARY_ALIGNED_ALLOCATION)
		/** @brief Honors stronger compiler-provided alignment on ABIs that expose it. */
		template <typename... Args>
		static void *operator new(::std::size_t bytes, ::std::align_val_t align, conversion_frame_storage<T> &storage, Args &...)
		{
			auto alignment{static_cast<::std::size_t>(align)};
			return storage.acquire(bytes,alignment < RequiredAlignment ? RequiredAlignment : alignment);
		}
#endif
		/** @brief The enclosing storage owner survives parameter, local and promise destruction. */
		static void operator delete(void *, ::std::size_t) noexcept {}
		/** @brief Enters and retires this coroutine within the complete join. */
		constexpr ::std::suspend_never initial_suspend() const noexcept { return {}; }
		constexpr ::std::suspend_never final_suspend() const noexcept { return {}; }
		constexpr scoped_conversion_task get_return_object() const noexcept { return {}; }
		/** @brief Preserves the dynamic standard exception for the consuming call. */
		void unhandled_exception() noexcept { this->storage.unhandled_exception(); }
#if defined(__HERBCEPTIONS__)
		/** @brief Transfers the deterministic error payload into the same owning result. */
		void unhandled_herbception(::std::cxx_std_error failure) noexcept { this->storage.unhandled_herbception(failure); }
#endif
		/** @brief Exposes the caller-owned history without forcing the compiler to spill a promise handle. */
		ready_issuer<conversion_frame_storage<T>> yield_value(conversion_issuer) noexcept { return {{},this->storage}; }
		/** @brief Commits a release or start before the next provider step can run. */
		template <typename Graph, typename Owner>
		FAST_IO_GNU_ALWAYS_INLINE ::std::suspend_never await_transform(conversion_publication<Graph,Owner> const &event) noexcept
		{
			event.record();
			return {};
		}
		/** @brief Stores a structured scanner failure without constructing an intermediate exception. */
		::std::suspend_never await_transform(store_native_error failure) noexcept
		{
			this->storage.native = failure.error;
			return {};
		}
	};
};

/** @brief Owns the issue-time snapshots while their small conversion can still be moved or returned.
 * @details Only a complete private join lends stack memory. A point subscription, type erasure or
 * independent submission can leave a live handle after the current call and receives stable storage.
 */
template <typename T, typename Graph, typename Plan>
struct conversion_work : issue_slot<Plan>
{
	using publication_graph = Graph;
	observation_history const *joining_history{};
	conversion_work() noexcept = default;
	/** @brief Relocates only ordinary C++ captures. A parked coroutine keeps its independent address. */
	conversion_work(conversion_work &&other) noexcept
	{
		if (other.active)
		{
			::new (static_cast<void *>(::fast_io::freestanding::addressof(this->value))) Plan(static_cast<Plan &&>(other.value));
			this->active = true;
		}
	}
	/** @brief Constructs captures directly in the owner, retaining the existing issue-time error boundary. */
	template <typename Make>
	void stage(Make &make) FAST_IO_ASYNC_THROWS
	{
		::new (static_cast<void *>(::fast_io::freestanding::addressof(this->value))) Plan(make());
		this->active = true;
	}
	/** @brief A ready peer must be able to run between selected stages. */
	static bool can_join_inline() noexcept { return !has_layer_peer(); }
	/** @brief Creates the ordinary separately owned coroutine when an issuer needs a stable root. */
	auto operator()(::std::false_type) FAST_IO_ASYNC_THROWS { return this->value(independent_conversion); }
	/** @brief Lends one bounded slot for all private stages and retires their receipt at the complete join. */
	template <typename Report = ignore_observations>
	FAST_IO_GNU_ALWAYS_INLINE T consume(Report report = {}) FAST_IO_ASYNC_THROWS
	{
		conversion_frame_storage<T> storage;
		// A user step can inspect its own operation while this private join is on the call stack.
		struct lease
		{
			conversion_work &owner;
			~lease()
			{
				owner.joining_history = nullptr;
				owner.active = false;
				owner.value.~Plan();
			}
		} release{*this};
		joining_history = ::fast_io::freestanding::addressof(storage.history);
		this->value(storage);
		report(::fast_io::freestanding::addressof(storage.history),storage.has_error(),false);
		storage.rethrow();
		return storage.take_value();
	}
};
} // namespace fast_io::async::details
