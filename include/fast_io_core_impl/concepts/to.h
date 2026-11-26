#pragma once

/**
 * @file
 * @brief Defines the complete-input proofs available to in-memory conversions.
 *
 * @par Stream and conversion state machines
 * A refillable scanner holds a parse state and a readable window. If the window contains
 * @c "12", the next refill can still supply @c "3". When the leaf returns partial, the driver
 * carries its state into the next window. Only the endpoint can authorize the EOF transition.
 * A terminal input device can establish EOF separately. Pointer subtraction gives the size of
 * the current window in either case.
 *
 * For a nonempty pack, @c to and @c inplace_to have a finite list of source occurrences.
 * Write @f$S_i@f$ for the sequence emitted by occurrence @f$i@f$ in the selected character
 * domain. The logical input is their ordered concatenation @f$S=S_1\cdots S_k@f$.
 * Exhausting the last source authorizes the terminal transition. A context scanner can finish
 * inside an earlier fragment. The operation must preserve the observation and ownership rules
 * of its chosen strategy, including any unused suffix. A parse result may still be an error.
 *
 * @par Derivation and use of the static bound
 * If each selected representation proves @f$|S_i|\le B_i@f$ for every admitted source value,
 * then @f$|S|\le\sum_i B_i@f$. Units are elements of the conversion's character type.
 * The proof survives fragmented production and coroutine suspensions because it concerns the
 * complete spelling of each capture. An unknown component or an unrepresentable sum discards
 * the aggregate proof. A bounded scratch buffer per step only constrains that individual step.
 * Once the complete range is available, the operation passes the bound as a template argument.
 * The target chooses the specialized leaf and preserves the ordinary scanner's observable result.
 *
 * @par Example of a valid substitution
 * For a decimal field of at most three units, even an all-digit spelling has magnitude at most 999.
 * A target that represents 999 can omit a per-digit overflow test. An 8-bit unsigned target still
 * checks the parsed magnitude against 255 before publishing the value. In either case the scanner
 * must recognize signs, whitespace, a redundant leading zero and the first trailing delimiter
 * exactly as its ordinary grammar requires. The end supplied by the caller limits every load.
 *
 * @par Result ownership
 * A returning conversion publishes one new T on success. An in-place conversion borrows an existing
 * target for the duration of its execution. In the asynchronous forms the final join delivers the
 * new value or ends the borrow. Type-level bounds describe the input and confer no earlier access
 * to either destination. A provider start can be observed while the target is still pending.
 *
 * For a returning operation, successful delivery requires both a successful provider result and
 * successful construction of the value in the consumer. A move that fails during delivery leaves
 * every recorded provider point as it was when production ended. No usable result reaches the caller.
 * A result or committed target may retain external storage only with its own valid lifetime contract.
 * The private input spelling can expire as soon as conversion finishes.
 *
 * @see bounded_to_scannable
 * @see bounded_to_scatter_source
 */

namespace fast_io
{

/// @brief Lets a conversion target refine its contiguous scanner for a complete spelling of at most Maximum code units.
/// @details For every such range and initial target state, the CPO must preserve the ordinary contiguous
///          scanner's consumed position, parse code, target writes and exceptions. The bound covers the entire
///          range, including signs, whitespace and trailing separators. Only [first,last) is readable.
///          Define the overload for the bounds that the target can exploit. A dynamic extent selects the
///          ordinary conversion strategy. A stream refill does not establish a bound on its remaining input.
template <typename Char, typename T, ::std::size_t Maximum>
concept bounded_to_scannable = (Maximum != SIZE_MAX) && contiguous_scannable<Char, T> &&
	requires(Char const *first, Char const *last, T &target) {
		{ to_scan_bounded_define(io_reserve_type<Char, ::std::remove_cvref_t<T>>,
			::std::integral_constant<::std::size_t, Maximum>{}, first, last, target) }
			-> ::std::same_as<parse_result<Char const *>>;
	};

/// @brief Proves a type-level upper bound for the descriptor produced by a named conversion source.
/// @details The number is in Char units and bounds every call to print_scatter_define on this source.
///          It may be zero. No scatter is acquired to make this query. A separate reserve writer, storage
///          capacity, padding and the source's size in bytes convey no bound on this representation.
template <typename Char, typename Source>
concept bounded_to_scatter_source = scatter_printable_for<Char, Source &> && requires {
	{ to_scatter_max_size(io_reserve_type<Char, ::std::remove_cvref_t<Source>>) } noexcept
		-> ::std::same_as<::std::size_t>;
	typename ::std::integral_constant<::std::size_t, to_scatter_max_size(io_reserve_type<Char, ::std::remove_cvref_t<Source>>)>;
	requires (to_scatter_max_size(io_reserve_type<Char, ::std::remove_cvref_t<Source>>) != SIZE_MAX);
};

/// @brief Preserves the underlying scatter bound when normal argument transport borrows a large source.
/// @details Both mutable and const descriptors of the underlying type must respect the advertised type-wide bound.
template <::std::integral Char, typename T>
	requires bounded_to_scatter_source<Char, T>
inline consteval ::std::size_t to_scatter_max_size(io_reserve_type_t<Char, parameter<T>>) noexcept
{
	return to_scatter_max_size(io_reserve_type<Char, ::std::remove_cvref_t<T>>);
}

} // namespace fast_io
