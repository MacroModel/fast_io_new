#pragma once

#include "forward.h"

namespace fast_io
{

namespace details
{

template <typename T>
using width_alias_result = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief Selects the representation retained by a width semantic node.
/// @details The original source category is deliberately kept as part of this decision. Applying
///          `io_print_forward_transport` only to the alias result would lose that evidence: an alias invoked on an
///          rvalue is permitted to return an lvalue reference to one of the source's subobjects, and wrapping that
///          reference would let it escape the source temporary. Therefore every rvalue-derived result is materialized.
///          An alias reference obtained from an lvalue retains its exact cv/ref identity so noncopyable mutable proxies
///          remain usable. An ordinary ABI-small trivial lvalue is copied as the established calling-convention
///          optimization; every other ordinary lvalue is a borrowed exact reference. Consequently a width node that
///          stores an lvalue reference is a view and must not outlive that source object.
template <typename T>
using width_storage_type = ::std::conditional_t<
	::std::is_function_v<::std::remove_cvref_t<T>>,
	::std::remove_cvref_t<::fast_io::details::width_alias_result<T>>,
	::std::conditional_t<
		::std::is_lvalue_reference_v<T &&> && ::fast_io::alias_printable<T> &&
			::std::is_lvalue_reference_v<::fast_io::details::width_alias_result<T>>,
		::fast_io::details::width_alias_result<T>,
		::std::conditional_t<
			::std::is_lvalue_reference_v<T &&> && !::fast_io::alias_printable<T>,
			::std::conditional_t<::fast_io::details::io_print_forward_transport_by_value<T>,
								 ::std::remove_cvref_t<T>, T>,
			::std::remove_cvref_t<::fast_io::details::width_alias_result<T>>>>>;

/// @brief Tests the exact conversion used to initialize a width node's stored child.
/// @details Expressing the conversion as a requirement, rather than only using `is_constructible`, models the actual
///          explicit alias-to-storage conversion and cleanly rejects an rvalue alias that exposes a noncopyable borrowed
///          subobject. Guaranteed copy elision can nevertheless make an immovable owned prvalue pass that first test.
///          Such a child cannot cross the normalized semantic node's later by-value boundary, so owned storage also has
///          to be constructible from its rvalue category. Reference storage already preserves an existing identity and
///          intentionally needs no move proof. This is the same compositional contract used by condition and pack nodes.
template <typename T>
concept width_storable =
	requires {
		static_cast<::fast_io::details::width_storage_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>()));
	} &&
	(::std::is_lvalue_reference_v<::fast_io::details::width_storage_type<T>> ||
	 ::std::constructible_from<
		 ::std::remove_cvref_t<::fast_io::details::width_storage_type<T>>,
		 ::std::remove_cvref_t<::fast_io::details::width_storage_type<T>> &&>);

/// @brief Computes whether width child normalization can propagate an exception.
/// @details The expression includes both the selected alias CPO and the materialization/copy required by the storage
///          policy. It is kept conditional so an unstorable adversarial alias remains a clean constraint failure rather
///          than producing a second diagnostic from a factory's exception specification.
template <typename T>
inline constexpr bool width_storage_nothrow_constructible = []() constexpr {
	if constexpr (::fast_io::details::width_storable<T>)
	{
		return noexcept(static_cast<::fast_io::details::width_storage_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>())));
	}
	else
	{
		return false;
	}
}();

/// @brief Normalizes one width child according to `width_storage_type`.
/// @details The explicit cast is the single construction point shared by all placement and fill-character factories;
///          this prevents those overloads from drifting into different alias, lifetime, or exception rules.
template <typename T>
	requires ::fast_io::details::width_storable<T>
inline constexpr ::fast_io::details::width_storage_type<T> width_store(T &&t)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	return static_cast<::fast_io::details::width_storage_type<T>>(
		::fast_io::io_print_alias(::std::forward<T>(t)));
}

} // namespace details

namespace manipulators
{

template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto width(scalar_placement placement, T &&t, ::std::size_t n)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_runtime_t<storage_type>{
		placement, ::fast_io::details::width_store(::std::forward<T>(t)), n};
}

template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto width(scalar_placement placement, T &&t, ::std::size_t n, char_type ch)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_runtime_ch_t<storage_type, char_type>{
		placement, ::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto left(T &&t, ::std::size_t n)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::left, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto middle(T &&t, ::std::size_t n)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::middle, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto right(T &&t, ::std::size_t n)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::right, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto internal(T &&t, ::std::size_t n)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::internal, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto left(T &&t, ::std::size_t n, char_type ch)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::left, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto middle(T &&t, ::std::size_t n, char_type ch)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::middle, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto right(T &&t, ::std::size_t n, char_type ch)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::right, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto internal(T &&t, ::std::size_t n, char_type ch)
	noexcept(::fast_io::details::width_storage_nothrow_constructible<T>)
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::internal, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

} // namespace manipulators

/// @brief Propagates an established read proof through each width semantic representation.
/// @details Width contributes padding but obtains its external source range exclusively from the stored child. Fixed
///          versus run-time placement and default versus explicit fill characters do not change that provenance. These
///          four overloads are kept distinct because the representation types are distinct protocol nodes; a generic
///          structural rule would accidentally certify unrelated user types with similarly named members.
template <manipulators::scalar_placement placement, typename T>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <manipulators::scalar_placement placement, typename T, ::std::integral char_type>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_ch_t<placement, T, char_type>>) noexcept
{
	return {};
}

template <typename T>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <typename T, ::std::integral char_type>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_runtime_ch_t<T, char_type>>) noexcept
{
	return {};
}

#if 0
namespace details
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_reserve_size_width_impl(T t, ::std::size_t wid)
{
	if constexpr (reserve_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)};
		if (wid < sz)
		{
			return sz;
		}
	}
	else if constexpr (dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		::std::size_t sz{print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)};
		if (wid < sz)
		{
			return sz;
		}
	}
	else if constexpr (scatter_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		auto sz{print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t).len};
		if (wid < sz)
		{
			return sz;
		}
	}
	return wid;
}

template <typename char_type, typename T>
concept print_reserve_static_stack_size_width_ok =
	::std::integral<char_type> && (reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
								   dynamic_reserve_with_possible_static_stack_size<char_type, ::std::remove_cvref_t<T>>);

template <::std::integral char_type, typename T>
	requires print_reserve_static_stack_size_width_ok<char_type, T>
inline constexpr ::std::size_t print_reserve_static_stack_size_width_impl() noexcept
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (reserve_printable<char_type, value_type>)
	{
		return print_reserve_size(io_reserve_type<char_type, value_type>);
	}
	else
	{
		return print_reserve_static_stack_size(io_reserve_type<char_type, value_type>);
	}
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type>
inline constexpr char_type *handle_common_ch(char_type *first, char_type *last, ::std::size_t wd, char_type fillch)
{
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	if (wd <= diff)
	{
		return last;
	}
	::std::size_t const to_fill_chs{wd - diff};
	if constexpr (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		my_fill_n(last, to_fill_chs, fillch);
	}
	else if constexpr (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		constexpr ::std::size_t one{1};
		::std::size_t const left_indent{static_cast<::std::size_t>(to_fill_chs >> one)};
		::std::size_t const right_indent{to_fill_chs - left_indent};
		my_copy_right_shift(first, last, left_indent);
		my_fill_n(first, left_indent, fillch);
		my_fill_n(first + wd - right_indent, right_indent, fillch);
	}
	else
	{
		my_copy_right_shift(first, last, to_fill_chs);
		my_fill_n(first, to_fill_chs, fillch);
	}
	return first + wd;
}

template <::std::integral char_type>
inline constexpr char_type *handle_common_internal_ch(char_type *first, char_type *last, ::std::size_t wd,
													  char_type fillch, ::std::size_t internal_len)
{
	::std::size_t const diff1{static_cast<::std::size_t>(last - first)};
	if (wd <= diff1 || diff1 < internal_len)
	{
		return last;
	}
	first += internal_len;
	wd -= internal_len;
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t const to_fill_chs{wd - diff};
	my_copy_right_shift(first, last, to_fill_chs);
	my_fill_n(first, to_fill_chs, fillch);
	return first + wd;
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type, typename T>
inline constexpr char_type *print_reserve_define_width_ch_impl(char_type *iter, T t, ::std::size_t wdt,
															   char_type fillch)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (placement == ::fast_io::manipulators::scalar_placement::internal)
	{
		if constexpr (printable_internal_shift<char_type, value_type>)
		{
			if constexpr (scatter_printable<char_type, value_type>)
			{
				auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
				auto it{copy_scatter(sc, iter)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
			else
			{
				char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
		}
		else
		{
			return print_reserve_define_width_ch_impl<::fast_io::manipulators::scalar_placement::right>(iter, t, wdt,
																										fillch);
		}
	}
	else
	{
		if constexpr (scatter_printable<char_type, value_type>)
		{
			auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
			auto it{copy_scatter(sc, iter)};
			return handle_common_ch<placement>(iter, it, wdt, fillch);
		}
		else
		{
			char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
			return handle_common_ch<placement>(iter, it, wdt, fillch);
		}
	}
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type, typename T>
	requires ::std::is_trivially_copyable_v<T>
inline constexpr char_type *print_reserve_define_width_impl(char_type *iter, T t, ::std::size_t wdt)
{
	return print_reserve_define_width_ch_impl<placement>(iter, t, wdt, char_literal_v<u8' ', char_type>);
}

template <::std::integral char_type>
inline constexpr char_type *handle_common_rt_ch(::fast_io::manipulators::scalar_placement placement, char_type *first,
												char_type *last, ::std::size_t wd, char_type fillch)
{
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	if (wd <= diff)
	{
		return last;
	}
	::std::size_t const to_fill_chs{wd - diff};
	if (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		my_fill_n(last, to_fill_chs, fillch);
		return first + wd;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		::std::size_t one{1};
		::std::size_t const left_indent{static_cast<::std::size_t>(to_fill_chs >> one)};
		::std::size_t const right_indent{to_fill_chs - left_indent};
		my_copy_right_shift(first, last, left_indent);
		my_fill_n(first, left_indent, fillch);
		my_fill_n(first + wd - right_indent, right_indent, fillch);
		return first + wd;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::right)
	{
		my_copy_right_shift(first, last, to_fill_chs);
		my_fill_n(first, to_fill_chs, fillch);
		return first + wd;
	}
	else
	{
		return last;
	}
}

template <::std::integral char_type, typename T>
inline constexpr char_type *print_reserve_define_width_rt_ch_impl(char_type *iter,
																  ::fast_io::manipulators::scalar_placement placement,
																  T t, ::std::size_t wdt, char_type fillch)
{
	using value_type = ::std::remove_cvref_t<T>;
	if (placement == ::fast_io::manipulators::scalar_placement::internal)
	{
		if constexpr (printable_internal_shift<char_type, value_type>)
		{
			if constexpr (scatter_printable<char_type, value_type>)
			{
				auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
				auto it{copy_scatter(sc, iter)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
			else
			{
				char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
		}
		else
		{
			placement = ::fast_io::manipulators::scalar_placement::right;
		}
	}
	if constexpr (scatter_printable<char_type, value_type>)
	{
		auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
		auto it{copy_scatter(sc, iter)};
		return handle_common_rt_ch(placement, iter, it, wdt, fillch);
	}
	else
	{
		char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
		return handle_common_rt_ch(placement, iter, it, wdt, fillch);
	}
}

template <::std::integral char_type, typename T>
	requires ::std::is_trivially_copyable_v<T>
inline constexpr char_type *print_reserve_define_rt_width_impl(char_type *iter,
															   ::fast_io::manipulators::scalar_placement placement, T t,
															   ::std::size_t wdt)
{
	return print_reserve_define_width_rt_ch_impl(iter, placement, t, wdt, char_literal_v<u8' ', char_type>);
}

} // namespace details

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>,
										   ::fast_io::manipulators::width_t<placement, T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T> &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>,
										  char_type *iter, ::fast_io::manipulators::width_t<placement, T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_impl<placement>(iter, parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_impl<placement>(iter, t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T> &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>,
				   ::fast_io::manipulators::width_ch_t<placement, T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>,
					 char_type *iter, ::fast_io::manipulators::width_ch_t<placement, T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_ch_impl<placement>(iter, parameter<T>{t.reference},
																				 t.width, t.ch);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_ch_impl<placement>(iter, t.reference, t.width, t.ch);
	}
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>,
										   ::fast_io::manipulators::width_runtime_t<T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>,
										  char_type *iter, ::fast_io::manipulators::width_runtime_t<T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_rt_width_impl(iter, t.placement, parameter<T>{t.reference},
																	  t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_rt_width_impl(iter, t.placement, t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>,
				   ::fast_io::manipulators::width_runtime_ch_t<T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>,
					 char_type *iter, ::fast_io::manipulators::width_runtime_ch_t<T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_rt_ch_impl(iter, t.placement, parameter<T>{t.reference},
																		 t.width, t.ch);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_rt_ch_impl(iter, t.placement, t.reference, t.width, t.ch);
	}
}

#endif
} // namespace fast_io
