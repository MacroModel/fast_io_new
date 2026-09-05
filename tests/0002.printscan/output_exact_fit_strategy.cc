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

struct counted_growable_output
{
	::std::array<char, 8u> storage{};
	char *begin{storage.data()};
	char *current{storage.data()};
	char *end{storage.data() + 4u};
	::std::size_t typed_overflows{};
	::std::size_t byte_overflows{};

	inline void reset(::std::size_t initial_capacity = 4u) noexcept
	{
		storage.fill('\0');
		begin = current = storage.data();
		end = storage.data() + initial_capacity;
		typed_overflows = byte_overflows = 0u;
	}

	inline void reset_null() noexcept
	{
		storage.fill('\0');
		begin = current = end = nullptr;
		typed_overflows = byte_overflows = 0u;
	}
};

struct counted_growable_output_ref
{
	using output_char_type = char;
	counted_growable_output *state{};
};

[[nodiscard]] inline constexpr counted_growable_output_ref output_stream_ref_define(
	counted_growable_output &output) noexcept
{
	return {__builtin_addressof(output)};
}

[[maybe_unused]] [[nodiscard]] inline constexpr char *obuffer_begin(
	counted_growable_output_ref output) noexcept
{
	return output.state->begin;
}

[[nodiscard]] inline constexpr char *obuffer_curr(counted_growable_output_ref output) noexcept
{
	return output.state->current;
}

[[nodiscard]] inline constexpr char *obuffer_end(counted_growable_output_ref output) noexcept
{
	return output.state->end;
}

inline constexpr void obuffer_set_curr(
	counted_growable_output_ref output, char *current) noexcept
{
	output.state->current = current;
}

inline void write_all_overflow_define(
	counted_growable_output_ref output, char const *first, char const *last) noexcept
{
	++output.state->typed_overflows;
	auto const count{static_cast<::std::size_t>(last - first)};
	auto const used{static_cast<::std::size_t>(output.state->current - output.state->storage.data())};
	assert(used + count <= output.state->storage.size());
	output.state->begin = output.state->storage.data();
	output.state->end = output.state->storage.data() + output.state->storage.size();
	for (; first != last; ++first)
	{
		*output.state->current++ = *first;
	}
}

inline void write_all_bytes_overflow_define(
	counted_growable_output_ref output, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	++output.state->byte_overflows;
	auto const count{static_cast<::std::size_t>(last - first)};
	auto const used{static_cast<::std::size_t>(output.state->current - output.state->storage.data())};
	assert(used + count <= output.state->storage.size());
	output.state->begin = output.state->storage.data();
	output.state->end = output.state->storage.data() + output.state->storage.size();
	for (; first != last; ++first)
	{
		*output.state->current++ = static_cast<char>(::std::to_integer<unsigned char>(*first));
	}
}

struct exact_remainder_output
{
	::std::array<char, 4u> tail{};
	char prefix{};
	char *current{tail.data()};
	char *end{tail.data()};
	::std::size_t typed_some_calls{};
	::std::size_t byte_some_calls{};

	inline void reset() noexcept
	{
		tail.fill('\0');
		prefix = '\0';
		current = end = tail.data();
		typed_some_calls = byte_some_calls = 0u;
	}
};

struct exact_remainder_output_ref
{
	using output_char_type = char;
	exact_remainder_output *state{};
};

[[nodiscard]] inline constexpr exact_remainder_output_ref output_stream_ref_define(
	exact_remainder_output &output) noexcept
{
	return {__builtin_addressof(output)};
}

[[maybe_unused]] [[nodiscard]] inline constexpr char *obuffer_begin(
	exact_remainder_output_ref output) noexcept
{
	return output.state->tail.data();
}

[[nodiscard]] inline constexpr char *obuffer_curr(exact_remainder_output_ref output) noexcept
{
	return output.state->current;
}

[[nodiscard]] inline constexpr char *obuffer_end(exact_remainder_output_ref output) noexcept
{
	return output.state->end;
}

inline constexpr void obuffer_set_curr(
	exact_remainder_output_ref output, char *current) noexcept
{
	output.state->current = current;
}

[[nodiscard]] inline char const *write_some_overflow_define(
	exact_remainder_output_ref output, char const *first, char const *last) noexcept
{
	++output.state->typed_some_calls;
	assert(first != last);
	assert(output.state->typed_some_calls == 1u);
	output.state->prefix = *first++;
	output.state->current = output.state->tail.data();
	output.state->end = output.state->tail.data() + output.state->tail.size();
	return first;
}

[[nodiscard]] inline ::std::byte const *write_some_bytes_overflow_define(
	exact_remainder_output_ref output, ::std::byte const *first,
	::std::byte const *last) noexcept
{
	++output.state->byte_some_calls;
	assert(first != last);
	assert(output.state->byte_some_calls == 1u);
	output.state->prefix = static_cast<char>(::std::to_integer<unsigned char>(*first++));
	output.state->current = output.state->tail.data();
	output.state->end = output.state->tail.data() + output.state->tail.size();
	return first;
}

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

inline void test_scalar_exact_fit_dispatch()
{
	char const payload[]{'a', 'b', 'c', 'd', 'e'};
	auto const *const bytes{reinterpret_cast<::std::byte const *>(payload)};
	counted_growable_output output;

	// Typed and byte `some`/`all` operations may publish the one-past put-area cursor. None may invoke an overflow
	// provider merely because the source exactly consumes all four writable elements.
	output.reset();
	auto const *typed_some_end{
		::fast_io::operations::write_some(output, payload, payload + 4u)};
	assert(typed_some_end == payload + 4u);
	assert(output.current == output.end);
	assert(output.typed_overflows == 0u);

	output.reset();
	::fast_io::operations::write_all(output, payload, payload + 4u);
	assert(output.current == output.end);
	assert(output.typed_overflows == 0u);

	output.reset();
	auto const *byte_some_end{
		::fast_io::operations::write_some_bytes(output, bytes, bytes + 4u)};
	assert(byte_some_end == bytes + 4u);
	assert(output.current == output.end);
	assert(output.byte_overflows == 0u);

	output.reset();
	::fast_io::operations::write_all_bytes(output, bytes, bytes + 4u);
	assert(output.current == output.end);
	assert(output.byte_overflows == 0u);

	// A real miss must still dispatch exactly once and preserve both the already selected protocol and complete prefix.
	output.reset();
	::fast_io::operations::write_all(output, payload, payload + 5u);
	assert(output.typed_overflows == 1u);
	assert(output.byte_overflows == 0u);
	assert(::std::string_view(output.storage.data(), 5u) == "abcde");

	output.reset();
	::fast_io::operations::write_all_bytes(output, bytes, bytes + 5u);
	assert(output.typed_overflows == 0u);
	assert(output.byte_overflows == 1u);
	assert(::std::string_view(output.storage.data(), 5u) == "abcde");

	// The canonical null empty range is complete before put-area inspection, locking, or overflow-provider dispatch.
	output.reset_null();
	auto const *const null_chars{static_cast<char const *>(nullptr)};
	auto const *const null_bytes{static_cast<::std::byte const *>(nullptr)};
	assert(::fast_io::operations::write_some(output, null_chars, null_chars) == null_chars);
	::fast_io::operations::write_all(output, null_chars, null_chars);
	assert(::fast_io::operations::write_some_bytes(output, null_bytes, null_bytes) == null_bytes);
	::fast_io::operations::write_all_bytes(output, null_bytes, null_bytes);
	assert(output.typed_overflows == 0u);
	assert(output.byte_overflows == 0u);
}

inline void test_cold_retry_exact_remainder()
{
	char const payload[]{'a', 'b', 'c', 'd', 'e'};
	auto const *const bytes{reinterpret_cast<::std::byte const *>(payload)};
	exact_remainder_output output;

	// The first native `some` call consumes one element and publishes a four-element put area. The remaining exact fit
	// must complete locally; a second provider call would be both redundant and observably different.
	output.reset();
	::fast_io::operations::write_all(output, payload, payload + 5u);
	assert(output.typed_some_calls == 1u);
	assert(output.byte_some_calls == 0u);
	assert(output.prefix == 'a');
	assert(output.current == output.end);
	assert(::std::string_view(output.tail.data(), output.tail.size()) == "bcde");

	output.reset();
	::fast_io::operations::write_all_bytes(output, bytes, bytes + 5u);
	assert(output.typed_some_calls == 0u);
	assert(output.byte_some_calls == 1u);
	assert(output.prefix == 'a');
	assert(output.current == output.end);
	assert(::std::string_view(output.tail.data(), output.tail.size()) == "bcde");
}

} // namespace

int main()
{
	test_print_exact_fit();
	test_scatter_exact_fit();
	test_scalar_exact_fit_dispatch();
	test_cold_retry_exact_remainder();
}
