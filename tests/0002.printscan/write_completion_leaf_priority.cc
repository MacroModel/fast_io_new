#include <cstddef>
#include <cstdlib>
#include <initializer_list>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io_core.h>

namespace write_completion_leaf_priority
{

inline void require(bool value) noexcept
{
	if (!value)
	{
		::std::abort();
	}
}

struct completion_failure
{};

/*
 * A typed all-completion is present in every fixture. The four independent
 * mask bits add native byte all, scatter-all, some, and scatter-some CPOs.
 * This Cartesian product proves that exposing a direct completion leaf must
 * preserve the complete byte-domain priority graph, not merely its byte-all
 * alternative. The observer owns mutable cursor state and cannot be copied.
 */
template <unsigned mask, bool buffered>
struct output
{
	using output_char_type = char;
	char storage[16u]{};
	::std::size_t used{};
	::std::size_t capacity{};
	unsigned calls{};
	unsigned selected{};
	bool fail{};
	output *identity{this};

	output() = default;
	output(output const &) = delete;
	output &operator=(output const &) = delete;

	void observe(unsigned operation)
	{
		require(identity == this);
		require(selected == 0u || selected == operation);
		selected = operation;
		++calls;
		if (fail)
		{
			throw completion_failure{};
		}
	}

	void append(char value) noexcept
	{
		require(used < sizeof(storage));
		storage[used++] = value;
		capacity = sizeof(storage);
	}
};

template <unsigned mask, bool buffered>
inline output<mask, buffered> &output_stream_ref_define(output<mask, buffered> &stream) noexcept
{
	return stream;
}

template <unsigned mask>
inline char *obuffer_begin(output<mask, true> &stream) noexcept
{
	return stream.storage;
}

template <unsigned mask>
inline char *obuffer_curr(output<mask, true> &stream) noexcept
{
	return stream.storage + stream.used;
}

template <unsigned mask>
inline char *obuffer_end(output<mask, true> &stream) noexcept
{
	return stream.storage + stream.capacity;
}

template <unsigned mask>
inline void obuffer_set_curr(output<mask, true> &stream, char *current) noexcept
{
	stream.used = static_cast<::std::size_t>(current - stream.storage);
}

template <unsigned mask, bool buffered>
inline void write_all_overflow_define(output<mask, buffered> &stream, char const *first, char const *last)
{
	stream.observe(1u);
	for (; first != last; ++first)
	{
		stream.append(*first);
	}
}

template <unsigned mask, bool buffered>
	requires((mask & 1u) != 0u)
inline void write_all_bytes_overflow_define(output<mask, buffered> &stream,
											::std::byte const *first, ::std::byte const *last)
{
	stream.observe(2u);
	for (; first != last; ++first)
	{
		stream.append(static_cast<char>(::std::to_integer<unsigned char>(*first)));
	}
}

template <unsigned mask, bool buffered>
	requires((mask & 2u) != 0u)
inline void scatter_write_all_bytes_overflow_define(output<mask, buffered> &stream,
													::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	stream.observe(3u);
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const *first{static_cast<char const *>(scatters[index].base)};
		for (::std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			stream.append(first[offset]);
		}
	}
}

template <unsigned mask, bool buffered>
	requires((mask & 4u) != 0u)
inline ::std::byte const *write_some_bytes_overflow_define(output<mask, buffered> &stream,
														   ::std::byte const *first, ::std::byte const *last)
{
	stream.observe(4u);
	// Publish one byte per partial call. Buffered retries may then complete the
	// remainder through the newly available put area; unbuffered retries cannot.
	if (first != last)
	{
		stream.append(static_cast<char>(::std::to_integer<unsigned char>(*first++)));
	}
	return first;
}

template <unsigned mask, bool buffered>
	requires((mask & 8u) != 0u)
inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	output<mask, buffered> &stream, ::fast_io::io_scatter_t const *scatters, ::std::size_t count)
{
	stream.observe(5u);
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const *first{static_cast<char const *>(scatters[index].base)};
		for (::std::size_t offset{}; offset != scatters[index].len; ++offset)
		{
			stream.append(first[offset]);
		}
	}
	return {count, 0u};
}

template <unsigned mask, bool buffered, bool bytes>
inline void verify(::std::size_t capacity, bool empty, bool fail)
{
	output<mask, buffered> stream;
	stream.capacity = capacity;
	stream.fail = fail;
	char const typed_source[]{'a', 'b', 'c'};
	::std::byte const byte_source[]{::std::byte{'a'}, ::std::byte{'b'}, ::std::byte{'c'}};
	::std::size_t const count{empty ? 0u : 3u};
	constexpr unsigned preferred{
		!bytes ? 1u : (mask & 1u) ? 2u
				  : (mask & 2u)   ? 3u
				  : (mask & 4u)   ? 4u
				  : (mask & 8u)   ? 5u
								  : 1u};
	bool const hit{buffered && count <= capacity};
	// Scatter-some synthesis has a zero-iteration empty loop. Other selected
	// completion leaves are still observed for an unbuffered empty request.
	bool const invoked{!hit && !(empty && preferred == 5u)};
	bool const expected_failure{invoked && fail};
	bool caught{};
	try
	{
		if constexpr (bytes)
		{
			auto const *first{empty ? nullptr : byte_source};
			::fast_io::operations::write_all_bytes(stream, first, empty ? first : first + 3u);
		}
		else
		{
			auto const *first{empty ? nullptr : typed_source};
			::fast_io::operations::write_all(stream, first, empty ? first : first + 3u);
		}
	}
	catch (completion_failure const &)
	{
		caught = true;
	}
	unsigned const expected_calls{!invoked ? 0u : (!buffered && !fail && !empty && preferred == 4u) ? 3u
																									: 1u};
	require(caught == expected_failure);
	require(stream.calls == expected_calls && stream.selected == (invoked ? preferred : 0u));
	require(stream.used == (expected_failure ? 0u : count));
	for (::std::size_t index{}; index != stream.used; ++index)
	{
		require(stream.storage[index] == typed_source[index]);
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
				verify<mask, false, false>(capacity, empty, fail);
				verify<mask, false, true>(capacity, empty, fail);
				verify<mask, true, false>(capacity, empty, fail);
				verify<mask, true, true>(capacity, empty, fail);
			}
		}
	}
	if constexpr (mask != 15u)
	{
		verify_mask<mask + 1u>();
	}
}

} // namespace write_completion_leaf_priority

int main()
{
	::write_completion_leaf_priority::verify_mask<0u>();
}
