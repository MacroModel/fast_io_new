#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace fast_io::manipulators
{

template <::std::integral char_type, ::std::size_t extent>
struct static_scatter_t;

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
 * Destination-independent recipe for immutable characters produced from type
 * information alone.
 *
 * A parser or semantic adapter may define the recipe, but it never owns the
 * resulting character object. The core node below is the sole owner of that
 * object and consequently keeps materialization, provider lifetime, and output
 * selection below every syntax front end. `emit` must be usable in a constant
 * expression, must write exactly `size` code units, and must return the
 * one-past-end pointer. The core invokes it from an immediate materializer and
 * verifies that returned pointer; declaring the provider operation `constexpr`
 * is the portable C++20 spelling because early frontends cannot pass a local
 * buffer through a dependent consteval call.
 */
template <typename provider_type>
concept static_provider_recipe = requires(
	typename provider_type::char_type *output) {
	typename provider_type::char_type;
	typename ::std::integral_constant<::std::size_t, provider_type::size>;
	{
		provider_type::emit(output)
	} noexcept -> ::std::same_as<typename provider_type::char_type *>;
} && static_argument_character<typename provider_type::char_type>;

/**
 * Maximum structural code-unit count owned by one immutable provider object.
 *
 * The limiting frontend cost follows structural element count more closely
 * than payload bytes. Clang 23 compiled 65,536 `char32_t` code units in 6.51 s
 * / 490 MiB (715,600-byte object), while the 16,384-code-unit / 64-KiB-byte
 * point took 4.00 s / 339 MiB. This remains comparable to the measured narrow
 * 64-KiB tier; the narrow 128-KiB tier was the first deliberate rejection
 * point. Keep one character-domain-independent ceiling in core so raw IO and
 * every syntax front end share the same compile-shape contract.
 */
inline constexpr ::std::size_t static_provider_materialization_code_unit_limit{
	1u << 16u};

/** Materializes one provider recipe in a namespace-scope immediate context. */
template <static_provider_recipe provider_type>
[[nodiscard]] inline consteval auto static_provider_make_storage() noexcept
{
	using char_type = typename provider_type::char_type;
	constexpr ::std::size_t size{provider_type::size};
	// A real one-element object keeps the empty-provider case free of null
	// pointer arithmetic while exposing an empty range to every consumer.
	::fast_io::freestanding::array<
		char_type, size == 0u ? 1u : size> result{};
	auto const end{provider_type::emit(result.data())};
	if (end != result.data() + size)
	{
		::fast_io::fast_terminate();
	}
	return result;
}

/** Core-owned immutable object obtained from one semantic provider recipe. */
template <static_provider_recipe provider_type>
struct static_provider_storage_t
{
	using char_type = typename provider_type::char_type;
	static inline constexpr ::std::size_t size{provider_type::size};
	static_assert(size <= static_provider_materialization_code_unit_limit,
		"fast_io static provider: materialized output exceeds the 65536-code-unit compile-time budget");
	static_assert(size <= static_cast<::std::size_t>(
		::std::numeric_limits<::std::ptrdiff_t>::max()),
		"fast_io static provider: character extent exceeds pointer-difference range");
	static_assert(size <= ::std::numeric_limits<::std::size_t>::max() /
		sizeof(char_type),
		"fast_io static provider: byte extent is not representable");

	// Hidden visibility prevents an ELF interposition/GOT boundary from being
	// introduced for this implementation-owned COMDAT. It does not request
	// inlining and therefore has no effect on the caller's text-size policy. The
	// namespace-scope immediate helper is intentional: early Clang rejects the
	// equivalent static-member call, while GCC 12 rejects a dependent provider
	// call inside the otherwise equivalent immediate lambda. This form keeps
	// both calls in one mandatory constant-evaluation context.
	static inline constexpr auto storage
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
		__attribute__((visibility("hidden")))
#endif
		{::fast_io::manipulators::static_provider_make_storage<provider_type>()};
};

/**
 * Canonical ABI descriptor for one immutable provider subrange.
 *
 * This holder is deliberately restricted to a content-canonical provider,
 * offset, and extent.  It must not be used for a caller-owned literal or any
 * run-time scatter: those values do not have the lifetime or identity needed
 * to share one pointer-bearing object.  Returning this descriptor by reference
 * prevents a large syntax program from materializing the same static-scatter
 * pointer at every lowering boundary while leaving endpoint strategy in core
 * IO and character ownership in `static_provider_storage_t`.
 *
 * A Clang 23 PIE A/B with 96 string-view fields and two distinct literal runs
 * added two eight-byte `.data.rel.ro` objects and two `R_X86_64_RELATIVE`
 * relocations.  In exchange, executable text fell by 2,848 bytes, compile time
 * and peak RSS were unchanged (21.67 s / 994,876 KiB versus 21.95 s /
 * 994,676 KiB), and direct-buffer output improved from 361 ns to 351 ns.
 * Direct `/dev/null` output was neutral (725 ns versus 729 ns).  The bounded
 * relocation cost is therefore paid once per distinct provider subrange, not
 * once per format operation.
 */
template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires(offset <= static_provider_storage_t<provider_type>::size &&
		extent <= static_provider_storage_t<provider_type>::size - offset)
struct static_provider_scatter_descriptor_t
{
	using char_type = typename provider_type::char_type;
	static inline constexpr ::fast_io::manipulators::static_scatter_t<
		char_type, extent>
		value{static_provider_storage_t<provider_type>::storage.data() + offset};
};

/** Returns the single core-owned descriptor for an immutable provider subrange. */
template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires(offset <= static_provider_storage_t<provider_type>::size &&
		extent <= static_provider_storage_t<provider_type>::size - offset)
[[nodiscard]] inline constexpr auto const &
static_provider_scatter_descriptor() noexcept
{
	return static_provider_scatter_descriptor_t<
		provider_type, offset, extent>::value;
}

/**
 * Typed view of a core-owned immutable provider object.
 *
 * `provider_type`, `offset`, and `extent` carry the complete lifetime and
 * bounds proof in the type. The node is deliberately stateless: static
 * fragment and reserve CPOs compute the provider address only after output
 * policy has been selected, avoiding a pointer field, relocation, or
 * value-carrying ABI edge.
 */
template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires(offset <= static_provider_storage_t<provider_type>::size &&
		extent <= static_provider_storage_t<provider_type>::size - offset)
struct static_provider_node
{
	using provider = provider_type;
	using char_type = typename provider_type::char_type;
	static inline constexpr ::std::size_t provider_offset{offset};
	static inline constexpr ::std::size_t size{extent};
};

namespace syntax_transport_details
{

/**
 * Stateless immutable scatter transport for syntax-owned literal content.
 *
 * Unlike `static_provider_node`, this type does not claim language-level
 * static-argument identity and therefore must not make a mixed record enter
 * the eager all-static fragment policy. Core may still retain its provider
 * address for an unbuffered scatter endpoint; buffered and concat outputs use
 * the same exact reserve protocol. The distinction lets syntax frontends pass
 * literal ownership to IO without carrying a pointer-bearing descriptor. It
 * remains in the internal `syntax_transport_details` namespace so the public
 * `mnp` surface continues to expose only language-level manipulators such as
 * `static_arg`.
 */
template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires(offset <= static_provider_storage_t<provider_type>::size &&
		extent <= static_provider_storage_t<provider_type>::size - offset)
struct static_provider_scatter_node
{
	using provider = provider_type;
	using char_type = typename provider_type::char_type;
	static inline constexpr ::std::size_t provider_offset{offset};
	static inline constexpr ::std::size_t size{extent};
};

template <typename T>
struct is_static_provider_scatter_node : ::std::false_type
{};

template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
struct is_static_provider_scatter_node<
	static_provider_scatter_node<provider_type, offset, extent>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_provider_scatter_node_v{
	is_static_provider_scatter_node<::std::remove_cvref_t<T>>::value};

} // namespace syntax_transport_details

template <typename T>
struct is_static_provider_node : ::std::false_type
{};

template <static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
struct is_static_provider_node<
	static_provider_node<provider_type, offset, extent>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_provider_node_v{
	is_static_provider_node<::std::remove_cvref_t<T>>::value};

namespace syntax_transport_details
{

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_scatter_node<provider_type, offset, extent>>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t
print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_scatter_node<provider_type, offset, extent>>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_scatter_node<provider_type, offset, extent>>,
	output_char_type *output,
	static_provider_scatter_node<provider_type, offset, extent>) noexcept
{
	auto const source{
		static_provider_storage_t<provider_type>::storage.data() + offset};
	return ::fast_io::freestanding::non_overlapped_copy_n(
		source, extent, output);
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_scatter_node<provider_type, offset, extent>>,
	static_provider_scatter_node<provider_type, offset, extent>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_scatter_node<provider_type, offset, extent>> tag,
	output_char_type *output, ::std::size_t,
	static_provider_scatter_node<provider_type, offset, extent> value) noexcept
{
	return print_reserve_define(tag, output, value);
}

} // namespace syntax_transport_details

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t
print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>,
	output_char_type *output,
	static_provider_node<provider_type, offset, extent>) noexcept
{
	auto const source{
		static_provider_storage_t<provider_type>::storage.data() + offset};
	// Use the core constexpr copy primitive: constant evaluation keeps its
	// element-wise definition, while run time reaches the existing memcpy/SIMD
	// selection instead of creating a second provider-specific copy loop.
	return ::fast_io::freestanding::non_overlapped_copy_n(
		source, extent, output);
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>,
	static_provider_node<provider_type, offset, extent>) noexcept
{
	return extent;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>> tag,
	output_char_type *output, ::std::size_t,
	static_provider_node<provider_type, offset, extent> value) noexcept
{
	return print_reserve_define(tag, output, value);
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::std::size_t
print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>) noexcept
{
	return 1u;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
inline constexpr ::fast_io::basic_io_scatter_t<output_char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>,
	::fast_io::basic_io_scatter_t<output_char_type> *first,
	static_provider_node<provider_type, offset, extent> const &) noexcept
{
	if constexpr (extent != 0u)
	{
		*first++ = {
			static_provider_storage_t<provider_type>::storage.data() + offset,
			extent};
	}
	return first;
}

template <::std::integral output_char_type,
	static_provider_recipe provider_type, ::std::size_t offset,
	::std::size_t extent>
	requires ::std::same_as<
		output_char_type, typename provider_type::char_type>
[[nodiscard]] inline constexpr ::fast_io::basic_io_scatter_t<output_char_type>
print_compiler_constant_single_static_fragment(
	::fast_io::io_reserve_type_t<
		output_char_type,
		static_provider_node<provider_type, offset, extent>>,
	static_provider_node<provider_type, offset, extent> const &) noexcept
{
	return {
		static_provider_storage_t<provider_type>::storage.data() + offset,
		extent};
}

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

/**
 * Rejects address-bearing values from the type-owned static-argument API.
 *
 * A pointer NTTP identifies an object, function, or class member, not immutable
 * printable content.  Treating it as a static argument would let an address or
 * member locator escape into the core constant-materialization protocol and
 * would make character pointers look like an alternate spelling for
 * `basic_static_string`.  The dedicated structural string wrapper remains the
 * only accepted string-literal route. `nullptr_t` is included because it
 * denotes the same pointer-value domain even though the language trait does not
 * classify it as a pointer type.
 */
template <typename T>
inline constexpr bool is_static_argument_pointer_like_v{
	::std::is_pointer_v<::std::remove_cv_t<T>> ||
	::std::is_member_pointer_v<::std::remove_cv_t<T>> ||
	::std::same_as<::std::remove_cv_t<T>, ::std::nullptr_t>};

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
	 requires (!is_static_argument_pointer_like_v<
		 ::std::remove_cv_t<decltype(value_literal.value)>>)
[[nodiscard]] inline consteval auto make_static_argument() noexcept
{
	return static_arg_t<value_literal>{};
}

template <static_argument_constant name_literal,
	static_argument_constant value_literal>
	 requires (
		 is_static_argument_string_value_v<decltype(name_literal.value)> &&
		 !is_static_argument_pointer_like_v<
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
	if constexpr (::std::same_as<native_type, ::fast_io::io_null_t>)
	{
		// Empty string literals normalize to the core null semantic before any
		// storage policy is selected. Preserve that zero-width result here;
		// constructing a zero-capacity reserve protocol would be ill-formed.
		return 0u;
	}
	else if constexpr (
		::fast_io::precise_reserve_printable<char_type, native_type>)
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

/**
 * Native-MSVC direct floating provider materialization is frontend-unsafe.
 *
 * MSVC 19.44, both 19.50 releases, 19.51, and the current 19.latest backend
 * ICE while forming `make_native_storage` for a direct floating NTTP. Keep
 * the requires-expression uninstantiated on those frontends; the ordinary
 * static-argument alias remains the semantics-preserving run-time fallback.
 */
template <static_argument_constant value_literal>
inline constexpr bool native_static_argument_frontend_safe{
#if defined(_MSC_VER) && !defined(__clang__)
	!::std::floating_point<::std::remove_cv_t<decltype(value_literal.value)>>
#else
	true
#endif
};

/** Substitution-safe proof that native lowering can produce a constant record. */
template <typename char_type, static_argument_constant value_literal>
concept native_materializable = ::std::integral<char_type> &&
	native_static_argument_frontend_safe<value_literal> && requires {
	typename structural_value_token<
		::fast_io::manipulators::static_argument_details::
			make_native_storage<char_type, value_literal>()>;
};

} // namespace static_argument_details

/** Core provider recipe for the native spelling of one NTTP-backed value. */
template <::std::integral output_char_type,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<output_char_type, value_literal>
struct static_argument_native_provider
{
	using char_type = output_char_type;
	static inline constexpr ::std::size_t size{
		::fast_io::manipulators::static_argument_details::native_exact_size<
			output_char_type, value_literal>()};

	[[nodiscard]] inline static constexpr char_type *emit(
		char_type *output) noexcept
	{
		constexpr auto spelling{
			::fast_io::manipulators::static_argument_details::make_native_storage<
				output_char_type, value_literal>()};
		for (::std::size_t index{}; index != size; ++index)
		{
			output[index] = spelling[index];
		}
		return output + size;
	}
};

/** Unified provider-node spelling for one NTTP-backed native value. */
template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
using static_argument_materialized_t = static_provider_node<
	static_argument_native_provider<char_type, value_literal>, 0u,
	static_argument_native_provider<char_type, value_literal>::size>;

/** Core IO falls back through the ordinary native alias CPO of the stored value. */
template <static_argument_constant value_literal>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	static_arg_t<value_literal>) noexcept
{
	using stored_type = ::std::remove_cv_t<
		decltype(static_arg_t<value_literal>::stored_value)>;
	if constexpr (is_static_argument_string_value_v<stored_type>)
	{
		// The ordinary character-array alias intentionally has a small
		// fixed-extent admission ceiling. An explicit static_arg carries a
		// stronger lifetime proof and must not become unprintable at that
		// unrelated threshold, so it shares the provider node used by format
		// syntax. Destination policy still decides retain versus copy.
		using char_type = typename stored_type::value_type;
		using materialized_type =
			static_argument_materialized_t<char_type, value_literal>;
		// A zero-extent provider cannot satisfy the positive-capacity reserve
		// protocol. Lower it at the alias boundary so raw IO, format IO, and
		// both concat families observe the same vacuous semantic component.
		if constexpr (materialized_type::size == 0u)
		{
			return ::fast_io::io_null;
		}
		else
		{
			return materialized_type{};
		}
	}
	else
	{
		return ::fast_io::io_print_alias(static_arg_t<value_literal>::get());
	}
}

/** A compile-time name is format metadata; raw IO prints only its stored value. */
template <static_argument_constant name_literal,
	static_argument_constant value_literal>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	static_named_arg_t<name_literal, value_literal>) noexcept
{
	return print_alias_define(
		::fast_io::io_alias, static_arg_t<value_literal>{});
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


/// @brief Classifies an unnamed static argument as a type-owned, graph-proven constant provider.
/// @details Its value is an NTTP rather than an optimizer-discovered field, so this proof is valid even on native MSVC;
///          the native-materializable constraint still rejects unsupported representations before proxy formation.
template <::std::integral char_type, static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
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

/// @brief Applies the same type-owned graph proof to a named static argument.
template <::std::integral char_type, static_argument_constant name_literal,
	static_argument_constant value_literal>
	requires ::fast_io::manipulators::static_argument_details::
		native_materializable<char_type, value_literal>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<
		char_type, static_named_arg_t<name_literal, value_literal>>) noexcept
{
	return {};
}

} // namespace fast_io::manipulators
