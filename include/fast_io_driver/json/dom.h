#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "../../fast_io_dsal/hash_map.h"
#include "concepts.h"
#include "error.h"
#include "escape.h"
#include "options.h"
#include "segmented_array.h"

namespace fast_io::json
{

struct nulljson_t
{
	explicit constexpr nulljson_t() noexcept = default;
};

struct disable_integer_t
{
	explicit constexpr disable_integer_t() noexcept = default;
};

struct disable_uinteger_t
{
	explicit constexpr disable_uinteger_t() noexcept = default;
};

inline constexpr nulljson_t nulljson{};

enum class json_kind : unsigned char
{
	undefined,
	null,
	boolean,
	number,
	integer,
	uinteger,
	string,
	array,
	object
};

template <typename Char = char, typename Number = double,
		  typename Integer = ::std::int64_t, typename UInteger = ::std::uint64_t,
		  template <typename> class Allocator = ::std::allocator,
		  template <typename, typename> class Array = basic_json_segmented_array>
class basic_json_node;

template <typename Node = basic_json_node<>>
class basic_json;

template <typename Node = basic_json_node<>>
class basic_const_json_slice;

template <typename Node = basic_json_node<>>
class basic_json_slice;

} // namespace fast_io::json

namespace fast_io::json
{

namespace detail
{

struct basic_json_access;

struct json_slice_pointer_arg_t
{
	explicit constexpr json_slice_pointer_arg_t() noexcept = default;
};

inline constexpr json_slice_pointer_arg_t json_slice_pointer_arg{};

template <typename Size, typename Position>
	requires(::fast_io::details::my_unsigned_integral<Size> &&
			 ::fast_io::details::my_integral<::std::remove_cvref_t<Position>> &&
			 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
[[nodiscard]] constexpr bool json_index_in_range(Position position, Size size) noexcept
{
	using position_type = ::std::remove_cvref_t<Position>;
	if constexpr (::fast_io::details::my_signed_integral<position_type>)
	{
		if (position < 0)
		{
			return false;
		}
	}
	using unsigned_position_type = ::fast_io::details::my_make_unsigned_t<position_type>;
	auto const unsigned_position{static_cast<unsigned_position_type>(position)};
	if constexpr (sizeof(unsigned_position_type) > sizeof(Size))
	{
		return unsigned_position < static_cast<unsigned_position_type>(size);
	}
	else
	{
		return static_cast<Size>(unsigned_position) < size;
	}
}

template <typename Size, typename Position>
	requires(::fast_io::details::my_unsigned_integral<Size> &&
			 ::fast_io::details::my_integral<::std::remove_cvref_t<Position>> &&
			 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
[[nodiscard]] constexpr bool json_index_at_most_size(
	Position position, Size size) noexcept
{
	using position_type = ::std::remove_cvref_t<Position>;
	if constexpr (::fast_io::details::my_signed_integral<position_type>)
	{
		if (position < 0)
		{
			return false;
		}
	}
	using unsigned_position_type =
		::fast_io::details::my_make_unsigned_t<position_type>;
	auto const unsigned_position{
		static_cast<unsigned_position_type>(position)};
	if constexpr (sizeof(unsigned_position_type) > sizeof(Size))
	{
		return unsigned_position <=
			static_cast<unsigned_position_type>(size);
	}
	else
	{
		return static_cast<Size>(unsigned_position) <= size;
	}
}

template <typename Object, typename Key>
[[nodiscard]] constexpr auto json_object_find(Object &object, Key const &key)
{
	if constexpr (::std::is_pointer_v<::std::remove_cvref_t<Key>>)
	{
		if (key == nullptr) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::null_pointer);
		}
	}
	using object_type = ::std::remove_cvref_t<Object>;
	using key_type = typename object_type::key_type;
	if constexpr (requires { object.find(key); })
	{
		return object.find(key);
	}
	else
	{
		return object.find(key_type(key));
	}
}

template <typename Object, typename Key, typename... Args>
[[nodiscard]] constexpr auto json_object_emplace_known_absent(
	Object &object, Key &&key, Args &&...args)
{
	if constexpr (requires {
		object.emplace_unique_known_absent(::std::forward<Key>(key),
			::std::forward<Args>(args)...);
	})
	{
		return object.emplace_unique_known_absent(::std::forward<Key>(key),
			::std::forward<Args>(args)...);
	}
	else
	{
		return object.try_emplace(::std::forward<Key>(key),
			::std::forward<Args>(args)...).first;
	}
}

/*
Content hash/equality for JSON object keys.  Both call operators are
transparent: a stored allocator-aware basic_string can be queried by another
string allocator, basic_string_view, a string literal, or a C string without
manufacturing a temporary key.  Equal code-unit sequences deliberately have
the same hash regardless of their owning/view type.
*/
template <typename Char>
struct basic_json_string_hash
{
	using is_transparent = void;

	template <typename StringLike>
		requires(::std::constructible_from<::std::basic_string_view<Char>,
			StringLike const &>)
	[[nodiscard]] constexpr ::std::size_t operator()(
		StringLike const &input) const
		noexcept(noexcept(::std::basic_string_view<Char>{input}))
	{
		auto const view{::std::basic_string_view<Char>{input}};
		::std::uint64_t state{0x9e3779b97f4a7c15ULL ^
			(static_cast<::std::uint64_t>(view.size()) *
			 0xd6e8feb86659fd93ULL)};
		using unsigned_char_type = ::std::make_unsigned_t<Char>;
		for (auto const code_unit : view)
		{
			state ^= static_cast<::std::uint64_t>(
				static_cast<unsigned_char_type>(code_unit)) +
				0xa0761d6478bd642fULL;
			state = ::std::rotl(state, 27) * 0xe7037ed1a0b428dbULL;
		}
		state ^= state >> 32u;
		state *= 0xd6e8feb86659fd93ULL;
		state ^= state >> 32u;
		if constexpr (sizeof(::std::size_t) < sizeof(::std::uint64_t))
		{
			return static_cast<::std::size_t>(state ^ (state >> 32u));
		}
		else
		{
			return static_cast<::std::size_t>(state);
		}
	}
};

template <typename Char>
struct basic_json_string_equal
{
	using is_transparent = void;

	template <typename Left, typename Right>
		requires(::std::constructible_from<::std::basic_string_view<Char>,
			Left const &> &&
			 ::std::constructible_from<::std::basic_string_view<Char>,
				 Right const &>)
	[[nodiscard]] constexpr bool operator()(
		Left const &left, Right const &right) const
		noexcept(noexcept(::std::basic_string_view<Char>{left}) &&
				 noexcept(::std::basic_string_view<Char>{right}))
	{
		return ::std::basic_string_view<Char>{left} ==
			::std::basic_string_view<Char>{right};
	}
};

template <typename T, typename = void>
struct json_pointer_traits
{
	using element_type = T;
	static inline constexpr bool is_pointer = false;
};

template <typename T>
inline constexpr bool json_pointer_like = ::std::is_pointer_v<T> || requires(T const &pointer) {
	pointer.operator->();
};

template <typename T>
struct json_pointer_traits<T, ::std::enable_if_t<json_pointer_like<T>>>
{
	using address_type = decltype(::std::to_address(::std::declval<T const &>()));
	using element_type = ::std::remove_pointer_t<address_type>;
	static inline constexpr bool is_pointer = true;
};

template <typename Variant, typename Allocator, typename Node>
struct json_traits
{
	using variant_type = Variant;
	using allocator_type = Allocator;
	using node_type = Node;

	static_assert(::std::same_as<
					  typename ::std::allocator_traits<allocator_type>::value_type,
					  node_type>,
				  "a fast_io JSON node allocator must allocate that exact node type");

	static_assert(::std::variant_size_v<variant_type> == 9u,
				  "a fast_io JSON node must use the documented nine-alternative variant");
	static_assert(::std::same_as<::std::variant_alternative_t<0u, variant_type>, ::std::monostate>);
	static_assert(::std::same_as<::std::variant_alternative_t<1u, variant_type>, nulljson_t>);
	static_assert(::std::same_as<::std::variant_alternative_t<2u, variant_type>, bool>);

	using number_type = ::std::variant_alternative_t<3u, variant_type>;
	using integer_type = ::std::variant_alternative_t<4u, variant_type>;
	using uinteger_type = ::std::variant_alternative_t<5u, variant_type>;
	using raw_string_type = ::std::variant_alternative_t<6u, variant_type>;
	using raw_array_type = ::std::variant_alternative_t<7u, variant_type>;
	using raw_object_type = ::std::variant_alternative_t<8u, variant_type>;

	static_assert(::fast_io::json_floating_point<number_type>);
	static_assert(::fast_io::json_finite_number<number_type>,
				  "custom JSON floating types must provide json_number_is_finite_define(value) noexcept -> bool");
	static_assert(::fast_io::json_signed_integer<integer_type> ||
				  ::std::same_as<integer_type, disable_integer_t>);
	static_assert(::fast_io::json_unsigned_integer<uinteger_type> ||
				  ::std::same_as<uinteger_type, disable_uinteger_t>);

	using string_pointer_traits = json_pointer_traits<raw_string_type>;
	using array_pointer_traits = json_pointer_traits<raw_array_type>;
	using object_pointer_traits = json_pointer_traits<raw_object_type>;
	using string_type = typename string_pointer_traits::element_type;
	using array_type = typename array_pointer_traits::element_type;
	using object_type = typename object_pointer_traits::element_type;
	using char_type = typename string_type::value_type;
	using key_string_type = typename object_type::key_type;
	using key_char_type = typename key_string_type::value_type;
	using map_node_type = typename object_type::value_type;

	static inline constexpr bool direct_string = !string_pointer_traits::is_pointer;
	static inline constexpr bool has_integer = !::std::same_as<integer_type, disable_integer_t>;
	static inline constexpr bool has_uinteger = !::std::same_as<uinteger_type, disable_uinteger_t>;

	static_assert(array_pointer_traits::is_pointer,
				  "the array alternative must be an allocator pointer");
	static_assert(object_pointer_traits::is_pointer,
				  "the object alternative must be an allocator pointer");
	static_assert(::std::same_as<typename array_type::value_type, node_type>);
	static_assert(::std::same_as<typename object_type::mapped_type, node_type>);
	static_assert(::std::integral<char_type>);
	static_assert(::std::integral<key_char_type>);

	template <typename T>
	using rebind_allocator = typename ::std::allocator_traits<allocator_type>::template rebind_alloc<T>;

	template <typename T>
	using rebind_traits = ::std::allocator_traits<rebind_allocator<T>>;

	static_assert(direct_string ||
				  ::std::same_as<typename rebind_traits<string_type>::pointer, raw_string_type>);
	static_assert(::std::same_as<typename rebind_traits<array_type>::pointer, raw_array_type>);
	static_assert(::std::same_as<typename rebind_traits<object_type>::pointer, raw_object_type>);

	[[nodiscard]] static constexpr json_kind kind(variant_type const &storage) noexcept
	{
		auto const index{storage.index()};
		return index < 9u ? static_cast<json_kind>(index) : json_kind::undefined;
	}

	template <typename T>
	[[nodiscard]] static constexpr T *get_if(variant_type *storage) noexcept
	{
		if constexpr (::std::same_as<T, string_type>)
		{
			if constexpr (direct_string)
			{
				return ::std::get_if<raw_string_type>(storage);
			}
			else if (auto pointer{::std::get_if<raw_string_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			else
			{
				return nullptr;
			}
		}
		else if constexpr (::std::same_as<T, array_type>)
		{
			if (auto pointer{::std::get_if<raw_array_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			return nullptr;
		}
		else if constexpr (::std::same_as<T, object_type>)
		{
			if (auto pointer{::std::get_if<raw_object_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			return nullptr;
		}
		else
		{
			return ::std::get_if<T>(storage);
		}
	}

	template <typename T>
	[[nodiscard]] static constexpr T const *get_if(variant_type const *storage) noexcept
	{
		if constexpr (::std::same_as<T, string_type>)
		{
			if constexpr (direct_string)
			{
				return ::std::get_if<raw_string_type>(storage);
			}
			else if (auto pointer{::std::get_if<raw_string_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			else
			{
				return nullptr;
			}
		}
		else if constexpr (::std::same_as<T, array_type>)
		{
			if (auto pointer{::std::get_if<raw_array_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			return nullptr;
		}
		else if constexpr (::std::same_as<T, object_type>)
		{
			if (auto pointer{::std::get_if<raw_object_type>(storage)})
			{
				return ::std::to_address(*pointer);
			}
			return nullptr;
		}
		else
		{
			return ::std::get_if<T>(storage);
		}
	}

	template <typename T, typename... Args>
	[[nodiscard]] static auto allocate_construct(allocator_type const &allocator, Args &&...args)
	{
		using rebound_allocator = rebind_allocator<T>;
		using rebound_traits = ::std::allocator_traits<rebound_allocator>;
		rebound_allocator rebound{allocator};
		auto pointer{rebound_traits::allocate(rebound, 1u)};
		auto raw_pointer{::std::to_address(pointer)};

		struct allocation_guard
		{
			rebound_allocator *allocator_pointer;
			decltype(pointer) allocated_pointer;
			bool released{};

			~allocation_guard()
			{
				if (!released)
				{
					rebound_traits::deallocate(*allocator_pointer, allocated_pointer, 1u);
				}
			}
		} guard{::std::addressof(rebound), pointer, false};

		if constexpr (requires { typename T::allocator_type; })
		{
			if constexpr (requires { typename T::allocator_type{allocator}; } &&
						  ::std::constructible_from<T, Args...,
													typename T::allocator_type>)
			{
				::std::construct_at(raw_pointer, ::std::forward<Args>(args)...,
									typename T::allocator_type{allocator});
			}
			else
			{
				::std::construct_at(raw_pointer, ::std::forward<Args>(args)...);
			}
		}
		else
		{
			::std::construct_at(raw_pointer, ::std::forward<Args>(args)...);
		}
		guard.released = true;
		return pointer;
	}

	template <typename T, typename Pointer>
	static void destroy_deallocate(allocator_type const &allocator, Pointer pointer) noexcept
	{
		using rebound_allocator = rebind_allocator<T>;
		using rebound_traits = ::std::allocator_traits<rebound_allocator>;
		rebound_allocator rebound{allocator};
		::std::destroy_at(::std::to_address(pointer));
		rebound_traits::deallocate(rebound, pointer, 1u);
	}
};

template <typename Integer, typename UInteger>
struct integer_alias_base
{
	using integer_type = Integer;
	using uinteger_type = UInteger;
};

template <typename Integer>
struct integer_alias_base<Integer, disable_uinteger_t>
{
	using integer_type = Integer;
};

template <typename UInteger>
struct integer_alias_base<disable_integer_t, UInteger>
{
	using uinteger_type = UInteger;
};

template <>
struct integer_alias_base<disable_integer_t, disable_uinteger_t>
{};

/*
`compact_direct` is a proof about the stored, decoded string: its code units
form valid Unicode and minimal JSON output can copy them verbatim between the
two quotation marks.  `validated_escaped` records valid Unicode which still
contains a quotation mark, reverse solidus, or C0 control scalar.  `unknown`
is deliberately sticky after mutable string storage escapes to user code.

This is only a cache of a proved property; unknown always falls back to the
normal validating serializer.  Consequently a stale positive result cannot
be observed after mutation, while failed validation never becomes trusted.
*/
enum class json_string_metadata : unsigned char
{
	unknown,
	validated_escaped,
	compact_direct
};

template <typename string_type>
[[nodiscard]] inline constexpr json_string_metadata
classify_json_string_metadata(string_type const &string) noexcept
{
	auto const *const data{string.data()};
	auto const size{static_cast<::std::size_t>(string.size())};
	::std::size_t position{};
	bool direct{true};
	while (position != size)
	{
		auto const decoded{::fast_io::json::details::decode_json_code_point(
			data, size, position)};
		if (decoded.status != ::fast_io::json::details::unicode_decode_status::ok)
		{
			return json_string_metadata::unknown;
		}
		auto const code_point{decoded.code_point};
		direct = direct && code_point >= 0x20u && code_point != 0x22u &&
				 code_point != 0x5cu;
		position = decoded.next;
	}
	return direct ? json_string_metadata::compact_direct
				  : json_string_metadata::validated_escaped;
}

} // namespace detail

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
class basic_json_node
{
	using self_type = basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>;

public:
	using char_type = Char;
	using number_type = Number;
	using integer_type = Integer;
	using uinteger_type = UInteger;
	using allocator_type = Allocator<self_type>;
	using string_type = ::std::basic_string<Char, ::std::char_traits<Char>, Allocator<Char>>;
	using array_type = Array<self_type, Allocator<self_type>>;
	using object_allocator_type = Allocator<::std::pair<string_type const, self_type>>;
	using object_type = ::fast_io::containers::basic_hash_map<
		string_type, self_type, detail::basic_json_string_hash<Char>,
		detail::basic_json_string_equal<Char>, object_allocator_type>;

private:
	using string_pointer = typename ::std::allocator_traits<allocator_type>::template rebind_traits<string_type>::pointer;
	using array_pointer = typename ::std::allocator_traits<allocator_type>::template rebind_traits<array_type>::pointer;
	using object_pointer = typename ::std::allocator_traits<allocator_type>::template rebind_traits<object_type>::pointer;
	using variant_type = ::std::variant<::std::monostate, nulljson_t, bool, Number, Integer, UInteger,
										string_pointer, array_pointer, object_pointer>;

	variant_type stor;
	allocator_type alloc;
	detail::json_string_metadata string_metadata{};

	template <typename>
	friend class basic_json;
	template <typename>
	friend class basic_json_slice;
	template <typename>
	friend class basic_const_json_slice;
	friend struct detail::basic_json_access;

public:
	constexpr basic_json_node()
		requires ::std::default_initializable<allocator_type>
	= default;

	constexpr explicit basic_json_node(::std::allocator_arg_t,
									   allocator_type const &allocator) noexcept(::std::is_nothrow_copy_constructible_v<allocator_type>)
		: alloc(allocator)
	{}

	basic_json_node(basic_json_node const &other);
	basic_json_node(::std::allocator_arg_t, allocator_type const &allocator,
					basic_json_node const &other);
	basic_json_node(basic_json_node &&other) noexcept(
		::std::is_nothrow_move_constructible_v<allocator_type> &&
		::std::is_nothrow_move_constructible_v<variant_type>)
		: stor(::std::move(other.stor)), alloc(::std::move(other.alloc)),
		  string_metadata(other.string_metadata)
	{
		/* Pointer alternatives are trivially moved by variant and therefore
		   still appear in the source.  Disarm that non-owning duplicate only
		   after every member of this node has been constructed successfully. */
		other.stor.template emplace<::std::monostate>();
		other.string_metadata = detail::json_string_metadata::unknown;
	}
	basic_json_node(::std::allocator_arg_t, allocator_type const &allocator,
					basic_json_node &&other);
	basic_json_node &operator=(basic_json_node const &other);
	basic_json_node &operator=(basic_json_node &&other);
	~basic_json_node() noexcept;
};

namespace detail
{

template <typename Slice, typename Node, typename Variant, typename Allocator>
class basic_json_slice_common_base
	: public integer_alias_base<typename json_traits<Variant, Allocator, Node>::integer_type,
								typename json_traits<Variant, Allocator, Node>::uinteger_type>
{
	using traits_type = json_traits<Variant, Allocator, Node>;
	using integer_internal = typename traits_type::integer_type;
	using uinteger_internal = typename traits_type::uinteger_type;

protected:
	using node_type_internal = typename traits_type::node_type;
	node_type_internal const *node_{};

	constexpr explicit basic_json_slice_common_base(node_type_internal const *node) noexcept : node_(node)
	{}

	[[nodiscard]] constexpr Slice const &derived() const noexcept
	{
		return static_cast<Slice const &>(*this);
	}

public:
	using json_type = basic_json<node_type_internal>;
	using node_type = node_type_internal;
	using value_type = node_type_internal;
	using allocator_type = typename traits_type::allocator_type;
	using number_type = typename traits_type::number_type;
	using string_type = typename traits_type::string_type;
	using array_type = typename traits_type::array_type;
	using object_type = typename traits_type::object_type;
	using char_type = typename traits_type::char_type;
	using key_string_type = typename traits_type::key_string_type;
	using key_char_type = typename traits_type::key_char_type;
	using map_node_type = typename traits_type::map_node_type;

	static inline constexpr bool has_integer = traits_type::has_integer;
	static inline constexpr bool has_uinteger = traits_type::has_uinteger;

	constexpr basic_json_slice_common_base() noexcept = default;

	[[nodiscard]] constexpr bool empty() const noexcept
	{
		return node_ == nullptr;
	}

	[[nodiscard]] constexpr bool bound() const noexcept
	{
		return node_ != nullptr;
	}

	[[nodiscard]] constexpr bool has_reference() const noexcept
	{
		return node_ != nullptr;
	}

	[[nodiscard]] constexpr json_kind kind() const
	{
		if (empty())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::is_empty);
		}
		return derived().kind_impl();
	}

	[[nodiscard]] constexpr bool undefined() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::undefined;
	}
	[[nodiscard]] constexpr bool null() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::null;
	}
	[[nodiscard]] constexpr bool boolean() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::boolean;
	}
	[[nodiscard]] constexpr bool string() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::string;
	}
	[[nodiscard]] constexpr bool array() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::array;
	}
	[[nodiscard]] constexpr bool object() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::object;
	}
	[[nodiscard]] constexpr bool integer() const noexcept
		requires(has_integer)
	{
		return !empty() && derived().kind_impl() == json_kind::integer;
	}
	[[nodiscard]] constexpr bool uinteger() const noexcept
		requires(has_uinteger)
	{
		return !empty() && derived().kind_impl() == json_kind::uinteger;
	}
	[[nodiscard]] constexpr bool number() const noexcept
	{
		if (empty())
		{
			return false;
		}
		auto const value_kind{derived().kind_impl()};
		return value_kind == json_kind::number ||
			   (has_integer && value_kind == json_kind::integer) ||
			   (has_uinteger && value_kind == json_kind::uinteger);
	}

	[[nodiscard]] constexpr bool is_undefined() const noexcept
	{
		return undefined();
	}
	[[nodiscard]] constexpr bool is_null() const noexcept
	{
		return null();
	}
	[[nodiscard]] constexpr bool is_boolean() const noexcept
	{
		return boolean();
	}
	[[nodiscard]] constexpr bool is_number() const noexcept
	{
		return number();
	}
	[[nodiscard]] constexpr bool is_integer() const noexcept
		requires(has_integer)
	{
		return integer();
	}
	[[nodiscard]] constexpr bool is_uinteger() const noexcept
		requires(has_uinteger)
	{
		return uinteger();
	}
	[[nodiscard]] constexpr bool is_string() const noexcept
	{
		return string();
	}
	[[nodiscard]] constexpr bool is_array() const noexcept
	{
		return array();
	}
	[[nodiscard]] constexpr bool is_object() const noexcept
	{
		return object();
	}

	/* JSON value cardinality used by the ergonomic DOM layer.  Strings report
	   code units, arrays report elements, objects report members, null/undefined
	   report zero, and scalar values report one.  `empty()` remains the slice
	   binding query for compatibility; use `value_empty()` for the value. */
	[[nodiscard]] constexpr ::std::size_t size() const noexcept
	{
		if (empty())
		{
			return 0u;
		}
		switch (derived().kind_impl())
		{
		case json_kind::undefined:
		case json_kind::null:
			return 0u;
		case json_kind::string:
			return static_cast<::std::size_t>(
				derived().template get_if_impl<string_type>()->size());
		case json_kind::array:
			return static_cast<::std::size_t>(
				derived().template get_if_impl<array_type>()->size());
		case json_kind::object:
			return static_cast<::std::size_t>(
				derived().template get_if_impl<object_type>()->size());
		default:
			return 1u;
		}
	}

	[[nodiscard]] constexpr bool value_empty() const noexcept
	{
		return size() == 0u;
	}

	template <typename T>
	[[nodiscard]] constexpr T const *get_if() const noexcept
	{
		return empty() ? nullptr : derived().template get_if_impl<T>();
	}

	[[nodiscard]] constexpr bool get_boolean() const
	{
		auto pointer{get_if<bool>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_boolean);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr number_type const &get_number() const
	{
		auto pointer{get_if<number_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_number);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr number_type as_number() const
	{
		if (auto pointer{get_if<number_type>()})
		{
			return *pointer;
		}
		if constexpr (has_integer)
		{
			if (auto pointer{get_if<integer_internal>()})
			{
				return number_type(*pointer);
			}
		}
		if constexpr (has_uinteger)
		{
			if (auto pointer{get_if<uinteger_internal>()})
			{
				return number_type(*pointer);
			}
		}
		::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_number);
	}

	[[nodiscard]] constexpr integer_internal const &get_integer() const
		requires(has_integer)
	{
		auto pointer{get_if<integer_internal>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_integer);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr uinteger_internal const &get_uinteger() const
		requires(has_uinteger)
	{
		auto pointer{get_if<uinteger_internal>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_uinteger);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr string_type const &get_string() const
	{
		auto pointer{get_if<string_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_string);
		}
		return *pointer;
	}

	/* True only while the DOM still owns a proof that compact minimal output
	   may copy this decoded string without an escape/Unicode scan. */
	[[nodiscard]] constexpr bool string_is_compact_direct() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::string &&
			   derived().string_is_compact_direct_impl();
	}

	/* A parsed or internally classified string keeps a proof of Unicode
	   validity until mutable storage is exposed.  Serializers may use this
	   weaker proof even when quote/control escaping is still required. */
	[[nodiscard]] constexpr bool string_is_unicode_validated() const noexcept
	{
		return !empty() && derived().kind_impl() == json_kind::string &&
			   derived().string_is_unicode_validated_impl();
	}

	[[nodiscard]] constexpr array_type const &get_array() const
	{
		auto pointer{get_if<array_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_array);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr object_type const &get_object() const
	{
		auto pointer{get_if<object_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_object);
		}
		return *pointer;
	}

	constexpr explicit operator bool() const
	{
		return get_boolean();
	}
	constexpr explicit operator number_type() const
	{
		return as_number();
	}
	constexpr explicit operator nulljson_t() const
	{
		if (!null())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_null);
		}
		return nulljson;
	}
	constexpr explicit operator string_type const &() const &
	{
		return get_string();
	}
	constexpr explicit operator array_type const &() const &
	{
		return get_array();
	}
	constexpr explicit operator object_type const &() const &
	{
		return get_object();
	}
	constexpr explicit operator integer_internal() const
		requires(has_integer)
	{
		return get_integer();
	}
	constexpr explicit operator uinteger_internal() const
		requires(has_uinteger)
	{
		return get_uinteger();
	}
};

} // namespace detail

template <typename Node>
class basic_const_json_slice
	: public detail::basic_json_slice_common_base<basic_const_json_slice<Node>, Node,
												  decltype(Node::stor), decltype(Node::alloc)>
{
	using traits_type = detail::json_traits<decltype(Node::stor), decltype(Node::alloc), Node>;
	using base_type = detail::basic_json_slice_common_base<basic_const_json_slice<Node>, Node,
														   decltype(Node::stor), decltype(Node::alloc)>;

	friend base_type;
	friend class basic_json_slice<Node>;
	using base_type::node_;

	[[nodiscard]] constexpr json_kind kind_impl() const noexcept
	{
		return traits_type::kind(node_->stor);
	}

	template <typename T>
	[[nodiscard]] constexpr T const *get_if_impl() const noexcept
	{
		return traits_type::template get_if<T>(::std::addressof(node_->stor));
	}

	[[nodiscard]] constexpr bool string_is_compact_direct_impl() const noexcept
	{
		if constexpr (requires { node_->string_metadata; })
		{
			return node_->string_metadata == detail::json_string_metadata::compact_direct;
		}
		else
		{
			return false;
		}
	}

	[[nodiscard]] constexpr bool string_is_unicode_validated_impl() const noexcept
	{
		if constexpr (requires { node_->string_metadata; })
		{
			return node_->string_metadata != detail::json_string_metadata::unknown;
		}
		else
		{
			return false;
		}
	}

public:
	using base_type::as_number;
	using base_type::get_array;
	using base_type::get_boolean;
	using base_type::get_if;
	using base_type::get_integer;
	using base_type::get_number;
	using base_type::get_object;
	using base_type::get_string;
	using base_type::get_uinteger;
	using typename base_type::array_type;
	using typename base_type::key_char_type;
	using typename base_type::key_string_type;
	using typename base_type::node_type;
	using typename base_type::object_type;

	constexpr basic_const_json_slice() noexcept = default;
	constexpr basic_const_json_slice(basic_const_json_slice const &) noexcept = default;
	constexpr basic_const_json_slice(basic_const_json_slice &&) noexcept = default;
	constexpr basic_const_json_slice &operator=(basic_const_json_slice const &) noexcept = default;
	constexpr basic_const_json_slice &operator=(basic_const_json_slice &&) noexcept = default;

	constexpr basic_const_json_slice(typename base_type::json_type const &value) noexcept
	{
		node_ = ::std::addressof(value.node_);
	}
	basic_const_json_slice(typename base_type::json_type &&) = delete;
	basic_const_json_slice(typename base_type::json_type const &&) = delete;

	constexpr basic_const_json_slice(node_type const &node) noexcept
	{
		node_ = ::std::addressof(node);
	}
	basic_const_json_slice(node_type &&) = delete;
	basic_const_json_slice(node_type const &&) = delete;
	constexpr basic_const_json_slice(detail::json_slice_pointer_arg_t,
									 void const *node) noexcept
	{
		node_ = static_cast<node_type const *>(node);
	}

	constexpr basic_const_json_slice(basic_json_slice<Node> const &slice) noexcept
	{
		node_ = slice.node_;
	}

	constexpr void swap(basic_const_json_slice &other) noexcept
	{
		::std::swap(node_, other.node_);
	}
	friend constexpr void swap(basic_const_json_slice &left, basic_const_json_slice &right) noexcept
	{
		left.swap(right);
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_const_json_slice operator[](Position position) const
	{
		if (!this->array())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::nonarray_indexing);
		}
		auto const &array{this->get_array()};
		if (!detail::json_index_in_range(position, array.size()))
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::index_out_of_range);
		}
		return array[static_cast<typename array_type::size_type>(position)];
	}

	[[nodiscard]] constexpr basic_const_json_slice operator[](key_string_type const &key) const
	{
		if (!this->object())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::nonobject_indexing);
		}
		auto const &object{this->get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::key_not_found);
		}
		return iterator->second;
	}

	[[nodiscard]] constexpr basic_const_json_slice operator[](key_char_type const *key) const
	{
		if (!this->object())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::nonobject_indexing);
		}
		auto const &object{this->get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::key_not_found);
		}
		return iterator->second;
	}
	basic_const_json_slice operator[](::std::nullptr_t) const = delete;

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice operator[](Key const &key) const
	{
		if (!this->object())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::nonobject_indexing);
		}
		auto const &object{this->get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::key_not_found);
		}
		return iterator->second;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice find(Key const &key) const
	{
		if (!this->object())
		{
			return {};
		}
		auto const &object{this->get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			return {};
		}
		return iterator->second;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr bool contains(Key const &key) const
	{
		return !find(key).empty();
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice at(Key const &key) const
	{
		if (this->empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
		if (!this->object())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		auto result{find(key)};
		if (result.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::key_not_found);
		}
		return result;
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_const_json_slice at(Position position) const
	{
		return (*this)[position];
	}

	[[nodiscard]] constexpr basic_const_json_slice front() const
	{
		return at(0u);
	}

	[[nodiscard]] constexpr basic_const_json_slice back() const
	{
		if (!this->array())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto const &array{this->get_array()};
		if (array.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::index_out_of_range);
		}
		return array.back();
	}

	[[nodiscard]] constexpr auto as_array() const
	{
		return this->get_array() | ::std::views::transform([](node_type const &node) noexcept {
				   return basic_const_json_slice{node};
			   });
	}

	[[nodiscard]] constexpr auto as_object() const
	{
		return this->get_object() | ::std::views::transform([](typename object_type::value_type const &entry) noexcept {
				   return ::std::pair<key_string_type const &, basic_const_json_slice>{entry.first, entry.second};
			   });
	}
};

template <typename Node>
class basic_json_slice
	: public detail::basic_json_slice_common_base<basic_json_slice<Node>, Node,
												  decltype(Node::stor), decltype(Node::alloc)>
{
	using traits_type = detail::json_traits<decltype(Node::stor), decltype(Node::alloc), Node>;
	using base_type = detail::basic_json_slice_common_base<basic_json_slice<Node>, Node,
														   decltype(Node::stor), decltype(Node::alloc)>;

	friend base_type;
	friend class basic_const_json_slice<Node>;
	using base_type::node_;

	[[nodiscard]] constexpr Node *mutable_node() const noexcept
	{
		return const_cast<Node *>(node_);
	}

	[[nodiscard]] constexpr json_kind kind_impl() const noexcept
	{
		return traits_type::kind(node_->stor);
	}

	template <typename T>
	[[nodiscard]] constexpr T const *get_if_impl() const noexcept
	{
		return traits_type::template get_if<T>(::std::addressof(node_->stor));
	}

	[[nodiscard]] constexpr bool string_is_compact_direct_impl() const noexcept
	{
		if constexpr (requires { node_->string_metadata; })
		{
			return node_->string_metadata == detail::json_string_metadata::compact_direct;
		}
		else
		{
			return false;
		}
	}

	[[nodiscard]] constexpr bool string_is_unicode_validated_impl() const noexcept
	{
		if constexpr (requires { node_->string_metadata; })
		{
			return node_->string_metadata != detail::json_string_metadata::unknown;
		}
		else
		{
			return false;
		}
	}

	constexpr void require_bound() const
	{
		if (this->empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
	}

	void assign_node(Node const &source)
	{
		require_bound();
		auto &destination{*mutable_node()};
		if (::std::addressof(destination) == ::std::addressof(source))
		{
			return;
		}

		/* Build the complete replacement with the destination allocator before
		   publishing it.  Besides giving assignment the strong guarantee, this
		   makes `document["a"] = document["b"]` safe even when both nodes are in
		   the same segmented container. */
		auto replacement{
			basic_json<Node>::make_empty_node(destination.alloc)};
		basic_json<Node>::clone_node_into(
			replacement, source, destination.alloc);
		basic_json<Node>::reset_node(destination);
		basic_json<Node>::swap_node_storage(destination, replacement);
	}

	[[nodiscard]] auto &ensure_array()
	{
		require_bound();
		if (this->undefined() || this->null())
		{
			return basic_json<Node>::emplace_array_node(*mutable_node());
		}
		if (!this->array()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		return *traits_type::template get_if<typename traits_type::array_type>(
			::std::addressof(mutable_node()->stor));
	}

	[[nodiscard]] auto &ensure_object()
	{
		require_bound();
		if (this->undefined() || this->null())
		{
			return basic_json<Node>::emplace_object_node(*mutable_node());
		}
		if (!this->object()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		return *traits_type::template get_if<typename traits_type::object_type>(
			::std::addressof(mutable_node()->stor));
	}

	template <typename Key>
	[[nodiscard]] auto make_object_key(Key const &key)
	{
		using key_type = typename traits_type::key_string_type;
		using key_allocator_type = typename key_type::allocator_type;
		if constexpr (::std::constructible_from<
					  key_allocator_type, typename traits_type::allocator_type const &> &&
				  ::std::constructible_from<key_type, Key const &,
									key_allocator_type>)
		{
			return key_type(key, key_allocator_type{mutable_node()->alloc});
		}
		else
		{
			/* A custom node may intentionally put keys in an unrelated allocator
			   domain.  Such a key cannot be rebound from the node allocator, so
			   retain the key type's documented construction semantics. */
			return key_type(key);
		}
	}

	template <typename Value>
	constexpr void require_acyclic_owning_value(Value &&value) const
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<Value>, json_type> &&
					  ::std::is_rvalue_reference_v<Value &&> &&
					  !::std::is_const_v<::std::remove_reference_t<Value>>)
		{
			/* Moving an owning root into one of its own descendants would make the
			   new child own an ancestor container which in turn owns that child. */
			if (json_type::node_reaches(value.node_, *mutable_node())) [[unlikely]]
			{
				::fast_io::json::throw_json_error(
					::fast_io::json::json_errc::cyclic_reference);
			}
		}
	}

	template <typename Value>
	[[nodiscard]] Node make_child_with_value(Value &&value)
	{
		require_acyclic_owning_value(::std::forward<Value>(value));
		auto child{basic_json<Node>::make_child_node(*mutable_node())};
		basic_json_slice child_slice{child};
		child_slice = ::std::forward<Value>(value);
		return child;
	}

	[[nodiscard]] Node make_null_child()
	{
		auto child{basic_json<Node>::make_child_node(*mutable_node())};
		basic_json<Node>::set_null_node(child);
		return child;
	}

	template <typename Key>
	static constexpr void require_nonnull_key(Key const &key)
	{
		using key_type = ::std::remove_cvref_t<Key>;
		if constexpr (::std::is_pointer_v<key_type> &&
					  ::std::same_as<::std::remove_cv_t<
						  ::std::remove_pointer_t<key_type>>, key_char_type>)
		{
			if (key == nullptr) [[unlikely]]
			{
				::fast_io::json::throw_json_error(
					::fast_io::json::json_errc::null_pointer);
			}
		}
	}

public:
	using base_type::as_number;
	using base_type::get_array;
	using base_type::get_boolean;
	using base_type::get_if;
	using base_type::get_number;
	using base_type::get_object;
	using base_type::get_string;
	using base_type::get_uinteger;
	using typename base_type::array_type;
	using typename base_type::char_type;
	using typename base_type::json_type;
	using typename base_type::key_char_type;
	using typename base_type::key_string_type;
	using typename base_type::node_type;
	using typename base_type::number_type;
	using typename base_type::object_type;
	using typename base_type::string_type;
	using integer_type = typename traits_type::integer_type;
	using uinteger_type = typename traits_type::uinteger_type;

	constexpr basic_json_slice() noexcept = default;
	constexpr basic_json_slice(basic_json_slice const &) noexcept = default;
	constexpr basic_json_slice(basic_json_slice &&) noexcept = default;

	constexpr basic_json_slice(json_type &value) noexcept
	{
		node_ = ::std::addressof(value.node_);
	}
	constexpr basic_json_slice(node_type &node) noexcept
	{
		node_ = ::std::addressof(node);
	}
	constexpr basic_json_slice(detail::json_slice_pointer_arg_t, void *node) noexcept
	{
		node_ = static_cast<node_type *>(node);
	}

	/* A mutable slice is a reference proxy: copying it constructs another cheap
	   reference, while assigning through it replaces the referenced JSON value.
	   This matches ordinary `document[key] = document[other_key]` expectations.
	   `rebind` is the explicit operation for changing which node a named slice
	   refers to. */
	basic_json_slice &operator=(basic_json_slice const &other)
	{
		if (other.empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
		assign_node(*other.node_);
		return *this;
	}

	basic_json_slice &operator=(basic_const_json_slice<Node> other)
	{
		if (other.empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
		assign_node(*other.node_);
		return *this;
	}

	basic_json_slice &operator=(json_type const &value)
	{
		assign_node(value.node_);
		return *this;
	}

	basic_json_slice &operator=(json_type &&value)
	{
		require_bound();
		auto &destination{*mutable_node()};
		if (::std::addressof(destination) == ::std::addressof(value.node_))
		{
			return *this;
		}
		if (json_type::node_reaches(value.node_, destination)) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::cyclic_reference);
		}
		json_type replacement{::std::move(value), destination.alloc};
		json_type::reset_node(destination);
		json_type::swap_node_storage(destination, replacement.node_);
		return *this;
	}

	constexpr void rebind(basic_json_slice other) noexcept
	{
		node_ = other.node_;
	}

	constexpr void reset_reference() noexcept
	{
		node_ = nullptr;
	}

	constexpr void swap(basic_json_slice &other) noexcept
	{
		::std::swap(node_, other.node_);
	}
	/* Slice swap is a handle operation, like swapping two iterators.  Use
	   unqualified `swap(left, right)`/ranges::swap so ADL reaches this overload;
	   use swap_values below when the two referred JSON subtrees must exchange. */
	friend constexpr void swap(basic_json_slice &left, basic_json_slice &right) noexcept
	{
		left.swap(right);
	}

	void swap_values(basic_json_slice other)
	{
		require_bound();
		other.require_bound();
		auto &left{*mutable_node()};
		auto &right{*other.mutable_node()};
		if (::std::addressof(left) == ::std::addressof(right))
		{
			return;
		}
		if (json_type::node_reaches(left, right) ||
			json_type::node_reaches(right, left)) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::cyclic_reference);
		}
		if constexpr (::std::allocator_traits<typename base_type::allocator_type>::is_always_equal::value)
		{
			json_type::swap_node_storage(left, right);
		}
		else if (left.alloc == right.alloc)
		{
			json_type::swap_node_storage(left, right);
		}
		else
		{
			auto replacement_for_left{json_type::make_empty_node(left.alloc)};
			json_type::clone_node_into(
				replacement_for_left, right, left.alloc);
			auto replacement_for_right{json_type::make_empty_node(right.alloc)};
			json_type::clone_node_into(
				replacement_for_right, left, right.alloc);
			json_type::reset_node(left);
			json_type::reset_node(right);
			json_type::swap_node_storage(left, replacement_for_left);
			json_type::swap_node_storage(right, replacement_for_right);
		}
	}

	template <typename T>
	[[nodiscard]] constexpr T *get_if() noexcept
	{
		if (this->empty())
		{
			return nullptr;
		}
		if constexpr (::std::same_as<::std::remove_cv_t<T>, string_type>)
		{
			/* Returning a mutable std::basic_string pointer lets mutation happen
			   after this call, so no later operation may reinstate this proof. */
			if constexpr (requires { mutable_node()->string_metadata; })
			{
				mutable_node()->string_metadata = detail::json_string_metadata::unknown;
			}
		}
		return traits_type::template get_if<T>(::std::addressof(mutable_node()->stor));
	}

	[[nodiscard]] constexpr integer_type &get_integer()
		requires(traits_type::has_integer)
	{
		auto pointer{get_if<integer_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_integer);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr bool &get_boolean()
	{
		auto pointer{get_if<bool>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::not_boolean);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr number_type &get_number()
	{
		auto pointer{get_if<number_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::not_number);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr uinteger_type &get_uinteger()
		requires(traits_type::has_uinteger)
	{
		auto pointer{get_if<uinteger_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::not_uinteger);
		}
		return *pointer;
	}

	[[nodiscard]] constexpr string_type &get_string()
	{
		auto pointer{get_if<string_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_string);
		}
		return *pointer;
	}
	[[nodiscard]] constexpr array_type &get_array()
	{
		auto pointer{get_if<array_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_array);
		}
		return *pointer;
	}
	[[nodiscard]] constexpr object_type &get_object()
	{
		auto pointer{get_if<object_type>()};
		if (pointer == nullptr)
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::not_object);
		}
		return *pointer;
	}

	constexpr explicit operator string_type &() &
	{
		return get_string();
	}
	constexpr explicit operator array_type &() &
	{
		return get_array();
	}
	constexpr explicit operator object_type &() &
	{
		return get_object();
	}

	constexpr void reset() noexcept
	{
		if (!this->empty())
		{
			json_type::reset_node(*mutable_node());
		}
	}

	constexpr basic_json_slice &operator=(nulljson_t)
	{
		require_bound();
		json_type::set_null_node(*mutable_node());
		return *this;
	}

	template <typename T>
		requires(::std::same_as<::std::remove_cvref_t<T>, bool> ||
				 ::fast_io::json_signed_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_unsigned_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_floating_point<::std::remove_cvref_t<T>>)
	constexpr basic_json_slice &operator=(T &&value)
	{
		using value_type = ::std::remove_cvref_t<T>;
		require_bound();
		if constexpr (::std::same_as<value_type, bool>)
		{
			json_type::set_boolean_node(*mutable_node(), value);
		}
		else if constexpr (::fast_io::json_signed_integer<value_type> && traits_type::has_integer)
		{
			json_type::set_integer_node(*mutable_node(), integer_type(::std::forward<T>(value)));
		}
		else if constexpr (::fast_io::json_unsigned_integer<value_type> && traits_type::has_uinteger)
		{
			json_type::set_uinteger_node(*mutable_node(), uinteger_type(::std::forward<T>(value)));
		}
		else
		{
			json_type::set_number_node(*mutable_node(), number_type(::std::forward<T>(value)));
		}
		return *this;
	}

	constexpr basic_json_slice &operator=(string_type const &value)
	{
		require_bound();
		auto const current{this->kind()};
		if (current == json_kind::string)
		{
			json_type::assign_string_node(*mutable_node(), value);
		}
		else
		{
			json_type::emplace_string_node(*mutable_node(), value);
		}
		return *this;
	}

	constexpr basic_json_slice &operator=(string_type &&value)
	{
		require_bound();
		auto const current{this->kind()};
		if (current == json_kind::string)
		{
			json_type::assign_string_node(*mutable_node(), ::std::move(value));
		}
		else
		{
			json_type::emplace_string_node(*mutable_node(), ::std::move(value));
		}
		return *this;
	}

	constexpr basic_json_slice &operator=(char_type const *value)
	{
		require_bound();
		if (value == nullptr) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::null_pointer);
		}
		auto const current{this->kind()};
		if (current == json_kind::string)
		{
			json_type::assign_string_node(*mutable_node(), value);
		}
		else
		{
			json_type::emplace_string_node(*mutable_node(), value);
		}
		return *this;
	}
	basic_json_slice &operator=(::std::nullptr_t) = delete;

	template <typename StringLike>
		requires(!::std::same_as<::std::remove_cvref_t<StringLike>, string_type> &&
				 !::std::is_convertible_v<StringLike const &, char_type const *> &&
				 ::std::constructible_from<string_type, StringLike const &>)
	constexpr basic_json_slice &operator=(StringLike const &value)
	{
		return *this = string_type(value);
	}

	[[nodiscard]] constexpr basic_json_slice operator[](key_string_type const &key)
	{
		auto &object{ensure_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator != object.end())
		{
			return iterator->second;
		}
		auto result{detail::json_object_emplace_known_absent(object,
			make_object_key(key), make_null_child())};
		return result->second;
	}

	[[nodiscard]] constexpr basic_json_slice operator[](key_char_type const *key)
	{
		require_nonnull_key(key);
		auto &object{ensure_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator != object.end())
		{
			return iterator->second;
		}
		auto result{detail::json_object_emplace_known_absent(object,
			make_object_key(key), make_null_child())};
		return result->second;
	}
	basic_json_slice operator[](::std::nullptr_t) = delete;

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_json_slice operator[](Key const &key)
	{
		require_nonnull_key(key);
		auto &object{ensure_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator != object.end())
		{
			return iterator->second;
		}
		auto result{detail::json_object_emplace_known_absent(object,
			make_object_key(key), make_null_child())};
		return result->second;
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_json_slice operator[](Position position)
	{
		if (!this->array())
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::nonarray_indexing);
		}
		auto &array{get_array()};
		if (!detail::json_index_in_range(position, array.size()))
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::index_out_of_range);
		}
		return array[static_cast<typename array_type::size_type>(position)];
	}

	[[nodiscard]] constexpr basic_const_json_slice<Node> operator[](
		key_string_type const &key) const
	{
		return basic_const_json_slice<Node>{*this}[key];
	}

	[[nodiscard]] constexpr basic_const_json_slice<Node> operator[](
		key_char_type const *key) const
	{
		return basic_const_json_slice<Node>{*this}[key];
	}
	basic_const_json_slice<Node> operator[](::std::nullptr_t) const = delete;

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice<Node> operator[](
		Key const &key) const
	{
		return basic_const_json_slice<Node>{*this}[key];
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_const_json_slice<Node> operator[](
		Position position) const
	{
		return basic_const_json_slice<Node>{*this}[position];
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_json_slice find(Key const &key)
	{
		if (!this->object())
		{
			return {};
		}
		auto &object{get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			return {};
		}
		return iterator->second;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice<Node> find(
		Key const &key) const
	{
		if (!this->object())
		{
			return {};
		}
		auto const &object{this->get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			return {};
		}
		return iterator->second;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr bool contains(Key const &key) const
	{
		return !find(key).empty();
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_json_slice at(Key const &key)
	{
		if (this->empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
		if (!this->object())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		auto result{find(key)};
		if (result.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::key_not_found);
		}
		return result;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	[[nodiscard]] constexpr basic_const_json_slice<Node> at(
		Key const &key) const
	{
		if (this->empty()) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_empty);
		}
		if (!this->object())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		auto result{find(key)};
		if (result.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::key_not_found);
		}
		return result;
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_json_slice at(Position position)
	{
		return (*this)[position];
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	[[nodiscard]] constexpr basic_const_json_slice<Node> at(
		Position position) const
	{
		return basic_const_json_slice<Node>{*this}[position];
	}

	[[nodiscard]] constexpr basic_json_slice front()
	{
		return at(0u);
	}

	[[nodiscard]] constexpr basic_const_json_slice<Node> front() const
	{
		return at(0u);
	}

	[[nodiscard]] constexpr basic_json_slice back()
	{
		if (!this->array())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto &array{get_array()};
		if (array.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::index_out_of_range);
		}
		return array.back();
	}

	[[nodiscard]] constexpr basic_const_json_slice<Node> back() const
	{
		return basic_const_json_slice<Node>{*this}.back();
	}

	template <typename Value>
	basic_json_slice emplace_back(Value &&value)
	{
		require_bound();
		if (!this->array() && !this->null() && !this->undefined())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto child{make_child_with_value(::std::forward<Value>(value))};
		auto &array{ensure_array()};
		array.emplace_back(::std::move(child));
		return array.back();
	}

	template <typename Value>
	void push_back(Value &&value)
	{
		static_cast<void>(emplace_back(::std::forward<Value>(value)));
	}

	void pop_back()
	{
		require_bound();
		if (!this->array())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto &array{get_array()};
		if (array.empty())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::index_out_of_range);
		}
		array.pop_back();
	}

	template <typename Position, typename Value>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	basic_json_slice insert(Position position, Value &&value)
	{
		require_bound();
		if (!this->array() && !this->null() && !this->undefined())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto const current_size{this->array()
			? get_array().size()
			: typename array_type::size_type{}};
		if (!detail::json_index_at_most_size(position, current_size))
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::index_out_of_range);
		}
		auto child{make_child_with_value(::std::forward<Value>(value))};
		auto &array{ensure_array()};
		auto const index{
			static_cast<typename array_type::size_type>(position)};
		auto const original_size{array.size()};
		if (index == original_size)
		{
			array.emplace_back(::std::move(child));
			return array.back();
		}

		/* Construct the extra tail first, so allocation failure leaves the array
		   unchanged.  The following moves operate only between nodes carrying the
		   same parent allocator.  As with vector insertion, an exception from a
		   user-defined numeric move gives the basic guarantee. */
		array.emplace_back(::std::move(array.back()));
		for (auto current{original_size - 1u}; current != index; --current)
		{
			array[current] = ::std::move(array[current - 1u]);
		}
		array[index] = ::std::move(child);
		return array[index];
	}

	template <typename Position>
		requires(::fast_io::details::my_integral<
				 ::std::remove_cvref_t<Position>> &&
				 !::std::same_as<::std::remove_cvref_t<Position>, bool>)
	basic_json_slice erase(Position position)
	{
		if (!this->array())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonarray_indexing);
		}
		auto &array{get_array()};
		if (!detail::json_index_in_range(position, array.size()))
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::index_out_of_range);
		}
		auto const index{
			static_cast<typename array_type::size_type>(position)};
		for (auto current{index}; current + 1u != array.size(); ++current)
		{
			array[current] = ::std::move(array[current + 1u]);
		}
		array.pop_back();
		if (index == array.size())
		{
			return {};
		}
		return array[index];
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	::std::size_t erase(Key const &key)
	{
		if (!this->object())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		auto &object{get_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator == object.end())
		{
			return 0u;
		}
		object.erase(iterator);
		return 1u;
	}

	template <typename Key, typename Value>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	basic_json_slice insert_or_assign(
		Key const &key, Value &&value)
	{
		require_bound();
		require_nonnull_key(key);
		if (!this->object() && !this->null() && !this->undefined())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		require_acyclic_owning_value(::std::forward<Value>(value));
		auto result{(*this)[key]};
		result = ::std::forward<Value>(value);
		return result;
	}

	template <typename Key>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	::std::pair<basic_json_slice, bool> try_emplace(Key const &key)
	{
		require_nonnull_key(key);
		auto &object{ensure_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator != object.end())
		{
			return {basic_json_slice{iterator->second}, false};
		}
		auto insertion{detail::json_object_emplace_known_absent(object,
			make_object_key(key), make_null_child())};
		return {basic_json_slice{insertion->second}, true};
	}

	template <typename Key, typename Value>
		requires(!::fast_io::details::my_integral<::std::remove_cvref_t<Key>> &&
				 ::std::constructible_from<key_string_type, Key const &>)
	::std::pair<basic_json_slice, bool> try_emplace(
		Key const &key, Value &&value)
	{
		require_bound();
		require_nonnull_key(key);
		if (!this->object() && !this->null() && !this->undefined())
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::nonobject_indexing);
		}
		require_acyclic_owning_value(::std::forward<Value>(value));
		auto &object{ensure_object()};
		auto iterator{detail::json_object_find(object, key)};
		if (iterator != object.end())
		{
			return {basic_json_slice{iterator->second}, false};
		}
		auto child{make_child_with_value(::std::forward<Value>(value))};
		auto insertion{detail::json_object_emplace_known_absent(object,
			make_object_key(key), ::std::move(child))};
		return {basic_json_slice{insertion->second}, true};
	}

	void resize(typename array_type::size_type requested)
	{
		auto &array{ensure_array()};
		while (requested < array.size())
		{
			array.pop_back();
		}
		if constexpr (requires { array.reserve(requested); })
		{
			array.reserve(requested);
		}
		while (array.size() < requested)
		{
			auto child{basic_json<Node>::make_child_node(*mutable_node())};
			basic_json<Node>::set_null_node(child);
			array.emplace_back(::std::move(child));
		}
	}

	void reserve(typename array_type::size_type requested)
	{
		auto &array{ensure_array()};
		if constexpr (requires { array.reserve(requested); })
		{
			array.reserve(requested);
		}
	}

	void clear()
	{
		require_bound();
		switch (this->kind())
		{
		case json_kind::undefined:
		case json_kind::null:
			return;
		case json_kind::boolean:
			basic_json<Node>::set_boolean_node(*mutable_node(), false);
			return;
		case json_kind::number:
			if constexpr (::fast_io::details::my_floating_point<number_type>)
			{
				basic_json<Node>::set_number_node(
					*mutable_node(), number_type{});
			}
			else
			{
				/* A custom number's default constructor need not denote JSON zero
				   (a token-backed decimal commonly defaults to an empty token).  The
				   numeric concept deliberately does not invent that algebraic promise,
				   so use the universally valid empty scalar instead. */
				basic_json<Node>::set_null_node(*mutable_node());
			}
			return;
		case json_kind::integer:
			if constexpr (traits_type::has_integer &&
						  ::fast_io::details::my_integral<integer_type>)
			{
				basic_json<Node>::set_integer_node(
					*mutable_node(), integer_type{});
			}
			else
			{
				basic_json<Node>::set_null_node(*mutable_node());
			}
			return;
		case json_kind::uinteger:
			if constexpr (traits_type::has_uinteger &&
						  ::fast_io::details::my_integral<uinteger_type>)
			{
				basic_json<Node>::set_uinteger_node(
					*mutable_node(), uinteger_type{});
			}
			else
			{
				basic_json<Node>::set_null_node(*mutable_node());
			}
			return;
		case json_kind::string:
			get_string().clear();
			return;
		case json_kind::array:
			get_array().clear();
			return;
		case json_kind::object:
			get_object().clear();
			return;
		}
	}

	[[nodiscard]] constexpr auto as_array()
	{
		return get_array() | ::std::views::transform([](node_type &node) noexcept {
				   return basic_json_slice{node};
			   });
	}

	[[nodiscard]] constexpr auto as_object()
	{
		return get_object() | ::std::views::transform([](typename object_type::value_type &entry) noexcept {
				   return ::std::pair<key_string_type const &, basic_json_slice>{entry.first, entry.second};
			   });
	}
};

template <typename Node>
class basic_json
	: public detail::integer_alias_base<typename detail::json_traits<decltype(Node::stor), decltype(Node::alloc), Node>::integer_type,
										typename detail::json_traits<decltype(Node::stor), decltype(Node::alloc), Node>::uinteger_type>
{
	using traits_type = detail::json_traits<decltype(Node::stor), decltype(Node::alloc), Node>;
	using variant_type = typename traits_type::variant_type;

	friend class basic_json_slice<Node>;
	friend class basic_const_json_slice<Node>;
	friend struct detail::basic_json_access;

public:
	using allocator_type = typename traits_type::allocator_type;
	using node_type = Node;
	using value_type = Node;
	using number_type = typename traits_type::number_type;
	using integer_type = typename traits_type::integer_type;
	using uinteger_type = typename traits_type::uinteger_type;
	using string_type = typename traits_type::string_type;
	using array_type = typename traits_type::array_type;
	using object_type = typename traits_type::object_type;
	using char_type = typename traits_type::char_type;
	using key_string_type = typename traits_type::key_string_type;
	using key_char_type = typename traits_type::key_char_type;
	using map_node_type = typename traits_type::map_node_type;
	using slice_type = basic_json_slice<Node>;
	using const_slice_type = basic_const_json_slice<Node>;

	static inline constexpr bool has_integer = traits_type::has_integer;
	static inline constexpr bool has_uinteger = traits_type::has_uinteger;

private:
	static_assert(::std::same_as<Node, typename traits_type::node_type>);
	node_type node_;

	[[nodiscard]] static constexpr allocator_type const &allocator_of(node_type const &node) noexcept
	{
		return node.alloc;
	}

	[[nodiscard]] static constexpr allocator_type &allocator_of(node_type &node) noexcept
	{
		return node.alloc;
	}

	[[nodiscard]] static constexpr json_kind kind_of_node(node_type const &node) noexcept
	{
		return traits_type::kind(node.stor);
	}

	static constexpr void set_string_metadata(
		node_type &node, detail::json_string_metadata metadata) noexcept
	{
		if constexpr (requires { node.string_metadata; })
		{
			node.string_metadata = metadata;
		}
	}

	static constexpr void invalidate_string_metadata(node_type &node) noexcept
	{
		set_string_metadata(node, detail::json_string_metadata::unknown);
	}

	static constexpr void set_validated_string_metadata(node_type &node,
														bool compact_direct) noexcept
	{
		set_string_metadata(node,
							compact_direct
								? detail::json_string_metadata::compact_direct
								: detail::json_string_metadata::validated_escaped);
	}

	static constexpr void swap_node_storage(node_type &left, node_type &right) noexcept(
		noexcept(::std::ranges::swap(left.stor, right.stor)))
	{
		::std::ranges::swap(left.stor, right.stor);
		if constexpr (requires { left.string_metadata; })
		{
			::std::ranges::swap(left.string_metadata, right.string_metadata);
		}
	}

	[[nodiscard]] static node_type make_empty_node(allocator_type const &allocator)
	{
		if constexpr (::std::constructible_from<node_type, ::std::allocator_arg_t,
												allocator_type const &>)
		{
			return node_type(::std::allocator_arg, allocator);
		}
		else
		{
			static_assert(::std::constructible_from<node_type, allocator_type const &>,
						  "a custom fast_io JSON node must be constructible from its allocator");
			return node_type(allocator);
		}
	}

	static void destroy_value(node_type &node) noexcept
	{
		/*
		Destroying a container before disarming its children would let the
		container invoke node destructors on still-owning child variants.  Doing
		that recursively consumes one native stack frame per JSON level.  Keep
		the parent containers alive instead and walk one child at a time with an
		explicit stack.  A parent is destroyed only after every one of its direct
		children has been changed to monostate, so the destructors run by the
		container are shallow no-ops.

		Loop invariant: every frame owns a live container, its iterator denotes
		the first not-yet-visited child, and all preceding children are
		monostate.  Descending preserves the invariant.  Popping a completed
		frame proves that all children are disarmed, which makes
		destroy_shallow_value safe.  Thus each node is visited once in post-order
		and native call-stack use is constant.
		*/
		using array_iterator = decltype(::std::declval<array_type &>().begin());
		using array_sentinel = decltype(::std::declval<array_type &>().end());
		using object_iterator = decltype(::std::declval<object_type &>().begin());
		using object_sentinel = decltype(::std::declval<object_type &>().end());

		struct array_destruction_frame
		{
			node_type *owner;
			array_iterator current;
			array_sentinel last;
		};

		struct object_destruction_frame
		{
			node_type *owner;
			object_iterator current;
			object_sentinel last;
		};

		using destruction_frame =
			::std::variant<array_destruction_frame, object_destruction_frame>;

		auto const destroy_shallow_value = [](node_type &current_node) noexcept {
			switch (traits_type::kind(current_node.stor))
			{
			case json_kind::string:
				if constexpr (!traits_type::direct_string)
				{
					auto pointer{::std::get<typename traits_type::raw_string_type>(
						current_node.stor)};
					current_node.stor.template emplace<::std::monostate>();
					invalidate_string_metadata(current_node);
					traits_type::template destroy_deallocate<string_type>(
						current_node.alloc, pointer);
					return;
				}
				break;
			case json_kind::array:
			{
				auto pointer{::std::get<typename traits_type::raw_array_type>(
					current_node.stor)};
				current_node.stor.template emplace<::std::monostate>();
				invalidate_string_metadata(current_node);
				traits_type::template destroy_deallocate<array_type>(
					current_node.alloc, pointer);
				return;
			}
			case json_kind::object:
			{
				auto pointer{::std::get<typename traits_type::raw_object_type>(
					current_node.stor)};
				current_node.stor.template emplace<::std::monostate>();
				invalidate_string_metadata(current_node);
				traits_type::template destroy_deallocate<object_type>(
					current_node.alloc, pointer);
				return;
			}
			default:
				break;
			}

			current_node.stor.template emplace<::std::monostate>();
			invalidate_string_metadata(current_node);
		};

		/* Scalar teardown must stay as cheap as the old direct variant reset.
		   Avoid materializing the explicit stack unless the root actually has a
		   child to visit. */
		switch (traits_type::kind(node.stor))
		{
		case json_kind::array:
		{
			auto pointer{::std::get<typename traits_type::raw_array_type>(node.stor)};
			auto *const container{::std::to_address(pointer)};
			if (container->begin() != container->end())
			{
				break;
			}
			destroy_shallow_value(node);
			return;
		}
		case json_kind::object:
		{
			auto pointer{::std::get<typename traits_type::raw_object_type>(node.stor)};
			auto *const container{::std::to_address(pointer)};
			if (container->begin() != container->end())
			{
				break;
			}
			destroy_shallow_value(node);
			return;
		}
		default:
			destroy_shallow_value(node);
			return;
		}

		/*
		Emergency traversal used only when the explicit frame stack cannot grow.
		It deliberately owns no state that can allocate: one pass starts at the
		root, follows the first child whose variant is not exactly monostate, and
		destroys the first scalar or container having no such child.  A new pass
		then reconstructs the path from the still-live containers.

		Restart invariant: monostate nodes are finished and are never selected;
		every non-monostate node remains reachable from `node`.  A container is
		changed to monostate only when all of its direct children are already
		monostate, so deallocating it can invoke only shallow no-op child
		destructors.  Every pass changes at least one additional node to
		monostate.  The finite tree therefore reaches a monostate root without
		recursion or allocation.  Re-scanning can be O(node_count * depth), which
		is intentionally confined to an allocation-failure cleanup path.
		*/
		[[maybe_unused]] auto const destroy_by_restart_scan = [&]() noexcept {
			while (node.stor.index() != 0u)
			{
				node_type *current_node{::std::addressof(node)};
				for (;;)
				{
					bool descended{};
					switch (traits_type::kind(current_node->stor))
					{
					case json_kind::array:
					{
						auto pointer{
							::std::get<typename traits_type::raw_array_type>(
								current_node->stor)};
						auto *const container{::std::to_address(pointer)};
						for (auto &child : *container)
						{
							/* index()==0, rather than kind()==undefined, also
							   treats a valueless variant as unfinished. */
							if (child.stor.index() != 0u)
							{
								current_node = ::std::addressof(child);
								descended = true;
								break;
							}
						}
						break;
					}
					case json_kind::object:
					{
						auto pointer{
							::std::get<typename traits_type::raw_object_type>(
								current_node->stor)};
						auto *const container{::std::to_address(pointer)};
						for (auto &entry : *container)
						{
							auto &child{entry.second};
							if (child.stor.index() != 0u)
							{
								current_node = ::std::addressof(child);
								descended = true;
								break;
							}
						}
						break;
					}
					default:
						break;
					}

					if (descended)
					{
						continue;
					}
					destroy_shallow_value(*current_node);
					break;
				}
			}
		};

		/* Most JSON documents are shallow.  Keep their teardown allocation-free;
		   only pathological nesting needs the dynamically growing overflow.  The
		   byte array is intentionally uninitialized: the active prefix is given
		   an object lifetime by construct_at and ended by destroy_at, avoiding a
		   fixed-cost initialization of all inline slots for tiny containers.

		   Overflow storage is segmented: growing it never relocates live iterator
		   frames.  Sixty-four initial overflow slots avoid repeated allocations
		   around the inline boundary, while larger stacks use the JSON segmented
		   container's 64/256 schedule. */
		constexpr ::std::size_t inline_frame_count{32u};
		alignas(destruction_frame)::std::array<
			::std::byte, sizeof(destruction_frame) * inline_frame_count>
			inline_frame_storage;
		using overflow_frame_stack = basic_json_segmented_array<
			destruction_frame, ::std::allocator<destruction_frame>, 64u, 256u>;
		overflow_frame_stack overflow_frames;
		::std::size_t frame_count{};

		auto const inline_frame_pointer =
			[&](::std::size_t index) noexcept -> destruction_frame * {
			return reinterpret_cast<destruction_frame *>(
				inline_frame_storage.data() + sizeof(destruction_frame) * index);
		};

		auto const push_frame = [&](destruction_frame frame) {
			if (frame_count < inline_frame_count)
			{
				::std::construct_at(inline_frame_pointer(frame_count),
									::std::move(frame));
			}
			else
			{
				overflow_frames.emplace_back(::std::move(frame));
			}
			++frame_count;
		};

		auto const pop_frame = [&]() noexcept {
			if (frame_count > inline_frame_count)
			{
				overflow_frames.pop_back();
			}
			else
			{
				::std::destroy_at(
					::std::launder(inline_frame_pointer(frame_count - 1u)));
			}
			--frame_count;
		};

		auto const back_frame = [&]() noexcept -> destruction_frame & {
			if (frame_count > inline_frame_count)
			{
				return overflow_frames.back();
			}
			return *::std::launder(inline_frame_pointer(frame_count - 1u));
		};

		[[maybe_unused]] auto const clear_frames = [&]() noexcept {
			while (frame_count != 0u)
			{
				pop_frame();
			}
		};

		node_type *current_node{::std::addressof(node)};

#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
#endif
			for (;;)
			{
				switch (traits_type::kind(current_node->stor))
				{
				case json_kind::array:
				{
					auto pointer{::std::get<typename traits_type::raw_array_type>(
						current_node->stor)};
					auto *const container{::std::to_address(pointer)};
					auto iterator{container->begin()};
					auto last{container->end()};
					if (iterator != last)
					{
						auto *const child{::std::addressof(*iterator)};
						++iterator;
						push_frame(array_destruction_frame{
							current_node, ::std::move(iterator), ::std::move(last)});
						current_node = child;
						continue;
					}
					break;
				}
				case json_kind::object:
				{
					auto pointer{::std::get<typename traits_type::raw_object_type>(
						current_node->stor)};
					auto *const container{::std::to_address(pointer)};
					auto iterator{container->begin()};
					auto last{container->end()};
					if (iterator != last)
					{
						auto &entry{*iterator};
						auto *const child{::std::addressof(entry.second)};
						++iterator;
						push_frame(object_destruction_frame{
							current_node, ::std::move(iterator), ::std::move(last)});
						current_node = child;
						continue;
					}
					break;
				}
				default:
					break;
				}

				destroy_shallow_value(*current_node);

				for (;;)
				{
					if (frame_count == 0u)
					{
						return;
					}

					auto &frame{back_frame()};
					if (auto *const array_frame{
							::std::get_if<array_destruction_frame>(::std::addressof(frame))})
					{
						if (array_frame->current != array_frame->last)
						{
							current_node = ::std::addressof(*array_frame->current);
							++array_frame->current;
							break;
						}

						current_node = array_frame->owner;
						pop_frame();
						destroy_shallow_value(*current_node);
						continue;
					}

					auto &object_frame{::std::get<object_destruction_frame>(frame)};
					if (object_frame.current != object_frame.last)
					{
						auto &entry{*object_frame.current};
						current_node = ::std::addressof(entry.second);
						++object_frame.current;
						break;
					}

					current_node = object_frame.owner;
					pop_frame();
					destroy_shallow_value(*current_node);
				}
			}
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		}
		catch (...)
		{
			/* A failed push has not detached its child.  Frames only record
			   progress through still-live owners, so discarding them preserves
			   reachability and lets the allocation-free scan reconstruct it. */
			clear_frames();
			destroy_by_restart_scan();
		}
#endif
	}

	static void reset_node(node_type &node) noexcept
	{
		/* Container destruction calls every child node destructor after the
		   post-order pass has already disarmed it.  Avoid constructing an empty
		   traversal stack again for that overwhelmingly common path. */
		if (node.stor.index() != 0u)
		{
			destroy_value(node);
		}
		invalidate_string_metadata(node);
	}

	template <typename T, typename Value>
	static void set_scalar_node(node_type &node, Value &&value)
	{
		reset_node(node);
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
			node.stor.template emplace<T>(::std::forward<Value>(value));
		}
		catch (...)
		{
			node.stor.template emplace<::std::monostate>();
			throw;
		}
#else
		node.stor.template emplace<T>(::std::forward<Value>(value));
#endif
	}

	static void set_null_node(node_type &node)
	{
		set_scalar_node<nulljson_t>(node, nulljson);
	}
	static void set_boolean_node(node_type &node, bool value)
	{
		set_scalar_node<bool>(node, value);
	}
	static void set_number_node(node_type &node, number_type value)
	{
		set_scalar_node<number_type>(node, ::std::move(value));
	}
	static void set_integer_node(node_type &node, integer_type value)
		requires(has_integer)
	{
		set_scalar_node<integer_type>(node, ::std::move(value));
	}
	static void set_uinteger_node(node_type &node, uinteger_type value)
		requires(has_uinteger)
	{
		set_scalar_node<uinteger_type>(node, ::std::move(value));
	}

	template <typename T, typename Raw, typename... Args>
	static T &emplace_indirect_node(node_type &node, Args &&...args)
	{
		auto pointer{traits_type::template allocate_construct<T>(node.alloc, ::std::forward<Args>(args)...)};
		struct pointer_guard
		{
			node_type *node_pointer;
			Raw allocated_pointer;
			bool released{};
			~pointer_guard()
			{
				if (!released)
				{
					traits_type::template destroy_deallocate<T>(node_pointer->alloc, allocated_pointer);
				}
			}
		} guard{::std::addressof(node), pointer, false};
		reset_node(node);
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
		try
		{
			node.stor.template emplace<Raw>(pointer);
		}
		catch (...)
		{
			node.stor.template emplace<::std::monostate>();
			throw;
		}
#else
		node.stor.template emplace<Raw>(pointer);
#endif
		guard.released = true;
		return *::std::to_address(pointer);
	}

	template <typename... Args>
	static string_type &emplace_string_node(node_type &node, Args &&...args)
	{
		string_type *result{};
		if constexpr (traits_type::direct_string)
		{
			reset_node(node);
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
			try
			{
				result = ::std::addressof(
					node.stor.template emplace<typename traits_type::raw_string_type>(
						::std::forward<Args>(args)...));
			}
			catch (...)
			{
				node.stor.template emplace<::std::monostate>();
				throw;
			}
#else
			result = ::std::addressof(
				node.stor.template emplace<typename traits_type::raw_string_type>(
					::std::forward<Args>(args)...));
#endif
		}
		else
		{
			result = ::std::addressof(
				emplace_indirect_node<string_type, typename traits_type::raw_string_type>(
					node, ::std::forward<Args>(args)...));
		}
		set_string_metadata(node, detail::classify_json_string_metadata(*result));
		return *result;
	}

	template <typename Value>
	static string_type &assign_string_node(node_type &node, Value &&value)
	{
		auto *const result{traits_type::template get_if<string_type>(
			::std::addressof(node.stor))};
		if (result == nullptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		/* basic_string assignment can throw after changing its target.  Drop
		   the positive proof before entering that operation, not afterwards. */
		invalidate_string_metadata(node);
		*result = ::std::forward<Value>(value);
		set_string_metadata(node, detail::classify_json_string_metadata(*result));
		return *result;
	}

	static array_type &emplace_array_node(node_type &node)
	{
		return emplace_indirect_node<array_type, typename traits_type::raw_array_type>(node);
	}

	static object_type &emplace_object_node(node_type &node)
	{
		return emplace_indirect_node<object_type, typename traits_type::raw_object_type>(node);
	}

	[[nodiscard]] static node_type make_child_node(node_type const &parent)
	{
		return make_empty_node(parent.alloc);
	}

	static void clone_node_into(node_type &result, node_type const &source,
								allocator_type const &allocator)
	{
		reset_node(result);
		struct result_guard
		{
			node_type *node_pointer;
			bool released{};
			~result_guard()
			{
				if (!released)
				{
					reset_node(*node_pointer);
				}
			}
		} guard{::std::addressof(result), false};

		auto const clone_noncontainer = [](node_type &destination,
										  node_type const &input,
										  json_kind input_kind) -> bool {
			switch (input_kind)
			{
			case json_kind::undefined:
				break;
			case json_kind::null:
				set_null_node(destination);
				break;
			case json_kind::boolean:
				set_boolean_node(
					destination,
					*traits_type::template get_if<bool>(
						::std::addressof(input.stor)));
				break;
			case json_kind::number:
				set_number_node(
					destination,
					*traits_type::template get_if<number_type>(
						::std::addressof(input.stor)));
				break;
			case json_kind::integer:
				if constexpr (has_integer)
				{
					set_integer_node(
						destination,
						*traits_type::template get_if<integer_type>(
							::std::addressof(input.stor)));
				}
				break;
			case json_kind::uinteger:
				if constexpr (has_uinteger)
				{
					set_uinteger_node(
						destination,
						*traits_type::template get_if<uinteger_type>(
							::std::addressof(input.stor)));
				}
				break;
			case json_kind::string:
				emplace_string_node(
					destination,
					*traits_type::template get_if<string_type>(
						::std::addressof(input.stor)));
				break;
			case json_kind::array:
			case json_kind::object:
				return false;
			}
			return true;
		};

		/* Scalars and strings retain the old direct path.  Besides avoiding all
		   traversal bookkeeping, placing this return before the inline frame
		   storage lets optimizing compilers shrink-wrap the comparatively large
		   stack reservation to actual container clones. */
		auto const root_kind{traits_type::kind(source.stor)};
		if (clone_noncontainer(result, source, root_kind))
		{
			guard.released = true;
			return;
		}

		/*
		A recursive clone holds one native call frame for every containing array or
		object.  JSON nesting is not bounded by the type system, so use the same
		pre-order state machine as the serializer: `source_node` and
		`destination_node` denote the one value being copied, while each frame
		retains the first source sibling not yet copied and its stable destination
		container.

		Loop invariant: every destination node preceding a frame's current source
		iterator is a complete clone using `allocator`; the current destination
		node is either still monostate or a complete scalar/empty container.  A
		descent first attaches an empty child to the owning result tree, so any
		subsequent allocation or value-copy failure is covered by `guard`.
		Consequently reset_node(result) destroys every successfully attached value
		and gives the operation the strong cleanup guarantee without recursion.
		*/
		using array_const_iterator =
			decltype(::std::declval<array_type const &>().begin());
		using array_const_sentinel =
			decltype(::std::declval<array_type const &>().end());
		using object_const_iterator =
			decltype(::std::declval<object_type const &>().begin());
		using object_const_sentinel =
			decltype(::std::declval<object_type const &>().end());

		struct array_clone_frame
		{
			array_type *destination;
			array_const_iterator current;
			array_const_sentinel last;
		};

		struct object_clone_frame
		{
			object_type *destination;
			object_const_iterator current;
			object_const_sentinel last;
		};

		using clone_frame = ::std::variant<array_clone_frame, object_clone_frame>;

		/* Small documents must not pay for a separate traversal allocation.  The
		   active prefix alone has object lifetime; the guard is required because a
		   scalar, string, container, or overflow-vector operation may throw while
		   inline iterator objects are live. */
		constexpr ::std::size_t inline_frame_count{32u};
		alignas(clone_frame)::std::array<
			::std::byte, sizeof(clone_frame) * inline_frame_count>
			inline_frame_storage;
		::std::vector<clone_frame> overflow_frames;
		::std::size_t frame_count{};

		struct inline_frame_guard
		{
			::std::byte *storage;
			::std::size_t const *count;
			::std::size_t capacity;

			~inline_frame_guard()
			{
				auto active{*count < capacity
							? *count
							: capacity};
				while (active != 0u)
				{
					--active;
					auto *const pointer{reinterpret_cast<clone_frame *>(
						storage + sizeof(clone_frame) * active)};
					::std::destroy_at(::std::launder(pointer));
				}
			}
		} inline_guard{inline_frame_storage.data(), ::std::addressof(frame_count),
					   inline_frame_count};

		auto const inline_frame_pointer =
			[&](::std::size_t index) noexcept -> clone_frame * {
			return reinterpret_cast<clone_frame *>(
				inline_frame_storage.data() + sizeof(clone_frame) * index);
		};

		auto const push_frame = [&](clone_frame frame) {
			if (frame_count < inline_frame_count)
			{
				::std::construct_at(inline_frame_pointer(frame_count),
									::std::move(frame));
			}
			else
			{
				overflow_frames.emplace_back(::std::move(frame));
			}
			++frame_count;
		};

		auto const pop_frame = [&]() noexcept {
			if (frame_count > inline_frame_count)
			{
				overflow_frames.pop_back();
			}
			else
			{
				::std::destroy_at(
					::std::launder(inline_frame_pointer(frame_count - 1u)));
			}
			--frame_count;
		};

		auto const back_frame = [&]() noexcept -> clone_frame & {
			if (frame_count > inline_frame_count)
			{
				return overflow_frames.back();
			}
			return *::std::launder(inline_frame_pointer(frame_count - 1u));
		};

		auto const append_array_child = [&](array_type &destination) -> node_type & {
			/* `uses_allocator_v` describes an element constructor protocol; it
			   does not prove that an arbitrary container allocator performs nested
			   allocator injection.  In particular, a plain stateful allocator used
			   by std::vector may forward an empty emplace argument list unchanged.
			   Materialize the empty child with the clone's allocator first.  Both a
			   conventional container move and an allocator-aware segmented move then
			   preserve that allocator, matching the parser's child-insertion path. */
			destination.emplace_back(make_empty_node(allocator));
			return destination.back();
		};

		auto const append_object_child =
			[&](object_type &destination,
				object_const_iterator source_iterator) -> node_type & {
				/* Associative pair construction has the same distinction between
				   accepting an allocator and propagating it into the mapped node.
				   The key copy must likewise be rebound explicitly: an ordinary
				   basic_string copy selects from the source string allocator. */
				using key_allocator_type =
					typename object_type::key_type::allocator_type;
				auto target_key_allocator{[&]() -> key_allocator_type {
					if constexpr (::std::constructible_from<
								  key_allocator_type, allocator_type const &>)
					{
						return key_allocator_type{allocator};
					}
					else if constexpr (::std::constructible_from<
								   key_allocator_type,
								   typename object_type::allocator_type const &>)
					{
						return key_allocator_type{destination.get_allocator()};
					}
					else
					{
						/* A custom node may deliberately use an unrelated key
						   allocator domain.  In that case no target rebind exists;
						   preserve the key's own allocator rather than inventing a
						   default-constructibility requirement. */
						return source_iterator->first.get_allocator();
					}
				}()};
				typename object_type::key_type target_key{
					source_iterator->first, ::std::move(target_key_allocator)};
				auto inserted{detail::json_object_emplace_known_absent(
					destination, ::std::move(target_key),
					make_empty_node(allocator))};
				return inserted->second;
			};

		node_type const *source_node{::std::addressof(source)};
		node_type *destination_node{::std::addressof(result)};

		for (;;)
		{
			auto const source_kind{traits_type::kind(source_node->stor)};
			if (!clone_noncontainer(*destination_node, *source_node, source_kind))
			{
				if (source_kind == json_kind::array)
				{
					auto &destination_array{
						emplace_array_node(*destination_node)};
					auto const &source_array{
						*traits_type::template get_if<array_type>(
							::std::addressof(source_node->stor))};
					if constexpr (requires {
								  destination_array.reserve(source_array.size());
							  })
					{
						destination_array.reserve(source_array.size());
					}

					auto current{source_array.begin()};
					auto last{source_array.end()};
					if (current != last)
					{
						auto const *const child_source{
							::std::addressof(*current)};
						++current;
						auto *const child_destination{::std::addressof(
							append_array_child(destination_array))};
						push_frame(array_clone_frame{
							::std::addressof(destination_array),
							::std::move(current), ::std::move(last)});
						source_node = child_source;
						destination_node = child_destination;
						continue;
					}
				}
				else
				{
					auto &destination_object{
						emplace_object_node(*destination_node)};
					auto const &source_object{
						*traits_type::template get_if<object_type>(
							::std::addressof(source_node->stor))};
					auto current{source_object.begin()};
					auto last{source_object.end()};
					if (current != last)
					{
						auto source_iterator{current};
						++current;
						auto *const child_destination{::std::addressof(
							append_object_child(destination_object,
												source_iterator))};
						push_frame(object_clone_frame{
							::std::addressof(destination_object),
							::std::move(current), ::std::move(last)});
						source_node =
							::std::addressof(source_iterator->second);
						destination_node = child_destination;
						continue;
					}
				}
			}

			/* The current subtree is complete.  Find the nearest frame with a
			   remaining sibling; completed container frames need no separate
			   action because their owning nodes were installed on descent. */
			for (;;)
			{
				if (frame_count == 0u)
				{
					guard.released = true;
					return;
				}

				auto &frame{back_frame()};
				if (auto *const array_frame{
						::std::get_if<array_clone_frame>(::std::addressof(frame))})
				{
					if (array_frame->current != array_frame->last)
					{
						source_node =
							::std::addressof(*array_frame->current);
						++array_frame->current;
						destination_node = ::std::addressof(
							append_array_child(*array_frame->destination));
						break;
					}
					pop_frame();
					continue;
				}

				auto &object_frame{::std::get<object_clone_frame>(frame)};
				if (object_frame.current != object_frame.last)
				{
					auto source_iterator{object_frame.current};
					++object_frame.current;
					source_node = ::std::addressof(source_iterator->second);
					destination_node = ::std::addressof(
						append_object_child(*object_frame.destination,
											source_iterator));
					break;
				}
				pop_frame();
			}
		}
		}

	/* A tree cannot record parent pointers without enlarging every DOM node and
	   slowing the ordinary serializer.  Owning-rvalue insertion is rare enough
	   to perform this explicit O(nodes), O(depth) guard instead.  It is iterative
	   so the same 100k-depth documents supported by clone/destroy remain safe. */
	[[nodiscard]] static bool node_reaches(
		node_type const &root, node_type const &target)
	{
		if (::std::addressof(root) == ::std::addressof(target))
		{
			return true;
		}
		auto const root_kind{traits_type::kind(root.stor)};
		if (root_kind != json_kind::array && root_kind != json_kind::object)
		{
			return false;
		}
		if (root_kind == json_kind::array &&
			traits_type::template get_if<array_type>(
				::std::addressof(root.stor))->empty())
		{
			return false;
		}
		if (root_kind == json_kind::object &&
			traits_type::template get_if<object_type>(
				::std::addressof(root.stor))->empty())
		{
			return false;
		}

		using array_iterator = decltype(
			::std::declval<array_type const &>().begin());
		using array_sentinel = decltype(
			::std::declval<array_type const &>().end());
		using object_iterator = decltype(
			::std::declval<object_type const &>().begin());
		using object_sentinel = decltype(
			::std::declval<object_type const &>().end());

		struct array_frame
		{
			array_iterator current;
			array_sentinel last;
		};
		struct object_frame
		{
			object_iterator current;
			object_sentinel last;
		};
		using traversal_frame = ::std::variant<array_frame, object_frame>;

		/* The cycle guard is on an ownership-transfer path, but shallow moves are
		   still common.  Keep their ancestor cursors in automatic storage and grow
		   only beyond 32 nested containers. */
		constexpr ::std::size_t inline_frame_count{32u};
		alignas(traversal_frame)::std::array<
			::std::byte, sizeof(traversal_frame) * inline_frame_count>
			inline_frame_storage;
		::std::vector<traversal_frame> overflow_frames;
		::std::size_t frame_count{};
		struct inline_frame_guard
		{
			::std::byte *storage;
			::std::size_t const *count;
			::std::size_t capacity;
			~inline_frame_guard()
			{
				auto active{*count < capacity
					? *count
					: capacity};
				while (active != 0u)
				{
					--active;
					auto *const frame{reinterpret_cast<traversal_frame *>(
						storage + sizeof(traversal_frame) * active)};
					::std::destroy_at(::std::launder(frame));
				}
			}
		} inline_guard{inline_frame_storage.data(),
			::std::addressof(frame_count), inline_frame_count};
		auto const inline_frame_pointer =
			[&](::std::size_t index) noexcept -> traversal_frame * {
			return reinterpret_cast<traversal_frame *>(
				inline_frame_storage.data() + sizeof(traversal_frame) * index);
		};
		auto const push_frame = [&](traversal_frame frame) {
			if (frame_count < inline_frame_count)
			{
				::std::construct_at(
					inline_frame_pointer(frame_count), ::std::move(frame));
			}
			else
			{
				overflow_frames.emplace_back(::std::move(frame));
			}
			++frame_count;
		};
		auto const pop_frame = [&]() noexcept {
			if (frame_count > inline_frame_count)
			{
				overflow_frames.pop_back();
			}
			else
			{
				::std::destroy_at(::std::launder(
					inline_frame_pointer(frame_count - 1u)));
			}
			--frame_count;
		};
		auto const back_frame = [&]() noexcept -> traversal_frame & {
			if (frame_count > inline_frame_count)
			{
				return overflow_frames.back();
			}
			return *::std::launder(
				inline_frame_pointer(frame_count - 1u));
		};
		auto const *current_node{::std::addressof(root)};

		for (;;)
		{
			auto const current_kind{traits_type::kind(current_node->stor)};
			if (current_kind == json_kind::array)
			{
				auto const &array{*traits_type::template get_if<array_type>(
					::std::addressof(current_node->stor))};
				auto current{array.begin()};
				auto last{array.end()};
				if (current != last)
				{
					current_node = ::std::addressof(*current);
					++current;
					push_frame(array_frame{
						::std::move(current), ::std::move(last)});
					if (current_node == ::std::addressof(target))
					{
						return true;
					}
					continue;
				}
			}
			else if (current_kind == json_kind::object)
			{
				auto const &object{*traits_type::template get_if<object_type>(
					::std::addressof(current_node->stor))};
				auto current{object.begin()};
				auto last{object.end()};
				if (current != last)
				{
					current_node = ::std::addressof(current->second);
					++current;
					push_frame(object_frame{
						::std::move(current), ::std::move(last)});
					if (current_node == ::std::addressof(target))
					{
						return true;
					}
					continue;
				}
			}

			for (;;)
			{
				if (frame_count == 0u)
				{
					return false;
				}
				auto &frame{back_frame()};
				if (auto *const array{::std::get_if<array_frame>(
						::std::addressof(frame))})
				{
					if (array->current == array->last)
					{
						pop_frame();
						continue;
					}
					current_node = ::std::addressof(*array->current);
					++array->current;
				}
				else
				{
					auto &object{::std::get<object_frame>(frame)};
					if (object.current == object.last)
					{
						pop_frame();
						continue;
					}
					current_node =
						::std::addressof(object.current->second);
					++object.current;
				}
				if (current_node == ::std::addressof(target))
				{
					return true;
				}
				break;
			}
		}
	}

	static void transfer_storage(node_type &destination, node_type &source) noexcept
	{
		swap_node_storage(destination, source);
		source.stor.template emplace<::std::monostate>();
		invalidate_string_metadata(source);
	}

public:
	constexpr basic_json()
		requires ::std::default_initializable<allocator_type>
	= default;

	constexpr explicit basic_json(allocator_type const &allocator) : node_(make_empty_node(allocator))
	{}

	basic_json(basic_json const &other)
		: node_(make_empty_node(
			  ::std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.node_.alloc)))
	{
		clone_node_into(node_, other.node_, node_.alloc);
	}

	explicit basic_json(basic_json const &other, allocator_type const &allocator)
		: node_(make_empty_node(allocator))
	{
		clone_node_into(node_, other.node_, allocator);
	}

	basic_json(basic_json &&other)
		: node_(make_empty_node(other.node_.alloc))
	{
		swap_node_storage(node_, other.node_);
	}

	basic_json(basic_json &&other, allocator_type const &allocator)
		: node_(make_empty_node(allocator))
	{
		if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
		{
			swap_node_storage(node_, other.node_);
		}
		else if (allocator == other.node_.alloc)
		{
			swap_node_storage(node_, other.node_);
		}
		else
		{
			clone_node_into(node_, other.node_, allocator);
			reset_node(other.node_);
		}
	}

	constexpr basic_json(nulljson_t, allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		set_null_node(node_);
	}

	template <typename T>
		requires(::std::same_as<::std::remove_cvref_t<T>, bool> ||
				 ::fast_io::json_signed_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_unsigned_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_floating_point<::std::remove_cvref_t<T>>)
	constexpr basic_json(T &&value, allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		using input_type = ::std::remove_cvref_t<T>;
		if constexpr (::std::same_as<input_type, bool>)
		{
			set_boolean_node(node_, value);
		}
		else if constexpr (::fast_io::json_signed_integer<input_type> && has_integer)
		{
			set_integer_node(node_, integer_type(::std::forward<T>(value)));
		}
		else if constexpr (::fast_io::json_unsigned_integer<input_type> && has_uinteger)
		{
			set_uinteger_node(node_, uinteger_type(::std::forward<T>(value)));
		}
		else
		{
			set_number_node(node_, number_type(::std::forward<T>(value)));
		}
	}

	constexpr explicit basic_json(string_type value, allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		emplace_string_node(node_, ::std::move(value));
	}

	constexpr basic_json(char_type const *value, typename string_type::size_type count,
						 allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		if (value == nullptr) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::null_pointer);
		}
		emplace_string_node(node_, value, count);
	}

	constexpr basic_json(char_type const *value, allocator_type const &allocator = allocator_type())
		: node_(make_empty_node(allocator))
	{
		if (value == nullptr) [[unlikely]]
		{
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::null_pointer);
		}
		emplace_string_node(node_, value);
	}

	template <typename StringLike>
		requires(!::std::same_as<::std::remove_cvref_t<StringLike>, string_type> &&
				 !::std::is_convertible_v<StringLike const &, char_type const *> &&
				 ::std::constructible_from<string_type, StringLike const &>)
	constexpr explicit basic_json(StringLike const &value, allocator_type const &allocator = allocator_type())
		: node_(make_empty_node(allocator))
	{
		emplace_string_node(node_, value);
	}

	constexpr basic_json(array_type value, allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		struct elements_guard
		{
			array_type *array_pointer;
			bool released{};
			~elements_guard()
			{
				if (!released)
				{
					for (auto &child : *array_pointer)
					{
						reset_node(child);
					}
				}
			}
		} guard{::std::addressof(value), false};
		emplace_indirect_node<array_type, typename traits_type::raw_array_type>(node_, ::std::move(value));
		guard.released = true;
	}

	constexpr basic_json(object_type value, allocator_type const &allocator = allocator_type()) : node_(make_empty_node(allocator))
	{
		struct elements_guard
		{
			object_type *object_pointer;
			bool released{};
			~elements_guard()
			{
				if (!released)
				{
					for (auto &entry : *object_pointer)
					{
						reset_node(entry.second);
					}
				}
			}
		} guard{::std::addressof(value), false};
		emplace_indirect_node<object_type, typename traits_type::raw_object_type>(node_, ::std::move(value));
		guard.released = true;
	}

	constexpr basic_json(::std::nullptr_t, allocator_type const & = allocator_type()) = delete;

	constexpr explicit basic_json(node_type &&node) noexcept(::std::is_nothrow_move_constructible_v<node_type>)
		: node_(::std::move(node))
	{
		node.stor.template emplace<::std::monostate>();
	}

	explicit basic_json(node_type const &node) : node_(make_empty_node(node.alloc))
	{
		clone_node_into(node_, node, node.alloc);
	}

	basic_json(node_type &&node, allocator_type const &allocator) : node_(make_empty_node(allocator))
	{
		if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
		{
			swap_node_storage(node_, node);
		}
		else if (node.alloc == allocator)
		{
			swap_node_storage(node_, node);
		}
		else
		{
			clone_node_into(node_, node, allocator);
			reset_node(node);
		}
	}

	basic_json &operator=(basic_json const &other)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		allocator_type target_allocator{node_.alloc};
		if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
		{
			target_allocator = other.node_.alloc;
		}
		basic_json replacement(other, target_allocator);
		reset_node(node_);
		if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
		{
			node_.alloc = target_allocator;
		}
		swap_node_storage(node_, replacement.node_);
		return *this;
	}

	basic_json &operator=(basic_json &&other)
	{
		if (this == ::std::addressof(other))
		{
			return *this;
		}
		if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value)
		{
			reset_node(node_);
			node_.alloc = ::std::move(other.node_.alloc);
			swap_node_storage(node_, other.node_);
		}
		else if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
		{
			reset_node(node_);
			swap_node_storage(node_, other.node_);
		}
		else if (node_.alloc == other.node_.alloc)
		{
			reset_node(node_);
			swap_node_storage(node_, other.node_);
		}
		else
		{
			basic_json replacement(other, node_.alloc);
			reset_node(node_);
			swap_node_storage(node_, replacement.node_);
			reset_node(other.node_);
		}
		return *this;
	}

	basic_json &operator=(nulljson_t value)
	{
		slice() = value;
		return *this;
	}

	template <typename T>
		requires(::std::same_as<::std::remove_cvref_t<T>, bool> ||
				 ::fast_io::json_signed_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_unsigned_integer<::std::remove_cvref_t<T>> ||
				 ::fast_io::json_floating_point<::std::remove_cvref_t<T>>)
	basic_json &operator=(T &&value)
	{
		slice() = ::std::forward<T>(value);
		return *this;
	}

	basic_json &operator=(string_type const &value)
	{
		slice() = value;
		return *this;
	}

	basic_json &operator=(string_type &&value)
	{
		slice() = ::std::move(value);
		return *this;
	}

	basic_json &operator=(char_type const *value)
	{
		slice() = value;
		return *this;
	}
	basic_json &operator=(::std::nullptr_t) = delete;

	template <typename StringLike>
		requires(!::std::same_as<::std::remove_cvref_t<StringLike>, string_type> &&
				 !::std::is_convertible_v<StringLike const &, char_type const *> &&
				 ::std::constructible_from<string_type, StringLike const &>)
	basic_json &operator=(StringLike const &value)
	{
		slice() = value;
		return *this;
	}

	basic_json &operator=(slice_type value)
	{
		slice() = value;
		return *this;
	}

	basic_json &operator=(const_slice_type value)
	{
		slice() = value;
		return *this;
	}

	~basic_json() noexcept
	{
		reset_node(node_);
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept
	{
		return node_.alloc;
	}
	[[nodiscard]] constexpr json_kind kind() const noexcept
	{
		return traits_type::kind(node_.stor);
	}
	[[nodiscard]] constexpr bool undefined() const noexcept
	{
		return kind() == json_kind::undefined;
	}
	[[nodiscard]] constexpr bool null() const noexcept
	{
		return kind() == json_kind::null;
	}
	[[nodiscard]] constexpr bool boolean() const noexcept
	{
		return kind() == json_kind::boolean;
	}
	[[nodiscard]] constexpr bool string() const noexcept
	{
		return kind() == json_kind::string;
	}
	[[nodiscard]] constexpr bool is_array() const noexcept
	{
		return kind() == json_kind::array;
	}
	[[nodiscard]] constexpr bool is_object() const noexcept
	{
		return kind() == json_kind::object;
	}
	[[nodiscard]] constexpr bool integer() const noexcept
		requires(has_integer)
	{
		return kind() == json_kind::integer;
	}
	[[nodiscard]] constexpr bool uinteger() const noexcept
		requires(has_uinteger)
	{
		return kind() == json_kind::uinteger;
	}
	[[nodiscard]] constexpr bool number() const noexcept
	{
		return kind() == json_kind::number || (has_integer && kind() == json_kind::integer) ||
			   (has_uinteger && kind() == json_kind::uinteger);
	}
	[[nodiscard]] constexpr bool is_undefined() const noexcept
	{
		return undefined();
	}
	[[nodiscard]] constexpr bool is_null() const noexcept
	{
		return null();
	}
	[[nodiscard]] constexpr bool is_boolean() const noexcept
	{
		return boolean();
	}
	[[nodiscard]] constexpr bool is_number() const noexcept
	{
		return number();
	}
	[[nodiscard]] constexpr bool is_integer() const noexcept
		requires(has_integer)
	{
		return integer();
	}
	[[nodiscard]] constexpr bool is_uinteger() const noexcept
		requires(has_uinteger)
	{
		return uinteger();
	}
	[[nodiscard]] constexpr bool is_string() const noexcept
	{
		return string();
	}

	template <typename T>
	[[nodiscard]] constexpr T *get_if() & noexcept
	{
		if constexpr (::std::same_as<::std::remove_cv_t<T>, string_type>)
		{
			invalidate_string_metadata(node_);
		}
		return traits_type::template get_if<T>(::std::addressof(node_.stor));
	}
	template <typename T>
	[[nodiscard]] constexpr T const *get_if() const & noexcept
	{
		return traits_type::template get_if<T>(::std::addressof(node_.stor));
	}
	template <typename T>
	T *get_if() && = delete;
	template <typename T>
	T const *get_if() const && = delete;

	[[nodiscard]] constexpr slice_type slice() & noexcept
	{
		return slice_type{detail::json_slice_pointer_arg,
						  static_cast<void *>(::std::addressof(node_))};
	}
	[[nodiscard]] constexpr const_slice_type slice() const & noexcept
	{
		return const_slice_type{detail::json_slice_pointer_arg,
								static_cast<void const *>(::std::addressof(node_))};
	}
	const_slice_type slice() && = delete;
	const_slice_type slice() const && = delete;

	/* Owning documents forward the common DOM surface to a zero-cost slice.
	   Users should not need to expose node_type or spell `.slice()` merely to
	   inspect or edit a document. */
	[[nodiscard]] constexpr ::std::size_t size() const noexcept
	{
		return slice().size();
	}

	[[nodiscard]] constexpr bool value_empty() const noexcept
	{
		return slice().value_empty();
	}

	[[nodiscard]] constexpr bool &get_boolean() &
	{
		return slice().get_boolean();
	}

	[[nodiscard]] constexpr bool get_boolean() const &
	{
		return slice().get_boolean();
	}
	bool get_boolean() && = delete;
	bool get_boolean() const && = delete;

	[[nodiscard]] constexpr number_type &get_number() &
	{
		return slice().get_number();
	}

	[[nodiscard]] constexpr number_type const &get_number() const &
	{
		return slice().get_number();
	}
	number_type &get_number() && = delete;
	number_type const &get_number() const && = delete;

	[[nodiscard]] constexpr number_type as_number() const
	{
		return slice().as_number();
	}

	[[nodiscard]] constexpr integer_type &get_integer() &
		requires(has_integer)
	{
		return slice().get_integer();
	}

	[[nodiscard]] constexpr integer_type const &get_integer() const &
		requires(has_integer)
	{
		return slice().get_integer();
	}
	integer_type &get_integer() && = delete;
	integer_type const &get_integer() const && = delete;

	[[nodiscard]] constexpr uinteger_type &get_uinteger() &
		requires(has_uinteger)
	{
		return slice().get_uinteger();
	}

	[[nodiscard]] constexpr uinteger_type const &get_uinteger() const &
		requires(has_uinteger)
	{
		return slice().get_uinteger();
	}
	uinteger_type &get_uinteger() && = delete;
	uinteger_type const &get_uinteger() const && = delete;

	[[nodiscard]] constexpr string_type &get_string() &
	{
		return slice().get_string();
	}

	[[nodiscard]] constexpr string_type const &get_string() const &
	{
		return slice().get_string();
	}
	string_type &get_string() && = delete;
	string_type const &get_string() const && = delete;

	[[nodiscard]] constexpr array_type &get_array() &
	{
		return slice().get_array();
	}

	[[nodiscard]] constexpr array_type const &get_array() const &
	{
		return slice().get_array();
	}
	array_type &get_array() && = delete;
	array_type const &get_array() const && = delete;

	[[nodiscard]] constexpr object_type &get_object() &
	{
		return slice().get_object();
	}

	[[nodiscard]] constexpr object_type const &get_object() const &
	{
		return slice().get_object();
	}
	object_type &get_object() && = delete;
	object_type const &get_object() const && = delete;

	template <typename Index>
		requires requires(slice_type value, Index &&index) {
			value[::std::forward<Index>(index)];
		}
	[[nodiscard]] constexpr auto operator[](Index &&index) &
	{
		return slice()[::std::forward<Index>(index)];
	}

	template <typename Index>
		requires requires(const_slice_type value, Index &&index) {
			value[::std::forward<Index>(index)];
		}
	[[nodiscard]] constexpr auto operator[](Index &&index) const &
	{
		return slice()[::std::forward<Index>(index)];
	}
	template <typename Index>
	auto operator[](Index &&) && = delete;
	template <typename Index>
	auto operator[](Index &&) const && = delete;

	template <typename Index>
		requires requires(slice_type value, Index &&index) {
			value.at(::std::forward<Index>(index));
		}
	[[nodiscard]] constexpr auto at(Index &&index) &
	{
		return slice().at(::std::forward<Index>(index));
	}

	template <typename Index>
		requires requires(const_slice_type value, Index &&index) {
			value.at(::std::forward<Index>(index));
		}
	[[nodiscard]] constexpr auto at(Index &&index) const &
	{
		return slice().at(::std::forward<Index>(index));
	}
	template <typename Index>
	auto at(Index &&) && = delete;
	template <typename Index>
	auto at(Index &&) const && = delete;

	template <typename Key>
		requires requires(slice_type value, Key const &key) {
			value.find(key);
		}
	[[nodiscard]] constexpr auto find(Key const &key) &
	{
		return slice().find(key);
	}

	template <typename Key>
		requires requires(const_slice_type value, Key const &key) {
			value.find(key);
		}
	[[nodiscard]] constexpr auto find(Key const &key) const &
	{
		return slice().find(key);
	}
	template <typename Key>
	auto find(Key const &) && = delete;
	template <typename Key>
	auto find(Key const &) const && = delete;

	template <typename Key>
		requires requires(const_slice_type value, Key const &key) {
			value.contains(key);
		}
	[[nodiscard]] constexpr bool contains(Key const &key) const
	{
		return slice().contains(key);
	}

	[[nodiscard]] constexpr auto front() &
	{
		return slice().front();
	}

	[[nodiscard]] constexpr auto front() const &
	{
		return slice().front();
	}
	auto front() && = delete;
	auto front() const && = delete;

	[[nodiscard]] constexpr auto back() &
	{
		return slice().back();
	}

	[[nodiscard]] constexpr auto back() const &
	{
		return slice().back();
	}
	auto back() && = delete;
	auto back() const && = delete;

	[[nodiscard]] constexpr auto as_array() &
	{
		return slice().as_array();
	}

	[[nodiscard]] constexpr auto as_array() const &
	{
		return slice().as_array();
	}
	auto as_array() && = delete;
	auto as_array() const && = delete;

	[[nodiscard]] constexpr auto as_object() &
	{
		return slice().as_object();
	}

	[[nodiscard]] constexpr auto as_object() const &
	{
		return slice().as_object();
	}
	auto as_object() && = delete;
	auto as_object() const && = delete;

	template <typename Value>
	auto emplace_back(Value &&value) &
	{
		return slice().emplace_back(::std::forward<Value>(value));
	}

	template <typename Value>
	void push_back(Value &&value) &
	{
		slice().push_back(::std::forward<Value>(value));
	}

	void pop_back() &
	{
		slice().pop_back();
	}

	template <typename Position, typename Value>
		requires requires(slice_type target, Position position, Value &&value) {
			target.insert(position, ::std::forward<Value>(value));
		}
	auto insert(Position position, Value &&value) &
	{
		return slice().insert(position, ::std::forward<Value>(value));
	}

	template <typename Key, typename Value>
		requires requires(slice_type target, Key const &key, Value &&value) {
			target.insert_or_assign(key, ::std::forward<Value>(value));
		}
	auto insert_or_assign(Key const &key, Value &&value) &
	{
		return slice().insert_or_assign(
			key, ::std::forward<Value>(value));
	}

	template <typename Key, typename... Values>
		requires requires(slice_type target, Key const &key,
						  Values &&...values) {
			target.try_emplace(key, ::std::forward<Values>(values)...);
		}
	auto try_emplace(Key const &key, Values &&...values) &
	{
		return slice().try_emplace(
			key, ::std::forward<Values>(values)...);
	}

	template <typename Index>
		requires requires(slice_type target, Index &&index) {
			target.erase(::std::forward<Index>(index));
		}
	auto erase(Index &&index) &
	{
		return slice().erase(::std::forward<Index>(index));
	}

	void resize(typename array_type::size_type requested) &
	{
		slice().resize(requested);
	}

	void reserve(typename array_type::size_type requested) &
	{
		slice().reserve(requested);
	}

	void clear()
	{
		slice().clear();
	}

	constexpr void reset() noexcept
	{
		reset_node(node_);
	}

	void swap(basic_json &other)
	{
		if (this == ::std::addressof(other))
		{
			return;
		}
		if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_swap::value)
		{
			::std::ranges::swap(node_.alloc, other.node_.alloc);
			swap_node_storage(node_, other.node_);
		}
		else if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
		{
			swap_node_storage(node_, other.node_);
		}
		else if (node_.alloc == other.node_.alloc)
		{
			swap_node_storage(node_, other.node_);
		}
		else
		{
			basic_json left_copy(*this, other.node_.alloc);
			basic_json right_copy(other, node_.alloc);
			reset_node(node_);
			reset_node(other.node_);
			swap_node_storage(node_, right_copy.node_);
			swap_node_storage(other.node_, left_copy.node_);
		}
	}
	friend void swap(basic_json &left, basic_json &right)
	{
		left.swap(right);
	}

	[[nodiscard("discarding a released JSON node leaks its owned subtree")]]
	constexpr operator node_type() && noexcept(::std::is_nothrow_move_constructible_v<node_type>)
	{
		auto result{::std::move(node_)};
		node_.stor.template emplace<::std::monostate>();
		return result;
	}

private:
	struct object_pair
	{
		key_string_type key;
		basic_json value;
	};

	template <typename...>
	struct type_list
	{};

	template <typename First, typename Second, typename... Rest>
	static consteval ::std::size_t object_pair_count_after_key(type_list<First, Second, Rest...>) noexcept
	{
		return 1u + object_pair_count(type_list<Rest...>{});
	}

	template <typename First, typename... Rest>
	static consteval ::std::size_t object_pair_count(type_list<First, Rest...>) noexcept
	{
		if constexpr (::std::same_as<map_node_type, ::std::remove_cvref_t<First>>)
		{
			return 1u + object_pair_count(type_list<Rest...>{});
		}
		else
		{
			static_assert(sizeof...(Rest) != 0u, "a JSON object key must be followed by a value");
			return object_pair_count_after_key(type_list<First, Rest...>{});
		}
	}

	static consteval ::std::size_t object_pair_count(type_list<>) noexcept
	{
		return 0u;
	}

public:
	template <::std::size_t Size>
	struct object
	{
		::std::array<object_pair, Size> values{};

	private:
		template <::std::size_t Index>
		constexpr void initialize() noexcept
		{
			static_assert(Index == Size);
		}

		template <::std::size_t Index, typename First, typename... Rest>
		constexpr void initialize(First &&first, Rest &&...rest)
		{
			if constexpr (::std::same_as<map_node_type, ::std::remove_cvref_t<First>>)
			{
				values[Index].key = first.first;
				values[Index].value = basic_json(first.second);
				initialize<Index + 1u>(::std::forward<Rest>(rest)...);
			}
			else
			{
				initialize_key<Index>(::std::forward<First>(first), ::std::forward<Rest>(rest)...);
			}
		}

		template <::std::size_t Index, typename Key, typename Value, typename... Rest>
		constexpr void initialize_key(Key &&key, Value &&value, Rest &&...rest)
		{
			values[Index].key = key_string_type(::std::forward<Key>(key));
			values[Index].value = basic_json(::std::forward<Value>(value));
			initialize<Index + 1u>(::std::forward<Rest>(rest)...);
		}

	public:
		template <typename... Values>
			requires(object_pair_count(type_list<Values...>{}) == Size)
		constexpr explicit object(Values &&...input)
		{
			initialize<0u>(::std::forward<Values>(input)...);
		}

		constexpr operator basic_json() &&
		{
			object_type result;
			for (auto &entry : values)
			{
				result.emplace(::std::move(entry.key), static_cast<node_type>(::std::move(entry.value)));
			}
			return basic_json(::std::move(result));
		}
	};

	template <typename... Values>
	object(Values &&...) -> object<object_pair_count(type_list<Values...>{})>;

	template <::std::size_t Size>
	struct array
	{
		::std::array<basic_json, Size> values;

		template <typename... Values>
			requires(sizeof...(Values) == Size)
		constexpr explicit array(Values &&...input)
			: values{{basic_json(::std::forward<Values>(input))...}}
		{}

		constexpr operator basic_json() &&
		{
			array_type result;
			if constexpr (requires { result.reserve(Size); })
			{
				result.reserve(Size);
			}
			for (auto &entry : values)
			{
				result.push_back(static_cast<node_type>(::std::move(entry)));
			}
			return basic_json(::std::move(result));
		}
	};

	template <typename... Values>
	array(Values &&...) -> array<sizeof...(Values)>;
};

namespace detail
{

struct basic_json_access
{
	template <typename Node>
	[[nodiscard]] static constexpr Node &node(basic_json<Node> &value) noexcept
	{
		return value.node_;
	}

	template <typename Node>
	[[nodiscard]] static constexpr Node const &node(basic_json<Node> const &value) noexcept
	{
		return value.node_;
	}

	template <typename Node>
	[[nodiscard]] static constexpr auto const &allocator(Node const &value) noexcept
	{
		return basic_json<Node>::allocator_of(value);
	}

	template <typename Node>
	[[nodiscard]] static Node make_child(Node const &parent)
	{
		return basic_json<Node>::make_child_node(parent);
	}

	template <typename Node>
	[[nodiscard]] static constexpr json_kind kind(Node const &value) noexcept
	{
		return basic_json<Node>::kind_of_node(value);
	}

	template <typename Node>
	static void destroy(Node &value) noexcept
	{
		basic_json<Node>::reset_node(value);
	}

	template <typename Node>
	static void reset(Node &value) noexcept
	{
		basic_json<Node>::reset_node(value);
	}

	template <typename Node>
	static void set_null(Node &value)
	{
		basic_json<Node>::set_null_node(value);
	}

	template <typename Node>
	static void set_boolean(Node &value, bool input)
	{
		basic_json<Node>::set_boolean_node(value, input);
	}

	template <typename Node>
	static void set_number(Node &value, typename basic_json<Node>::number_type input)
	{
		basic_json<Node>::set_number_node(value, ::std::move(input));
	}

	template <typename Node>
	static void set_integer(Node &value, typename basic_json<Node>::integer_type input)
		requires(basic_json<Node>::has_integer)
	{
		basic_json<Node>::set_integer_node(value, ::std::move(input));
	}

	template <typename Node>
	static void set_uinteger(Node &value, typename basic_json<Node>::uinteger_type input)
		requires(basic_json<Node>::has_uinteger)
	{
		basic_json<Node>::set_uinteger_node(value, ::std::move(input));
	}

	template <typename Node, typename... Args>
	[[nodiscard]] static auto &emplace_string(Node &value, Args &&...args)
	{
		return basic_json<Node>::emplace_string_node(value, ::std::forward<Args>(args)...);
	}

	template <typename Node>
	static constexpr void invalidate_string_metadata(Node &value) noexcept
	{
		basic_json<Node>::invalidate_string_metadata(value);
	}

	template <typename Node>
	static constexpr void set_validated_string_metadata(Node &value,
													 bool compact_direct) noexcept
	{
		basic_json<Node>::set_validated_string_metadata(value, compact_direct);
	}

	template <typename Node>
	[[nodiscard]] static auto &emplace_array(Node &value)
	{
		return basic_json<Node>::emplace_array_node(value);
	}

	template <typename Node>
	[[nodiscard]] static auto &emplace_object(Node &value)
	{
		return basic_json<Node>::emplace_object_node(value);
	}

	template <typename Node>
	static void clone_into(Node &destination, Node const &source,
						   typename basic_json<Node>::allocator_type const &allocator)
	{
		basic_json<Node>::clone_node_into(destination, source, allocator);
	}

	template <typename Node>
	static constexpr void swap_storage(Node &left, Node &right) noexcept(
		noexcept(basic_json<Node>::swap_node_storage(left, right)))
	{
		basic_json<Node>::swap_node_storage(left, right);
	}

	template <typename Node>
	[[nodiscard]] static Node release(basic_json<Node> &&value)
	{
		return static_cast<Node>(::std::move(value));
	}

	template <typename Node>
	static void adopt(basic_json<Node> &destination, Node &&source)
	{
		destination = basic_json<Node>(::std::move(source), destination.node_.alloc);
	}
};

} // namespace detail

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::basic_json_node(
	basic_json_node const &other)
	: alloc(::std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.alloc))
{
	detail::basic_json_access::clone_into(*this, other, alloc);
}

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::basic_json_node(
	::std::allocator_arg_t, allocator_type const &allocator,
	basic_json_node const &other)
	: alloc(allocator)
{
	detail::basic_json_access::clone_into(*this, other, alloc);
}

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::basic_json_node(
	::std::allocator_arg_t, allocator_type const &allocator,
	basic_json_node &&other)
	: alloc(allocator)
{
	if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
	{
		detail::basic_json_access::swap_storage(*this, other);
	}
	else if (alloc == other.alloc)
	{
		detail::basic_json_access::swap_storage(*this, other);
	}
	else
	{
		detail::basic_json_access::clone_into(*this, other, alloc);
		detail::basic_json_access::reset(other);
	}
}

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
auto basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::operator=(
	basic_json_node const &other) -> basic_json_node &
{
	if (this == ::std::addressof(other))
	{
		return *this;
	}
	allocator_type target_allocator{alloc};
	if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
	{
		target_allocator = other.alloc;
	}
	basic_json_node replacement{::std::allocator_arg, target_allocator};
	detail::basic_json_access::clone_into(replacement, other, target_allocator);
	detail::basic_json_access::reset(*this);
	if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
	{
		alloc = target_allocator;
	}
	detail::basic_json_access::swap_storage(*this, replacement);
	return *this;
}

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
auto basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::operator=(
	basic_json_node &&other)
	-> basic_json_node &
{
	if (this == ::std::addressof(other))
	{
		return *this;
	}
	if constexpr (::std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value)
	{
		detail::basic_json_access::reset(*this);
		alloc = ::std::move(other.alloc);
		detail::basic_json_access::swap_storage(*this, other);
	}
	else if constexpr (::std::allocator_traits<allocator_type>::is_always_equal::value)
	{
		detail::basic_json_access::reset(*this);
		detail::basic_json_access::swap_storage(*this, other);
	}
	else if (alloc == other.alloc)
	{
		detail::basic_json_access::reset(*this);
		detail::basic_json_access::swap_storage(*this, other);
	}
	else
	{
		basic_json_node replacement{::std::allocator_arg, alloc};
		detail::basic_json_access::clone_into(replacement, other, alloc);
		detail::basic_json_access::reset(*this);
		detail::basic_json_access::swap_storage(*this, replacement);
		detail::basic_json_access::reset(other);
	}
	return *this;
}

template <typename Char, typename Number, typename Integer, typename UInteger,
		  template <typename> class Allocator,
		  template <typename, typename> class Array>
basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>::~basic_json_node() noexcept
{
	detail::basic_json_access::reset(*this);
}

/*
The default owning tree is the editable JSON representation.  Keep the word
`mutable` in its canonical public names so code which also uses the compact
read-only DOM cannot accidentally benchmark or pass the wrong ownership
model.  The short historical aliases remain source-compatible and name the
same mutable types; new code should prefer the explicit spellings.
*/
template <typename Char = char, typename Number = double,
		  typename Integer = ::std::int_least64_t,
		  typename UInteger = ::std::uint_least64_t,
		  template <typename> class Allocator = ::std::allocator,
		  template <typename, typename> class Array = basic_json_segmented_array>
using basic_mutable_json_node =
	basic_json_node<Char, Number, Integer, UInteger, Allocator, Array>;

template <typename Node>
using basic_mutable_json = basic_json<Node>;

template <typename Node>
using basic_mutable_json_slice = basic_json_slice<Node>;

template <typename Node>
using basic_const_mutable_json_slice = basic_const_json_slice<Node>;

using mutable_json_node = basic_mutable_json_node<>;
using mutable_json = basic_mutable_json<mutable_json_node>;
using mutable_json_slice = basic_mutable_json_slice<mutable_json_node>;
using const_mutable_json_slice =
	basic_const_mutable_json_slice<mutable_json_node>;

using json_node = mutable_json_node;
using json = mutable_json;
using json_slice = mutable_json_slice;
using const_json_slice = const_mutable_json_slice;

} // namespace fast_io::json

namespace std
{

template <typename Node, typename Allocator>
struct uses_allocator<::fast_io::json::basic_json_slice<Node>, Allocator> : false_type
{};

template <typename Node, typename Allocator>
struct uses_allocator<::fast_io::json::basic_const_json_slice<Node>, Allocator> : false_type
{};

} // namespace std
