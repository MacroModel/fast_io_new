#pragma once

namespace fast_io
{

template <::std::integral char_type, typename T>
struct io_strlike_type_t
{
	inline explicit constexpr io_strlike_type_t() noexcept = default;
};

template <::std::integral char_type, typename T>
inline constexpr io_strlike_type_t<char_type, T> io_strlike_type{};

namespace details
{

/// @brief Rejects incomplete and non-object result types before construction traits are instantiated.
template <typename T>
inline consteval bool strlike_result_object_impl() noexcept
{
	if constexpr (!::std::same_as<T, ::std::remove_cvref_t<T>> || !::std::is_object_v<T> ||
		::std::is_array_v<T> || !requires { sizeof(T); })
	{
		return false;
	}
	else
	{
		// Standard construction traits may diagnose an incomplete class. The branch above is therefore a substitution
		// gate, not merely documentation of the result object's lifetime requirement.
		return ::std::is_default_constructible_v<T>;
	}
}

template <typename T>
concept strlike_result_object = ::fast_io::details::strlike_result_object_impl<T>();

/// @brief Proves the exact writable cursor protocol consumed by the generic string output adapter.
/// @details Expression existence is insufficient here: concat subtracts and writes through all three cursors, publishes
///          the same pointer type, and treats reserve/set-current as effect-only operations. Exact results keep malformed
///          proxy cursors and count-returning mutations from becoming concept-true only to fail in an adapter body.
template <typename char_type, typename T>
concept buffer_strlike_impl = requires(T &t) {
	{ strlike_begin(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_curr(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	{ strlike_end(io_strlike_type<char_type, T>, t) } -> ::std::same_as<char_type *>;
	requires requires(char_type *ptr) {
		{ strlike_set_curr(io_strlike_type<char_type, T>, t, ptr) } -> ::std::same_as<void>;
	};
	requires requires(::std::size_t n) {
		{ strlike_reserve(io_strlike_type<char_type, T>, t, n) } -> ::std::same_as<void>;
	};
};
} // namespace details

/// @brief Proves the exact contiguous-range construction alternative of a string-like result.
/// @details Buffer and construction protocols are independent. Naming this branch lets concat test it directly instead
///          of assuming that any similarly named ADL function returns the result type required by its return statement.
template <typename char_type, typename T>
concept range_constructible_strlike =
	::std::integral<char_type> && ::fast_io::details::strlike_result_object<T> &&
	requires(char_type const *first) {
		{ strlike_construct_define(io_strlike_type<char_type, T>, first, first) } -> ::std::same_as<T>;
	};

template <typename char_type, typename T>
concept strlike =
	::std::integral<char_type> && ::fast_io::details::strlike_result_object<T> &&
	(range_constructible_strlike<char_type, T> || ::fast_io::details::buffer_strlike_impl<char_type, T>);

template <typename char_type, typename T>
concept single_character_constructible_strlike = strlike<char_type, T> && requires(char_type ch) {
	{ strlike_construct_single_character_define(io_strlike_type<char_type, T>, ch) } -> ::std::same_as<T>;
};

template <typename char_type, typename T>
concept alias_strlike = requires(T &t) { strlike_alias_define(io_alias, t); };

template <typename char_type, typename T>
concept buffer_strlike = strlike<char_type, T> && ::fast_io::details::buffer_strlike_impl<char_type, T>;

template <typename char_type, typename T>
concept auxiliary_strlike = strlike<char_type, T> && requires(T &t, char_type ch, char_type const *ptr) {
	{ strlike_push_back(io_strlike_type<char_type, T>, t, ch) } -> ::std::same_as<void>;
	{ strlike_append(io_strlike_type<char_type, T>, t, ptr, ptr) } -> ::std::same_as<void>;
};

template <typename char_type, typename T>
concept sso_buffer_strlike = buffer_strlike<char_type, T> &&
							 requires {
								 {
									 strlike_sso_size(io_strlike_type<char_type, T>)
								 } -> ::std::same_as<::std::size_t>;
							 };

/// @brief Marks a string-like output adapter whose amortized-growth path is a preferred print destination.
/// @details `strlike` and `buffer_strlike` describe callable storage protocols, not their costs. In particular, a
///          user-defined adapter may flush externally, allocate on every append, or expose a deliberately small fixed
///          area. Inferring this policy from cursor syntax would make such a type enter range/context strategies which
///          were measured only for reusable storage. The exact-`true_type` CPO is therefore an explicit promise by the
///          underlying string-like implementation that synchronous incremental output normally reuses destination-
///          owned storage with amortized growth. It grants no cursor-folding permission; that stronger semantic proof
///          is represented independently below.
/// @fn      strlike_buffered_print_preferred
/// @return  std::true_type
template <typename char_type, typename T>
concept buffered_print_preferred_strlike = strlike<char_type, T> && requires {
	{
		strlike_buffered_print_preferred(io_strlike_type<char_type, T>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Marks an underlying buffer-string protocol whose put-area cursor publications may be folded.
/// @details Structural buffer conformance proves only that cursor and reserve expressions exist. It cannot prove that
///          the area stays put between raw writes, that `strlike_set_curr` has no effect beyond publishing the cursor,
///          or that output/status/locking customizations associated with `T` are observationally equivalent to direct
///          scatter copies. This explicit opt-in supplies those output-side facts to
///          `io_strlike_reference_wrapper`; the range strategy still requires independent source-side lifetime,
///          cursor-independence, and direct-print-equivalence proofs. Keeping the marker on `T` is important because a
///          class template argument contributes its namespace to ADL: a blanket wrapper marker would silently certify
///          user-defined hooks which the wrapper itself cannot inspect or exclude.
/// @fn      strlike_deferred_obuffer_commit_safe
/// @return  std::true_type
template <typename char_type, typename T>
concept deferred_obuffer_commit_safe_strlike = buffer_strlike<char_type, T> && requires {
	{
		strlike_deferred_obuffer_commit_safe(io_strlike_type<char_type, T>)
	} -> ::std::same_as<::std::true_type>;
};

/// @brief Proves that a string-like result can establish one exact writable logical extent.
/// @details This capability is deliberately independent of `buffer_strlike`. The CPO must first make the destination's
///          observable size exactly `n`, then return the beginning of one contiguous array containing at least `n`
///          live mutable `char_type` objects. `[result, result + n)` remains valid until a later non-const operation or
///          destruction. No character beyond that logical range is exposed and no cursor mutation is implied. The CPO
///          may throw; returning proves only destination storage and lifetime, while the source's precise-print protocol
///          separately proves the required extent. This separation permits portable `std::basic_string::resize` use
///          without treating spare capacity as constructed character storage.
/// @fn      strlike_precise_resize_and_get_begin
/// @param   io_strlike_type<char_type, T> exact destination tag
/// @param   T&                            destination being constructed
/// @param   std::size_t                   exact final character count
/// @return  char_type*                    beginning of the live writable logical range
template <typename char_type, typename T>
concept precise_resize_writable_strlike = strlike<char_type, T> && requires(T &str, ::std::size_t n) {
	{
		strlike_precise_resize_and_get_begin(io_strlike_type<char_type, T>, str, n)
	} -> ::std::same_as<char_type *>;
};

/// @brief Proves that exact resize creates writable characters without first initializing the overwritten range.
/// @details `precise_resize_writable_strlike` is a lifetime/capability contract and intentionally permits portable
///          `std::basic_string::resize`, which value-initializes every new character. This refinement is only a cost
///          proof. Returning true states that establishing logical size `n` does not perform a character-writing pass
///          over `[begin, begin + n)` before the exact formatter writes that complete range. Ordinary concat uses the
///          proof for initialization-sensitive leaves; semantic concat retains its independent whole-graph policy.
///          The marker grants no permission to write outside the live logical range established by the resize CPO.
/// @fn      strlike_precise_resize_without_initialization
/// @return  std::true_type
template <typename char_type, typename T>
concept precise_resize_without_initialization_strlike =
	precise_resize_writable_strlike<char_type, T> && requires {
		{
			strlike_precise_resize_without_initialization(io_strlike_type<char_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

} // namespace fast_io
