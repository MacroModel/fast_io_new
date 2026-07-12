#pragma once

#include "forward.h"

namespace fast_io
{

namespace details
{

template <typename T>
inline constexpr bool is_print_pack_v = false;

template <typename... Args>
inline constexpr bool is_print_pack_v<::fast_io::manipulators::pack_t<Args...>> = true;

template <typename T>
concept print_pack = is_print_pack_v<::std::remove_cvref_t<T>>;

} // namespace details

namespace details::decay
{

template <typename T>
inline constexpr bool print_semantic_condition_v = false;

template <typename T1, typename T2>
inline constexpr bool
	print_semantic_condition_v<::fast_io::manipulators::condition<T1, T2>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v = false;

template <::fast_io::manipulators::scalar_placement placement, typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_t<placement, T>> = true;

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
inline constexpr bool
	print_semantic_width_v<::fast_io::manipulators::width_ch_t<placement, T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_t<T>> = true;

template <typename T, ::std::integral ch_type>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_node_no_parameter_v =
	::fast_io::details::print_pack<T> ||
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_width_v<::std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v<::fast_io::parameter<T>> = true;

template <typename T>
inline constexpr bool print_semantic_parameter_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_v<::fast_io::parameter<T>> =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
concept print_semantic_node =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
struct print_semantic_width_traits
{};

template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_t<placement, T>>
{
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = false;
};

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = true;
};

template <typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_t<T>>
{
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = false;
};

template <typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = true;
};

template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single;

template <::std::integral char_type, typename T>
using print_semantic_forwarded_arg_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

/// @brief    Counts the maximum number of emitted leaves represented by a semantic print type.
/// @details  Ordinary values contribute one leaf. Semantic node specializations recursively describe their active
///           output shape so strategy selection can distinguish compact compositions from long materializations.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_leaf_count_impl : ::std::integral_constant<::std::size_t, 1u>
{};

/// @brief    Normalizes cv-reference qualifiers before computing a semantic output leaf count.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_leaf_count
	: ::fast_io::details::decay::print_semantic_leaf_count_impl<::std::remove_cvref_t<T>>
{};

/// @brief    Propagates semantic leaf counting through a parameter wrapper.
/// @tparam   T the wrapped semantic type
template <typename T>
struct print_semantic_leaf_count_impl<::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Sums semantic leaf counts while rejecting compile-time size overflow.
/// @tparam   Args the semantic types whose leaf counts are accumulated
/// @return   ::std::size_t the checked total semantic leaf count
template <typename... Args>
inline consteval ::std::size_t print_semantic_leaf_count_sum() noexcept
{
	::std::size_t total{};
	((total = ::fast_io::details::intrinsics::add_or_overflow_die(
		  total, ::fast_io::details::decay::print_semantic_leaf_count<Args>::value)),
	 ...);
	return total;
}

/// @brief    Sums the semantic leaf counts of every stored pack element.
/// @tparam   Args the stored pack element types
template <typename... Args>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::pack_t<Args...>>
	: ::std::integral_constant<::std::size_t,
							   ::fast_io::details::decay::print_semantic_leaf_count_sum<Args...>()>
{};

/// @brief    Uses the larger alternative as a condition node's compile-time leaf-count bound.
/// @tparam   T1 the true alternative type
/// @tparam   T2 the false alternative type
template <typename T1, typename T2>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::condition<T1, T2>>
	: ::std::integral_constant<
		  ::std::size_t,
		  (::fast_io::details::decay::print_semantic_leaf_count<T1>::value <
		   ::fast_io::details::decay::print_semantic_leaf_count<T2>::value)
			  ? ::fast_io::details::decay::print_semantic_leaf_count<T2>::value
			  : ::fast_io::details::decay::print_semantic_leaf_count<T1>::value>
{};

/// @brief    Propagates semantic leaf counting through a statically placed width node.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a statically placed width node with an explicit fill character.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
/// @tparam   ch_type   the fill character type
template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a run-time placed width node.
/// @tparam   T the formatted child type
template <typename T>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief    Propagates semantic leaf counting through a run-time placed width node with an explicit fill character.
/// @tparam   T       the formatted child type
/// @tparam   ch_type the fill character type
template <typename T, ::std::integral ch_type>
struct print_semantic_leaf_count_impl<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::fast_io::details::decay::print_semantic_leaf_count<T>
{};

/// @brief  Maximum leaf count for selecting a separate run-time precise-size traversal.
/// @details Eight-leaf and larger statically bounded compositions favor one-pass bounded materialization according to
///          concat, fake-system-call, null-device, and file-sink benchmarks.
inline constexpr ::std::size_t print_semantic_precise_materialization_leaf_threshold{8u};

/// @brief    Detects whether a semantic print type contains width formatting.
/// @details  Width-bearing compositions use a distinct no-coalescing strategy because exact measurement can otherwise
///           be repeated by both the enclosing semantic run and the width dispatcher.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_contains_width_impl : ::std::false_type
{};

/// @brief    Normalizes cv-reference qualifiers before detecting width formatting.
/// @tparam   T the semantic node or leaf type
template <typename T>
struct print_semantic_contains_width
	: ::fast_io::details::decay::print_semantic_contains_width_impl<::std::remove_cvref_t<T>>
{};

/// @brief    Propagates width detection through a parameter wrapper.
/// @tparam   T the wrapped semantic type
template <typename T>
struct print_semantic_contains_width_impl<::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_contains_width<T>
{};

/// @brief    Detects width formatting in any stored pack element.
/// @tparam   Args the stored pack element types
template <typename... Args>
struct print_semantic_contains_width_impl<::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_contains_width<Args>::value || ...)>
{};

/// @brief    Detects width formatting in either condition alternative.
/// @tparam   T1 the true alternative type
/// @tparam   T2 the false alternative type
template <typename T1, typename T2>
struct print_semantic_contains_width_impl<::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_contains_width<T1>::value ||
		  ::fast_io::details::decay::print_semantic_contains_width<T2>::value>
{};

/// @brief    Marks a statically placed width node as width-bearing.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_t<placement, T>> : ::std::true_type
{};

/// @brief    Marks a statically placed width node with an explicit fill character as width-bearing.
/// @tparam   placement the compile-time scalar placement
/// @tparam   T         the formatted child type
/// @tparam   ch_type   the fill character type
template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::true_type
{};

/// @brief    Marks a run-time placed width node as width-bearing.
/// @tparam   T the formatted child type
template <typename T>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_runtime_t<T>> : ::std::true_type
{};

/// @brief    Marks a run-time placed width node with an explicit fill character as width-bearing.
/// @tparam   T       the formatted child type
/// @tparam   ch_type the fill character type
template <typename T, ::std::integral ch_type>
struct print_semantic_contains_width_impl<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::true_type
{};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl
{
	inline static constexpr bool available = ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;
	inline static constexpr ::std::size_t size = available ? 0u : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size
	: ::fast_io::details::decay::print_semantic_static_precise_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
	requires(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
			 ::fast_io::static_precise_reserve_printable<char_type, T>)
struct print_semantic_static_precise_size_impl<char_type, T>
{
	inline static constexpr bool available = true;
	inline static constexpr ::std::size_t size{
		print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>)};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_precise_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool available =
		(::fast_io::details::decay::print_semantic_static_precise_size<
			 char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::intrinsics::add_or_overflow_die(
				  total, ::fast_io::details::decay::print_semantic_static_precise_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available =
		first_size::available && second_size::available && (first_size::size == second_size::size);
	inline static constexpr ::std::size_t size = available ? first_size::size : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size_impl
{
	inline static constexpr bool available =
		::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
		(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
		 (::fast_io::static_precise_reserve_printable<char_type, T> ||
		  ::fast_io::reserve_printable<char_type, T>));

	inline static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			return 0u;
		}
		else if constexpr (!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
						   ::fast_io::static_precise_reserve_printable<char_type, T>)
		{
			return print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>);
		}
		else if constexpr (!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
						   ::fast_io::reserve_printable<char_type, T>)
		{
			return print_reserve_size(::fast_io::io_reserve_type<char_type, T>);
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size
	: ::fast_io::details::decay::print_semantic_static_bounded_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_bounded_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool available =
		(::fast_io::details::decay::print_semantic_static_bounded_size<
			 char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	inline static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::intrinsics::add_or_overflow_die(
				  total, ::fast_io::details::decay::print_semantic_static_bounded_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_bounded_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_bounded_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_bounded_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available = first_size::available && second_size::available;

	inline static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			if constexpr (first_size::size < second_size::size)
			{
				return second_size::size;
			}
			else
			{
				return first_size::size;
			}
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_precise_leaf_size_ok_v =
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
	::fast_io::static_precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::scatter_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::reserve_scatters_printable<char_type, ::std::remove_cvref_t<T>>;

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok
	: ::fast_io::details::decay::print_semantic_precise_size_ok_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_precise_size_ok<
			   char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>::value && ::fast_io::details::decay::print_semantic_precise_size_ok<char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_bounded_leaf_size_ok_v =
	::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T> ||
	::fast_io::dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::reserve_printable<char_type, ::std::remove_cvref_t<T>>;

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok_impl
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_bounded_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok
	: ::fast_io::details::decay::print_semantic_bounded_size_ok_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_bounded_size_ok<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_bounded_size_ok<
			   char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>::value && ::fast_io::details::decay::print_semantic_bounded_size_ok<char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_bounded_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_bounded_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_bounded_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> && ::fast_io::details::decay::print_semantic_bounded_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay : ::std::false_type
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::parameter<T>>
	: print_semantic_params_okay<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (print_freestanding_decay_param_okay_single<
			   char_type, print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T1>>::value &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

} // namespace details::decay

} // namespace fast_io
