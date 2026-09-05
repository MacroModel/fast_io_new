#include <cstddef>
#include <cstdlib>
#include <type_traits>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io_core.h>

namespace ordered_terminal_overflow_hooks
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct writer_failure
{};

struct hook_failure
{};

struct observations
{
	unsigned writers{};
	unsigned hooks{};
	unsigned destructions{};
	bool throw_writer{};
	bool throw_hook{};
};

inline observations *active_observations{};

/*
 * Each provider owns independent fixed storage but exposes capacity only after
 * reserve. This makes a full put area deterministic without depending on a
 * native string's allocation granularity or on the platform cost-policy gate.
 * Destruction never reads the unpublished logical end or any unwritten byte.
 */
template <unsigned family>
struct destination
{
	char storage[8193u];
	char *current{storage};
	char *end{storage};
	observations *observed{active_observations};

	destination() noexcept
	{
		require(observed != nullptr);
		storage[0u] = '\0';
	}

	destination(destination const &) = delete;
	destination &operator=(destination const &) = delete;
	destination &operator=(destination &&) = delete;

	destination(destination &&other) noexcept
		: current(storage + (other.current - other.storage)),
		  end(storage + (other.end - other.storage)), observed(other.observed)
	{
		// A result move copies only the published prefix and its terminator;
		// the rest of the private capacity intentionally remains uninitialized.
		auto const size{static_cast<::std::size_t>(current - storage)};
		for (::std::size_t index{}; index <= size; ++index)
		{
			storage[index] = other.storage[index];
		}
	}

	~destination()
	{
		++observed->destructions;
	}
};

template <unsigned family>
inline char *strlike_begin(
	::fast_io::io_strlike_type_t<char, destination<family>>, destination<family> &value) noexcept
{
	return value.storage;
}

template <unsigned family>
inline char *strlike_curr(
	::fast_io::io_strlike_type_t<char, destination<family>>, destination<family> &value) noexcept
{
	return value.current;
}

template <unsigned family>
inline char *strlike_end(
	::fast_io::io_strlike_type_t<char, destination<family>>, destination<family> &value) noexcept
{
	return value.end;
}

template <unsigned family>
inline void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, destination<family>>, destination<family> &value,
	char *current) noexcept
{
	value.current = current;
	*current = '\0';
}

template <unsigned family>
inline void strlike_reserve(
	::fast_io::io_strlike_type_t<char, destination<family>>, destination<family> &value,
	::std::size_t requested) noexcept
{
	require(requested <= 8192u);
	if (static_cast<::std::size_t>(value.end - value.storage) < requested)
	{
		value.end = value.storage + requested;
	}
}

template <unsigned family>
inline constexpr ::std::true_type strlike_deferred_obuffer_commit_safe(
	::fast_io::io_strlike_type_t<char, destination<family>>) noexcept
{
	return {};
}

template <unsigned family>
inline constexpr ::std::true_type concat_ordered_staging_adaptive_promotion_safe(
	::fast_io::io_strlike_type_t<char, destination<family>>) noexcept
{
	// Promotion preserves the copied prefix. The independent overflow hooks
	// below observe only call ordering, never the unpublished final cursor.
	return {};
}

template <unsigned family>
using adaptive_buffer = ::fast_io::details::decay::basic_concat_ordered_adaptive_buffer<
	char, destination<family>>;

template <unsigned family>
using output = ::fast_io::io_strlike_reference_wrapper<char, adaptive_buffer<family>>;

template <unsigned family>
inline void intercepted_newline(output<family> stream, char character)
{
	auto &buffer{*stream.ptr};
	auto &observed{*buffer.result_pointer->observed};
	++observed.hooks;
	require(observed.writers == 1u && character == '\n');
	if (observed.throw_hook)
	{
		throw hook_failure{};
	}
	// The hook owns its overflow transaction. Delegating capacity acquisition
	// here is valid; acquiring it before char_put would suppress this very hook.
	::fast_io::details::decay::basic_concat_ordered_adaptive_ensure<false>(buffer, 1u);
	*buffer.buffer_current++ = character;
}

inline void output_stream_char_put_overflow_define(output<0u> stream, char character)
{
	intercepted_newline(stream, character);
}

inline void write_all_overflow_define(output<1u> stream, char const *first, char const *last)
{
	// With no character hook, ordinary char_put synthesizes one scalar write.
	// This exact ADL overload must win over the generic string-wrapper provider.
	require(last - first == 1);
	intercepted_newline(stream, *first);
}

static_assert(::fast_io::operations::decay::defines::
	has_output_stream_char_put_overflow_define<output<0u>>);
static_assert(!::fast_io::operations::decay::defines::
	has_output_stream_char_put_overflow_define<output<1u>>);
static_assert(::fast_io::operations::decay::defines::
	has_write_all_overflow_define<output<1u>>);

struct terminal_leaf
{
	observations *observed;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, terminal_leaf>) noexcept
{
	return 1u;
}

inline char *print_reserve_define(::fast_io::io_reserve_type_t<char, terminal_leaf>,
	char *current, terminal_leaf &leaf)
{
	++leaf.observed->writers;
	require(leaf.observed->hooks == 0u);
	if (leaf.observed->throw_writer)
	{
		throw writer_failure{};
	}
	*current = 'z';
	return current + 1u;
}

template <unsigned family>
inline void verify(bool full_after_writer, bool throw_writer, bool throw_hook)
{
	observations observed{};
	observed.throw_writer = throw_writer;
	observed.throw_hook = throw_hook;
	active_observations = __builtin_addressof(observed);
	{
		adaptive_buffer<family> buffer;
		for (unsigned index{}; index != 512u; ++index)
		{
			*buffer.buffer_current++ = 'a';
		}
		::fast_io::details::decay::basic_concat_ordered_adaptive_promote_between_pairs(buffer);
		require(buffer.result_pointer != nullptr);
		require(buffer.buffer_end - buffer.buffer_begin == 4096);
		::std::size_t const prefix_size{full_after_writer ? 4095u : 4094u};
		while (static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin) != prefix_size)
		{
			*buffer.buffer_current++ = 'a';
		}
		output<family> stream{__builtin_addressof(buffer)};
		terminal_leaf leaf{__builtin_addressof(observed)};
		auto after_pair{[] { require(false); }};
		bool caught_writer{};
		bool caught_hook{};
		try
		{
			::fast_io::details::decay::basic_general_concat_ordered_emit<true, char>(
				stream, after_pair, leaf);
		}
		catch (writer_failure const &)
		{
			caught_writer = true;
		}
		catch (hook_failure const &)
		{
			caught_hook = true;
		}
		bool const expects_hook{full_after_writer && !throw_writer};
		bool const expects_hook_failure{expects_hook && throw_hook};
		require(observed.writers == 1u);
		require(observed.hooks == static_cast<unsigned>(expects_hook));
		require(caught_writer == throw_writer && caught_hook == expects_hook_failure);
		::std::size_t const committed{prefix_size + static_cast<::std::size_t>(!throw_writer) +
			static_cast<::std::size_t>(!throw_writer && !expects_hook_failure)};
		require(static_cast<::std::size_t>(buffer.buffer_current - buffer.buffer_begin) == committed);
		for (::std::size_t index{}; index != prefix_size; ++index)
		{
			require(buffer.buffer_begin[index] == 'a');
		}
		if (!throw_writer)
		{
			require(buffer.buffer_begin[prefix_size] == 'z');
			if (!expects_hook_failure)
			{
				require(buffer.buffer_begin[prefix_size + 1u] == '\n');
			}
		}
	}
	require(observed.destructions == 1u);
	active_observations = nullptr;
}

template <unsigned family>
inline void verify_family()
{
	verify<family>(true, false, false);
	verify<family>(true, false, true);
	// Even a potentially throwing overflow hook is not reached when the line
	// already fits. A failed source likewise prevents all newline observations.
	verify<family>(false, false, true);
	verify<family>(true, true, true);
}

} // namespace ordered_terminal_overflow_hooks

int main()
{
	::ordered_terminal_overflow_hooks::verify_family<0u>();
	::ordered_terminal_overflow_hooks::verify_family<1u>();
}
