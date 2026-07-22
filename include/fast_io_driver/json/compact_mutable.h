#pragma once

/*
 * Compact, explicitly arena-owned mutable JSON DOM.
 *
 * The ordinary mutable_json favors allocator-generic C++ value semantics.
 * This representation instead follows the document model used by high-speed
 * C JSON libraries: every value is a stable 24-byte record in a geometrically
 * growing arena and containers link those records without relocating them.
 * The type is deliberately opt-in because erasing or replacing a subtree does
 * not reclaim arena storage before the owning document is destroyed/reset.
 */

#include "../../fast_io_dsal/hash_map.h"
#include "parse.h"
#include "serialize.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace fast_io::json
{

class compact_mutable_json;
class compact_json_ref;
class compact_const_json_ref;
class compact_json_array_iterator;
class compact_const_json_array_iterator;
class compact_json_object_iterator;
class compact_const_json_object_iterator;

namespace details
{

inline constexpr ::std::uint_least64_t compact_kind_mask{0x0fu};
inline constexpr ::std::uint_least64_t compact_direct_string_mask{0x10u};
inline constexpr ::std::uint_least64_t compact_validated_string_mask{0x20u};
inline constexpr ::std::uint_least64_t compact_lookup_mask{0xc0u};
inline constexpr unsigned compact_lookup_shift{6u};
inline constexpr unsigned compact_length_shift{8u};
inline constexpr ::std::size_t compact_linear_lookup_limit{16u};
inline constexpr ::std::size_t compact_eager_index_size{48u};

struct compact_json_node;
struct compact_json_container;

union compact_json_payload
{
	bool boolean;
	::std::int_least64_t integer;
	::std::uint_least64_t uinteger;
	double number;
	char const *string;
	compact_json_container *container;
	void *pointer;

	constexpr compact_json_payload() noexcept : uinteger{} {}
};

struct compact_json_node
{
	::std::uint_least64_t tag{};
	compact_json_payload payload{};
	compact_json_node *next{};
};

static_assert(sizeof(compact_json_node) == 24u,
	"the compact mutable ABI requires a 24-byte value record");
static_assert(alignof(compact_json_node) == alignof(void *));
static_assert(::std::is_trivially_destructible_v<compact_json_node>);

using compact_object_index = ::fast_io::hash_map<
	::std::string_view, compact_json_node *,
	::fast_io::json::detail::basic_json_string_hash<char>,
	::fast_io::json::detail::basic_json_string_equal<char>>;

struct compact_json_index_record
{
	compact_json_index_record *next{};
	compact_object_index index{};
};

struct compact_json_container
{
	compact_json_node *tail{};
	compact_json_index_record *index{};
};

[[nodiscard]] inline constexpr ::std::uint_least64_t compact_make_tag(
	json_kind kind, ::std::size_t length = 0u,
	::std::uint_least64_t flags = 0u) noexcept
{
	return (static_cast<::std::uint_least64_t>(length) <<
			compact_length_shift) |
		flags | static_cast<::std::uint_least64_t>(kind);
}

[[nodiscard]] inline constexpr json_kind compact_node_kind(
	compact_json_node const *value) noexcept
{
	return static_cast<json_kind>(value->tag & compact_kind_mask);
}

[[nodiscard]] inline constexpr ::std::size_t compact_node_length(
	compact_json_node const *value) noexcept
{
	return static_cast<::std::size_t>(value->tag >> compact_length_shift);
}

inline constexpr void compact_set_node_length(
	compact_json_node *value, ::std::size_t length) noexcept
{
	value->tag = (value->tag & 0xffu) |
		(static_cast<::std::uint_least64_t>(length) << compact_length_shift);
}

[[nodiscard]] inline constexpr bool compact_is_container(
	compact_json_node const *value) noexcept
{
	auto const kind{compact_node_kind(value)};
	return kind == json_kind::array || kind == json_kind::object;
}

[[nodiscard]] inline constexpr ::std::string_view compact_string_view(
	compact_json_node const *value) noexcept
{
	return {value->payload.string, compact_node_length(value)};
}

[[noreturn]] inline void compact_type_error(json_errc code)
{
	::fast_io::json::throw_json_error(code);
}

/* A typed arena never relocates an allocated object.  New chunks grow up to a
   bounded regular size, keeping both short-document locality and predictable
   large-document allocation counts. */
template <typename value_type, ::std::size_t initial_capacity,
	::std::size_t maximum_capacity>
class compact_typed_arena
{
	struct chunk
	{
		chunk *previous{};
		::std::size_t capacity{};
		::std::size_t used{};

		[[nodiscard]] value_type *begin() noexcept
		{
			return reinterpret_cast<value_type *>(this + 1u);
		}
	};

	chunk *current_{};
	::std::size_t next_capacity_{initial_capacity};
	::std::size_t used_{};
	::std::size_t reserved_bytes_{};
	::std::size_t allocation_calls_{};

	void add_chunk(::std::size_t minimum)
	{
		auto capacity{next_capacity_ < minimum ? minimum : next_capacity_};
		if (next_capacity_ < maximum_capacity)
		{
			next_capacity_ *= 2u;
			if (maximum_capacity < next_capacity_)
			{
				next_capacity_ = maximum_capacity;
			}
		}
		if ((::std::numeric_limits<::std::size_t>::max)() /
				sizeof(value_type) < capacity) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		auto const bytes{sizeof(chunk) + capacity * sizeof(value_type)};
		auto *const created{static_cast<chunk *>(
			::fast_io::native_global_allocator::allocate(bytes))};
		::new (created) chunk{current_, capacity, 0u};
		current_ = created;
		reserved_bytes_ += bytes;
		++allocation_calls_;
	}

public:
	compact_typed_arena() = default;
	compact_typed_arena(compact_typed_arena const &) = delete;
	compact_typed_arena &operator=(compact_typed_arena const &) = delete;

	~compact_typed_arena()
	{
		while (current_ != nullptr)
		{
			auto *const previous{current_->previous};
			current_->~chunk();
			::fast_io::native_global_allocator::deallocate(current_);
			current_ = previous;
		}
	}

	[[nodiscard]] value_type *allocate(::std::size_t count = 1u)
	{
		if (current_ == nullptr || current_->capacity - current_->used < count)
		{
			add_chunk(count);
		}
		auto *const result{current_->begin() + current_->used};
		current_->used += count;
		used_ += count;
		for (::std::size_t index{}; index != count; ++index)
		{
			::new (result + index) value_type{};
		}
		return result;
	}

	[[nodiscard]] constexpr ::std::size_t used() const noexcept { return used_; }
	[[nodiscard]] constexpr ::std::size_t reserved_bytes() const noexcept
	{
		return reserved_bytes_;
	}
	[[nodiscard]] constexpr ::std::size_t allocation_calls() const noexcept
	{
		return allocation_calls_;
	}
};

class compact_string_arena
{
	struct chunk
	{
		chunk *previous{};
		::std::size_t capacity{};
		::std::size_t used{};

		[[nodiscard]] char *begin() noexcept
		{
			return reinterpret_cast<char *>(this + 1u);
		}
	};

	chunk *current_{};
	::std::size_t next_capacity_{4096u};
	::std::size_t used_{};
	::std::size_t reserved_bytes_{};
	::std::size_t allocation_calls_{};

	void add_chunk(::std::size_t minimum)
	{
		auto capacity{next_capacity_ < minimum ? minimum : next_capacity_};
		constexpr ::std::size_t maximum_capacity{1024u * 1024u};
		if (next_capacity_ < maximum_capacity)
		{
			next_capacity_ *= 2u;
			if (maximum_capacity < next_capacity_)
			{
				next_capacity_ = maximum_capacity;
			}
		}
		if ((::std::numeric_limits<::std::size_t>::max)() - sizeof(chunk) <
			capacity) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		auto const bytes{sizeof(chunk) + capacity};
		auto *const created{static_cast<chunk *>(
			::fast_io::native_global_allocator::allocate(bytes))};
		::new (created) chunk{current_, capacity, 0u};
		current_ = created;
		reserved_bytes_ += bytes;
		++allocation_calls_;
	}

public:
	compact_string_arena() = default;
	compact_string_arena(compact_string_arena const &) = delete;
	compact_string_arena &operator=(compact_string_arena const &) = delete;

	~compact_string_arena()
	{
		while (current_ != nullptr)
		{
			auto *const previous{current_->previous};
			current_->~chunk();
			::fast_io::native_global_allocator::deallocate(current_);
			current_ = previous;
		}
	}

	[[nodiscard]] char *allocate(::std::size_t count)
	{
		if (current_ == nullptr || current_->capacity - current_->used < count)
		{
			add_chunk(count);
		}
		auto *const result{current_->begin() + current_->used};
		current_->used += count;
		used_ += count;
		return result;
	}

	[[nodiscard]] char const *copy(::std::string_view input)
	{
		auto *const result{allocate(input.size() + 1u)};
		if (!input.empty())
		{
			::std::memcpy(result, input.data(), input.size());
		}
		result[input.size()] = '\0';
		return result;
	}

	[[nodiscard]] constexpr ::std::size_t used() const noexcept { return used_; }
	[[nodiscard]] constexpr ::std::size_t reserved_bytes() const noexcept
	{
		return reserved_bytes_;
	}
	[[nodiscard]] constexpr ::std::size_t allocation_calls() const noexcept
	{
		return allocation_calls_;
	}
};

struct compact_json_storage
{
	compact_typed_arena<compact_json_node, 256u, 65536u> nodes{};
	compact_typed_arena<compact_json_container, 64u, 16384u> containers{};
	compact_string_arena strings{};
	compact_json_index_record *indexes{};
	compact_json_node *root{};

	compact_json_storage()
	{
		root = nodes.allocate();
		root->tag = compact_make_tag(json_kind::undefined);
	}

	compact_json_storage(compact_json_storage const &) = delete;
	compact_json_storage &operator=(compact_json_storage const &) = delete;

	~compact_json_storage()
	{
		while (indexes != nullptr)
		{
			auto *const next{indexes->next};
			::std::destroy_at(indexes);
			::fast_io::native_typed_global_allocator<
				compact_json_index_record>::deallocate(indexes);
			indexes = next;
		}
	}

	[[nodiscard]] compact_json_node *allocate_node(
		json_kind kind = json_kind::undefined,
		::std::size_t length = 0u,
		::std::uint_least64_t flags = 0u)
	{
		auto *const result{nodes.allocate()};
		result->tag = compact_make_tag(kind, length, flags);
		return result;
	}

	[[nodiscard]] compact_json_container *allocate_container()
	{
		return containers.allocate();
	}
};

[[nodiscard]] inline bool compact_valid_utf8(::std::string_view input) noexcept
{
	auto position{::std::size_t{}};
	while (position != input.size())
	{
		auto const decoded{decode_json_code_point(
			input.data(), input.size(), position)};
		if (decoded.status != unicode_decode_status::ok)
		{
			return false;
		}
		position = decoded.next;
	}
	return true;
}

[[nodiscard]] inline bool compact_string_needs_escape(
	::std::string_view input) noexcept
{
	for (auto const character : input)
	{
		auto const value{static_cast<unsigned char>(character)};
		if (value < 0x20u || value == 0x22u || value == 0x5cu)
		{
			return true;
		}
	}
	return false;
}

class compact_storage_access
{
	compact_json_storage *storage_{};

	[[nodiscard]] static compact_json_container *require_container(
		compact_json_node *node, json_kind kind, json_errc error)
	{
		if (compact_node_kind(node) != kind) [[unlikely]]
		{
			compact_type_error(error);
		}
		return node->payload.container;
	}

	[[nodiscard]] compact_json_node *linear_object_find_key(
		compact_json_node *object, ::std::string_view key) const noexcept
	{
		auto remaining{compact_node_length(object)};
		if (remaining == 0u)
		{
			return nullptr;
		}
		auto *current{object->payload.container->tail->next->next};
		while (remaining-- != 0u)
		{
			if (compact_string_view(current) == key)
			{
				return current;
			}
			current = current->next->next;
		}
		return nullptr;
	}

	void build_object_index(compact_json_node *object)
	{
		auto *const record_storage{
			::fast_io::native_typed_global_allocator<
				compact_json_index_record>::allocate(1u)};
		::std::construct_at(record_storage);
		struct record_guard
		{
			compact_json_index_record *pointer{};
			~record_guard()
			{
				if (pointer != nullptr)
				{
					::std::destroy_at(pointer);
					::fast_io::native_typed_global_allocator<
						compact_json_index_record>::deallocate(pointer);
				}
			}
		} guard{record_storage};
		auto const length{compact_node_length(object)};
		record_storage->index.reserve(length);
		if (length != 0u)
		{
			auto *key{object->payload.container->tail->next->next};
			for (::std::size_t remaining{length}; remaining != 0u; --remaining)
			{
				record_storage->index.emplace_unique_known_absent(
					compact_string_view(key), key->next);
				key = key->next->next;
			}
		}
		record_storage->next = storage_->indexes;
		storage_->indexes = record_storage;
		guard.pointer = nullptr;
		object->payload.container->index = storage_->indexes;
	}

	[[nodiscard]] compact_json_node *object_find_value_impl(
		compact_json_node *object, ::std::string_view key,
		bool learn_lookup)
	{
		auto *const container{require_container(
			object, json_kind::object, json_errc::not_object)};
		if (container->index != nullptr)
		{
			auto found{container->index->index.find(key)};
			return found == container->index->index.end()
				? nullptr
				: found->second;
		}

		auto const length{compact_node_length(object)};
		auto const lookups{static_cast<unsigned>(
			(object->tag & compact_lookup_mask) >> compact_lookup_shift)};
		if (compact_eager_index_size <= length ||
			(compact_linear_lookup_limit <= length && lookups == 3u))
		{
			build_object_index(object);
			return object_find_value_impl(object, key, false);
		}
		if (learn_lookup && compact_linear_lookup_limit <= length &&
			lookups != 3u)
		{
			object->tag = (object->tag & ~compact_lookup_mask) |
				(static_cast<::std::uint_least64_t>(lookups + 1u) <<
				 compact_lookup_shift);
		}
		auto *const key_node{linear_object_find_key(object, key)};
		return key_node == nullptr ? nullptr : key_node->next;
	}

	[[nodiscard]] bool subtree_contains(
		compact_json_node *root, compact_json_node *needle) const
	{
		if (root == needle)
		{
			return true;
		}
		if (!compact_is_container(root) || compact_node_length(root) == 0u)
		{
			return false;
		}
		json_vector<compact_json_node *> pending;
		pending.reserve(16u);
		pending.push_back(root);
		while (!pending.empty())
		{
			auto *const container_node{pending.back()};
			pending.pop_back();
			auto remaining{compact_node_length(container_node)};
			auto *current{container_node->payload.container->tail->next};
			bool const object{compact_node_kind(container_node) == json_kind::object};
			if (object)
			{
				current = current->next;
			}
			while (remaining-- != 0u)
			{
				if (current == needle)
				{
					return true;
				}
				if (compact_is_container(current) &&
					compact_node_length(current) != 0u)
				{
					pending.push_back(current);
				}
				current = object ? current->next->next : current->next;
			}
		}
		return false;
	}

	void require_detached_for_insert(
		compact_json_node *container, compact_json_node *value) const
	{
		if (value == nullptr || value == storage_->root || value->next != nullptr)
			[[unlikely]]
		{
			compact_type_error(json_errc::cyclic_reference);
		}
		if (compact_is_container(value) && subtree_contains(value, container))
			[[unlikely]]
		{
			compact_type_error(json_errc::cyclic_reference);
		}
	}

	static void append_array_link(
		compact_json_node *array, compact_json_node *value) noexcept
	{
		auto *const container{array->payload.container};
		auto const length{compact_node_length(array)};
		if (length == 0u)
		{
			value->next = value;
		}
		else
		{
			value->next = container->tail->next;
			container->tail->next = value;
		}
		container->tail = value;
		compact_set_node_length(array, length + 1u);
	}

	static void append_object_link(compact_json_node *object,
		compact_json_node *key, compact_json_node *value) noexcept
	{
		auto *const container{object->payload.container};
		auto const length{compact_node_length(object)};
		if (length == 0u)
		{
			key->next = value;
			value->next = key;
		}
		else
		{
			auto *const first_key{container->tail->next->next};
			container->tail->next->next = key;
			key->next = value;
			value->next = first_key;
		}
		container->tail = key;
		compact_set_node_length(object, length + 1u);
	}

public:
	explicit constexpr compact_storage_access(
		compact_json_storage *storage) noexcept : storage_(storage)
	{}

	[[nodiscard]] compact_json_node *make_node(
		json_kind kind = json_kind::undefined)
	{
		return storage_->allocate_node(kind);
	}

	void set_undefined(compact_json_node *node) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::undefined);
		node->payload.uinteger = 0u;
		node->next = link;
	}

	void set_null(compact_json_node *node) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::null);
		node->payload.uinteger = 0u;
		node->next = link;
	}

	void set_boolean(compact_json_node *node, bool value) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::boolean);
		node->payload.boolean = value;
		node->next = link;
	}

	void set_integer(compact_json_node *node,
		::std::int_least64_t value) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::integer);
		node->payload.integer = value;
		node->next = link;
	}

	void set_uinteger(compact_json_node *node,
		::std::uint_least64_t value) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::uinteger);
		node->payload.uinteger = value;
		node->next = link;
	}

	void set_number(compact_json_node *node, double value) noexcept
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::number);
		node->payload.number = value;
		node->next = link;
	}

	void set_string_known_valid(compact_json_node *node,
		::std::string_view value)
	{
		auto *const link{node->next};
		auto const direct{!compact_string_needs_escape(value)};
		node->tag = compact_make_tag(json_kind::string, value.size(),
			compact_validated_string_mask |
				(direct ? compact_direct_string_mask : 0u));
		node->payload.string = storage_->strings.copy(value);
		node->next = link;
	}

	void set_string(compact_json_node *node, ::std::string_view value)
	{
		if (!compact_valid_utf8(value)) [[unlikely]]
		{
			compact_type_error(json_errc::invalid_utf8);
		}
		set_string_known_valid(node, value);
	}

	void set_array(compact_json_node *node)
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::array);
		node->payload.container = storage_->allocate_container();
		node->next = link;
	}

	void set_object(compact_json_node *node)
	{
		auto *const link{node->next};
		node->tag = compact_make_tag(json_kind::object);
		node->payload.container = storage_->allocate_container();
		node->next = link;
	}

	void adopt_value(compact_json_node *target, compact_json_node *source)
	{
		if (target == source)
		{
			return;
		}
		if (source == nullptr || source == storage_->root || source->next != nullptr)
			[[unlikely]]
		{
			compact_type_error(json_errc::cyclic_reference);
		}
		if (compact_is_container(source) && subtree_contains(source, target))
			[[unlikely]]
		{
			compact_type_error(json_errc::cyclic_reference);
		}
		auto *const link{target->next};
		target->tag = source->tag;
		target->payload = source->payload;
		target->next = link;
		set_undefined(source);
	}

	void array_append(compact_json_node *array, compact_json_node *value,
		bool trusted = false)
	{
		static_cast<void>(require_container(
			array, json_kind::array, json_errc::not_array));
		if (!trusted)
		{
			require_detached_for_insert(array, value);
		}
		append_array_link(array, value);
	}

	void array_prepend(compact_json_node *array, compact_json_node *value)
	{
		auto *const container{require_container(
			array, json_kind::array, json_errc::not_array)};
		require_detached_for_insert(array, value);
		auto const length{compact_node_length(array)};
		if (length == 0u)
		{
			append_array_link(array, value);
			return;
		}
		value->next = container->tail->next;
		container->tail->next = value;
		compact_set_node_length(array, length + 1u);
	}

	[[nodiscard]] compact_json_node *array_at(
		compact_json_node *array, ::std::size_t index) const noexcept
	{
		if (compact_node_kind(array) != json_kind::array ||
			compact_node_length(array) <= index)
		{
			return nullptr;
		}
		auto *current{array->payload.container->tail->next};
		while (index-- != 0u)
		{
			current = current->next;
		}
		return current;
	}

	[[nodiscard]] compact_json_node *array_erase(
		compact_json_node *array, ::std::size_t index)
	{
		auto *const container{require_container(
			array, json_kind::array, json_errc::not_array)};
		auto const length{compact_node_length(array)};
		if (length <= index)
		{
			return nullptr;
		}
		auto *previous{container->tail};
		while (index-- != 0u)
		{
			previous = previous->next;
		}
		auto *const removed{previous->next};
		if (length == 1u)
		{
			container->tail = nullptr;
		}
		else
		{
			previous->next = removed->next;
			if (removed == container->tail)
			{
				container->tail = previous;
			}
		}
		removed->next = nullptr;
		compact_set_node_length(array, length - 1u);
		return removed;
	}

	[[nodiscard]] compact_json_node *object_find(
		compact_json_node *object, ::std::string_view key)
	{
		return object_find_value_impl(object, key, true);
	}

	[[nodiscard]] compact_json_node *object_append_known_absent(
		compact_json_node *object, ::std::string_view key,
		compact_json_node *value, bool trusted_value = false)
	{
		auto *const container{require_container(
			object, json_kind::object, json_errc::not_object)};
		if (!trusted_value)
		{
			require_detached_for_insert(object, value);
		}
		if (!compact_valid_utf8(key)) [[unlikely]]
		{
			compact_type_error(json_errc::invalid_utf8);
		}
		auto *const key_node{storage_->allocate_node(
			json_kind::string, key.size(), compact_validated_string_mask |
				(!compact_string_needs_escape(key)
					? compact_direct_string_mask
					: 0u))};
		key_node->payload.string = storage_->strings.copy(key);
		if (container->index != nullptr)
		{
			container->index->index.emplace_unique_known_absent(
				compact_string_view(key_node), value);
		}
		append_object_link(object, key_node, value);
		return value;
	}

	[[nodiscard]] ::std::pair<compact_json_node *, bool> object_try_insert(
		compact_json_node *object, ::std::string_view key,
		compact_json_node *value)
	{
		if (auto *const found{object_find_value_impl(object, key, true)};
			found != nullptr)
		{
			return {found, false};
		}
		return {object_append_known_absent(object, key, value), true};
	}

	[[nodiscard]] compact_json_node *object_insert_or_assign(
		compact_json_node *object, ::std::string_view key,
		compact_json_node *value)
	{
		if (auto *const found{object_find_value_impl(object, key, true)};
			found != nullptr)
		{
			adopt_value(found, value);
			return found;
		}
		return object_append_known_absent(object, key, value);
	}

	[[nodiscard]] compact_json_node *object_erase(
		compact_json_node *object, ::std::string_view key)
	{
		auto *const container{require_container(
			object, json_kind::object, json_errc::not_object)};
		auto const length{compact_node_length(object)};
		if (length == 0u)
		{
			return nullptr;
		}
		auto *previous_key{container->tail};
		auto *current_key{previous_key->next->next};
		for (::std::size_t index{}; index != length; ++index)
		{
			if (compact_string_view(current_key) == key)
			{
				auto *const removed_value{current_key->next};
				if (container->index != nullptr)
				{
					static_cast<void>(container->index->index.erase(key));
				}
				if (length == 1u)
				{
					container->tail = nullptr;
				}
				else
				{
					previous_key->next->next = removed_value->next;
					if (current_key == container->tail)
					{
						container->tail = previous_key;
					}
				}
				current_key->next = nullptr;
				removed_value->next = nullptr;
				compact_set_node_length(object, length - 1u);
				return removed_value;
			}
			previous_key = current_key;
			current_key = current_key->next->next;
		}
		return nullptr;
	}

	[[nodiscard]] constexpr compact_json_storage *storage() const noexcept
	{
		return storage_;
	}
};

} // namespace details

struct compact_json_statistics
{
	::std::size_t value_count{};
	::std::size_t container_count{};
	::std::size_t string_bytes{};
	::std::size_t reserved_bytes{};
	::std::size_t arena_allocation_calls{};
	::std::size_t indexed_object_count{};
};

class compact_const_json_ref
{
protected:
	details::compact_json_storage *storage_{};
	details::compact_json_node *node_{};

	friend class compact_mutable_json;
	friend class compact_json_ref;
	friend class compact_json_array_iterator;
	friend class compact_const_json_array_iterator;
	friend class compact_json_object_iterator;
	friend class compact_const_json_object_iterator;
	friend struct compact_json_array_range;
	friend struct compact_const_json_array_range;
	friend struct compact_json_object_range;
	friend struct compact_const_json_object_range;

	constexpr compact_const_json_ref(details::compact_json_storage *storage,
		details::compact_json_node *node) noexcept
		: storage_(storage), node_(node)
	{}

	void require_present() const
	{
		if (node_ == nullptr) [[unlikely]]
		{
			details::compact_type_error(json_errc::is_empty);
		}
	}

public:
	constexpr compact_const_json_ref() noexcept = default;

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return node_ != nullptr;
	}

	[[nodiscard]] json_kind kind() const
	{
		require_present();
		return details::compact_node_kind(node_);
	}

	[[nodiscard]] bool is_undefined() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::undefined;
	}
	[[nodiscard]] bool is_null() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::null;
	}
	[[nodiscard]] bool is_boolean() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::boolean;
	}
	[[nodiscard]] bool is_integer() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::integer;
	}
	[[nodiscard]] bool is_uinteger() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::uinteger;
	}
	[[nodiscard]] bool is_number() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::number;
	}
	[[nodiscard]] bool is_string() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::string;
	}
	[[nodiscard]] bool is_array() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::array;
	}
	[[nodiscard]] bool is_object() const noexcept
	{
		return node_ != nullptr && details::compact_node_kind(node_) == json_kind::object;
	}

	[[nodiscard]] ::std::size_t size() const
	{
		require_present();
		auto const type{details::compact_node_kind(node_)};
		if (type != json_kind::string && type != json_kind::array &&
			type != json_kind::object)
		{
			return 0u;
		}
		return details::compact_node_length(node_);
	}

	[[nodiscard]] bool get_boolean() const
	{
		if (!is_boolean()) [[unlikely]]
		{
			details::compact_type_error(json_errc::not_boolean);
		}
		return node_->payload.boolean;
	}

	[[nodiscard]] ::std::int_least64_t get_integer() const
	{
		if (!is_integer()) [[unlikely]]
		{
			details::compact_type_error(json_errc::not_integer);
		}
		return node_->payload.integer;
	}

	[[nodiscard]] ::std::uint_least64_t get_uinteger() const
	{
		if (!is_uinteger()) [[unlikely]]
		{
			details::compact_type_error(json_errc::not_uinteger);
		}
		return node_->payload.uinteger;
	}

	[[nodiscard]] double get_number() const
	{
		if (!is_number()) [[unlikely]]
		{
			details::compact_type_error(json_errc::not_number);
		}
		return node_->payload.number;
	}

	[[nodiscard]] ::std::string_view get_string() const
	{
		if (!is_string()) [[unlikely]]
		{
			details::compact_type_error(json_errc::not_string);
		}
		return details::compact_string_view(node_);
	}

	[[nodiscard]] bool string_is_compact_direct() const noexcept
	{
		return node_ != nullptr &&
			(node_->tag & details::compact_direct_string_mask) != 0u;
	}

	[[nodiscard]] bool string_is_unicode_validated() const noexcept
	{
		return node_ != nullptr &&
			(node_->tag & details::compact_validated_string_mask) != 0u;
	}

	[[nodiscard]] compact_const_json_ref operator[](::std::size_t index) const noexcept
	{
		if (!is_array())
		{
			return {};
		}
		details::compact_storage_access access{storage_};
		return {storage_, access.array_at(node_, index)};
	}

	[[nodiscard]] compact_const_json_ref find(::std::string_view key) const
	{
		if (!is_object())
		{
			return {};
		}
		details::compact_storage_access access{storage_};
		return {storage_, access.object_find(node_, key)};
	}

	[[nodiscard]] compact_const_json_ref at(::std::size_t index) const
	{
		auto result{(*this)[index]};
		if (!result) [[unlikely]]
		{
			details::compact_type_error(
				is_array() ? json_errc::index_out_of_range : json_errc::not_array);
		}
		return result;
	}

	[[nodiscard]] compact_const_json_ref at(::std::string_view key) const
	{
		auto result{find(key)};
		if (!result) [[unlikely]]
		{
			details::compact_type_error(
				is_object() ? json_errc::key_not_found : json_errc::not_object);
		}
		return result;
	}

	[[nodiscard]] details::compact_json_node const *raw_node() const noexcept
	{
		return node_;
	}
};

class compact_json_ref : public compact_const_json_ref
{
	friend class compact_mutable_json;
	friend class compact_json_array_iterator;
	friend class compact_json_object_iterator;

	constexpr compact_json_ref(details::compact_json_storage *storage,
		details::compact_json_node *node) noexcept
		: compact_const_json_ref(storage, node)
	{}

	[[nodiscard]] details::compact_storage_access access() const noexcept
	{
		return details::compact_storage_access{storage_};
	}

public:
	constexpr compact_json_ref() noexcept = default;

	void set_undefined() { require_present(); access().set_undefined(node_); }
	void set_null() { require_present(); access().set_null(node_); }
	void set_boolean(bool value) { require_present(); access().set_boolean(node_, value); }
	void set_integer(::std::int_least64_t value) { require_present(); access().set_integer(node_, value); }
	void set_uinteger(::std::uint_least64_t value) { require_present(); access().set_uinteger(node_, value); }
	void set_number(double value) { require_present(); access().set_number(node_, value); }
	void set_string(::std::string_view value) { require_present(); access().set_string(node_, value); }
	void set_array() { require_present(); access().set_array(node_); }
	void set_object() { require_present(); access().set_object(node_); }

	[[nodiscard]] compact_json_ref operator[](::std::size_t index) const noexcept
	{
		if (!is_array())
		{
			return {};
		}
		return {storage_, access().array_at(node_, index)};
	}

	[[nodiscard]] compact_json_ref find(::std::string_view key) const
	{
		if (!is_object())
		{
			return {};
		}
		return {storage_, access().object_find(node_, key)};
	}

	[[nodiscard]] compact_json_ref at(::std::size_t index) const
	{
		auto result{(*this)[index]};
		if (!result) [[unlikely]]
		{
			details::compact_type_error(
				is_array() ? json_errc::index_out_of_range : json_errc::not_array);
		}
		return result;
	}

	[[nodiscard]] compact_json_ref at(::std::string_view key) const
	{
		auto result{find(key)};
		if (!result) [[unlikely]]
		{
			details::compact_type_error(
				is_object() ? json_errc::key_not_found : json_errc::not_object);
		}
		return result;
	}

	void push_back(compact_json_ref value)
	{
		require_present();
		if (value.storage_ != storage_) [[unlikely]]
		{
			details::compact_type_error(json_errc::cyclic_reference);
		}
		access().array_append(node_, value.node_);
	}

	void push_front(compact_json_ref value)
	{
		require_present();
		if (value.storage_ != storage_) [[unlikely]]
		{
			details::compact_type_error(json_errc::cyclic_reference);
		}
		access().array_prepend(node_, value.node_);
	}

	[[nodiscard]] compact_json_ref erase(::std::size_t index)
	{
		require_present();
		return {storage_, access().array_erase(node_, index)};
	}

	[[nodiscard]] compact_json_ref erase(::std::string_view key)
	{
		require_present();
		return {storage_, access().object_erase(node_, key)};
	}

	[[nodiscard]] ::std::pair<compact_json_ref, bool> try_insert(
		::std::string_view key, compact_json_ref value)
	{
		require_present();
		if (value.storage_ != storage_) [[unlikely]]
		{
			details::compact_type_error(json_errc::cyclic_reference);
		}
		auto const result{access().object_try_insert(node_, key, value.node_)};
		return {{storage_, result.first}, result.second};
	}

	[[nodiscard]] compact_json_ref insert_or_assign(
		::std::string_view key, compact_json_ref value)
	{
		require_present();
		if (value.storage_ != storage_) [[unlikely]]
		{
			details::compact_type_error(json_errc::cyclic_reference);
		}
		return {storage_, access().object_insert_or_assign(node_, key, value.node_)};
	}

	[[nodiscard]] details::compact_json_node *raw_node() const noexcept
	{
		return node_;
	}
};

struct compact_json_member_ref
{
	::std::string_view key{};
	compact_json_ref value{};
};

struct compact_const_json_member_ref
{
	::std::string_view key{};
	compact_const_json_ref value{};
};

class compact_json_array_iterator
{
	details::compact_json_storage *storage_{};
	details::compact_json_node *current_{};
	::std::size_t remaining_{};

public:
	using value_type = compact_json_ref;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;
	using iterator_concept = ::std::forward_iterator_tag;

	constexpr compact_json_array_iterator() noexcept = default;
	constexpr compact_json_array_iterator(details::compact_json_storage *storage,
		details::compact_json_node *current, ::std::size_t remaining) noexcept
		: storage_(storage), current_(current), remaining_(remaining)
	{}

	[[nodiscard]] constexpr compact_json_ref operator*() const noexcept
	{
		return {storage_, current_};
	}
	constexpr compact_json_array_iterator &operator++() noexcept
	{
		current_ = current_->next;
		--remaining_;
		return *this;
	}
	constexpr compact_json_array_iterator operator++(int) noexcept
	{
		auto copy{*this};
		++*this;
		return copy;
	}
	friend constexpr bool operator==(compact_json_array_iterator const &left,
		compact_json_array_iterator const &right) noexcept
	{
		return left.storage_ == right.storage_ &&
			left.current_ == right.current_ &&
			left.remaining_ == right.remaining_;
	}
};

class compact_const_json_array_iterator
{
	details::compact_json_storage *storage_{};
	details::compact_json_node *current_{};
	::std::size_t remaining_{};

public:
	using value_type = compact_const_json_ref;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;
	using iterator_concept = ::std::forward_iterator_tag;

	constexpr compact_const_json_array_iterator() noexcept = default;
	constexpr compact_const_json_array_iterator(details::compact_json_storage *storage,
		details::compact_json_node *current, ::std::size_t remaining) noexcept
		: storage_(storage), current_(current), remaining_(remaining)
	{}

	[[nodiscard]] constexpr compact_const_json_ref operator*() const noexcept
	{
		return {storage_, current_};
	}
	constexpr compact_const_json_array_iterator &operator++() noexcept
	{
		current_ = current_->next;
		--remaining_;
		return *this;
	}
	constexpr compact_const_json_array_iterator operator++(int) noexcept
	{
		auto copy{*this};
		++*this;
		return copy;
	}
	friend constexpr bool operator==(
		compact_const_json_array_iterator const &left,
		compact_const_json_array_iterator const &right) noexcept
	{
		return left.storage_ == right.storage_ &&
			left.current_ == right.current_ &&
			left.remaining_ == right.remaining_;
	}
};

class compact_json_object_iterator
{
	details::compact_json_storage *storage_{};
	details::compact_json_node *key_{};
	::std::size_t remaining_{};

public:
	using value_type = compact_json_member_ref;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;
	using iterator_concept = ::std::forward_iterator_tag;

	constexpr compact_json_object_iterator() noexcept = default;
	constexpr compact_json_object_iterator(details::compact_json_storage *storage,
		details::compact_json_node *key, ::std::size_t remaining) noexcept
		: storage_(storage), key_(key), remaining_(remaining)
	{}

	[[nodiscard]] constexpr compact_json_member_ref operator*() const noexcept
	{
		return {details::compact_string_view(key_), {storage_, key_->next}};
	}
	constexpr compact_json_object_iterator &operator++() noexcept
	{
		key_ = key_->next->next;
		--remaining_;
		return *this;
	}
	constexpr compact_json_object_iterator operator++(int) noexcept
	{
		auto copy{*this};
		++*this;
		return copy;
	}
	friend constexpr bool operator==(compact_json_object_iterator const &left,
		compact_json_object_iterator const &right) noexcept
	{
		return left.storage_ == right.storage_ && left.key_ == right.key_ &&
			left.remaining_ == right.remaining_;
	}
};

class compact_const_json_object_iterator
{
	details::compact_json_storage *storage_{};
	details::compact_json_node *key_{};
	::std::size_t remaining_{};

public:
	using value_type = compact_const_json_member_ref;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;
	using iterator_concept = ::std::forward_iterator_tag;

	constexpr compact_const_json_object_iterator() noexcept = default;
	constexpr compact_const_json_object_iterator(
		details::compact_json_storage *storage,
		details::compact_json_node *key, ::std::size_t remaining) noexcept
		: storage_(storage), key_(key), remaining_(remaining)
	{}

	[[nodiscard]] constexpr compact_const_json_member_ref operator*() const noexcept
	{
		return {details::compact_string_view(key_), {storage_, key_->next}};
	}
	constexpr compact_const_json_object_iterator &operator++() noexcept
	{
		key_ = key_->next->next;
		--remaining_;
		return *this;
	}
	constexpr compact_const_json_object_iterator operator++(int) noexcept
	{
		auto copy{*this};
		++*this;
		return copy;
	}
	friend constexpr bool operator==(
		compact_const_json_object_iterator const &left,
		compact_const_json_object_iterator const &right) noexcept
	{
		return left.storage_ == right.storage_ && left.key_ == right.key_ &&
			left.remaining_ == right.remaining_;
	}
};

struct compact_json_array_range
{
	compact_json_ref value{};

	[[nodiscard]] compact_json_array_iterator begin() const noexcept
	{
		auto const length{value.is_array() ? value.size() : 0u};
		return {value.storage_, length == 0u ? nullptr
			: value.node_->payload.container->tail->next, length};
	}
	[[nodiscard]] constexpr compact_json_array_iterator end() const noexcept
	{
		return {};
	}
};

struct compact_const_json_array_range
{
	compact_const_json_ref value{};

	[[nodiscard]] compact_const_json_array_iterator begin() const noexcept
	{
		auto const length{value.is_array() ? value.size() : 0u};
		return {value.storage_, length == 0u ? nullptr
			: value.node_->payload.container->tail->next, length};
	}
	[[nodiscard]] constexpr compact_const_json_array_iterator end() const noexcept
	{
		return {};
	}
};

struct compact_json_object_range
{
	compact_json_ref value{};

	[[nodiscard]] compact_json_object_iterator begin() const noexcept
	{
		auto const length{value.is_object() ? value.size() : 0u};
		return {value.storage_, length == 0u ? nullptr
			: value.node_->payload.container->tail->next->next, length};
	}
	[[nodiscard]] constexpr compact_json_object_iterator end() const noexcept
	{
		return {};
	}
};

struct compact_const_json_object_range
{
	compact_const_json_ref value{};

	[[nodiscard]] compact_const_json_object_iterator begin() const noexcept
	{
		auto const length{value.is_object() ? value.size() : 0u};
		return {value.storage_, length == 0u ? nullptr
			: value.node_->payload.container->tail->next->next, length};
	}
	[[nodiscard]] constexpr compact_const_json_object_iterator end() const noexcept
	{
		return {};
	}
};

class compact_mutable_json
{
	details::compact_json_storage *storage_{};

	[[nodiscard]] static details::compact_json_storage *allocate_storage()
	{
		auto *const result{
			::fast_io::native_typed_global_allocator<
				details::compact_json_storage>::allocate(1u)};
		::std::construct_at(result);
		return result;
	}

	static void release_storage(details::compact_json_storage *storage) noexcept
	{
		if (storage != nullptr)
		{
			::std::destroy_at(storage);
			::fast_io::native_typed_global_allocator<
				details::compact_json_storage>::deallocate(storage);
		}
	}

	[[nodiscard]] details::compact_json_storage *ensure_storage()
	{
		if (storage_ == nullptr)
		{
			storage_ = allocate_storage();
		}
		return storage_;
	}

	[[nodiscard]] compact_json_ref make_node(json_kind type)
	{
		auto *const storage{ensure_storage()};
		details::compact_storage_access access{storage};
		return {storage, access.make_node(type)};
	}

public:
	compact_mutable_json() : storage_(allocate_storage()) {}
	~compact_mutable_json() { release_storage(storage_); }
	compact_mutable_json(compact_mutable_json const &) = delete;
	compact_mutable_json &operator=(compact_mutable_json const &) = delete;
	compact_mutable_json(compact_mutable_json &&other) noexcept
		: storage_(other.storage_)
	{
		other.storage_ = nullptr;
	}
	compact_mutable_json &operator=(compact_mutable_json &&other) noexcept
	{
		if (this != __builtin_addressof(other))
		{
			release_storage(storage_);
			storage_ = other.storage_;
			other.storage_ = nullptr;
		}
		return *this;
	}

	void swap(compact_mutable_json &other) noexcept
	{
		auto *const temporary{storage_};
		storage_ = other.storage_;
		other.storage_ = temporary;
	}
	friend void swap(compact_mutable_json &left,
		compact_mutable_json &right) noexcept
	{
		left.swap(right);
	}

	[[nodiscard]] compact_json_ref root()
	{
		auto *const storage{ensure_storage()};
		return {storage, storage->root};
	}
	[[nodiscard]] constexpr compact_const_json_ref root() const noexcept
	{
		return storage_ == nullptr ? compact_const_json_ref{}
			: compact_const_json_ref{storage_, storage_->root};
	}

	[[nodiscard]] compact_json_ref make_undefined()
	{
		return make_node(json_kind::undefined);
	}
	[[nodiscard]] compact_json_ref make_null()
	{
		return make_node(json_kind::null);
	}
	[[nodiscard]] compact_json_ref make_boolean(bool value)
	{
		auto result{make_node(json_kind::boolean)};
		result.node_->payload.boolean = value;
		return result;
	}
	[[nodiscard]] compact_json_ref make_integer(::std::int_least64_t value)
	{
		auto result{make_node(json_kind::integer)};
		result.node_->payload.integer = value;
		return result;
	}
	[[nodiscard]] compact_json_ref make_uinteger(::std::uint_least64_t value)
	{
		auto result{make_node(json_kind::uinteger)};
		result.node_->payload.uinteger = value;
		return result;
	}
	[[nodiscard]] compact_json_ref make_number(double value)
	{
		auto result{make_node(json_kind::number)};
		result.node_->payload.number = value;
		return result;
	}
	[[nodiscard]] compact_json_ref make_string(::std::string_view value)
	{
		auto result{make_node(json_kind::undefined)};
		details::compact_storage_access{result.storage_}.set_string(result.node_, value);
		return result;
	}
	[[nodiscard]] compact_json_ref make_array()
	{
		auto result{make_node(json_kind::undefined)};
		details::compact_storage_access{result.storage_}.set_array(result.node_);
		return result;
	}
	[[nodiscard]] compact_json_ref make_object()
	{
		auto result{make_node(json_kind::undefined)};
		details::compact_storage_access{result.storage_}.set_object(result.node_);
		return result;
	}

	void adopt_root(compact_json_ref value)
	{
		auto *const storage{ensure_storage()};
		if (value.storage_ != storage) [[unlikely]]
		{
			details::compact_type_error(json_errc::cyclic_reference);
		}
		details::compact_storage_access{storage}.adopt_value(storage->root, value.node_);
	}

	[[nodiscard]] compact_json_array_range array_items(compact_json_ref value) noexcept
	{
		return {value};
	}
	[[nodiscard]] compact_const_json_array_range array_items(
		compact_const_json_ref value) const noexcept
	{
		return {value};
	}
	[[nodiscard]] compact_json_object_range object_items(compact_json_ref value) noexcept
	{
		return {value};
	}
	[[nodiscard]] compact_const_json_object_range object_items(
		compact_const_json_ref value) const noexcept
	{
		return {value};
	}

	void reset()
	{
		auto *const replacement{allocate_storage()};
		release_storage(storage_);
		storage_ = replacement;
	}

	[[nodiscard]] compact_json_statistics statistics() const noexcept
	{
		if (storage_ == nullptr)
		{
			return {};
		}
		compact_json_statistics result{};
		result.value_count = storage_->nodes.used();
		result.container_count = storage_->containers.used();
		result.string_bytes = storage_->strings.used();
		result.reserved_bytes = storage_->nodes.reserved_bytes() +
			storage_->containers.reserved_bytes() +
			storage_->strings.reserved_bytes();
		result.arena_allocation_calls = storage_->nodes.allocation_calls() +
			storage_->containers.allocation_calls() +
			storage_->strings.allocation_calls();
		for (auto *index{storage_->indexes}; index != nullptr; index = index->next)
		{
			++result.indexed_object_count;
		}
		return result;
	}

	[[nodiscard]] details::compact_json_storage *raw_storage() noexcept
	{
		return ensure_storage();
	}
	[[nodiscard]] details::compact_json_storage const *raw_storage() const noexcept
	{
		return storage_;
	}
};

[[nodiscard]] inline compact_json_array_range array_items(
	compact_json_ref value) noexcept
{
	return {value};
}

[[nodiscard]] inline compact_const_json_array_range array_items(
	compact_const_json_ref value) noexcept
{
	return {value};
}

[[nodiscard]] inline compact_json_object_range object_items(
	compact_json_ref value) noexcept
{
	return {value};
}

[[nodiscard]] inline compact_const_json_object_range object_items(
	compact_const_json_ref value) noexcept
{
	return {value};
}

struct compact_json_print_view
{
	compact_const_json_ref reference{};
	json_serialize_options options{};
};

template <::std::integral output_char_type, bool transcoding>
struct basic_compact_json_io_print_view : compact_json_print_view
{
	using base_type = compact_json_print_view;
	using output_char_type_alias = output_char_type;
	inline static constexpr bool is_transcoding{transcoding};

	static_assert(transcoding !=
		::std::same_as<::std::remove_cv_t<output_char_type>, char>);

	constexpr basic_compact_json_io_print_view() noexcept = default;
	constexpr explicit basic_compact_json_io_print_view(
		base_type value) noexcept : base_type(value)
	{}
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>,
	compact_json_print_view value) noexcept
{
	return basic_compact_json_io_print_view<char_type,
		!::std::same_as<::std::remove_cv_t<char_type>, char>>{value};
}

namespace details
{

template <typename sink_type>
[[nodiscard]] inline bool compact_emit_noncontainer(
	sink_type &sink,
	json_vector<typename sink_type::output_char_type> &dynamic_storage,
	compact_json_node const *value,
	json_serialize_options const &options)
{
	switch (compact_node_kind(value))
	{
	case json_kind::undefined:
		switch (options.undefined)
		{
		case json_undefined_policy::error:
			compact_type_error(json_errc::is_undefined);
		case json_undefined_policy::as_null:
			json_sink_literal<u8'n', u8'u', u8'l', u8'l'>(sink);
			break;
		case json_undefined_policy::as_literal:
			json_sink_literal<u8'u', u8'n', u8'd', u8'e', u8'f', u8'i',
				u8'n', u8'e', u8'd'>(sink);
			break;
		}
		return true;
	case json_kind::null:
		json_sink_literal<u8'n', u8'u', u8'l', u8'l'>(sink);
		return true;
	case json_kind::boolean:
		json_emit_boolean(sink, dynamic_storage, value->payload.boolean);
		return true;
	case json_kind::integer:
		json_emit_integer(sink, dynamic_storage, value->payload.integer);
		return true;
	case json_kind::uinteger:
		json_emit_integer(sink, dynamic_storage, value->payload.uinteger);
		return true;
	case json_kind::number:
		json_emit_floating(sink, dynamic_storage, value->payload.number);
		return true;
	case json_kind::string:
		json_emit_dom_quoted_string(sink, compact_string_view(value),
			(value->tag & compact_direct_string_mask) != 0u,
			(value->tag & compact_validated_string_mask) != 0u, options);
		return true;
	case json_kind::array:
	case json_kind::object:
		return false;
	default:
		::fast_io::fast_terminate();
	}
}

struct compact_json_walk_frame
{
	compact_json_node const *current{};
	::std::size_t remaining{};
	::std::size_t depth{};
	bool object{};
};

class compact_json_frame_stack
{
	inline static constexpr ::std::size_t local_capacity{64u};
	::std::array<compact_json_walk_frame, local_capacity> local_{};
	json_vector<compact_json_walk_frame> overflow_{};
	::std::size_t size_{};

public:
	[[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0u; }
	[[nodiscard]] constexpr ::std::size_t size() const noexcept { return size_; }

	void push(compact_json_walk_frame value)
	{
		if (size_ < local_capacity)
		{
			local_[size_] = value;
		}
		else
		{
			overflow_.push_back(value);
		}
		++size_;
	}

	[[nodiscard]] compact_json_walk_frame &back() noexcept
	{
		return size_ <= local_capacity ? local_[size_ - 1u] : overflow_.back();
	}

	void pop() noexcept
	{
		if (local_capacity < size_)
		{
			overflow_.pop_back();
		}
		--size_;
	}
};

/*
The pending node is entered exactly once.  A frame contains the circular-list
cursor of one open ancestor and the number of children not yet completed.
The cursor advances only after its current child completes, so nested descent
cannot invalidate it.  This invariant balances delimiters without recursion
and gives depth checking independent of the C++ call stack.
*/
template <typename sink_type>
inline void compact_walk_document(
	sink_type &sink, compact_json_node const *root,
	json_serialize_options const &options)
{
	using char_type = typename sink_type::output_char_type;
	json_vector<char_type> dynamic_storage;
	compact_json_frame_stack frames;
	auto const *pending{root};

	for (;;)
	{
		if (pending != nullptr)
		{
			if (compact_emit_noncontainer(
					sink, dynamic_storage, pending, options))
			{
				pending = nullptr;
				continue;
			}

			auto const depth{frames.size()};
			if (options.max_depth <= depth) [[unlikely]]
			{
				compact_type_error(json_errc::depth_exceeded);
			}
			auto const object{compact_node_kind(pending) == json_kind::object};
			sink.append_one(
				object ? ::fast_io::char_literal_v<u8'{', char_type>
					: ::fast_io::char_literal_v<u8'[', char_type>);
			auto const length{compact_node_length(pending)};
			if (length == 0u)
			{
				sink.append_one(
					object ? ::fast_io::char_literal_v<u8'}', char_type>
						: ::fast_io::char_literal_v<u8']', char_type>);
				pending = nullptr;
				continue;
			}
			if (options.pretty)
			{
				json_emit_layout(sink, depth + 1u, options);
			}
			auto const *current{pending->payload.container->tail->next};
			if (object)
			{
				current = current->next;
			}
			frames.push({current, length, depth, object});
			if (object)
			{
				json_emit_dom_quoted_string(sink, compact_string_view(current),
					(current->tag & compact_direct_string_mask) != 0u,
					true, options);
				if (options.pretty)
				{
					json_sink_literal<u8':', u8' '>(sink);
				}
				else
				{
					json_sink_literal<u8':'>(sink);
				}
				pending = current->next;
			}
			else
			{
				pending = current;
			}
			continue;
		}

		if (frames.empty())
		{
			return;
		}
		auto &parent{frames.back()};
		if (--parent.remaining == 0u)
		{
			if (options.pretty)
			{
				json_emit_layout(sink, parent.depth, options);
			}
			sink.append_one(
				parent.object ? ::fast_io::char_literal_v<u8'}', char_type>
					: ::fast_io::char_literal_v<u8']', char_type>);
			frames.pop();
			continue;
		}

		json_sink_literal<u8','>(sink);
		if (options.pretty)
		{
			json_emit_layout(sink, parent.depth + 1u, options);
		}
		parent.current = parent.object
			? parent.current->next->next
			: parent.current->next;
		if (parent.object)
		{
			json_emit_dom_quoted_string(sink,
				compact_string_view(parent.current),
				(parent.current->tag & compact_direct_string_mask) != 0u,
				true, options);
			if (options.pretty)
			{
				json_sink_literal<u8':', u8' '>(sink);
			}
			else
			{
				json_sink_literal<u8':'>(sink);
			}
			pending = parent.current->next;
		}
		else
		{
			pending = parent.current;
		}
	}
}

template <::std::integral char_type>
[[nodiscard]] inline ::std::size_t compact_measure_document(
	compact_json_print_view const &view)
{
	basic_json_size_sink<char_type> sink;
	compact_walk_document(sink,
		static_cast<compact_json_node const *>(view.reference.raw_node()),
		view.options);
	return sink.size();
}

template <::std::integral char_type>
[[nodiscard]] inline ::std::size_t compact_bound_document(
	compact_json_print_view const &view)
{
	basic_json_bound_sink<char_type> sink;
	compact_walk_document(sink,
		static_cast<compact_json_node const *>(view.reference.raw_node()),
		view.options);
	return sink.size();
}

template <::std::integral char_type>
[[nodiscard]] inline char_type *compact_write_document(
	char_type *output, compact_json_print_view const &view)
{
	basic_json_contiguous_sink<char_type> sink{output};
	compact_walk_document(sink,
		static_cast<compact_json_node const *>(view.reference.raw_node()),
		view.options);
	return sink.current();
}

template <::std::integral char_type>
[[nodiscard]] inline char_type *compact_write_document_precise(
	char_type *output, ::std::size_t size,
	compact_json_print_view const &view)
{
	auto *const result{compact_write_document(output, view)};
	if (result != output + size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return result;
}

template <::std::integral char_type, typename output_type>
inline void compact_write_output(
	output_type &output, compact_json_print_view const &view)
{
	basic_json_output_sink<char_type, output_type> sink{output};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
	try
	{
		compact_walk_document(sink,
			static_cast<compact_json_node const *>(view.reference.raw_node()),
			view.options);
		sink.flush();
	}
	catch (...)
	{
		try
		{
			sink.flush();
		}
		catch (...)
		{
		}
		throw;
	}
#else
	compact_walk_document(sink,
		static_cast<compact_json_node const *>(view.reference.raw_node()),
		view.options);
	sink.flush();
#endif
}

template <::std::integral char_type>
class compact_json_print_context
{
	json_vector<char_type> materialized_{};
	::std::size_t position_{};
	bool initialized_{};
	bool finished_{};

public:
	inline ::fast_io::context_print_result<char_type *> print_context_define(
		compact_json_print_view const &view,
		char_type *output, char_type *output_last)
	{
		if (finished_ || output == output_last)
		{
			return {output, finished_};
		}
		if (!initialized_)
		{
			auto const size{compact_measure_document<char_type>(view)};
			materialized_.resize(size);
			static_cast<void>(compact_write_document_precise(
				materialized_.data(), size, view));
			initialized_ = true;
		}
		auto const available{static_cast<::std::size_t>(output_last - output)};
		auto const remaining{materialized_.size() - position_};
		auto const copied{available < remaining ? available : remaining};
		for (::std::size_t index{}; index != copied; ++index)
		{
			output[index] = materialized_[position_ + index];
		}
		output += copied;
		position_ += copied;
		finished_ = position_ == materialized_.size();
		return {output, finished_};
	}
};

} // namespace details

[[nodiscard]] inline constexpr compact_json_print_view make_json_print_view(
	compact_mutable_json const &value,
	json_serialize_options options = {}) noexcept
{
	return {value.root(), options};
}

[[nodiscard]] inline constexpr compact_json_print_view make_json_print_view(
	compact_const_json_ref value,
	json_serialize_options options = {}) noexcept
{
	return {value, options};
}

auto make_json_print_view(compact_mutable_json &&,
	json_serialize_options = {}) = delete;
auto make_json_print_view(compact_mutable_json const &&,
	json_serialize_options = {}) = delete;

[[nodiscard]] inline constexpr compact_json_print_view print_alias_define(
	::fast_io::io_alias_t, compact_mutable_json const &value) noexcept
{
	return make_json_print_view(value);
}

[[nodiscard]] inline constexpr compact_json_print_view print_alias_define(
	::fast_io::io_alias_t, compact_const_json_ref value) noexcept
{
	return make_json_print_view(value);
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr auto print_context_type(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return ::fast_io::io_type_t<details::compact_json_print_context<char_type>>{};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	constexpr ::std::size_t count{4096u / sizeof(char_type)};
	return count == 0u ? 1u : count;
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>,
	basic_compact_json_io_print_view<char_type, transcoding> const &view)
{
	return details::compact_bound_document<char_type>(view);
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>,
	char_type *output,
	basic_compact_json_io_print_view<char_type, transcoding> const &view)
{
	return details::compact_write_document(output, view);
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>,
	basic_compact_json_io_print_view<char_type, transcoding> const &view)
{
	return details::compact_measure_document<char_type>(view);
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>,
	char_type *output, ::std::size_t size,
	basic_compact_json_io_print_view<char_type, transcoding> const &view)
{
	return details::compact_write_document_precise(output, size, view);
}

template <::std::integral char_type, typename output_type, bool transcoding>
inline void print_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>,
	output_type &output,
	basic_compact_json_io_print_view<char_type, transcoding> const &view)
{
	details::compact_write_output<char_type>(output, view);
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type
print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_single_pass_staging_safe(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_put_area_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_buffered_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_one_pass_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_concat_one_pass_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_compact_json_io_print_view<char_type, transcoding>>) noexcept
{
	return {};
}

} // namespace fast_io::json

namespace fast_io::manipulators
{

[[nodiscard]] inline constexpr auto json(
	::fast_io::json::compact_mutable_json const &value,
	::fast_io::json::json_serialize_options options = {}) noexcept
{
	return ::fast_io::json::make_json_print_view(value, options);
}

[[nodiscard]] inline constexpr auto pretty_json(
	::fast_io::json::compact_mutable_json const &value,
	::std::size_t indentation = 2u) noexcept
{
	::fast_io::json::json_serialize_options options{};
	options.pretty = true;
	options.indent_width = indentation;
	return ::fast_io::json::make_json_print_view(value, options);
}

auto json(::fast_io::json::compact_mutable_json &&,
	::fast_io::json::json_serialize_options = {}) = delete;
auto pretty_json(::fast_io::json::compact_mutable_json &&,
	::std::size_t = 2u) = delete;

} // namespace fast_io::manipulators
