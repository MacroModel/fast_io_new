#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "dom.h"
#include "parse.h"
#include "serialize.h"

namespace fast_io::json
{

/*
An arena allocation is deliberately represented by a four-byte registry
handle instead of a native pointer.  A pointer-sized stateful allocator grows
the ordinary 24-byte JSON node to 32 bytes on common 64-bit ABIs; the handle
keeps it at 24 bytes while the registry lookup remains outside the node.

Handle zero is never registered.  A live allocator handle may only be used
while its resource is alive, exactly like a pointer to a
std::pmr::memory_resource.  basic_arena_mutable_json enforces that lifetime and
is the preferred owner; the public resource/allocator pair is also useful for
allocator-aware extension containers whose lifetime is nested in a document.
*/
class json_arena_resource;

struct json_arena_options
{
	::std::size_t initial_chunk_size{64u * 1024u};
	::std::size_t maximum_regular_chunk_size{2u * 1024u * 1024u};
};

struct json_arena_statistics
{
	::std::size_t chunk_count{};
	::std::size_t allocation_calls{};
	::std::size_t used_bytes{};
	::std::size_t reserved_bytes{};
};

namespace details
{

[[noreturn]] inline void json_arena_allocation_failure()
{
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
	throw ::std::bad_alloc{};
#else
	::fast_io::fast_terminate();
#endif
}

/*
The fixed registry admits 65,535 simultaneously live arenas.  Registration
and release are cold operations protected by the mutex; allocate performs one
acquire load and then uses the resource's bump pointer without synchronization.
JSON mutation itself is not concurrently writable, so adding a lock to every
bump would provide no usable thread-safety guarantee.

The free list is stored in a fixed array so resource destruction is noexcept:
unregistering never allocates.  Slots are reused only after the owning JSON has
destroyed all allocator-aware objects.  Therefore no valid allocator can
observe an ABA reuse.
*/
class json_arena_registry
{
	inline static constexpr ::std::uint32_t slot_count{65536u};
	::std::array<::std::atomic<json_arena_resource *>, slot_count> slots_{};
	::std::array<::std::uint32_t, slot_count> free_next_{};
	::std::mutex mutex_{};
	::std::uint32_t next_handle_{1u};
	::std::uint32_t free_head_{};

	json_arena_registry() noexcept
	{
		for (auto &slot : slots_)
		{
			slot.store(nullptr, ::std::memory_order_relaxed);
		}
	}

public:
	json_arena_registry(json_arena_registry const &) = delete;
	json_arena_registry &operator=(json_arena_registry const &) = delete;

	[[nodiscard]] static json_arena_registry &instance()
	{
		/* Intentionally process-lifetime: resources may themselves have static
		   storage duration, so destroying the registry would create an otherwise
		   unorderable cross-translation-unit teardown dependency. */
		static auto *const registry{new json_arena_registry};
		return *registry;
	}

	[[nodiscard]] ::std::uint32_t register_resource(
		json_arena_resource *resource)
	{
		::std::lock_guard lock{mutex_};
		::std::uint32_t handle{};
		if (free_head_ != 0u)
		{
			handle = free_head_;
			free_head_ = free_next_[handle];
		}
		else
		{
			if (next_handle_ == slot_count) [[unlikely]]
			{
				json_arena_allocation_failure();
			}
			handle = next_handle_++;
		}
		slots_[handle].store(resource, ::std::memory_order_release);
		return handle;
	}

	void unregister_resource(::std::uint32_t handle,
							 json_arena_resource *resource) noexcept
	{
		if (handle == 0u || handle >= slot_count) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		::std::lock_guard lock{mutex_};
		if (slots_[handle].load(::std::memory_order_relaxed) != resource)
			[[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		slots_[handle].store(nullptr, ::std::memory_order_release);
		free_next_[handle] = free_head_;
		free_head_ = handle;
	}

	[[nodiscard]] json_arena_resource *find(
		::std::uint32_t handle) noexcept
	{
		if (handle == 0u || handle >= slot_count) [[unlikely]]
		{
			return nullptr;
		}
		return slots_[handle].load(::std::memory_order_acquire);
	}
};

[[nodiscard]] inline constexpr bool json_arena_power_of_two(
	::std::size_t value) noexcept
{
	return value != 0u && (value & (value - 1u)) == 0u;
}

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
inline constexpr ::std::size_t json_arena_default_new_alignment{
	static_cast<::std::size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__)};
#else
inline constexpr ::std::size_t json_arena_default_new_alignment{
	alignof(::std::max_align_t)};
#endif

} // namespace details

class json_arena_resource
{
	struct alignas(::std::max_align_t) chunk
	{
		chunk *previous{};
		::std::size_t capacity{};
		::std::size_t used{};
		::std::size_t allocation_alignment{};
	};

	chunk *current_{};
	json_arena_options options_{};
	::std::size_t next_chunk_size_{};
	json_arena_statistics statistics_{};
	::std::uint32_t handle_{};

	[[nodiscard]] static ::std::size_t normalize_initial_chunk_size(
		::std::size_t value) noexcept
	{
		constexpr ::std::size_t minimum{256u};
		return value < minimum ? minimum : value;
	}

	[[nodiscard]] static ::std::size_t normalize_maximum_chunk_size(
		::std::size_t initial, ::std::size_t maximum) noexcept
	{
		return maximum < initial ? initial : maximum;
	}

	[[nodiscard]] static void *allocate_block(
		::std::size_t bytes, ::std::size_t alignment)
	{
		if (alignment > details::json_arena_default_new_alignment)
		{
			return ::operator new(bytes, ::std::align_val_t{alignment});
		}
		return ::operator new(bytes);
	}

	static void deallocate_block(void *pointer,
								 ::std::size_t alignment) noexcept
	{
		if (alignment > details::json_arena_default_new_alignment)
		{
			::operator delete(pointer, ::std::align_val_t{alignment});
			return;
		}
		::operator delete(pointer);
	}

	[[nodiscard]] void *try_allocate_from_current(
		::std::size_t bytes, ::std::size_t alignment) noexcept
	{
		if (current_ == nullptr ||
			alignment > current_->allocation_alignment)
		{
			return nullptr;
		}
		auto *const data{reinterpret_cast<::std::byte *>(current_ + 1u)};
		auto *candidate{static_cast<void *>(data + current_->used)};
		auto space{current_->capacity - current_->used};
		auto *const aligned{::std::align(alignment, bytes, candidate, space)};
		if (aligned == nullptr)
		{
			return nullptr;
		}
		current_->used = static_cast<::std::size_t>(
							 static_cast<::std::byte *>(aligned) - data) +
						 bytes;
		return aligned;
	}

	void allocate_chunk(::std::size_t bytes, ::std::size_t alignment)
	{
		constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
		if (alignment - 1u > maximum - bytes) [[unlikely]]
		{
			details::json_arena_allocation_failure();
		}
		auto const required_capacity{bytes + alignment - 1u};
		auto capacity{next_chunk_size_ < required_capacity
						  ? required_capacity
						  : next_chunk_size_};
		if (capacity > maximum - sizeof(chunk)) [[unlikely]]
		{
			details::json_arena_allocation_failure();
		}
		auto const allocation_alignment{
			alignment < alignof(chunk) ? alignof(chunk) : alignment};
		auto *const storage{allocate_block(
			sizeof(chunk) + capacity, allocation_alignment)};
		auto *const created{::std::construct_at(
			static_cast<chunk *>(storage), chunk{current_, capacity, 0u,
												 allocation_alignment})};
		current_ = created;
		++statistics_.chunk_count;
		statistics_.reserved_bytes += capacity;

		if (next_chunk_size_ < options_.maximum_regular_chunk_size)
		{
			auto grown{next_chunk_size_};
			if (grown <= options_.maximum_regular_chunk_size / 2u)
			{
				grown *= 2u;
			}
			else
			{
				grown = options_.maximum_regular_chunk_size;
			}
			next_chunk_size_ = grown;
		}
	}

public:
	explicit json_arena_resource(json_arena_options options = {})
		: options_{normalize_initial_chunk_size(options.initial_chunk_size), 0u},
		  next_chunk_size_{options_.initial_chunk_size}
	{
		options_.maximum_regular_chunk_size =
			normalize_maximum_chunk_size(options_.initial_chunk_size,
										 options.maximum_regular_chunk_size);
		handle_ = details::json_arena_registry::instance().register_resource(this);
	}

	json_arena_resource(json_arena_resource const &) = delete;
	json_arena_resource(json_arena_resource &&) = delete;
	json_arena_resource &operator=(json_arena_resource const &) = delete;
	json_arena_resource &operator=(json_arena_resource &&) = delete;

	~json_arena_resource() noexcept
	{
		release();
		details::json_arena_registry::instance().unregister_resource(
			handle_, this);
	}

	[[nodiscard]] void *allocate_bytes(
		::std::size_t bytes, ::std::size_t alignment)
	{
		if (!details::json_arena_power_of_two(alignment)) [[unlikely]]
		{
			details::json_arena_allocation_failure();
		}
		if (bytes == 0u)
		{
			bytes = 1u;
		}
		auto *result{try_allocate_from_current(bytes, alignment)};
		if (result == nullptr)
		{
			allocate_chunk(bytes, alignment);
			result = try_allocate_from_current(bytes, alignment);
			if (result == nullptr) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
		++statistics_.allocation_calls;
		statistics_.used_bytes += bytes;
		return result;
	}

	/* Monotonic deallocation is intentionally a no-op.  Object destructors must
	   still run; basic_json does that before its owning resource is released. */
	void deallocate_bytes(void *, ::std::size_t,
						  ::std::size_t) noexcept
	{}

	/* All pointers obtained from this resource become invalid.  Callers using
	   the raw resource API must first destroy every allocated object. */
	void release() noexcept
	{
		while (current_ != nullptr)
		{
			auto *const previous{current_->previous};
			auto const alignment{current_->allocation_alignment};
			::std::destroy_at(current_);
			deallocate_block(current_, alignment);
			current_ = previous;
		}
		statistics_ = {};
		next_chunk_size_ = options_.initial_chunk_size;
	}

	[[nodiscard]] constexpr ::std::uint32_t handle() const noexcept
	{
		return handle_;
	}

	[[nodiscard]] constexpr json_arena_options options() const noexcept
	{
		return options_;
	}

	[[nodiscard]] constexpr json_arena_statistics statistics() const noexcept
	{
		return statistics_;
	}
};

template <typename T>
class json_arena_allocator
{
	template <typename>
	friend class json_arena_allocator;

	::std::uint32_t handle_{};

	[[nodiscard]] json_arena_resource *checked_resource() const
	{
		auto *const result{
			details::json_arena_registry::instance().find(handle_)};
		if (result == nullptr) [[unlikely]]
		{
			details::json_arena_allocation_failure();
		}
		return result;
	}

public:
	using value_type = T;
	using size_type = ::std::size_t;
	using difference_type = ::std::ptrdiff_t;
	using propagate_on_container_copy_assignment = ::std::false_type;
	using propagate_on_container_move_assignment = ::std::false_type;
	using propagate_on_container_swap = ::std::false_type;
	using is_always_equal = ::std::false_type;

	template <typename U>
	struct rebind
	{
		using other = json_arena_allocator<U>;
	};

	constexpr json_arena_allocator() noexcept = default;

	explicit constexpr json_arena_allocator(
		json_arena_resource &resource) noexcept : handle_(resource.handle())
	{}

	template <typename U>
	constexpr json_arena_allocator(
		json_arena_allocator<U> const &other) noexcept : handle_(other.handle_)
	{}

	[[nodiscard]] T *allocate(size_type count)
	{
		constexpr auto maximum{(::std::numeric_limits<size_type>::max)()};
		if (count > maximum / sizeof(T)) [[unlikely]]
		{
			details::json_arena_allocation_failure();
		}
		return static_cast<T *>(checked_resource()->allocate_bytes(
			count * sizeof(T), alignof(T)));
	}

	void deallocate(T *pointer, size_type count) noexcept
	{
		/* Do not even perform the registry lookup on destruction: monotonic
		   deallocation has no observable resource-side work. */
		static_cast<void>(pointer);
		static_cast<void>(count);
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept
	{
		return (::std::numeric_limits<size_type>::max)() / sizeof(T);
	}

	[[nodiscard]] json_arena_resource *resource() const noexcept
	{
		return details::json_arena_registry::instance().find(handle_);
	}

	[[nodiscard]] constexpr ::std::uint32_t handle() const noexcept
	{
		return handle_;
	}

	template <typename U>
	[[nodiscard]] friend constexpr bool operator==(
		json_arena_allocator left,
		json_arena_allocator<U> right) noexcept
	{
		return left.handle_ == right.handle_;
	}
};

static_assert(sizeof(json_arena_allocator<::std::byte>) == 4u,
			  "the JSON arena allocator handle must stay four bytes");

namespace details
{

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
struct basic_arena_mutable_json_types
{
	using node_type = ::fast_io::json::basic_json_node<Char, Number,
													   Integer, UInteger, ::fast_io::json::json_arena_allocator, Array>;
	using json_type = ::fast_io::json::basic_json<node_type>;
	using allocator_type = ::fast_io::json::json_arena_allocator<node_type>;
};

template <typename Json>
struct basic_arena_mutable_json_state
{
	using json_type = Json;
	using allocator_type = typename json_type::allocator_type;

	/* Declaration order is a lifetime invariant, not cosmetic.  Members are
	   destroyed in reverse order, so document recursively destroys every
	   allocator-aware object before arena releases its chunks and registry
	   handle. */
	::fast_io::json::json_arena_resource arena;
	json_type document;

	explicit basic_arena_mutable_json_state(
		::fast_io::json::json_arena_options options)
		: arena(options), document(allocator_type{arena})
	{}

	basic_arena_mutable_json_state(json_type const &source,
								   ::fast_io::json::json_arena_options options)
		: arena(options), document(source, allocator_type{arena})
	{}

	basic_arena_mutable_json_state(
		basic_arena_mutable_json_state const &) = delete;
	basic_arena_mutable_json_state &operator=(
		basic_arena_mutable_json_state const &) = delete;
};

} // namespace details

template <typename Char = char, typename Number = double,
		  typename Integer = ::std::int_least64_t,
		  typename UInteger = ::std::uint_least64_t,
		  template <typename, typename> class Array = basic_json_segmented_array>
class basic_arena_mutable_json
{
	using types_type = details::basic_arena_mutable_json_types<
		Char, Number, Integer, UInteger, Array>;
	using state_type = details::basic_arena_mutable_json_state<
		typename types_type::json_type>;

	::std::unique_ptr<state_type> state_;

	[[nodiscard]] static ::std::unique_ptr<state_type> make_state(
		json_arena_options options)
	{
		return ::std::make_unique<state_type>(options);
	}

public:
	using node_type = typename types_type::node_type;
	using json_type = typename types_type::json_type;
	using allocator_type = typename types_type::allocator_type;
	using char_type = typename json_type::char_type;
	using number_type = typename json_type::number_type;
	using integer_type = typename json_type::integer_type;
	using uinteger_type = typename json_type::uinteger_type;
	using string_type = typename json_type::string_type;
	using array_type = typename json_type::array_type;
	using object_type = typename json_type::object_type;
	using slice_type = typename json_type::slice_type;
	using const_slice_type = typename json_type::const_slice_type;

	basic_arena_mutable_json()
		: state_(make_state({}))
	{}

	explicit basic_arena_mutable_json(json_arena_options options)
		: state_(make_state(options))
	{}

	explicit basic_arena_mutable_json(json_type const &source,
									  json_arena_options options = {})
		: state_(::std::make_unique<state_type>(source, options))
	{}

	basic_arena_mutable_json(basic_arena_mutable_json const &other)
		: state_(::std::make_unique<state_type>(
			  other.document(), other.arena_options()))
	{}

	/* Allocate the source's replacement state before transferring ownership.
	   If allocation fails, `other` is unchanged.  After success both wrappers
	   remain fully usable, and no allocator handle ever points at a moved
	   resource object. */
	basic_arena_mutable_json(basic_arena_mutable_json &&other)
		: state_(make_state(other.arena_options()))
	{
		state_.swap(other.state_);
	}

	basic_arena_mutable_json &operator=(
		basic_arena_mutable_json const &other)
	{
		if (this != ::std::addressof(other))
		{
			basic_arena_mutable_json replacement{other};
			swap(replacement);
		}
		return *this;
	}

	basic_arena_mutable_json &operator=(
		basic_arena_mutable_json &&other)
	{
		if (this != ::std::addressof(other))
		{
			basic_arena_mutable_json replacement{::std::move(other)};
			swap(replacement);
		}
		return *this;
	}

	template <typename Value>
		requires(!::std::same_as<::std::remove_cvref_t<Value>,
								 basic_arena_mutable_json> &&
				 requires(json_type &document, Value &&value) {
					 document = ::std::forward<Value>(value);
				 })
	basic_arena_mutable_json &operator=(Value &&value)
	{
		state_->document = ::std::forward<Value>(value);
		return *this;
	}

	~basic_arena_mutable_json() = default;

	void swap(basic_arena_mutable_json &other) noexcept
	{
		state_.swap(other.state_);
	}

	friend void swap(basic_arena_mutable_json &left,
					 basic_arena_mutable_json &right) noexcept
	{
		left.swap(right);
	}

	[[nodiscard]] json_type &document() & noexcept
	{
		return state_->document;
	}

	[[nodiscard]] json_type const &document() const & noexcept
	{
		return state_->document;
	}
	json_type &document() && = delete;
	json_type const &document() const && = delete;

	[[nodiscard]] json_type *operator->() & noexcept
	{
		return ::std::addressof(state_->document);
	}

	[[nodiscard]] json_type const *operator->() const & noexcept
	{
		return ::std::addressof(state_->document);
	}
	json_type *operator->() && = delete;
	json_type const *operator->() const && = delete;

	[[nodiscard]] operator json_type &() & noexcept
	{
		return state_->document;
	}

	[[nodiscard]] operator json_type const &() const & noexcept
	{
		return state_->document;
	}

	operator json_type &&() && = delete;
	operator json_type const &() const && = delete;

	[[nodiscard]] slice_type slice() & noexcept
	{
		return state_->document.slice();
	}

	[[nodiscard]] const_slice_type slice() const & noexcept
	{
		return state_->document.slice();
	}

	const_slice_type slice() && = delete;
	const_slice_type slice() const && = delete;

	[[nodiscard]] constexpr json_kind kind() const noexcept
	{
		return state_->document.kind();
	}

	[[nodiscard]] constexpr bool undefined() const noexcept
	{
		return state_->document.undefined();
	}

	[[nodiscard]] constexpr bool null() const noexcept
	{
		return state_->document.null();
	}

	[[nodiscard]] constexpr bool boolean() const noexcept
	{
		return state_->document.boolean();
	}

	[[nodiscard]] constexpr bool number() const noexcept
	{
		return state_->document.number();
	}

	[[nodiscard]] constexpr bool integer() const noexcept
		requires(json_type::has_integer)
	{
		return state_->document.integer();
	}

	[[nodiscard]] constexpr bool uinteger() const noexcept
		requires(json_type::has_uinteger)
	{
		return state_->document.uinteger();
	}

	[[nodiscard]] constexpr bool string() const noexcept
	{
		return state_->document.string();
	}

	[[nodiscard]] constexpr bool array() const noexcept
	{
		return state_->document.is_array();
	}

	[[nodiscard]] constexpr bool object() const noexcept
	{
		return state_->document.is_object();
	}

	[[nodiscard]] constexpr bool is_undefined() const noexcept
	{
		return state_->document.is_undefined();
	}

	[[nodiscard]] constexpr bool is_null() const noexcept
	{
		return state_->document.is_null();
	}

	[[nodiscard]] constexpr bool is_boolean() const noexcept
	{
		return state_->document.is_boolean();
	}

	[[nodiscard]] constexpr bool is_number() const noexcept
	{
		return state_->document.is_number();
	}

	[[nodiscard]] constexpr bool is_integer() const noexcept
		requires(json_type::has_integer)
	{
		return state_->document.is_integer();
	}

	[[nodiscard]] constexpr bool is_uinteger() const noexcept
		requires(json_type::has_uinteger)
	{
		return state_->document.is_uinteger();
	}

	[[nodiscard]] constexpr bool is_string() const noexcept
	{
		return state_->document.is_string();
	}

	[[nodiscard]] constexpr bool is_array() const noexcept
	{
		return state_->document.is_array();
	}

	[[nodiscard]] constexpr bool is_object() const noexcept
	{
		return state_->document.is_object();
	}

	[[nodiscard]] constexpr ::std::size_t size() const noexcept
	{
		return state_->document.size();
	}

	[[nodiscard]] constexpr bool value_empty() const noexcept
	{
		return state_->document.value_empty();
	}

	[[nodiscard]] decltype(auto) get_boolean() &
	{
		return state_->document.get_boolean();
	}

	[[nodiscard]] decltype(auto) get_boolean() const &
	{
		return state_->document.get_boolean();
	}

	[[nodiscard]] decltype(auto) get_number() &
	{
		return state_->document.get_number();
	}

	[[nodiscard]] decltype(auto) get_number() const &
	{
		return state_->document.get_number();
	}

	[[nodiscard]] number_type as_number() const
	{
		return state_->document.as_number();
	}

	[[nodiscard]] decltype(auto) get_integer() &
		requires(json_type::has_integer)
	{
		return state_->document.get_integer();
	}

	[[nodiscard]] decltype(auto) get_integer() const &
		requires(json_type::has_integer)
	{
		return state_->document.get_integer();
	}

	[[nodiscard]] decltype(auto) get_uinteger() &
		requires(json_type::has_uinteger)
	{
		return state_->document.get_uinteger();
	}

	[[nodiscard]] decltype(auto) get_uinteger() const &
		requires(json_type::has_uinteger)
	{
		return state_->document.get_uinteger();
	}

	[[nodiscard]] decltype(auto) get_string() &
	{
		return state_->document.get_string();
	}

	[[nodiscard]] decltype(auto) get_string() const &
	{
		return state_->document.get_string();
	}

	[[nodiscard]] decltype(auto) get_array() &
	{
		return state_->document.get_array();
	}

	[[nodiscard]] decltype(auto) get_array() const &
	{
		return state_->document.get_array();
	}

	[[nodiscard]] decltype(auto) get_object() &
	{
		return state_->document.get_object();
	}

	[[nodiscard]] decltype(auto) get_object() const &
	{
		return state_->document.get_object();
	}

	template <typename Index>
	[[nodiscard]] decltype(auto) operator[](Index &&index) &
	{
		return state_->document[::std::forward<Index>(index)];
	}

	template <typename Index>
	[[nodiscard]] decltype(auto) operator[](Index &&index) const &
	{
		return state_->document[::std::forward<Index>(index)];
	}

	template <typename Index>
	decltype(auto) operator[](Index &&) && = delete;
	template <typename Index>
	decltype(auto) operator[](Index &&) const && = delete;

	template <typename Index>
	[[nodiscard]] decltype(auto) at(Index &&index) &
	{
		return state_->document.at(::std::forward<Index>(index));
	}

	template <typename Index>
	[[nodiscard]] decltype(auto) at(Index &&index) const &
	{
		return state_->document.at(::std::forward<Index>(index));
	}

	template <typename Key>
	[[nodiscard]] decltype(auto) find(Key const &key) &
	{
		return state_->document.find(key);
	}

	template <typename Key>
	[[nodiscard]] decltype(auto) find(Key const &key) const &
	{
		return state_->document.find(key);
	}

	template <typename Key>
	[[nodiscard]] bool contains(Key const &key) const
	{
		return state_->document.contains(key);
	}

	[[nodiscard]] decltype(auto) front() &
	{
		return state_->document.front();
	}

	[[nodiscard]] decltype(auto) front() const &
	{
		return state_->document.front();
	}

	[[nodiscard]] decltype(auto) back() &
	{
		return state_->document.back();
	}

	[[nodiscard]] decltype(auto) back() const &
	{
		return state_->document.back();
	}

	[[nodiscard]] decltype(auto) as_array() &
	{
		return state_->document.as_array();
	}

	[[nodiscard]] decltype(auto) as_array() const &
	{
		return state_->document.as_array();
	}

	[[nodiscard]] decltype(auto) as_object() &
	{
		return state_->document.as_object();
	}

	[[nodiscard]] decltype(auto) as_object() const &
	{
		return state_->document.as_object();
	}

	template <typename Value>
	decltype(auto) emplace_back(Value &&value) &
	{
		return state_->document.emplace_back(::std::forward<Value>(value));
	}

	template <typename Value>
	void push_back(Value &&value) &
	{
		state_->document.push_back(::std::forward<Value>(value));
	}

	void pop_back() &
	{
		state_->document.pop_back();
	}

	template <typename Position, typename Value>
	decltype(auto) insert(Position position, Value &&value) &
	{
		return state_->document.insert(
			position, ::std::forward<Value>(value));
	}

	template <typename Key, typename... Values>
	decltype(auto) try_emplace(Key &&key, Values &&...values) &
	{
		return state_->document.try_emplace(::std::forward<Key>(key),
											::std::forward<Values>(values)...);
	}

	template <typename Key, typename Value>
	decltype(auto) insert_or_assign(Key const &key, Value &&value) &
	{
		return state_->document.insert_or_assign(
			key, ::std::forward<Value>(value));
	}

	template <typename Index>
	decltype(auto) erase(Index &&index) &
	{
		return state_->document.erase(::std::forward<Index>(index));
	}

	void resize(typename array_type::size_type requested) &
	{
		state_->document.resize(requested);
	}

	void reserve(typename array_type::size_type requested) &
	{
		state_->document.reserve(requested);
	}

	/* clear/erase on the mutable DOM run destructors, but allocator deallocation
	   is monotonic and therefore retains the bytes for the arena lifetime. */
	void clear()
	{
		state_->document.clear();
	}

	/* Destroy the complete document first, then release all chunks.  The root
	   node remains bound to the still-live arena handle and can be reused. */
	void reset() noexcept
	{
		state_->document.reset();
		state_->arena.release();
	}

	/* Release storage only when no root container/string remains alive. */
	[[nodiscard]] bool release_unused() noexcept
	{
		if (!state_->document.undefined())
		{
			return false;
		}
		state_->arena.release();
		return true;
	}

	[[nodiscard]] constexpr json_arena_options arena_options() const noexcept
	{
		return state_->arena.options();
	}

	[[nodiscard]] constexpr json_arena_statistics arena_statistics() const noexcept
	{
		return state_->arena.statistics();
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept
	{
		return allocator_type{state_->arena};
	}

	[[nodiscard]] constexpr json_arena_resource &arena_resource() & noexcept
	{
		return state_->arena;
	}

	[[nodiscard]] constexpr json_arena_resource const &arena_resource() const & noexcept
	{
		return state_->arena;
	}
	json_arena_resource &arena_resource() && = delete;
	json_arena_resource const &arena_resource() const && = delete;
};

using arena_mutable_json = basic_arena_mutable_json<>;

static_assert(sizeof(arena_mutable_json::node_type) == sizeof(mutable_json_node),
			  "the arena allocator must not enlarge the default mutable JSON node");

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array,
		  ::fast_io::details::character input_char_type>
[[nodiscard]] inline json_parse_result<input_char_type const *>
try_parse_arena_mutable_json(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &destination,
	input_char_type const *first, input_char_type const *last,
	json_parse_options options = {})
{
	using wrapper_type = basic_arena_mutable_json<
		Char, Number, Integer, UInteger, Array>;
	wrapper_type replacement{destination.arena_options()};
	auto result{::fast_io::json::try_parse_json(
		replacement.document(), first, last, options)};
	if (result)
	{
		destination.swap(replacement);
	}
	return result;
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array,
		  typename input_char_type, typename traits_type>
/* The parser itself constrains the supported character domain. */
[[nodiscard]] inline json_parse_result<input_char_type const *>
try_parse_arena_mutable_json(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &destination,
	::std::basic_string_view<input_char_type, traits_type> input,
	json_parse_options options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return try_parse_arena_mutable_json(
		destination, first, last, options);
}

/* Preserve the familiar non-throwing parser spelling for an existing arena
   owner.  The more explicit name remains useful in generic code which wants
   to make the ownership policy visible. */
template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array,
		  ::fast_io::details::character input_char_type>
[[nodiscard]] inline json_parse_result<input_char_type const *>
try_parse_json(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &destination,
	input_char_type const *first, input_char_type const *last,
	json_parse_options options = {})
{
	return try_parse_arena_mutable_json(
		destination, first, last, options);
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array,
		  typename input_char_type, typename traits_type>
[[nodiscard]] inline json_parse_result<input_char_type const *>
try_parse_json(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &destination,
	::std::basic_string_view<input_char_type, traits_type> input,
	json_parse_options options = {})
{
	return try_parse_arena_mutable_json(destination, input, options);
}

template <typename Char = char, typename Number = double,
		  typename Integer = ::std::int_least64_t,
		  typename UInteger = ::std::uint_least64_t,
		  template <typename, typename> class Array = basic_json_segmented_array,
		  ::fast_io::details::character input_char_type>
[[nodiscard]] inline basic_arena_mutable_json<
	Char, Number, Integer, UInteger, Array>
parse_arena_mutable_json(input_char_type const *first,
						 input_char_type const *last, json_parse_options parse_options = {},
						 json_arena_options arena_options = {})
{
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> result{
		arena_options};
	auto const parsed{::fast_io::json::try_parse_json(
		result.document(), first, last, parse_options)};
	if (!parsed)
	{
		throw_json_error(parsed.code);
	}
	return result;
}

template <typename Char = char, typename Number = double,
		  typename Integer = ::std::int_least64_t,
		  typename UInteger = ::std::uint_least64_t,
		  template <typename, typename> class Array = basic_json_segmented_array,
		  typename input_char_type, typename traits_type>
[[nodiscard]] inline basic_arena_mutable_json<
	Char, Number, Integer, UInteger, Array>
parse_arena_mutable_json(
	::std::basic_string_view<input_char_type, traits_type> input,
	json_parse_options parse_options = {},
	json_arena_options arena_options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return parse_arena_mutable_json<Char, Number, Integer, UInteger, Array>(
		first, last, parse_options, arena_options);
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto make_json_print_view(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> const &value,
	json_serialize_options options = {}) noexcept
{
	return ::fast_io::json::make_json_print_view(value.document(), options);
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto make_json_print_view(basic_arena_mutable_json<
							  Char, Number, Integer, UInteger, Array> &&,
						  json_serialize_options = {}) = delete;

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto make_json_print_view(basic_arena_mutable_json<
							  Char, Number, Integer, UInteger, Array> const &&,
						  json_serialize_options = {}) = delete;

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t,
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> const &value) noexcept
{
	return ::fast_io::json::make_json_print_view(value.document());
}

template <typename arena_json_type>
struct arena_json_scan_proxy
{
	using manip_tag = ::fast_io::manip_tag_t;
	arena_json_type *value{};
	json_parse_options options{};
};

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto scan_arena_mutable_json(
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &value,
	json_parse_options options = {}) noexcept
{
	using owner_type = basic_arena_mutable_json<
		Char, Number, Integer, UInteger, Array>;
	return arena_json_scan_proxy<owner_type>{
		::std::addressof(value), options};
}

template <::fast_io::details::character char_type, typename arena_json_type>
[[nodiscard]] inline ::fast_io::parse_result<char_type const *>
scan_contiguous_define(
	::fast_io::io_reserve_type_t<char_type,
								 arena_json_scan_proxy<arena_json_type>>,
	char_type const *first, char_type const *last,
	arena_json_scan_proxy<arena_json_type> proxy)
{
	auto const result{try_parse_arena_mutable_json(
		*proxy.value, first, last, proxy.options)};
	if (result)
	{
		return {result.iter, ::fast_io::parse_code::ok};
	}
	switch (result.code)
	{
	case json_errc::number_overflow:
	case json_errc::integer_overflow:
	case json_errc::uinteger_overflow:
		return {result.iter, ::fast_io::parse_code::overflow};
	case json_errc::unexpected_end:
		return {result.iter, ::fast_io::parse_code::end_of_file};
	default:
		return {result.iter, ::fast_io::parse_code::invalid};
	}
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto scan_alias_define(
	::fast_io::io_alias_t,
	basic_arena_mutable_json<Char, Number, Integer, UInteger, Array> &value) noexcept
{
	using owner_type = basic_arena_mutable_json<
		Char, Number, Integer, UInteger, Array>;
	return arena_json_scan_proxy<owner_type>{::std::addressof(value), {}};
}

} // namespace fast_io::json

namespace fast_io::manipulators
{

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto json(
	::fast_io::json::basic_arena_mutable_json<
		Char, Number, Integer, UInteger, Array> const &value,
	::fast_io::json::json_serialize_options options = {}) noexcept
{
	return ::fast_io::json::make_json_print_view(value, options);
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto json(::fast_io::json::basic_arena_mutable_json<
			  Char, Number, Integer, UInteger, Array> &&,
		  ::fast_io::json::json_serialize_options = {}) = delete;

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto json(::fast_io::json::basic_arena_mutable_json<
			  Char, Number, Integer, UInteger, Array> const &&,
		  ::fast_io::json::json_serialize_options = {}) = delete;

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
[[nodiscard]] inline constexpr auto pretty_json(
	::fast_io::json::basic_arena_mutable_json<
		Char, Number, Integer, UInteger, Array> const &value,
	::std::size_t indent = 2u) noexcept
{
	::fast_io::json::json_serialize_options options{};
	options.pretty = true;
	options.indent_width = indent;
	return ::fast_io::json::make_json_print_view(value, options);
}

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto pretty_json(::fast_io::json::basic_arena_mutable_json<
					 Char, Number, Integer, UInteger, Array> &&,
				 ::std::size_t = 2u) = delete;

template <typename Char, typename Number, typename Integer,
		  typename UInteger, template <typename, typename> class Array>
auto pretty_json(::fast_io::json::basic_arena_mutable_json<
					 Char, Number, Integer, UInteger, Array> const &&,
				 ::std::size_t = 2u) = delete;

} // namespace fast_io::manipulators
