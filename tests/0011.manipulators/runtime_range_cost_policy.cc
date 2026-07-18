#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

#include <fast_io.h>

namespace
{

struct sink_state
{
	::std::string output;
	::std::size_t contiguous_calls{};
	::std::size_t scatter_calls{};
};

struct direct_streaming_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct unmarked_write_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct marked_native_scatter_sink
{
	using output_char_type = char;
	sink_state *state;
};

inline constexpr direct_streaming_sink output_stream_ref_define(direct_streaming_sink sink) noexcept
{
	return sink;
}

inline constexpr unmarked_write_sink output_stream_ref_define(unmarked_write_sink sink) noexcept
{
	return sink;
}

inline constexpr marked_native_scatter_sink output_stream_ref_define(marked_native_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, direct_streaming_sink>) noexcept
{
	// This test sink models an in-memory/fake boundary whose individual calls are intentionally cheap. A real file
	// does not acquire this property merely by exposing write_all; production sinks must make the same positive choice.
	return {};
}

inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, marked_native_scatter_sink>) noexcept
{
	// Declaring both policies verifies priority: native scatter must still beat direct per-element streaming.
	return {};
}

inline constexpr ::std::size_t scatter_write_maximum_count(
	::fast_io::io_reserve_type_t<char, marked_native_scatter_sink>) noexcept
{
	return 64u;
}

inline void write_all_overflow_define(
	direct_streaming_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_calls;
	sink.state->output.append(first, last);
}

inline void write_all_overflow_define(
	unmarked_write_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_calls;
	sink.state->output.append(first, last);
}

inline void write_all_overflow_define(
	marked_native_scatter_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_calls;
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(
	marked_native_scatter_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	++sink.state->scatter_calls;
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->output.append(scatters[i].base, scatters[i].len);
	}
}

struct counting_forward_range
{
	struct iterator
	{
		using value_type = ::std::string_view;
		using difference_type = ::std::ptrdiff_t;
		using iterator_concept = ::std::forward_iterator_tag;
		using iterator_category = ::std::forward_iterator_tag;

		value_type const *current;
		::std::size_t *increments;

		inline constexpr value_type const &operator*() const noexcept
		{
			return *current;
		}

		inline constexpr iterator &operator++() noexcept
		{
			++current;
			++*increments;
			return *this;
		}

		inline constexpr iterator operator++(int) noexcept
		{
			auto copy{*this};
			++*this;
			return copy;
		}

		friend inline constexpr bool operator==(iterator left, iterator right) noexcept
		{
			return left.current == right.current;
		}
	};

	::std::string_view const *first;
	::std::size_t count;
	::std::size_t *increments;

	inline constexpr iterator begin() const noexcept
	{
		return {first, increments};
	}

	inline constexpr iterator end() const noexcept
	{
		return {first + count, increments};
	}

	inline constexpr ::std::size_t size() const noexcept
	{
		return count;
	}
};

struct counting_contiguous_iterator
{
	using value_type = ::std::string_view;
	using difference_type = ::std::ptrdiff_t;
	using reference = value_type &;
	using pointer = value_type *;
	using iterator_concept = ::std::contiguous_iterator_tag;
	using iterator_category = ::std::random_access_iterator_tag;

	pointer current;
	::std::size_t *increments;
	::std::size_t *random_advances;

	inline constexpr reference operator*() const noexcept
	{
		return *current;
	}

	inline constexpr pointer operator->() const noexcept
	{
		return current;
	}

	inline constexpr reference operator[](difference_type offset) const noexcept
	{
		return current[offset];
	}

	inline constexpr counting_contiguous_iterator &operator++() noexcept
	{
		++current;
		++*increments;
		return *this;
	}

	inline constexpr counting_contiguous_iterator operator++(int) noexcept
	{
		auto copy{*this};
		++*this;
		return copy;
	}

	inline constexpr counting_contiguous_iterator &operator--() noexcept
	{
		--current;
		return *this;
	}

	inline constexpr counting_contiguous_iterator operator--(int) noexcept
	{
		auto copy{*this};
		--*this;
		return copy;
	}

	inline constexpr counting_contiguous_iterator &operator+=(difference_type offset) noexcept
	{
		current += offset;
		++*random_advances;
		return *this;
	}

	inline constexpr counting_contiguous_iterator &operator-=(difference_type offset) noexcept
	{
		current -= offset;
		++*random_advances;
		return *this;
	}

	friend inline constexpr counting_contiguous_iterator operator+(
		counting_contiguous_iterator iterator, difference_type offset) noexcept
	{
		iterator += offset;
		return iterator;
	}

	friend inline constexpr counting_contiguous_iterator operator+(
		difference_type offset, counting_contiguous_iterator iterator) noexcept
	{
		iterator += offset;
		return iterator;
	}

	friend inline constexpr counting_contiguous_iterator operator-(
		counting_contiguous_iterator iterator, difference_type offset) noexcept
	{
		iterator -= offset;
		return iterator;
	}

	friend inline constexpr difference_type operator-(
		counting_contiguous_iterator left, counting_contiguous_iterator right) noexcept
	{
		return left.current - right.current;
	}

	friend inline constexpr bool operator==(
		counting_contiguous_iterator left, counting_contiguous_iterator right) noexcept
	{
		return left.current == right.current;
	}

	friend inline constexpr auto operator<=>(
		counting_contiguous_iterator left, counting_contiguous_iterator right) noexcept
	{
		return left.current <=> right.current;
	}
};

struct initializing_resize_text
{
	::std::string value;
	::std::size_t resize_calls{};
	::std::size_t append_calls{};
};

inline initializing_resize_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, initializing_resize_text>,
	char const *first, char const *last)
{
	return {::std::string(first, last), 0u, 0u};
}

inline char *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, initializing_resize_text>,
	initializing_resize_text &text, ::std::size_t size)
{
	++text.resize_calls;
	text.value.resize(size);
	return text.value.data();
}

inline constexpr ::fast_io::io_strlike_reference_wrapper<char, initializing_resize_text>
io_strlike_ref(::fast_io::io_alias_t, initializing_resize_text &text) noexcept
{
	return {__builtin_addressof(text)};
}

inline void strlike_push_back(
	::fast_io::io_strlike_type_t<char, initializing_resize_text>,
	initializing_resize_text &text, char ch)
{
	++text.append_calls;
	text.value.push_back(ch);
}

inline void strlike_append(
	::fast_io::io_strlike_type_t<char, initializing_resize_text>,
	initializing_resize_text &text, char const *first, char const *last)
{
	++text.append_calls;
	text.value.append(first, last);
}

struct noinit_resize_text
{
	::std::array<char, 256u> storage{};
	::std::size_t size{};
	::std::size_t resize_calls{};
};

inline noinit_resize_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, noinit_resize_text>,
	char const *first, char const *last)
{
	noinit_resize_text result;
	for (; first != last; ++first)
	{
		result.storage[result.size++] = *first;
	}
	return result;
}

inline char *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, noinit_resize_text>,
	noinit_resize_text &text, ::std::size_t size)
{
	if (text.storage.size() < size)
	{
		::std::abort();
	}
	++text.resize_calls;
	text.size = size;
	// `std::array` already owns live characters, and changing the logical endpoint writes none of them.
	return text.storage.data();
}

inline constexpr ::std::true_type strlike_precise_resize_without_initialization(
	::fast_io::io_strlike_type_t<char, noinit_resize_text>) noexcept
{
	return {};
}

inline ::std::string_view view(noinit_resize_text const &text) noexcept
{
	return {text.storage.data(), text.size};
}

using raw_range_type = ::fast_io::sized_range_view_t<char, ::std::string_view *>;

static_assert(::fast_io::dynamic_reserve_scatters_printable<char, raw_range_type>);
static_assert(::fast_io::one_pass_printable_preferred<char, raw_range_type>);
static_assert(::fast_io::precise_resize_initialization_sensitive_printable<char, raw_range_type>);
static_assert(::fast_io::precise_resize_writable_strlike<char, ::std::string>);
static_assert(!::fast_io::precise_resize_without_initialization_strlike<char, ::std::string>);
static_assert(::fast_io::precise_resize_without_initialization_strlike<char, noinit_resize_text>);
static_assert(!::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
			  char, ::std::string, raw_range_type>);
static_assert(::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
			  char, noinit_resize_text, raw_range_type>);
static_assert(::std::contiguous_iterator<counting_contiguous_iterator>);

inline constexpr ::std::array<::std::string_view, 4u> values{"a", "bc", "def", "g"};
inline constexpr ::std::string_view expected{"a::bc::def::g"};

void test_source_destination_pair_and_native_priority()
{
	::std::size_t increments{};
	counting_forward_range source{values.data(), values.size(), __builtin_addressof(increments)};
	auto range{::fast_io::mnp::rgvw(source, "::")};
	static_assert(::fast_io::one_pass_printable_preferred<char, decltype(range)>);

	sink_state direct_state;
	::fast_io::print(direct_streaming_sink{__builtin_addressof(direct_state)}, range);
	assert(direct_state.output == expected);
	// The first element is one call; every later separator/element pair remains two calls on this deliberately
	// no-scatter sink. The destination marker states that those calls are cheaper than whole-range materialization.
	assert(direct_state.contiguous_calls == values.size() * 2u - 1u);
	assert(direct_state.scatter_calls == 0u);
	assert(increments == values.size());

	increments = 0u;
	sink_state unmarked_state;
	::fast_io::print(unmarked_write_sink{__builtin_addressof(unmarked_state)}, range);
	assert(unmarked_state.output == expected);
	assert(unmarked_state.contiguous_calls == 1u);
	assert(unmarked_state.scatter_calls == 0u);
	assert(increments == values.size() * 2u);

	increments = 0u;
	sink_state scatter_state;
	::fast_io::print(marked_native_scatter_sink{__builtin_addressof(scatter_state)}, range);
	assert(scatter_state.output == expected);
	assert(scatter_state.contiguous_calls == 0u);
	assert(scatter_state.scatter_calls == 1u);
	assert(increments == values.size());
}

void test_semantic_singleton_and_mixed_record_boundary()
{
	::std::size_t increments{};
	counting_forward_range source{values.data(), values.size(), __builtin_addressof(increments)};
	auto range{::fast_io::mnp::rgvw(source, "::")};

	sink_state semantic_state;
	::fast_io::print(
		direct_streaming_sink{__builtin_addressof(semantic_state)}, ::fast_io::mnp::pack(range));
	assert(semantic_state.output == expected);
	assert(increments == values.size());

	// The policy is deliberately singleton-only. An adjacent record leaf keeps whole-record materialization instead of
	// treating the stream marker as permission to split every mixed composition into incremental calls.
	increments = 0u;
	sink_state mixed_state;
	::fast_io::print(direct_streaming_sink{__builtin_addressof(mixed_state)}, "[", range, "]");
	assert(mixed_state.output == ::std::string("[") + ::std::string(expected) + "]");
	assert(increments == values.size() * 2u);
	assert(mixed_state.contiguous_calls == 1u);
}

void test_contiguous_pointer_end_loop()
{
	auto mutable_values{values};
	::std::size_t increments{};
	::std::size_t random_advances{};
	counting_contiguous_iterator first{
		mutable_values.data(), __builtin_addressof(increments), __builtin_addressof(random_advances)};
	::fast_io::sized_range_view_t<char, counting_contiguous_iterator> range{
		{"::", 2u}, first, mutable_values.size()};

	sink_state state;
	::fast_io::print(direct_streaming_sink{__builtin_addressof(state)}, range);
	assert(state.output == expected);
	assert(increments == mutable_values.size());
	// One random-access advance constructs the invariant endpoint; the hot loop itself advances only one cursor.
	assert(random_advances == 1u);
}

void test_concat_initialization_cost_gate()
{
	::std::size_t increments{};
	counting_forward_range source{
		values.data(), values.size(), __builtin_addressof(increments)};
	auto range{::fast_io::mnp::rgvw(source, "::")};
	using range_type = decltype(range);
	static_assert(::fast_io::precise_resize_initialization_sensitive_printable<char, range_type>);
	static_assert(!::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
				  char, initializing_resize_text, range_type>);
	static_assert(::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
				  char, noinit_resize_text, range_type>);
	static_assert(::fast_io::details::decay::basic_general_concat_initialization_sensitive_staging_required_v<
				  char, initializing_resize_text, range_type>);
	static_assert(!::fast_io::details::decay::basic_general_concat_initialization_sensitive_staging_required_v<
				  char, noinit_resize_text, range_type>);

	increments = 0u;
	auto portable{::fast_io::basic_general_concat_checked<false, char, initializing_resize_text>(range)};
	assert(portable.value == expected);
	assert(portable.resize_calls == 0u);
	// The portable result deliberately supports append, so this proves policy rather than mere capability failure:
	// concat measures and emits into one contiguous staging buffer, then range-constructs the final object. It neither
	// value-initializes the final extent nor degrades the range producer into per-element destination appends.
	assert(portable.append_calls == 0u);
	assert(increments == values.size() * 2u);

	increments = 0u;
	auto noinit{::fast_io::basic_general_concat_checked<false, char, noinit_resize_text>(range)};
	assert(view(noinit) == expected);
	assert(noinit.resize_calls == 1u);
	assert(increments == values.size() * 2u);

	increments = 0u;
	auto standard{::fast_io::concat_std(range)};
	assert(standard == expected);
	assert(increments == values.size() * 2u);

	// A semantic width has a whole-graph placement/copy trade-off and retains the existing semantic exact-resize path.
	// The ordinary range leaf marker must not silently disable that independent strategy.
	auto semantic{::fast_io::basic_general_concat_checked<false, char, initializing_resize_text>(
		::fast_io::mnp::left(range, 20u, '.'))};
	assert(semantic.value == ::std::string(expected) + ".......");
	assert(semantic.resize_calls == 1u);
	assert(semantic.append_calls == 0u);
}

} // namespace

int main()
{
	test_source_destination_pair_and_native_priority();
	test_semantic_singleton_and_mixed_record_boundary();
	test_contiguous_pointer_end_loop();
	test_concat_initialization_cost_gate();
}
