#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

template <typename output>
inline constexpr bool writable_output_v =
	::fast_io::operations::decay::defines::writable<output>;

static_assert(!writable_output_v<::fast_io::basic_obuffer_view_ref<char const>>);
static_assert(!writable_output_v<::fast_io::basic_obuffer_view_ref<char volatile>>);
static_assert(!writable_output_v<::fast_io::basic_omemory_map_ref<char const>>);
static_assert(!writable_output_v<::fast_io::basic_omemory_map_ref<char volatile>>);

consteval bool fixed_output_completion_consteval_contract()
{
	// Empty fixed-capacity outputs use typed null sentinels. A zero-length completion must not form pointer arithmetic
	// or dereference the owner merely to establish that there is no work.
	{
		::fast_io::basic_obuffer_view<char> empty{};
		::fast_io::write_all_overflow_define(
			::fast_io::basic_obuffer_view_ref<char>{__builtin_addressof(empty)},
			static_cast<char const *>(nullptr), static_cast<char const *>(nullptr));
	}
	{
		::fast_io::basic_omemory_map<char> empty{};
		::fast_io::write_all_overflow_define(
			::fast_io::basic_omemory_map_ref<char>{__builtin_addressof(empty)},
			static_cast<char const *>(nullptr), static_cast<char const *>(nullptr));
	}

	char const source[]{'x', 'y', 'z'};
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::write_all_overflow_define(
			::fast_io::basic_obuffer_view_ref<char>{__builtin_addressof(output)},
			source, source + 3u);
		if (output.curr_ptr != output.end_ptr || storage != ::std::array<char, 3u>{'x', 'y', 'z'})
		{
			return false;
		}
	}
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_omemory_map<char> output{};
		output.begin_ptr = output.curr_ptr = storage.data();
		output.end_ptr = storage.data() + storage.size();
		::fast_io::write_all_overflow_define(
			::fast_io::basic_omemory_map_ref<char>{__builtin_addressof(output)},
			source, source + 3u);
		if (output.curr_ptr != output.end_ptr || storage != ::std::array<char, 3u>{'x', 'y', 'z'})
		{
			return false;
		}
	}
	return true;
}

static_assert(fixed_output_completion_consteval_contract());

struct static_pair
{
	char first;
	char second;
};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, static_pair>) noexcept
{
	return 2u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, static_pair>, char *iter, static_pair value) noexcept
{
	*iter++ = value.first;
	*iter++ = value.second;
	return iter;
}

struct dynamic_text
{
	::std::string_view value;
};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, dynamic_text>, dynamic_text value) noexcept
{
	return value.value.size();
}

inline constexpr ::std::size_t
print_reserve_static_stack_size(::fast_io::io_reserve_type_t<char, dynamic_text>) noexcept
{
	return 8u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_text>, char *iter, dynamic_text value) noexcept
{
	for (char ch : value.value)
	{
		*iter++ = ch;
	}
	return iter;
}

template <::std::size_t n>
inline void require_contents(::fast_io::basic_obuffer_view<char> const &view, char const (&expected)[n])
{
	assert(view.size() == n - 1u);
	assert(::std::string_view(view.data(), view.size()) == ::std::string_view(expected, n - 1u));
}

inline void test_print_exact_fit()
{
	{
		::std::array<char, 2u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		using output_ref = ::std::remove_cvref_t<decltype(
			::fast_io::operations::output_stream_ref(output))>;
		static_assert(::fast_io::operations::decay::defines::has_obuffer_basic_operations<output_ref>);
		static_assert(::fast_io::operations::decay::defines::writable<output_ref>);
		::fast_io::print(output, static_pair{'A', 'B'});
		require_contents(output, "AB");
	}
	{
		// Cursor state must survive normalization across calls; the second reserve run exactly fills the remaining area.
		::std::array<char, 4u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::print(output, static_pair{'A', 'B'});
		::fast_io::print(output, static_pair{'C', 'D'});
		require_contents(output, "ABCD");
	}
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::println(output, static_pair{'A', 'B'});
		require_contents(output, "AB\n");
	}
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::print(output, dynamic_text{"xyz"});
		require_contents(output, "xyz");
	}
	{
		::std::array<char, 4u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::print(output, static_pair{'A', 'B'}, static_pair{'C', 'D'});
		require_contents(output, "ABCD");
	}
}

inline void test_scatter_exact_fit()
{
	char const first[]{'a'};
	char const second[]{'b', 'c'};
	::std::array<::fast_io::basic_io_scatter_t<char>, 2u> const character_scatters{{
		{first, 1u},
		{second, 2u},
	}};
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		auto const result{::fast_io::operations::scatter_write_some(
			output, character_scatters.data(), character_scatters.size())};
		assert(result.position == character_scatters.size());
		assert(result.position_in_scatter == 0u);
		require_contents(output, "abc");
	}
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::operations::scatter_write_all(
			output, character_scatters.data(), character_scatters.size());
		require_contents(output, "abc");
	}

	::std::array<::fast_io::io_scatter_t, 2u> const byte_scatters{{
		{first, 1u},
		{second, 2u},
	}};
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		auto const result{::fast_io::operations::scatter_write_some_bytes(
			output, byte_scatters.data(), byte_scatters.size())};
		assert(result.position == byte_scatters.size());
		assert(result.position_in_scatter == 0u);
		require_contents(output, "abc");
	}
	{
		::std::array<char, 3u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		::fast_io::operations::scatter_write_all_bytes(
			output, byte_scatters.data(), byte_scatters.size());
		require_contents(output, "abc");
	}
}

} // namespace

int main()
{
	test_print_exact_fit();
	test_scatter_exact_fit();
}
