#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_PRINT_OBSERVER_TEST_RESTORE_FLOATING_POINT
#endif
#include <fast_io.h>
#if defined(FAST_IO_PRINT_OBSERVER_TEST_RESTORE_FLOATING_POINT)
#undef FAST_IO_PRINT_OBSERVER_TEST_RESTORE_FLOATING_POINT
#undef FAST_IO_DISABLE_FLOATING_POINT
#endif

namespace
{

struct record
{
	char value{};
};

struct direct_record
{
	direct_record() = default;
	direct_record(direct_record const &) = delete;
	direct_record &operator=(direct_record const &) = delete;
	direct_record(direct_record &&) = default;
	direct_record &operator=(direct_record &&) = default;
};

struct observer_state
{
	::std::size_t copies{};
	::std::size_t records{};
	::std::size_t scatter_calls{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	bool locked{};
};

struct counting_observer
{
	using output_char_type = char;
	observer_state *state{};

	inline explicit constexpr counting_observer(observer_state *value) noexcept : state(value)
	{}

	inline counting_observer(counting_observer const &other) noexcept : state(other.state)
	{
		++state->copies;
	}

	counting_observer &operator=(counting_observer const &) = delete;
};

struct counting_output
{
	counting_observer observer;
};

inline constexpr counting_observer &output_stream_ref_define(counting_output &output) noexcept
{
	return output.observer;
}

struct move_only_observer
{
	using output_char_type = char;
	observer_state *state{};

	inline explicit constexpr move_only_observer(observer_state *value) noexcept : state(value)
	{}
	move_only_observer(move_only_observer const &) = delete;
	move_only_observer &operator=(move_only_observer const &) = delete;
	inline constexpr move_only_observer(move_only_observer &&other) noexcept
		: state(::std::exchange(other.state, nullptr))
	{}
	move_only_observer &operator=(move_only_observer &&) = delete;
};

struct move_only_output
{
	observer_state *state{};
};

inline constexpr move_only_observer output_stream_ref_define(move_only_output &output) noexcept
{
	return move_only_observer{output.state};
}

struct lock_proxy
{
	observer_state *state{};

	inline void lock() noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct locked_observer
{
	using output_char_type = char;
	counting_observer *unlocked{};
	observer_state *state{};
};

struct locked_output
{
	locked_observer observer;
};

inline constexpr locked_observer output_stream_ref_define(locked_output &output) noexcept
{
	return output.observer;
}

inline constexpr lock_proxy output_stream_mutex_ref_define(locked_observer &observer) noexcept
{
	return {observer.state};
}

inline constexpr counting_observer &
output_stream_unlocked_ref_define(locked_observer &observer) noexcept
{
	return *observer.unlocked;
}

template <bool line>
inline void status_print_define(counting_observer &observer, record &)
{
	static_assert(!line);
	++observer.state->records;
}

template <bool line, typename Prefix>
inline void status_print_define(counting_observer &observer, Prefix &, record &)
{
	static_assert(!line);
	++observer.state->records;
}

template <bool line>
inline void status_print_define(move_only_observer &observer, record &)
{
	static_assert(!line);
	++observer.state->records;
}

inline void print_define(
	::fast_io::io_reserve_type_t<char, direct_record>, counting_observer &observer,
	direct_record &) noexcept
{
	++observer.state->records;
}

inline void print_define(
	::fast_io::io_reserve_type_t<char, direct_record>, move_only_observer &observer,
	direct_record &) noexcept
{
	++observer.state->records;
}

// The empty compiled plan never reaches this primitive, but writable admission must still prove the exact final
// output protocol. Keeping the CPO reference-only ensures the test cannot accidentally regain validity through a
// hidden observer copy at the strategy boundary.
inline void scatter_write_all_overflow_define(
	counting_observer &observer, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t)
{
	++observer.state->scatter_calls;
}

inline void scatter_write_all_overflow_define(
	move_only_observer &observer, ::fast_io::basic_io_scatter_t<char> const *, ::std::size_t)
{
	++observer.state->scatter_calls;
}

} // namespace

int main()
{
	observer_state counting_state;
	counting_output counting{counting_observer{__builtin_addressof(counting_state)}};

	// A non-trivial reference-returning CPO denotes the device's existing cursor identity. ABI-aware normalization
	// preserves that lvalue, and every ordinary print layer must borrow it without constructing a surrogate owner.
	::fast_io::operations::print_freestanding<false>(counting, record{'a'});
	assert(counting_state.copies == 0u);
	assert(counting_state.records == 1u);

	counting_state = {};
	::fast_io::io::print(counting, record{'b'});
	assert(counting_state.copies == 0u);
	assert(counting_state.records == 1u);

	// A noncopyable source is normalized to `parameter<T&>`. Its transparent direct-print adapter must borrow the
	// already-normalized observer instead of requiring a trivial observer or creating a discarded cursor surrogate.
	counting_state = {};
	::fast_io::operations::print_freestanding<false>(counting, direct_record{});
	assert(counting_state.copies == 0u);
	assert(counting_state.records == 1u);

	// range_view receives that same stable reference. Its element loop must neither re-normalize the stream nor copy
	// the observer once per element.
	counting_state = {};
	::std::array records{record{'a'}, record{'b'}, record{'c'}};
	auto range{::fast_io::mnp::rgvw(records, "|")};
	decltype(auto) range_output = ::fast_io::operations::output_stream_ref(counting);
	print_define(::fast_io::io_reserve_type<char, decltype(range)>, range_output, range);
	assert(counting_state.copies == 0u);
	assert(counting_state.records == records.size());

	constexpr auto empty_plan{
		::fast_io::make_scatter_plan<char>(::fast_io::mnp::scatter_literal<"">)};

	// A zero-length descriptor exercises compiled-plan output normalization, print_output, and emit without entering
	// the separately owned primitive-write ABI layer. Thus an exact copy count isolates this strategy regression.
	counting_state = {};
	empty_plan.print(counting);
	assert(counting_state.copies == 0u);
	assert(counting_state.scatter_calls == 0u);

	counting_state = {};
	::fast_io::operations::print_freestanding<false>(counting, empty_plan());
	assert(counting_state.copies == 0u);
	assert(counting_state.scatter_calls == 0u);

	counting_state = {};
	::fast_io::io::print(counting, empty_plan());
	assert(counting_state.copies == 0u);
	assert(counting_state.scatter_calls == 0u);

	// Mutex normalization may itself own a prvalue unlocked proxy, but an lvalue result is already stable. Preserving
	// that exact reference avoids a non-trivial copy both in ordinary print recursion and in compiled-plan recursion.
	counting_state = {};
	locked_output locked{{__builtin_addressof(counting.observer),
						  __builtin_addressof(counting_state)}};
	::fast_io::operations::print_freestanding<false>(locked, record{'l'});
	assert(counting_state.copies == 0u);
	assert(counting_state.records == 1u);
	assert(counting_state.locks == 1u && counting_state.unlocks == 1u);
	assert(!counting_state.locked);

	counting_state = {};
	empty_plan.print(locked);
	assert(counting_state.copies == 0u);
	assert(counting_state.scatter_calls == 0u);
	assert(counting_state.locks == 1u && counting_state.unlocks == 1u);
	assert(!counting_state.locked);

	// A move-only prvalue observer proves the same ordinary-print ownership rule at compile time and run time: any
	// reopened by-value helper below output_stream_ref would make either call ill-formed. Compiled-plan emission is
	// tested above with an exact copy count; its eventual primitive writer has a separate transport contract.
	observer_state move_state;
	move_only_output moving{__builtin_addressof(move_state)};
	static_assert(!::std::is_copy_constructible_v<move_only_observer>);
	::fast_io::operations::print_freestanding<false>(moving, record{'x'});
	::fast_io::io::print(moving, record{'y'});
	::fast_io::operations::print_freestanding<false>(moving, direct_record{});
	assert(move_state.records == 3u);
	assert(move_state.scatter_calls == 0u);
}
