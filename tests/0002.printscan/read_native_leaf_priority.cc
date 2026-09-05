#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <initializer_list>
#include <source_location>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io_core.h>

namespace read_native_leaf_priority
{

inline unsigned case_identity{};

inline void require(bool condition, ::std::source_location location = ::std::source_location::current()) noexcept
{
	if (!condition)
	{
		::std::fprintf(stderr, "read native leaf case=%u line=%u\n", case_identity, location.line());
		::std::abort();
	}
}

struct input_failure
{};

/*
 * Each mask independently admits typed some/all and byte some/all native
 * leaves. Scatter alternatives are always present and produce a distinct
 * byte, making priority observable rather than merely counting successful
 * reads. The noncopyable observer keeps both cursor and identity inline.
 */
template <unsigned mask, bool buffered>
struct input
{
	using input_char_type = char;
	char storage[3u]{'b', 'b', 'b'};
	::std::size_t capacity{};
	::std::size_t used{};
	unsigned calls{};
	bool fail{};
	input *identity{this};
	input() = default;
	input(input const &) = delete;
	input &operator=(input const &) = delete;
	void observe()
	{
		require(identity == this);
		++calls;
		if (fail)
		{
			throw input_failure{};
		}
	}
};

template <unsigned mask, bool buffered>
inline input<mask, buffered> &input_stream_ref_define(input<mask, buffered> &stream) noexcept
{
	return stream;
}

template <unsigned mask>
inline char const *ibuffer_begin(input<mask, true> &stream) noexcept
{
	return stream.storage;
}
template <unsigned mask>
inline char const *ibuffer_curr(input<mask, true> &stream) noexcept
{
	return stream.storage + stream.used;
}
template <unsigned mask>
inline char const *ibuffer_end(input<mask, true> &stream) noexcept
{
	return stream.storage + stream.capacity;
}
template <unsigned mask>
inline void ibuffer_set_curr(input<mask, true> &stream, char const *current) noexcept
{
	stream.used = static_cast<::std::size_t>(current - stream.storage);
}

template <unsigned mask>
inline bool ibuffer_underflow(input<mask, true> &) noexcept
{
	// A coherent get area includes the bool refill protocol. The selected bulk
	// CPOs own every tested miss; an unexpected refill is a separate failure.
	::std::abort();
}

template <unsigned mask, bool buffered>
	requires((mask & 1u) != 0u)
inline char *read_some_underflow_define(input<mask, buffered> &stream, char *first, char *last)
{
	stream.observe();
	if (first != last)
	{
		*first++ = 'x';
	}
	return first;
}

template <unsigned mask, bool buffered>
	requires((mask & 2u) != 0u)
inline void read_all_underflow_define(input<mask, buffered> &stream, char *first, char *last)
{
	stream.observe();
	for (; first != last; ++first)
	{
		*first = 'x';
	}
}

template <unsigned mask, bool buffered>
	requires((mask & 4u) != 0u)
inline ::std::byte *read_some_bytes_underflow_define(input<mask, buffered> &stream,
													 ::std::byte *first, ::std::byte *last)
{
	stream.observe();
	if (first != last)
	{
		*first++ = ::std::byte{'x'};
	}
	return first;
}

template <unsigned mask, bool buffered>
	requires((mask & 8u) != 0u)
inline void read_all_bytes_underflow_define(input<mask, buffered> &stream,
											::std::byte *first, ::std::byte *last)
{
	stream.observe();
	for (; first != last; ++first)
	{
		*first = ::std::byte{'x'};
	}
}

template <typename scatter>
inline void fill_scatter(scatter const *scatters, ::std::size_t count) noexcept
{
	for (::std::size_t index{}; index != count; ++index)
	{
		auto *first{const_cast<char *>(static_cast<char const *>(scatters[index].base))};
		for (::std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			first[offset] = 'y';
		}
	}
}

template <unsigned mask, bool buffered>
inline ::fast_io::io_scatter_status_t scatter_read_some_underflow_define(input<mask, buffered> &stream,
																		 ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	stream.observe();
	fill_scatter(scatters, count);
	return {count, 0u};
}
template <unsigned mask, bool buffered>
inline void scatter_read_all_underflow_define(input<mask, buffered> &stream,
											  ::fast_io::basic_io_scatter_t<char> const *scatters, ::std::size_t count)
{
	stream.observe();
	fill_scatter(scatters, count);
}
template <unsigned mask, bool buffered>
inline ::fast_io::io_scatter_status_t scatter_read_some_bytes_underflow_define(input<mask, buffered> &stream,
																			   ::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	stream.observe();
	fill_scatter(scatters, count);
	return {count, 0u};
}
template <unsigned mask, bool buffered>
inline void scatter_read_all_bytes_underflow_define(input<mask, buffered> &stream,
													::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	stream.observe();
	fill_scatter(scatters, count);
}

template <unsigned mask, bool buffered, bool bytes, bool all>
inline void verify(::std::size_t capacity, bool empty, bool fail)
{
	// Coherent cursors and the bool refill hook must jointly admit the fixture;
	// otherwise an intended buffer-hit test silently exercises an unbuffered CPO.
	static_assert(::fast_io::operations::decay::defines::has_ibuffer_basic_operations<input<mask, buffered>> == buffered);
	case_identity = mask | (static_cast<unsigned>(buffered) << 4u) |
					(static_cast<unsigned>(bytes) << 5u) | (static_cast<unsigned>(all) << 6u) |
					(static_cast<unsigned>(capacity) << 7u) | (static_cast<unsigned>(empty) << 9u) |
					(static_cast<unsigned>(fail) << 10u);
	input<mask, buffered> stream;
	stream.capacity = capacity;
	stream.fail = fail;
	char output[3u]{};
	::std::size_t const count{empty ? 0u : 3u};
	bool const hit{buffered && count <= capacity};
	constexpr bool native{(mask & (bytes ? (all ? 8u : 4u) : (all ? 2u : 1u))) != 0u};
	::std::size_t const produced{hit || all || !native ? count : (empty ? 0u : 1u)};
	bool caught{};
	try
	{
		if constexpr (bytes)
		{
			auto *first{empty ? nullptr : reinterpret_cast<::std::byte *>(output)};
			auto *last{empty ? first : first + count};
			if constexpr (all)
			{
				::fast_io::operations::read_all_bytes(stream, first, last);
			}
			else
			{
				auto *result{::fast_io::operations::read_some_bytes(stream, first, last)};
				require(result == (empty ? first : first + produced));
			}
		}
		else
		{
			auto *first{empty ? nullptr : output};
			auto *last{empty ? first : first + count};
			if constexpr (all)
			{
				::fast_io::operations::read_all(stream, first, last);
			}
			else
			{
				auto *result{::fast_io::operations::read_some(stream, first, last)};
				require(result == (empty ? first : first + produced));
			}
		}
	}
	catch (input_failure const &)
	{
		caught = true;
	}
	// A failed primitive must not publish a byte or consume the buffered cursor.
	// An empty unbuffered interval still observes its selected native/scatter CPO.
	require(caught == (!hit && fail));
	require(stream.calls == (hit ? 0u : 1u));
	require(stream.used == (hit ? count : 0u));
	for (::std::size_t index{}; index != 3u; ++index)
	{
		char const expected{caught || index >= produced ? '\0' : hit  ? 'b'
															 : native ? 'x'
																	  : 'y'};
		require(output[index] == expected);
	}
}

template <unsigned mask>
inline void verify_mask()
{
	for (auto capacity : {0u, 2u, 3u})
	{
		for (bool empty : {false, true})
		{
			for (bool fail : {false, true})
			{
				verify<mask, false, false, false>(capacity, empty, fail);
				verify<mask, false, false, true>(capacity, empty, fail);
				verify<mask, false, true, false>(capacity, empty, fail);
				verify<mask, false, true, true>(capacity, empty, fail);
				verify<mask, true, false, false>(capacity, empty, fail);
				verify<mask, true, false, true>(capacity, empty, fail);
				verify<mask, true, true, false>(capacity, empty, fail);
				verify<mask, true, true, true>(capacity, empty, fail);
			}
		}
	}
	if constexpr (mask != 15u)
	{
		verify_mask<mask + 1u>();
	}
}

} // namespace read_native_leaf_priority

int main()
{
	::read_native_leaf_priority::verify_mask<0u>();
}
