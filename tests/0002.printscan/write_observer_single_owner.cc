#include <array>
#include <cassert>
#include <cstddef>
#include <list>
#include <type_traits>

#include <fast_io_core.h>

namespace write_observer_single_owner
{

struct operation_state
{
	::std::size_t normalizations{};
	::std::size_t unlocked_materializations{};
	::std::size_t write_calls{};
	::std::size_t pwrite_calls{};
	::std::size_t copies{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	bool locked{};
};

// Deleting both copy and move construction makes hidden observer propagation a compile-time failure. Direct return
// and local initialization remain valid because same-type prvalues are guaranteed-elided since C++17.
struct immovable_observer
{
	using output_char_type = char;
	operation_state *state{};

	inline explicit constexpr immovable_observer(operation_state *value) noexcept : state(value)
	{}
	immovable_observer(immovable_observer const &) = delete;
	immovable_observer(immovable_observer &&) = delete;
	immovable_observer &operator=(immovable_observer const &) = delete;
	immovable_observer &operator=(immovable_observer &&) = delete;
};

struct immovable_output
{
	operation_state *state{};
};

inline immovable_observer output_stream_ref_define(immovable_output &output) noexcept
{
	++output.state->normalizations;
	return immovable_observer{output.state};
}

inline char const *write_some_overflow_define(immovable_observer &observer, char const *first,
											  char const *last) noexcept
{
	++observer.state->write_calls;
	return first == last ? last : first + 1;
}

inline char const *pwrite_some_overflow_define(immovable_observer &observer, char const *first,
											   char const *last, ::fast_io::intfpos_t) noexcept
{
	++observer.state->pwrite_calls;
	return first == last ? last : first + 1;
}

struct reference_observer
{
	using output_char_type = char;
	operation_state *state{};

	inline explicit constexpr reference_observer(operation_state *value) noexcept : state(value)
	{}
	inline reference_observer(reference_observer const &other) noexcept : state(other.state)
	{
		++state->copies;
	}
};

struct reference_output
{
	reference_observer observer;
};

inline reference_observer &output_stream_ref_define(reference_output &output) noexcept
{
	++output.observer.state->normalizations;
	return output.observer;
}

inline char const *write_some_overflow_define(reference_observer &observer, char const *first,
											  char const *last) noexcept
{
	++observer.state->write_calls;
	return first == last ? last : first + 1;
}

struct mutex_proxy
{
	operation_state *state{};

	inline void lock() const noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

struct locked_observer
{
	using output_char_type = char;
	operation_state *state{};

	inline explicit constexpr locked_observer(operation_state *value) noexcept : state(value)
	{}
	locked_observer(locked_observer const &) = delete;
	locked_observer(locked_observer &&) = delete;
};

struct locked_output
{
	operation_state *state{};
};

inline locked_observer output_stream_ref_define(locked_output &output) noexcept
{
	++output.state->normalizations;
	return locked_observer{output.state};
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(locked_observer &observer) noexcept
{
	return {observer.state};
}

inline immovable_observer output_stream_unlocked_ref_define(locked_observer &observer) noexcept
{
	++observer.state->unlocked_materializations;
	return immovable_observer{observer.state};
}

static_assert(!::std::is_copy_constructible_v<immovable_observer>);
static_assert(!::std::is_move_constructible_v<immovable_observer>);
static_assert(!::std::is_copy_constructible_v<locked_observer>);
static_assert(!::std::is_move_constructible_v<locked_observer>);

} // namespace write_observer_single_owner

int main()
{
	using namespace write_observer_single_owner;

	operation_state state;
	immovable_output output{__builtin_addressof(state)};
	char const text[]{'a', 'b', 'c', 'd'};
	auto const *bytes_first{reinterpret_cast<::std::byte const *>(text)};
	auto const *bytes_last{bytes_first + sizeof(text)};
	::fast_io::basic_io_scatter_t<char> typed_scatters[]{{text, 2u}, {text + 2, 2u}};
	::fast_io::io_scatter_t byte_scatters[]{{bytes_first, 2u}, {bytes_first + 2, 2u}};

	// Every public call must perform exactly one normalization regardless of how many fallback chunks it emits. The
	// immovable result proves that all scalar, byte, scatter, positional, range, and span edges borrow that one owner.
	auto once = [&](auto &&operation) {
		auto const before{state.normalizations};
		operation();
		assert(state.normalizations == before + 1u);
	};

	once([&] { (void)::fast_io::operations::write_some(output, text, text + 4); });
	once([&] { ::fast_io::operations::write_all(output, text, text + 4); });
	once([&] { (void)::fast_io::operations::write_some_bytes(output, bytes_first, bytes_last); });
	once([&] { ::fast_io::operations::write_all_bytes(output, bytes_first, bytes_last); });
	once([&] { (void)::fast_io::operations::scatter_write_some(output, typed_scatters, 2u); });
	once([&] { ::fast_io::operations::scatter_write_all(output, typed_scatters, 2u); });
	once([&] { (void)::fast_io::operations::scatter_write_some_bytes(output, byte_scatters, 2u); });
	once([&] { ::fast_io::operations::scatter_write_all_bytes(output, byte_scatters, 2u); });
	once([&] { (void)::fast_io::operations::pwrite_some(output, text, text + 4, 7); });
	once([&] { ::fast_io::operations::pwrite_all(output, text, text + 4, 7); });
	once([&] { (void)::fast_io::operations::pwrite_some_bytes(output, bytes_first, bytes_last, 7); });
	once([&] { ::fast_io::operations::pwrite_all_bytes(output, bytes_first, bytes_last, 7); });
	once([&] { (void)::fast_io::operations::scatter_pwrite_some(output, typed_scatters, 2u, 7); });
	once([&] { ::fast_io::operations::scatter_pwrite_all(output, typed_scatters, 2u, 7); });
	once([&] { (void)::fast_io::operations::scatter_pwrite_some_bytes(output, byte_scatters, 2u, 7); });
	once([&] { ::fast_io::operations::scatter_pwrite_all_bytes(output, byte_scatters, 2u, 7); });
	once([&] { ::fast_io::operations::char_put(output, 'x'); });

	::std::list<char> noncontiguous_range{'r', 'a', 'n', 'g', 'e'};
	once([&] { ::fast_io::operations::write_all_range(output, noncontiguous_range); });

	::fast_io::containers::span<char const> text_span{text, 4u};
	::fast_io::containers::span<::std::byte const> byte_span{bytes_first, sizeof(text)};
	::fast_io::containers::span<::fast_io::basic_io_scatter_t<char> const> typed_scatter_span{typed_scatters, 2u};
	::fast_io::containers::span<::fast_io::io_scatter_t const> byte_scatter_span{byte_scatters, 2u};
	once([&] { (void)::fast_io::operations::write_some_span(output, text_span); });
	once([&] { ::fast_io::operations::write_all_span(output, text_span); });
	once([&] { (void)::fast_io::operations::write_some_bytes_span(output, byte_span); });
	once([&] { ::fast_io::operations::write_all_bytes_span(output, byte_span); });
	once([&] { (void)::fast_io::operations::scatter_write_some_span(output, typed_scatter_span); });
	once([&] { ::fast_io::operations::scatter_write_all_span(output, typed_scatter_span); });
	once([&] { (void)::fast_io::operations::scatter_write_some_bytes_span(output, byte_scatter_span); });
	once([&] { ::fast_io::operations::scatter_write_all_bytes_span(output, byte_scatter_span); });
	once([&] { (void)::fast_io::operations::pwrite_some_span(output, text_span, 11); });
	once([&] { ::fast_io::operations::pwrite_all_span(output, text_span, 11); });
	once([&] { (void)::fast_io::operations::pwrite_some_bytes_span(output, byte_span, 11); });
	once([&] { ::fast_io::operations::pwrite_all_bytes_span(output, byte_span, 11); });
	once([&] { (void)::fast_io::operations::scatter_pwrite_some_span(output, typed_scatter_span, 11); });
	once([&] { ::fast_io::operations::scatter_pwrite_all_span(output, typed_scatter_span, 11); });
	once([&] { (void)::fast_io::operations::scatter_pwrite_some_bytes_span(output, byte_scatter_span, 11); });
	once([&] { ::fast_io::operations::scatter_pwrite_all_bytes_span(output, byte_scatter_span, 11); });

	// Non-trivial lvalue observers are intentionally outside the ABI-direct value class. Their reference identity must
	// survive both normalization and primitive dispatch without invoking the user-visible copy constructor.
	operation_state reference_state;
	reference_output reference{reference_observer{__builtin_addressof(reference_state)}};
	::fast_io::operations::write_all(reference, text, text + 4);
	assert(reference_state.normalizations == 1u);
	assert(reference_state.copies == 0u);
	assert(reference_state.write_calls == 4u);

	// Mutex unwrapping has a second one-owner boundary inside the guard. An immovable unlocked prvalue proves that it is
	// named once and borrowed for every retry while exactly one lock protects the logical write.
	operation_state locked_state;
	locked_output locked{__builtin_addressof(locked_state)};
	::fast_io::operations::write_all(locked, text, text + 4);
	assert(locked_state.normalizations == 1u);
	assert(locked_state.unlocked_materializations == 1u);
	assert(locked_state.write_calls == 4u);
	assert(locked_state.locks == 1u && locked_state.unlocks == 1u);
	assert(!locked_state.locked);
}
