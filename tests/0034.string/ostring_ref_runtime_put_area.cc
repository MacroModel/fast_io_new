#include <fast_io.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>

namespace
{

struct custom_char_traits : ::std::char_traits<char>
{};

template <typename element_type>
struct custom_allocator
{
	using value_type = element_type;

	/// @brief Constructs a stateless allocator for the requested character type.
	constexpr custom_allocator() noexcept = default;

	/// @brief Converts another stateless allocator specialization without changing allocation state.
	template <typename other_type>
	constexpr custom_allocator(custom_allocator<other_type> const &) noexcept
	{}

	/// @brief Allocates the requested number of elements through the standard allocator backend.
	[[nodiscard]] value_type *allocate(::std::size_t count)
	{
		return ::std::allocator<value_type>{}.allocate(count);
	}

	/// @brief Releases a range previously obtained from the standard allocator backend.
	void deallocate(value_type *pointer, ::std::size_t count) noexcept
	{
		::std::allocator<value_type>{}.deallocate(pointer, count);
	}

	template <typename other_type>
	struct rebind
	{
		using other = custom_allocator<other_type>;
	};
};

/// @brief Declares all stateless test allocator specializations interchangeable.
template <typename left, typename right>
inline constexpr bool operator==(custom_allocator<left>, custom_allocator<right>) noexcept
{
	return true;
}

/// @brief Confirms that no pair of stateless test allocator specializations compares unequal.
template <typename left, typename right>
inline constexpr bool operator!=(custom_allocator<left>, custom_allocator<right>) noexcept
{
	return false;
}

/// @brief Verifies that custom traits or allocators retain the portable append protocol.
template <typename string_type>
void verify_nonstandard_specialization_uses_portable_protocol()
{
	static_assert(::fast_io::auxiliary_strlike<char, string_type>);
	static_assert(!::fast_io::runtime_buffer_strlike<char, string_type>);

	string_type destination(5u, 'p');
	::fast_io::basic_ostring_ref_std<
		char, typename string_type::traits_type,
		typename string_type::allocator_type>
		output{__builtin_addressof(destination)};
	::fast_io::io::print(output, "xyz");
	assert(destination.size() == 8u);
	for (::std::size_t index{}; index != 5u; ++index)
	{
		assert(destination[index] == 'p');
	}
	assert(destination[5u] == 'x');
	assert(destination[6u] == 'y');
	assert(destination[7u] == 'z');
}

} // namespace

/// @brief Verifies spare-capacity output, growth, and terminator publication for one standard character domain.
template <typename char_type>
void verify_standard_string_output()
{
	using string_type = ::std::basic_string<char_type>;
	static_assert(::fast_io::auxiliary_strlike<char_type, string_type>);
	static_assert(::fast_io::runtime_buffer_strlike<char_type, string_type> ==
				  ::fast_io::details::string_hack::standard_string_runtime_put_area_available);

	string_type destination(7u, static_cast<char_type>('p'));
	destination.reserve(257u);
	char_type *const original_data{destination.data()};
	string_type middle(113u, static_cast<char_type>('m'));
	::fast_io::basic_ostring_ref_std<char_type> output{__builtin_addressof(destination)};
	::fast_io::io::print(output, middle);

	assert(destination.data() == original_data);
	assert(destination.size() == 120u);
	for (::std::size_t index{}; index != 7u; ++index)
	{
		assert(destination[index] == static_cast<char_type>('p'));
	}
	for (::std::size_t index{7u}; index != destination.size(); ++index)
	{
		assert(destination[index] == static_cast<char_type>('m'));
	}
	assert(destination.data()[destination.size()] == char_type{});

	// Cross the advertised capacity in one operation. Both the implementation put-area path and the portable append
	// fallback must publish the buffered prefix before growth and restore the terminator at the final logical end.
	string_type large(destination.capacity() + 37u, static_cast<char_type>('z'));
	::fast_io::io::print(output, large);
	assert(destination.size() == 120u + large.size());
	for (::std::size_t index{120u}; index != destination.size(); ++index)
	{
		assert(destination[index] == static_cast<char_type>('z'));
	}
	assert(destination.data()[destination.size()] == char_type{});
}

/// @brief Runs the standard and nonstandard string adapter matrix.
int main()
{
	verify_standard_string_output<char>();
	verify_standard_string_output<wchar_t>();
	verify_standard_string_output<char8_t>();
	verify_standard_string_output<char16_t>();
	verify_standard_string_output<char32_t>();

	// Private-layout access is intentionally limited to the exact standard specialization. A user traits or allocator
	// type may change representation, annotation, and ADL semantics, so both cases must retain ordinary append growth.
	verify_nonstandard_specialization_uses_portable_protocol<
		::std::basic_string<char, custom_char_traits>>();
	verify_nonstandard_specialization_uses_portable_protocol<
		::std::basic_string<char, ::std::char_traits<char>, custom_allocator<char>>>();
}
