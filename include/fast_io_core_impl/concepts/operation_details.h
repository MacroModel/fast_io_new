#pragma once

namespace fast_io::details
{

template <typename ch_type>
concept character = ::std::integral<ch_type> && (::std::same_as<ch_type, char> || ::std::same_as<ch_type, wchar_t> ||
												 ::std::same_as<ch_type, char16_t> ||
												 ::std::same_as<ch_type, char8_t> || ::std::same_as<ch_type, char32_t>);

template <typename T>
concept c_str_pointer =
	::std::same_as<T, char const *> || ::std::same_as<T, char8_t const *> || ::std::same_as<T, wchar_t const *> ||
	::std::same_as<T, char16_t const *> || ::std::same_as<T, char32_t const *>;

/// @brief Proves the exact pointer category consumed by the non-null-terminated path adapter.
/// @details The encoding converters instantiate their code-unit implementation from `data()`'s declared return type.
///          A reference-to-pointer, pointer-to-pointer, floating element, or volatile element can satisfy a bare
///          expression probe but cannot bind to the converter's `const integral*` parameter. This structural predicate
///          rejects those cases at the public concept boundary. It deliberately accepts every integral code-unit type
///          supported by the converter; encoding validity remains a value-level provider obligation.
template <typename T>
concept os_str_data_pointer = ::std::is_pointer_v<T> &&
							  ::std::integral<::std::remove_cv_t<::std::remove_pointer_t<T>>> &&
							  (!::std::is_volatile_v<::std::remove_pointer_t<T>>);

/// @brief Proves the one-dimensional integral array alternative consumed by the path adapters.
/// @details Consumer templates preserve the array extent and pass its first element to an integral code-unit
///          converter. Requiring that exact shape here prevents a floating or multidimensional array from selecting an
///          adapter whose body is necessarily ill-formed.
template <typename T>
concept os_str_array = ::std::is_array_v<::std::remove_reference_t<T>> &&
					   (::std::rank_v<::std::remove_reference_t<T>> == 1u) &&
					   ::std::integral<::std::remove_cv_t<::std::remove_extent_t<::std::remove_reference_t<T>>>> &&
					   (!::std::is_volatile_v<::std::remove_extent_t<::std::remove_reference_t<T>>>);

/// @brief Proves the view expressions used by the allocating path-adapter branch.
/// @details The consumer receives `T const&`, copies exactly `length()` code units from `data()`, and appends a null
///          terminator. Consequently those two operations are tested on a const lvalue. `substr()` is retained on the
///          historical mutable-lvalue category only as the protocol's explicit view discriminator; consumers do not
///          invoke it. Providers must additionally guarantee that `length()` is nonnegative and representable as
///          `size_t`, and that `data()` denotes at least that many contiguous readable code units for the complete
///          callback invocation; those lifetime and bounds properties cannot be established by a requires-expression.
template <typename T>
concept os_str_view = requires(::std::remove_reference_t<T> const &t, ::std::remove_reference_t<T> &mutable_t) {
	{ t.data() } -> ::fast_io::details::os_str_data_pointer;
	{ t.length() } -> ::std::convertible_to<::std::size_t>;
	mutable_t.substr();
};

} // namespace fast_io::details

namespace fast_io
{

/// @brief Recognizes the null-terminated, const-observable path-source alternative.
/// @details A provider must expose `c_str()` on `T const&` and return exactly one of the supported immutable character
///          pointers. The returned range must be null terminated and remain readable until the synchronous OS adapter
///          callback returns. The concept encodes the expression and return type; termination, bounds, and lifetime are
///          semantic proof obligations of the provider.
template <typename T>
concept type_has_c_str_method = requires(::std::remove_reference_t<T> const &t) {
	{ t.c_str() } -> ::fast_io::details::c_str_pointer;
};

/// @brief Recognizes every source shape from which an OS path adapter can construct a terminated character sequence.
/// @details The three alternatives mirror the consumer's `if constexpr` partition exactly: a const-observable `c_str`
///          source, a one-dimensional integral code-unit array, or a const-observable string view. Array providers must
///          keep their complete extent readable. View providers inherit the bounds and lifetime obligations documented
///          by `details::os_str_view`. A successful concept query proves only safe construction of a temporary native
///          string; it does not prove that the resulting path is valid for any particular operating-system operation.
template <typename T>
concept constructible_to_os_c_str =
	type_has_c_str_method<T> || ::fast_io::details::os_str_array<T> || ::fast_io::details::os_str_view<T>;

/// @brief Extends OS path-source recognition with the nullable-source observation protocol.
/// @details `is_nullptr()` is tested on the same const object category used by path consumers and must return an exact
///          `bool`. When this nullable-only alternative is provided, returning `false` is a semantic promise that the
///          consuming API has another documented way to obtain the path characters; this concept alone cannot derive
///          such a representation. The result must remain stable throughout one synchronous adapter invocation.
template <typename T>
concept constructible_to_os_c_str_or_nullptr = constructible_to_os_c_str<T> ||
											   requires(::std::remove_reference_t<T> const &t) {
												   { t.is_nullptr() } -> ::std::same_as<bool>;
											   };

namespace manipulators
{

template <::std::integral ch_type>
struct basic_os_c_str
{
	using char_type = ch_type;
	char_type const *ptr{};
	inline constexpr char_type const *c_str() const noexcept
	{
		return ptr;
	}
};

template <::std::integral char_type>
inline constexpr basic_os_c_str<char_type> os_c_str(char_type const *cstr) noexcept
{
	return {cstr};
}

inline constexpr void os_c_str(decltype(nullptr)) = delete;

template <::std::integral ch_type>
struct basic_os_c_str_or_nullptr
{
	using char_type = ch_type;
	char_type const *ptr{};
	inline constexpr char_type const *c_str() const noexcept
	{
		return ptr;
	}
	inline constexpr bool is_nullptr() const noexcept
	{
		return ptr == nullptr;
	}
};

template <::std::integral char_type>
inline constexpr basic_os_c_str_or_nullptr<char_type> os_c_str_or_nullptr(char_type const *cstr) noexcept
{
	return {cstr};
}

template <::std::integral ch_type>
struct basic_os_c_str_with_known_size
{
	using char_type = ch_type;
	char_type const *ptr{};
	::std::size_t n{};
	inline constexpr char_type const *c_str() const noexcept
	{
		return ptr;
	}
	inline constexpr ::std::size_t size() const noexcept
	{
		return n;
	}
	inline constexpr char_type const *data() const noexcept
	{
		return ptr;
	}
	inline constexpr char_type const *begin() const noexcept
	{
		return ptr;
	}
	inline constexpr char_type const *end() const noexcept
	{
		return ptr + n;
	}
};

template <::std::integral char_type>
inline constexpr basic_os_c_str_with_known_size<char_type> os_c_str_with_known_size(char_type const *cstr,
																					::std::size_t n) noexcept
{
	return {cstr, n};
}

inline constexpr void os_c_str_with_known_size(decltype(nullptr), ::std::size_t) = delete;

template <::std::integral ch_type>
struct basic_os_str_known_size_without_null_terminated
{
	using char_type = ch_type;
	char_type const *ptr{};
	::std::size_t n{};
	inline constexpr ::std::size_t size() const noexcept
	{
		return n;
	}
	inline constexpr char_type const *data() const noexcept
	{
		return ptr;
	}
	inline constexpr char_type const *begin() const noexcept
	{
		return ptr;
	}
	inline constexpr char_type const *end() const noexcept
	{
		return ptr + n;
	}
};

template <::std::integral char_type>
inline constexpr basic_os_str_known_size_without_null_terminated<char_type>
os_str_known_size_without_null_terminated(char_type const *cstr, ::std::size_t n) noexcept
{
	return {cstr, n};
}

template <::std::integral char_type>
inline constexpr basic_os_str_known_size_without_null_terminated<char_type>
os_str_known_size_without_null_terminated(char_type const *cstr, char_type const *end) noexcept
{
	return {cstr, static_cast<::std::size_t>(end - cstr)};
}

inline constexpr void os_str_known_size_without_null_terminated(decltype(nullptr), ::std::size_t) = delete;

} // namespace manipulators

} // namespace fast_io
