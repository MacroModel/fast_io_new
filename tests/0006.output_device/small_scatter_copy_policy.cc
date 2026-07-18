#include <array>
#include <cassert>
#include <cstddef>

#include <fast_io_core.h>

namespace
{

template <typename char_type, ::std::size_t capacity>
struct line_capture_sink
{
	using output_char_type = char_type;
	::std::array<char_type, capacity> *storage{};
	::std::size_t *size{};
	::std::size_t *calls{};
};

template <typename char_type, ::std::size_t capacity>
inline constexpr line_capture_sink<char_type, capacity> &output_stream_ref_define(
	line_capture_sink<char_type, capacity> &sink) noexcept
{
	return sink;
}

template <typename char_type, ::std::size_t capacity>
inline void write_all_overflow_define(
	line_capture_sink<char_type, capacity> &sink,
	char_type const *first, char_type const *last) noexcept
{
	auto const count{static_cast<::std::size_t>(last - first)};
	assert(count <= capacity - *sink.size);
	for (; first != last; ++first)
	{
		(*sink.storage)[(*sink.size)++] = *first;
	}
	++*sink.calls;
}

template <typename char_type>
consteval bool constant_evaluation_copy_matches()
{
	::std::array<char_type, 40u> source{};
	for (::std::size_t i{}; i != source.size(); ++i)
	{
		source[i] = static_cast<char_type>(i + 1u);
	}
	for (::std::size_t count{}; count != 34u; ++count)
	{
		::std::array<char_type, 42u> destination{};
		destination.front() = static_cast<char_type>(0x55u);
		destination.back() = static_cast<char_type>(0x66u);
		auto *const first{destination.data() + 1u};
		auto *const end{::fast_io::details::decay::small_scatter_copy_n(
			source.data(), count, first)};
		if (end != first + count || destination.front() != static_cast<char_type>(0x55u) ||
			destination.back() != static_cast<char_type>(0x66u))
		{
			return false;
		}
		for (::std::size_t i{}; i != count; ++i)
		{
			if (destination[i + 1u] != source[i])
			{
				return false;
			}
		}
	}
	return true;
}

template <typename char_type>
consteval bool zero_static_copy_preserves_null_representation()
{
	return ::fast_io::details::decay::static_scatter_copy_n<0u>(
		static_cast<char_type const *>(nullptr), static_cast<char_type *>(nullptr)) == nullptr;
}

template <typename char_type, ::std::size_t count>
consteval bool constant_static_scatter_cpos_match()
{
	::std::array<char_type, count> source{};
	for (::std::size_t i{}; i != count; ++i)
	{
		source[i] = static_cast<char_type>(i + 1u);
	}
	::fast_io::manipulators::static_scatter_t<char_type, count> scatter{source.data()};

	::std::array<char_type, count + 2u> ordinary{};
	ordinary.front() = static_cast<char_type>(0x55u);
	ordinary.back() = static_cast<char_type>(0x66u);
	auto *const ordinary_first{ordinary.data() + 1u};
	auto *const ordinary_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, decltype(scatter)>, ordinary_first, scatter)};

	::std::array<char_type, count + 2u> precise{};
	precise.front() = static_cast<char_type>(0x55u);
	precise.back() = static_cast<char_type>(0x66u);
	auto *const precise_first{precise.data() + 1u};
	auto *const precise_end{::fast_io::print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, decltype(scatter)>, precise_first, count, scatter)};

	if (ordinary_end != ordinary_first + count || precise_end != precise_first + count ||
		ordinary.front() != static_cast<char_type>(0x55u) ||
		ordinary.back() != static_cast<char_type>(0x66u) ||
		precise.front() != static_cast<char_type>(0x55u) ||
		precise.back() != static_cast<char_type>(0x66u))
	{
		return false;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		if (ordinary[i + 1u] != source[i] || precise[i + 1u] != source[i])
		{
			return false;
		}
	}
	return true;
}

template <typename char_type>
void test_runtime_copy_and_guards()
{
	::std::array<char_type, 40u> source{};
	for (::std::size_t i{}; i != source.size(); ++i)
	{
		source[i] = static_cast<char_type>(i + 1u);
	}
	for (::std::size_t count{}; count != 34u; ++count)
	{
		::std::array<char_type, 42u> destination{};
		destination.front() = static_cast<char_type>(0x55u);
		destination[count + 1u] = static_cast<char_type>(0x66u);
		auto *const first{destination.data() + 1u};
		auto *const end{::fast_io::details::decay::small_scatter_copy_n(
			source.data(), count, first)};
		assert(end == first + count);
		assert(destination.front() == static_cast<char_type>(0x55u));
		assert(destination[count + 1u] == static_cast<char_type>(0x66u));
		for (::std::size_t i{}; i != count; ++i)
		{
			assert(destination[i + 1u] == source[i]);
		}
	}
}

template <typename char_type, ::std::size_t count>
void test_runtime_static_scatter_cpos_and_guards()
{
	// The source has exactly `count` live elements and deliberately provides no terminator or readable padding. ASan
	// therefore catches any future attempt to turn the small-copy policy into a widened over-read.
	::std::array<char_type, count> source{};
	for (::std::size_t i{}; i != count; ++i)
	{
		source[i] = static_cast<char_type>(i + 1u);
	}
	::fast_io::manipulators::static_scatter_t<char_type, count> scatter{source.data()};
	::std::array<char_type, count + 2u> destination{};
	destination.front() = static_cast<char_type>(0x55u);
	destination.back() = static_cast<char_type>(0x66u);
	auto *const first{destination.data() + 1u};
	auto *const end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, decltype(scatter)>, first, scatter)};
	assert(end == first + count);
	assert(destination.front() == static_cast<char_type>(0x55u));
	assert(destination.back() == static_cast<char_type>(0x66u));
	for (::std::size_t i{}; i != count; ++i)
	{
		assert(destination[i + 1u] == source[i]);
	}
}

template <typename char_type>
void test_runtime_static_scatter_boundaries()
{
	test_runtime_static_scatter_cpos_and_guards<char_type, 0u>();
	test_runtime_static_scatter_cpos_and_guards<char_type, 1u>();
	test_runtime_static_scatter_cpos_and_guards<char_type, 3u>();
	test_runtime_static_scatter_cpos_and_guards<char_type, 16u>();
	test_runtime_static_scatter_cpos_and_guards<char_type, 17u>();
}

template <typename char_type, ::std::size_t count>
void test_static_scatter_line_ownership()
{
	static_assert(count != 0u);
	::std::array<char_type, count> payload{};
	::std::array<char_type, count + 1u> embedded_payload{};
	for (::std::size_t i{}; i != count; ++i)
	{
		payload[i] = static_cast<char_type>(u8'a' + i % 23u);
		embedded_payload[i] = payload[i];
	}
	embedded_payload[count] = ::fast_io::char_literal_v<u8'\n', char_type>;

	::std::array<char_type, count + 2u> line_storage{};
	::std::array<char_type, count + 2u> embedded_storage{};
	::std::size_t line_size{};
	::std::size_t embedded_size{};
	::std::size_t line_calls{};
	::std::size_t embedded_calls{};
	line_capture_sink<char_type, count + 2u> line_sink{
		__builtin_addressof(line_storage), __builtin_addressof(line_size),
		__builtin_addressof(line_calls)};
	line_capture_sink<char_type, count + 2u> embedded_sink{
		__builtin_addressof(embedded_storage), __builtin_addressof(embedded_size),
		__builtin_addressof(embedded_calls)};

	::fast_io::manipulators::static_scatter_t<char_type, count> line_source{
		payload.data()};
	::fast_io::manipulators::static_scatter_t<char_type, count + 1u>
		embedded_source{embedded_payload.data()};
	::fast_io::operations::print_freestanding<true>(line_sink, line_source);
	::fast_io::operations::print_freestanding<false>(embedded_sink, embedded_source);

	/*
	The line bit owns one adjacent character in the same fixed-capacity reserve
	materialization.  It must therefore reach a scalar-only destination in one
	completed range, exactly like a source whose static extent already includes
	the newline.  Besides preserving call boundaries, this is the semantic proof
	used by the assembly probe: for short literals the back end may combine the
	type-level copy and final line store into one machine-width store.
	*/
	assert(line_calls == 1u);
	assert(embedded_calls == 1u);
	assert(line_size == count + 1u);
	assert(embedded_size == count + 1u);
	for (::std::size_t i{}; i != count + 1u; ++i)
	{
		assert(line_storage[i] == embedded_storage[i]);
	}
}

template <typename char_type>
void test_static_scatter_line_boundaries()
{
	test_static_scatter_line_ownership<char_type, 1u>();
	test_static_scatter_line_ownership<char_type, 3u>();
	test_static_scatter_line_ownership<char_type, 15u>();
	test_static_scatter_line_ownership<char_type, 16u>();
	test_static_scatter_line_ownership<char_type, 17u>();
}

static_assert(constant_evaluation_copy_matches<char>());
static_assert(constant_evaluation_copy_matches<char8_t>());
static_assert(constant_evaluation_copy_matches<wchar_t>());
static_assert(constant_evaluation_copy_matches<char16_t>());
static_assert(constant_evaluation_copy_matches<char32_t>());

static_assert(zero_static_copy_preserves_null_representation<char>());
static_assert(zero_static_copy_preserves_null_representation<char16_t>());

static_assert(constant_static_scatter_cpos_match<char, 0u>());
static_assert(constant_static_scatter_cpos_match<char, 1u>());
static_assert(constant_static_scatter_cpos_match<char, 3u>());
static_assert(constant_static_scatter_cpos_match<char, 16u>());
static_assert(constant_static_scatter_cpos_match<char, 17u>());
static_assert(constant_static_scatter_cpos_match<char8_t, 3u>());
static_assert(constant_static_scatter_cpos_match<char16_t, 3u>());
static_assert(constant_static_scatter_cpos_match<char32_t, 3u>());

} // namespace

int main()
{
	test_runtime_copy_and_guards<char>();
	test_runtime_copy_and_guards<char8_t>();
	test_runtime_copy_and_guards<wchar_t>();
	test_runtime_copy_and_guards<char16_t>();
	test_runtime_copy_and_guards<char32_t>();
	test_runtime_static_scatter_boundaries<char>();
	test_runtime_static_scatter_boundaries<char8_t>();
	test_runtime_static_scatter_boundaries<char16_t>();
	test_runtime_static_scatter_boundaries<char32_t>();
	test_static_scatter_line_boundaries<char>();
	test_static_scatter_line_boundaries<char8_t>();
	test_static_scatter_line_boundaries<wchar_t>();
	test_static_scatter_line_boundaries<char16_t>();
	test_static_scatter_line_boundaries<char32_t>();
}
