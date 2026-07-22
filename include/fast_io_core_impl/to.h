#pragma once

namespace fast_io
{

namespace details
{

/// @brief Forms the terminal pointer of a scatter without performing arithmetic on an empty null view.
/// @details `basic_io_scatter_t` permits the conventional empty representation `{nullptr, 0}`. Although no character
///          is dereferenced, spelling `base + len` for that representation is not valid C++ pointer arithmetic because
///          null does not designate an array. Returning `base` for zero length preserves the exact empty range; a
///          positive length retains the producer's ordinary requirement that `base` starts a live character array.
template <::std::integral char_type>
inline constexpr char_type const *scan_scatter_end(
	char_type const *base, ::std::size_t length) noexcept
{
	return length == 0u ? base : base + length;
}

/// @brief Proves the scatter expression actually issued by the by-value `to` dispatcher.
/// @details `basic_inplace_to_decay` owns each normalized argument by value and every helper subsequently names that
///          object. Its CPO expression is therefore `T&`, not the public compatibility query's `T&&`. Keeping this fact
///          in one predicate prevents an rvalue-only scatter overload from selecting a strategy whose body calls it as
///          an lvalue.
template <::std::integral char_type, typename T>
inline constexpr bool to_named_scatter_printable_v =
	::fast_io::scatter_printable_for<char_type, T &>;

/// @brief Selects a scatter for length-then-copy conversion only when its observation is repeatable.
/// @details A single-fragment context conversion may consume a scatter immediately and needs no retained-lifetime proof.
///          A contiguous target with several fragments first sums lengths and later asks every producer for bytes again.
///          The borrowed marker is the source-side promise that both calls return the same live character sequence. If
///          the marker is absent but a reserve protocol exists, the two-pass strategy uses that reserve protocol instead;
///          a scatter-only producer falls back to one-pass dynamic materialization.
template <::std::integral char_type, typename T>
inline constexpr bool to_repeatable_named_scatter_v =
	::fast_io::details::to_named_scatter_printable_v<char_type, T> &&
	::fast_io::borrowed_scatter_source<char_type, T>;

template <::std::integral char_type, typename T>
inline constexpr bool to_two_pass_fragment_available_v =
	::fast_io::details::to_repeatable_named_scatter_v<char_type, T> ||
	::fast_io::reserve_printable<char_type, T> ||
	::fast_io::dynamic_reserve_printable<char_type, T>;

/// @brief Feeds one materialized print fragment to a context scanner.
/// @return `true` exactly when the scanner reports completion; `false` when the fragment is exhausted while partial.
/// @details A parse code and an iterator answer independent questions. `ok` may be returned at the fragment end, while
///          `partial` may consume only a prefix because the state machine changed phase. Therefore completion is proved
///          only by the code, and the iterator is used only to select the next suffix. A partial result that consumes no
///          available input violates the progress contract and is rejected instead of entering an infinite loop.
template <::std::integral char_type, typename state, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr bool inplace_to_decay_context_consume(state &s, T &t, char_type const *first,
												char_type const *last)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	auto current{first};
	while (current != last)
	{
		auto [it, ec] = scan_context_define(
			io_reserve_type<char_type, scanner_type>, s, current, last, t);
		// The CPO result must designate this supplied fragment. Check membership before accepting even `ok`, because a
		// successful code paired with an escaped iterator would otherwise make the next suffix and pointer arithmetic
		// invalid; this is a range proof, not a statement about ownership of the underlying character storage.
		if (!::fast_io::details::scan_iterator_in_current_chunk(current, last, it)) [[unlikely]]
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
		}
		if (ec == ::fast_io::parse_code::ok)
		{
			return true;
		}
		if (ec != ::fast_io::parse_code::partial)
		{
			::fast_io::throw_parse_code(ec);
		}
		if (it == current) [[unlikely]]
		{
			::fast_io::throw_parse_code(::fast_io::parse_code::invalid);
		}
		current = it;
	}
	return false;
}

/// @brief Applies the single terminal transition shared by every fragmented `inplace_to` strategy.
/// @details Fragment production and EOF finalization are deliberately split: each producer may choose scatter,
///          caller-owned reserve storage, or a dynamic buffer, but all of them have exactly one terminal boundary.
///          Centralizing that boundary proves the EOF CPO is invoked once and normalizes `partial` to `invalid`, since
///          no future fragment exists that could make an unfinished state productive.
template <::std::integral char_type, typename state, typename T>
inline constexpr void inplace_to_decay_context_finish(state &s, T &t)
{
	using scanner_type = ::std::remove_cvref_t<T>;
	auto const code{scan_context_eof_define(io_reserve_type<char_type, scanner_type>, s, t)};
	if (code == ::fast_io::parse_code::ok)
	{
		return;
	}
	::fast_io::throw_parse_code(
		code == ::fast_io::parse_code::partial ? ::fast_io::parse_code::invalid : code);
}

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
/// @brief Materializes and scans exactly one printable argument before deciding whether another argument is needed.
/// @details This is the dynamic fallback paired with `scan_context_define`, not the whole-pack high-throughput print
///          path. A context scanner carries its parse state across fragment boundaries and can report `ok` as soon as
///          the minimum printed prefix determines the result--for example, once an integer's terminating delimiter is
///          observed. Printing the complete argument pack first would format producers which the scanner never needs,
///          execute their observable side effects, and change peak storage from the largest required fragment to the
///          sum of every fragment. The reserve/scatter branches above already coalesce packs whose complete size and
///          replay behavior are proved. This fallback instead prioritizes obtaining the exact scan result from the
///          least output required by the context protocol; it reuses one dynamic buffer and stops immediately on `ok`.
inline constexpr void
inplace_to_decay_context_impl(basic_dynamic_output_buffer_ref<basic_dynamic_output_buffer<char_type>> buffer, state &s,
							  T &t, Arg1 arg, Args... args)
{
	// Keep this call single-argument. Combining `arg, args...` would erase the context scanner's early-completion and
	// bounded-fragment properties even though it can look faster as an isolated print operation.
	::fast_io::operations::decay::print_freestanding_decay_impl<false>(buffer, arg);

	char_type *buffer_beg{buffer.dob_ptr->begin_ptr};
	char_type const *buffer_begin{buffer_beg};
	char_type const *buffer_curr{buffer.dob_ptr->curr_ptr};
	if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
	{
		return;
	}
	if constexpr (sizeof...(Args) != 0)
	{
		// The scanner consumed this fragment while retaining `s`; rewind only the output cursor so the next argument can
		// reuse the same allocation. Character storage is not required after `scan_context_define` returns partial.
		buffer.dob_ptr->curr_ptr = buffer_beg;
		inplace_to_decay_context_impl(buffer, s, t, args...);
	}
	else
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void inplace_to_decay_buffer_scatter_context_impl(state &s, T &t, Arg1 arg, Args... args)
{
	basic_io_scatter_t<char_type> scatter{print_scatter_define(io_reserve_type<char_type, Arg1>, arg)};
	char_type const *buffer_begin{scatter.base};
	char_type const *buffer_curr{
		::fast_io::details::scan_scatter_end(buffer_begin, scatter.len)};
	if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
	{
		return;
	}
	if constexpr (sizeof...(Args) != 0)
	{
		inplace_to_decay_buffer_scatter_context_impl<char_type>(s, t, args...);
	}
	else
	{
		::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
	}
}

template <::std::integral char_type, typename state, typename T, typename Arg1, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void inplace_to_decay_buffer_context_impl(char_type *buffer, state &s, T &t, Arg1 arg, Args... args)
{
	if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, Arg1> &&
				  ((::fast_io::details::to_named_scatter_printable_v<char_type, Args> && ...)))
	{
		inplace_to_decay_buffer_scatter_context_impl<char_type>(s, t, arg, args...);
	}
	else
	{
		char_type const *buffer_begin;
		char_type const *buffer_curr;
		if constexpr (::fast_io::details::to_named_scatter_printable_v<char_type, Arg1>)
		{
			auto scatter{print_scatter_define(io_reserve_type<char_type, Arg1>, arg)};
			buffer_begin = scatter.base;
			buffer_curr = ::fast_io::details::scan_scatter_end(buffer_begin, scatter.len);
		}
		else
		{

			buffer_curr = print_reserve_define(io_reserve_type<char_type, Arg1>, buffer, arg);
			buffer_begin = buffer;
		}
		if (::fast_io::details::inplace_to_decay_context_consume<char_type>(s, t, buffer_begin, buffer_curr))
		{
			return;
		}
		if constexpr (sizeof...(Args) != 0)
		{
			inplace_to_decay_buffer_context_impl(buffer, s, t, args...);
		}
		else
		{
			::fast_io::details::inplace_to_decay_context_finish<char_type>(s, t);
		}
	}
}

template <::std::integral char_type, bool ln, typename T, typename... Args>
inline constexpr ::std::size_t calculate_print_normal_maxium_size_main(::std::size_t mx_value) noexcept
{
	::std::size_t val{};
	if constexpr (ln && (sizeof...(Args) == 0))
	{
		++val;
	}
	if constexpr (reserve_printable<char_type, T>)
	{
		constexpr ::std::size_t size{print_reserve_size(io_reserve_type<char_type, T>)};
		static_assert(size != SIZE_MAX, "overflow");
		val += size;
	}
	if (mx_value < val)
	{
		mx_value = val;
	}
	if constexpr ((sizeof...(Args) == 0))
	{
		return mx_value;
	}
	else
	{
		return calculate_print_normal_maxium_size_main<char_type, ln, Args...>(mx_value);
	}
}

template <::std::integral char_type, bool ln, typename... Args>
inline constexpr ::std::size_t calculate_print_normal_maxium_size() noexcept
{
	return calculate_print_normal_maxium_size_main<char_type, ln, Args...>(0);
}

template <::std::integral char_type, bool ln, typename T, typename... Args>
inline constexpr ::std::size_t
calculate_print_normal_dynamic_maxium_main(::std::size_t mx_value, T t, Args... args)
{
	::std::size_t size{};
	if constexpr (dynamic_reserve_printable<char_type, T>)
	{
		size = print_reserve_size(io_reserve_type<char_type, T>, t);
	}
	else if constexpr (reserve_printable<char_type, T>)
	{
		// The dynamic path reuses one fragment buffer for both run-time and type-level reserve producers. Ignoring a
		// static-only producer here made the maximum smaller than a later write whenever another argument forced this path.
		size = print_reserve_size(io_reserve_type<char_type, T>);
	}
	if constexpr (dynamic_reserve_printable<char_type, T> || reserve_printable<char_type, T>)
	{
		if constexpr (ln && (sizeof...(Args) == 0))
		{
			if (size == SIZE_MAX)
			{
				fast_terminate();
			}
			++size;
		}
		if (mx_value < size)
		{
			mx_value = size;
		}
	}
	if constexpr ((sizeof...(Args) == 0))
	{
		return mx_value;
	}
	else
	{
		return calculate_print_normal_dynamic_maxium_main<char_type, ln>(mx_value, args...);
	}
}

template <::std::integral char_type, typename T>
inline constexpr void deal_with_single_to(char_type const *buffer_begin, char_type const *buffer_end, T &t)
{
	// The normalized scanner is deliberately borrowed. An alias CPO may return a noncopyable lvalue proxy, and the
	// enclosing public call already guarantees that either its referenced storage or its prvalue temporary remains alive.
	// Its cv-qualification is part of CPO overload resolution, while the reserve tag follows the public scanner concepts
	// and names the unqualified proxy representation.
	auto const result{scan_contiguous_define(
		io_reserve_type<char_type, ::std::remove_cvref_t<T>>, buffer_begin, buffer_end, t)};
	if (!::fast_io::details::scan_iterator_in_current_chunk(buffer_begin, buffer_end, result.iter)) [[unlikely]]
	{
		// The conversion bridge owns only the materialized fragment. Validate before observing success so an escaped
		// iterator cannot be accepted merely because this path intentionally permits an unconsumed suffix.
		throw_parse_code(parse_code::invalid);
	}
	if (result.code != parse_code::ok)
	{
		throw_parse_code(result.code);
	}
}

template <::std::integral char_type, typename T, typename Arg>
inline constexpr void to_deal_with_contiguous_single_scatter(T &t, Arg arg)
{
	basic_io_scatter_t<char_type> scatter{print_scatter_define(io_reserve_type<char_type, Arg>, arg)};
	if (scatter.len == 0u)
	{
		// Unlike the context bridge, the contiguous bridge invokes its scanner even for empty input. Supply one valid
		// object address so the CPO may compare `first == last` without being exposed to a null pointer pair. The scanner
		// receives an empty half-open range and therefore has no permission to inspect the dummy character.
		char_type dummy{};
		char_type const *const empty{__builtin_addressof(dummy)};
		deal_with_single_to<char_type>(empty, empty, t);
	}
	else
	{
		auto base{scatter.base};
		deal_with_single_to<char_type>(base, base + scatter.len, t);
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *to_impl_with_reserve_recursive(char_type *p, T t, Args... args)
{
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		p = copy_scatter(print_scatter_define(io_reserve_type<char_type, T>, t), p);
	}
	else
	{
		p = print_reserve_define(io_reserve_type<char_type, T>, p, t);
	}
	if constexpr (sizeof...(Args) == 0)
	{
		return p;
	}
	else
	{
		return to_impl_with_reserve_recursive<char_type>(p, args...);
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_dynamic_reserve_size_with_scatter([[maybe_unused]] T t, Args... args)
{
	::std::size_t res{};
	if constexpr (::fast_io::details::to_repeatable_named_scatter_v<char_type, T>)
	{
		// Emission selects the same repeatable named-scatter branch in `to_impl_with_reserve_recursive`. Measuring a
		// dynamic reserve representation here while emitting a scatter representation later can under-allocate even when
		// both protocols are individually valid.
		res = print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t).len;
	}
	else if constexpr (dynamic_reserve_printable<char_type, T>)
	{
		res = print_reserve_size(io_reserve_type<char_type, T>, t);
	}
	else if constexpr (reserve_printable<char_type, T>)
	{
		// This function is also used for packs mixing a static reserve producer with a dynamic one. The static capacity
		// remains part of the total even though it needs no object-dependent measurement.
		res = print_reserve_size(io_reserve_type<char_type, T>);
	}
	if constexpr (sizeof...(Args) == 0)
	{
		return res;
	}
	else
	{
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			res, calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...));
	}
}

/// @brief Proves that `to` can materialize its normalized print arguments without constructing a stream fallback.
/// @details This predicate is the type-level counterpart of the first branch in `basic_inplace_to_decay`.  Keeping the
///          complete fragment proof in one function prevents public availability from drifting away from execution:
///          context scanners may consume each fragment once, a single contiguous scatter is observed once, static
///          reserves need no replay, and a multi-fragment contiguous scan retains only explicitly repeatable scatters.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool inplace_to_direct_fragment_strategy_available() noexcept
{
	// An empty run or a non-scannable target cannot consume any direct printable fragment.
	if constexpr (
		sizeof...(Args) == 0u ||
		!(::fast_io::contiguous_scannable<char_type, T> ||
		  ::fast_io::context_scannable<char_type, T>))
	{
		return false;
	}
	else
	{
		constexpr bool all_named_fragments{
			((::fast_io::reserve_printable<char_type, Args> ||
			  ::fast_io::dynamic_reserve_printable<char_type, Args> ||
			  ::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_scatters{
			((::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_static_reserves{
			((::fast_io::reserve_printable<char_type, Args>) && ...)};
		constexpr bool context_fragment_strategy{
			::fast_io::context_scannable<char_type, T> &&
			(!(::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u))};
		constexpr bool contiguous_single_scatter_strategy{
			::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u && all_scatters};
		constexpr bool contiguous_two_pass_strategy{
			::fast_io::contiguous_scannable<char_type, T> &&
			((::fast_io::details::to_two_pass_fragment_available_v<char_type, Args>) && ...)};
		return all_named_fragments &&
			(context_fragment_strategy || contiguous_single_scatter_strategy ||
			 all_static_reserves || contiguous_two_pass_strategy);
	}
}

/// @brief Proves the exact dynamic-output fallback issued by `basic_inplace_to_decay`.
/// @details A context scanner formats one argument at a time so it can stop as soon as parsing completes; its proof must
///          therefore validate every singleton call.  A contiguous scanner formats the complete run in one call and
///          instead requires that exact pack.  Testing the concrete dynamic-buffer observer closes the former
///          character-only concept hole where a dummy-stream-only `print_define` was admitted and failed later inside
///          the dispatcher.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool inplace_to_dynamic_output_strategy_available() noexcept
{
	using output_type = ::fast_io::basic_dynamic_output_buffer_ref<
		::fast_io::basic_dynamic_output_buffer<char_type>>;
	// Context scanners require singleton printability because the fallback checks completion after every argument.
	if constexpr (
		::fast_io::context_scannable<char_type, T> &&
		(!(::fast_io::contiguous_scannable<char_type, T> && sizeof...(Args) == 1u)))
	{
		return (::fast_io::details::decay::print_freestanding_output_run_okay<
			false, output_type, Args>() && ...);
	}
	// Contiguous scanners observe one complete dynamic-buffer run and therefore validate the whole argument pack.
	else if constexpr (::fast_io::contiguous_scannable<char_type, T>)
	{
		return ::fast_io::details::decay::print_freestanding_output_run_okay<
			false, output_type, Args...>();
	}
	else
	{
		return false;
	}
}

/// @brief Accepts exactly the normalized source packs supported by either direct-fragment or dynamic-output conversion.
template <typename char_type, typename T, typename... Args>
concept inplace_to_decay_detect =
	::std::integral<char_type> &&
	(sizeof...(Args) != 0u &&
	 (::fast_io::contiguous_scannable<char_type, T> ||
	  ::fast_io::context_scannable<char_type, T>) &&
	 (::fast_io::details::inplace_to_direct_fragment_strategy_available<
		  char_type, T, Args...>() ||
	  ::fast_io::details::inplace_to_dynamic_output_strategy_available<
		  char_type, T, Args...>()));

} // namespace details

/// @brief Converts printable fragments into an already-normalized scan target without materializing its alias.
/// @details `io_scan_alias` is permitted to return either a proxy value or a noncopyable proxy reference. A forwarding
///          parameter extends the value's lifetime through this call and retains an lvalue's identity; every lower
///          strategy consequently borrows the same named object. Taking this parameter by value would silently make
///          concept detection and execution disagree for otherwise valid reference aliases.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to_decay(T &&t, Args... args)
{
	constexpr bool available{::fast_io::details::inplace_to_decay_detect<char_type, T, Args...>};
	// Instantiate execution only after the exact direct-or-dynamic strategy has been proved for the normalized pack.
	if constexpr (available)
	{
		constexpr bool direct_fragment_strategy{
			::fast_io::details::inplace_to_direct_fragment_strategy_available<
				char_type, T, Args...>()};
		constexpr bool all_scatters{
			((::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_static_reserves{((reserve_printable<char_type, Args>) && ...)};
		// Context scanning consumes every fragment immediately, a single contiguous scatter is observed once, and a
		// static-reserve pack needs no sizing pass. Every other contiguous composition is length-then-copy and therefore
		// enters it only when each selected scatter has explicit repeatable provenance (or a reserve fallback).
		if constexpr (direct_fragment_strategy)
		{
			constexpr bool no_need_dynamic_reserve{
				((reserve_printable<char_type, Args> ||
				  ::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
			if constexpr (context_scannable<char_type, T> &&
						  (!(contiguous_scannable<char_type, T> && sizeof...(args) == 1)))
			{
				using state_type = ::fast_io::details::scan_context_state_t<char_type, T>;
				::fast_io::details::with_scan_context_state<state_type>([&](state_type &state) {
					if constexpr (all_scatters)
					{
						::fast_io::details::inplace_to_decay_buffer_scatter_context_impl<char_type>(
							state, t, args...);
					}
					else if constexpr (no_need_dynamic_reserve)
					{
						constexpr ::std::size_t maximum_reserve_size{
							::fast_io::details::calculate_print_normal_maxium_size<char_type, false, Args...>()};
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
								maximum_reserve_size, char_type>)
						{
							// One reusable fragment buffer fits the configured hot-stack budget.
							char_type buffer[maximum_reserve_size];
							::fast_io::details::inplace_to_decay_buffer_context_impl<char_type>(
								buffer, state, t, args...);
						}
						else
						{
							// A type-level reserve bound is a capacity proof, not permission to enlarge every caller's
							// frame. The dynamic branch preserves reuse once the policy limit is exceeded.
							::fast_io::details::local_operator_new_array_ptr<char_type> buffer(maximum_reserve_size);
							::fast_io::details::inplace_to_decay_buffer_context_impl<char_type>(
								buffer.ptr, state, t, args...);
						}
					}
					else
					{
						::std::size_t const maximum_reserve_size{
							::fast_io::details::calculate_print_normal_dynamic_maxium_main<char_type, false>(
								0, args...)};
						::fast_io::details::local_operator_new_array_ptr<char_type> heap_buffer(maximum_reserve_size);
						::fast_io::details::inplace_to_decay_buffer_context_impl<char_type>(
							heap_buffer.ptr, state, t, args...);
					}
				});
			}
			else if constexpr (contiguous_scannable<char_type, T>)
			{
				if constexpr (all_scatters && sizeof...(Args) == 1) // crucial for performance
				{
					::fast_io::details::to_deal_with_contiguous_single_scatter<char_type>(t, args...);
				}
				else if constexpr (all_static_reserves)
				{
					constexpr ::std::size_t total_size{
						::fast_io::details::decay::calculate_scatter_reserve_size<char_type, Args...>()};
					if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
							total_size, char_type>)
					{
						char_type buffer[total_size];
						auto const ret{::fast_io::details::to_impl_with_reserve_recursive(buffer, args...)};
						::fast_io::details::deal_with_single_to<char_type>(buffer, ret, t);
					}
					else
					{
						// Summing several individually valid reserve bounds can still create an unbounded automatic
						// object. Dynamic storage keeps one materialization without coupling capacity to frame size.
						::fast_io::details::local_operator_new_array_ptr<char_type> buffer(total_size);
						auto const ret{
							::fast_io::details::to_impl_with_reserve_recursive(buffer.ptr, args...)};
						::fast_io::details::deal_with_single_to<char_type>(buffer.ptr, ret, t);
					}
				}
				else
				{
					::std::size_t const maximum_reserve_size{
						::fast_io::details::calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...)};
					::fast_io::details::local_operator_new_array_ptr<char_type> heap_buffer(maximum_reserve_size);
					auto ret{::fast_io::details::to_impl_with_reserve_recursive(heap_buffer.ptr, args...)};
					::fast_io::details::deal_with_single_to<char_type>(heap_buffer.ptr, ret, t);
				}
			}
		}
		else
		{
			static_assert(
				::fast_io::details::inplace_to_dynamic_output_strategy_available<
					char_type, T, Args...>(),
				"the normalized to() fallback is not printable to its dynamic output buffer");
			basic_dynamic_output_buffer<char_type> buffer;
			decltype(auto) ref = ::fast_io::operations::output_stream_ref(buffer);
			if constexpr (context_scannable<char_type, T> &&
						  (!(contiguous_scannable<char_type, T> && sizeof...(args) == 1)))
			{
				using state_type = ::fast_io::details::scan_context_state_t<char_type, T>;
				::fast_io::details::with_scan_context_state<state_type>([&](state_type &state) {
					::fast_io::details::inplace_to_decay_context_impl(ref, state, t, args...);
				});
			}
			else if constexpr (contiguous_scannable<char_type, T>)
			{
				::fast_io::operations::decay::print_freestanding_decay<false>(ref, args...);
				// The dynamic output object may have grown from its inline array, so use its active pointers rather than
				// naming the embedded storage. Both pointers are updated together by every growth operation.
				::fast_io::details::deal_with_single_to<char_type>(buffer.begin_ptr, buffer.curr_ptr, t);
			}
			else
			{
				constexpr bool type_error{context_scannable<char_type, T>};
				static_assert(type_error, "scan type error");
			}
		}
	}
	else
	{
		static_assert(available, "either some arguments are not printable or the target is not scannable");
	}
}

namespace details
{

template <::std::integral char_type, typename T, typename... Args>
	requires ::fast_io::details::inplace_to_decay_detect<char_type, T, Args...>
inline constexpr void basic_inplace_to_decay_model(T &&, Args &&...)
{
}

template <typename char_type, typename T, typename... Args>
concept can_do_inplace_to = requires(T &t, Args &&...args) {
	::fast_io::details::basic_inplace_to_decay_model<char_type>(
		::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(t)),
		io_print_forward<char_type>(io_print_alias(args))...);
};

template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_source_replacement_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_replacement_t<char_type, T>;

/// The exact already-normalized prvalue passed by the compiler-constant true arm.
/// `plain_true_forward` intentionally treats candidates and untouched sources differently: a candidate aliases its
/// newly materialized prvalue, while a non-candidate aliases the helper's named source lvalue. Re-running
/// `io_print_alias` on the raw replacement type would erase that distinction and make availability disagree with the
/// call below for ref-qualified customization sets.
template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_normalized_t = ::std::remove_cvref_t<decltype(
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_plain_true_forward<
			false, char_type>(::std::declval<T>()))>;

/// The exact forwarding-parameter type deduced by `basic_inplace_to_decay` for the named public scan target.
template <::std::integral char_type, typename T>
using inplace_to_normalized_target_t = decltype(
	::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(
		::std::declval<::std::remove_reference_t<T> &>())));

/// The exact normalized source type passed by the historical false arm.
template <::std::integral char_type, typename T>
using inplace_to_compiler_constant_source_normalized_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_normalized_t<
			char_type, false, T>;

/// @brief Detects a status owner which the selected dynamic-output `to` strategy would actually invoke.
/// @details Direct fragment conversion never enters an output dispatcher, so an otherwise matching status CPO is
///          irrelevant there. The dynamic context fallback emits singleton runs to preserve early completion, whereas
///          the contiguous fallback emits the complete pack. Mirroring that exact shape prevents a compiler-constant
///          replacement from adding or removing a whole-run customization and thereby changing the characters scanned.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool
inplace_to_selected_dynamic_status_owner() noexcept
{
	// Direct fragment scanning bypasses every output dispatcher, so no dynamic-output status owner can participate.
	if constexpr (
		::fast_io::details::inplace_to_direct_fragment_strategy_available<
			char_type, T, Args...>())
	{
		return false;
	}
	else
	{
		using output_type = ::fast_io::basic_dynamic_output_buffer_ref<
			::fast_io::basic_dynamic_output_buffer<char_type>>;
		// A fragmented context fallback dispatches each argument separately to preserve early completion.
		if constexpr (
			::fast_io::context_scannable<char_type, T> &&
			(!(::fast_io::contiguous_scannable<char_type, T> &&
			   sizeof...(Args) == 1u)))
		{
			return (false || ... ||
				::fast_io::operations::decay::defines::
					has_status_print_define<false, output_type, Args>);
		}
		// A contiguous fallback dispatches the complete pack and must test its whole-run status customization.
		else if constexpr (::fast_io::contiguous_scannable<char_type, T>)
		{
			return ::fast_io::operations::decay::defines::
				has_status_print_define<false, output_type, Args...>;
		}
		else
		{
			return false;
		}
	}
}

/// @brief Proves that `to` may replace one or more public source values before print normalization.
/// @details The replacement uses the same destination-neutral compiler-constant CPO as print and concat, but admission
///          is checked against `to`'s exact scanner and dynamic-output/direct-fragment strategies. Semantic nodes keep
///          their existing graph-owned normalization. Both arms perform the same per-source alias/status forwarding;
///          destination status owners are checked separately against the exact strategy and run shape below. Only
///          candidate proxy state counts toward the common materialization budget; unchanged run-time fragments retain
///          their ordinary zero-copy or reserve protocol.
template <::std::integral char_type, typename T, typename... Args>
inline consteval bool
inplace_to_compiler_constant_source_available() noexcept
{
	constexpr bool has_candidate{
		(false || ... ||
		 ::fast_io::operations::decay::
			 print_compiler_constant_pre_normalization_candidate_v<
				 char_type, Args>)};
	constexpr bool has_semantic_source{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 char_type, Args>)};
	constexpr bool has_semantic_replacement{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 char_type,
			 ::fast_io::details::inplace_to_compiler_constant_source_replacement_t<
				 char_type, Args>>)};
	// Empty candidate sets and semantic graphs remain on their established normalization path.
	if constexpr (!has_candidate || has_semantic_source ||
				  has_semantic_replacement)
	{
		return false;
	}
	else
	{
		using target_type =
			::fast_io::details::inplace_to_normalized_target_t<char_type, T>;
		constexpr bool source_status_owner{
			::fast_io::details::inplace_to_selected_dynamic_status_owner<
				char_type, target_type,
				::fast_io::details::
					inplace_to_compiler_constant_source_normalized_t<
						char_type, Args>...>()};
		constexpr bool replacement_status_owner{
			::fast_io::details::inplace_to_selected_dynamic_status_owner<
				char_type, target_type,
				::fast_io::details::inplace_to_compiler_constant_normalized_t<
					char_type, Args>...>()};
		// Either spelling must retain a selected dynamic-output status owner, so direct replacement is rejected.
		if constexpr (source_status_owner || replacement_status_owner)
		{
			return false;
		}

		constexpr ::std::size_t proxy_bytes{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes};
			::std::size_t total{};
			((total = [](::std::size_t current) consteval {
				// Only actual candidates contribute proxy state; untouched sources retain their zero-byte budget entry.
				if constexpr (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_candidate_v<
						char_type, Args>)
				{
					constexpr ::std::size_t extent{sizeof(
						::fast_io::details::inplace_to_compiler_constant_source_replacement_t<
							char_type, Args>)};
					return current > maximum || extent > maximum - current
						? SIZE_MAX
						: current + extent;
				}
				else
				{
					return current;
				}
			}(total)),
			 ...);
			return total;
		}()};
		return proxy_bytes != SIZE_MAX &&
			   proxy_bytes <=
				   ::fast_io::details::compiler_constant_materialization_max_bytes &&
			   ::fast_io::details::inplace_to_decay_detect<
				   char_type,
				   target_type,
				   ::fast_io::details::inplace_to_compiler_constant_normalized_t<
					   char_type, Args>...>;
	}
}

/// @brief Executes only the proven compiler-constant arm and then reuses the ordinary normalized `to` dispatcher.
/// @details Every proxy is a synchronous prvalue whose lifetime covers aliasing, printing, and scanning in the nested
///          call. Non-candidates retain the historical named-lvalue normalization, so a mixed constant/run-time
///          conversion does not copy or pre-measure its dynamic fragments.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void inplace_to_compiler_constant_source_materialized(
	T &&target, Args &&...args)
{
	::fast_io::basic_inplace_to_decay<char_type>(
		::fast_io::io_scan_forward<char_type>(
			::fast_io::io_scan_alias(target)),
		// A non-candidate must still be aliased from this function's named source lvalue. Forwarding it directly to
		// `io_print_alias` would select an rvalue-only customization only in a neighboring argument's constant true arm,
		// making observable spelling depend on optimization. The shared print primitive materializes candidates as owned
		// prvalues while deliberately retaining the historical named-lvalue category for every untouched source.
		::fast_io::operations::decay::
			print_compiler_constant_pre_normalization_plain_true_forward<
				false, char_type>(::std::forward<Args>(args))...);
}

/// @brief Shared public source boundary for every character-domain `inplace_to` facade.
/// @details The false arm is exactly the historical alias/forward/decay expression. Consequently an unknown value pays
///          no proxy construction, size query, or extra output pass; only a proven true optimizer query enters the
///          optional replacement arm.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void inplace_to_compiler_constant_checked_entry(
	T &&target, Args &&...args)
{
	constexpr bool ordinary_available{
		::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
	// Keep malformed public source/target combinations in the diagnostic arm without instantiating either execution path.
	if constexpr (ordinary_available)
	{
		// Enter the optional source replacement only when its normalized scan and status semantics have been proved.
		if constexpr (
			::fast_io::details::inplace_to_compiler_constant_source_available<
				char_type, T, Args &&...>())
		{
			if (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_gate<char_type>(args...))
			{
				::fast_io::details::inplace_to_compiler_constant_source_materialized<
					char_type>(::std::forward<T>(target),
							   ::std::forward<Args>(args)...);
				return;
			}
		}
		::fast_io::basic_inplace_to_decay<char_type>(
			::fast_io::io_scan_forward<char_type>(
				::fast_io::io_scan_alias(target)),
			::fast_io::io_print_forward<char_type>(
				::fast_io::io_print_alias(args))...);
	}
	else
	{
		static_assert(ordinary_available,
			"either some arguments are not printable or the target is not scannable");
	}
}

} // namespace details

/// @brief Applies the shared compiler-constant source gate to an explicit character-domain inplace conversion.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to(T &&t, Args &&...args)
{
	::fast_io::details::inplace_to_compiler_constant_checked_entry<char_type>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable narrow-character fragments into an existing scan target.
template <typename T, typename... Args>
inline constexpr void inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable wide-character fragments into an existing scan target.
template <typename T, typename... Args>
inline constexpr void winplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<wchar_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-8 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
inline constexpr void u8inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char8_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-16 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
inline constexpr void u16inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char16_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

/// @brief Converts printable UTF-32 code-unit fragments into an existing scan target.
template <typename T, typename... Args>
inline constexpr void u32inplace_to(T &&t, Args &&...args)
{
	::fast_io::basic_inplace_to<char32_t>(
		::std::forward<T>(t), ::std::forward<Args>(args)...);
}

namespace decay
{

/// @brief Constructs and scans a target from an already-normalized source pack without repeating alias normalization.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr T basic_to_decay(Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		return T();
	}
	else
	{
		// `Args...` have already passed the public source alias/forward boundary and are owned by value here. Re-applying
		// `can_do_inplace_to` would alias those normalized objects a second time, so a non-idempotent alias set could make
		// this admission test disagree with the direct `args...` expression in the body. Model precisely that body instead.
		constexpr bool available{
			::fast_io::details::inplace_to_decay_detect<
				char_type,
				::fast_io::details::inplace_to_normalized_target_t<char_type, T>,
				Args...>};
		// Construct the target only when the already-normalized source pack has a concrete conversion strategy.
		if constexpr (available)
		{
			// Scalar targets are value-initialized so a scanner can safely assign only the fields required by its protocol.
			if constexpr (::std::is_scalar_v<T>)
			{
				T v{};
				basic_inplace_to_decay<char_type>(::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(v)),
												  args...);
				return v;
			}
			else
			{
				T v;
				basic_inplace_to_decay<char_type>(::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(v)),
												  args...);
				return v;
			}
		}
		else
		{
			static_assert(available,
				"the normalized arguments are not printable to the to() conversion strategy");
			return T();
		}
	}
}

} // namespace decay

namespace details
{

/// @brief Shared compiler-constant source boundary for every value-returning `to` facade.
/// @details Both selected arms first normalize the public print sources and only then enter `basic_to_decay`, which
///          constructs the scan target. This preserves the historical observable order: source alias CPO side effects
///          precede `T`'s default construction. Constructing `T` here and delegating to `basic_inplace_to` would reverse
///          that order even though the final character sequence is identical.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr T to_compiler_constant_checked_entry(Args &&...args)
{
	// An empty conversion has no source boundary and preserves the historical default-construction semantics directly.
	if constexpr (sizeof...(Args) == 0u)
	{
		// `basic_to_decay` has always defined the empty conversion as ordinary value/default construction. Keep the public
		// source boundary transparent for that case: there is no source protocol to normalize or compiler-constant query.
		return ::fast_io::decay::basic_to_decay<char_type, T>();
	}
	else
	{
		constexpr bool ordinary_available{
			::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
		// Delay all conversion instantiation until the public alias and scanner protocols are known to be valid.
		if constexpr (ordinary_available)
		{
			// The compiler-constant arm is available only when replacing sources preserves the exact selected scan strategy.
			if constexpr (
				::fast_io::details::inplace_to_compiler_constant_source_available<
					char_type, T, Args &&...>())
			{
				if (::fast_io::operations::decay::
						print_compiler_constant_pre_normalization_gate<char_type>(args...))
				{
					return ::fast_io::decay::basic_to_decay<char_type, T>(
						::fast_io::operations::decay::
							print_compiler_constant_pre_normalization_plain_true_forward<
								false, char_type>(::std::forward<Args>(args))...);
				}
			}
			return ::fast_io::decay::basic_to_decay<char_type, T>(
				::fast_io::io_print_forward<char_type>(
					::fast_io::io_print_alias(args))...);
		}
		else
		{
			static_assert(ordinary_available,
				"either some arguments are not printable or the target is not scannable");
		}
	}
}

} // namespace details

/// @brief Constructs a target in the requested character domain through the shared compiler-constant source boundary.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr T basic_to(Args &&...args)
{
	return ::fast_io::details::to_compiler_constant_checked_entry<char_type, T>(
		::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning narrow-character source fragments.
template <typename T, typename... Args>
[[nodiscard]] inline constexpr T to(Args &&...args)
{
	return ::fast_io::basic_to<char, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning wide-character source fragments.
template <typename T, typename... Args>
[[nodiscard]] inline constexpr T wto(Args &&...args)
{
	return ::fast_io::basic_to<wchar_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-8 code-unit source fragments.
template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u8to(Args &&...args)
{
	return ::fast_io::basic_to<char8_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-16 code-unit source fragments.
template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u16to(Args &&...args)
{
	return ::fast_io::basic_to<char16_t, T>(::std::forward<Args>(args)...);
}

/// @brief Constructs a target by formatting and scanning UTF-32 code-unit source fragments.
template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u32to(Args &&...args)
{
	return ::fast_io::basic_to<char32_t, T>(::std::forward<Args>(args)...);
}

} // namespace fast_io
