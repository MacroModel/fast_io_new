#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace decofilter_ownership_transport
{

struct lifetime_state
{
	unsigned live{};
	unsigned copies{};
	unsigned moves{};
};

template <bool copyable>
struct tracked_codec;

template <>
struct tracked_codec<false>
{
	lifetime_state *state{};
	bool active{};

	inline explicit tracked_codec(lifetime_state &value) noexcept
		: state(__builtin_addressof(value)), active(true)
	{
		++state->live;
	}

	tracked_codec(tracked_codec const &) = delete;
	tracked_codec &operator=(tracked_codec const &) = delete;

	inline tracked_codec(tracked_codec &&other) noexcept
		: state(other.state), active(::std::exchange(other.active, false))
	{
		++state->moves;
	}

	tracked_codec &operator=(tracked_codec &&) = delete;

	inline ~tracked_codec()
	{
		if (active)
		{
			--state->live;
		}
	}

	template <::std::integral input_char_type, ::std::integral output_char_type>
	inline ::fast_io::deco_result<input_char_type, output_char_type> process_chars(
		input_char_type const *first, input_char_type const *last,
		output_char_type *out, output_char_type *out_last) noexcept
	{
		while (first != last && out != out_last)
		{
			*out++ = static_cast<output_char_type>(*first++);
		}
		return {first, out};
	}
};

template <>
struct tracked_codec<true>
{
	lifetime_state *state{};
	bool active{};

	inline explicit tracked_codec(lifetime_state &value) noexcept
		: state(__builtin_addressof(value)), active(true)
	{
		++state->live;
	}

	inline tracked_codec(tracked_codec const &other) noexcept
		: state(other.state), active(other.active)
	{
		++state->copies;
		if (active)
		{
			++state->live;
		}
	}

	tracked_codec &operator=(tracked_codec const &) = delete;

	inline tracked_codec(tracked_codec &&other) noexcept
		: state(other.state), active(::std::exchange(other.active, false))
	{
		++state->moves;
	}

	tracked_codec &operator=(tracked_codec &&) = delete;

	inline ~tracked_codec()
	{
		if (active)
		{
			--state->live;
		}
	}

	template <::std::integral input_char_type, ::std::integral output_char_type>
	inline ::fast_io::deco_result<input_char_type, output_char_type> process_chars(
		input_char_type const *first, input_char_type const *last,
		output_char_type *out, output_char_type *out_last) noexcept
	{
		while (first != last && out != out_last)
		{
			*out++ = static_cast<output_char_type>(*first++);
		}
		return {first, out};
	}
};

using move_only_decorator = ::fast_io::basic_bidirectional_decorator_adaptor<
	tracked_codec<false>, tracked_codec<false>>;
using copyable_decorator = ::fast_io::basic_bidirectional_decorator_adaptor<
	tracked_codec<true>, tracked_codec<true>>;

template <typename source_type, typename decorator_type>
concept can_add_io_decorator = requires(source_type &&source, decorator_type &&decorator) {
	::fast_io::operations::add_io_decos(
		::std::forward<source_type>(source), ::std::forward<decorator_type>(decorator));
};

static_assert(can_add_io_decorator<::fast_io::io_file &, move_only_decorator>);
static_assert(!can_add_io_decorator<::fast_io::io_file &, move_only_decorator &>);
static_assert(can_add_io_decorator<::fast_io::io_file &, copyable_decorator &>);

struct large_filter_proxy
{
	unsigned *ref_calls{};
	unsigned *rvalue_calls{};
	unsigned *legacy_calls{};
	unsigned char padding[192]{};

	large_filter_proxy() = default;
	large_filter_proxy(large_filter_proxy const &) = delete;
	large_filter_proxy &operator=(large_filter_proxy const &) = delete;
	large_filter_proxy(large_filter_proxy &&) = delete;
	large_filter_proxy &operator=(large_filter_proxy &&) = delete;
};

struct large_filter_source
{
	large_filter_proxy proxy;
};

inline large_filter_proxy &io_stream_deco_filter_ref_define(large_filter_source &source) noexcept
{
	++*source.proxy.ref_calls;
	return source.proxy;
}

struct rvalue_only_decorator
{
	rvalue_only_decorator() = default;
	rvalue_only_decorator(rvalue_only_decorator const &) = delete;
	rvalue_only_decorator &operator=(rvalue_only_decorator const &) = delete;
	rvalue_only_decorator(rvalue_only_decorator &&) = default;
};

inline void io_stream_add_deco_filter_define(
	large_filter_proxy &proxy, rvalue_only_decorator &&) noexcept
{
	++*proxy.rvalue_calls;
}

struct legacy_lvalue_decorator
{};

inline void io_stream_add_deco_filter_define(
	large_filter_proxy &proxy, legacy_lvalue_decorator &) noexcept
{
	++*proxy.legacy_calls;
}

struct unsupported_decorator
{};

static_assert(can_add_io_decorator<large_filter_source &, rvalue_only_decorator>);
static_assert(can_add_io_decorator<large_filter_source &, legacy_lvalue_decorator>);
static_assert(!can_add_io_decorator<large_filter_source &, unsupported_decorator>);

struct movable_projection
{
	unsigned *moves{};

	inline explicit movable_projection(unsigned &count) noexcept : moves(__builtin_addressof(count))
	{}
	movable_projection(movable_projection const &) = delete;
	movable_projection &operator=(movable_projection const &) = delete;
	inline movable_projection(movable_projection &&other) noexcept
		: moves(::std::exchange(other.moves, nullptr))
	{
		++*moves;
	}
};

struct decorators_xvalue_source
{
	movable_projection projection;
};

inline movable_projection &&output_decorators_ref_define(decorators_xvalue_source &source) noexcept
{
	return ::std::move(source.projection);
}

struct transcode_xvalue_source
{
	movable_projection projection;
};

inline movable_projection &&io_stream_transcode_deco_filter_ref_define(
	transcode_xvalue_source &&source) noexcept
{
	return ::std::move(source.projection);
}

inline void test_move_only_temporary_is_owned_by_io_file()
{
	lifetime_state state;
	{
		::fast_io::io_file file;
		::fast_io::operations::add_io_decos(
			file, move_only_decorator{tracked_codec<false>{state}, tracked_codec<false>{state}});
		// The entry parameter and every forwarding helper have returned. Two live codec tokens therefore prove that the
		// type-erased file owns decayed decorator storage rather than a reference to a destroyed helper parameter.
		assert(state.live == 2u);
		assert(state.copies == 0u);
	}
	assert(state.live == 0u);
}

inline void test_copyable_lvalue_has_one_entry_copy()
{
	lifetime_state state;
	{
		::fast_io::io_file file;
		{
			copyable_decorator decorator{tracked_codec<true>{state}, tracked_codec<true>{state}};
			assert(state.live == 2u);
			::fast_io::operations::add_io_decos(file, decorator);
			assert(state.live == 4u);
			assert(state.copies == 2u);
		}
		// Only the two codec objects stored behind the type-erased file remain alive.
		assert(state.live == 2u);
	}
	assert(state.live == 0u);
}

inline void test_exact_category_and_stable_reference_dispatch()
{
	unsigned ref_calls{};
	unsigned rvalue_calls{};
	unsigned legacy_calls{};
	large_filter_source source{};
	source.proxy.ref_calls = __builtin_addressof(ref_calls);
	source.proxy.rvalue_calls = __builtin_addressof(rvalue_calls);
	source.proxy.legacy_calls = __builtin_addressof(legacy_calls);

	decltype(auto) ref = ::fast_io::operations::io_stream_deco_filter_ref(source);
	static_assert(::std::same_as<decltype(ref), large_filter_proxy &>);
	assert(__builtin_addressof(ref) == __builtin_addressof(source.proxy));

	::fast_io::operations::add_io_decos(source, rvalue_only_decorator{});
	::fast_io::operations::add_io_decos(source, legacy_lvalue_decorator{});
	assert(ref_calls == 3u);
	assert(rvalue_calls == 1u);
	assert(legacy_calls == 1u);
}

inline void test_xvalue_reference_results_are_materialized_once()
{
	unsigned decorator_moves{};
	decorators_xvalue_source decorator_source{movable_projection{decorator_moves}};
	decltype(auto) decorator_owner =
		::fast_io::operations::refs::output_decorators_ref(decorator_source);
	static_assert(::std::same_as<decltype(decorator_owner), movable_projection>);
	assert(decorator_moves == 1u);

	unsigned transcode_moves{};
	transcode_xvalue_source transcode_source{movable_projection{transcode_moves}};
	decltype(auto) transcode_owner =
		::fast_io::operations::io_stream_transcode_deco_filter_ref(::std::move(transcode_source));
	static_assert(::std::same_as<decltype(transcode_owner), movable_projection>);
	assert(transcode_moves == 1u);
}

} // namespace decofilter_ownership_transport

int main()
{
	using namespace ::decofilter_ownership_transport;
	test_move_only_temporary_is_owned_by_io_file();
	test_copyable_lvalue_has_one_entry_copy();
	test_exact_category_and_stable_reference_dispatch();
	test_xvalue_reference_results_are_materialized_once();
}
