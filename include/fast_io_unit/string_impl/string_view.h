#pragma once

namespace fast_io
{
namespace details
{

/** Fixed-capacity carrier used only after every visible text code unit is compiler-known. */
template <::std::integral char_type>
inline constexpr ::std::size_t compiler_constant_text_code_unit_limit{
	(::fast_io::details::compiler_constant_materialization_max_bytes -
	 sizeof(::std::size_t)) /
	sizeof(char_type)};

template <::std::integral char_type>
struct compiler_constant_text_materialized
{
	inline static constexpr ::std::size_t capacity{
		compiler_constant_text_code_unit_limit<char_type>};
	char_type const *data{};
	::std::size_t size{};
};

/** Tests every in-range code unit without reading one which the source does not contain.
 *
 * The caller first proves that `size` itself is compiler-known.  Consequently
 * every `index >= size` guard folds before its indexed load.  A constant
 * address alone is deliberately insufficient: a pointer to mutable global
 * storage must fail unless the optimizer also proves every observed value.
 */
template <::std::integral char_type, ::std::size_t... index>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool compiler_constant_text_known_impl(
		char_type const *data, ::std::size_t size,
		::std::index_sequence<index...>) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return ((index >= size || __builtin_constant_p(data[index])) && ...);
#else
	(void)data;
	(void)size;
	return false;
#endif
}

/** Proves a bounded text spelling at the optimizer-visible source boundary. */
template <::std::integral char_type>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool compiler_constant_text_known(
		char_type const *data, ::std::size_t size) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if (!__builtin_constant_p(size) ||
		size > compiler_constant_text_code_unit_limit<char_type>)
	{
		return false;
	}
	return compiler_constant_text_known_impl(
		data, size,
		::std::make_index_sequence<
			compiler_constant_text_code_unit_limit<char_type>>{});
#else
	(void)data;
	(void)size;
	return false;
#endif
}

/** Captures one already-proved text spelling in a synchronous borrowed carrier.
 *
 * Public print, concat, and to keep every source expression alive until their
 * selected operation completes.  Retaining this pointer therefore preserves
 * the standard string/view lifetime while avoiding a fixed-capacity proxy
 * copy; the reserve emitter below is the sole character copy.
 */
template <::std::integral char_type>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr compiler_constant_text_materialized<char_type>
	compiler_constant_text_materialize(
		char_type const *data, ::std::size_t size) noexcept
{
	if (compiler_constant_text_materialized<char_type>::capacity < size)
	{
		::fast_io::fast_terminate();
	}
	return {data, size};
}

} // namespace details

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>) noexcept
{
	return ::fast_io::details::compiler_constant_text_code_unit_limit<char_type>;
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>,
	char_type *output,
	::fast_io::details::compiler_constant_text_materialized<char_type> const &
		value) noexcept
{
	if (value.capacity < value.size)
	{
		::fast_io::fast_terminate();
	}
	return ::fast_io::details::non_overlapped_copy_n(
		value.data, value.size, output);
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<
		char_type,
		::fast_io::details::compiler_constant_text_materialized<char_type>>) noexcept
{
	// This internal proxy is an immutable pointer/length spelling emitted by a bounded non-throwing copy.
	return {};
}

#if defined(_GLIBCXX_STRING_VIEW) || defined(_LIBCPP_STRING_VIEW) || defined(_STRING_VIEW_)

/// @brief Opts standard string views into retained-scatter composition when stored as range lvalue elements.
/// @details Their alias points directly at the view's `[data(), data()+size())` range. The source-side marker is kept
///          here, beside the standard string integration, instead of inferring lifetime merely from a scatter-shaped
///          alias; that separation prevents unrelated scratch-producing aliases from receiving the same permission.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// Custom traits add an ADL namespace and may replace the view's otherwise trivial print protocol.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// The default view contributes only its stored pointer and length and has no formatter state or failure path.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// A string_view scatter is exactly its stored pointer/length pair and is independent of every output object.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// The view's complete print semantics are the characters in its stored pointer/length pair.
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<char_type, ::std::basic_string_view<char_type, traits_type>>) noexcept
{
	// Moving or destroying the standard view never changes the lifetime of its externally owned character range. This
	// stronger proof lets normalization copy the two-word view before retaining its alias. Custom traits remain excluded:
	// their associated namespace may replace alias/status forwarding with an identity-sensitive representation.
	return {};
}

/** Applies the same value-complete constant proof to a standard string view. */
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr bool
	print_compiler_constant_materialization_eligible(
		io_reserve_type_t<
			char_type,
			::std::basic_string_view<char_type, traits_type>>,
		::std::basic_string_view<char_type, traits_type> value) noexcept
{
	return ::fast_io::details::compiler_constant_text_known(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__clang__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
	[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
		io_reserve_type_t<
			char_type,
			::std::basic_string_view<char_type, traits_type>>,
		::std::basic_string_view<char_type, traits_type> value) noexcept
{
	return ::fast_io::details::compiler_constant_text_materialize(
		value.data(), value.size());
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

/// @brief Records the pointer/length/content query classification for a standard string view.
/// @details Borrowed-text consumer gates remain responsible for keeping this graph FCO when copying cannot remove work.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

/// @brief Classifies the standard view's exact externally borrowed spelling.
/// @details The marker is independent of character constancy and lifetime
///          ownership. The existing copy-stability and synchronous-consumption
///          proofs remain responsible for any consumer which retains its range.
template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_borrowed_text_leaf(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename traits_type>
	requires ::std::same_as<traits_type, ::std::char_traits<char_type>>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	io_reserve_type_t<
		char_type,
		::std::basic_string_view<char_type, traits_type>>) noexcept
{
	return {};
}
#endif
} // namespace fast_io
