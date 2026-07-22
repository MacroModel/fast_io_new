#include <array>
#include <cstddef>
#include <cstdlib>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fast_io.h>

namespace
{

struct sink_state
{
	::std::string output;
	::std::vector<::std::size_t> scatter_batches;
	::std::size_t contiguous_writes{};
};

struct direct_scatter_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct append_only_sink
{
	using output_char_type = char;
	sink_state *state;
};

struct non_idempotent_output
{
	sink_state *state;
};

struct non_idempotent_output_ref
{
	using output_char_type = char;
	sink_state *state;
};

inline constexpr direct_scatter_sink output_stream_ref_define(direct_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr append_only_sink output_stream_ref_define(append_only_sink sink) noexcept
{
	return sink;
}

inline constexpr non_idempotent_output_ref output_stream_ref_define(non_idempotent_output output) noexcept
{
	return {output.state};
}

// There is intentionally no output_stream_ref_define(non_idempotent_output_ref). An output-reference result is only
// required to be storable and character-bearing; the protocol never promises that normalizing it again is valid.

inline constexpr ::std::true_type print_buffered_preferred_stream(
	::fast_io::io_reserve_type_t<char, append_only_sink>) noexcept
{
	// This sink deliberately advertises cheap append without exposing obuffer cursors. It is the negative destination
	// proof for put-area-only sources: a generic buffered marker must not be enough to select incremental range output.
	return {};
}

inline constexpr ::std::size_t
scatter_write_maximum_count(::fast_io::io_reserve_type_t<char, direct_scatter_sink>) noexcept
{
	return 4u;
}

inline void write_all_overflow_define(direct_scatter_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_writes;
	sink.state->output.append(first, last);
}

inline void write_all_overflow_define(append_only_sink sink, char const *first, char const *last)
{
	++sink.state->contiguous_writes;
	sink.state->output.append(first, last);
}

inline void write_all_overflow_define(non_idempotent_output_ref sink, char const *first, char const *last)
{
	++sink.state->contiguous_writes;
	sink.state->output.append(first, last);
}

inline void scatter_write_all_overflow_define(direct_scatter_sink sink,
											  ::fast_io::basic_io_scatter_t<char> const *scatters,
											  ::std::size_t count)
{
	sink.state->scatter_batches.push_back(count);
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.state->output.append(scatters[i].base, scatters[i].len);
	}
}

struct lock_state
{
	::std::size_t lock_calls{};
	::std::size_t unlock_calls{};
	bool locked{};
};

struct counting_mutex_ref
{
	lock_state *state;

	inline void lock() const noexcept
	{
		if (state->locked)
		{
			::std::abort();
		}
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() const noexcept
	{
		if (!state->locked)
		{
			::std::abort();
		}
		state->locked = false;
		++state->unlock_calls;
	}
};

struct locked_scatter_sink
{
	using output_char_type = char;
	direct_scatter_sink unlocked;
	lock_state *lock;
};

inline constexpr locked_scatter_sink output_stream_ref_define(locked_scatter_sink sink) noexcept
{
	return sink;
}

inline constexpr counting_mutex_ref output_stream_mutex_ref_define(locked_scatter_sink sink) noexcept
{
	return {sink.lock};
}

inline constexpr direct_scatter_sink output_stream_unlocked_ref_define(locked_scatter_sink sink) noexcept
{
	return sink.unlocked;
}

struct scratch_alias_text
{
	char value;
};

// Alias conversion is allowed to materialize a temporary protocol object.  The temporary below deliberately exposes
// an immediate-use scatter whose storage is shared by every formatter invocation; it must therefore remain outside
// the retained-scatter strategy even when the original range elements are stable lvalues.
struct scratch_alias_proxy
{
	char value;
};

struct unprintable_element
{};

struct fixed_text
{};

struct move_only_text_range
{
	::std::vector<::std::string_view> values;

	explicit move_only_text_range(::std::vector<::std::string_view> initial_values)
		: values(::std::move(initial_values))
	{}

	move_only_text_range(move_only_text_range &&) = default;
	move_only_text_range &operator=(move_only_text_range &&) = default;
	move_only_text_range(move_only_text_range const &) = delete;
	move_only_text_range &operator=(move_only_text_range const &) = delete;

	inline auto begin() noexcept
	{
		return values.begin();
	}

	inline auto end() noexcept
	{
		return values.end();
	}

	inline auto begin() const noexcept
	{
		return values.begin();
	}

	inline auto end() const noexcept
	{
		return values.end();
	}
};

static_assert(::std::ranges::range<move_only_text_range>);
static_assert(!::std::copy_constructible<move_only_text_range>);

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, fixed_text>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_text>, char *destination, fixed_text) noexcept
{
	*destination = 'F';
	return destination + 1u;
}

struct counting_forward_range
{
	struct iterator
	{
		using value_type = ::std::string_view;
		using difference_type = ::std::ptrdiff_t;
		using iterator_concept = ::std::forward_iterator_tag;
		using iterator_category = ::std::forward_iterator_tag;

		::std::string_view const *current;
		::std::size_t *increments;

		inline constexpr ::std::string_view const &operator*() const noexcept
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

struct counting_input_iterator
{
	using value_type = ::std::string_view;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;
	using iterator_category = ::std::input_iterator_tag;

	::std::string_view const *current;
	::std::size_t *increments;

	inline constexpr ::std::string_view const &operator*() const noexcept
	{
		return *current;
	}

	inline constexpr counting_input_iterator &operator++() noexcept
	{
		++current;
		++*increments;
		return *this;
	}

	inline constexpr void operator++(int) noexcept
	{
		++*this;
	}

	friend inline constexpr bool operator==(counting_input_iterator left,
										 counting_input_iterator right) noexcept
	{
		return left.current == right.current;
	}
};

static_assert(::std::ranges::forward_range<counting_forward_range>);
static_assert(::std::ranges::sized_range<counting_forward_range>);
static_assert(::std::input_iterator<counting_input_iterator>);
static_assert(!::std::forward_iterator<counting_input_iterator>);

using unprintable_sized_forward_view =
	::fast_io::sized_range_view_t<char, unprintable_element *>;

// A multipass iterator is a traversal property, not a formatting protocol. The sized range must be rejected during
// capability admission rather than selecting dynamic reserve and failing in its implementation body.
static_assert(!::fast_io::dynamic_reserve_printable<char, unprintable_sized_forward_view>);

inline constexpr scratch_alias_proxy
print_alias_define(::fast_io::io_alias_t, scratch_alias_text const &text) noexcept
{
	return {text.value};
}

inline ::fast_io::basic_io_scatter_t<char>
print_scatter_define(::fast_io::io_reserve_type_t<char, scratch_alias_proxy>, scratch_alias_proxy proxy) noexcept
{
	// Every call overwrites the same storage.  Retaining several descriptors would make them all observe the final
	// element, whereas the sequential contiguous strategy consumes each descriptor before the next overwrite.
	static char scratch[2u];
	scratch[0] = proxy.value;
	scratch[1] = '!';
	return {scratch, 2u};
}

[[noreturn]] inline void test_failure() noexcept
{
	::std::abort();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		test_failure();
	}
}

inline void require_batches_within_hard_limit(sink_state const &state)
{
	for (auto const count : state.scatter_batches)
	{
		require(count != 0u);
		require(count <= 4u);
	}
}

template <typename operation>
inline sink_state capture(operation &&op)
{
	sink_state state;
	direct_scatter_sink sink{__builtin_addressof(state)};
	::std::forward<operation>(op)(sink);
	require_batches_within_hard_limit(state);
	return state;
}

template <typename printable>
inline sink_state capture_print(printable const &value)
{
	return capture([&](direct_scatter_sink sink) { ::fast_io::print(sink, value); });
}

template <typename printable>
inline sink_state capture_println(printable const &value)
{
	return capture([&](direct_scatter_sink sink) { ::fast_io::println(sink, value); });
}

inline void test_cardinality_and_separators()
{
	using namespace ::std::literals;

	::std::array<::std::string_view, 0u> empty{};
	auto empty_range{::fast_io::mnp::rgvw(empty, "|")};
	static_assert(::fast_io::dynamic_reserve_printable<char, decltype(empty_range)>);
	static_assert(::fast_io::dynamic_reserve_scatters_printable<char, decltype(empty_range)>);
	static_assert(::fast_io::precise_reserve_printable<char, decltype(empty_range)>);
	auto empty_state{capture_print(empty_range)};
	require(empty_state.output.empty());
	require(empty_state.scatter_batches.empty());
	require(::fast_io::concat_std(empty_range).empty());

	auto empty_line_state{capture_println(empty_range)};
	require(empty_line_state.output == "\n");

	::std::array one{"one"sv};
	auto one_range{::fast_io::mnp::rgvw(one, "|")};
	auto one_state{capture_print(one_range)};
	require(one_state.output == "one");
	require(::fast_io::concat_std(one_range) == "one");

	::std::array two{"left"sv, "right"sv};
	auto two_range{::fast_io::mnp::rgvw(two, "|")};
	static_assert(::fast_io::dynamic_reserve_scatters_printable<char, decltype(two_range)>);
	static_assert(::fast_io::precise_reserve_printable<char, decltype(two_range)>);
	static_assert(::fast_io::put_area_printable_preferred<char, decltype(two_range)>);
	static_assert(!::fast_io::buffered_printable_preferred<char, decltype(two_range)>);
	static_assert(::fast_io::put_area_printable_preferred<
		char, ::fast_io::parameter<decltype(two_range) &>>);
	auto two_state{capture_print(two_range)};
	require(two_state.output == "left|right");
	require(two_state.scatter_batches.size() == 1u);
	require(two_state.scatter_batches.front() == 3u);
	require(::fast_io::concat_std(two_range) == "left|right");

	sink_state append_state;
	::fast_io::print(append_only_sink{__builtin_addressof(append_state)}, two_range);
	require(append_state.output == "left|right");
	// The put-area marker must not inherit the append-only stream marker. The canonical contiguous strategy performs
	// one final write; the forbidden incremental route would publish the range in several writes.
	require(append_state.contiguous_writes == 1u);

	::std::array with_empty{""sv, "middle"sv, ""sv};
	auto empty_elements{::fast_io::mnp::rgvw(with_empty, "|")};
	auto empty_elements_state{capture_print(empty_elements)};
	require(empty_elements_state.output == "|middle|");
	require(::fast_io::concat_std(empty_elements) == "|middle|");

	auto empty_separator{::fast_io::mnp::rgvw(two, "")};
	auto empty_separator_state{capture_print(empty_separator)};
	require(empty_separator_state.output == "leftright");
	require(::fast_io::concat_std(empty_separator) == "leftright");

	::std::array<fixed_text, 3u> fixed_values{};
	auto fixed_range{::fast_io::mnp::rgvw(fixed_values, "|")};
	static_assert(::fast_io::put_area_printable_preferred<char, decltype(fixed_range)>);
	static_assert(!::fast_io::buffered_printable_preferred<char, decltype(fixed_range)>);
	sink_state fixed_append_state;
	::fast_io::print(append_only_sink{__builtin_addressof(fixed_append_state)}, fixed_range);
	require(fixed_append_state.output == "F|F|F");
	// A broad append preference would invoke range print_define and publish one separator/element pair at a time.
	// Put-area-only admission keeps this generic append destination on one final contiguous write.
	require(fixed_append_state.contiguous_writes == 1u);
}

inline void test_plain_line_and_composed_runs()
{
	using namespace ::std::literals;
	::std::array values{"a"sv, "b"sv, "c"sv, "d"sv, "e"sv, "f"sv};
	auto range{::fast_io::mnp::rgvw(values, ",")};
	::std::string const expected{"a,b,c,d,e,f"};

	auto plain{capture_print(range)};
	require(plain.output == expected);
	require(plain.scatter_batches == ::std::vector<::std::size_t>({4u, 4u, 3u}));

	auto line{capture_println(range)};
	require(line.output == expected + "\n");
	require(line.scatter_batches == ::std::vector<::std::size_t>({4u, 4u, 4u}));

	auto surrounded_state{capture([&](direct_scatter_sink sink) {
		::fast_io::print(sink, "pre["sv, range, "]post"sv);
	})};
	require(surrounded_state.output == "pre[" + expected + "]post");
	require(surrounded_state.scatter_batches ==
			::std::vector<::std::size_t>({4u, 4u, 4u, 1u}));
	require(::fast_io::concat_std("pre["sv, range, "]post"sv) == "pre[" + expected + "]post");
}

inline void test_condition_and_nested_pack()
{
	using namespace ::std::literals;
	::std::array values{"red"sv, "green"sv, "blue"sv};
	auto range{::fast_io::mnp::rgvw(values, "/")};

	auto true_branch{::fast_io::mnp::pack("colors="sv, range)};
	auto false_branch{::fast_io::mnp::pack("colors=none"sv)};
	auto selected_true{::fast_io::mnp::cond(true, true_branch, false_branch)};
	auto selected_false{::fast_io::mnp::cond(false, true_branch, false_branch)};
	auto true_record{::fast_io::mnp::pack("<"sv, ::fast_io::mnp::pack(selected_true), ">"sv)};
	auto false_record{::fast_io::mnp::pack("<"sv, ::fast_io::mnp::pack(selected_false), ">"sv)};

	auto true_state{capture_print(true_record)};
	require(true_state.output == "<colors=red/green/blue>");
	require(!true_state.scatter_batches.empty());
	require(::fast_io::concat_std(true_record) == "<colors=red/green/blue>");

	auto false_state{capture_print(false_record)};
	require(false_state.output == "<colors=none>");
	require(::fast_io::concat_std(false_record) == "<colors=none>");
}

inline void test_singleton_semantic_put_area_keeps_one_pass()
{
	using namespace ::std::literals;
	::std::array values{"left"sv, "middle"sv, "right"sv};
	::std::size_t increments{};
	counting_forward_range source{values.data(), values.size(), __builtin_addressof(increments)};
	auto range{::fast_io::mnp::rgvw(source, "|")};
	static_assert(::fast_io::put_area_printable_preferred<char, decltype(range)>);
	::std::string_view const expected{"left|middle|right"};
	::std::string const expected_line{::std::string(expected) + "\n"};

	auto verify = [&]<bool line>(auto const &formatted) {
		increments = 0u;
		::std::array<char, 64u> storage{};
		::fast_io::basic_obuffer_view<char> output(storage);
		if constexpr (line)
		{
			::fast_io::println(output, formatted);
			require(::std::string_view(storage.data(), output.size()) == expected_line);
		}
		else
		{
			::fast_io::print(output, formatted);
			require(::std::string_view(storage.data(), output.size()) == expected);
		}
		// One increment per source element proves that neither semantic flattening nor line ownership re-entered the
		// precise size pass. The old composition path reported exactly twice this count.
		require(increments == values.size());
	};

	verify.template operator()<false>(range);
	auto packed{::fast_io::mnp::pack(range)};
	verify.template operator()<false>(packed);
	auto selected{::fast_io::mnp::cond(true, range)};
	verify.template operator()<false>(selected);
	verify.template operator()<true>(packed);

	increments = 0u;
	::std::array<char, 64u> adjacent_storage{};
	::fast_io::basic_obuffer_view<char> adjacent_output(adjacent_storage);
	::fast_io::print(adjacent_output, ""sv, range, ""sv);
	require(::std::string_view(adjacent_storage.data(), adjacent_output.size()) == expected);
	// Empty adjacent scatters change only the ordinary source-shape classifier. They must not make a true put-area
	// destination premeasure a direct range whose explicit cost marker proves that one-pass emission is preferable.
	require(increments == values.size());

	increments = 0u;
	::std::array<char, 64u> framed_storage{};
	::fast_io::basic_obuffer_view<char> framed_output(framed_storage);
	::fast_io::println(framed_output, "pre["sv, range, "]post"sv);
	require(::std::string_view(framed_storage.data(), framed_output.size()) ==
			"pre[left|middle|right]post\n");
	require(increments == values.size());
}

inline void test_unsized_range_does_not_renormalize_output_ref()
{
	using namespace ::std::literals;
	::std::array values{"first"sv, "second"sv, "third"sv};
	::std::size_t increments{};
	using iterator = counting_input_iterator;
	::fast_io::range_view_t<char, iterator> range{
		{"::", 2u},
		{values.data(), __builtin_addressof(increments)},
		{values.data() + values.size(), __builtin_addressof(increments)}};

	sink_state state;
	::fast_io::print(non_idempotent_output{__builtin_addressof(state)}, range);
	require(state.output == "first::second::third");
	require(state.contiguous_writes != 0u);
	require(increments == values.size());
	// Compilation itself proves the important negative contract: nested range emission used the already-selected
	// observer directly. Re-entering the public entry would require the deliberately absent ref-of-ref customization.
}

inline void test_heap_descriptor_plan()
{
	using namespace ::std::literals;
	::std::vector<::std::string_view> values(600u, "abcdefgh"sv);
	auto range{::fast_io::mnp::rgvw(values, "::")};
	::std::string expected;
	expected.reserve(5998u);
	for (::std::size_t i{}; i != values.size(); ++i)
	{
		if (i != 0u)
		{
			expected.append("::");
		}
		expected.append(values[i]);
	}

	auto state{capture_print(range)};
	require(state.output == expected);
	require(state.scatter_batches.size() == 300u);
	require(state.scatter_batches.front() == 4u);
	require(state.scatter_batches.back() == 3u);
	require(::fast_io::concat_std(range) == expected);
}

inline void test_width_placements()
{
	using namespace ::std::literals;
	::std::array values{"aa"sv, "b"sv};
	auto range{::fast_io::mnp::rgvw(values, "|")};
	constexpr ::std::size_t width{300u};
	constexpr ::std::size_t child_size{4u};
	constexpr ::std::size_t padding{width - child_size};
	::std::string const child{"aa|b"};

	auto left{::fast_io::mnp::left(range, width, '.')};
	auto middle{::fast_io::mnp::middle(range, width, '.')};
	auto right{::fast_io::mnp::right(range, width, '.')};
	auto internal{::fast_io::mnp::internal(range, width, '.')};
	static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_runtime_scatter<
		char, decltype(left)>());
	using forwarded_left_type = decltype(
		::fast_io::io_print_forward<char>(::fast_io::io_print_alias(left)));
	static_assert(::fast_io::operations::decay::print_semantic_top_level_width_v<forwarded_left_type>);
	static_assert(::fast_io::operations::decay::print_semantic_top_level_width_has_runtime_scatter<
		char, forwarded_left_type>());

	::std::string const expected_left{child + ::std::string(padding, '.')};
	::std::size_t const middle_left{padding >> 1u};
	::std::string const expected_middle{::std::string(middle_left, '.') + child +
											 ::std::string(padding - middle_left, '.')};
	::std::string const expected_right{::std::string(padding, '.') + child};

	auto left_state{capture_print(left)};
	require(left_state.output == expected_left);
	auto middle_state{capture_print(middle)};
	require(middle_state.output == expected_middle);
	auto right_state{capture_print(right)};
	require(right_state.output == expected_right);
	auto internal_state{capture_print(internal)};
	// A range has no internal shift point, so internal placement follows the documented right-placement fallback.
	require(internal_state.output == expected_right);

	require(::fast_io::concat_std(left) == expected_left);
	require(::fast_io::concat_std(middle) == expected_middle);
	require(::fast_io::concat_std(right) == expected_right);
	require(::fast_io::concat_std(internal) == expected_right);
}

inline void test_owning_prvalue_elements_are_materialized()
{
	::std::array seeds{'a', 'b', 'c'};
	auto owning_values{seeds | ::std::views::transform([](char ch) {
		// Exceed common small-string capacities so an incorrectly retained scatter is caught by address sanitizers.
		return ::std::string(96u, ch);
	})};
	auto range{::fast_io::mnp::rgvw(owning_values, "|")};
	static_assert(::fast_io::printable<char, decltype(range)>);
	static_assert(!::fast_io::dynamic_reserve_printable<char, decltype(range)>);
	static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, decltype(range)>);

	::std::string const expected{::std::string(96u, 'a') + "|" + ::std::string(96u, 'b') + "|" +
							 ::std::string(96u, 'c')};
	auto state{capture_print(range)};
	require(state.output == expected);
	require(::fast_io::concat_std(range) == expected);
}

inline void test_lvalue_scratch_alias_is_not_retained()
{
	::std::array values{scratch_alias_text{'a'}, scratch_alias_text{'b'}, scratch_alias_text{'c'}};
	auto range{::fast_io::mnp::rgvw(values, "|")};
	static_assert(::fast_io::printable<char, decltype(range)>);
	static_assert(!::fast_io::dynamic_reserve_printable<char, decltype(range)>);
	static_assert(!::fast_io::dynamic_reserve_scatters_printable<char, decltype(range)>);

	// Contiguous materialization consumes each alias immediately, so later calls cannot overwrite retained output.
	auto state{capture_print(range)};
	require(state.output == "a!|b!|c!");
	require(::fast_io::concat_std(range) == "a!|b!|c!");
}

inline void test_owned_rvalue_range_lifetime()
{
	using namespace ::std::literals;

	// An owning container can be moved into rgvw and the returned formatter can outlive the construction expression.
	auto owned{::fast_io::mnp::rgvw(
		::std::vector<::std::string_view>{"red"sv, "green"sv, "blue"sv}, "::")};
	require(::fast_io::concat_std(owned) == "red::green::blue");
	auto owned_state{capture_print(owned)};
	require(owned_state.output == "red::green::blue");
	auto move_only_owned{::fast_io::mnp::rgvw(
		move_only_text_range{{"left"sv, "right"sv}}, "|")};
	require(::fast_io::concat_std(move_only_owned) == "left|right");

	// transform_view iterators can refer to their parent view's callable. Move the completed owning formatter once more
	// before printing: success proves that rgvw did not cache iterators before either move.
	::std::vector<::std::string_view> source{"alpha"sv, "bravo"sv, "cider"sv};
	auto transformed{::fast_io::mnp::rgvw(
		source | ::std::views::transform([](::std::string_view value) { return value.substr(0u, 1u); }), "/")};
	auto moved{::std::move(transformed)};
	require(::fast_io::concat_std(moved) == "a/b/c");

	// Preserve the historical nested call as well as the stored-view case above.
	auto immediate_state{capture([&](direct_scatter_sink sink) {
		::fast_io::print(
			sink,
			::fast_io::mnp::rgvw(
				source | ::std::views::transform([](::std::string_view value) { return value; }), ","));
	})};
	require(immediate_state.output == "alpha,bravo,cider");
}

inline void test_mutex_scope()
{
	using namespace ::std::literals;
	::std::array values{"0"sv, "1"sv, "2"sv, "3"sv, "4"sv, "5"sv};
	auto range{::fast_io::mnp::rgvw(values, ":")};

	sink_state state;
	lock_state lock;
	locked_scatter_sink sink{{__builtin_addressof(state)}, __builtin_addressof(lock)};
	::fast_io::println(sink, range);

	require(state.output == "0:1:2:3:4:5\n");
	require_batches_within_hard_limit(state);
	require(state.scatter_batches == ::std::vector<::std::size_t>({4u, 4u, 4u}));
	require(lock.lock_calls == 1u);
	require(lock.unlock_calls == 1u);
	require(!lock.locked);
}

} // namespace

int main()
{
	static_assert(::fast_io::scatter_write_maximum_count_stream<char, direct_scatter_sink>);
	static_assert(::fast_io::details::scatter_write_maximum_count_or_unlimited<char, direct_scatter_sink>() == 4u);

	test_cardinality_and_separators();
	test_plain_line_and_composed_runs();
	test_condition_and_nested_pack();
	test_singleton_semantic_put_area_keeps_one_pass();
	test_unsized_range_does_not_renormalize_output_ref();
	test_heap_descriptor_plan();
	test_width_placements();
	test_owning_prvalue_elements_are_materialized();
	test_lvalue_scratch_alias_is_not_retained();
	test_owned_rvalue_range_lifetime();
	test_mutex_scope();
}
