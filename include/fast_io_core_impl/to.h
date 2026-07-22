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

template <typename char_type, typename T, typename... Args>
concept inplace_to_decay_detect =
	::std::integral<char_type> &&
	(sizeof...(Args) != 0 &&
	 ::fast_io::operations::decay::defines::print_freestanding_params_okay<char_type, Args...> &&
	 (contiguous_scannable<char_type, T> || context_scannable<char_type, T>));

} // namespace details

/// @brief Converts printable fragments into an already-normalized scan target without materializing its alias.
/// @details `io_scan_alias` is permitted to return either a proxy value or a noncopyable proxy reference. A forwarding
///          parameter extends the value's lifetime through this call and retains an lvalue's identity; every lower
///          strategy consequently borrows the same named object. Taking this parameter by value would silently make
///          concept detection and execution disagree for otherwise valid reference aliases.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to_decay(T &&t, Args... args)
{
	constexpr bool failed{::fast_io::details::inplace_to_decay_detect<char_type, T, Args...>};
	if constexpr (failed)
	{
		constexpr bool all_named_fragments{
			((reserve_printable<char_type, Args> || dynamic_reserve_printable<char_type, Args> ||
			  ::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_scatters{
			((::fast_io::details::to_named_scatter_printable_v<char_type, Args>) && ...)};
		constexpr bool all_static_reserves{((reserve_printable<char_type, Args>) && ...)};
		constexpr bool context_fragment_strategy{
			context_scannable<char_type, T> &&
			(!(contiguous_scannable<char_type, T> && sizeof...(Args) == 1u))};
		constexpr bool contiguous_single_scatter_strategy{
			contiguous_scannable<char_type, T> && sizeof...(Args) == 1u && all_scatters};
		constexpr bool contiguous_two_pass_strategy{
			contiguous_scannable<char_type, T> &&
			((::fast_io::details::to_two_pass_fragment_available_v<char_type, Args>) && ...)};
		// Context scanning consumes every fragment immediately, a single contiguous scatter is observed once, and a
		// static-reserve pack needs no sizing pass. Every other contiguous composition is length-then-copy and therefore
		// enters it only when each selected scatter has explicit repeatable provenance (or a reserve fallback).
		constexpr bool direct_fragment_strategy_safe{
			context_fragment_strategy || contiguous_single_scatter_strategy || all_static_reserves ||
			contiguous_two_pass_strategy};
		if constexpr (all_named_fragments && direct_fragment_strategy_safe)
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
		static_assert(failed, "either somes args not printable or some type not detectable");
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

} // namespace details

template <::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_inplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<char_type>(::fast_io::io_scan_forward<char_type>(::fast_io::io_scan_alias(t)),
													 io_print_forward<char_type>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

template <typename T, typename... Args>
inline constexpr void inplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<char>(::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(t)),
												io_print_forward<char>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

template <typename T, typename... Args>
inline constexpr void winplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<wchar_t, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<wchar_t>(::fast_io::io_scan_forward<wchar_t>(::fast_io::io_scan_alias(t)),
												   io_print_forward<wchar_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

template <typename T, typename... Args>
inline constexpr void u8inplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char8_t, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<char8_t>(::fast_io::io_scan_forward<char8_t>(::fast_io::io_scan_alias(t)),
												   io_print_forward<char8_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

template <typename T, typename... Args>
inline constexpr void u16inplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char16_t, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<char16_t>(::fast_io::io_scan_forward<char16_t>(::fast_io::io_scan_alias(t)),
													io_print_forward<char16_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

template <typename T, typename... Args>
inline constexpr void u32inplace_to(T &&t, Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char32_t, T, Args...>};
	if constexpr (failed)
	{
		::fast_io::basic_inplace_to_decay<char32_t>(::fast_io::io_scan_forward<char32_t>(::fast_io::io_scan_alias(t)),
													io_print_forward<char32_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
	}
}

namespace decay
{

template <::std::integral char_type, typename T, typename... Args>
inline constexpr T basic_to_decay(Args... args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
	if constexpr (sizeof...(Args) == 0)
	{
		return T();
	}
	else if constexpr (failed)
	{
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
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

} // namespace decay

template <::std::integral char_type, typename T, typename... Args>
inline constexpr T basic_to(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char_type, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<char_type, T>(io_print_forward<char_type>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

template <typename T, typename... Args>
[[nodiscard]] inline constexpr T to(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<char, T>(io_print_forward<char>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

template <typename T, typename... Args>
[[nodiscard]] inline constexpr T wto(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<wchar_t, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<wchar_t, T>(io_print_forward<wchar_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u8to(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char8_t, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<char8_t, T>(io_print_forward<char8_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u16to(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char16_t, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<char16_t, T>(io_print_forward<char16_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

template <typename T, typename... Args>
[[nodiscard]] inline constexpr T u32to(Args &&...args)
{
	constexpr bool failed{::fast_io::details::can_do_inplace_to<char32_t, T, Args...>};
	if constexpr (failed)
	{
		return ::fast_io::decay::basic_to_decay<char32_t, T>(io_print_forward<char32_t>(io_print_alias(args))...);
	}
	else
	{
		static_assert(failed, "either somes args not printable or some type not detectable");
		return T();
	}
}

} // namespace fast_io
