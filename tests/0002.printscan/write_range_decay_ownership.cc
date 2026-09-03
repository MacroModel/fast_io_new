#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <type_traits>

#include <fast_io_core.h>

namespace write_range_decay_ownership
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct iterator_state
{
	::std::size_t begin_calls{};
	::std::size_t move_constructions{};
	::std::size_t move_assignments{};
	::std::size_t dereferences{};
	::std::size_t increments{};
};

struct distinct_sentinel
{
	char const *last{};
};

class move_only_iterator
{
	char const *current_{};
	iterator_state *state_{};

public:
	using value_type = char;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;
	using iterator_category = ::std::input_iterator_tag;

	constexpr move_only_iterator() noexcept = default;

	inline constexpr move_only_iterator(char const *current, iterator_state *state) noexcept
		: current_(current), state_(state)
	{}

	move_only_iterator(move_only_iterator const &) = delete;
	move_only_iterator &operator=(move_only_iterator const &) = delete;

	inline constexpr move_only_iterator(move_only_iterator &&other) noexcept
		: current_(other.current_), state_(other.state_)
	{
		other.current_ = nullptr;
		other.state_ = nullptr;
		if (state_ != nullptr)
		{
			++state_->move_constructions;
		}
	}

	inline constexpr move_only_iterator &operator=(move_only_iterator &&other) noexcept
	{
		current_ = other.current_;
		state_ = other.state_;
		other.current_ = nullptr;
		other.state_ = nullptr;
		if (state_ != nullptr)
		{
			++state_->move_assignments;
		}
		return *this;
	}

	inline constexpr char const &operator*() const noexcept
	{
		++state_->dereferences;
		return *current_;
	}

	inline constexpr move_only_iterator &operator++() noexcept
	{
		++current_;
		++state_->increments;
		return *this;
	}

	inline constexpr void operator++(int) noexcept
	{
		++*this;
	}

	friend inline constexpr bool operator==(move_only_iterator const &iter, distinct_sentinel sentinel) noexcept
	{
		return iter.current_ == sentinel.last;
	}

	friend inline constexpr bool operator==(distinct_sentinel sentinel, move_only_iterator const &iter) noexcept
	{
		return iter == sentinel;
	}
};

static_assert(::std::input_iterator<move_only_iterator>);
static_assert(::std::sentinel_for<distinct_sentinel, move_only_iterator>);
static_assert(!::std::copy_constructible<move_only_iterator>);

struct move_only_range
{
	char const *first{};
	char const *last{};
	iterator_state *state{};

	inline constexpr move_only_iterator begin() const noexcept
	{
		++state->begin_calls;
		return {first, state};
	}

	inline constexpr distinct_sentinel end() const noexcept
	{
		return {last};
	}
};

static_assert(::std::ranges::input_range<move_only_range const>);
static_assert(!::std::ranges::contiguous_range<move_only_range const>);

template <::std::size_t size>
struct record_value
{
	unsigned char bytes[size];
};

template <typename value_type>
struct record_sentinel
{
	value_type const *last{};
};

template <typename value_type_arg>
class record_move_only_iterator
{
	value_type_arg const *current_{};
	iterator_state *state_{};

public:
	using value_type = value_type_arg;
	using difference_type = ::std::ptrdiff_t;
	using iterator_concept = ::std::input_iterator_tag;
	using iterator_category = ::std::input_iterator_tag;

	constexpr record_move_only_iterator() noexcept = default;

	inline constexpr record_move_only_iterator(value_type const *current, iterator_state *state) noexcept
		: current_(current), state_(state)
	{}

	record_move_only_iterator(record_move_only_iterator const &) = delete;
	record_move_only_iterator &operator=(record_move_only_iterator const &) = delete;

	inline constexpr record_move_only_iterator(record_move_only_iterator &&other) noexcept
		: current_(other.current_), state_(other.state_)
	{
		other.current_ = nullptr;
		other.state_ = nullptr;
		if (state_ != nullptr)
		{
			++state_->move_constructions;
		}
	}

	inline constexpr record_move_only_iterator &operator=(record_move_only_iterator &&other) noexcept
	{
		current_ = other.current_;
		state_ = other.state_;
		other.current_ = nullptr;
		other.state_ = nullptr;
		if (state_ != nullptr)
		{
			++state_->move_assignments;
		}
		return *this;
	}

	inline constexpr value_type const &operator*() const noexcept
	{
		++state_->dereferences;
		return *current_;
	}

	inline constexpr record_move_only_iterator &operator++() noexcept
	{
		++current_;
		++state_->increments;
		return *this;
	}

	inline constexpr void operator++(int) noexcept
	{
		++*this;
	}

	friend inline constexpr bool operator==(record_move_only_iterator const &iter,
											record_sentinel<value_type> sentinel) noexcept
	{
		return iter.current_ == sentinel.last;
	}

	friend inline constexpr bool operator==(record_sentinel<value_type> sentinel,
											record_move_only_iterator const &iter) noexcept
	{
		return iter == sentinel;
	}
};

template <typename value_type>
struct record_move_only_range
{
	value_type const *first{};
	value_type const *last{};
	iterator_state *state{};

	inline constexpr record_move_only_iterator<value_type> begin() const noexcept
	{
		++state->begin_calls;
		return {first, state};
	}

	inline constexpr record_sentinel<value_type> end() const noexcept
	{
		return {last};
	}
};

using medium_value = record_value<64>;
using oversized_value = record_value<640>;
using medium_move_only_iterator = record_move_only_iterator<medium_value>;
using oversized_move_only_iterator = record_move_only_iterator<oversized_value>;
using medium_move_only_range = record_move_only_range<medium_value>;
using oversized_move_only_range = record_move_only_range<oversized_value>;

static_assert(::std::is_trivially_copyable_v<medium_value>);
static_assert(::std::is_trivially_copyable_v<oversized_value>);
static_assert(sizeof(medium_value) <= 512u);
static_assert(sizeof(oversized_value) > 512u);
static_assert(::std::input_iterator<medium_move_only_iterator>);
static_assert(::std::input_iterator<oversized_move_only_iterator>);
static_assert(!::std::copy_constructible<medium_move_only_iterator>);
static_assert(!::std::copy_constructible<oversized_move_only_iterator>);
static_assert(::std::ranges::input_range<medium_move_only_range const>);
static_assert(::std::ranges::input_range<oversized_move_only_range const>);
static_assert(!::std::ranges::contiguous_range<medium_move_only_range const>);
static_assert(!::std::ranges::contiguous_range<oversized_move_only_range const>);

struct output_state
{
	char buffer[2048]{};
	::std::size_t size{};
	::std::size_t normalizations{};
	::std::size_t unlocked_materializations{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	::std::size_t unlocked_write_calls{};
	::std::size_t locked_write_calls{};
	bool locked{};
};

struct locked_output
{
	output_state *state{};
};

struct locked_output_ref
{
	using output_char_type = char;
	output_state *state{};
};

struct unlocked_output_ref
{
	using output_char_type = char;
	output_state *state{};
};

struct byte_locked_output
{
	output_state *state{};
};

struct byte_locked_output_ref
{
	using output_char_type = char16_t;
	output_state *state{};
};

struct byte_unlocked_output_ref
{
	using output_char_type = char16_t;
	output_state *state{};
};

struct mutex_proxy
{
	output_state *state{};

	inline void lock() const noexcept
	{
		require(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() const noexcept
	{
		require(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

inline locked_output_ref output_stream_ref_define(locked_output &output) noexcept
{
	++output.state->normalizations;
	return {output.state};
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(locked_output_ref &output) noexcept
{
	return {output.state};
}

inline unlocked_output_ref output_stream_unlocked_ref_define(locked_output_ref &output) noexcept
{
	++output.state->unlocked_materializations;
	return {output.state};
}

inline byte_locked_output_ref output_stream_ref_define(byte_locked_output &output) noexcept
{
	++output.state->normalizations;
	return {output.state};
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(byte_locked_output_ref &output) noexcept
{
	return {output.state};
}

inline byte_unlocked_output_ref output_stream_unlocked_ref_define(byte_locked_output_ref &output) noexcept
{
	++output.state->unlocked_materializations;
	return {output.state};
}

// Range admission is checked on the locked observer before mutex unwrapping.  Reaching this overload would prove
// that the dispatcher had violated the complete mutex protocol instead of using the unlocked observer.
inline void write_all_overflow_define(locked_output_ref &output, char const *, char const *) noexcept
{
	++output.state->locked_write_calls;
	require(false);
}

inline void write_all_overflow_define(unlocked_output_ref &output, char const *first, char const *last) noexcept
{
	require(output.state->locked);
	++output.state->unlocked_write_calls;
	for (; first != last; ++first)
	{
		require(output.state->size != sizeof(output.state->buffer));
		output.state->buffer[output.state->size] = *first;
		++output.state->size;
	}
}

inline void write_all_bytes_overflow_define(byte_locked_output_ref &output, ::std::byte const *,
											::std::byte const *) noexcept
{
	++output.state->locked_write_calls;
	require(false);
}

inline void write_all_bytes_overflow_define(byte_unlocked_output_ref &output, ::std::byte const *first,
											::std::byte const *last) noexcept
{
	require(output.state->locked);
	++output.state->unlocked_write_calls;
	for (; first != last; ++first)
	{
		require(output.state->size != sizeof(output.state->buffer));
		output.state->buffer[output.state->size] =
			static_cast<char>(::std::to_integer<unsigned char>(*first));
		++output.state->size;
	}
}

} // namespace write_range_decay_ownership

int main()
{
	using namespace write_range_decay_ownership;

	char const text[]{'m', 'o', 'v', 'e', '-', 'o', 'n', 'l', 'y'};
	iterator_state iterator_observations;
	move_only_range range{text, text + sizeof(text), __builtin_addressof(iterator_observations)};
	output_state output_observations;
	locked_output output{__builtin_addressof(output_observations)};

	// The deleted iterator copy constructor is the compile-time ownership oracle: the public decay boundary may own
	// the prvalue once, while the mutex implementation must synchronously borrow that same object.  Runtime counters
	// independently verify one lock, one normalization, and source-order traversal through the unlocked observer.
	::fast_io::operations::write_all_range(output, range);

	require(iterator_observations.begin_calls == 1u);
	require(iterator_observations.dereferences == sizeof(text));
	require(iterator_observations.increments == sizeof(text));
	require(output_observations.normalizations == 1u);
	require(output_observations.unlocked_materializations == 1u);
	require(output_observations.locks == 1u);
	require(output_observations.unlocks == 1u);
	require(!output_observations.locked);
	require(output_observations.locked_write_calls == 0u);
	require(output_observations.unlocked_write_calls == sizeof(text));
	require(output_observations.size == sizeof(text));
	for (::std::size_t index{}; index != sizeof(text); ++index)
	{
		require(output_observations.buffer[index] == text[index]);
	}

	medium_value medium_values[4]{};
	for (::std::size_t value_index{}; value_index != 4u; ++value_index)
	{
		for (::std::size_t byte_index{}; byte_index != sizeof(medium_value::bytes); ++byte_index)
		{
			medium_values[value_index].bytes[byte_index] =
				static_cast<unsigned char>((value_index * 53u + byte_index * 3u) & 0xffu);
		}
	}
	iterator_state medium_iterator_observations;
	medium_move_only_range medium_range{medium_values, medium_values + 4,
										__builtin_addressof(medium_iterator_observations)};
	output_state medium_output_observations;
	byte_locked_output medium_output{__builtin_addressof(medium_output_observations)};

	// A 64-byte non-pointer iterator value exercises bounded batching.  This sink intentionally exposes only the byte
	// primitive while declaring a two-byte character type, proving that admission and dispatch remain in the byte CPO
	// domain and that batching copies element representations rather than bytes from the iterator object itself.
	::fast_io::operations::write_all_range(medium_output, medium_range);

	require(medium_iterator_observations.begin_calls == 1u);
	require(medium_iterator_observations.move_constructions <= 1u);
	require(medium_iterator_observations.move_assignments == 0u);
	require(medium_iterator_observations.dereferences == 4u);
	require(medium_iterator_observations.increments == 4u);
	require(medium_output_observations.normalizations == 1u);
	require(medium_output_observations.unlocked_materializations == 1u);
	require(medium_output_observations.locks == 1u);
	require(medium_output_observations.unlocks == 1u);
	require(!medium_output_observations.locked);
	require(medium_output_observations.locked_write_calls == 0u);
	require(medium_output_observations.unlocked_write_calls == 1u);
	require(medium_output_observations.size == sizeof(medium_values));
	for (::std::size_t value_index{}; value_index != 4u; ++value_index)
	{
		for (::std::size_t byte_index{}; byte_index != sizeof(medium_value::bytes); ++byte_index)
		{
			auto const output_index{value_index * sizeof(medium_value) + byte_index};
			require(static_cast<unsigned char>(medium_output_observations.buffer[output_index]) ==
					medium_values[value_index].bytes[byte_index]);
		}
	}

	oversized_value oversized_values[2]{};
	for (::std::size_t value_index{}; value_index != 2u; ++value_index)
	{
		for (::std::size_t byte_index{}; byte_index != sizeof(oversized_value::bytes); ++byte_index)
		{
			oversized_values[value_index].bytes[byte_index] =
				static_cast<unsigned char>((value_index * 97u + byte_index) & 0xffu);
		}
	}
	iterator_state oversized_iterator_observations;
	oversized_move_only_range oversized_range{oversized_values, oversized_values + 2,
											  __builtin_addressof(oversized_iterator_observations)};
	output_state oversized_output_observations;
	locked_output oversized_output{__builtin_addressof(oversized_output_observations)};

	// Values larger than the fixed staging buffer must be written directly from the referred element object.  The
	// iterator is still owned once, and two elements cannot cause per-element or per-mutex iterator moves.
	::fast_io::operations::write_all_range(oversized_output, oversized_range);

	require(oversized_iterator_observations.begin_calls == 1u);
	require(oversized_iterator_observations.move_constructions <= 1u);
	require(oversized_iterator_observations.move_assignments == 0u);
	require(oversized_iterator_observations.dereferences == 2u);
	require(oversized_iterator_observations.increments == 2u);
	require(oversized_output_observations.normalizations == 1u);
	require(oversized_output_observations.unlocked_materializations == 1u);
	require(oversized_output_observations.locks == 1u);
	require(oversized_output_observations.unlocks == 1u);
	require(!oversized_output_observations.locked);
	require(oversized_output_observations.locked_write_calls == 0u);
	require(oversized_output_observations.unlocked_write_calls == 2u);
	require(oversized_output_observations.size == sizeof(oversized_values));
	for (::std::size_t value_index{}; value_index != 2u; ++value_index)
	{
		for (::std::size_t byte_index{}; byte_index != sizeof(oversized_value::bytes); ++byte_index)
		{
			auto const output_index{value_index * sizeof(oversized_value) + byte_index};
			require(static_cast<unsigned char>(oversized_output_observations.buffer[output_index]) ==
					oversized_values[value_index].bytes[byte_index]);
		}
	}
}
