#include <concepts>
#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>

#include <fast_io.h>

#if defined(__linux__) || defined(__APPLE__)

namespace
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct rvalue_only_task
{
	int *calls{};

	inline explicit rvalue_only_task(int *counter) noexcept : calls(counter)
	{}

	rvalue_only_task(rvalue_only_task const &) = delete;
	rvalue_only_task(rvalue_only_task &&) = default;

	void operator()() & = delete;

	inline void operator()() && noexcept
	{
		++*calls;
	}
};

struct unique_value_task
{
	int *observed{};

	void operator()(::std::unique_ptr<int> value) & = delete;

	inline void operator()(::std::unique_ptr<int> value) && noexcept
	{
		*observed = *value;
	}
};

struct reference_identity_task
{
	int *expected{};
	bool *same_object{};

	inline void operator()(int &value) && noexcept
	{
		*same_object = __builtin_addressof(value) == expected;
		value = 47;
	}
};

struct lvalue_only_task
{
	void operator()() & noexcept
	{}
};

struct noncopyable_rvalue_task
{
	noncopyable_rvalue_task() = default;
	noncopyable_rvalue_task(noncopyable_rvalue_task const &) = delete;
	noncopyable_rvalue_task(noncopyable_rvalue_task &&) = default;

	void operator()() && noexcept
	{}
};

#ifdef FAST_IO_CPP_EXCEPTIONS
struct decay_copy_failure
{};

struct throwing_decay_value
{
	bool *copy_attempted{};

	inline explicit throwing_decay_value(bool *attempted) noexcept : copy_attempted(attempted)
	{}

	inline throwing_decay_value(throwing_decay_value const &other)
		: copy_attempted(other.copy_attempted)
	{
		*copy_attempted = true;
		throw decay_copy_failure{};
	}

	throwing_decay_value(throwing_decay_value &&) = default;
};

struct throwing_decay_task
{
	inline void operator()(throwing_decay_value) && noexcept
	{}
};
#endif

static_assert(::std::invocable<rvalue_only_task>);
static_assert(::std::constructible_from<::fast_io::native_thread, rvalue_only_task>);

// A caller-side lvalue invocation is insufficient: the owned decay-copy is consumed as an rvalue in the new thread.
static_assert(::std::invocable<lvalue_only_task &>);
static_assert(!::std::constructible_from<::fast_io::native_thread, lvalue_only_task>);

// Even an rvalue-invocable stored type cannot be launched from an lvalue when its decay-copy construction is deleted.
static_assert(::std::invocable<noncopyable_rvalue_task>);
static_assert(!::std::constructible_from<::fast_io::native_thread, noncopyable_rvalue_task &>);

#ifdef FAST_IO_CPP_EXCEPTIONS
// A potentially throwing decay-copy remains a valid launch expression; storage ownership must cover its construction.
static_assert(::std::constructible_from<
	::fast_io::native_thread, throwing_decay_task, throwing_decay_value &>);
#endif

} // namespace

int main()
{
	int calls{};
	{
		::fast_io::native_thread thread{rvalue_only_task{__builtin_addressof(calls)}};
		thread.join();
	}
	require(calls == 1);

	int observed{};
	auto owner{::std::make_unique<int>(29)};
	{
		::fast_io::native_thread thread{
			unique_value_task{__builtin_addressof(observed)}, ::std::move(owner)};
		require(owner == nullptr);
		thread.join();
	}
	require(observed == 29);

	int referred{11};
	bool same_object{};
	{
		::fast_io::native_thread thread{
			reference_identity_task{__builtin_addressof(referred), __builtin_addressof(same_object)},
			::std::ref(referred)};
		thread.join();
	}
	require(same_object);
	require(referred == 47);

#ifdef FAST_IO_CPP_EXCEPTIONS
	bool copy_attempted{};
	throwing_decay_value throwing_value{__builtin_addressof(copy_attempted)};
	bool caught{};
	try
	{
		::fast_io::native_thread thread{throwing_decay_task{}, throwing_value};
		(void)thread;
	}
	catch (decay_copy_failure const &)
	{
		caught = true;
	}
	// No platform thread can have been started: tuple construction failed before the creation API boundary.
	require(copy_attempted && caught);
#endif
}

#else

int main()
{}

#endif
