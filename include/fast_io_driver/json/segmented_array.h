#pragma once

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../../fast_io_core.h"

namespace fast_io::json
{

/*
Describes one physically contiguous, fully constructed segment of a JSON
array.  Unlike an I/O scatter this descriptor retains the element type: a JSON
walker can traverse nodes without erasing their semantics or manufacturing a
byte-oriented provenance promise.
*/
template <typename T>
struct basic_json_array_segment
{
	using value_type = T;
	T *base{};
	::std::size_t len{};

	[[nodiscard]] constexpr T *begin() const noexcept
	{
		return base;
	}

	[[nodiscard]] constexpr T *end() const noexcept
	{
		return len == 0u ? base : base + len;
	}

	[[nodiscard]] constexpr bool empty() const noexcept
	{
		return len == 0u;
	}

	[[nodiscard]] constexpr ::std::size_t size() const noexcept
	{
		return len;
	}

	[[nodiscard]] constexpr T &operator[](::std::size_t index) const noexcept
	{
		return base[index];
	}
};

/*
Allocator-aware, growth-stable segmented storage for JSON DOM array nodes.

The controller owns an allocator pointer for every physical segment.  Values
in existing segments are never relocated when the array grows; only the small
controller directory can move.  This property removes the O(n) node moves of
geometrically growing contiguous storage without imposing std::deque's very
small implementation-defined block size.

The default schedule is intentionally JSON-shaped rather than uniform:

  * the first segment stores four nodes;
  * 64-node segments cover total capacity through roughly 4 Ki nodes;
  * 256-node segments are used thereafter.

Small and medium arrays therefore waste at most 63 ordinary node slots, while
a million-element array uses only about 1.3% more controller entries than a
uniform 256-node scheme.  `Block` remains the steady-state segment size;
changing it is a policy decision and does not affect the public API.

Invariants:

  * [0,size_) is fully constructed, with no holes;
  * every controller entry owns exactly one allocator allocation;
  * size_ <= capacity(), and no element address changes before erase/destruct;
  * back_curr_ names one-past the last element (or the first slot when empty),
    and back_end_ names the allocation boundary of that same segment;
  * directory growth has a commit point after all new pointer entries have
    been constructed, so allocation failure leaves the old directory intact.

The container deliberately implements the mutation surface required by the
JSON DOM.  It is not a drop-in implementation of every std::vector insertion
and erasure operation.
*/
template <typename T, typename Allocator = ::std::allocator<T>,
		  ::std::size_t First = 4u, ::std::size_t Block = 256u>
class basic_json_segmented_array
{
	static_assert(First != 0u, "a JSON segmented array needs a non-empty first segment");
	static_assert(Block != 0u, "a JSON segmented array needs a non-empty steady segment");
	static_assert(First <= static_cast<::std::size_t>(
							 (::std::numeric_limits<::std::ptrdiff_t>::max)()),
		"the first JSON segment must fit iterator difference_type");
	static_assert(Block <= static_cast<::std::size_t>(
							 (::std::numeric_limits<::std::ptrdiff_t>::max)()),
		"the steady JSON segment must fit iterator difference_type");

public:
	using value_type = T;
	using allocator_type = Allocator;
	using allocator_traits = ::std::allocator_traits<allocator_type>;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using reference = value_type &;
	using const_reference = value_type const &;
	using pointer = typename allocator_traits::pointer;
	using const_pointer = typename allocator_traits::const_pointer;

	static inline constexpr size_type first_segment_capacity{First};
	static inline constexpr size_type regular_segment_capacity{Block};
	static inline constexpr size_type small_segment_capacity{
		Block < 64u ? Block : static_cast<size_type>(64u)};
	static inline constexpr size_type small_capacity_target{4096u};

private:
	using block_pointer = pointer;
	using directory_allocator =
		typename allocator_traits::template rebind_alloc<block_pointer>;
	using directory_traits = ::std::allocator_traits<directory_allocator>;
	using directory_pointer = typename directory_traits::pointer;

	struct location
	{
		size_type block_index;
		size_type offset;
	};

	[[nodiscard]] static constexpr size_type small_segment_count_value() noexcept
	{
		if constexpr (First >= small_capacity_target)
		{
			return 0u;
		}
		else
		{
			auto const remaining{small_capacity_target - First};
			return remaining / small_segment_capacity +
				   static_cast<size_type>(remaining % small_segment_capacity != 0u);
		}
	}

	static inline constexpr size_type small_segment_count{
		small_segment_count_value()};

	[[nodiscard]] static constexpr size_type large_segment_begin() noexcept
	{
		return First + small_segment_count * small_segment_capacity;
	}

	[[nodiscard]] static constexpr size_type
	segment_capacity_unchecked(size_type block_index) noexcept
	{
		if (block_index == 0u)
		{
			return First;
		}
		return block_index <= small_segment_count ? small_segment_capacity : Block;
	}

	[[nodiscard]] static constexpr size_type
	segment_begin_unchecked(size_type block_index) noexcept
	{
		if (block_index == 0u)
		{
			return 0u;
		}
		if (block_index <= small_segment_count + 1u)
		{
			return First + (block_index - 1u) * small_segment_capacity;
		}
		return large_segment_begin() +
			   (block_index - (small_segment_count + 1u)) * Block;
	}

	[[nodiscard]] static constexpr size_type
	capacity_for_blocks_unchecked(size_type block_count) noexcept
	{
		return block_count == 0u ? 0u : segment_begin_unchecked(block_count);
	}

	[[nodiscard]] static constexpr size_type blocks_for_size_unchecked(size_type size) noexcept
	{
		if (size == 0u)
		{
			return 0u;
		}
		if (size <= First)
		{
			return 1u;
		}
		auto const small_end{large_segment_begin()};
		if (size <= small_end)
		{
			auto const remaining{size - First};
			return 1u + remaining / small_segment_capacity +
				   static_cast<size_type>(remaining % small_segment_capacity != 0u);
		}
		auto const remaining{size - small_end};
		return 1u + small_segment_count + remaining / Block +
			   static_cast<size_type>(remaining % Block != 0u);
	}

	[[nodiscard]] static constexpr location locate_unchecked(size_type index) noexcept
	{
		if (index < First)
		{
			return {0u, index};
		}
		auto const small_end{large_segment_begin()};
		if (index < small_end)
		{
			auto const tail{index - First};
			return {1u + tail / small_segment_capacity,
					tail % small_segment_capacity};
		}
		auto const tail{index - small_end};
		return {1u + small_segment_count + tail / Block, tail % Block};
	}

	[[noreturn]] static void throw_length_error()
	{
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		throw ::std::length_error("fast_io JSON segmented array is too large");
#else
		::fast_io::fast_terminate();
#endif
	}

	[[nodiscard]] block_pointer *directory_raw() noexcept
	{
		return directory_capacity_ == 0u ? nullptr : ::std::to_address(directory_);
	}

	[[nodiscard]] block_pointer const *directory_raw() const noexcept
	{
		return directory_capacity_ == 0u ? nullptr : ::std::to_address(directory_);
	}

	[[nodiscard]] value_type *block_raw(size_type index) noexcept
	{
		return ::std::to_address(directory_raw()[index]);
	}

	[[nodiscard]] value_type const *block_raw(size_type index) const noexcept
	{
		return ::std::to_address(directory_raw()[index]);
	}

	[[nodiscard]] size_type directory_max_size() const noexcept
	{
		return directory_traits::max_size(directory_allocator_);
	}

	[[nodiscard]] size_type maximum_size_for_current_allocators() const noexcept
	{
		auto const element_allocation_limit{allocator_traits::max_size(allocator_)};
		auto const controller_limit{directory_max_size()};
		if (controller_limit == 0u || element_allocation_limit < First)
		{
			return 0u;
		}

		auto const difference_limit{max_size_static()};
		size_type block_limit{controller_limit};
		if (element_allocation_limit < small_segment_capacity)
		{
			block_limit = (::std::min)(block_limit, static_cast<size_type>(1u));
		}
		else if (element_allocation_limit < Block)
		{
			block_limit = (::std::min)(
				block_limit, static_cast<size_type>(1u + small_segment_count));
		}

		if (block_limit == 0u)
		{
			return 0u;
		}
		if (block_limit <= 1u + small_segment_count)
		{
			return (::std::min)(difference_limit,
				capacity_for_blocks_unchecked(block_limit));
		}
		auto const base{large_segment_begin()};
		auto const large_blocks{block_limit - (1u + small_segment_count)};
		if (base >= difference_limit ||
			large_blocks > (difference_limit - base) / Block)
		{
			return difference_limit;
		}
		return base + large_blocks * Block;
	}

	void reserve_directory(size_type requested)
	{
		if (requested <= directory_capacity_)
		{
			return;
		}
		if (requested > directory_max_size()) [[unlikely]]
		{
			throw_length_error();
		}

		auto const maximum_directory_size{directory_max_size()};
		auto grown{directory_capacity_ == 0u
			? (::std::min)(static_cast<size_type>(4u), maximum_directory_size)
			: directory_capacity_};
		while (grown < requested)
		{
			if (grown > maximum_directory_size / 2u)
			{
				grown = requested;
				break;
			}
			grown *= 2u;
		}
		auto replacement{directory_traits::allocate(directory_allocator_, grown)};
		auto *const replacement_raw{::std::to_address(replacement)};
		auto const *const old_raw{directory_raw()};
		size_type constructed{};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			for (; constructed != directory_size_; ++constructed)
			{
				directory_traits::construct(directory_allocator_,
					replacement_raw + constructed, old_raw[constructed]);
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			while (constructed != 0u)
			{
				--constructed;
				directory_traits::destroy(directory_allocator_,
					replacement_raw + constructed);
			}
			directory_traits::deallocate(directory_allocator_, replacement, grown);
			throw;
		}
#endif

		for (size_type index{}; index != directory_size_; ++index)
		{
			directory_traits::destroy(directory_allocator_,
				const_cast<block_pointer *>(old_raw) + index);
		}
		if (directory_capacity_ != 0u)
		{
			directory_traits::deallocate(directory_allocator_, directory_,
				directory_capacity_);
		}
		directory_ = replacement;
		directory_capacity_ = grown;
	}

	void append_directory_entry(block_pointer const &entry)
	{
		reserve_directory(directory_size_ + 1u);
		directory_traits::construct(directory_allocator_,
			directory_raw() + directory_size_, entry);
		++directory_size_;
	}

	void remove_last_directory_entry() noexcept
	{
		--directory_size_;
		directory_traits::destroy(directory_allocator_,
			directory_raw() + directory_size_);
	}

	void append_block()
	{
		auto const segment_capacity{segment_capacity_unchecked(directory_size_)};
		if (segment_capacity > allocator_traits::max_size(allocator_)) [[unlikely]]
		{
			throw_length_error();
		}
		auto allocated{allocator_traits::allocate(allocator_, segment_capacity)};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
			append_directory_entry(allocated);
		}
		catch (...)
		{
			allocator_traits::deallocate(allocator_, allocated, segment_capacity);
			throw;
		}
#else
		append_directory_entry(allocated);
#endif
	}

	template <typename... Args>
	void construct_element(value_type *destination, Args &&...args)
	{
		/* A custom allocator construct hook may already perform uses-allocator
		   injection (notably tracking/scoped allocators).  Passing synthesized
		   arguments to that hook would inject twice. */
		if constexpr (requires(allocator_type &allocator, value_type *pointer_value) {
			allocator.construct(pointer_value, ::std::forward<Args>(args)...);
		})
		{
			allocator_traits::construct(allocator_, destination,
				::std::forward<Args>(args)...);
		}
		else
		{
			auto construction_arguments{
				::std::uses_allocator_construction_args<value_type>(
					allocator_, ::std::forward<Args>(args)...)};
			::std::apply(
				[&](auto &&...unpacked) {
					allocator_traits::construct(
						allocator_, destination,
						::std::forward<decltype(unpacked)>(unpacked)...);
				},
				::std::move(construction_arguments));
		}
	}

	void reset_back_cursor() noexcept
	{
		back_block_index_ = 0u;
		if (directory_size_ == 0u)
		{
			back_curr_ = back_end_ = nullptr;
			return;
		}
		back_curr_ = block_raw(0u);
		back_end_ = back_curr_ + segment_capacity_unchecked(0u);
	}

	void set_back_cursor_from_size() noexcept
	{
		if (size_ == 0u)
		{
			reset_back_cursor();
			return;
		}
		auto where{locate_unchecked(size_ - 1u)};
		back_block_index_ = where.block_index;
		auto *const begin{block_raw(where.block_index)};
		back_curr_ = begin + where.offset + 1u;
		back_end_ = begin + segment_capacity_unchecked(where.block_index);
	}

	void destroy_constructed_elements() noexcept
	{
		while (size_ != 0u)
		{
			--back_curr_;
			allocator_traits::destroy(allocator_, back_curr_);
			--size_;
			if (size_ != 0u && back_curr_ == block_raw(back_block_index_))
			{
				--back_block_index_;
				back_curr_ = block_raw(back_block_index_) +
					segment_capacity_unchecked(back_block_index_);
				back_end_ = back_curr_;
			}
		}
		reset_back_cursor();
	}

	void release_storage() noexcept
	{
		destroy_constructed_elements();
		auto *const raw{directory_raw()};
		for (size_type index{}; index != directory_size_; ++index)
		{
			allocator_traits::deallocate(allocator_, raw[index],
				segment_capacity_unchecked(index));
			directory_traits::destroy(directory_allocator_, raw + index);
		}
		if (directory_capacity_ != 0u)
		{
			directory_traits::deallocate(directory_allocator_, directory_,
				directory_capacity_);
		}
		directory_ = directory_pointer{};
		directory_size_ = directory_capacity_ = 0u;
		back_curr_ = back_end_ = nullptr;
		back_block_index_ = 0u;
	}

	void steal_storage(basic_json_segmented_array &other) noexcept
	{
		directory_ = ::std::exchange(other.directory_, directory_pointer{});
		directory_size_ = ::std::exchange(other.directory_size_, 0u);
		directory_capacity_ = ::std::exchange(other.directory_capacity_, 0u);
		size_ = ::std::exchange(other.size_, 0u);
		back_block_index_ = ::std::exchange(other.back_block_index_, 0u);
		back_curr_ = ::std::exchange(other.back_curr_, nullptr);
		back_end_ = ::std::exchange(other.back_end_, nullptr);
	}

	void swap_storage(basic_json_segmented_array &other) noexcept
	{
		using ::std::swap;
		swap(directory_, other.directory_);
		swap(directory_size_, other.directory_size_);
		swap(directory_capacity_, other.directory_capacity_);
		swap(size_, other.size_);
		swap(back_block_index_, other.back_block_index_);
		swap(back_curr_, other.back_curr_);
		swap(back_end_, other.back_end_);
	}

	template <bool Const>
	class iterator_impl
	{
		using raw_pointer = ::std::conditional_t<Const, T const *, T *>;
		using directory_entry_pointer = block_pointer const *;

		directory_entry_pointer controller_begin_{};
		directory_entry_pointer controller_after_{};
		directory_entry_pointer controller_current_{};
		raw_pointer block_curr_{};
		raw_pointer block_end_{};

		friend class basic_json_segmented_array;
		template <bool>
		friend class iterator_impl;

		constexpr iterator_impl(directory_entry_pointer controller_begin,
			directory_entry_pointer controller_after, size_type logical_position,
			size_type total_capacity) noexcept
			: controller_begin_(controller_begin), controller_after_(controller_after)
		{
			set_position(logical_position, total_capacity);
		}

		[[nodiscard]] constexpr size_type current_block_index() const noexcept
		{
			return static_cast<size_type>(controller_current_ - controller_begin_);
		}

		[[nodiscard]] constexpr size_type logical_position() const noexcept
		{
			if (controller_begin_ == controller_after_)
			{
				return 0u;
			}
			auto const *const begin{::std::to_address(*controller_current_)};
			return segment_begin_unchecked(current_block_index()) +
				   static_cast<size_type>(block_curr_ - begin);
		}

		constexpr void set_position(size_type position, size_type total_capacity) noexcept
		{
			if (controller_begin_ == controller_after_)
			{
				controller_current_ = controller_begin_;
				block_curr_ = block_end_ = nullptr;
				return;
			}
			location where{};
			if (position == total_capacity)
			{
				where.block_index = static_cast<size_type>(
					controller_after_ - controller_begin_ - 1);
				where.offset = segment_capacity_unchecked(where.block_index);
			}
			else
			{
				where = locate_unchecked(position);
			}
			controller_current_ = controller_begin_ + where.block_index;
			auto *const begin{::std::to_address(*controller_current_)};
			block_curr_ = begin + where.offset;
			block_end_ = begin + segment_capacity_unchecked(where.block_index);
		}

	public:
		using value_type = T;
		using difference_type = ::std::ptrdiff_t;
		using reference = ::std::conditional_t<Const, T const &, T &>;
		using pointer = ::std::conditional_t<Const, T const *, T *>;
		using iterator_category = ::std::random_access_iterator_tag;
		using iterator_concept = ::std::random_access_iterator_tag;

		constexpr iterator_impl() noexcept = default;

		template <bool OtherConst>
		constexpr iterator_impl(iterator_impl<OtherConst> const &other) noexcept
			requires(Const && !OtherConst)
			: controller_begin_(other.controller_begin_),
			  controller_after_(other.controller_after_),
			  controller_current_(other.controller_current_),
			  block_curr_(other.block_curr_), block_end_(other.block_end_)
		{}

		[[nodiscard]] constexpr reference operator*() const noexcept
		{
			return *block_curr_;
		}

		[[nodiscard]] constexpr pointer operator->() const noexcept
		{
			return block_curr_;
		}

		constexpr iterator_impl &operator++() noexcept
		{
			if (++block_curr_ == block_end_ &&
				controller_current_ + 1 != controller_after_)
			{
				++controller_current_;
				block_curr_ = ::std::to_address(*controller_current_);
				block_end_ = block_curr_ +
					segment_capacity_unchecked(current_block_index());
			}
			return *this;
		}

		constexpr iterator_impl operator++(int) noexcept
		{
			auto copy{*this};
			++*this;
			return copy;
		}

		constexpr iterator_impl &operator--() noexcept
		{
			auto const *const current_begin{
				::std::to_address(*controller_current_)};
			if (block_curr_ == current_begin)
			{
				--controller_current_;
				auto *const previous_begin{
					::std::to_address(*controller_current_)};
				block_end_ = previous_begin +
					segment_capacity_unchecked(current_block_index());
				block_curr_ = block_end_;
			}
			--block_curr_;
			return *this;
		}

		constexpr iterator_impl operator--(int) noexcept
		{
			auto copy{*this};
			--*this;
			return copy;
		}

		constexpr iterator_impl &operator+=(difference_type offset) noexcept
		{
			auto const current{static_cast<difference_type>(logical_position())};
			auto const target{current + offset};
			auto const block_count{controller_begin_ == controller_after_
				? 0u
				: static_cast<size_type>(controller_after_ - controller_begin_)};
			auto const total_capacity{
				capacity_for_blocks_unchecked(block_count)};
			set_position(static_cast<size_type>(target), total_capacity);
			return *this;
		}

		constexpr iterator_impl &operator-=(difference_type offset) noexcept
		{
			return *this += -offset;
		}

		[[nodiscard]] constexpr reference operator[](difference_type offset) const noexcept
		{
			return *(*this + offset);
		}

		friend constexpr iterator_impl operator+(iterator_impl iterator,
			difference_type offset) noexcept
		{
			iterator += offset;
			return iterator;
		}

		friend constexpr iterator_impl operator+(difference_type offset,
			iterator_impl iterator) noexcept
		{
			return iterator + offset;
		}

		friend constexpr iterator_impl operator-(iterator_impl iterator,
			difference_type offset) noexcept
		{
			iterator -= offset;
			return iterator;
		}

		template <bool OtherConst>
		[[nodiscard]] constexpr difference_type operator-(
			iterator_impl<OtherConst> const &other) const noexcept
		{
			return static_cast<difference_type>(logical_position()) -
				   static_cast<difference_type>(other.logical_position());
		}

		template <bool OtherConst>
		[[nodiscard]] constexpr bool operator==(
			iterator_impl<OtherConst> const &other) const noexcept
		{
			return controller_current_ == other.controller_current_ &&
				   block_curr_ == other.block_curr_;
		}

		template <bool OtherConst>
		[[nodiscard]] constexpr auto operator<=> (
			iterator_impl<OtherConst> const &other) const noexcept
		{
			return logical_position() <=> other.logical_position();
		}
	};

public:
	using iterator = iterator_impl<false>;
	using const_iterator = iterator_impl<true>;
	using reverse_iterator = ::std::reverse_iterator<iterator>;
	using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

	constexpr basic_json_segmented_array() noexcept(
		::std::is_nothrow_default_constructible_v<allocator_type> &&
		::std::is_nothrow_constructible_v<directory_allocator,
			allocator_type const &>)
		: allocator_{}, directory_allocator_{allocator_}
	{}

	constexpr explicit basic_json_segmented_array(
		allocator_type const &allocator) noexcept(
			::std::is_nothrow_copy_constructible_v<allocator_type> &&
			::std::is_nothrow_constructible_v<directory_allocator,
				allocator_type const &>)
		: allocator_(allocator), directory_allocator_(allocator)
	{}

	basic_json_segmented_array(basic_json_segmented_array const &other)
		: basic_json_segmented_array(
			allocator_traits::select_on_container_copy_construction(
				other.allocator_))
	{
		copy_from(other);
	}

	basic_json_segmented_array(basic_json_segmented_array const &other,
		allocator_type const &allocator)
		: basic_json_segmented_array(allocator)
	{
		copy_from(other);
	}

	basic_json_segmented_array(basic_json_segmented_array &&other) noexcept(
		::std::is_nothrow_move_constructible_v<allocator_type> &&
		::std::is_nothrow_move_constructible_v<directory_allocator>)
		: allocator_(::std::move(other.allocator_)),
		  directory_allocator_(::std::move(other.directory_allocator_))
	{
		steal_storage(other);
	}

	basic_json_segmented_array(basic_json_segmented_array &&other,
		allocator_type const &allocator)
		: basic_json_segmented_array(allocator)
	{
		if (allocator_ == other.allocator_ &&
			directory_allocator_ == other.directory_allocator_)
		{
			steal_storage(other);
		}
		else
		{
			move_from_unequal(other);
		}
	}

	basic_json_segmented_array &operator=(basic_json_segmented_array const &other)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		if constexpr (allocator_traits::propagate_on_container_copy_assignment::value)
		{
			basic_json_segmented_array replacement(other, other.allocator_);
			release_storage();
			allocator_ = other.allocator_;
			directory_allocator_ = directory_allocator{allocator_};
			steal_storage(replacement);
			return *this;
		}
		basic_json_segmented_array replacement(other, allocator_);
		swap_storage(replacement);
		return *this;
	}

	basic_json_segmented_array &operator=(basic_json_segmented_array &&other) noexcept(
		allocator_traits::propagate_on_container_move_assignment::value
			? (::std::is_nothrow_move_assignable_v<allocator_type> &&
			   ::std::is_nothrow_move_assignable_v<directory_allocator>)
			: allocator_traits::is_always_equal::value)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		if constexpr (allocator_traits::propagate_on_container_move_assignment::value)
		{
			release_storage();
			allocator_ = ::std::move(other.allocator_);
			directory_allocator_ = ::std::move(other.directory_allocator_);
			steal_storage(other);
		}
		else if constexpr (allocator_traits::is_always_equal::value)
		{
			release_storage();
			steal_storage(other);
		}
		else
		{
			if (allocator_ == other.allocator_ &&
				directory_allocator_ == other.directory_allocator_)
			{
				release_storage();
				steal_storage(other);
			}
			else
			{
				basic_json_segmented_array replacement(::std::move(other), allocator_);
				swap_storage(replacement);
			}
		}
		return *this;
	}

	~basic_json_segmented_array() noexcept
	{
		release_storage();
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept(
		::std::is_nothrow_copy_constructible_v<allocator_type>)
	{
		return allocator_;
	}

	[[nodiscard]] static constexpr size_type max_size_static() noexcept
	{
		return static_cast<size_type>((::std::numeric_limits<difference_type>::max)());
	}

	[[nodiscard]] size_type max_size() const noexcept
	{
		return maximum_size_for_current_allocators();
	}

	[[nodiscard]] constexpr size_type size() const noexcept
	{
		return size_;
	}

	[[nodiscard]] constexpr bool empty() const noexcept
	{
		return size_ == 0u;
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept
	{
		return capacity_for_blocks_unchecked(directory_size_);
	}

	void reserve(size_type requested)
	{
		if (requested <= capacity())
		{
			return;
		}
		if (requested > max_size()) [[unlikely]]
		{
			throw_length_error();
		}
		auto const required_blocks{blocks_for_size_unchecked(requested)};
		if (required_blocks > directory_max_size()) [[unlikely]]
		{
			throw_length_error();
		}
		reserve_directory(required_blocks);
		auto const original_blocks{directory_size_};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			while (directory_size_ != required_blocks)
			{
				append_block();
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			while (directory_size_ != original_blocks)
			{
				auto const index{directory_size_ - 1u};
				allocator_traits::deallocate(allocator_, directory_raw()[index],
					segment_capacity_unchecked(index));
				remove_last_directory_entry();
			}
			throw;
		}
#endif
		if (original_blocks == 0u)
		{
			reset_back_cursor();
		}
	}

	template <typename... Args>
	reference emplace_back(Args &&...args)
	{
		[[maybe_unused]] bool added_block{};
		[[maybe_unused]] bool advanced_block{};
		if (size_ == capacity())
		{
			if (size_ == max_size()) [[unlikely]]
			{
				throw_length_error();
			}
			append_block();
			added_block = true;
			if (size_ == 0u)
			{
				reset_back_cursor();
			}
		}
		if (back_curr_ == back_end_)
		{
			++back_block_index_;
			back_curr_ = block_raw(back_block_index_);
			back_end_ = back_curr_ +
				segment_capacity_unchecked(back_block_index_);
			advanced_block = true;
		}
		auto *const destination{back_curr_};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
			construct_element(destination, ::std::forward<Args>(args)...);
		}
		catch (...)
		{
			if (added_block)
			{
				auto const index{directory_size_ - 1u};
				allocator_traits::deallocate(allocator_, directory_raw()[index],
					segment_capacity_unchecked(index));
				remove_last_directory_entry();
			}
			if (added_block || advanced_block)
			{
				set_back_cursor_from_size();
			}
			throw;
		}
#else
		construct_element(destination, ::std::forward<Args>(args)...);
#endif
		++back_curr_;
		++size_;
		return *destination;
	}

	void push_back(value_type const &value)
	{
		emplace_back(value);
	}

	void push_back(value_type &&value)
	{
		emplace_back(::std::move(value));
	}

	void clear() noexcept
	{
		destroy_constructed_elements();
	}

	void pop_back() noexcept
	{
		--back_curr_;
		allocator_traits::destroy(allocator_, back_curr_);
		--size_;
		if (size_ == 0u)
		{
			reset_back_cursor();
			return;
		}
		auto *const current_begin{block_raw(back_block_index_)};
		if (back_curr_ == current_begin)
		{
			--back_block_index_;
			back_curr_ = block_raw(back_block_index_) +
				segment_capacity_unchecked(back_block_index_);
			back_end_ = back_curr_;
		}
	}

	/*
	Erase preserves the segmented allocation directory and shifts only the
	logical suffix.  Thus elements preceding `first` keep both their values and
	addresses, while erased elements and the suffix have the usual
	vector-compatible invalidation rule.  If value_type move assignment throws,
	the array remains fully constructed with its original size (the basic
	guarantee); no element is destroyed until every suffix assignment succeeds.
	*/
	iterator erase(const_iterator position)
		requires(::std::is_move_assignable_v<value_type>)
	{
		return erase(position, position + 1);
	}

	iterator erase(const_iterator first, const_iterator last)
		requires(::std::is_move_assignable_v<value_type>)
	{
		auto const first_offset{first - cbegin()};
		auto const last_offset{last - cbegin()};
		auto const first_index{static_cast<size_type>(first_offset)};
		auto const last_index{static_cast<size_type>(last_offset)};
		auto const erased_count{last_index - first_index};
		if (erased_count == 0u)
		{
			return begin() + first_offset;
		}

		for (auto source{last_index}; source != size_; ++source)
		{
			(*this)[source - erased_count] = ::std::move((*this)[source]);
		}
		for (size_type count{}; count != erased_count; ++count)
		{
			pop_back();
		}
		return begin() + first_offset;
	}

	[[nodiscard]] reference operator[](size_type index) noexcept
	{
		auto const where{locate_unchecked(index)};
		return block_raw(where.block_index)[where.offset];
	}

	[[nodiscard]] const_reference operator[](size_type index) const noexcept
	{
		auto const where{locate_unchecked(index)};
		return block_raw(where.block_index)[where.offset];
	}

	[[nodiscard]] reference back() noexcept
	{
		return back_curr_[-1];
	}

	[[nodiscard]] const_reference back() const noexcept
	{
		return back_curr_[-1];
	}

	[[nodiscard]] reference front() noexcept
	{
		return *block_raw(0u);
	}

	[[nodiscard]] const_reference front() const noexcept
	{
		return *block_raw(0u);
	}

	[[nodiscard]] iterator begin() noexcept
	{
		auto *const first{directory_raw()};
		auto *const after{first == nullptr ? nullptr : first + directory_size_};
		return {first, after, 0u, capacity()};
	}

	[[nodiscard]] iterator end() noexcept
	{
		auto *const first{directory_raw()};
		auto *const after{first == nullptr ? nullptr : first + directory_size_};
		return {first, after, size_, capacity()};
	}

	[[nodiscard]] const_iterator begin() const noexcept
	{
		auto const *const first{directory_raw()};
		auto const *const after{first == nullptr ? nullptr : first + directory_size_};
		return {first, after, 0u, capacity()};
	}

	[[nodiscard]] const_iterator end() const noexcept
	{
		auto const *const first{directory_raw()};
		auto const *const after{first == nullptr ? nullptr : first + directory_size_};
		return {first, after, size_, capacity()};
	}

	[[nodiscard]] const_iterator cbegin() const noexcept
	{
		return begin();
	}

	[[nodiscard]] const_iterator cend() const noexcept
	{
		return end();
	}

	[[nodiscard]] reverse_iterator rbegin() noexcept
	{
		return reverse_iterator{end()};
	}

	[[nodiscard]] reverse_iterator rend() noexcept
	{
		return reverse_iterator{begin()};
	}

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator{end()};
	}

	[[nodiscard]] const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] size_type segment_count() const noexcept
	{
		return blocks_for_size_unchecked(size_);
	}

	[[nodiscard]] basic_json_array_segment<value_type>
	segment_at(size_type index) noexcept
	{
		auto const begin_index{segment_begin_unchecked(index)};
		auto const remaining{size_ - begin_index};
		return {block_raw(index), (::std::min)(remaining,
			segment_capacity_unchecked(index))};
	}

	[[nodiscard]] basic_json_array_segment<value_type const>
	segment_at(size_type index) const noexcept
	{
		auto const begin_index{segment_begin_unchecked(index)};
		auto const remaining{size_ - begin_index};
		return {block_raw(index), (::std::min)(remaining,
			segment_capacity_unchecked(index))};
	}

	void swap(basic_json_segmented_array &other) noexcept(
		allocator_traits::propagate_on_container_swap::value
			? (::std::is_nothrow_swappable_v<allocator_type> &&
			   ::std::is_nothrow_swappable_v<directory_allocator>)
			: true)
	{
		using ::std::swap;
		if constexpr (allocator_traits::propagate_on_container_swap::value)
		{
			swap(allocator_, other.allocator_);
			swap(directory_allocator_, other.directory_allocator_);
		}
		else if (!(allocator_ == other.allocator_) ||
				 !(directory_allocator_ == other.directory_allocator_)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		swap_storage(other);
	}

private:
	void copy_from(basic_json_segmented_array const &other)
	{
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			reserve(other.size_);
			for (auto const &value : other)
			{
				emplace_back(value);
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			release_storage();
			throw;
		}
#endif
	}

	void move_from_unequal(basic_json_segmented_array &other)
	{
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			reserve(other.size_);
			for (auto &value : other)
			{
				emplace_back(::std::move(value));
			}
			other.clear();
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			release_storage();
			throw;
		}
#endif
	}

	[[no_unique_address]] allocator_type allocator_{};
	[[no_unique_address]] directory_allocator directory_allocator_{allocator_};
	directory_pointer directory_{};
	size_type directory_size_{};
	size_type directory_capacity_{};
	size_type size_{};
	size_type back_block_index_{};
	value_type *back_curr_{};
	value_type *back_end_{};
};

template <typename T, typename Allocator, ::std::size_t First, ::std::size_t Block>
inline void swap(
	basic_json_segmented_array<T, Allocator, First, Block> &left,
	basic_json_segmented_array<T, Allocator, First, Block> &right) noexcept(
	noexcept(left.swap(right)))
{
	left.swap(right);
}

template <typename T, typename Allocator, ::std::size_t First, ::std::size_t Block>
[[nodiscard]] inline constexpr ::std::size_t json_array_segment_count_define(
	basic_json_segmented_array<T, Allocator, First, Block> const &array) noexcept
{
	return array.segment_count();
}

template <typename T, typename Allocator, ::std::size_t First, ::std::size_t Block>
[[nodiscard]] inline constexpr basic_json_array_segment<T>
json_array_segment_at_define(
	basic_json_segmented_array<T, Allocator, First, Block> &array,
	::std::size_t index) noexcept
{
	return array.segment_at(index);
}

template <typename T, typename Allocator, ::std::size_t First, ::std::size_t Block>
[[nodiscard]] inline constexpr basic_json_array_segment<T const>
json_array_segment_at_define(
	basic_json_segmented_array<T, Allocator, First, Block> const &array,
	::std::size_t index) noexcept
{
	return array.segment_at(index);
}

/* Read-prefetch is opt-in at the allocator provenance layer.  In particular,
   std::allocator receives no promise here merely because it is the default. */
template <typename T, typename Allocator, ::std::size_t First, ::std::size_t Block>
	requires ::fast_io::prfch_cacheable_allocator_provenance<Allocator>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	::fast_io::io_type_t<
		basic_json_segmented_array<T, Allocator, First, Block>>) noexcept
{
	return {};
}

} // namespace fast_io::json
