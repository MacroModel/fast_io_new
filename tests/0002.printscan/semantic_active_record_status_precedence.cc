#include <cassert>
#include <cstddef>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct retained_leaf
{
	char value{'x'};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, retained_leaf>,
	retained_leaf const &) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, retained_leaf>, char *iter,
	retained_leaf const &value) noexcept
{
	*iter++ = value.value;
	return iter;
}

inline constexpr ::fast_io::reserve_scatters_size_result
print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, retained_leaf>,
	retained_leaf &) noexcept
{
	return {1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, retained_leaf>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	retained_leaf &value) noexcept
{
	*scatters = {__builtin_addressof(value.value), 1u};
	return {scatters + 1, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, retained_leaf>) noexcept
{
	return {};
}

struct capture_state
{
	::std::size_t status_calls{};
	::std::size_t scatter_calls{};
};

struct native_scatter_sink
{
	using output_char_type = char;
	capture_state *state{};
};

inline constexpr native_scatter_sink output_stream_ref_define(
	native_scatter_sink sink) noexcept
{
	return sink;
}

inline void scatter_write_all_overflow_define(
	native_scatter_sink sink,
	::fast_io::basic_io_scatter_t<char> const *,
	::std::size_t) noexcept
{
	++sink.state->scatter_calls;
}

template <bool line>
inline void status_print_define(
	native_scatter_sink sink,
	::fast_io::parameter<retained_leaf &> &) noexcept
{
	static_assert(!line);
	++sink.state->status_calls;
}

} // namespace

int main()
{
	retained_leaf value;
	auto record{::fast_io::mnp::pack(value)};
	using active_leaf =
		::fast_io::details::decay::print_semantic_named_member_forwarded_arg_t<
			char, retained_leaf>;
	static_assert(::std::same_as<
				  active_leaf, ::fast_io::parameter<retained_leaf &>>);
	static_assert(
		::fast_io::operations::decay::defines::has_status_print_define<
			false, native_scatter_sink, active_leaf>);
	static_assert(
		::fast_io::details::decay::
			print_runtime_scatter_plan_fast_entry_available_v<
				char, native_scatter_sink, active_leaf &>);

	// Pack expansion produces one plain active record which owns an exact
	// whole-record status CPO. Native scatter availability is only a strategy
	// capability and must not pre-empt that completion operation.
	capture_state state;
	::fast_io::io::print(
		native_scatter_sink{__builtin_addressof(state)}, record);
	assert(state.status_calls == 1u);
	assert(state.scatter_calls == 0u);
}
