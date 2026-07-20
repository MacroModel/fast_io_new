#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::manipulators
{

template <typename char_type>
inline constexpr bool is_static_argument_character_v{
	::std::same_as<::std::remove_cv_t<char_type>, char> ||
	::std::same_as<::std::remove_cv_t<char_type>, wchar_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char8_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char16_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char32_t>};

template <typename char_type>
concept static_argument_character =
	is_static_argument_character_v<char_type>;

/** A structural, null-terminated character value owned by the core IO layer. */
template <static_argument_character char_type, ::std::size_t extent_value>
struct basic_static_string
{
	static_assert(extent_value != 0u);

	using value_type = char_type;
	static inline constexpr ::std::size_t extent{extent_value};
	char_type elements[extent_value]{};

	consteval basic_static_string(
		char_type const (&source)[extent_value]) noexcept
	{
		for (::std::size_t index{}; index != extent_value; ++index)
		{
			elements[index] = source[index];
		}
	}

	[[nodiscard]] inline constexpr char_type const *data() const noexcept
	{
		return elements;
	}

	[[nodiscard]] inline static constexpr ::std::size_t size() noexcept
	{
		return extent_value - 1u;
	}

	[[nodiscard]] inline constexpr char_type const *begin() const noexcept
	{
		return elements;
	}

	[[nodiscard]] inline constexpr char_type const *end() const noexcept
	{
		return elements + size();
	}

	[[nodiscard]] inline constexpr char_type const &operator[](
		::std::size_t index) const noexcept
	{
		return elements[index];
	}

	[[nodiscard]] inline constexpr bool operator==(
		basic_static_string const &) const noexcept = default;
};

template <static_argument_character char_type, ::std::size_t extent>
basic_static_string(char_type const (&)[extent])
	-> basic_static_string<char_type, extent>;

template <typename T>
struct is_basic_static_string : ::std::false_type
{};

template <static_argument_character char_type, ::std::size_t extent>
struct is_basic_static_string<basic_static_string<char_type, extent>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_basic_static_string_v{
	is_basic_static_string<::std::remove_cv_t<T>>::value};

/** Recognizes compatible structural fixed-string values without a format-layer dependency. */
template <typename T>
inline constexpr bool is_static_argument_string_value_v{[]() consteval {
	using value_type = ::std::remove_cv_t<T>;
	if constexpr (is_basic_static_string_v<value_type>)
	{
		return true;
	}
	else if constexpr (requires(value_type const &value) {
		typename value_type::value_type;
		value_type::extent;
		value.elements;
		value.size();
	})
	{
		using char_type = typename value_type::value_type;
		using elements_type = decltype(::std::declval<value_type const &>().elements);
		return static_argument_character<char_type> &&
			::std::is_bounded_array_v<elements_type> &&
			::std::same_as<
				::std::remove_cv_t<::std::remove_extent_t<elements_type>>,
				::std::remove_cv_t<char_type>> &&
			value_type::extent == ::std::extent_v<elements_type>;
	}
	else
	{
		return false;
	}
}()};

template <typename value_type>
inline constexpr void copy_static_c_array_element(
	value_type &destination, value_type const &source) noexcept
{
	if constexpr (::std::is_array_v<value_type>)
	{
		for (::std::size_t index{};
			 index != ::std::extent_v<value_type>; ++index)
		{
			copy_static_c_array_element(destination[index], source[index]);
		}
	}
	else
	{
		destination = source;
	}
}

/** Structural by-value copy used to prove that a C array is constant-readable. */
template <typename element_type, ::std::size_t extent>
struct basic_static_c_array_value
{
	element_type elements[extent]{};

	consteval basic_static_c_array_value(
		element_type const (&source)[extent]) noexcept
	{
		for (::std::size_t index{}; index != extent; ++index)
		{
			copy_static_c_array_element(elements[index], source[index]);
		}
	}
};

template <typename element_type, ::std::size_t extent>
basic_static_c_array_value(element_type const (&)[extent])
	-> basic_static_c_array_value<element_type, extent>;

template <typename T>
struct is_basic_static_c_array_value : ::std::false_type
{};

template <typename element_type, ::std::size_t extent>
struct is_basic_static_c_array_value<
	basic_static_c_array_value<element_type, extent>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_basic_static_c_array_value_v{
	is_basic_static_c_array_value<::std::remove_cv_t<T>>::value};

/**
 * Structural deduction wrapper used by the `static_arg<...>` variable template.
 *
 * The public member also preserves the Clang 17 class-NTTP route for floating
 * values on targets where spelling that value again as a direct `auto` NTTP is
 * rejected.
 */
template <typename value_type>
struct static_argument_constant
{
	value_type value;

	consteval static_argument_constant(value_type source) noexcept
		: value(source)
	{}

	template <static_argument_character char_type, ::std::size_t extent>
		requires ::std::same_as<
			value_type, basic_static_string<char_type, extent>>
	consteval static_argument_constant(
		char_type const (&source)[extent]) noexcept
		: value(source)
	{}
};

template <typename value_type>
static_argument_constant(value_type const &)
	-> static_argument_constant<value_type>;

template <static_argument_character char_type, ::std::size_t extent>
static_argument_constant(char_type const (&)[extent])
	-> static_argument_constant<basic_static_string<char_type, extent>>;

template <typename T>
inline constexpr bool is_static_argument_character_pointer_v{
	::std::is_pointer_v<T> &&
	static_argument_character<
		::std::remove_cv_t<::std::remove_pointer_t<T>>>};

/** Stateless core node whose printable value is carried entirely by its type. */
template <static_argument_constant value_literal>
struct static_arg_t
{
	static inline constexpr auto stored_value{value_literal.value};

	[[nodiscard]] inline static constexpr decltype(auto) get() noexcept
	{
		using stored_type = ::std::remove_cv_t<decltype(stored_value)>;
		if constexpr (is_static_argument_string_value_v<stored_type> ||
				  is_basic_static_c_array_value_v<stored_type>)
		{
			return (stored_value.elements);
		}
		else
		{
			return (stored_value);
		}
	}
};

/** Named static value; core IO ignores the tag while format lowering may use it. */
template <static_argument_constant name_literal,
	static_argument_constant value_literal>
struct static_named_arg_t
{
	static_assert(is_static_argument_string_value_v<decltype(name_literal.value)>);
	static inline constexpr auto name{name_literal.value};
	// Keep the named node as a pure type token.  Format lowering may continue to
	// spell `argument.value`, but that expression names this provider object and
	// transports no state through a public print/concat call.
	static inline constexpr static_arg_t<value_literal> value{};

	[[nodiscard]] inline static constexpr decltype(auto) get() noexcept
	{
		return static_arg_t<value_literal>::get();
	}
};

template <typename T>
struct is_static_arg : ::std::false_type
{};

template <static_argument_constant value_literal>
struct is_static_arg<static_arg_t<value_literal>> : ::std::true_type
{};

template <static_argument_constant name_literal,
	static_argument_constant value_literal>
struct is_static_arg<static_named_arg_t<name_literal, value_literal>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_arg_v{
	is_static_arg<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_static_named_arg : ::std::false_type
{};

template <static_argument_constant name_literal,
	static_argument_constant value_literal>
struct is_static_named_arg<
	static_named_arg_t<name_literal, value_literal>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_named_arg_v{
	is_static_named_arg<::std::remove_cvref_t<T>>::value};

namespace static_argument_details
{

template <static_argument_constant value_literal>
	 requires (!is_static_argument_character_pointer_v<
		 ::std::remove_cv_t<decltype(value_literal.value)>>)
[[nodiscard]] inline consteval auto make_static_argument() noexcept
{
	return static_arg_t<value_literal>{};
}

template <static_argument_constant name_literal,
	static_argument_constant value_literal>
	 requires (
		 is_static_argument_string_value_v<decltype(name_literal.value)> &&
		 !is_static_argument_character_pointer_v<
			 ::std::remove_cv_t<decltype(value_literal.value)>>)
[[nodiscard]] inline consteval auto make_static_argument() noexcept
{
	return static_named_arg_t<name_literal, value_literal>{};
}

} // namespace static_argument_details

/**
 * NTTP-backed IO argument with no run-time value member.
 *
 * This is deliberately a variable template.  `static_arg<42>()` and calls with
 * run-time arguments therefore remain ill-formed instead of resembling a
 * factory function.
 */
template <static_argument_constant... value_literals>
	 requires requires {
		 ::fast_io::manipulators::static_argument_details::
			 make_static_argument<value_literals...>();
	 }
inline constexpr auto static_arg{
	::fast_io::manipulators::static_argument_details::
		make_static_argument<value_literals...>()};

namespace static_argument_details
{

/** The exact core-normalized leaf selected for a type-owned static value. */
template <::std::integral char_type, static_argument_constant value_literal>
[[nodiscard]] inline consteval auto make_native_value()
{
	return ::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(static_arg_t<value_literal>::get()));
}

template <::std::integral char_type, static_argument_constant value_literal>
using native_value_t = ::std::remove_cvref_t<decltype(
	::fast_io::manipulators::static_argument_details::
		make_native_value<char_type, value_literal>())>;

template <::std::integral char_type, static_argument_constant value_literal>
inline constexpr auto native_value{
	::fast_io::manipulators::static_argument_details::
		make_native_value<char_type, value_literal>()};

/** Computes the exact native spelling without invoking any format-layer renderer. */
template <::std::integral char_type, static_argument_constant value_literal>
[[nodiscard]] inline consteval ::std::size_t native_exact_size()
{
	using native_type =
		::fast_io::manipulators::static_argument_details::native_value_t<
			char_type, value_literal>;
	constexpr auto value{
		::fast_io::manipulators::static_argument_details::native_value<
			char_type, value_literal>};
	if constexpr (::fast_io::precise_reserve_printable<char_type, native_type>)
	{
		return print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, native_type>, value);
	}
	else if constexpr (::fast_io::reserve_printable<char_type, native_type>)
	{
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, native_type>)};
		::fast_io::freestanding::array<
			char_type, capacity == 0u ? 1u : capacity> buffer{};
		auto const end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, native_type>,
			buffer.data(), value)};
		return static_cast<::std::size_t>(end - buffer.data());
	}
	else if constexpr (
		::fast_io::dynamic_reserve_printable<char_type, native_type>)
	{
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, native_type>, value)};
		::fast_io::freestanding::array<
			char_type, capacity == 0u ? 1u : capacity> buffer{};
		auto const end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, native_type>,
			buffer.data(), value)};
		return static_cast<::std::size_t>(end - buffer.data());
	}
	else if constexpr (::fast_io::scatter_printable_for<char_type, native_type>)
	{
		return print_scatter_define(
			::fast_io::io_reserve_type<char_type, native_type>, value).len;
	}
	else
	{
		static_assert(sizeof(native_type) == 0u,
			"fast_io: mnp::static_arg has no native contiguous core print protocol");
		return 0u;
	}
}

/** Builds the provider-owned DSAL record selected by the native core leaf. */
template <::std::integral char_type, static_argument_constant value_literal>
[[nodiscard]] inline consteval auto make_native_storage()
{
	using native_type =
		::fast_io::manipulators::static_argument_details::native_value_t<
			char_type, value_literal>;
	constexpr auto value{
		::fast_io::manipulators::static_argument_details::native_value<
			char_type, value_literal>};
	constexpr ::std::size_t size{
		::fast_io::manipulators::static_argument_details::native_exact_size<
			char_type, value_literal>()};
	::fast_io::freestanding::array<char_type, size == 0u ? 1u : size> result{};
	if constexpr (size != 0u)
	{
		if constexpr (::fast_io::precise_reserve_printable<char_type, native_type>)
		{
			using result_type = decltype(print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, native_type>,
				result.data(), size, value));
			if constexpr (::std::same_as<result_type, char_type *>)
			{
				auto const end{print_reserve_precise_define(
					::fast_io::io_reserve_type<char_type, native_type>,
					result.data(), size, value)};
				if (end != result.data() + size)
				{
					::fast_io::fast_terminate();
				}
			}
			else
			{
				print_reserve_precise_define(
					::fast_io::io_reserve_type<char_type, native_type>,
					result.data(), size, value);
			}
		}
		else if constexpr (::fast_io::reserve_printable<char_type, native_type> ||
			::fast_io::dynamic_reserve_printable<char_type, native_type>)
		{
			auto const end{print_reserve_define(
				::fast_io::io_reserve_type<char_type, native_type>,
				result.data(), value)};
			if (end != result.data() + size)
			{
				::fast_io::fast_terminate();
			}
		}
		else
		{
			auto const scatter{print_scatter_define(
				::fast_io::io_reserve_type<char_type, native_type>, value)};
			if (scatter.len != size)
			{
				::fast_io::fast_terminate();
			}
			for (::std::size_t index{}; index != size; ++index)
			{
				result[index] = scatter.base[index];
			}
		}
	}
	return result;
}

template <auto>
struct structural_value_token
{};

/** Substitution-safe proof that native lowering can produce a constant record. */
template <typename char_type, static_argument_constant value_literal>
concept native_materializable = ::std::integral<char_type> && requires {
	typename structural_value_token<
		::fast_io::manipulators::static_argument_details::
			make_native_storage<char_type, value_literal>()>;
};

} // namespace static_argument_details

/** Stateless materialized proxy whose complete spelling is one static record. */
template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
struct static_argument_materialized_t
{
	static inline constexpr ::std::size_t size{
		::fast_io::manipulators::static_argument_details::native_exact_size<
			char_type, value_literal>()};
	static inline constexpr auto storage{
		::fast_io::manipulators::static_argument_details::make_native_storage<
			char_type, value_literal>()};
};

/** Core IO falls back through the ordinary native alias CPO of the stored value. */
template <static_argument_constant value_literal>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	static_arg_t<value_literal>) noexcept
{
	return ::fast_io::io_print_alias(static_arg_t<value_literal>::get());
}

/** A compile-time name is format metadata; raw IO prints only its stored value. */
template <static_argument_constant name_literal,
	static_argument_constant value_literal>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	static_named_arg_t<name_literal, value_literal>) noexcept
{
	return ::fast_io::io_print_alias(
		static_named_arg_t<name_literal, value_literal>::get());
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, static_arg_t<value_literal>>,
	static_arg_t<value_literal> const &) noexcept
{
	return true;
}

template <::std::integral char_type, static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<
		char_type, static_named_arg_t<name_literal, value_literal>>,
	static_named_arg_t<name_literal, value_literal> const &) noexcept
{
	return true;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, static_arg_t<value_literal>>,
	static_arg_t<value_literal> const &) noexcept
{
	return static_argument_materialized_t<char_type, value_literal>{};
}

template <::std::integral char_type, static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr auto print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<
		char_type, static_named_arg_t<name_literal, value_literal>>,
	static_named_arg_t<name_literal, value_literal> const &) noexcept
{
	return static_argument_materialized_t<char_type, value_literal>{};
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type, static_arg_t<value_literal>>) noexcept
{
	return {};
}

template <::std::integral char_type, static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<
		char_type, static_named_arg_t<name_literal, value_literal>>) noexcept
{
	return {};
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type, static_arg_t<value_literal>>) noexcept
{
	return {};
}

template <::std::integral char_type, static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<
		char_type, static_named_arg_t<name_literal, value_literal>>) noexcept
{
	return {};
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>) noexcept
{
	return static_argument_materialized_t<char_type, value_literal>::size;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>,
	char_type *iter,
	static_argument_materialized_t<char_type, value_literal>) noexcept
{
	using proxy_type = static_argument_materialized_t<char_type, value_literal>;
	for (::std::size_t index{}; index != proxy_type::size; ++index)
	{
		*iter++ = proxy_type::storage[index];
	}
	return iter;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>,
	static_argument_materialized_t<char_type, value_literal>) noexcept
{
	return static_argument_materialized_t<char_type, value_literal>::size;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
inline constexpr char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>> tag,
	char_type *iter, ::std::size_t,
	static_argument_materialized_t<char_type, value_literal> value) noexcept
{
	return print_reserve_define(tag, iter, value);
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
inline constexpr ::std::size_t
print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>) noexcept
{
	return 1u;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
inline constexpr ::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	static_argument_materialized_t<char_type, value_literal> const &) noexcept
{
	using proxy_type = static_argument_materialized_t<char_type, value_literal>;
	if constexpr (proxy_type::size != 0u)
	{
		*first++ = {proxy_type::storage.data(), proxy_type::size};
	}
	return first;
}

template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<char_type>
print_compiler_constant_single_static_fragment(
	::fast_io::io_reserve_type_t<
		char_type, static_argument_materialized_t<char_type, value_literal>>,
	static_argument_materialized_t<char_type, value_literal> const &) noexcept
{
	using proxy_type = static_argument_materialized_t<char_type, value_literal>;
	return {proxy_type::storage.data(), proxy_type::size};
}

} // namespace fast_io::manipulators
