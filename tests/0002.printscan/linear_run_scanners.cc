#include <fast_io.h>

namespace print_linear_run_scanners_test
{
namespace reference
{
using namespace ::fast_io::details::decay;

// Independent copies of the original right-recursive transitions. These intentionally do not consume the candidate
// descriptors, so category priority, stop behavior, overflow resets and tail flags all have a separate oracle.
template <bool retain_static_scatter, ::std::integral char_type, typename Arg, typename... Args>
inline constexpr contiguous_scatter_result find_continuous_scatters_n_impl()
{
	contiguous_scatter_result ret{};
	using value_type = ::std::remove_cvref_t<Arg>;
	constexpr bool static_scatter{
		::fast_io::details::decay::print_static_scatter_traits<char_type, value_type>::available};
	if constexpr (
		::fast_io::details::decay::retained_scatter_printable_v<char_type, Arg &> ||
		(retain_static_scatter && static_scatter))
	{
		// A scatter-printable argument contributes one existing output range to the contiguous run.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so the current argument can extend the leading run.
			ret = find_continuous_scatters_n_impl<retain_static_scatter, char_type, Args...>();
		}
		constexpr ::std::size_t one{1u};
		::std::size_t const neededscatters{
			::fast_io::details::decay::print_strategy_extent_add_or_unavailable(ret.neededscatters, one)};
		if (neededscatters == SIZE_MAX)
		{
			// Descriptor aggregation is optional.  Splitting here lets the dispatcher emit the current object and retry
			// the tail instead of instantiating an unrepresentable retained descriptor array.
			return {};
		}
		++ret.position;
		ret.hasscatters = true;
		ret.neededscatters = neededscatters;
	}
	else if constexpr (::fast_io::reserve_printable<char_type, Arg>)
	{
		// A static reserve-printable argument contributes one materialized range and a known reserve size.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first to preserve the aggregate run accounting.
			ret = find_continuous_scatters_n_impl<retain_static_scatter, char_type, Args...>();
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(sz != 0);
		constexpr ::std::size_t one{1u};
		::std::size_t const neededscatters{
			::fast_io::details::decay::print_strategy_extent_add_or_unavailable(ret.neededscatters, one)};
		::std::size_t const neededspace{
			::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
				ret.neededspace, sz)};
		if (neededscatters == SIZE_MAX || neededspace == SIZE_MAX)
		{
			// Every individual reserve producer remains valid; only this contiguous combination is declined.
			return {};
		}
		if constexpr (sizeof...(Args) == 0)
		{
			// A trailing reserve argument can be emitted directly without an extra scatter continuation.
			ret.lastisreserve = true;
		}
		++ret.position;
		ret.neededscatters = neededscatters;
		ret.neededspace = neededspace;
		ret.hasreserve = true;
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, Arg>)
	{
		// A dynamic reserve-printable argument is part of the run, but its reserve size is measured later.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so this dynamic reserve extends the same leading run.
			ret = find_continuous_scatters_n_impl<retain_static_scatter, char_type, Args...>();
		}
		constexpr ::std::size_t one{1u};
		::std::size_t const neededscatters{
			::fast_io::details::decay::print_strategy_extent_add_or_unavailable(ret.neededscatters, one)};
		if (neededscatters == SIZE_MAX)
		{
			return {};
		}
		if constexpr (sizeof...(Args) == 0)
		{
			// A trailing dynamic reserve argument can be handled as the final materialized output.
			ret.lastisreserve = true;
		}
		++ret.position;
		ret.neededscatters = neededscatters;
		ret.hasdynamicreserve = true;
	}
	else if constexpr (
		::fast_io::reserve_scatters_printable<char_type, Arg> &&
		::fast_io::details::decay::retained_reserve_scatters_printable_v<char_type, Arg>)
	{
		// A reserve-scatters argument contributes its declared scatter count and reserve storage requirement.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first to accumulate the complete leading run.
			ret = find_continuous_scatters_n_impl<retain_static_scatter, char_type, Args...>();
		}
		constexpr auto scatszres{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(scatszres.scatters_size != 0);
		::std::size_t const neededspace{
			::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
				ret.neededspace, scatszres.reserve_size)};
		::std::size_t const neededscatters{
			::fast_io::details::decay::print_strategy_extent_add_or_unavailable(ret.neededscatters,
																				scatszres.scatters_size)};
		if (neededspace == SIZE_MAX || neededscatters == SIZE_MAX)
		{
			// A retained reserve-scatter plan is only a batching optimization; an oversized aggregate is split safely.
			return {};
		}
		ret.hasscatters = true;
		ret.hasreserve = true;
		++ret.position;
		ret.neededspace = neededspace;
		ret.neededscatters = neededscatters;
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, ::fast_io::io_null_t>)
	{
		// Null output is counted so pack positions remain correct while no emitted characters are reserved.
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before adding this null position to the run.
			ret = find_continuous_scatters_n_impl<retain_static_scatter, char_type, Args...>();
		}
		::std::size_t const null_count{
			::fast_io::details::decay::print_strategy_saturating_add(ret.null, static_cast<::std::size_t>(1u))};
		if (null_count == SIZE_MAX)
		{
			return {};
		}
		++ret.position;
		ret.null = null_count;
	}
	else if constexpr (::fast_io::printable<char_type, Arg>)
	{
		// A generic printable argument stops the scatter/reserve run because it needs the normal emit path.
	}
	return ret;
}


template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr context_capture_run_result find_context_capture_run_n()
{
	using nocvreft = ::std::remove_cvref_t<Arg>;
	if constexpr (::fast_io::reserve_printable<char_type, nocvreft>)
	{
		// Static reserve output can be accumulated into the leading contiguous reserve burst.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so this reserve output extends the front of the run.
			ret = ::print_linear_run_scanners_test::reference::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		static_assert(sz != 0);
		::std::size_t const leading_burst{
			::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
				ret.leading_static_reserve_burst_size, sz)};
		if (leading_burst == SIZE_MAX)
		{
			// Context capture is an optional coalescing plan.  Declining this prefix preserves each producer's valid
			// individual protocol and prevents an impossible aggregate array from becoming an NTTP.
			return {};
		}
		++ret.position;
		ret.leading_static_reserve_burst_size = leading_burst;
		if (ret.max_static_reserve_burst_size < ret.leading_static_reserve_burst_size)
		{
			// The current static reserve burst is the largest contiguous reserve output seen so far.
			ret.max_static_reserve_burst_size = ret.leading_static_reserve_burst_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		// Bounded dynamic reserve output participates through its reusable maximum stack window.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before this dynamic producer resets the static burst.
			ret = ::print_linear_run_scanners_test::reference::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t static_stack_size{
			print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		constexpr ::std::size_t dynamic_buffer_size{
			::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<static_stack_size, char_type>()};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_dynamic = true;
		if (ret.dynamic_buffer_size < dynamic_buffer_size)
		{
			// The capture buffer must fit the largest bounded dynamic reserve window in the run.
			ret.dynamic_buffer_size = dynamic_buffer_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, nocvreft>)
	{
		// Context-printable output anchors the capture run and requires its declared context window.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first before this context producer resets the static burst.
			ret = ::print_linear_run_scanners_test::reference::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t context_buffer_size{
			::fast_io::details::decay::context_print_static_buffer_size_v<false, char_type, nocvreft>};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_context = true;
		if (ret.context_buffer_size < context_buffer_size)
		{
			// The capture buffer must fit the largest static context window in the run.
			ret.context_buffer_size = context_buffer_size;
		}
		return ret;
	}
	else if constexpr (::std::same_as<nocvreft, ::fast_io::io_null_t>)
	{
		// Null output keeps the argument position in the run while requiring no capture storage.
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			// Remaining arguments are scanned first so the null can extend the positional prefix.
			ret = ::print_linear_run_scanners_test::reference::find_context_capture_run_n<char_type, Args...>();
		}
		++ret.position;
		return ret;
	}
	else
	{
		// Any other output protocol terminates the context-capture prefix.
		return {};
	}
}

} // namespace reference

template <::std::size_t extent>
struct fixed
{};

template <::std::integral Char, ::std::size_t extent>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, fixed<extent>>) noexcept
{
	return extent;
}

template <::std::integral Char, ::std::size_t extent>
inline constexpr Char *print_reserve_define(::fast_io::io_reserve_type_t<Char, fixed<extent>>, Char *first, fixed<extent>) noexcept
{
	for (::std::size_t index{}; index != extent; ++index)
	{
		first[index] = static_cast<Char>('x');
	}
	return first + extent;
}

template <::std::size_t hint>
struct dynamic
{};

template <::std::integral Char, ::std::size_t hint>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, dynamic<hint>>, dynamic<hint>) noexcept
{
	return 1u;
}

template <::std::integral Char, ::std::size_t hint>
inline constexpr Char *print_reserve_define(::fast_io::io_reserve_type_t<Char, dynamic<hint>>, Char *first, dynamic<hint>) noexcept
{
	*first = static_cast<Char>('d');
	return first + 1;
}

template <::std::integral Char, ::std::size_t hint>
	requires(hint != 0u)
inline constexpr ::std::size_t print_reserve_static_stack_size(::fast_io::io_reserve_type_t<Char, dynamic<hint>>) noexcept
{
	return hint;
}

template <::std::size_t extent>
struct context
{};

template <::std::integral Char, ::std::size_t extent>
struct context_state
{
	inline constexpr ::fast_io::context_print_result<Char *> print_context_define(context<extent>, Char *first, Char *) noexcept
	{
		return {first, true};
	}
};

template <::std::integral Char, ::std::size_t extent>
inline constexpr ::fast_io::io_type_t<context_state<Char, extent>> print_context_type(
	::fast_io::io_reserve_type_t<Char, context<extent>>) noexcept
{
	return {};
}

template <::std::integral Char, ::std::size_t extent>
inline constexpr ::std::size_t print_context_static_buffer_size(::fast_io::io_reserve_type_t<Char, context<extent>>) noexcept
{
	return extent;
}

template <::std::size_t scatters, ::std::size_t reserve>
struct reserved_scatter
{};

template <::std::integral Char, ::std::size_t scatters, ::std::size_t reserve>
inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<Char, reserved_scatter<scatters, reserve>>) noexcept
{
	return {scatters, reserve};
}

template <::std::integral Char, ::std::size_t scatters, ::std::size_t reserve>
inline constexpr ::fast_io::basic_reserve_scatters_define_result<Char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<Char, reserved_scatter<scatters, reserve>>,
	::fast_io::basic_io_scatter_t<Char> *descriptors, Char *first, reserved_scatter<scatters, reserve>) noexcept
{
	return {descriptors, first};
}

template <::std::integral Char, ::std::size_t scatters, ::std::size_t reserve>
inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<Char, reserved_scatter<scatters, reserve>>) noexcept
{
	return {};
}

struct stop
{};

template <typename T>
struct poisoned_tail
{};

template <::std::integral Char, typename T>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<Char, poisoned_tail<T>>) noexcept
{
	static_assert(!::std::same_as<T, T>, "a stopped scanner instantiated its unreachable tail");
	return 1u;
}

template <::std::integral Char, typename T>
inline constexpr Char *print_reserve_define(::fast_io::io_reserve_type_t<Char, poisoned_tail<T>>, Char *first, poisoned_tail<T>) noexcept
{
	return first;
}

inline constexpr bool same(::fast_io::details::decay::contiguous_scatter_result left,
						   ::fast_io::details::decay::contiguous_scatter_result right) noexcept
{
	return left.position == right.position && left.neededscatters == right.neededscatters &&
		   left.neededspace == right.neededspace && left.null == right.null && left.lastisreserve == right.lastisreserve &&
		   left.hasscatters == right.hasscatters && left.hasreserve == right.hasreserve && left.hasdynamicreserve == right.hasdynamicreserve;
}

inline constexpr bool same(::fast_io::details::decay::context_capture_run_result left,
						   ::fast_io::details::decay::context_capture_run_result right) noexcept
{
	return left.position == right.position && left.context_buffer_size == right.context_buffer_size &&
		   left.dynamic_buffer_size == right.dynamic_buffer_size && left.max_static_reserve_burst_size == right.max_static_reserve_burst_size &&
		   left.leading_static_reserve_burst_size == right.leading_static_reserve_burst_size && left.has_context == right.has_context &&
		   left.has_dynamic == right.has_dynamic;
}

template <::std::integral Char, typename... Args>
inline consteval bool check() noexcept
{
	return same(reference::find_continuous_scatters_n_impl<false, Char, Args...>(),
				::fast_io::details::decay::find_continuous_scatters_n_impl<false, Char, Args...>()) &&
		   same(reference::find_continuous_scatters_n_impl<true, Char, Args...>(),
				::fast_io::details::decay::find_continuous_scatters_n_impl<true, Char, Args...>()) &&
		   same(reference::find_context_capture_run_n<Char, Args...>(),
				::fast_io::details::decay::find_context_capture_run_n<Char, Args...>());
}

template <typename... Types>
struct matrix
{
	template <typename First, typename Second>
	inline static consteval bool third() noexcept
	{
		return (check<char, First, Second, Types>() && ...);
	}
	template <typename First>
	inline static consteval bool second() noexcept
	{
		return (third<First, Types>() && ...);
	}
	inline static constexpr bool value{(second<Types>() && ...)};
};

using null = ::fast_io::io_null_t;
using scatter = ::fast_io::basic_io_scatter_t<char>;
using static_scatter = ::fast_io::manipulators::static_scatter_t<char, 3u>;
using huge = fixed<static_cast<::std::size_t>(PTRDIFF_MAX) / 2u>;
using huge_scatter = reserved_scatter<SIZE_MAX / sizeof(::fast_io::basic_io_scatter_t<char>), 0u>;

static_assert(matrix<stop, null, fixed<3u>, dynamic<0u>, dynamic<37u>, context<19u>, scatter, reserved_scatter<2u, 5u>>::value);
static_assert(check<char, static_scatter, fixed<7u>, static_scatter, null>());
static_assert(check<char, int const, scatter &, dynamic<37u> const, context<19u> const &>());
static_assert(check<char, fixed<3u>, huge, huge, huge>());
static_assert(check<char, huge, fixed<3u>, huge, context<19u>, fixed<7u>>());
static_assert(check<char, fixed<3u>, huge, huge, huge, dynamic<SIZE_MAX - 8u>>());
static_assert(check<char, huge_scatter, huge_scatter, huge_scatter, huge_scatter, huge_scatter,
					huge_scatter, huge_scatter, huge_scatter, huge_scatter, fixed<3u>>());
static_assert(check<char32_t, fixed<3u>, huge, huge, null, context<19u>>());
static_assert(check<char, fixed<3u>, stop, poisoned_tail<int>>());
static_assert(check<char, context<19u>, stop, poisoned_tail<long>>());
static_assert(check<char, fixed<3u>, null>());
static_assert(check<char, fixed<3u>, stop>());
static_assert(check<char, fixed<3u>>());
static_assert(check<char, dynamic<37u>>());

// Exercise the indexed branch as well as the small-pack suffix cache. Padding is inert and the separately retained
// reference still sees the exact same complete argument sequence, including true-tail and stopped-tail distinctions.
template <::std::size_t>
using padding_null = null;

template <::std::integral Char, typename... Args, ::std::size_t... index>
inline consteval bool check_large_impl(::std::index_sequence<index...>) noexcept
{
	return check<Char, padding_null<index>..., Args...>();
}

template <::std::integral Char, typename... Args>
inline consteval bool check_large() noexcept
{
	return check_large_impl<Char, Args...>(::std::make_index_sequence<128u>{});
}

template <::std::size_t... index>
inline consteval bool check_large_first_stop(::std::index_sequence<index...>) noexcept
{
	return check<char, stop, poisoned_tail<short>, padding_null<index>...>();
}

static_assert(check_large<char, static_scatter, fixed<7u>, static_scatter, null>());
static_assert(check_large<char, fixed<3u>, dynamic<37u>, context<19u>, fixed<7u>>());
static_assert(check_large<char, fixed<3u>, huge, huge, huge>());
static_assert(check_large<char, huge, fixed<3u>, huge, context<19u>, fixed<7u>>());
static_assert(check_large<char, fixed<3u>, huge, huge, huge, dynamic<SIZE_MAX - 8u>>());
static_assert(check_large<char, huge_scatter, huge_scatter, huge_scatter, huge_scatter, huge_scatter,
						  huge_scatter, huge_scatter, huge_scatter, huge_scatter, fixed<3u>>());
static_assert(check_large<char32_t, fixed<3u>, huge, huge, null, context<19u>>());
static_assert(check_large<char, fixed<3u>, stop, poisoned_tail<int>>());
static_assert(check_large<char, context<19u>, stop, poisoned_tail<long>>());
static_assert(check_large<char, fixed<3u>>());
static_assert(check_large<char, dynamic<37u>>());
static_assert(check_large_first_stop(::std::make_index_sequence<128u>{}));

// Prefix termination must not turn a reserve immediately before the stopping source into the complete-pack tail.
static_assert(!::fast_io::details::decay::find_continuous_scatters_n<char, fixed<3u>, stop>().lastisreserve);
static_assert(!::fast_io::details::decay::find_continuous_scatters_n<char, fixed<3u>, null>().lastisreserve);
static_assert(::fast_io::details::decay::find_continuous_scatters_n<char, fixed<3u>>().lastisreserve);

} // namespace print_linear_run_scanners_test

int main()
{}
