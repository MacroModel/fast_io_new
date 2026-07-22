#pragma once

namespace fast_io::containers
{

template <typename T>
struct basic_hash_map_segment
{
	using value_type = T;
	T *base{};
	::std::size_t len{};

	[[nodiscard]] constexpr T *begin() const noexcept { return base; }
	[[nodiscard]] constexpr T *end() const noexcept
	{
		return len == 0u ? base : base + len;
	}
	[[nodiscard]] constexpr bool empty() const noexcept { return len == 0u; }
	[[nodiscard]] constexpr ::std::size_t size() const noexcept { return len; }
	[[nodiscard]] constexpr T &operator[](::std::size_t index) const noexcept
	{
		return base[index];
	}
};

/*
Insertion-ordered hash map for pointer-stable DOM members.

The value storage and lookup index deliberately have different layouts:

  * entries occupy stable, insertion-ordered segments sized to approximately
    256 bytes initially and 1 KiB thereafter (power-of-two element counts,
    clamped to 1..8 and first..64 respectively);
  * objects of at most sixteen members use a linear search and allocate no
    bucket table;
  * the seventeenth member builds a power-of-two, open-addressed index whose
    cells contain physical entry indexes, never entry pointers.

Insertion never relocates an existing entry, so references and iterators to
existing members remain valid.  Erasure destroys only the selected entry and
leaves a hole; iterators skip holes and every other reference remains valid.
`clear`, assignment, swap, and destruction invalidate every reference.
Interior holes are intentionally not reused because doing so would violate
insertion order.  A workload with sustained erase/insert churn should rebuild
the map (or call clear when all values can be discarded) to reclaim those
physical slots; this is the explicit cost of stable references plus ordered
iteration.

The non-standard `segment_count`/`segment_at` surface is an optimization
contract for serializers.  It exposes only fully-live insertion-order spans
and therefore reports zero segments after an interior erase.  A caller must
fall back to ordinary iteration when `has_erase_holes()` is true.  Erasing a
suffix does not create a persistent hole and retains the span fast path.

The table uses 0 for an empty bucket, 1 for an erased bucket, and physical
index + 2 for a live bucket.  Rehashing has a commit point: all hashes are
computed into replacement storage before the old table is released.  Entry
construction likewise precedes publication in both the live mask and bucket
table.  Thus a throwing hash, allocator, key, or mapped constructor cannot
make an unconstructed slot reachable.
*/
template <typename Key, typename T, typename Hash, typename KeyEqual,
		  typename Allocator>
class basic_hash_map
{
public:
	using key_type = Key;
	using mapped_type = T;
	using value_type = ::std::pair<key_type const, mapped_type>;
	using hasher = Hash;
	using key_equal = KeyEqual;
	using allocator_type = Allocator;
	using allocator_traits = ::std::allocator_traits<allocator_type>;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using reference = value_type &;
	using const_reference = value_type const &;
	using pointer = typename allocator_traits::pointer;
	using const_pointer = typename allocator_traits::const_pointer;

private:
	static_assert(::std::same_as<typename allocator_traits::value_type,
		value_type>);
	[[nodiscard]] static consteval size_type first_capacity_value() noexcept
	{
		auto count{static_cast<size_type>(256u / sizeof(value_type))};
		if (count == 0u)
		{
			return 1u;
		}
		count = ::std::bit_floor(count);
		return count < 8u ? count : static_cast<size_type>(8u);
	}
	[[nodiscard]] static consteval size_type regular_capacity_value() noexcept
	{
		auto count{static_cast<size_type>(1024u / sizeof(value_type))};
		if (count == 0u)
		{
			count = 1u;
		}
		else
		{
			count = ::std::bit_floor(count);
		}
		auto const first{first_capacity_value()};
		if (count < first)
		{
			count = first;
		}
		return count < 64u ? count : static_cast<size_type>(64u);
	}
	static inline constexpr size_type first_segment_capacity{
		first_capacity_value()};
	static inline constexpr size_type regular_segment_capacity{
		regular_capacity_value()};
	/* Linear lookup wins for the short keys and small object cardinalities
	   common in JSON.  Deferring the index through 16 members
	   also avoids building a bucket table for the ubiquitous 9-field record. */
	static inline constexpr size_type linear_lookup_limit{16u};
	static inline constexpr size_type empty_bucket{};
	static inline constexpr size_type erased_bucket{1u};

	using block_pointer = pointer;
	struct block_descriptor
	{
		block_pointer block{};
		::std::uint64_t live_mask{};
	};
	using descriptor_allocator =
		typename allocator_traits::template rebind_alloc<block_descriptor>;
	using descriptor_traits = ::std::allocator_traits<descriptor_allocator>;
	using descriptor_pointer = typename descriptor_traits::pointer;
	using bucket_allocator =
		typename allocator_traits::template rebind_alloc<size_type>;
	using bucket_traits = ::std::allocator_traits<bucket_allocator>;
	using bucket_pointer = typename bucket_traits::pointer;

	[[no_unique_address]] allocator_type allocator_{};
	[[no_unique_address]] hasher hash_{};
	[[no_unique_address]] key_equal equal_{};
	block_pointer first_block_{};
	::std::uint64_t first_live_mask_{};
	descriptor_pointer extra_blocks_{};
	size_type extra_block_count_{};
	size_type extra_block_capacity_{};
	bucket_pointer buckets_{};
	size_type bucket_count_{};
	size_type tombstone_count_{};
	size_type used_{};
	size_type size_{};

	[[noreturn]] static void throw_length_error()
	{
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		throw ::std::length_error("fast_io hash_map is too large");
#else
		::fast_io::fast_terminate();
#endif
	}

	[[nodiscard]] descriptor_allocator descriptor_alloc() const
	{
		return descriptor_allocator{allocator_};
	}
	[[nodiscard]] bucket_allocator bucket_alloc() const
	{
		return bucket_allocator{allocator_};
	}
	[[nodiscard]] block_descriptor *extra_raw() noexcept
	{
		return extra_block_capacity_ == 0u
			? nullptr
			: ::std::to_address(extra_blocks_);
	}
	[[nodiscard]] block_descriptor const *extra_raw() const noexcept
	{
		return extra_block_capacity_ == 0u
			? nullptr
			: ::std::to_address(extra_blocks_);
	}
	[[nodiscard]] size_type *bucket_raw() noexcept
	{
		return bucket_count_ == 0u ? nullptr : ::std::to_address(buckets_);
	}
	[[nodiscard]] size_type const *bucket_raw() const noexcept
	{
		return bucket_count_ == 0u ? nullptr : ::std::to_address(buckets_);
	}

	[[nodiscard]] value_type *slot_pointer(size_type index) noexcept
	{
		if (index < first_segment_capacity)
		{
			return ::std::to_address(first_block_) + index;
		}
		auto const tail{index - first_segment_capacity};
		auto const block_index{tail / regular_segment_capacity};
		auto const offset{tail % regular_segment_capacity};
		return ::std::to_address(extra_raw()[block_index].block) + offset;
	}
	[[nodiscard]] value_type const *slot_pointer(size_type index) const noexcept
	{
		if (index < first_segment_capacity)
		{
			return ::std::to_address(first_block_) + index;
		}
		auto const tail{index - first_segment_capacity};
		auto const block_index{tail / regular_segment_capacity};
		auto const offset{tail % regular_segment_capacity};
		return ::std::to_address(extra_raw()[block_index].block) + offset;
	}
	[[nodiscard]] bool occupied(size_type index) const noexcept
	{
		if (index < first_segment_capacity)
		{
			return (first_live_mask_ &
				(static_cast<::std::uint64_t>(1u) << index)) != 0u;
		}
		auto const tail{index - first_segment_capacity};
		auto const block_index{tail / regular_segment_capacity};
		auto const offset{tail % regular_segment_capacity};
		return (extra_raw()[block_index].live_mask &
			(static_cast<::std::uint64_t>(1u) << offset)) != 0u;
	}
	void set_occupied(size_type index) noexcept
	{
		if (index < first_segment_capacity)
		{
			first_live_mask_ |= static_cast<::std::uint64_t>(1u) << index;
			return;
		}
		auto const tail{index - first_segment_capacity};
		extra_raw()[tail / regular_segment_capacity].live_mask |=
			static_cast<::std::uint64_t>(1u) <<
			(tail % regular_segment_capacity);
	}
	void clear_occupied(size_type index) noexcept
	{
		if (index < first_segment_capacity)
		{
			first_live_mask_ &=
				~(static_cast<::std::uint64_t>(1u) << index);
			return;
		}
		auto const tail{index - first_segment_capacity};
		extra_raw()[tail / regular_segment_capacity].live_mask &=
			~(static_cast<::std::uint64_t>(1u) <<
			  (tail % regular_segment_capacity));
	}

	[[nodiscard]] size_type next_occupied(size_type index) const noexcept
	{
		if (used_ == size_)
		{
			return index < used_ ? index : used_;
		}
		while (index != used_ && !occupied(index))
		{
			++index;
		}
		return index;
	}
	[[nodiscard]] size_type previous_occupied(size_type index) const noexcept
	{
		while (index != 0u)
		{
			--index;
			if (occupied(index))
			{
				return index;
			}
		}
		return used_;
	}

	void reserve_extra_directory(size_type requested)
	{
		if (requested <= extra_block_capacity_)
		{
			return;
		}
		auto alloc{descriptor_alloc()};
		auto const maximum{descriptor_traits::max_size(alloc)};
		if (requested > maximum) [[unlikely]]
		{
			throw_length_error();
		}
		auto grown{extra_block_capacity_ == 0u
			? static_cast<size_type>(2u)
			: extra_block_capacity_};
		while (grown < requested)
		{
			if (grown > maximum / 2u)
			{
				grown = requested;
				break;
			}
			grown *= 2u;
		}
		auto replacement{descriptor_traits::allocate(alloc, grown)};
		auto *const replacement_raw{::std::to_address(replacement)};
		auto const *const old_raw{extra_raw()};
		size_type constructed{};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			for (; constructed != extra_block_count_; ++constructed)
			{
				descriptor_traits::construct(
					alloc, replacement_raw + constructed, old_raw[constructed]);
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			while (constructed != 0u)
			{
				--constructed;
				descriptor_traits::destroy(alloc,
					replacement_raw + constructed);
			}
			descriptor_traits::deallocate(alloc, replacement, grown);
			throw;
		}
#endif
		for (size_type index{}; index != extra_block_count_; ++index)
		{
			descriptor_traits::destroy(
				alloc, const_cast<block_descriptor *>(old_raw) + index);
		}
		if (extra_block_capacity_ != 0u)
		{
			descriptor_traits::deallocate(
				alloc, extra_blocks_, extra_block_capacity_);
		}
		extra_blocks_ = replacement;
		extra_block_capacity_ = grown;
	}

	void append_block()
	{
		if (first_block_ == block_pointer{})
		{
			if (first_segment_capacity > allocator_traits::max_size(allocator_))
				[[unlikely]]
			{
				throw_length_error();
			}
			first_block_ = allocator_traits::allocate(
				allocator_, first_segment_capacity);
			return;
		}
		if (regular_segment_capacity > allocator_traits::max_size(allocator_))
			[[unlikely]]
		{
			throw_length_error();
		}
		auto allocated{allocator_traits::allocate(
			allocator_, regular_segment_capacity)};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			reserve_extra_directory(extra_block_count_ + 1u);
			auto alloc{descriptor_alloc()};
			descriptor_traits::construct(alloc,
				extra_raw() + extra_block_count_, block_descriptor{allocated, 0u});
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			allocator_traits::deallocate(
				allocator_, allocated, regular_segment_capacity);
			throw;
		}
#endif
		++extra_block_count_;
	}

	void ensure_slot_capacity(size_type requested)
	{
		if (requested <= capacity())
		{
			return;
		}
		if (requested > max_size()) [[unlikely]]
		{
			throw_length_error();
		}
		while (capacity() < requested)
		{
			append_block();
		}
	}

	template <typename... Args>
	void construct_value(value_type *destination, Args &&...args)
	{
		if constexpr (requires(allocator_type &allocator,
			value_type *pointer_value) {
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
					allocator_traits::construct(allocator_, destination,
						::std::forward<decltype(unpacked)>(unpacked)...);
				},
				::std::move(construction_arguments));
		}
	}

	void destroy_live_entries() noexcept
	{
		for (size_type index{}; index != used_; ++index)
		{
			if (occupied(index))
			{
				allocator_traits::destroy(allocator_, slot_pointer(index));
			}
		}
		first_live_mask_ = 0u;
		for (size_type index{}; index != extra_block_count_; ++index)
		{
			extra_raw()[index].live_mask = 0u;
		}
		used_ = size_ = 0u;
	}

	void release_buckets() noexcept
	{
		if (bucket_count_ == 0u)
		{
			return;
		}
		auto alloc{bucket_alloc()};
		auto *const raw{bucket_raw()};
		for (size_type index{}; index != bucket_count_; ++index)
		{
			bucket_traits::destroy(alloc, raw + index);
		}
		bucket_traits::deallocate(alloc, buckets_, bucket_count_);
		buckets_ = bucket_pointer{};
		bucket_count_ = tombstone_count_ = 0u;
	}

	void release_storage() noexcept
	{
		destroy_live_entries();
		release_buckets();
		if (first_block_ != block_pointer{})
		{
			allocator_traits::deallocate(
				allocator_, first_block_, first_segment_capacity);
		}
		for (size_type index{}; index != extra_block_count_; ++index)
		{
			allocator_traits::deallocate(allocator_,
				extra_raw()[index].block, regular_segment_capacity);
		}
		auto descriptor_allocator_value{descriptor_alloc()};
		for (size_type index{}; index != extra_block_count_; ++index)
		{
			descriptor_traits::destroy(
				descriptor_allocator_value, extra_raw() + index);
		}
		if (extra_block_capacity_ != 0u)
		{
			descriptor_traits::deallocate(descriptor_allocator_value,
				extra_blocks_, extra_block_capacity_);
		}
		first_block_ = block_pointer{};
		extra_blocks_ = descriptor_pointer{};
		extra_block_count_ = extra_block_capacity_ = 0u;
	}

	void steal_storage(basic_hash_map &other) noexcept
	{
		first_block_ = ::std::exchange(other.first_block_, block_pointer{});
		first_live_mask_ = ::std::exchange(other.first_live_mask_, 0u);
		extra_blocks_ =
			::std::exchange(other.extra_blocks_, descriptor_pointer{});
		extra_block_count_ = ::std::exchange(other.extra_block_count_, 0u);
		extra_block_capacity_ =
			::std::exchange(other.extra_block_capacity_, 0u);
		buckets_ = ::std::exchange(other.buckets_, bucket_pointer{});
		bucket_count_ = ::std::exchange(other.bucket_count_, 0u);
		tombstone_count_ = ::std::exchange(other.tombstone_count_, 0u);
		used_ = ::std::exchange(other.used_, 0u);
		size_ = ::std::exchange(other.size_, 0u);
	}

	void swap_storage(basic_hash_map &other) noexcept
	{
		using ::std::swap;
		swap(first_block_, other.first_block_);
		swap(first_live_mask_, other.first_live_mask_);
		swap(extra_blocks_, other.extra_blocks_);
		swap(extra_block_count_, other.extra_block_count_);
		swap(extra_block_capacity_, other.extra_block_capacity_);
		swap(buckets_, other.buckets_);
		swap(bucket_count_, other.bucket_count_);
		swap(tombstone_count_, other.tombstone_count_);
		swap(used_, other.used_);
		swap(size_, other.size_);
	}

	[[nodiscard]] static constexpr size_type minimum_bucket_count_for(
		size_type requested) noexcept
	{
		size_type result{16u};
		while (result - result / 4u < requested)
		{
			if (result > ((::std::numeric_limits<size_type>::max)() >> 1u))
			{
				return 0u;
			}
			result <<= 1u;
		}
		return result;
	}

	void rehash(size_type requested)
	{
		requested = (::std::max)(requested, size_);
		auto const new_count{minimum_bucket_count_for(requested)};
		if (new_count == 0u) [[unlikely]]
		{
			throw_length_error();
		}
		if (new_count == bucket_count_ && tombstone_count_ == 0u)
		{
			return;
		}
		auto alloc{bucket_alloc()};
		if (new_count > bucket_traits::max_size(alloc)) [[unlikely]]
		{
			throw_length_error();
		}
		auto replacement{bucket_traits::allocate(alloc, new_count)};
		auto *const raw{::std::to_address(replacement)};
		size_type constructed{};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			for (; constructed != new_count; ++constructed)
			{
				bucket_traits::construct(alloc, raw + constructed,
					empty_bucket);
			}
			for (size_type index{}; index != used_; ++index)
			{
				if (!occupied(index))
				{
					continue;
				}
				auto position{static_cast<size_type>(
					hash_(slot_pointer(index)->first)) & (new_count - 1u)};
				while (raw[position] != empty_bucket)
				{
					position = (position + 1u) & (new_count - 1u);
				}
				raw[position] = index + 2u;
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			while (constructed != 0u)
			{
				--constructed;
				bucket_traits::destroy(alloc, raw + constructed);
			}
			bucket_traits::deallocate(alloc, replacement, new_count);
			throw;
		}
#endif
		release_buckets();
		buckets_ = replacement;
		bucket_count_ = new_count;
		tombstone_count_ = 0u;
	}

	void prepare_index_for_insert()
	{
		if (bucket_count_ == 0u)
		{
			if (size_ + 1u > linear_lookup_limit)
			{
				rehash(size_ + 1u);
			}
			return;
		}
		if ((size_ + tombstone_count_ + 1u) >
			bucket_count_ - bucket_count_ / 4u ||
			tombstone_count_ > size_)
		{
			rehash(size_ + 1u);
		}
	}

	template <typename Query>
	[[nodiscard]] size_type linear_find_index(Query const &query) const
	{
		for (size_type index{}; index != used_; ++index)
		{
			if (occupied(index) && equal_(slot_pointer(index)->first, query))
			{
				return index;
			}
		}
		return used_;
	}

	template <typename Query>
	[[nodiscard]] size_type indexed_find_index(
		Query const &query, size_type hash_value) const
	{
		auto position{hash_value & (bucket_count_ - 1u)};
		auto const *const raw{bucket_raw()};
		for (;;)
		{
			auto const encoded{raw[position]};
			if (encoded == empty_bucket)
			{
				return used_;
			}
			if (encoded != erased_bucket)
			{
				auto const index{encoded - 2u};
				if (equal_(slot_pointer(index)->first, query))
				{
					return index;
				}
			}
			position = (position + 1u) & (bucket_count_ - 1u);
		}
	}

	void publish_bucket(size_type entry_index, size_type hash_value) noexcept
	{
		if (bucket_count_ == 0u)
		{
			return;
		}
		auto position{hash_value & (bucket_count_ - 1u)};
		auto *const raw{bucket_raw()};
		size_type first_erased{bucket_count_};
		for (;;)
		{
			auto const encoded{raw[position]};
			if (encoded == empty_bucket)
			{
				auto const target{first_erased == bucket_count_
					? position
					: first_erased};
				raw[target] = entry_index + 2u;
				if (first_erased != bucket_count_)
				{
					--tombstone_count_;
				}
				return;
			}
			if (encoded == erased_bucket && first_erased == bucket_count_)
			{
				first_erased = position;
			}
			position = (position + 1u) & (bucket_count_ - 1u);
		}
	}

	void erase_bucket_for_index(size_type entry_index) noexcept
	{
		if (bucket_count_ == 0u)
		{
			return;
		}
		auto *const raw{bucket_raw()};
		auto const encoded{entry_index + 2u};
		for (size_type index{}; index != bucket_count_; ++index)
		{
			if (raw[index] == encoded)
			{
				raw[index] = erased_bucket;
				++tombstone_count_;
				return;
			}
		}
		::fast_io::fast_terminate();
	}

	template <typename KeyArgument, typename... Args>
	[[nodiscard]] size_type emplace_known_absent_index(
		KeyArgument &&key, bool hash_is_known, size_type hash_value,
		Args &&...args)
	{
		prepare_index_for_insert();
		if (bucket_count_ != 0u && !hash_is_known)
		{
			hash_value = static_cast<size_type>(hash_(key));
		}
		if (used_ > max_size() - 1u) [[unlikely]]
		{
			throw_length_error();
		}
		ensure_slot_capacity(used_ + 1u);
		auto *const destination{slot_pointer(used_)};
		construct_value(destination, ::std::piecewise_construct,
			::std::forward_as_tuple(::std::forward<KeyArgument>(key)),
			::std::forward_as_tuple(::std::forward<Args>(args)...));
		auto const inserted_index{used_++};
		set_occupied(inserted_index);
		++size_;
		publish_bucket(inserted_index, hash_value);
		return inserted_index;
	}

	template <bool Const>
	class iterator_impl
	{
		using owner_type = ::std::conditional_t<Const,
			basic_hash_map const, basic_hash_map>;
		owner_type *owner_{};
		size_type index_{};
		friend class basic_hash_map;
		template <bool>
		friend class iterator_impl;

		constexpr iterator_impl(owner_type *owner, size_type index) noexcept
			: owner_(owner), index_(index)
		{}

	public:
		using value_type = basic_hash_map::value_type;
		using difference_type = ::std::ptrdiff_t;
		using reference = ::std::conditional_t<Const,
			value_type const &, value_type &>;
		using pointer = ::std::conditional_t<Const,
			value_type const *, value_type *>;
		using iterator_category = ::std::bidirectional_iterator_tag;
		using iterator_concept = ::std::bidirectional_iterator_tag;

		constexpr iterator_impl() noexcept = default;
		template <bool OtherConst>
		constexpr iterator_impl(iterator_impl<OtherConst> const &other) noexcept
			requires(Const && !OtherConst)
			: owner_(other.owner_), index_(other.index_)
		{}

		[[nodiscard]] constexpr reference operator*() const noexcept
		{
			return *owner_->slot_pointer(index_);
		}
		[[nodiscard]] constexpr pointer operator->() const noexcept
		{
			return owner_->slot_pointer(index_);
		}
		constexpr iterator_impl &operator++() noexcept
		{
			index_ = owner_->next_occupied(index_ + 1u);
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
			index_ = owner_->previous_occupied(index_);
			return *this;
		}
		constexpr iterator_impl operator--(int) noexcept
		{
			auto copy{*this};
			--*this;
			return copy;
		}
		template <bool OtherConst>
		[[nodiscard]] constexpr bool operator==(
			iterator_impl<OtherConst> const &other) const noexcept
		{
			return owner_ == other.owner_ && index_ == other.index_;
		}
	};

public:
	using iterator = iterator_impl<false>;
	using const_iterator = iterator_impl<true>;
	using segment_type = basic_hash_map_segment<value_type>;
	using const_segment_type = basic_hash_map_segment<value_type const>;

	constexpr basic_hash_map()
		requires(::std::default_initializable<allocator_type> &&
			 ::std::default_initializable<hasher> &&
			 ::std::default_initializable<key_equal>)
	= default;

	constexpr explicit basic_hash_map(allocator_type const &allocator)
		: allocator_(allocator)
	{}

	constexpr basic_hash_map(hasher const &hash, key_equal const &equal,
		allocator_type const &allocator = allocator_type{})
		: allocator_(allocator), hash_(hash), equal_(equal)
	{}

	basic_hash_map(basic_hash_map const &other)
		: basic_hash_map(other,
			allocator_traits::select_on_container_copy_construction(
				other.allocator_))
	{}

	basic_hash_map(basic_hash_map const &other,
		allocator_type const &allocator)
		: allocator_(allocator), hash_(other.hash_), equal_(other.equal_)
	{
		reserve(other.size_);
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			for (auto const &entry : other)
			{
				try_emplace(entry.first, entry.second);
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

	basic_hash_map(basic_hash_map &&other) noexcept(
		::std::is_nothrow_move_constructible_v<allocator_type> &&
		::std::is_nothrow_move_constructible_v<hasher> &&
		::std::is_nothrow_move_constructible_v<key_equal>)
		: allocator_(::std::move(other.allocator_)),
		  hash_(::std::move(other.hash_)), equal_(::std::move(other.equal_))
	{
		steal_storage(other);
	}

	basic_hash_map(basic_hash_map &&other,
		allocator_type const &allocator)
		: allocator_(allocator), hash_(::std::move(other.hash_)),
		  equal_(::std::move(other.equal_))
	{
		if constexpr (allocator_traits::is_always_equal::value)
		{
			steal_storage(other);
		}
		else if (allocator_ == other.allocator_)
		{
			steal_storage(other);
		}
		else
		{
			reserve(other.size_);
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
			try
			{
#endif
				for (auto &entry : other)
				{
					try_emplace(entry.first, ::std::move(entry.second));
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
			other.clear();
		}
	}

	basic_hash_map &operator=(basic_hash_map const &other)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		allocator_type target_allocator{allocator_};
		if constexpr (
			allocator_traits::propagate_on_container_copy_assignment::value)
		{
			target_allocator = other.allocator_;
		}
		basic_hash_map replacement{other, target_allocator};
		release_storage();
		if constexpr (
			allocator_traits::propagate_on_container_copy_assignment::value)
		{
			allocator_ = target_allocator;
		}
		hash_ = ::std::move(replacement.hash_);
		equal_ = ::std::move(replacement.equal_);
		steal_storage(replacement);
		return *this;
	}

	basic_hash_map &operator=(basic_hash_map &&other)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		if constexpr (
			allocator_traits::propagate_on_container_move_assignment::value)
		{
			release_storage();
			allocator_ = ::std::move(other.allocator_);
			hash_ = ::std::move(other.hash_);
			equal_ = ::std::move(other.equal_);
			steal_storage(other);
		}
		else if constexpr (allocator_traits::is_always_equal::value)
		{
			release_storage();
			hash_ = ::std::move(other.hash_);
			equal_ = ::std::move(other.equal_);
			steal_storage(other);
		}
		else if (allocator_ == other.allocator_)
		{
			release_storage();
			hash_ = ::std::move(other.hash_);
			equal_ = ::std::move(other.equal_);
			steal_storage(other);
		}
		else
		{
			basic_hash_map replacement{::std::move(other), allocator_};
			release_storage();
			hash_ = ::std::move(replacement.hash_);
			equal_ = ::std::move(replacement.equal_);
			steal_storage(replacement);
		}
		return *this;
	}

	~basic_hash_map() noexcept { release_storage(); }

	[[nodiscard]] constexpr allocator_type get_allocator() const
	{
		return allocator_;
	}
	[[nodiscard]] constexpr hasher hash_function() const { return hash_; }
	[[nodiscard]] constexpr key_equal key_eq() const { return equal_; }
	[[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0u; }
	[[nodiscard]] constexpr size_type size() const noexcept { return size_; }
	[[nodiscard]] constexpr size_type capacity() const noexcept
	{
		return first_block_ != block_pointer{}
			? first_segment_capacity +
				extra_block_count_ * regular_segment_capacity
			: 0u;
	}
	[[nodiscard]] constexpr size_type max_size() const noexcept
	{
		auto const allocation_limit{allocator_traits::max_size(allocator_)};
		if (allocation_limit < first_segment_capacity)
		{
			return 0u;
		}
		if (allocation_limit < regular_segment_capacity)
		{
			return first_segment_capacity;
		}
		auto const absolute_limit{
			(::std::numeric_limits<size_type>::max)() - 2u};
		return absolute_limit;
	}

	[[nodiscard]] constexpr iterator begin() noexcept
	{
		return iterator{this, next_occupied(0u)};
	}
	[[nodiscard]] constexpr const_iterator begin() const noexcept
	{
		return const_iterator{this, next_occupied(0u)};
	}
	[[nodiscard]] constexpr const_iterator cbegin() const noexcept
	{
		return begin();
	}
	[[nodiscard]] constexpr iterator end() noexcept
	{
		return iterator{this, used_};
	}
	[[nodiscard]] constexpr const_iterator end() const noexcept
	{
		return const_iterator{this, used_};
	}
	[[nodiscard]] constexpr const_iterator cend() const noexcept
	{
		return end();
	}

	[[nodiscard]] constexpr bool has_erase_holes() const noexcept
	{
		return used_ != size_;
	}
	[[nodiscard]] constexpr size_type segment_count() const noexcept
	{
		if (used_ != size_ || used_ == 0u)
		{
			return 0u;
		}
		if (used_ <= first_segment_capacity)
		{
			return 1u;
		}
		auto const tail{used_ - first_segment_capacity};
		return 1u + tail / regular_segment_capacity +
			static_cast<size_type>(tail % regular_segment_capacity != 0u);
	}
	[[nodiscard]] segment_type segment_at(size_type index) noexcept
	{
		if (index == 0u)
		{
			return {::std::to_address(first_block_),
				(::std::min)(used_, first_segment_capacity)};
		}
		auto const begin_index{first_segment_capacity +
			(index - 1u) * regular_segment_capacity};
		return {::std::to_address(extra_raw()[index - 1u].block),
			(::std::min)(used_ - begin_index, regular_segment_capacity)};
	}
	[[nodiscard]] const_segment_type segment_at(size_type index) const noexcept
	{
		if (index == 0u)
		{
			return {::std::to_address(first_block_),
				(::std::min)(used_, first_segment_capacity)};
		}
		auto const begin_index{first_segment_capacity +
			(index - 1u) * regular_segment_capacity};
		return {::std::to_address(extra_raw()[index - 1u].block),
			(::std::min)(used_ - begin_index, regular_segment_capacity)};
	}

	template <typename Query>
		requires requires(hasher const &hash, key_equal const &equal,
			key_type const &key, Query const &query) {
			{ hash(query) } -> ::std::convertible_to<size_type>;
			{ equal(key, query) } -> ::std::convertible_to<bool>;
		}
	[[nodiscard]] iterator find(Query const &query)
	{
		size_type index{};
		if (bucket_count_ == 0u)
		{
			index = linear_find_index(query);
		}
		else
		{
			auto const hash_value{static_cast<size_type>(hash_(query))};
			index = indexed_find_index(query, hash_value);
		}
		return iterator{this, index};
	}

	template <typename Query>
		requires requires(hasher const &hash, key_equal const &equal,
			key_type const &key, Query const &query) {
			{ hash(query) } -> ::std::convertible_to<size_type>;
			{ equal(key, query) } -> ::std::convertible_to<bool>;
		}
	[[nodiscard]] const_iterator find(Query const &query) const
	{
		size_type index{};
		if (bucket_count_ == 0u)
		{
			index = linear_find_index(query);
		}
		else
		{
			auto const hash_value{static_cast<size_type>(hash_(query))};
			index = indexed_find_index(query, hash_value);
		}
		return const_iterator{this, index};
	}

	template <typename Query>
	[[nodiscard]] bool contains(Query const &query) const
		requires requires(basic_hash_map const &map) { map.find(query); }
	{
		return find(query) != end();
	}

	template <typename KeyArgument, typename... Args>
		requires(::std::constructible_from<key_type, KeyArgument &&>)
	::std::pair<iterator, bool> try_emplace(
		KeyArgument &&key, Args &&...args)
	{
		size_type found_index{};
		size_type hash_value{};
		bool hash_is_known{bucket_count_ != 0u};
		if (hash_is_known)
		{
			hash_value = static_cast<size_type>(hash_(key));
			found_index = indexed_find_index(key, hash_value);
		}
		else
		{
			found_index = linear_find_index(key);
		}
		if (found_index != used_)
		{
			return {iterator{this, found_index}, false};
		}
		auto const inserted_index{emplace_known_absent_index(
			::std::forward<KeyArgument>(key), hash_is_known, hash_value,
			::std::forward<Args>(args)...)};
		return {iterator{this, inserted_index}, true};
	}

	/*
	Trusted insertion primitive for parsers and tree-clone walkers which have
	already proved that the key is absent.  Violating that precondition creates
	a duplicate key and is therefore a caller bug; the ordinary public mutation
	surface should use try_emplace instead.
	*/
	template <typename KeyArgument, typename... Args>
		requires(::std::constructible_from<key_type, KeyArgument &&>)
	iterator emplace_unique_known_absent(KeyArgument &&key, Args &&...args)
	{
		auto const inserted_index{emplace_known_absent_index(
			::std::forward<KeyArgument>(key), false, 0u,
			::std::forward<Args>(args)...)};
		return iterator{this, inserted_index};
	}

	template <typename KeyArgument, typename MappedArgument>
		requires(::std::constructible_from<key_type, KeyArgument &&>)
	::std::pair<iterator, bool> emplace(
		KeyArgument &&key, MappedArgument &&mapped)
	{
		return try_emplace(::std::forward<KeyArgument>(key),
			::std::forward<MappedArgument>(mapped));
	}
	::std::pair<iterator, bool> emplace(value_type const &value)
	{
		return try_emplace(value.first, value.second);
	}
	::std::pair<iterator, bool> emplace(value_type &&value)
	{
		return try_emplace(value.first, ::std::move(value.second));
	}

	iterator erase(const_iterator position)
	{
		if (position.owner_ != this || position.index_ == used_)
			[[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		auto const index{position.index_};
		erase_bucket_for_index(index);
		allocator_traits::destroy(allocator_, slot_pointer(index));
		clear_occupied(index);
		--size_;
		while (used_ != 0u && !occupied(used_ - 1u))
		{
			--used_;
		}
		return iterator{this, next_occupied(index)};
	}
	iterator erase(iterator position)
	{
		return erase(const_iterator{position});
	}

	template <typename Query>
	size_type erase(Query const &query)
		requires requires(basic_hash_map &map) { map.find(query); }
	{
		auto found{find(query)};
		if (found == end())
		{
			return 0u;
		}
		erase(found);
		return 1u;
	}

	void clear() noexcept
	{
		destroy_live_entries();
		if (bucket_count_ != 0u)
		{
			auto *const raw{bucket_raw()};
			for (size_type index{}; index != bucket_count_; ++index)
			{
				raw[index] = empty_bucket;
			}
			tombstone_count_ = 0u;
		}
	}

	void reserve(size_type requested)
	{
		if (requested > max_size()) [[unlikely]]
		{
			throw_length_error();
		}
		ensure_slot_capacity(requested);
		if (requested > linear_lookup_limit)
		{
			rehash(requested);
		}
	}

	void swap(basic_hash_map &other) noexcept(
		::std::is_nothrow_swappable_v<hasher> &&
		::std::is_nothrow_swappable_v<key_equal> &&
		(!allocator_traits::propagate_on_container_swap::value ||
		 ::std::is_nothrow_swappable_v<allocator_type>))
	{
		if (this == ::std::addressof(other))
		{
			return;
		}
		using ::std::swap;
		if constexpr (allocator_traits::propagate_on_container_swap::value)
		{
			swap(allocator_, other.allocator_);
		}
		else if constexpr (!allocator_traits::is_always_equal::value)
		{
			if (!(allocator_ == other.allocator_)) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
		swap(hash_, other.hash_);
		swap(equal_, other.equal_);
		swap_storage(other);
	}

	friend void swap(basic_hash_map &left, basic_hash_map &right)
		noexcept(noexcept(left.swap(right)))
	{
		left.swap(right);
	}
};

template <typename Key, typename T, typename Hash, typename KeyEqual,
		  typename Allocator>
[[nodiscard]] constexpr ::std::size_t hash_map_segment_count_define(
	basic_hash_map<Key, T, Hash, KeyEqual, Allocator> const &map) noexcept
{
	return map.segment_count();
}

template <typename Key, typename T, typename Hash, typename KeyEqual,
		  typename Allocator>
[[nodiscard]] constexpr auto hash_map_segment_at_define(
	basic_hash_map<Key, T, Hash, KeyEqual, Allocator> &map,
	::std::size_t index) noexcept
{
	return map.segment_at(index);
}

template <typename Key, typename T, typename Hash, typename KeyEqual,
		  typename Allocator>
[[nodiscard]] constexpr auto hash_map_segment_at_define(
	basic_hash_map<Key, T, Hash, KeyEqual, Allocator> const &map,
	::std::size_t index) noexcept
{
	return map.segment_at(index);
}

} // namespace fast_io::containers
