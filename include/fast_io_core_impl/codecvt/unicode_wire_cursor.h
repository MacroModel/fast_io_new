#pragma once

namespace fast_io::details::codecvt
{
/** @brief Addresses complete wire units using byte arithmetic. Individual loads and stores preserve alignment and lifetime. */
template <typename Unit>
struct unicode_wire_cursor
{
	using value_type = ::std::remove_const_t<Unit>;
	using byte_type = ::std::conditional_t<::std::is_const_v<Unit>, ::std::byte const, ::std::byte>;
	byte_type *bytes;

	struct reference
	{
		byte_type *address;
		void operator=(value_type value) const noexcept
		{
			::fast_io::freestanding::my_memcpy(address, __builtin_addressof(value), sizeof(value));
		}
	};

	auto operator*() const noexcept
	{
		if constexpr (::std::is_const_v<Unit>)
		{
			value_type value;
			::fast_io::freestanding::my_memcpy(__builtin_addressof(value), bytes, sizeof(value));
			return value;
		}
		else
		{
			return reference{bytes};
		}
	}
	auto operator[](::std::ptrdiff_t n) const noexcept { return *(*this + n); }
	unicode_wire_cursor &operator+=(::std::ptrdiff_t n) noexcept { bytes += n * static_cast<::std::ptrdiff_t>(sizeof(Unit)); return *this; }
	unicode_wire_cursor operator+(::std::ptrdiff_t n) const noexcept { auto next{*this}; return next += n; }
	unicode_wire_cursor &operator++() noexcept { return *this += 1; }
	unicode_wire_cursor operator++(int) noexcept { auto old{*this}; ++*this; return old; }
	::std::ptrdiff_t operator-(unicode_wire_cursor other) const noexcept { return (bytes - other.bytes) / static_cast<::std::ptrdiff_t>(sizeof(Unit)); }
	bool operator==(unicode_wire_cursor const &) const noexcept = default;
};

template <typename T>
inline constexpr ::std::type_identity<::std::remove_cv_t<T>> unicode_value_type(T *) noexcept { return {}; }
template <typename T>
inline constexpr ::std::type_identity<::std::remove_cv_t<T>> unicode_value_type(unicode_wire_cursor<T>) noexcept { return {}; }
template <typename Cursor>
using unicode_cursor_value_t = typename decltype(unicode_value_type(::std::declval<Cursor>()))::type;

template <typename T>
inline constexpr T *unicode_address(T *p) noexcept { return p; }
template <typename T>
inline auto unicode_address(unicode_wire_cursor<T> p) noexcept { return p.bytes; }

template <typename Unit, typename Byte>
inline auto unicode_byte_cursor(Byte *p) noexcept
{
	using char_type = ::std::conditional_t<::std::is_const_v<Byte>, char const, char>;
	using unit_type = ::std::conditional_t<::std::is_const_v<Byte>, Unit const, Unit>;
	if constexpr (sizeof(Unit) == 1) { return reinterpret_cast<char_type *>(p); }
	else { return unicode_wire_cursor<unit_type>{p}; }
}
} // namespace fast_io::details::codecvt
