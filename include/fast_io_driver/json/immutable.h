#pragma once

#include "parse.h"
#include "serialize.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fast_io::json
{

class immutable_json;
class immutable_json_slice;

namespace details
{

/*
The immutable DOM is a preorder tape.  A default scalar occupies one 16-byte
cell; a container is followed immediately by all of its descendant cells.
The payload of a container is its complete subtree span in cells, so skipping
an arbitrarily deep value is one addition.  Arrays store child values and
objects store key/value pairs.  This is the same locality property that makes
an immutable yyjson document cheap to traverse, while remaining an independent
fast_io representation and API.

Low tag bits hold json_kind and string proof bits.  The upper 56 bits hold a
decoded string length or direct child/member count.  All strings point into
the immutable document's owned, in-situ-decoded input arena.
*/
inline constexpr ::std::uint_least64_t immutable_json_kind_mask{0x0Fu};
inline constexpr ::std::uint_least64_t immutable_json_direct_string_mask{0x10u};
inline constexpr ::std::uint_least64_t immutable_json_discarded_key_mask{0x20u};
inline constexpr unsigned immutable_json_length_shift{8u};

union immutable_json_payload
{
	::std::uint_least64_t uinteger;
	::std::int_least64_t integer;
	double number;
	char const *string;
	::std::size_t subtree_span;

	constexpr immutable_json_payload() noexcept : uinteger{} {}
};

struct immutable_json_node
{
	::std::uint_least64_t tag{};
	immutable_json_payload payload{};
};

static_assert(sizeof(immutable_json_node) == 16u,
	"the default immutable JSON tape cell must remain 16 bytes");
static_assert(alignof(immutable_json_node) == alignof(::std::uint_least64_t));

[[nodiscard]] inline constexpr json_kind
immutable_json_kind(immutable_json_node const &value) noexcept
{
	return static_cast<json_kind>(value.tag & immutable_json_kind_mask);
}

[[nodiscard]] inline constexpr ::std::size_t
immutable_json_length(immutable_json_node const &value) noexcept
{
	return static_cast<::std::size_t>(
		value.tag >> immutable_json_length_shift);
}

[[nodiscard]] inline constexpr bool
immutable_json_is_container(immutable_json_node const &value) noexcept
{
	auto const kind{immutable_json_kind(value)};
	return kind == json_kind::array || kind == json_kind::object;
}

[[nodiscard]] inline constexpr immutable_json_node const *
immutable_json_next(immutable_json_node const *value) noexcept
{
	return value + (immutable_json_is_container(*value)
		? value->payload.subtree_span : 1u);
}

} // namespace details

class immutable_json_array_iterator
{
	details::immutable_json_node const *current_{};
	details::immutable_json_node const *end_{};

public:
	using value_type = immutable_json_slice;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;

	constexpr immutable_json_array_iterator() noexcept = default;
	constexpr immutable_json_array_iterator(
		details::immutable_json_node const *current,
		details::immutable_json_node const *end) noexcept
		: current_(current), end_(end)
	{}

	[[nodiscard]] constexpr immutable_json_slice operator*() const noexcept;

	constexpr immutable_json_array_iterator &operator++() noexcept
	{
		current_ = details::immutable_json_next(current_);
		return *this;
	}

	constexpr immutable_json_array_iterator operator++(int) noexcept
	{
		auto previous{*this};
		++*this;
		return previous;
	}

	friend constexpr bool operator==(
		immutable_json_array_iterator const &,
		immutable_json_array_iterator const &) noexcept = default;
};

struct immutable_json_member;

class immutable_json_object_iterator
{
	details::immutable_json_node const *current_{};
	details::immutable_json_node const *end_{};

	constexpr void skip_discarded() noexcept
	{
		while (current_ != end_ &&
			(current_->tag & details::immutable_json_discarded_key_mask) != 0u)
		{
			current_ = details::immutable_json_next(current_ + 1u);
		}
	}

public:
	using value_type = immutable_json_member;
	using difference_type = ::std::ptrdiff_t;
	using iterator_category = ::std::forward_iterator_tag;

	constexpr immutable_json_object_iterator() noexcept = default;
	constexpr immutable_json_object_iterator(
		details::immutable_json_node const *current,
		details::immutable_json_node const *end) noexcept
		: current_(current), end_(end)
	{
		skip_discarded();
	}

	[[nodiscard]] constexpr immutable_json_member operator*() const noexcept;

	constexpr immutable_json_object_iterator &operator++() noexcept
	{
		current_ = details::immutable_json_next(current_ + 1u);
		skip_discarded();
		return *this;
	}

	constexpr immutable_json_object_iterator operator++(int) noexcept
	{
		auto previous{*this};
		++*this;
		return previous;
	}

	friend constexpr bool operator==(
		immutable_json_object_iterator const &,
		immutable_json_object_iterator const &) noexcept = default;
};

struct immutable_json_array_range
{
	details::immutable_json_node const *first{};
	details::immutable_json_node const *last{};

	[[nodiscard]] constexpr immutable_json_array_iterator begin() const noexcept
	{
		return {first, last};
	}
	[[nodiscard]] constexpr immutable_json_array_iterator end() const noexcept
	{
		return {last, last};
	}
};

struct immutable_json_object_range
{
	details::immutable_json_node const *first{};
	details::immutable_json_node const *last{};

	[[nodiscard]] constexpr immutable_json_object_iterator begin() const noexcept
	{
		return {first, last};
	}
	[[nodiscard]] constexpr immutable_json_object_iterator end() const noexcept
	{
		return {last, last};
	}
};

class immutable_json_slice
{
	details::immutable_json_node const *node_{};

	friend class immutable_json;
	friend class immutable_json_array_iterator;
	friend class immutable_json_object_iterator;

public:
	using char_type = char;
	using number_type = double;
	using integer_type = ::std::int_least64_t;
	using uinteger_type = ::std::uint_least64_t;
	using string_view_type = ::std::string_view;

	constexpr immutable_json_slice() noexcept = default;
	explicit constexpr immutable_json_slice(
		details::immutable_json_node const *value) noexcept : node_(value)
	{}

	[[nodiscard]] constexpr bool bound() const noexcept { return node_ != nullptr; }
	[[nodiscard]] constexpr bool has_reference() const noexcept { return node_ != nullptr; }
	[[nodiscard]] constexpr bool empty() const noexcept { return node_ == nullptr; }

	[[nodiscard]] constexpr json_kind kind() const
	{
		if (node_ == nullptr)
		{
			throw_json_error(json_errc::is_empty);
		}
		return details::immutable_json_kind(*node_);
	}

	[[nodiscard]] constexpr bool is_undefined() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::undefined;
	}
	[[nodiscard]] constexpr bool is_null() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::null;
	}
	[[nodiscard]] constexpr bool is_boolean() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::boolean;
	}
	[[nodiscard]] constexpr bool is_number() const noexcept
	{
		if (node_ == nullptr) return false;
		auto const value_kind{details::immutable_json_kind(*node_)};
		return value_kind == json_kind::number ||
			value_kind == json_kind::integer ||
			value_kind == json_kind::uinteger;
	}
	[[nodiscard]] constexpr bool is_integer() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::integer;
	}
	[[nodiscard]] constexpr bool is_uinteger() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::uinteger;
	}
	[[nodiscard]] constexpr bool is_string() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::string;
	}
	[[nodiscard]] constexpr bool is_array() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::array;
	}
	[[nodiscard]] constexpr bool is_object() const noexcept
	{
		return node_ != nullptr &&
			details::immutable_json_kind(*node_) == json_kind::object;
	}

	[[nodiscard]] constexpr bool undefined() const noexcept { return is_undefined(); }
	[[nodiscard]] constexpr bool null() const noexcept { return is_null(); }
	[[nodiscard]] constexpr bool boolean() const noexcept { return is_boolean(); }
	[[nodiscard]] constexpr bool number() const noexcept { return is_number(); }
	[[nodiscard]] constexpr bool integer() const noexcept { return is_integer(); }
	[[nodiscard]] constexpr bool uinteger() const noexcept { return is_uinteger(); }
	[[nodiscard]] constexpr bool string() const noexcept { return is_string(); }
	[[nodiscard]] constexpr bool array() const noexcept { return is_array(); }
	[[nodiscard]] constexpr bool object() const noexcept { return is_object(); }

	[[nodiscard]] constexpr ::std::size_t size() const noexcept
	{
		if (node_ == nullptr) return 0u;
		switch (details::immutable_json_kind(*node_))
		{
		case json_kind::undefined:
		case json_kind::null:
			return 0u;
		case json_kind::string:
		case json_kind::array:
		case json_kind::object:
			return details::immutable_json_length(*node_);
		default:
			return 1u;
		}
	}

	[[nodiscard]] constexpr bool value_empty() const noexcept
	{
		return size() == 0u;
	}

	[[nodiscard]] constexpr bool get_boolean() const
	{
		if (!is_boolean()) throw_json_error(json_errc::not_boolean);
		return node_->payload.uinteger != 0u;
	}
	[[nodiscard]] constexpr number_type get_number() const
	{
		if (node_ == nullptr ||
			details::immutable_json_kind(*node_) != json_kind::number)
		{
			throw_json_error(json_errc::not_number);
		}
		return node_->payload.number;
	}
	[[nodiscard]] constexpr number_type as_number() const
	{
		if (node_ != nullptr)
		{
			switch (details::immutable_json_kind(*node_))
			{
			case json_kind::number: return node_->payload.number;
			case json_kind::integer:
				return static_cast<number_type>(node_->payload.integer);
			case json_kind::uinteger:
				return static_cast<number_type>(node_->payload.uinteger);
			default: break;
			}
		}
		throw_json_error(json_errc::not_number);
	}
	[[nodiscard]] constexpr integer_type get_integer() const
	{
		if (!is_integer()) throw_json_error(json_errc::not_integer);
		return node_->payload.integer;
	}
	[[nodiscard]] constexpr uinteger_type get_uinteger() const
	{
		if (!is_uinteger()) throw_json_error(json_errc::not_uinteger);
		return node_->payload.uinteger;
	}
	[[nodiscard]] constexpr string_view_type get_string() const
	{
		if (!is_string()) throw_json_error(json_errc::not_string);
		return {node_->payload.string, details::immutable_json_length(*node_)};
	}
	[[nodiscard]] constexpr bool string_is_compact_direct() const noexcept
	{
		return is_string() &&
			(node_->tag & details::immutable_json_direct_string_mask) != 0u;
	}

	[[nodiscard]] constexpr immutable_json_array_range as_array() const
	{
		if (!is_array()) throw_json_error(json_errc::not_array);
		return {node_ + 1u, node_ + node_->payload.subtree_span};
	}
	[[nodiscard]] constexpr immutable_json_object_range as_object() const
	{
		if (!is_object()) throw_json_error(json_errc::not_object);
		return {node_ + 1u, node_ + node_->payload.subtree_span};
	}

	[[nodiscard]] immutable_json_slice operator[](::std::size_t index) const noexcept
	{
		if (!is_array()) return {};
		auto current{node_ + 1u};
		auto const end{node_ + node_->payload.subtree_span};
		while (current != end && index != 0u)
		{
			current = details::immutable_json_next(current);
			--index;
		}
		return current == end ? immutable_json_slice{} : immutable_json_slice{current};
	}

	[[nodiscard]] immutable_json_slice at(::std::size_t index) const
	{
		if (!is_array()) throw_json_error(json_errc::nonarray_indexing);
		auto result{(*this)[index]};
		if (!result.bound()) throw_json_error(json_errc::index_out_of_range);
		return result;
	}

	[[nodiscard]] immutable_json_slice find(string_view_type key) const noexcept;
	[[nodiscard]] bool contains(string_view_type key) const noexcept
	{
		return find(key).bound();
	}
	[[nodiscard]] immutable_json_slice at(string_view_type key) const
	{
		if (!is_object()) throw_json_error(json_errc::nonobject_indexing);
		auto result{find(key)};
		if (!result.bound()) throw_json_error(json_errc::key_not_found);
		return result;
	}

	[[nodiscard]] immutable_json_slice front() const
	{
		if (!is_array()) throw_json_error(json_errc::not_array);
		if (size() == 0u) throw_json_error(json_errc::index_out_of_range);
		return immutable_json_slice{node_ + 1u};
	}
	[[nodiscard]] immutable_json_slice back() const
	{
		if (!is_array()) throw_json_error(json_errc::not_array);
		if (size() == 0u) throw_json_error(json_errc::index_out_of_range);
		auto current{node_ + 1u};
		auto const end{node_ + node_->payload.subtree_span};
		auto previous{current};
		while (current != end)
		{
			previous = current;
			current = details::immutable_json_next(current);
		}
		return immutable_json_slice{previous};
	}

	[[nodiscard]] constexpr details::immutable_json_node const *
	native_handle() const noexcept
	{
		return node_;
	}
};

struct immutable_json_member
{
	::std::string_view key{};
	immutable_json_slice value{};
};

inline constexpr immutable_json_slice
immutable_json_array_iterator::operator*() const noexcept
{
	return immutable_json_slice{current_};
}

inline constexpr immutable_json_member
immutable_json_object_iterator::operator*() const noexcept
{
	return {{current_->payload.string,
			 details::immutable_json_length(*current_)},
		immutable_json_slice{current_ + 1u}};
}

inline immutable_json_slice
immutable_json_slice::find(string_view_type key) const noexcept
{
	if (!is_object()) return {};
	for (auto const member : as_object())
	{
		if (member.key == key) return member.value;
	}
	return {};
}

namespace details
{
class immutable_json_parser;
}

class immutable_json
{
	::std::vector<details::immutable_json_node> nodes_{};
	::std::vector<char> input_{};
	::std::size_t minimal_serialized_size_{};
	::std::size_t parsed_depth_{};

	friend class details::immutable_json_parser;

	void repair_string_pointers(char const *old_base) noexcept
	{
		if (old_base == nullptr || old_base == input_.data()) return;
		for (auto &value : nodes_)
		{
			if (details::immutable_json_kind(value) == json_kind::string)
			{
				auto const offset{static_cast<::std::size_t>(
					value.payload.string - old_base)};
				value.payload.string = input_.data() + offset;
			}
		}
	}

public:
	using char_type = char;
	using number_type = double;
	using integer_type = ::std::int_least64_t;
	using uinteger_type = ::std::uint_least64_t;
	using slice_type = immutable_json_slice;
	using const_slice_type = immutable_json_slice;

	immutable_json() = default;

	immutable_json(immutable_json const &other)
		: nodes_(other.nodes_), input_(other.input_),
		  minimal_serialized_size_(other.minimal_serialized_size_),
		  parsed_depth_(other.parsed_depth_)
	{
		repair_string_pointers(other.input_.data());
	}

	immutable_json(immutable_json &&other) noexcept
		: nodes_(::std::move(other.nodes_)), input_(::std::move(other.input_)),
		  minimal_serialized_size_(other.minimal_serialized_size_),
		  parsed_depth_(other.parsed_depth_)
	{
		other.nodes_.clear();
		other.input_.clear();
		other.minimal_serialized_size_ = 0u;
		other.parsed_depth_ = 0u;
	}

	immutable_json &operator=(immutable_json const &other)
	{
		if (this != __builtin_addressof(other))
		{
			immutable_json replacement{other};
			swap(replacement);
		}
		return *this;
	}

	immutable_json &operator=(immutable_json &&other) noexcept
	{
		if (this != __builtin_addressof(other))
		{
			immutable_json replacement{::std::move(other)};
			swap(replacement);
		}
		return *this;
	}

	void swap(immutable_json &other) noexcept
	{
		nodes_.swap(other.nodes_);
		input_.swap(other.input_);
		::std::swap(minimal_serialized_size_,
			other.minimal_serialized_size_);
		::std::swap(parsed_depth_, other.parsed_depth_);
	}

	friend void swap(immutable_json &left, immutable_json &right) noexcept
	{
		left.swap(right);
	}

	[[nodiscard]] constexpr immutable_json_slice slice() const & noexcept
	{
		return nodes_.empty() ? immutable_json_slice{}
			: immutable_json_slice{nodes_.data()};
	}
	immutable_json_slice slice() const && = delete;

	[[nodiscard]] bool empty() const noexcept { return nodes_.empty(); }
	[[nodiscard]] bool bound() const noexcept { return !nodes_.empty(); }
	[[nodiscard]] bool has_reference() const noexcept { return !nodes_.empty(); }
	[[nodiscard]] json_kind kind() const { return slice().kind(); }
	[[nodiscard]] ::std::size_t size() const noexcept { return slice().size(); }
	[[nodiscard]] bool value_empty() const noexcept { return slice().value_empty(); }

	[[nodiscard]] bool is_undefined() const noexcept { return slice().is_undefined(); }
	[[nodiscard]] bool is_null() const noexcept { return slice().is_null(); }
	[[nodiscard]] bool is_boolean() const noexcept { return slice().is_boolean(); }
	[[nodiscard]] bool is_number() const noexcept { return slice().is_number(); }
	[[nodiscard]] bool is_integer() const noexcept { return slice().is_integer(); }
	[[nodiscard]] bool is_uinteger() const noexcept { return slice().is_uinteger(); }
	[[nodiscard]] bool is_string() const noexcept { return slice().is_string(); }
	[[nodiscard]] bool is_array() const noexcept { return slice().is_array(); }
	[[nodiscard]] bool is_object() const noexcept { return slice().is_object(); }

	[[nodiscard]] bool undefined() const noexcept { return is_undefined(); }
	[[nodiscard]] bool null() const noexcept { return is_null(); }
	[[nodiscard]] bool boolean() const noexcept { return is_boolean(); }
	[[nodiscard]] bool number() const noexcept { return is_number(); }
	[[nodiscard]] bool integer() const noexcept { return is_integer(); }
	[[nodiscard]] bool uinteger() const noexcept { return is_uinteger(); }
	[[nodiscard]] bool string() const noexcept { return is_string(); }
	[[nodiscard]] bool array() const noexcept { return is_array(); }
	[[nodiscard]] bool object() const noexcept { return is_object(); }

	[[nodiscard]] bool get_boolean() const { return slice().get_boolean(); }
	[[nodiscard]] number_type get_number() const { return slice().get_number(); }
	[[nodiscard]] number_type as_number() const { return slice().as_number(); }
	[[nodiscard]] integer_type get_integer() const { return slice().get_integer(); }
	[[nodiscard]] uinteger_type get_uinteger() const { return slice().get_uinteger(); }
	[[nodiscard]] ::std::string_view get_string() const { return slice().get_string(); }

	[[nodiscard]] immutable_json_array_range as_array() const &
	{
		return slice().as_array();
	}
	immutable_json_array_range as_array() const && = delete;
	[[nodiscard]] immutable_json_object_range as_object() const &
	{
		return slice().as_object();
	}
	immutable_json_object_range as_object() const && = delete;

	[[nodiscard]] immutable_json_slice operator[](::std::size_t index) const & noexcept
	{
		return slice()[index];
	}
	immutable_json_slice operator[](::std::size_t) const && = delete;
	[[nodiscard]] immutable_json_slice operator[](::std::string_view key) const & noexcept
	{
		return slice().find(key);
	}
	immutable_json_slice operator[](::std::string_view) const && = delete;

	[[nodiscard]] immutable_json_slice at(::std::size_t index) const &
	{
		return slice().at(index);
	}
	immutable_json_slice at(::std::size_t) const && = delete;
	[[nodiscard]] immutable_json_slice at(::std::string_view key) const &
	{
		return slice().at(key);
	}
	immutable_json_slice at(::std::string_view) const && = delete;

	[[nodiscard]] immutable_json_slice find(::std::string_view key) const & noexcept
	{
		return slice().find(key);
	}
	immutable_json_slice find(::std::string_view) const && = delete;
	[[nodiscard]] bool contains(::std::string_view key) const noexcept
	{
		return slice().contains(key);
	}
	[[nodiscard]] immutable_json_slice front() const & { return slice().front(); }
	immutable_json_slice front() const && = delete;
	[[nodiscard]] immutable_json_slice back() const & { return slice().back(); }
	immutable_json_slice back() const && = delete;

	[[nodiscard]] ::std::size_t node_count() const noexcept { return nodes_.size(); }
	[[nodiscard]] ::std::size_t storage_bytes() const noexcept
	{
		return nodes_.size() * sizeof(details::immutable_json_node) +
			input_.size() * sizeof(char);
	}
	[[nodiscard]] constexpr ::std::size_t minimal_serialized_size() const noexcept
	{
		return minimal_serialized_size_;
	}
	[[nodiscard]] constexpr ::std::size_t parsed_depth() const noexcept
	{
		return parsed_depth_;
	}
};

} // namespace fast_io::json

namespace fast_io::json
{

namespace details
{
template <::std::integral char_type>
[[nodiscard]] ::std::size_t immutable_json_measure_document(
	immutable_json_slice, json_serialize_options const &);
template <::std::integral char_type>
[[nodiscard]] ::std::size_t immutable_json_bound_document(
	immutable_json_slice, json_serialize_options const &);
template <::std::integral char_type>
[[nodiscard]] char_type *immutable_json_write_contiguous(
	char_type *, immutable_json_slice, json_serialize_options const &);
template <::std::integral char_type>
[[nodiscard]] char_type *immutable_json_write_contiguous_precise(
	char_type *, ::std::size_t, immutable_json_slice,
	json_serialize_options const &);
template <::std::integral char_type, typename output_type>
void immutable_json_write_output(output_type &, immutable_json_slice,
	json_serialize_options const &);
} // namespace details

template <bool cached_root>
struct basic_immutable_json_print_view
{
	immutable_json_slice reference{};
	json_serialize_options options{};
	::std::size_t cached_size{};
	::std::size_t parsed_depth{};
	inline static constexpr bool has_cached_root_size{cached_root};
};

using immutable_json_print_view = basic_immutable_json_print_view<false>;
using immutable_json_cached_print_view = basic_immutable_json_print_view<true>;

template <::std::integral output_char_type, bool transcoding, bool cached_root>
struct basic_immutable_json_io_print_view
	: basic_immutable_json_print_view<cached_root>
{
	using base_type = basic_immutable_json_print_view<cached_root>;
	using output_char_type_alias = output_char_type;
	inline static constexpr bool is_transcoding{transcoding};

	static_assert(transcoding != ::std::same_as<
		::std::remove_cv_t<output_char_type>, char>);

	constexpr basic_immutable_json_io_print_view() noexcept = default;
	constexpr explicit basic_immutable_json_io_print_view(
		base_type value) noexcept : base_type(value)
	{}
};

template <::std::integral char_type, bool cached_root>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>,
	basic_immutable_json_print_view<cached_root> value) noexcept
{
	return basic_immutable_json_io_print_view<char_type,
		!::std::same_as<::std::remove_cv_t<char_type>, char>, cached_root>{value};
}

[[nodiscard]] inline constexpr immutable_json_cached_print_view
make_json_print_view(immutable_json const &value) noexcept
{
	return {value.slice(), {}, value.minimal_serialized_size(),
		value.parsed_depth()};
}

[[nodiscard]] inline constexpr immutable_json_print_view
make_json_print_view(immutable_json const &value,
	json_serialize_options options) noexcept
{
	return {value.slice(), options, 0u, value.parsed_depth()};
}

auto make_json_print_view(immutable_json &&) = delete;
auto make_json_print_view(immutable_json &&, json_serialize_options) = delete;
auto make_json_print_view(immutable_json const &&) = delete;
auto make_json_print_view(
	immutable_json const &&, json_serialize_options) = delete;

[[nodiscard]] inline constexpr immutable_json_print_view
make_json_print_view(immutable_json_slice value,
	json_serialize_options options = {}) noexcept
{
	return {value, options, 0u, 0u};
}

namespace details
{

template <::std::integral char_type, bool cached_root>
[[nodiscard]] inline ::std::size_t immutable_json_print_precise_size(
	basic_immutable_json_print_view<cached_root> const &view)
{
	if constexpr (cached_root &&
		::std::same_as<::std::remove_cv_t<char_type>, char>)
	{
		if (!view.options.pretty &&
			view.options.escape == json_escape_policy::minimal &&
			!view.options.escape_solidus &&
			view.parsed_depth <= view.options.max_depth)
		{
			return view.cached_size;
		}
	}
	return immutable_json_measure_document<char_type>(
		view.reference, view.options);
}

template <::std::integral char_type, bool cached_root>
class basic_immutable_json_print_context
{
	json_vector<char_type> materialized_{};
	::std::size_t position_{};
	bool initialized_{};
	bool finished_{};

public:
	basic_immutable_json_print_context() = default;

	inline ::fast_io::context_print_result<char_type *> print_context_define(
		basic_immutable_json_print_view<cached_root> const &view,
		char_type *output, char_type *output_last)
	{
		if (finished_ || output == output_last)
		{
			return {output, finished_};
		}
		if (!initialized_)
		{
			auto const size{
				immutable_json_print_precise_size<char_type>(view)};
			materialized_.resize(size);
			static_cast<void>(immutable_json_write_contiguous_precise(
				materialized_.data(), size, view.reference, view.options));
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

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr auto print_context_type(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return ::fast_io::io_type_t<
		details::basic_immutable_json_print_context<
			char_type, cached_root>>{};
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	constexpr ::std::size_t preferred_bytes{4096u};
	constexpr ::std::size_t result{preferred_bytes / sizeof(char_type)};
	return result == 0u ? 1u : result;
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>,
	basic_immutable_json_io_print_view<
		char_type, transcoding, cached_root> const &view)
{
	if constexpr (cached_root && !transcoding)
	{
		return details::immutable_json_print_precise_size<char_type>(view);
	}
	return details::immutable_json_bound_document<char_type>(
		view.reference, view.options);
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>,
	char_type *output,
	basic_immutable_json_io_print_view<
		char_type, transcoding, cached_root> const &view)
{
	return details::immutable_json_write_contiguous(
		output, view.reference, view.options);
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>,
	basic_immutable_json_io_print_view<
		char_type, transcoding, cached_root> const &view)
{
	return details::immutable_json_print_precise_size<char_type>(view);
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>,
	char_type *output, ::std::size_t size,
	basic_immutable_json_io_print_view<
		char_type, transcoding, cached_root> const &view)
{
	return details::immutable_json_write_contiguous_precise(
		output, size, view.reference, view.options);
}

template <::std::integral char_type, typename output_type,
	bool transcoding, bool cached_root>
inline void print_define(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>,
	output_type &output,
	basic_immutable_json_io_print_view<
		char_type, transcoding, cached_root> const &view)
{
	details::immutable_json_write_output<char_type>(
		output, view.reference, view.options);
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::true_type
print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	constexpr ::std::size_t preferred_bytes{4096u};
	constexpr ::std::size_t result{preferred_bytes / sizeof(char_type)};
	return result == 0u ? 1u : result;
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::true_type print_single_pass_staging_safe(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::true_type print_put_area_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::true_type print_buffered_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding, bool cached_root>
[[nodiscard]] inline constexpr ::std::true_type print_one_pass_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<
			char_type, transcoding, cached_root>>) noexcept
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type
print_concat_fresh_precise_resize_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<char_type, transcoding, true>>) noexcept
	requires(!transcoding)
{
	return {};
}

template <::std::integral char_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_concat_one_pass_preferred(
	::fast_io::io_reserve_type_t<char_type,
		basic_immutable_json_io_print_view<char_type, transcoding, false>>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, immutable_json const &value) noexcept
{
	return make_json_print_view(value);
}

[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, immutable_json_slice value) noexcept
{
	return make_json_print_view(value);
}

} // namespace fast_io::json

namespace fast_io::manipulators
{

[[nodiscard]] inline constexpr auto json(
	::fast_io::json::immutable_json const &value) noexcept
{
	return ::fast_io::json::make_json_print_view(value);
}

[[nodiscard]] inline constexpr auto json(
	::fast_io::json::immutable_json const &value,
	::fast_io::json::json_serialize_options options) noexcept
{
	return ::fast_io::json::make_json_print_view(value, options);
}

auto json(::fast_io::json::immutable_json &&) = delete;
auto json(::fast_io::json::immutable_json &&,
	::fast_io::json::json_serialize_options) = delete;
auto json(::fast_io::json::immutable_json const &&) = delete;
auto json(::fast_io::json::immutable_json const &&,
	::fast_io::json::json_serialize_options) = delete;

[[nodiscard]] inline constexpr auto pretty_json(
	::fast_io::json::immutable_json const &value,
	::std::size_t indent = 2u) noexcept
{
	::fast_io::json::json_serialize_options options{};
	options.pretty = true;
	options.indent_width = indent;
	return ::fast_io::json::make_json_print_view(value, options);
}

auto pretty_json(::fast_io::json::immutable_json &&,
	::std::size_t = 2u) = delete;
auto pretty_json(::fast_io::json::immutable_json const &&,
	::std::size_t = 2u) = delete;

} // namespace fast_io::manipulators

namespace fast_io::json::details
{

struct immutable_json_write_frame
{
	immutable_json_node const *end{};
	::std::size_t depth{};
	bool object{};
};

template <typename sink_type>
inline void immutable_json_write_document(
	sink_type &sink, ::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	auto current{root.native_handle()};
	if (current == nullptr)
	{
		::fast_io::json::throw_json_error(
			::fast_io::json::json_errc::is_empty);
	}
	::std::vector<immutable_json_write_frame> frames;
	frames.reserve((::std::min)(options.max_depth,
		static_cast<::std::size_t>(64u)));
	json_vector<typename sink_type::output_char_type> scalar_storage;
	::std::size_t current_depth{};

	auto emit_string = [&](immutable_json_node const *value) {
		::std::string_view string{value->payload.string,
			immutable_json_length(*value)};
		json_emit_dom_quoted_string(sink, string,
			(value->tag & immutable_json_direct_string_mask) != 0u,
			true, options);
	};

	for (;;)
	{
		switch (immutable_json_kind(*current))
		{
		case json_kind::null:
			json_sink_literal<u8'n', u8'u', u8'l', u8'l'>(sink);
			break;
		case json_kind::boolean:
			json_emit_boolean(sink, scalar_storage,
				current->payload.uinteger != 0u);
			break;
		case json_kind::number:
			json_emit_floating(sink, scalar_storage,
				current->payload.number);
			break;
		case json_kind::integer:
			json_emit_integer(sink, scalar_storage,
				current->payload.integer);
			break;
		case json_kind::uinteger:
			json_emit_integer(sink, scalar_storage,
				current->payload.uinteger);
			break;
		case json_kind::string:
			emit_string(current);
			break;
		case json_kind::array:
		{
			if (options.max_depth <= current_depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(
					::fast_io::json::json_errc::depth_exceeded);
			}
			json_sink_literal<u8'['>(sink);
			auto const *const end{current + current->payload.subtree_span};
			if (current + 1u == end)
			{
				json_sink_literal<u8']'>(sink);
				break;
			}
			frames.push_back({end, current_depth, false});
			if (options.pretty)
			{
				json_emit_layout(sink, current_depth + 1u, options);
			}
			current += 1u;
			++current_depth;
			continue;
		}
		case json_kind::object:
		{
			if (options.max_depth <= current_depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(
					::fast_io::json::json_errc::depth_exceeded);
			}
			json_sink_literal<u8'{'>(sink);
			auto const *const end{current + current->payload.subtree_span};
			auto key{current + 1u};
			while (key != end &&
				(key->tag & immutable_json_discarded_key_mask) != 0u)
			{
				key = immutable_json_next(key + 1u);
			}
			if (key == end)
			{
				json_sink_literal<u8'}'>(sink);
				break;
			}
			frames.push_back({end, current_depth, true});
			if (options.pretty)
			{
				json_emit_layout(sink, current_depth + 1u, options);
			}
			emit_string(key);
			json_sink_literal<u8':'>(sink);
			if (options.pretty) json_sink_literal<u8' '>(sink);
			current = key + 1u;
			++current_depth;
			continue;
		}
		case json_kind::undefined:
		default:
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_undefined);
		}

		auto next{immutable_json_next(current)};
		for (;;)
		{
			if (frames.empty()) return;
			auto const frame{frames.back()};
			if (!frame.object)
			{
				if (next != frame.end)
				{
					json_sink_literal<u8','>(sink);
					if (options.pretty)
					{
						json_emit_layout(sink, frame.depth + 1u, options);
					}
					current = next;
					current_depth = frame.depth + 1u;
					break;
				}
				if (options.pretty)
				{
					json_emit_layout(sink, frame.depth, options);
				}
				json_sink_literal<u8']'>(sink);
				frames.pop_back();
				next = frame.end;
				continue;
			}

			auto key{next};
			while (key != frame.end &&
				(key->tag & immutable_json_discarded_key_mask) != 0u)
			{
				key = immutable_json_next(key + 1u);
			}
			if (key != frame.end)
			{
				json_sink_literal<u8','>(sink);
				if (options.pretty)
				{
					json_emit_layout(sink, frame.depth + 1u, options);
				}
				emit_string(key);
				json_sink_literal<u8':'>(sink);
				if (options.pretty) json_sink_literal<u8' '>(sink);
				current = key + 1u;
				current_depth = frame.depth + 1u;
				break;
			}

			if (options.pretty)
			{
				json_emit_layout(sink, frame.depth, options);
			}
			json_sink_literal<u8'}'>(sink);
			frames.pop_back();
			next = frame.end;
		}
	}
}

template <::std::integral char_type>
[[nodiscard]] inline ::std::size_t immutable_json_measure_document(
	::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	basic_json_size_sink<char_type> sink;
	immutable_json_write_document(sink, root, options);
	return sink.size();
}

template <::std::integral char_type>
[[nodiscard]] inline ::std::size_t immutable_json_bound_document(
	::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	basic_json_bound_sink<char_type> sink;
	immutable_json_write_document(sink, root, options);
	return sink.size();
}

template <::std::integral char_type>
[[nodiscard]] inline char_type *immutable_json_write_contiguous(
	char_type *output, ::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	basic_json_contiguous_sink<char_type> sink{output};
	immutable_json_write_document(sink, root, options);
	return sink.current();
}

template <::std::integral char_type>
[[nodiscard]] inline char_type *immutable_json_write_contiguous_precise(
	char_type *output, ::std::size_t size,
	::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	/* The precise fast_io protocol gives this function exactly `size` writable
	   elements.  For the cached canonical form, the parser accumulated that
	   size from the same scalar/string spellings stored in the tape; for every
	   other option, immutable_json_measure_document runs this identical state
	   machine with a size sink first.  Induction over the preorder walk therefore
	   proves that each emitted fragment belongs to the measured concatenation
	   and the final cursor is output+size.  Per-fragment bounds checks add no
	   safety beyond that protocol and materially tax punctuation-heavy JSON. */
	basic_json_contiguous_sink<char_type> sink{output};
	immutable_json_write_document(sink, root, options);
	auto *const result{sink.current()};
	if (result != output + size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return result;
}

template <::std::integral char_type, typename output_type>
inline void immutable_json_write_output(output_type &output,
	::fast_io::json::immutable_json_slice root,
	::fast_io::json::json_serialize_options const &options)
{
	basic_json_output_sink<char_type, output_type> sink{output};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
	try
	{
		immutable_json_write_document(sink, root, options);
		sink.flush();
	}
	catch (...)
	{
		try { sink.flush(); } catch (...) {}
		throw;
	}
#else
	immutable_json_write_document(sink, root, options);
	sink.flush();
#endif
}

} // namespace fast_io::json::details

namespace fast_io::json::details
{

class immutable_json_parser
{
	::fast_io::json::immutable_json result_{};
	::fast_io::json::json_parse_options options_{};
	json_stage1_result stage1_{};
	::std::vector<::std::size_t> output_sizes_{};
	::std::size_t structural_index_{};
	::std::size_t cursor_{};
	::fast_io::json::json_errc error_code_{
		::fast_io::json::json_errc::none};
	::std::size_t error_offset_{};
	json_vector<char> number_storage_{};

	[[nodiscard]] bool fail(
		::fast_io::json::json_errc code, ::std::size_t offset) noexcept
	{
		if (error_code_ == ::fast_io::json::json_errc::none)
		{
			error_code_ = code;
			error_offset_ = offset;
		}
		return false;
	}

	[[nodiscard]] bool spaces(::std::size_t first, ::std::size_t last,
		::fast_io::json::json_errc code)
	{
		for (; first != last; ++first)
		{
			if (!json_ascii_space(static_cast<unsigned char>(result_.input_[first])))
			{
				return fail(code, first);
			}
		}
		return true;
	}

	[[nodiscard]] bool peek(::std::size_t &offset,
		::fast_io::json::json_errc eof_code,
		::fast_io::json::json_errc gap_code)
	{
		auto const input_size{result_.input_.size() - 1u};
		if (structural_index_ == stage1_.structurals.size())
		{
			if (!spaces(cursor_, input_size, gap_code)) return false;
			return fail(eof_code, input_size);
		}
		offset = stage1_.structurals[structural_index_];
		if (offset < cursor_)
		{
			return fail(::fast_io::json::json_errc::syntax_error, offset);
		}
		return spaces(cursor_, offset, gap_code);
	}

	void consume_punctuation(::std::size_t offset) noexcept
	{
		++structural_index_;
		cursor_ = offset + 1u;
	}

	[[nodiscard]] bool consume_literal(
		::std::size_t offset, ::std::string_view literal)
	{
		auto const input_size{result_.input_.size() - 1u};
		if (input_size - offset < literal.size())
		{
			return fail(::fast_io::json::json_errc::unexpected_end, input_size);
		}
		for (::std::size_t index{}; index != literal.size(); ++index)
		{
			if (result_.input_[offset + index] != literal[index])
			{
				return fail(::fast_io::json::json_errc::invalid_literal,
					offset + index);
			}
		}
		auto const after{offset + literal.size()};
		if (after != input_size &&
			!json_number_is_delimiter(result_.input_[after]))
		{
			return fail(::fast_io::json::json_errc::invalid_literal, after);
		}
		cursor_ = after;
		return true;
	}

	[[nodiscard]] static constexpr unsigned hex_value(char value) noexcept
	{
		if (value >= '0' && value <= '9')
			return static_cast<unsigned>(value - '0');
		if (value >= 'a' && value <= 'f')
			return static_cast<unsigned>(value - 'a') + 10u;
		if (value >= 'A' && value <= 'F')
			return static_cast<unsigned>(value - 'A') + 10u;
		return 0xFFu;
	}

	[[nodiscard]] bool read_hex_quad(char const *source,
		::std::uint_least32_t &value, ::std::size_t error_base)
	{
		value = 0u;
		for (unsigned index{}; index != 4u; ++index)
		{
			auto const digit{hex_value(source[index])};
			if (digit == 0xFFu)
			{
				return fail(
					::fast_io::json::json_errc::invalid_unicode_escape,
					error_base + index);
			}
			value = static_cast<::std::uint_least32_t>(
				(value << 4u) | digit);
		}
		return true;
	}

	static void encode_utf8(char *&output,
		::std::uint_least32_t code_point) noexcept
	{
		if (code_point <= 0x7Fu)
		{
			*output++ = static_cast<char>(code_point);
		}
		else if (code_point <= 0x7FFu)
		{
			*output++ = static_cast<char>(0xC0u | (code_point >> 6u));
			*output++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
		}
		else if (code_point <= 0xFFFFu)
		{
			*output++ = static_cast<char>(0xE0u | (code_point >> 12u));
			*output++ = static_cast<char>(
				0x80u | ((code_point >> 6u) & 0x3Fu));
			*output++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
		}
		else
		{
			*output++ = static_cast<char>(0xF0u | (code_point >> 18u));
			*output++ = static_cast<char>(
				0x80u | ((code_point >> 12u) & 0x3Fu));
			*output++ = static_cast<char>(
				0x80u | ((code_point >> 6u) & 0x3Fu));
			*output++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
		}
	}

	[[nodiscard]] bool append_string_node(
		::std::size_t quote, ::std::size_t &node_index)
	{
		auto *const base{result_.input_.data()};
		auto *read{base + quote + 1u};
		auto *write{read};
		auto *const decoded_begin{write};
		auto *const last{base + result_.input_.size() - 1u};
		bool compact_direct{true};
		::std::size_t encoded_size{2u};

		for (;;)
		{
			auto const special{json_find_string_special(read, last)};
			if (special == last)
			{
				return fail(::fast_io::json::json_errc::unexpected_end,
					static_cast<::std::size_t>(last - base));
			}
			auto const raw_size{static_cast<::std::size_t>(special - read)};
			if (raw_size != 0u)
			{
				if (write != read)
				{
					__builtin_memmove(static_cast<void *>(write),
						static_cast<void const *>(read), raw_size);
				}
				write += raw_size;
				encoded_size += raw_size;
			}
			read = special + 1u;
			if (*special == '"') break;

			if (read == last)
			{
				return fail(::fast_io::json::json_errc::unexpected_end,
					static_cast<::std::size_t>(read - base));
			}
			auto const escaped{*read++};
			::std::uint_least32_t code_point{};
			switch (escaped)
			{
			case '"': code_point = '"'; break;
			case '\\': code_point = '\\'; break;
			case '/': code_point = '/'; break;
			case 'b': code_point = '\b'; break;
			case 'f': code_point = '\f'; break;
			case 'n': code_point = '\n'; break;
			case 'r': code_point = '\r'; break;
			case 't': code_point = '\t'; break;
			case 'u':
			{
				if (last - read < 4)
				{
					return fail(::fast_io::json::json_errc::unexpected_end,
						static_cast<::std::size_t>(last - base));
				}
				if (!read_hex_quad(read, code_point,
						static_cast<::std::size_t>(read - base)))
				{
					return false;
				}
				read += 4;
				if (code_point >= 0xD800u && code_point <= 0xDBFFu)
				{
					if (last - read < 6 || read[0] != '\\' || read[1] != 'u')
					{
						return fail(
							::fast_io::json::json_errc::invalid_unicode_escape,
							static_cast<::std::size_t>(read - base));
					}
					::std::uint_least32_t low_surrogate{};
					if (!read_hex_quad(read + 2, low_surrogate,
							static_cast<::std::size_t>(read + 2 - base)))
					{
						return false;
					}
					if (low_surrogate < 0xDC00u || low_surrogate > 0xDFFFu)
					{
						return fail(
							::fast_io::json::json_errc::invalid_unicode_escape,
							static_cast<::std::size_t>(read + 2 - base));
					}
					code_point = static_cast<::std::uint_least32_t>(
						0x10000u + ((code_point - 0xD800u) << 10u) +
						(low_surrogate - 0xDC00u));
					read += 6;
				}
				else if (code_point >= 0xDC00u && code_point <= 0xDFFFu)
				{
					return fail(
						::fast_io::json::json_errc::invalid_unicode_escape,
						static_cast<::std::size_t>(read - 4 - base));
				}
				break;
			}
			default:
				return fail(::fast_io::json::json_errc::invalid_escape,
					static_cast<::std::size_t>(read - 1 - base));
			}

			compact_direct = compact_direct && code_point >= 0x20u &&
				code_point != 0x22u && code_point != 0x5Cu;
			if (code_point == 0x22u || code_point == 0x5Cu ||
				code_point < 0x20u)
			{
				encoded_size += (code_point == '\b' || code_point == '\f' ||
					code_point == '\n' || code_point == '\r' ||
					code_point == '\t' || code_point == 0x22u ||
					code_point == 0x5Cu) ? 2u : 6u;
			}
			else if (code_point <= 0x7Fu) ++encoded_size;
			else if (code_point <= 0x7FFu) encoded_size += 2u;
			else if (code_point <= 0xFFFFu) encoded_size += 3u;
			else encoded_size += 4u;
			encode_utf8(write, code_point);
		}

		*write = 0;
		cursor_ = static_cast<::std::size_t>(read - base);
		node_index = result_.nodes_.size();
		auto const decoded_size{
			static_cast<::std::size_t>(write - decoded_begin)};
		constexpr auto maximum_length{
			(::std::numeric_limits<::std::uint_least64_t>::max)() >>
			immutable_json_length_shift};
		if (maximum_length < decoded_size) [[unlikely]]
		{
			return fail(::fast_io::json::json_errc::syntax_error, quote);
		}
		immutable_json_node value{};
		value.tag =
			(static_cast<::std::uint_least64_t>(decoded_size) <<
			 immutable_json_length_shift) |
			static_cast<::std::uint_least64_t>(json_kind::string) |
			(compact_direct ? immutable_json_direct_string_mask : 0u);
		value.payload.string = decoded_begin;
		result_.nodes_.push_back(value);
		output_sizes_.push_back(encoded_size);
		return true;
	}

	[[nodiscard]] bool append_number_node(
		::std::size_t offset, ::std::size_t &node_index)
	{
		auto const *const base{result_.input_.data()};
		auto const *const first{base + offset};
		auto const *const last{base + result_.input_.size() - 1u};
		auto const scanned{scan_json_number(first, last)};
		if (scanned.code != ::fast_io::parse_code::ok)
		{
			return fail(scanned.code == ::fast_io::parse_code::end_of_file
				? ::fast_io::json::json_errc::unexpected_end
				: ::fast_io::json::json_errc::invalid_number,
				static_cast<::std::size_t>(scanned.iter - base));
		}

		immutable_json_node value{};
		::std::size_t output_size{};
		if (scanned.token.kind == json_number_token_kind::floating ||
			options_.integer_preference ==
				::fast_io::json::json_integer_preference::prefer_floating)
		{
			value.tag = static_cast<::std::uint_least64_t>(json_kind::number);
			auto const code{parse_json_number_into(
				scanned.token, value.payload.number)};
			if (code != ::fast_io::parse_code::ok)
			{
				return fail(code == ::fast_io::parse_code::overflow
					? ::fast_io::json::json_errc::number_overflow
					: ::fast_io::json::json_errc::invalid_number, offset);
			}
			output_size = json_measure_scalar<char>(number_storage_,
				::fast_io::json::basic_json_floating_scalar<double>{
					__builtin_addressof(value.payload.number)});
		}
		else if (scanned.token.negative)
		{
			value.tag = static_cast<::std::uint_least64_t>(json_kind::integer);
			auto const code{parse_json_number_into(
				scanned.token, value.payload.integer)};
			if (code != ::fast_io::parse_code::ok)
			{
				return fail(code == ::fast_io::parse_code::overflow
					? ::fast_io::json::json_errc::integer_overflow
					: ::fast_io::json::json_errc::invalid_number, offset);
			}
			char buffer[32u];
			output_size = static_cast<::std::size_t>(::fast_io::to_chars(
				buffer, buffer + sizeof(buffer), value.payload.integer).ptr - buffer);
		}
		else if (options_.integer_preference ==
			::fast_io::json::json_integer_preference::prefer_unsigned)
		{
			value.tag = static_cast<::std::uint_least64_t>(json_kind::uinteger);
			auto const code{parse_json_number_into(
				scanned.token, value.payload.uinteger)};
			if (code != ::fast_io::parse_code::ok)
			{
				return fail(code == ::fast_io::parse_code::overflow
					? ::fast_io::json::json_errc::uinteger_overflow
					: ::fast_io::json::json_errc::invalid_number, offset);
			}
			char buffer[32u];
			output_size = static_cast<::std::size_t>(::fast_io::to_chars(
				buffer, buffer + sizeof(buffer), value.payload.uinteger).ptr - buffer);
		}
		else
		{
			::std::int_least64_t signed_value{};
			auto const signed_code{
				parse_json_number_into(scanned.token, signed_value)};
			if (signed_code == ::fast_io::parse_code::ok)
			{
				value.tag = static_cast<::std::uint_least64_t>(json_kind::integer);
				value.payload.integer = signed_value;
				char buffer[32u];
				output_size = static_cast<::std::size_t>(::fast_io::to_chars(
					buffer, buffer + sizeof(buffer), signed_value).ptr - buffer);
			}
			else if (signed_code == ::fast_io::parse_code::overflow)
			{
				value.tag = static_cast<::std::uint_least64_t>(json_kind::uinteger);
				auto const unsigned_code{parse_json_number_into(
					scanned.token, value.payload.uinteger)};
				if (unsigned_code != ::fast_io::parse_code::ok)
				{
					return fail(unsigned_code == ::fast_io::parse_code::overflow
						? ::fast_io::json::json_errc::uinteger_overflow
						: ::fast_io::json::json_errc::invalid_number, offset);
				}
				char buffer[32u];
				output_size = static_cast<::std::size_t>(::fast_io::to_chars(
					buffer, buffer + sizeof(buffer),
					value.payload.uinteger).ptr - buffer);
			}
			else
			{
				return fail(::fast_io::json::json_errc::invalid_number, offset);
			}
		}

		cursor_ = static_cast<::std::size_t>(scanned.token.last - base);
		node_index = result_.nodes_.size();
		result_.nodes_.push_back(value);
		output_sizes_.push_back(output_size);
		return true;
	}

	[[nodiscard]] ::std::size_t find_key_index(
		::std::size_t object_index, ::std::size_t key_index) const noexcept
	{
		auto const *current{result_.nodes_.data() + object_index + 1u};
		auto const *const target{result_.nodes_.data() + key_index};
		while (current != target)
		{
			if ((current->tag & immutable_json_discarded_key_mask) == 0u &&
				immutable_json_length(*current) == immutable_json_length(*target) &&
				::std::memcmp(current->payload.string, target->payload.string,
					immutable_json_length(*target)) == 0)
			{
				return static_cast<::std::size_t>(
					current - result_.nodes_.data());
			}
			current = immutable_json_next(current + 1u);
		}
		return (::std::numeric_limits<::std::size_t>::max)();
	}


	enum class iterative_phase : unsigned char
	{
		array_value_or_end,
		array_value,
		array_wait_value,
		array_comma_or_end,
		object_key_or_end,
		object_key,
		object_colon,
		object_wait_value,
		object_comma_or_end
	};

	struct iterative_frame
	{
		iterative_phase phase{};
		::std::size_t container_index{};
		::std::size_t key_index{};
		::std::size_t duplicate_index{
			(::std::numeric_limits<::std::size_t>::max)()};
		::std::size_t count{};
		::std::size_t payload_size{};
	};

	[[nodiscard]] bool start_iterative_container(
		::std::vector<iterative_frame> &frames, bool object,
		::std::size_t opening, ::std::size_t &node_index)
	{
		if (frames.size() >= options_.max_depth)
		{
			return fail(::fast_io::json::json_errc::depth_exceeded, opening);
		}
		node_index = result_.nodes_.size();
		result_.nodes_.emplace_back();
		output_sizes_.push_back(0u);
		cursor_ = opening + 1u;
		frames.push_back({object ? iterative_phase::object_key_or_end
								 : iterative_phase::array_value_or_end,
			node_index});
		result_.parsed_depth_ =
			(::std::max)(result_.parsed_depth_, frames.size());
		return true;
	}

	[[nodiscard]] bool start_iterative_value(
		::std::vector<iterative_frame> &frames,
		::std::size_t &node_index, bool &complete)
	{
		::std::size_t offset{};
		if (!peek(offset, ::fast_io::json::json_errc::unexpected_end,
				::fast_io::json::json_errc::unexpected_token))
		{
			return false;
		}
		++structural_index_;
		auto const value{result_.input_[offset]};
		complete = true;
		if (value == 'n')
		{
			if (!consume_literal(offset, "null")) return false;
			node_index = result_.nodes_.size();
			immutable_json_node node{};
			node.tag = static_cast<::std::uint_least64_t>(json_kind::null);
			result_.nodes_.push_back(node);
			output_sizes_.push_back(4u);
			return true;
		}
		if (value == 't' || value == 'f')
		{
			auto const literal{value == 't'
				? ::std::string_view{"true"} : ::std::string_view{"false"}};
			if (!consume_literal(offset, literal)) return false;
			node_index = result_.nodes_.size();
			immutable_json_node node{};
			node.tag = static_cast<::std::uint_least64_t>(json_kind::boolean);
			node.payload.uinteger = value == 't';
			result_.nodes_.push_back(node);
			output_sizes_.push_back(literal.size());
			return true;
		}
		if (value == '"') return append_string_node(offset, node_index);
		if (value == '-' || (value >= '0' && value <= '9'))
		{
			return append_number_node(offset, node_index);
		}
		if (value == '[' || value == '{')
		{
			complete = false;
			return start_iterative_container(
				frames, value == '{', offset, node_index);
		}
		return fail(::fast_io::json::json_errc::unexpected_token, offset);
	}

	void finalize_iterative_container(iterative_frame const &frame) noexcept
	{
		auto &container{result_.nodes_[frame.container_index]};
		auto const object{frame.phase == iterative_phase::object_key_or_end ||
			frame.phase == iterative_phase::object_key ||
			frame.phase == iterative_phase::object_colon ||
			frame.phase == iterative_phase::object_wait_value ||
			frame.phase == iterative_phase::object_comma_or_end};
		container.tag =
			(static_cast<::std::uint_least64_t>(frame.count) <<
			 immutable_json_length_shift) |
			static_cast<::std::uint_least64_t>(
				object ? json_kind::object : json_kind::array);
		container.payload.subtree_span =
			result_.nodes_.size() - frame.container_index;
		output_sizes_[frame.container_index] = 2u + frame.payload_size +
			(frame.count == 0u ? 0u : frame.count - 1u);
	}

	[[nodiscard]] bool accept_iterative_child(
		iterative_frame &parent, ::std::size_t child_index)
	{
		if (parent.phase == iterative_phase::array_wait_value)
		{
			parent.payload_size += output_sizes_[child_index];
			++parent.count;
			parent.phase = iterative_phase::array_comma_or_end;
			return true;
		}
		if (parent.phase != iterative_phase::object_wait_value) [[unlikely]]
		{
			return fail(::fast_io::json::json_errc::syntax_error, cursor_);
		}

		auto const pair_size{output_sizes_[parent.key_index] + 1u +
			output_sizes_[child_index]};
		constexpr auto no_duplicate{
			(::std::numeric_limits<::std::size_t>::max)()};
		if (parent.duplicate_index == no_duplicate)
		{
			parent.payload_size += pair_size;
			++parent.count;
		}
		else if (options_.duplicate_keys ==
			::fast_io::json::json_duplicate_key_policy::keep_first)
		{
			result_.nodes_[parent.key_index].tag |=
				immutable_json_discarded_key_mask;
		}
		else
		{
			auto const previous_value{parent.duplicate_index + 1u};
			auto const previous_pair{
				output_sizes_[parent.duplicate_index] + 1u +
				output_sizes_[previous_value]};
			parent.payload_size -= previous_pair;
			parent.payload_size += pair_size;
			result_.nodes_[parent.duplicate_index].tag |=
				immutable_json_discarded_key_mask;
		}
		parent.phase = iterative_phase::object_comma_or_end;
		return true;
	}

	[[nodiscard]] bool parse_iterative_root(::std::size_t &root_index)
	{
		::std::vector<iterative_frame> frames;
		frames.reserve((::std::min)(options_.max_depth,
			static_cast<::std::size_t>(64u)));
		constexpr auto no_index{
			(::std::numeric_limits<::std::size_t>::max)()};
		::std::size_t completed_index{no_index};
		bool root_started{};

		for (;;)
		{
			if (completed_index != no_index)
			{
				if (frames.empty()) break;
				if (!accept_iterative_child(frames.back(), completed_index))
				{
					return false;
				}
				completed_index = no_index;
				continue;
			}

			if (!root_started)
			{
				bool complete{};
				if (!start_iterative_value(frames, root_index, complete))
				{
					return false;
				}
				root_started = true;
				if (complete) completed_index = root_index;
				continue;
			}

			if (frames.empty()) [[unlikely]]
			{
				return fail(::fast_io::json::json_errc::syntax_error, cursor_);
			}
			auto &frame{frames.back()};
			::std::size_t token{};
			switch (frame.phase)
			{
			case iterative_phase::array_value_or_end:
			case iterative_phase::array_value:
			{
				if (!peek(token, ::fast_io::json::json_errc::unexpected_end,
						::fast_io::json::json_errc::unexpected_token))
				{
					return false;
				}
				if (result_.input_[token] == ']')
				{
					if (frame.phase == iterative_phase::array_value)
					{
						return fail(
							::fast_io::json::json_errc::unexpected_token, token);
					}
					consume_punctuation(token);
					auto const finished{frame};
					finalize_iterative_container(finished);
					frames.pop_back();
					completed_index = finished.container_index;
					continue;
				}
				frame.phase = iterative_phase::array_wait_value;
				::std::size_t child{};
				bool complete{};
				if (!start_iterative_value(frames, child, complete)) return false;
				if (complete) completed_index = child;
				continue;
			}
			case iterative_phase::array_comma_or_end:
				if (!peek(token, ::fast_io::json::json_errc::unexpected_end,
						::fast_io::json::json_errc::expected_comma_or_end))
				{
					return false;
				}
				if (result_.input_[token] == ',')
				{
					consume_punctuation(token);
					frame.phase = iterative_phase::array_value;
					continue;
				}
				if (result_.input_[token] == ']')
				{
					consume_punctuation(token);
					auto const finished{frame};
					finalize_iterative_container(finished);
					frames.pop_back();
					completed_index = finished.container_index;
					continue;
				}
				return fail(
					::fast_io::json::json_errc::expected_comma_or_end, token);

			case iterative_phase::object_key_or_end:
			case iterative_phase::object_key:
				if (!peek(token, ::fast_io::json::json_errc::unexpected_end,
						::fast_io::json::json_errc::unexpected_token))
				{
					return false;
				}
				if (result_.input_[token] == '}')
				{
					if (frame.phase == iterative_phase::object_key)
					{
						return fail(
							::fast_io::json::json_errc::unexpected_token, token);
					}
					consume_punctuation(token);
					auto const finished{frame};
					finalize_iterative_container(finished);
					frames.pop_back();
					completed_index = finished.container_index;
					continue;
				}
				if (result_.input_[token] != '"')
				{
					return fail(
						::fast_io::json::json_errc::unexpected_token, token);
				}
				++structural_index_;
				if (!append_string_node(token, frame.key_index)) return false;
				frame.duplicate_index = find_key_index(
					frame.container_index, frame.key_index);
				if (frame.duplicate_index != no_index &&
					options_.duplicate_keys ==
						::fast_io::json::json_duplicate_key_policy::reject)
				{
					return fail(
						::fast_io::json::json_errc::duplicate_key, token);
				}
				frame.phase = iterative_phase::object_colon;
				continue;

			case iterative_phase::object_colon:
				if (!peek(token, ::fast_io::json::json_errc::unexpected_end,
						::fast_io::json::json_errc::expected_colon))
				{
					return false;
				}
				if (result_.input_[token] != ':')
				{
					return fail(
						::fast_io::json::json_errc::expected_colon, token);
				}
				consume_punctuation(token);
				frame.phase = iterative_phase::object_wait_value;
				{
					::std::size_t child{};
					bool complete{};
					if (!start_iterative_value(frames, child, complete)) return false;
					if (complete) completed_index = child;
				}
				continue;

			case iterative_phase::object_comma_or_end:
				if (!peek(token, ::fast_io::json::json_errc::unexpected_end,
						::fast_io::json::json_errc::expected_comma_or_end))
				{
					return false;
				}
				if (result_.input_[token] == ',')
				{
					consume_punctuation(token);
					frame.phase = iterative_phase::object_key;
					continue;
				}
				if (result_.input_[token] == '}')
				{
					consume_punctuation(token);
					auto const finished{frame};
					finalize_iterative_container(finished);
					frames.pop_back();
					completed_index = finished.container_index;
					continue;
				}
				return fail(
					::fast_io::json::json_errc::expected_comma_or_end, token);

			case iterative_phase::array_wait_value:
			case iterative_phase::object_wait_value:
			default:
				return fail(
					::fast_io::json::json_errc::syntax_error, cursor_);
			}
		}
		return true;
	}

public:
	immutable_json_parser(char const *first, char const *last,
		::fast_io::json::json_parse_options options)
		: options_(options)
	{
		if (first != last)
		{
			result_.input_.assign(first, last);
		}
		result_.input_.push_back(0);
		stage1_ = json_build_structural_index(
			result_.input_.data(),
			result_.input_.data() + (result_.input_.size() - 1u));
		/* Every tape cell begins at a stage-one token.  Half the structural
		   count is a close estimate for ordinary object-heavy documents and
		   avoids the repeated node relocation that defeats a compact tape. */
		auto const reservation{(stage1_.structurals.size() + 1u) / 2u};
		result_.nodes_.reserve(reservation);
		output_sizes_.reserve(reservation);
	}

	[[nodiscard]] bool parse()
	{
		if (!stage1_)
		{
			switch (stage1_.error)
			{
			case json_stage1_errc::invalid_unicode:
				return fail(::fast_io::json::json_errc::invalid_utf8,
					stage1_.error_offset);
			case json_stage1_errc::unescaped_control_character:
				return fail(
					::fast_io::json::json_errc::unescaped_control_character,
					stage1_.error_offset);
			case json_stage1_errc::unterminated_string:
				return fail(::fast_io::json::json_errc::unexpected_end,
					stage1_.error_offset);
			default:
				return fail(::fast_io::json::json_errc::syntax_error,
					stage1_.error_offset);
			}
		}
		::std::size_t root_index{};
		if (!parse_iterative_root(root_index)) return false;
		if (structural_index_ != stage1_.structurals.size())
		{
			return fail(::fast_io::json::json_errc::trailing_data,
				stage1_.structurals[structural_index_]);
		}
		if (!spaces(cursor_, result_.input_.size() - 1u,
				::fast_io::json::json_errc::trailing_data))
		{
			return false;
		}
		result_.minimal_serialized_size_ = output_sizes_[root_index];
		return true;
	}

	[[nodiscard]] ::fast_io::json::immutable_json &&take() noexcept
	{
		return ::std::move(result_);
	}
	[[nodiscard]] ::fast_io::json::json_errc error_code() const noexcept
	{
		return error_code_;
	}
	[[nodiscard]] ::std::size_t error_offset() const noexcept
	{
		return error_offset_;
	}
};

} // namespace fast_io::json::details

namespace fast_io::json
{

[[nodiscard]] inline json_parse_result<char const *>
try_parse_immutable_json(immutable_json &destination,
	char const *first, char const *last, json_parse_options options = {})
{
	details::immutable_json_parser parser{first, last, options};
	if (!parser.parse())
	{
		auto const offset{parser.error_offset()};
		auto const *const position{offset == 0u ? first : first + offset};
		return details::json_make_parse_result(
			first, position, parser.error_code());
	}
	auto replacement{parser.take()};
	destination.swap(replacement);
	return details::json_make_success_parse_result(first, last);
}

[[nodiscard]] inline json_parse_result<char const *>
try_parse_immutable_json(immutable_json &destination,
	::std::string_view input, json_parse_options options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return try_parse_immutable_json(destination, first, last, options);
}

[[nodiscard]] inline immutable_json
parse_immutable_json(char const *first, char const *last,
	json_parse_options options = {})
{
	immutable_json result;
	auto const parsed{try_parse_immutable_json(result, first, last, options)};
	if (!parsed) throw_json_error(parsed.code);
	return result;
}

[[nodiscard]] inline immutable_json
parse_immutable_json(::std::string_view input,
	json_parse_options options = {})
{
	auto const *first{input.data()};
	auto const *last{input.empty() ? first : first + input.size()};
	return parse_immutable_json(first, last, options);
}

/* The explicit immutable names are the primary public entry points.  This
   overload also lets generic application code call try_parse_json() without
   accidentally instantiating the mutable node builder. */
[[nodiscard]] inline json_parse_result<char const *>
try_parse_json(immutable_json &destination,
	char const *first, char const *last, json_parse_options options = {})
{
	return try_parse_immutable_json(destination, first, last, options);
}

[[nodiscard]] inline json_parse_result<char const *>
try_parse_json(immutable_json &destination, ::std::string_view input,
	json_parse_options options = {})
{
	return try_parse_immutable_json(destination, input, options);
}

template <>
[[nodiscard]] inline immutable_json parse_json<immutable_json, char>(
	char const *first, char const *last, json_parse_options options)
{
	return parse_immutable_json(first, last, options);
}

template <>
[[nodiscard]] inline immutable_json
parse_json<immutable_json, char, ::std::char_traits<char>>(
	::std::basic_string_view<char, ::std::char_traits<char>> input,
	json_parse_options options)
{
	return parse_immutable_json(input, options);
}

template <typename json_type>
struct immutable_json_scan_proxy;

template <>
struct immutable_json_scan_proxy<immutable_json>
{
	using manip_tag = ::fast_io::manip_tag_t;
	immutable_json *value{};
	json_parse_options options{};
};

[[nodiscard]] inline constexpr immutable_json_scan_proxy<immutable_json>
scan_immutable_json(immutable_json &value,
	json_parse_options options = {}) noexcept
{
	return {__builtin_addressof(value), options};
}

template <::fast_io::details::character char_type>
	requires(::std::same_as<::std::remove_cv_t<char_type>, char>)
[[nodiscard]] inline ::fast_io::parse_result<char_type const *>
scan_contiguous_define(
	::fast_io::io_reserve_type_t<char_type,
		immutable_json_scan_proxy<immutable_json>>,
	char_type const *first, char_type const *last,
	immutable_json_scan_proxy<immutable_json> proxy)
{
	auto const result{try_parse_immutable_json(
		*proxy.value, first, last, proxy.options)};
	if (result) return {result.iter, ::fast_io::parse_code::ok};
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

[[nodiscard]] inline constexpr auto scan_alias_define(
	::fast_io::io_alias_t, immutable_json &value) noexcept
{
	return immutable_json_scan_proxy<immutable_json>{
		__builtin_addressof(value), {}};
}

} // namespace fast_io::json
