#pragma once

namespace fast_io
{

namespace details
{

struct cannot_output_type
{
	inline explicit constexpr cannot_output_type() noexcept = default;
};

/// @brief Recognizes a complete object before any library trait which requires completeness is instantiated.
/// @details `is_object` deliberately accepts forward-declared class types, whereas `sizeof` and several standard
///          triviality/constructibility traits do not. Keeping the completeness probe in a requires-expression makes
///          an incomplete cv/ref object an ordinary false result. Callers must test this concept in an `if constexpr`
///          before forming those traits; spelling all tests in one Boolean expression is not a substitution-safety
///          proof because a variable-template trait may diagnose its incomplete argument while being instantiated.
template <typename T>
concept io_print_complete_object =
	::std::is_object_v<::std::remove_cvref_t<T>> &&
	requires { sizeof(::std::remove_cvref_t<T>); };

/// @brief Computes the exception contract of print aliasing from the selected protocol branch.
/// @details This duplicates only the compile-time branch selection used by `io_print_alias`; it never evaluates the
///          argument.  Keeping the proof beside the dispatcher prevents a throwing ADL customization from being
///          hidden behind an unconditional `noexcept` while retaining useful nothrow information for ordinary values.
template <typename T>
inline constexpr bool io_print_alias_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::alias_printable<T>)
	{
		return noexcept(print_alias_define(::fast_io::io_alias, ::std::declval<T>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

/// @brief Largest object eligible for the normalized print transport's bounded ownership-copy branch.
/// @details This is a copy-work budget, not a formatting capacity or a promise of direct register return. Optional
///          lvalue copies additionally pass the model-specific aggregate-result policy; a non-lvalue may need the
///          bounded copy solely to establish ownership even when the compiler lowers its inevitable return through
///          caller storage.
inline constexpr ::std::size_t io_print_forward_transport_max_value_size{
	::fast_io::details::print_forward_transport_max_value_size};

/// @brief Selects the category-aware value representation shared by print semantic manipulators.
/// @details Trivial copyability alone is not a cost proof: a large pack or condition can be trivial while copying its
///          complete tuple at every semantic boundary. A true lvalue delegates to the shared conservative result
///          policy, while a non-lvalue may take the bounded ownership-copy rescue documented above. Other lvalues use
///          one reference wrapper. The compiler still performs the final ABI classification. The ordered completeness
///          gate is also semantically necessary: a forward-declared lvalue can be transported by reference, but asking
///          a standard triviality trait to classify its referent is not required to be valid.
template <typename T>
inline constexpr bool io_print_forward_transport_by_value = []() constexpr {
	if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::fast_io::details::print_forward_result_copied_from_named_value<T>();
	}
}();

/// @brief Tests whether character-aware normalization must preserve a borrowed producer's object identity.
/// @details A retained scatter may point into the producer itself. Copying a small lvalue and then retaining the
///          descriptor beyond that copy's lifetime is not justified by the ordinary borrowed marker. The independent
///          copy-stable marker is the only proof that the generic ABI-small value rule remains sound for such a source.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_borrowed_source_requires_identity = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	return ((::fast_io::scatter_printable_for<char_type, no_cvref_t &> &&
			 ::fast_io::borrowed_scatter_source<char_type, no_cvref_t>) ||
			((::fast_io::reserve_scatters_printable<char_type, no_cvref_t &> ||
			  ::fast_io::dynamic_reserve_scatters_printable<char_type, no_cvref_t &>) &&
			 ::fast_io::borrowed_reserve_scatters_source<char_type, no_cvref_t>)) &&
		   !::fast_io::copy_stable_borrowed_print_source<char_type, no_cvref_t>;
}();

template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transport_by_value_for = []() constexpr {
	// The provenance CPOs describe complete printable objects. Do not instantiate that protocol graph when the
	// completeness/value gate has already proved that this argument cannot be copied as an ABI-small value.
	if constexpr (!::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return false;
	}
	else
	{
		return !::fast_io::details::io_print_forward_borrowed_source_requires_identity<char_type, T>;
	}
}();

/// @brief Proves that ordinary value-or-reference normalization has a valid executed branch.
/// @details Lvalues either use the already-proved ABI-small copy or one exact-reference wrapper. Every other category
///          must become owned storage; completeness is ordered before `constructible_from` so an incomplete rvalue is
///          constraint-false rather than a standard-library diagnostic.
template <typename T>
inline constexpr bool io_print_forward_transportable = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value<T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Character-aware counterpart of `io_print_forward_transportable`.
/// @details The only different branch is the borrowed-source identity override. It can turn a nominal small-value
///          copy into an exact-reference wrapper for an lvalue, or into ordinary owned construction for a non-lvalue,
///          so transportability is proved after that override is applied.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transportable_for = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Computes the exception contract of the value-or-reference-wrapper print transport.
/// @details The downstream print and concat engines intentionally pass normalized arguments by value. An ABI-small
///          trivial object is copied, while another lvalue is carried by `parameter<T>` without copying its referent.
///          An rvalue must become an owned object because a reference to this helper's parameter would escape at return.
template <typename T>
inline constexpr bool io_print_forward_transport_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable<T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return noexcept(static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		// Aggregate construction stores only the already-existing reference. It cannot invoke the referent's copy,
		// move, allocation, or validation code.
		return true;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

/// @brief Converts one alias/status-forward result into the by-value transport used by the print engines.
/// @details This is deliberately separate from the ADL dispatcher. In particular, a status customization is first
///          evaluated with its exact value category and only its result is normalized. Consequently a noncopyable
///          lvalue result becomes `parameter<exact-reference>` rather than being copied at the first by-value decay
///          boundary. The compact reference wrapper lets the established control, semantic, and concat paths remain
///          ordinary by-value graphs without claiming that every ABI returns the wrapper in a register.
template <typename T>
	requires ::fast_io::details::io_print_forward_transportable<T>
inline constexpr auto io_print_forward_transport(T &&t) noexcept(::fast_io::details::io_print_forward_transport_nothrow<T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value<T>)
	{
		return static_cast<no_cvref_t>(t);
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return ::fast_io::parameter<T>{t};
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_transport_nothrow_for = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::io_print_forward_transportable_for<char_type, T>)
	{
		return false;
	}
	else if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T>)
	{
		return noexcept(static_cast<no_cvref_t>(
			::std::declval<::std::remove_reference_t<T> &>()));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else
	{
		return ::std::is_nothrow_constructible_v<no_cvref_t, T &&>;
	}
}();

/// @brief Applies character-aware value/reference decay without invalidating a retained borrowed descriptor.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::io_print_forward_transportable_for<char_type, T>
inline constexpr auto io_print_forward_transport_for(T &&t) noexcept(::fast_io::details::io_print_forward_transport_nothrow_for<char_type, T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::io_print_forward_transport_by_value_for<char_type, T>)
	{
		return static_cast<no_cvref_t>(t);
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return ::fast_io::parameter<T>{t};
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

/// @brief Computes the exception contract of character-dependent print forwarding.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_nothrow = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return true;
	}
	else if constexpr (::fast_io::status_io_print_forwardable<char_type, T>)
	{
		using forwarded_type = decltype(status_io_print_forward(
			::fast_io::io_alias_type<char_type>, ::std::declval<T>()));
		return noexcept(status_io_print_forward(
				   ::fast_io::io_alias_type<char_type>, ::std::declval<T>())) &&
			   ::fast_io::details::io_print_forward_transport_nothrow_for<char_type, forwarded_type>;
	}
	else
	{
		return ::fast_io::details::io_print_forward_transport_nothrow_for<char_type, T>;
	}
}();

/// @brief Admits the selected print-alias branch without instantiating ownership of an incomplete rvalue.
/// @details A valid ADL alias has already proved its result transport at the CPO boundary. Ordinary lvalues need only
///          retain their reference, while an ordinary rvalue follows the exact owning construction in
///          `io_print_alias`. An invalid customization is ignored like any other unrecognized protocol, but its source
///          still must satisfy the ordinary fallback branch.
template <typename T>
inline constexpr bool io_print_alias_admissible = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t> || ::fast_io::alias_printable<T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<T>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_cvref_t, T &&>;
	}
}();

/// @brief Proves that `io_print_forward` can execute the branch selected after status recognition.
template <::std::integral char_type, typename T>
inline constexpr bool io_print_forward_admissible = []() constexpr {
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t> ||
				  ::fast_io::status_io_print_forwardable<char_type, T>)
	{
		// The status concept includes the exact rvalue-result ownership proof.
		return true;
	}
	else
	{
		return ::fast_io::details::io_print_forward_transportable_for<char_type, T>;
	}
}();

/// @brief Proves that scan-alias normalization can execute its selected transport branch.
/// @details Function and valid alias branches construct their own known result. Every lvalue fallback is borrowed,
///          including an immovable manipulator. A non-lvalue fallback must instead cross the return boundary as owned
///          `remove_reference_t<T>` storage; completeness is tested before the constructibility trait so a forward-
///          declared or immovable rvalue makes the call constraint-false rather than diagnosing inside the function.
template <typename T>
inline constexpr bool io_scan_alias_admissible = []() constexpr {
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (::std::is_function_v<no_ref_t> || ::fast_io::alias_scannable<T> ||
				  ::std::is_lvalue_reference_v<T &&>)
	{
		return true;
	}
	else if constexpr (!::fast_io::details::io_print_complete_object<no_ref_t>)
	{
		return false;
	}
	else
	{
		return ::std::constructible_from<no_ref_t, T &&>;
	}
}();

} // namespace details

/// @brief Applies the print-alias customization for the argument's actual value category.
/// @details The alias probe intentionally uses `T`, not `remove_cvref_t<T>`.  An alias may be valid only for a
///          mutable lvalue, or may provide distinct lvalue and rvalue overloads; probing a synthesized rvalue would
///          recognize the wrong protocol.  Alias CPO results retain their declared value category, which permits a
///          noncopyable lvalue proxy.  An ordinary rvalue is instead materialized as a value: returning the forwarding
///          reference to this function parameter would leave a dangling reference as soon as the helper returned.
///          No unconditional `noexcept` is stated because an alias is allowed to validate or allocate.
template <typename T>
	requires ::fast_io::details::io_print_alias_admissible<T>
inline constexpr decltype(auto) io_print_alias(T &&t) noexcept(::fast_io::details::io_print_alias_nothrow<T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (alias_printable<T>)
	{
		return print_alias_define(io_alias, ::std::forward<T>(t));
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return (t);
	}
	else
	{
		return no_cvref_t(::std::forward<T>(t));
	}
}

/// @brief Applies character-dependent print forwarding and produces a cheap by-value transport.
/// @details A status-forward customization is called before transport selection, so overload resolution observes the
///          original aliased value category. Its result is the final printable representation: it is transported
///          directly and is never passed through `io_print_alias` a second time. The result then follows the same rule
///          as an ordinary argument: safely copyable small values remain values, while nontrivial or noncopyable
///          lvalues become `parameter<exact-reference>` and rvalues become owned values. This invariant is what
///          permits every downstream dispatcher to remain
///          by-value without erasing mutability, const qualification, or noncopyable proxy identity. Customization
///          exceptions propagate because neither the status-forward nor alias protocol requires a non-throwing CPO.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::io_print_forward_admissible<char_type, T>
inline constexpr auto io_print_forward(T &&t) noexcept(::fast_io::details::io_print_forward_nothrow<char_type, T>)
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::std::is_function_v<no_cvref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (status_io_print_forwardable<char_type, T>)
	{
		return ::fast_io::details::io_print_forward_transport_for<char_type>(
			status_io_print_forward(io_alias_type<char_type>, ::std::forward<T>(t)));
	}
	else
	{
		return ::fast_io::details::io_print_forward_transport_for<char_type>(::std::forward<T>(t));
	}
}

/// @brief Applies character-dependent scan forwarding without copying an existing proxy reference.
/// @details Both the source expression and a status-forward result obey the same borrow-or-own rule. An lvalue result
///          retains its exact reference identity, which permits noncopyable proxy customizations. A prvalue or xvalue
///          result is materialized as a value so this helper never returns a reference to its parameter or to a status
///          customization's expiring subobject. `status_io_scan_forwardable` proves this exact construction before the
///          branch is selected, making an incomplete or immovable non-lvalue result a substitution failure rather than
///          a delayed body diagnostic. Custom forwarding exceptions remain visible because the protocol does not promise
///          `noexcept`.
template <::std::integral char_type, typename T>
inline constexpr decltype(auto) io_scan_forward(T &&t)
{
	if constexpr (status_io_scan_forwardable<char_type, T>)
	{
		using result_type = decltype(status_io_scan_forward(
			io_alias_type<char_type>, ::std::forward<T>(t)));
		if constexpr (::std::is_lvalue_reference_v<result_type>)
		{
			return status_io_scan_forward(io_alias_type<char_type>, ::std::forward<T>(t));
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(status_io_scan_forward(
				io_alias_type<char_type>, ::std::forward<T>(t)));
		}
	}
	else if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		return (t);
	}
	else
	{
		return ::std::remove_cvref_t<T>(::std::forward<T>(t));
	}
}

/// @brief Normalizes a public scan target while preserving alias reference semantics.
/// @details A custom alias may return a noncopyable reference proxy; `decltype(auto)` retains it. Ordinary targets are
///          wrapped in a small parameter transport: an lvalue transport stores its exact reference and a direct rvalue
///          transport owns the object. Existing manipulators follow the same borrow-or-own rule. Returning a reference
///          to either kind of rvalue function parameter would dangle immediately after this helper call. Public `scan`
///          normally presents its named target as an lvalue, so ownership affects only direct normalization and does
///          not add a copy to the common path. The function is not unconditionally `noexcept`, because an alias
///          customization or rvalue construction may validate, allocate, or throw.
template <typename T>
	requires ::fast_io::details::io_scan_alias_admissible<T>
inline constexpr decltype(auto) io_scan_alias(T &&t)
{
	using no_ref_t = ::std::remove_reference_t<T>;
	if constexpr (::std::is_function_v<no_ref_t>)
	{
		return ::fast_io::details::cannot_output_type{};
	}
	else if constexpr (alias_scannable<T>)
	{
		// Recognition and invocation use the same value category. The former bare-type probe tested a named lvalue but
		// could then call an rvalue-only expression (or the reverse), producing a hard error after concept admission.
		return scan_alias_define(io_alias, ::std::forward<T>(t));
	}
	else if constexpr (manipulator<no_ref_t>)
	{
		if constexpr (::std::is_lvalue_reference_v<T &&>)
		{
			// The lvalue branch returns the exact existing object, so the classification marker needs no copy/move proof.
			return (t);
		}
		else
		{
			// The entry constraint proves this exact `no_ref_t(T&&)` expression before the owning branch is instantiated.
			// Ownership is mandatory here because returning a reference to the forwarding parameter would dangle.
			return no_ref_t(::std::forward<T>(t));
		}
	}
	else
	{
		if constexpr (::std::is_lvalue_reference_v<T &&>)
		{
			return parameter<no_ref_t &>{t};
		}
		else
		{
			return parameter<no_ref_t>{::std::forward<T>(t)};
		}
	}
}

} // namespace fast_io
