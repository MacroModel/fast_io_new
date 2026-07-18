#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace stream_ref_protocol
{

struct input_proxy
{
	using input_char_type = char;
	int *identity{};
};

struct referenced_input_source
{
	input_proxy proxy;
};

inline constexpr input_proxy &input_stream_ref_define(referenced_input_source &source) noexcept
{
	return source.proxy;
}

struct noncopyable_input_proxy
{
	using input_char_type = char;
	noncopyable_input_proxy() = default;
	noncopyable_input_proxy(noncopyable_input_proxy const &) = delete;
};

struct noncopyable_referenced_source
{
	noncopyable_input_proxy proxy;
};

inline constexpr noncopyable_input_proxy &
input_stream_ref_define(noncopyable_referenced_source &source) noexcept
{
	return source.proxy;
}

struct void_output_source
{};

inline constexpr void output_stream_ref_define(void_output_source &) noexcept
{}

struct scalar_output_source
{};

inline constexpr int output_stream_ref_define(scalar_output_source &) noexcept
{
	return 0;
}

struct input_only_io_source
{
	input_proxy proxy;
};

inline constexpr input_proxy &io_stream_ref_define(input_only_io_source &source) noexcept
{
	return source.proxy;
}

static_assert(::fast_io::operations::defines::has_input_stream_ref_define<referenced_input_source &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<referenced_input_source &>())),
	input_proxy &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<
	noncopyable_referenced_source &>);
static_assert(::std::same_as<
	decltype(::fast_io::operations::input_stream_ref(
		::std::declval<noncopyable_referenced_source &>())),
	noncopyable_input_proxy &>);
static_assert(!::fast_io::operations::defines::has_output_stream_ref_define<void_output_source &>);
static_assert(!::fast_io::operations::defines::has_output_stream_ref_define<scalar_output_source &>);
static_assert(::fast_io::operations::defines::has_input_or_io_stream_ref_define<input_only_io_source &>);
static_assert(!::fast_io::operations::defines::has_output_or_io_stream_ref_define<input_only_io_source &>);
static_assert(!::fast_io::operations::defines::has_io_stream_ref_define<input_only_io_source &>);

inline void run_reference_materialization_test()
{
	int identity{};
	referenced_input_source source{{__builtin_addressof(identity)}};
	decltype(auto) proxy{::fast_io::operations::input_stream_ref(source)};
	static_assert(::std::same_as<decltype(proxy), input_proxy &>);
	assert(__builtin_addressof(proxy) == __builtin_addressof(source.proxy));
	assert(proxy.identity == __builtin_addressof(identity));
	input_only_io_source io_source{{__builtin_addressof(identity)}};
	decltype(auto) io_proxy{::fast_io::operations::input_stream_ref(io_source)};
	static_assert(::std::same_as<decltype(io_proxy), input_proxy &>);
	assert(__builtin_addressof(io_proxy) == __builtin_addressof(io_source.proxy));
	assert(io_proxy.identity == __builtin_addressof(identity));
	noncopyable_referenced_source borrowed;
	decltype(auto) borrowed_proxy = ::fast_io::operations::input_stream_ref(borrowed);
	assert(__builtin_addressof(borrowed_proxy) == __builtin_addressof(borrowed.proxy));
}

} // namespace stream_ref_protocol

namespace mutex_protocol
{

struct lock_state
{
	bool locked{};
	unsigned lock_calls{};
	unsigned unlock_calls{};
};

struct move_only_mutex_proxy
{
	lock_state *state{};

	move_only_mutex_proxy() = default;
	inline explicit constexpr move_only_mutex_proxy(lock_state *value) noexcept : state(value) {}
	move_only_mutex_proxy(move_only_mutex_proxy const &) = delete;
	move_only_mutex_proxy &operator=(move_only_mutex_proxy const &) = delete;
	inline constexpr move_only_mutex_proxy(move_only_mutex_proxy &&other) noexcept
		: state(::std::exchange(other.state, nullptr))
	{}

	inline void lock() noexcept
	{
		assert(state != nullptr && !state->locked);
		state->locked = true;
		++state->lock_calls;
	}

	inline void unlock() noexcept
	{
		assert(state != nullptr && state->locked);
		state->locked = false;
		++state->unlock_calls;
	}
};

struct unlocked_output
{
	using output_char_type = char;
	lock_state *state{};
	char *current{};
};

inline void write_all_overflow_define(
	unlocked_output output, char const *first, char const *last) noexcept
{
	assert(output.state != nullptr && output.state->locked);
	for (; first != last; ++first)
	{
		*output.current++ = *first;
	}
}

struct locked_output
{
	using output_char_type = char;
	lock_state *state{};
	char *current{};
};

inline constexpr locked_output output_stream_ref_define(locked_output output) noexcept
{
	return output;
}

inline constexpr move_only_mutex_proxy output_stream_mutex_ref_define(locked_output output) noexcept
{
	return move_only_mutex_proxy(output.state);
}

inline constexpr unlocked_output output_stream_unlocked_ref_define(locked_output output) noexcept
{
	return {output.state, output.current};
}

struct noncopyable_lvalue_mutex_output
{
	using output_char_type = char;
	move_only_mutex_proxy *mutex{};
};

inline constexpr move_only_mutex_proxy &
output_stream_mutex_ref_define(noncopyable_lvalue_mutex_output output) noexcept
{
	return *output.mutex;
}

inline constexpr unlocked_output
output_stream_unlocked_ref_define(noncopyable_lvalue_mutex_output) noexcept
{
	return {};
}

static_assert(::fast_io::operations::decay::defines::storable_mutex_ref_result<
	move_only_mutex_proxy>);
static_assert(!::fast_io::operations::decay::defines::storable_mutex_ref_result<
	move_only_mutex_proxy &>);
static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_output>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	noncopyable_lvalue_mutex_output>);

inline void run_move_only_mutex_test()
{
	lock_state state;
	char buffer[3]{};
	::fast_io::print(locked_output{__builtin_addressof(state), buffer}, "mtx");
	assert(!state.locked);
	assert(state.lock_calls == 1u && state.unlock_calls == 1u);
	assert(buffer[0] == 'm' && buffer[1] == 't' && buffer[2] == 'x');
}

} // namespace mutex_protocol

namespace pr_rsv_protocol
{

struct alias_value
{
	char value{};
};

struct category_source
{
	char lvalue{'L'};
	char rvalue{'R'};
};

inline alias_value print_alias_define(
	::fast_io::io_alias_t, category_source &source) noexcept(false)
{
	return {source.lvalue};
}

inline constexpr alias_value print_alias_define(
	::fast_io::io_alias_t, category_source &&source) noexcept
{
	return {source.rvalue};
}

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, alias_value>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, alias_value>, char *iter, alias_value &value) noexcept
{
	*iter = value.value;
	return iter + 1;
}

struct throwing_direct
{};

inline constexpr ::std::size_t
print_reserve_size(::fast_io::io_reserve_type_t<char, throwing_direct>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_direct>, char *, throwing_direct &)
{
	throw 42;
}

static_assert(::fast_io::pr_rsv_size<char, category_source &> == 1u);
static_assert(::fast_io::pr_rsv_size<char, category_source> == 1u);
static_assert(!noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<category_source &>())));
static_assert(noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<category_source &&>())));
static_assert(!noexcept(::fast_io::pr_rsv_to_iterator_unchecked(
	::std::declval<char *>(), ::std::declval<throwing_direct &>())));

inline void run_category_and_exception_tests()
{
	category_source source;
	char lvalue_buffer[1]{};
	auto lvalue_end{::fast_io::pr_rsv_to_c_array(lvalue_buffer, source)};
	assert(lvalue_end == lvalue_buffer + 1 && lvalue_buffer[0] == 'L');

	char rvalue_buffer[1]{};
	auto rvalue_end{::fast_io::pr_rsv_to_c_array(rvalue_buffer, category_source{})};
	assert(rvalue_end == rvalue_buffer + 1 && rvalue_buffer[0] == 'R');

	throwing_direct throwing;
	char throwing_buffer[1]{};
	bool caught{};
	try
	{
		(void)::fast_io::pr_rsv_to_c_array(throwing_buffer, throwing);
	}
	catch (int value)
	{
		caught = value == 42;
	}
	assert(caught);
}

} // namespace pr_rsv_protocol

int main()
{
	::stream_ref_protocol::run_reference_materialization_test();
	::mutex_protocol::run_move_only_mutex_test();
	::pr_rsv_protocol::run_category_and_exception_tests();
}
