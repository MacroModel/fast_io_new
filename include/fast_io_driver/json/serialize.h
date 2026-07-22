#pragma once

#include "../../fast_io_freestanding.h"
#include "../../fast_io_dsal/vector.h"

#include "concepts.h"
#include "error.h"
#include "options.h"
#include "dom.h"
#include "escape.h"
#include "number.h"
#include "simd.h"

namespace fast_io::json
{

template <typename node_type>
struct basic_json_print_view
{
	using node_type_alias = node_type;
	::fast_io::json::basic_const_json_slice<node_type> reference{};
	::fast_io::json::json_serialize_options options{};
};

/*
`basic_json_print_view` is the character-neutral semantic alias.  The ordinary
fast_io normalization sequence first applies `print_alias_define`, then calls
`status_io_print_forward` with the destination character type.  Keeping that
second step in the type graph is important: a same-code-unit serialization may
copy validated safe runs, whereas a cross-code-unit serialization must decode
Unicode scalars and encode them in the destination domain.

The boolean is deliberately part of the concrete type rather than a run-time
flag.  It gives dispatch and tests an exact proof of which domain relation was
selected without duplicating the serializer implementation for two wrapper
classes.  The static assertion prevents callers from manufacturing a wrapper
whose advertised relation disagrees with its source and destination types.
*/
template <::std::integral output_char_type, typename node_type, bool transcoding>
struct basic_json_io_print_view : basic_json_print_view<node_type>
{
	using base_type = basic_json_print_view<node_type>;
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	using source_char_type = typename slice_type::char_type;
	using key_source_char_type = typename slice_type::key_char_type;
	using output_char_type_alias = output_char_type;
	static inline constexpr bool is_transcoding{transcoding};

	static_assert(transcoding ==
				  !(::std::same_as<output_char_type, source_char_type> &&
					::std::same_as<output_char_type, key_source_char_type>));

	inline constexpr basic_json_io_print_view() noexcept = default;

	inline constexpr explicit basic_json_io_print_view(base_type value) noexcept
		: base_type(value)
	{
	}
};

template <::std::integral char_type, typename node_type>
using basic_json_same_domain_print_view = basic_json_io_print_view<char_type, node_type, false>;

template <::std::integral char_type, typename node_type>
using basic_json_transcode_print_view = basic_json_io_print_view<char_type, node_type, true>;

template <::std::integral char_type, typename node_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, basic_json_print_view<node_type> value) noexcept
{
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	using source_char_type = typename slice_type::char_type;
	using key_source_char_type = typename slice_type::key_char_type;
	return basic_json_io_print_view<
		char_type, node_type,
		!(::std::same_as<char_type, source_char_type> &&
		  ::std::same_as<char_type, key_source_char_type>)>{value};
}

struct json_boolean_scalar
{
	bool value{};
};

template <typename value_type>
struct basic_json_integer_scalar
{
	value_type const *reference{};
};

template <typename value_type>
struct basic_json_floating_scalar
{
	value_type const *reference{};
};

template <typename value_type>
struct basic_json_custom_floating_scalar
{
	value_type const *reference{};
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, json_boolean_scalar value) noexcept
{
	return ::fast_io::mnp::boolalpha(value.value);
}

template <::std::integral char_type, typename value_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, basic_json_integer_scalar<value_type> value) noexcept(noexcept(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*value.reference))))
{
	return ::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(*value.reference));
}

template <::std::integral char_type, typename value_type>
	requires(::fast_io::details::my_floating_point<value_type>)
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, basic_json_floating_scalar<value_type> value) noexcept
{
	return ::fast_io::mnp::json_float(
		::fast_io::mnp::decimal(*value.reference));
}

template <::std::integral char_type, typename value_type>
[[nodiscard]] inline constexpr auto status_io_print_forward(
	::fast_io::io_alias_type_t<char_type>, basic_json_custom_floating_scalar<value_type> value) noexcept(noexcept(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(*value.reference))))
{
	return ::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(*value.reference));
}

namespace details
{

template <typename>
inline constexpr bool dependent_false{};

template <typename value_type>
using json_vector = ::fast_io::containers::vector<
	value_type, ::fast_io::native_global_allocator>;

/*
The document-level dynamic and precise reserve protocols replay the immutable
DOM: one traversal computes a capacity/extent and a second traversal emits.
That is intrinsically safe for fast_io's built-in integer and floating
formatters.  A custom numeric alternative participates only after its public
replay marker proves the complete alias/status/formatter chain stable.  The
one-pass direct and context protocols do not need this restriction.
*/
template <typename node_type>
inline constexpr bool json_node_print_replay_safe = []() constexpr {
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	if constexpr (!::fast_io::json_number_print_replay_safe<typename slice_type::number_type>)
	{
		return false;
	}
	else if constexpr (slice_type::has_integer)
	{
		if constexpr (!::fast_io::json_number_print_replay_safe<typename slice_type::integer_type>)
		{
			return false;
		}
	}
	if constexpr (slice_type::has_uinteger)
	{
		if constexpr (!::fast_io::json_number_print_replay_safe<typename slice_type::uinteger_type>)
		{
			return false;
		}
	}
	return true;
}();

template <::std::integral char_type>
inline void json_validate_materialized_cursor(
	char_type *begin, ::std::size_t capacity, char_type *result) noexcept
{
	if (capacity == 0u ||
		(::std::numeric_limits<::std::uintptr_t>::max)() / sizeof(char_type) < capacity) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const begin_address{reinterpret_cast<::std::uintptr_t>(begin)};
	auto const result_address{reinterpret_cast<::std::uintptr_t>(result)};
	auto const bytes{capacity * sizeof(char_type)};
	if (result_address < begin_address || bytes < result_address - begin_address ||
		(result_address - begin_address) % sizeof(char_type) != 0u) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
}

template <::std::integral char_type, typename value_type>
inline void json_materialize_normalized(
	json_vector<char_type> &destination, value_type &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	destination.clear();

	if constexpr (::fast_io::scatter_printable_for<char_type, value_type &>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (scatter.len != 0u && scatter.base == nullptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		destination.resize(scatter.len);
		for (::std::size_t index{}; index != scatter.len; ++index)
		{
			destination[index] = scatter.base[index];
		}
	}
	else if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>)};
		destination.resize(capacity);
		auto *const begin{destination.data()};
		auto *const result{print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
		json_validate_materialized_cursor(begin, capacity, result);
		destination.resize(static_cast<::std::size_t>(result - begin));
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, value_type>)
	{
		auto const capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (capacity == 0u || static_cast<::std::size_t>(PTRDIFF_MAX) < capacity) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		destination.resize(capacity);
		auto *const begin{destination.data()};
		auto *const result{print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
		json_validate_materialized_cursor(begin, capacity, result);
		destination.resize(static_cast<::std::size_t>(result - begin));
	}
	else if constexpr (::fast_io::context_printable<char_type, value_type>)
	{
		using context_type = typename decltype(print_context_type(
			::fast_io::io_reserve_type<char_type, clean_type>))::type;
		::fast_io::details::with_print_context_state<context_type>(
			[&](context_type &context) {
				char_type buffer[256u];
				for (;;)
				{
					auto const result{context.print_context_define(
						value, buffer, buffer + 256u)};
					::fast_io::details::decay::validate_context_print_result(
						buffer, buffer + 256u, result.iter, result.done);
					auto const old_size{destination.size()};
					auto const produced{static_cast<::std::size_t>(result.iter - buffer)};
					if ((::std::numeric_limits<::std::size_t>::max)() - old_size < produced) [[unlikely]]
					{
						::fast_io::fast_terminate();
					}
					destination.resize(old_size + produced);
					for (::std::size_t index{}; index != produced; ++index)
					{
						destination[old_size + index] = buffer[index];
					}
					if (result.done)
					{
						break;
					}
				}
			});
	}
	else if constexpr (::fast_io::alias_printable<value_type &>)
	{
		/*
		A custom JSON-number status CPO is allowed to return an ordinary public
		string/view object.  A status result is normally the final print leaf, but
		those interoperability objects acquire their scatter through the ordinary
		alias CPO.  Apply that one remaining public-to-leaf normalization here;
		the strict type-change assertion prevents an accidental alias cycle.
		*/
		auto next{::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(value))};
		using next_type = ::std::remove_cvref_t<decltype(next)>;
		static_assert(!::std::same_as<clean_type, next_type>,
					  "a custom JSON number alias must make progress toward a printable leaf");
		json_materialize_normalized(destination, next);
		return;
	}
	else
	{
		static_assert(dependent_false<clean_type>,
					  "a JSON number must normalize to a scatter, reserve, dynamic-reserve, or context printable leaf");
	}

	if (destination.empty()) [[unlikely]]
	{
		// Every JSON scalar token has at least one code unit.
		::fast_io::fast_terminate();
	}
}

template <::std::integral char_type, typename scalar_type>
inline void json_materialize_scalar(
	json_vector<char_type> &destination, scalar_type scalar)
{
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(scalar))};
	json_materialize_normalized(destination, normalized);
}

template <::std::integral char_type>
inline void json_validate_custom_number_token(
	json_vector<char_type> const &token, bool require_integer,
	bool require_nonnegative)
{
	auto const *const first{token.data()};
	auto const *const last{first + token.size()};
	auto const result{::fast_io::json::details::scan_json_number(first, last)};
	if (result.code != ::fast_io::parse_code::ok || result.iter != last ||
		result.token.last != last ||
		(require_integer &&
		 result.token.kind != ::fast_io::json::json_number_token_kind::integer) ||
		(require_nonnegative && result.token.negative)) [[unlikely]]
	{
		::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_number);
	}
}

template <::std::integral char_type>
inline void json_validate_custom_number_token(
	char_type const *first, ::std::size_t size, bool require_integer,
	bool require_nonnegative)
{
	char_type empty_sentinel{};
	if (size == 0u)
	{
		first = __builtin_addressof(empty_sentinel);
	}
	auto const *const last{first + size};
	auto const result{::fast_io::json::details::scan_json_number(first, last)};
	if (result.code != ::fast_io::parse_code::ok || result.iter != last ||
		result.token.last != last ||
		(require_integer &&
		 result.token.kind != ::fast_io::json::json_number_token_kind::integer) ||
		(require_nonnegative && result.token.negative)) [[unlikely]]
	{
		::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_number);
	}
}

/*
Materialize one already-normalized scalar and immediately lend its complete
token to `consumer`.  Fixed-reserve leaves use bounded automatic storage, so
the normal bool/integer/floating path performs no allocation.  A single caller-
owned vector is used only for an unusually large static bound, a dynamic bound,
or an incremental custom numeric formatter and is reused across every scalar
in the enclosing JSON traversal.

The reserve CPO's returned cursor is checked before it is observed as a length.
For context producers core's cursor/progress validator proves that every
non-final step advances.  Therefore the consumer receives exactly one finite,
contiguous token and never an unchecked producer cursor.
*/
template <::std::integral char_type, typename value_type, typename consumer_type>
inline void json_consume_normalized_scalar(
	json_vector<char_type> &dynamic_storage, value_type &value,
	consumer_type &consumer)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::scatter_printable_for<char_type, value_type &>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (scatter.len != 0u && scatter.base == nullptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		char_type empty_sentinel{};
		consumer(scatter.len == 0u ? __builtin_addressof(empty_sentinel) : scatter.base,
				 scatter.len);
	}
	else if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>)};
		constexpr ::std::size_t automatic_byte_limit{4096u};
		if constexpr (capacity <= automatic_byte_limit / sizeof(char_type))
		{
			char_type buffer[capacity];
			auto *const result{print_reserve_define(
				::fast_io::io_reserve_type<char_type, clean_type>, buffer, value)};
			json_validate_materialized_cursor(buffer, capacity, result);
			consumer(buffer, static_cast<::std::size_t>(result - buffer));
		}
		else
		{
			dynamic_storage.resize(capacity);
			auto *const begin{dynamic_storage.data()};
			auto *const result{print_reserve_define(
				::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
			json_validate_materialized_cursor(begin, capacity, result);
			consumer(begin, static_cast<::std::size_t>(result - begin));
		}
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, value_type>)
	{
		auto const capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (static_cast<::std::size_t>(PTRDIFF_MAX) < capacity) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if (capacity == 0u)
		{
			char_type empty_sentinel{};
			consumer(__builtin_addressof(empty_sentinel), 0u);
			return;
		}
		dynamic_storage.resize(capacity);
		auto *const begin{dynamic_storage.data()};
		auto *const result{print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
		json_validate_materialized_cursor(begin, capacity, result);
		consumer(begin, static_cast<::std::size_t>(result - begin));
	}
	else if constexpr (::fast_io::context_printable<char_type, value_type>)
	{
		using context_type = typename decltype(print_context_type(
			::fast_io::io_reserve_type<char_type, clean_type>))::type;
		dynamic_storage.clear();
		::fast_io::details::with_print_context_state<context_type>(
			[&](context_type &context) {
				char_type buffer[256u];
				for (;;)
				{
					auto const result{context.print_context_define(
						value, buffer, buffer + 256u)};
					::fast_io::details::decay::validate_context_print_result(
						buffer, buffer + 256u, result.iter, result.done);
					auto const old_size{dynamic_storage.size()};
					auto const produced{static_cast<::std::size_t>(result.iter - buffer)};
					if ((::std::numeric_limits<::std::size_t>::max)() - old_size < produced)
						[[unlikely]]
					{
						::fast_io::fast_terminate();
					}
					dynamic_storage.resize(old_size + produced);
					for (::std::size_t index{}; index != produced; ++index)
					{
						dynamic_storage[old_size + index] = buffer[index];
					}
					if (result.done)
					{
						break;
					}
				}
			});
		char_type empty_sentinel{};
		consumer(dynamic_storage.empty() ? __builtin_addressof(empty_sentinel) : dynamic_storage.data(),
				 dynamic_storage.size());
	}
	else if constexpr (::fast_io::alias_printable<value_type &>)
	{
		auto next{::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(value))};
		using next_type = ::std::remove_cvref_t<decltype(next)>;
		static_assert(!::std::same_as<clean_type, next_type>,
					  "a custom JSON number alias must make progress toward a printable leaf");
		json_consume_normalized_scalar(dynamic_storage, next, consumer);
	}
	else
	{
		static_assert(dependent_false<clean_type>,
					  "a JSON number must normalize to a scatter, reserve, dynamic-reserve, or context printable leaf");
	}
}

template <::std::integral char_type, typename scalar_type, typename consumer_type>
inline void json_consume_scalar(
	json_vector<char_type> &dynamic_storage, scalar_type scalar,
	consumer_type &consumer)
{
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(scalar))};
	json_consume_normalized_scalar(dynamic_storage, normalized, consumer);
}

template <bool trusted_builtin, typename sink_type,
		  ::std::integral char_type, typename value_type>
[[nodiscard]] inline bool json_try_append_normalized_reserve(
	sink_type &sink, value_type &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		return sink.template try_append_reserve<trusted_builtin>(value);
	}
	else if constexpr (::fast_io::alias_printable<value_type &>)
	{
		auto next{::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(value))};
		using next_type = ::std::remove_cvref_t<decltype(next)>;
		static_assert(!::std::same_as<clean_type, next_type>,
					  "a custom JSON number alias must make progress toward a printable leaf");
		return json_try_append_normalized_reserve<
			trusted_builtin, sink_type, char_type>(sink, next);
	}
	else
	{
		return false;
	}
}

template <typename scalar_type>
inline constexpr bool json_trusted_builtin_scalar_reserve{false};

template <>
inline constexpr bool json_trusted_builtin_scalar_reserve<json_boolean_scalar>{true};

template <typename value_type>
inline constexpr bool json_trusted_builtin_scalar_reserve<
	basic_json_integer_scalar<value_type>>{
	::fast_io::details::my_integral<value_type>};

template <typename value_type>
inline constexpr bool json_trusted_builtin_scalar_reserve<
	basic_json_floating_scalar<value_type>>{
	::fast_io::details::my_floating_point<value_type>};

template <::std::integral char_type, typename sink_type, typename scalar_type>
[[nodiscard]] inline bool json_try_append_scalar_reserve(
	sink_type &sink, scalar_type scalar)
{
	using clean_scalar_type = ::std::remove_cvref_t<scalar_type>;
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(scalar))};
	return json_try_append_normalized_reserve<
		json_trusted_builtin_scalar_reserve<clean_scalar_type>,
		sink_type, char_type>(sink, normalized);
}

template <::std::integral char_type, typename value_type>
[[nodiscard]] inline ::std::size_t json_measure_normalized_scalar(
	json_vector<char_type> &dynamic_storage, value_type &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::scatter_printable_for<char_type, value_type &>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (scatter.len != 0u && scatter.base == nullptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return scatter.len;
	}
	else if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
	{
		return print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, clean_type>, value);
	}
	else if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		return print_reserve_static_precise_size(
			::fast_io::io_reserve_type<char_type, clean_type>);
	}
	else if constexpr (::fast_io::alias_printable<value_type &>)
	{
		auto next{::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(value))};
		using next_type = ::std::remove_cvref_t<decltype(next)>;
		static_assert(!::std::same_as<clean_type, next_type>,
					  "a custom JSON number alias must make progress toward a printable leaf");
		return json_measure_normalized_scalar(dynamic_storage, next);
	}
	else
	{
		::std::size_t result{};
		auto consumer{[&result](char_type const *, ::std::size_t size) noexcept {
			result = size;
		}};
		json_consume_normalized_scalar(dynamic_storage, value, consumer);
		return result;
	}
}

template <::std::integral char_type, typename scalar_type>
[[nodiscard]] inline ::std::size_t json_measure_scalar(
	json_vector<char_type> &dynamic_storage, scalar_type scalar)
{
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(scalar))};
	return json_measure_normalized_scalar(dynamic_storage, normalized);
}

/*
Return a valid reserve bound without formatting a fixed-reserve leaf.  A
static reserve CPO promises that its writer returns inside exactly that
capacity; scatter length and dynamic reserve size carry the analogous run-time
proofs.  Only a context-only custom number has no independent bound, so that
rare extension is consumed once into the caller's reusable storage.  This is
the whole distinction between the cheap dynamic-reserve pass and the exact
precise-reserve pass below: built-in integers/floats are never converted while
the upper bound is accumulated.
*/
template <::std::integral char_type, typename value_type>
[[nodiscard]] inline ::std::size_t json_bound_normalized_scalar(
	json_vector<char_type> &dynamic_storage, value_type &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::scatter_printable_for<char_type, value_type &>)
	{
		auto const scatter{print_scatter_define(
			::fast_io::io_reserve_type<char_type, clean_type>, value)};
		if (scatter.len != 0u && scatter.base == nullptr) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return scatter.len;
	}
	else if constexpr (::fast_io::reserve_printable<char_type, value_type>)
	{
		return print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>);
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, value_type>)
	{
		return print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>, value);
	}
	else if constexpr (::fast_io::alias_printable<value_type &>)
	{
		auto next{::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(value))};
		using next_type = ::std::remove_cvref_t<decltype(next)>;
		static_assert(!::std::same_as<clean_type, next_type>,
					  "a custom JSON number alias must make progress toward a printable leaf");
		return json_bound_normalized_scalar(dynamic_storage, next);
	}
	else
	{
		::std::size_t result{};
		auto consumer{[&result](char_type const *, ::std::size_t size) noexcept {
			result = size;
		}};
		json_consume_normalized_scalar(dynamic_storage, value, consumer);
		return result;
	}
}

template <::std::integral char_type, typename scalar_type>
[[nodiscard]] inline ::std::size_t json_bound_scalar(
	json_vector<char_type> &dynamic_storage, scalar_type scalar)
{
	auto normalized{::fast_io::io_print_forward<char_type>(
		::fast_io::io_print_alias(scalar))};
	return json_bound_normalized_scalar(dynamic_storage, normalized);
}

template <::std::integral char_type, typename value_type>
inline void json_check_finite(value_type const &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
		using trait = ::fast_io::details::iec559_traits<clean_type>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<clean_type>(value)};
		if (fields.exponent == exponent_mask) [[unlikely]]
		{
			::fast_io::json::throw_json_error(fields.mantissa == 0u ? ::fast_io::json::json_errc::number_inf : ::fast_io::json::json_errc::number_nan);
		}
	}
	else
	{
		static_assert(::fast_io::json_finite_number<clean_type>);
		if (!json_number_is_finite_define(value)) [[unlikely]]
		{
			// The custom finite CPO intentionally reports one bit only; NaN is
			// the conservative diagnostic when it cannot distinguish infinity.
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::number_nan);
		}
	}
}

template <::std::integral char_type>
class basic_json_size_sink
{
	::std::size_t size_{};

public:
	using output_char_type = char_type;
	inline static constexpr bool measures_only{true};
	inline static constexpr bool upper_bounds_only{false};

	inline void append(char_type const *, ::std::size_t count) noexcept
	{
		constexpr auto maximum{static_cast<::std::size_t>(PTRDIFF_MAX)};
		if (maximum - size_ < count) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		size_ += count;
	}

	inline void append_one(char_type) noexcept
	{
		append(nullptr, 1u);
	}

	inline void append_repeat(char_type, ::std::size_t count) noexcept
	{
		append(nullptr, count);
	}

	[[nodiscard]] inline constexpr ::std::size_t size() const noexcept
	{
		return size_;
	}
};

template <::std::integral char_type>
class basic_json_bound_sink
{
	::std::size_t size_{};

public:
	using output_char_type = char_type;
	inline static constexpr bool measures_only{true};
	inline static constexpr bool upper_bounds_only{true};

	inline void append(char_type const *, ::std::size_t count) noexcept
	{
		constexpr auto maximum{static_cast<::std::size_t>(PTRDIFF_MAX)};
		if (maximum - size_ < count) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		size_ += count;
	}

	inline void append_one(char_type) noexcept
	{
		append(nullptr, 1u);
	}

	inline void append_repeat(char_type, ::std::size_t count) noexcept
	{
		append(nullptr, count);
	}

	[[nodiscard]] inline constexpr ::std::size_t size() const noexcept
	{
		return size_;
	}
};

template <::std::integral char_type, bool bounded = false>
class basic_json_contiguous_sink
{
	char_type *current_{};
	char_type *end_{};

	inline void require(::std::size_t count) const noexcept
	{
		if constexpr (bounded)
		{
			if (static_cast<::std::size_t>(end_ - current_) < count) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
	}

public:
	using output_char_type = char_type;
	inline static constexpr bool measures_only{false};
	inline static constexpr bool upper_bounds_only{false};

	inline constexpr explicit basic_json_contiguous_sink(char_type *begin) noexcept
		requires(!bounded)
		: current_(begin)
	{
	}

	inline constexpr basic_json_contiguous_sink(char_type *begin, char_type *end) noexcept
		requires(bounded)
		: current_(begin), end_(end)
	{
	}

	inline void append(char_type const *source, ::std::size_t count) noexcept
	{
		require(count);
		for (::std::size_t index{}; index != count; ++index)
		{
			current_[index] = source[index];
		}
		current_ += count;
	}

	inline void append_one(char_type value) noexcept
	{
		require(1u);
		*current_++ = value;
	}

	inline void append_repeat(char_type value, ::std::size_t count) noexcept
	{
		require(count);
		for (::std::size_t index{}; index != count; ++index)
		{
			current_[index] = value;
		}
		current_ += count;
	}

	template <bool trusted_builtin, typename value_type>
	[[nodiscard]] inline bool try_append_reserve(value_type &value) noexcept
	{
		using clean_type = ::std::remove_cvref_t<value_type>;
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>)};
		if constexpr (bounded)
		{
			if (static_cast<::std::size_t>(end_ - current_) < capacity)
			{
				return false;
			}
		}
		auto *const begin{current_};
		auto *const result{print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
		/* A fast_io built-in formatter is part of the reserve protocol's
		trusted implementation and cannot escape its compile-time extent.  A
		bounded target and every user-extensible formatter retain the defensive
		cursor validation. */
		if constexpr (bounded || !trusted_builtin)
		{
			json_validate_materialized_cursor(begin, capacity, result);
			if (result == begin) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
		}
		current_ = result;
		return true;
	}

	[[nodiscard]] inline constexpr char_type *current() const noexcept
	{
		return current_;
	}
};

template <::std::integral char_type, typename output_type,
		  bool has_put_area = ::fast_io::operations::decay::defines::has_obuffer_basic_operations<output_type>>
class basic_json_output_sink;

/* Unbuffered outputs receive one bounded coalescing window. */
template <::std::integral char_type, typename output_type>
class basic_json_output_sink<char_type, output_type, false>
{
	static inline constexpr ::std::size_t preferred_bytes{4096u};
	static inline constexpr ::std::size_t buffer_size{
		preferred_bytes / sizeof(char_type) == 0u ? 1u : preferred_bytes / sizeof(char_type)};
	output_type *output_{};
	char_type buffer_[buffer_size]{};
	::std::size_t used_{};

public:
	using output_char_type = char_type;
	inline static constexpr bool measures_only{false};
	inline static constexpr bool upper_bounds_only{false};

	inline constexpr explicit basic_json_output_sink(output_type &output) noexcept
		: output_(__builtin_addressof(output))
	{
	}

	inline void flush()
	{
		if (used_ != 0u)
		{
			auto const count{used_};
			/*
			Commit the staging state before entering the user-extensible write
			operation.  If it throws, the enclosing serializer may flush a later
			staged prefix, but it must never replay the range whose write was
			already attempted (the sink cannot know whether that operation made
			partial external progress).
			*/
			used_ = 0u;
			::fast_io::operations::decay::write_all_decay(
				*output_, buffer_, buffer_ + count);
		}
	}

	inline void append(char_type const *source, ::std::size_t count)
	{
		while (count != 0u)
		{
			if (used_ == 0u && buffer_size <= count)
			{
				::fast_io::operations::decay::write_all_decay(
					*output_, source, source + count);
				return;
			}
			auto const available{buffer_size - used_};
			auto const copied{count < available ? count : available};
			for (::std::size_t index{}; index != copied; ++index)
			{
				buffer_[used_ + index] = source[index];
			}
			used_ += copied;
			source += copied;
			count -= copied;
			if (used_ == buffer_size)
			{
				flush();
			}
		}
	}

	inline void append_one(char_type value)
	{
		if (used_ == buffer_size)
		{
			flush();
		}
		buffer_[used_++] = value;
	}

	inline void append_repeat(char_type value, ::std::size_t count)
	{
		while (count != 0u)
		{
			if (used_ == buffer_size)
			{
				flush();
			}
			auto const available{buffer_size - used_};
			auto const copied{count < available ? count : available};
			for (::std::size_t index{}; index != copied; ++index)
			{
				buffer_[used_ + index] = value;
			}
			used_ += copied;
			count -= copied;
		}
	}

	template <bool, typename value_type>
	[[nodiscard]] inline constexpr bool try_append_reserve(value_type &) noexcept
	{
		return false;
	}
};

/*
A true fast_io put area already is the coalescing window.  Retaining its cursor
inside the JSON walk removes an otherwise redundant 4 KiB staging copy while
committing only at overflow, successful completion, or exceptional exit.  No
pointer survives an overflow operation: commit invalidates the cached pair
before calling user-extensible output, and the next append reacquires the
possibly relocated range.
*/
template <::std::integral char_type, typename output_type>
class basic_json_output_sink<char_type, output_type, true>
{
	output_type *output_{};
	char_type *current_{};
	char_type *end_{};

#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
	[[msvc::noinline]]
#endif
	void append_slow(char_type const *source, ::std::size_t count)
	{
		ensure_acquired();
		auto const capacity{available()};
		auto const copied{count < capacity ? count : capacity};
		if (copied != 0u)
		{
			for (::std::size_t index{}; index != copied; ++index)
			{
				current_[index] = source[index];
			}
			current_ += copied;
			source += copied;
			count -= copied;
		}
		if (count != 0u)
		{
			commit();
			/* `source` names DOM/scalar storage, never this destination. */
			::fast_io::operations::decay::write_all_decay(
				*output_, source, source + count);
		}
	}

#if __has_cpp_attribute(__gnu__::__noinline__)
	[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
	[[msvc::noinline]]
#endif
	void append_one_slow(char_type value)
	{
		ensure_acquired();
		if (current_ != end_)
		{
			*current_++ = value;
			return;
		}
		commit();
		::fast_io::operations::decay::char_put_decay(*output_, value);
	}

	inline void acquire()
	{
		current_ = obuffer_curr(*output_);
		end_ = obuffer_end(*output_);
		if ((current_ == nullptr) != (end_ == nullptr)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if (current_ != nullptr && end_ < current_) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}

	[[nodiscard]] inline ::std::size_t available() const noexcept
	{
		if (current_ == nullptr)
		{
			return 0u;
		}
		auto const difference{end_ - current_};
		if (difference < 0) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return static_cast<::std::size_t>(difference);
	}

	inline void ensure_acquired()
	{
		if (current_ == nullptr)
		{
			acquire();
		}
	}

	inline void commit()
	{
		if (current_ != nullptr)
		{
			auto *const committed{current_};
			current_ = nullptr;
			end_ = nullptr;
			obuffer_set_curr(*output_, committed);
		}
	}

public:
	using output_char_type = char_type;
	inline static constexpr bool measures_only{false};
	inline static constexpr bool upper_bounds_only{false};

	inline explicit basic_json_output_sink(output_type &output)
		: output_(__builtin_addressof(output))
	{
		acquire();
	}

	inline void flush()
	{
		commit();
	}

	inline void append(char_type const *source, ::std::size_t count)
	{
		if (count == 0u)
		{
			return;
		}
		if (current_ != nullptr)
		{
			auto const capacity{
				static_cast<::std::size_t>(end_ - current_)};
			if (count <= capacity) [[likely]]
			{
				for (::std::size_t index{}; index != count; ++index)
				{
					current_[index] = source[index];
				}
				current_ += count;
				return;
			}
		}
		append_slow(source, count);
	}

	inline void append_one(char_type value)
	{
		if (current_ != end_) [[likely]]
		{
			*current_++ = value;
			return;
		}
		append_one_slow(value);
	}

	inline void append_repeat(char_type value, ::std::size_t count)
	{
		while (count != 0u)
		{
			ensure_acquired();
			auto const capacity{available()};
			if (capacity == 0u)
			{
				commit();
				::fast_io::operations::decay::char_put_decay(*output_, value);
				--count;
				continue;
			}
			auto const copied{count < capacity ? count : capacity};
			for (::std::size_t index{}; index != copied; ++index)
			{
				current_[index] = value;
			}
			current_ += copied;
			count -= copied;
		}
	}

	template <bool, typename value_type>
	[[nodiscard]] inline bool try_append_reserve(value_type &value)
	{
		using clean_type = ::std::remove_cvref_t<value_type>;
		constexpr ::std::size_t capacity{print_reserve_size(
			::fast_io::io_reserve_type<char_type, clean_type>)};
		ensure_acquired();
		if (available() < capacity)
		{
			return false;
		}
		auto *const begin{current_};
		auto *const result{print_reserve_define(
			::fast_io::io_reserve_type<char_type, clean_type>, begin, value)};
		json_validate_materialized_cursor(begin, capacity, result);
		if (result == begin) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		current_ = result;
		return true;
	}
};

template <char8_t... characters, typename sink_type>
inline void json_sink_literal(sink_type &sink)
{
	using char_type = typename sink_type::output_char_type;
	(sink.append_one(::fast_io::char_literal_v<characters, char_type>), ...);
}

template <typename sink_type, ::std::integral source_char_type>
inline void json_sink_ascii_source(
	sink_type &sink, source_char_type const *source, ::std::size_t count)
{
	using char_type = typename sink_type::output_char_type;
	if constexpr (::std::same_as<::std::remove_cv_t<source_char_type>,
								 ::std::remove_cv_t<char_type>>)
	{
		sink.append(source, count);
	}
	else
	{
		for (::std::size_t index{}; index != count; ++index)
		{
			sink.append_one(static_cast<char_type>(json_code_unit(source[index])));
		}
	}
}

template <bool unicode_validated = false, ::std::integral source_char_type>
[[nodiscard]] inline constexpr bool json_ascii_requires_escape_or_validation(
	source_char_type value, bool escape_solidus) noexcept
{
	auto const unit{json_code_unit(value)};
	return unit < 0x20u || (!unicode_validated && 0x7fu < unit) ||
		   unit == 0x22u || unit == 0x5cu ||
		   (escape_solidus && unit == 0x2fu);
}

template <bool escape_solidus, bool unicode_validated, ::std::size_t width,
		  ::std::integral source_char_type>
	requires(sizeof(source_char_type) == 1u && width != 0u && width <= 64u)
[[nodiscard]] inline ::std::size_t json_find_plain_ascii_simd_blocks(
	source_char_type const *data, ::std::size_t size,
	::std::size_t position) noexcept
{
	if (size - position < width)
	{
		return position;
	}
	using simd_type = ::fast_io::intrinsics::simd_vector<
		::std::uint_least8_t, width>;
	auto const space{json_simd_splat<width>(0x20u)};
	auto const high_bit{json_simd_splat<width>(0x80u)};
	auto const quote{json_simd_splat<width>(0x22u)};
	auto const reverse_solidus{json_simd_splat<width>(0x5cu)};
	auto const solidus{json_simd_splat<width>(0x2fu)};

	for (; size - position >= width; position += width)
	{
		simd_type bytes{};
		bytes.load(static_cast<void const *>(data + position));
		auto candidates{(bytes < space) | (bytes == quote) |
						(bytes == reverse_solidus)};
		if constexpr (!unicode_validated)
		{
			candidates = candidates | (bytes >= high_bit);
		}
		if constexpr (escape_solidus)
		{
			candidates = candidates | (bytes == solidus);
		}
		if constexpr (::fast_io::intrinsics::can_intrinsics_accelerate_mask_to_bitset<width>)
		{
			auto const candidate_bits{
				::fast_io::intrinsics::vector_mask_to_bitset(candidates)};
			if (candidate_bits == 0u)
			{
				continue;
			}
			/* Bit i denotes candidate lane i, so countr_zero of this known-
			   nonzero bitmap is exactly the first byte requiring slow handling. */
			return position + static_cast<::std::size_t>(
				::std::countr_zero(candidate_bits));
		}
		else
		{
			if (::fast_io::intrinsics::is_all_zeros(candidates))
			{
				continue;
			}
			::std::uint_least8_t lanes[width]{};
			candidates.store(lanes);
			for (::std::size_t index{}; index != width; ++index)
			{
				if (lanes[index] != 0u)
				{
					return position + index;
				}
			}
			::fast_io::fast_terminate();
		}
	}
	return position;
}

template <bool escape_solidus, bool unicode_validated,
		  ::std::integral source_char_type>
/* Keep vector setup and its architecture-specific constants outside the
   quoted-string state machine.  The public dispatcher below inlines the
   sub-vector scalar case and calls this body only when at least 16 bytes are
   available, amortizing the call while avoiding hot-loop register pressure. */
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr ::std::size_t json_find_plain_ascii_end_impl(
	source_char_type const *data, ::std::size_t size,
	::std::size_t position) noexcept
{
	if constexpr (sizeof(source_char_type) == 1u)
	{
		if (!__builtin_is_constant_evaluated())
		{
			constexpr ::std::size_t width{
				::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size};
			if constexpr (width != 0u && width <= 64u)
			{
				position = json_find_plain_ascii_simd_blocks<
					escape_solidus, unicode_validated, width>(
					data, size, position);
				if (position == size ||
					json_ascii_requires_escape_or_validation<unicode_validated>(
						data[position], escape_solidus))
				{
					return position;
				}
			}

			/*
			AVX2 makes 32 bytes the preferred bulk width, but a 16-byte SIMD
			check is still the profitable shape for the short keys and values
			which dominate object DOMs.  It is also the native NEON width.  This
			tail never reads beyond `size` and is compiled only where a 16-byte
			vector can run with the target instruction set.
			*/
			if constexpr (width != 16u &&
				::fast_io::details::calculate_can_simd_vector_run_with_cpu_instruction(16u))
			{
				position = json_find_plain_ascii_simd_blocks<
					escape_solidus, unicode_validated, 16u>(
					data, size, position);
				if (position == size ||
					json_ascii_requires_escape_or_validation<unicode_validated>(
						data[position], escape_solidus))
				{
					return position;
				}
			}
		}
	}

	for (; position != size; ++position)
	{
		if (json_ascii_requires_escape_or_validation<unicode_validated>(
				data[position], escape_solidus))
		{
			break;
		}
	}
	return position;
}

/*
Locate the end of a byte string's maximal plain-ASCII prefix.  Every SIMD lane
which compares zero is in [0x20,0x7f), is neither quote nor reverse solidus,
and (when requested) is not solidus.  Consequently copying the whole prefix is
equivalent to scalar JSON escaping.  The high-bit comparison is essential: it
routes every UTF-8 sequence through the strict decoder instead of treating an
arbitrary non-ASCII byte as validated text.
*/
template <::std::integral source_char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::std::size_t json_find_plain_ascii_end(
	source_char_type const *data, ::std::size_t size, ::std::size_t position,
	bool escape_solidus) noexcept
{
	if constexpr (sizeof(source_char_type) == 1u)
	{
		/* The scalar loop is kept in this tiny always-inline dispatcher so a
		   run shorter than one native 16-byte vector pays no SIMD call or
		   policy specialization.  Repeated short escapes depend on this path. */
		if (__builtin_is_constant_evaluated() || size - position < 16u)
		{
			for (; position != size; ++position)
			{
				if (json_ascii_requires_escape_or_validation<false>(
						data[position], escape_solidus))
				{
					break;
				}
			}
			return position;
		}
	}
	else
	{
		for (; position != size; ++position)
		{
			if (json_ascii_requires_escape_or_validation<false>(
					data[position], escape_solidus))
			{
				break;
			}
		}
		return position;
	}
	if (escape_solidus)
	{
		return json_find_plain_ascii_end_impl<true, false>(data, size, position);
	}
	return json_find_plain_ascii_end_impl<false, false>(data, size, position);
}

/* The caller owns a proof that the complete source string is valid Unicode.
   Minimal same-domain JSON therefore needs to locate only ASCII syntax and
   C0 controls; high-bit UTF code units are copied as part of the plain run. */
template <::std::integral source_char_type>
[[nodiscard]] inline constexpr ::std::size_t
json_find_plain_validated_minimal_end(source_char_type const *data,
	::std::size_t size, ::std::size_t position) noexcept
{
	if constexpr (sizeof(source_char_type) == 1u)
	{
		if (__builtin_is_constant_evaluated() || size - position < 16u)
		{
			for (; position != size; ++position)
			{
				if (json_ascii_requires_escape_or_validation<true>(
						data[position], false))
				{
					break;
				}
			}
			return position;
		}
	}
	else
	{
		for (; position != size; ++position)
		{
			if (json_ascii_requires_escape_or_validation<true>(
					data[position], false))
			{
				break;
			}
		}
		return position;
	}
	return json_find_plain_ascii_end_impl<false, true>(data, size, position);
}

template <typename sink_type, typename string_type>
inline void json_emit_validated_minimal_quoted_string(
	sink_type &sink, string_type const &string)
{
	using char_type = typename sink_type::output_char_type;
	auto const *const data{string.data()};
	auto const size{static_cast<::std::size_t>(string.size())};
	json_sink_literal<u8'"'>(sink);
	::std::size_t position{};
	while (position != size)
	{
		auto const run_begin{position};
		position = json_find_plain_validated_minimal_end(
			data, size, position);
		if (run_begin != position)
		{
			sink.append(data + run_begin, position - run_begin);
		}
		if (position == size)
		{
			break;
		}

		/* The validated-run finder stops only on an ASCII quote, reverse
		   solidus, or C0 control.  Feeding that one scalar to the existing
		   escape primitive preserves the canonical spelling without decoding
		   the already-proved UTF sequence again. */
		basic_json_escape_buffer<char_type> escaped{};
		auto const code_point{json_code_unit(data[position++])};
		if (!escape_json_code_point(
				escaped, code_point, false, false, false)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(escaped.buffer, escaped.size);
	}
	json_sink_literal<u8'"'>(sink);
}

template <typename sink_type, typename string_type>
inline void json_emit_quoted_string(
	sink_type &sink, string_type const &string,
	::fast_io::json::json_serialize_options const &options)
{
	using char_type = typename sink_type::output_char_type;
	if constexpr (sink_type::upper_bounds_only)
	{
		using source_char_type = typename string_type::value_type;
		/*
		A one-byte or UTF-16 code unit contributes at most one six-unit
		\\uXXXX escape.  One UTF-32 scalar can require a surrogate pair, hence
		twelve output code units.  Adding the quotes yields a valid bound for
		every escape policy and every output character width.
		*/
		constexpr ::std::size_t multiplier{sizeof(source_char_type) <= 2u ? 6u : 12u};
		auto const size{static_cast<::std::size_t>(string.size())};
		constexpr auto maximum{static_cast<::std::size_t>(PTRDIFF_MAX)};
		if ((maximum - 2u) / multiplier < size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(nullptr, size * multiplier + 2u);
		return;
	}

	auto const *const data{string.data()};
	auto const size{static_cast<::std::size_t>(string.size())};
	auto position{json_find_plain_ascii_end(
		data, size, 0u, options.escape_solidus)};
	if (position == size)
	{
		json_sink_literal<u8'"'>(sink);
		json_sink_ascii_source(sink, data, size);
		json_sink_literal<u8'"'>(sink);
		return;
	}
	json_sink_literal<u8'"'>(sink);
	if (position != 0u)
	{
		json_sink_ascii_source(sink, data, position);
	}
	while (position != size)
	{
		auto const decoded{decode_json_code_point(data, size, position)};
		if (decoded.status != unicode_decode_status::ok) [[unlikely]]
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_unicode);
		}
		position = decoded.next;
		basic_json_escape_buffer<char_type> escaped{};
		auto const ascii_only{options.escape == ::fast_io::json::json_escape_policy::ascii};
		auto const javascript_safe{
			options.escape == ::fast_io::json::json_escape_policy::javascript_safe};
		if (!escape_json_code_point(escaped, decoded.code_point, ascii_only,
									javascript_safe, options.escape_solidus)) [[unlikely]]
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_unicode);
		}
		sink.append(escaped.buffer, escaped.size);

		auto const run_begin{position};
		position = json_find_plain_ascii_end(
			data, size, position, options.escape_solidus);
		if (run_begin != position)
		{
			json_sink_ascii_source(sink, data + run_begin, position - run_begin);
		}
	}
	json_sink_literal<u8'"'>(sink);
}

template <typename sink_type, typename string_type>
inline void json_emit_dom_quoted_string(
	sink_type &sink, string_type const &string, bool compact_direct,
	bool unicode_validated,
	::fast_io::json::json_serialize_options const &options)
{
	using output_char_type = typename sink_type::output_char_type;
	using source_char_type = typename string_type::value_type;
	if constexpr (::std::same_as<::std::remove_cv_t<output_char_type>,
								 ::std::remove_cv_t<source_char_type>>)
	{
		/* The DOM bit proves both Unicode validity and the absence of every
		   scalar escaped by minimal JSON.  It is intentionally inapplicable to
		   ASCII-only, JavaScript-safe, solidus-escaping, or transcoding output. */
		if (compact_direct &&
			options.escape == ::fast_io::json::json_escape_policy::minimal &&
			!options.escape_solidus) [[likely]]
		{
			auto const size{static_cast<::std::size_t>(string.size())};
			if (static_cast<::std::size_t>(PTRDIFF_MAX) - 2u < size) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			json_sink_literal<u8'"'>(sink);
			sink.append(string.data(), size);
			json_sink_literal<u8'"'>(sink);
			return;
		}
		if constexpr (!sink_type::upper_bounds_only)
		{
			if (unicode_validated &&
				options.escape == ::fast_io::json::json_escape_policy::minimal &&
				!options.escape_solidus) [[likely]]
			{
				json_emit_validated_minimal_quoted_string(sink, string);
				return;
			}
		}
	}
	json_emit_quoted_string(sink, string, options);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr char_type json_indent_character(
	::fast_io::json::json_serialize_options const &options) noexcept
{
	switch (options.indent_char)
	{
	case ' ':
		return ::fast_io::char_literal_v<u8' ', char_type>;
	case '\t':
		return ::fast_io::char_literal_v<u8'\t', char_type>;
	case '\r':
		return ::fast_io::char_literal_v<u8'\r', char_type>;
	case '\n':
		return ::fast_io::char_literal_v<u8'\n', char_type>;
	default:
		return static_cast<char_type>(static_cast<unsigned char>(options.indent_char));
	}
}

template <typename sink_type>
inline void json_emit_layout(
	sink_type &sink, ::std::size_t depth,
	::fast_io::json::json_serialize_options const &options)
{
	if (depth != 0u &&
		(::std::numeric_limits<::std::size_t>::max)() / depth < options.indent_width)
		[[unlikely]]
	{
		::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
	}
	json_sink_literal<u8'\n'>(sink);
	using char_type = typename sink_type::output_char_type;
	sink.append_repeat(json_indent_character<char_type>(options), depth * options.indent_width);
}

template <typename sink_type, ::std::integral char_type>
inline void json_emit_boolean(
	sink_type &sink, json_vector<char_type> &dynamic_storage, bool value)
{
	if constexpr (sink_type::upper_bounds_only)
	{
		auto const size{json_bound_scalar<char_type>(
			dynamic_storage, json_boolean_scalar{value})};
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else if constexpr (sink_type::measures_only)
	{
		auto const size{json_measure_scalar<char_type>(
			dynamic_storage, json_boolean_scalar{value})};
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else
	{
		if (json_try_append_scalar_reserve<char_type>(
				sink, json_boolean_scalar{value})) [[likely]]
		{
			return;
		}
		auto consumer{[&sink](char_type const *first, ::std::size_t size) {
			if (size == 0u) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			sink.append(first, size);
		}};
		json_consume_scalar(dynamic_storage, json_boolean_scalar{value}, consumer);
	}
}

template <typename sink_type, ::std::integral char_type, typename value_type>
inline void json_emit_integer(
	sink_type &sink, json_vector<char_type> &dynamic_storage,
	value_type const &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (sink_type::upper_bounds_only)
	{
		auto const size{json_bound_scalar<char_type>(dynamic_storage,
													 basic_json_integer_scalar<clean_type>{__builtin_addressof(value)})};
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else if constexpr (sink_type::measures_only &&
					   ::fast_io::details::my_integral<clean_type>)
	{
		auto const size{json_measure_scalar<char_type>(dynamic_storage,
													   basic_json_integer_scalar<clean_type>{__builtin_addressof(value)})};
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else
	{
		if constexpr (::fast_io::details::my_integral<clean_type>)
		{
			if (json_try_append_scalar_reserve<char_type>(
					sink, basic_json_integer_scalar<clean_type>{__builtin_addressof(value)})) [[likely]]
			{
				return;
			}
		}
		auto consumer{[&](char_type const *first, ::std::size_t size) {
			if constexpr (!::fast_io::details::my_integral<clean_type>)
			{
				json_validate_custom_number_token(first, size, true,
												  ::fast_io::json_unsigned_integer<clean_type>);
			}
			else if (size == 0u) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			sink.append(first, size);
		}};
		json_consume_scalar(dynamic_storage,
							basic_json_integer_scalar<clean_type>{__builtin_addressof(value)}, consumer);
	}
}

template <typename sink_type, ::std::integral char_type, typename value_type>
inline void json_emit_floating(
	sink_type &sink, json_vector<char_type> &dynamic_storage,
	value_type const &value)
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	json_check_finite<char_type>(value);
	if constexpr (sink_type::upper_bounds_only)
	{
		::std::size_t size{};
		if constexpr (::fast_io::details::my_floating_point<clean_type>)
		{
			size = json_bound_scalar<char_type>(dynamic_storage,
												basic_json_floating_scalar<clean_type>{__builtin_addressof(value)});
		}
		else
		{
			size = json_bound_scalar<char_type>(dynamic_storage,
												basic_json_custom_floating_scalar<clean_type>{__builtin_addressof(value)});
		}
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else if constexpr (sink_type::measures_only &&
					   ::fast_io::details::my_floating_point<clean_type>)
	{
		auto const size{json_measure_scalar<char_type>(dynamic_storage,
													   basic_json_floating_scalar<clean_type>{__builtin_addressof(value)})};
		if (size == 0u) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		sink.append(static_cast<char_type const *>(nullptr), size);
	}
	else
	{
		if constexpr (::fast_io::details::my_floating_point<clean_type>)
		{
			if (json_try_append_scalar_reserve<char_type>(
					sink, basic_json_floating_scalar<clean_type>{__builtin_addressof(value)})) [[likely]]
			{
				return;
			}
		}
		auto consumer{[&](char_type const *first, ::std::size_t size) {
			if constexpr (!::fast_io::details::my_floating_point<clean_type>)
			{
				json_validate_custom_number_token(first, size, false, false);
			}
			else if (size == 0u) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			sink.append(first, size);
		}};
		if constexpr (::fast_io::details::my_floating_point<clean_type>)
		{
			json_consume_scalar(dynamic_storage,
								basic_json_floating_scalar<clean_type>{__builtin_addressof(value)}, consumer);
		}
		else
		{
			json_consume_scalar(dynamic_storage,
								basic_json_custom_floating_scalar<clean_type>{__builtin_addressof(value)}, consumer);
		}
	}
}

/*
A JSON array may expose constructed physical segments through two ADL CPOs:

  size_t json_array_segment_count_define(array const &) noexcept;
  descriptor json_array_segment_at_define(array const &, size_t) noexcept;

The descriptor names its element type and carries `base`/`len`.  This is a
storage-layout proof, not an iterator-category guess: a container which does
not opt in keeps the ordinary iterator cursor.  The selected cursor is a type
property, so the per-element loop contains no run-time storage-mode branch.
*/
template <typename array_type>
concept json_physical_segment_array = requires(array_type const &array,
	::std::size_t index) {
	{
		json_array_segment_count_define(array)
	} noexcept -> ::std::same_as<::std::size_t>;
	{ json_array_segment_at_define(array, index) } noexcept;
	requires requires(
		::std::remove_cvref_t<decltype(json_array_segment_at_define(array, index))> segment) {
		typename decltype(segment)::value_type;
		requires ::std::same_as<
			::std::remove_cv_t<typename decltype(segment)::value_type>,
			::std::remove_cv_t<typename array_type::value_type>>;
		requires ::std::same_as<
			::std::remove_cvref_t<decltype(segment.base)>,
			typename array_type::value_type const *>;
		requires ::std::same_as<
			::std::remove_cvref_t<decltype(segment.len)>, ::std::size_t>;
	};
};

template <typename array_type, bool = json_physical_segment_array<array_type>>
class basic_json_array_cursor
{
	using iterator = decltype(::std::declval<array_type const &>().begin());
	iterator current_{};
	iterator end_{};

public:
	inline constexpr basic_json_array_cursor() noexcept = default;

	inline constexpr explicit basic_json_array_cursor(array_type const &array)
		noexcept(noexcept(array.begin()) && noexcept(array.end()))
		: current_(array.begin()), end_(array.end())
	{}

	[[nodiscard]] inline constexpr bool empty() const
		noexcept(noexcept(current_ == end_))
	{
		return current_ == end_;
	}

	[[nodiscard]] inline constexpr typename array_type::value_type const &take()
		noexcept(noexcept(*current_) && noexcept(++current_))
	{
		auto const *const result{::std::addressof(*current_)};
		++current_;
		return *result;
	}
};

template <typename array_type>
class basic_json_array_cursor<array_type, true>
{
	using value_type = typename array_type::value_type;

	array_type const *array_{};
	value_type const *current_{};
	value_type const *end_{};
	::std::size_t segment_index_{};

	/* Empty physical segments are legal but never become a cursor state.  This
	   keeps take() branch-minimal while making an imperfect external CPO unable
	   to manufacture a dereferenceable empty range. */
	inline constexpr void load_next_nonempty_segment() noexcept
	{
		auto const segment_count{json_array_segment_count_define(*array_)};
		while (segment_index_ != segment_count)
		{
			auto const segment{json_array_segment_at_define(*array_, segment_index_)};
			if (segment.len != 0u)
			{
				current_ = segment.base;
				end_ = segment.base + segment.len;
				return;
			}
			++segment_index_;
		}
		current_ = end_ = nullptr;
	}

public:
	inline constexpr basic_json_array_cursor() noexcept = default;

	inline constexpr explicit basic_json_array_cursor(array_type const &array) noexcept
		: array_(::std::addressof(array))
	{
		load_next_nonempty_segment();
	}

	[[nodiscard]] inline constexpr bool empty() const noexcept
	{
		return current_ == nullptr;
	}

	[[nodiscard]] inline constexpr value_type const &take() noexcept
	{
		auto const *const result{current_++};
		if (current_ == end_)
		{
			++segment_index_;
			load_next_nonempty_segment();
		}
		return *result;
	}
};

/*
An insertion-ordered object container can expose the same physical-span proof
as an array without pretending that its lookup buckets are the traversal
order.  fast_io::hash_map deliberately returns zero spans after an interior
erase; the cursor then selects its ordinary hole-skipping iterator.  Dense
objects therefore pay only pointer increments in the writer, while mutation
keeps the stable-reference semantics of the general iterator path.
*/
template <typename object_type>
concept json_physical_segment_object = requires(object_type const &object,
	::std::size_t index) {
	{
		hash_map_segment_count_define(object)
	} noexcept -> ::std::same_as<::std::size_t>;
	{ hash_map_segment_at_define(object, index) } noexcept;
	requires requires(
		::std::remove_cvref_t<decltype(hash_map_segment_at_define(object, index))> segment) {
		typename decltype(segment)::value_type;
		requires ::std::same_as<
			::std::remove_cv_t<typename decltype(segment)::value_type>,
			::std::remove_cv_t<typename object_type::value_type>>;
		requires ::std::same_as<
			::std::remove_cvref_t<decltype(segment.base)>,
			typename object_type::value_type const *>;
		requires ::std::same_as<
			::std::remove_cvref_t<decltype(segment.len)>, ::std::size_t>;
	};
};

template <typename object_type,
	bool = json_physical_segment_object<object_type>>
class basic_json_object_cursor
{
	using iterator = decltype(::std::declval<object_type const &>().begin());
	iterator current_{};
	iterator end_{};

public:
	inline constexpr basic_json_object_cursor() noexcept = default;

	inline constexpr explicit basic_json_object_cursor(
		object_type const &object)
		noexcept(noexcept(object.begin()) && noexcept(object.end()))
		: current_(object.begin()), end_(object.end())
	{}

	[[nodiscard]] inline constexpr bool empty() const
		noexcept(noexcept(current_ == end_))
	{
		return current_ == end_;
	}

	[[nodiscard]] inline constexpr typename object_type::value_type const &take()
		noexcept(noexcept(*current_) && noexcept(++current_))
	{
		auto const *const result{::std::addressof(*current_)};
		++current_;
		return *result;
	}
};

template <typename object_type>
class basic_json_object_cursor<object_type, true>
{
	using value_type = typename object_type::value_type;
	using iterator = decltype(::std::declval<object_type const &>().begin());

	object_type const *object_{};
	value_type const *current_{};
	value_type const *end_{};
	iterator fallback_current_{};
	iterator fallback_end_{};
	::std::size_t segment_index_{};
	::std::size_t segment_count_{};
	bool fallback_{};

	inline constexpr void load_next_nonempty_segment() noexcept
	{
		while (segment_index_ != segment_count_)
		{
			auto const segment{
				hash_map_segment_at_define(*object_, segment_index_)};
			if (segment.len != 0u)
			{
				current_ = segment.base;
				end_ = segment.base + segment.len;
				return;
			}
			++segment_index_;
		}
		current_ = end_ = nullptr;
	}

public:
	inline constexpr basic_json_object_cursor() noexcept = default;

	inline constexpr explicit basic_json_object_cursor(
		object_type const &object) noexcept
		: object_(::std::addressof(object)),
		  segment_count_(hash_map_segment_count_define(object))
	{
		/* Zero means either an empty dense map or a map containing erase
		   holes.  Only the latter needs the stable hole-skipping iterator. */
		if (segment_count_ == 0u && !object.empty())
		{
			fallback_current_ = object.begin();
			fallback_end_ = object.end();
			fallback_ = true;
			return;
		}
		load_next_nonempty_segment();
	}

	[[nodiscard]] inline constexpr bool empty() const noexcept
	{
		return fallback_ ? fallback_current_ == fallback_end_
						 : current_ == nullptr;
	}

	[[nodiscard]] inline constexpr value_type const &take() noexcept
	{
		if (fallback_)
		{
			auto const *const result{::std::addressof(*fallback_current_)};
			++fallback_current_;
			return *result;
		}
		auto const *const result{current_++};
		if (current_ == end_)
		{
			++segment_index_;
			load_next_nonempty_segment();
		}
		return *result;
	}
};

template <typename node_type>
struct basic_json_walk_frame
{
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	using array_type = typename slice_type::array_type;
	using object_type = typename slice_type::object_type;
	using array_cursor = basic_json_array_cursor<array_type>;
	using object_cursor = basic_json_object_cursor<object_type>;

	struct array_state
	{
		array_cursor cursor;
	};

	struct object_state
	{
		object_cursor cursor;
	};

	/*
	Only an open container owns continuation state.  A tagged union keeps the
	mutually exclusive array and object iterator pairs from inflating every
	stack entry.  In particular, a scalar never creates a frame.
	*/
	::std::variant<array_state, object_state> state;
	::std::size_t depth{};

	inline constexpr basic_json_walk_frame(
		::std::in_place_index_t<0>, array_cursor cursor,
		::std::size_t depth_argument)
		: state(::std::in_place_index<0>, ::std::move(cursor)), depth(depth_argument)
	{
	}

	inline constexpr basic_json_walk_frame(
		::std::in_place_index_t<1>, object_cursor cursor,
		::std::size_t depth_argument)
		: state(::std::in_place_index<1>, ::std::move(cursor)),
		  depth(depth_argument)
	{
	}
};

/*
Emit one value which cannot open a traversal continuation.  Returning false is
an exact proof that the value is an array or object; all scalar alternatives,
including policy-controlled undefined, complete in this call.  Keeping this
operation separate lets a container fuse a run of scalar children without
round-tripping through the ancestor-frame state machine for every element.
*/
template <typename sink_type, typename node_type>
[[nodiscard]] inline bool json_emit_noncontainer_value(
	sink_type &sink,
	json_vector<typename sink_type::output_char_type> &dynamic_scalar_storage,
	::fast_io::json::basic_const_json_slice<node_type> value,
	::fast_io::json::json_serialize_options const &options)
{
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	switch (value.kind())
	{
	case ::fast_io::json::json_kind::undefined:
		switch (options.undefined)
		{
		case ::fast_io::json::json_undefined_policy::error:
			::fast_io::json::throw_json_error(
				::fast_io::json::json_errc::is_undefined);
		case ::fast_io::json::json_undefined_policy::as_null:
			json_sink_literal<u8'n', u8'u', u8'l', u8'l'>(sink);
			break;
		case ::fast_io::json::json_undefined_policy::as_literal:
			json_sink_literal<u8'u', u8'n', u8'd', u8'e', u8'f', u8'i',
				u8'n', u8'e', u8'd'>(sink);
			break;
		}
		return true;
	case ::fast_io::json::json_kind::null:
		json_sink_literal<u8'n', u8'u', u8'l', u8'l'>(sink);
		return true;
	case ::fast_io::json::json_kind::boolean:
		json_emit_boolean(sink, dynamic_scalar_storage, value.get_boolean());
		return true;
	case ::fast_io::json::json_kind::number:
		json_emit_floating(sink, dynamic_scalar_storage, value.get_number());
		return true;
	case ::fast_io::json::json_kind::integer:
		if constexpr (slice_type::has_integer)
		{
			json_emit_integer(sink, dynamic_scalar_storage, value.get_integer());
			return true;
		}
		else
		{
			::fast_io::fast_terminate();
		}
	case ::fast_io::json::json_kind::uinteger:
		if constexpr (slice_type::has_uinteger)
		{
			json_emit_integer(sink, dynamic_scalar_storage, value.get_uinteger());
			return true;
		}
		else
		{
			::fast_io::fast_terminate();
		}
	case ::fast_io::json::json_kind::string:
		json_emit_dom_quoted_string(sink, value.get_string(),
			value.string_is_compact_direct(),
			value.string_is_unicode_validated(), options);
		return true;
	case ::fast_io::json::json_kind::array:
	case ::fast_io::json::json_kind::object:
		return false;
	default:
		::fast_io::fast_terminate();
	}
}

/*
Iterative preorder serialization.  Only non-empty open containers are kept on
the continuation stack.  Scalar children are emitted immediately and then the
parent iterator is resumed; they never allocate, initialize, push, or pop a
frame.  Before descending into a container its parent iterator is advanced, so
stack reallocation cannot invalidate a live frame reference.

The loop invariant is that `current_value` is the one value still to be
entered, while every frame is one open ancestor whose iterator already points
past the child being entered.  Once that value completes, the inner loop either
selects the next sibling (emitting exactly one separator) or closes and removes
an exhausted ancestor.  Consequently every value is entered exactly once,
delimiters are balanced, commas are neither leading nor trailing, and depth is
bounded without C++ recursion.
*/
template <typename sink_type, typename node_type>
inline void json_walk_document(
	sink_type &sink, ::fast_io::json::basic_const_json_slice<node_type> root,
	::fast_io::json::json_serialize_options const &options)
{
	using char_type = typename sink_type::output_char_type;
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	using frame_type = basic_json_walk_frame<node_type>;
	using array_cursor = typename frame_type::array_cursor;
	using object_cursor = typename frame_type::object_cursor;
	json_vector<frame_type> frames;
	frames.reserve(16u);
	json_vector<char_type> dynamic_scalar_storage;
	slice_type current_value{root};
	::std::size_t current_depth{};

	for (;;)
	{
		if (!json_emit_noncontainer_value(
				sink, dynamic_scalar_storage, current_value, options))
		{
			switch (current_value.kind())
			{
			case ::fast_io::json::json_kind::array:
		{
			if (options.max_depth <= current_depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
			}
			auto const &array{current_value.get_array()};
			json_sink_literal<u8'['>(sink);
			array_cursor cursor{array};
			if (cursor.empty())
			{
				json_sink_literal<u8']'>(sink);
				break;
			}
			if (options.pretty)
			{
				json_emit_layout(sink, current_depth + 1u, options);
			}
			bool descending{};
			for (;;)
			{
				auto const child{slice_type{cursor.take()}};
				if (!json_emit_noncontainer_value(
						sink, dynamic_scalar_storage, child, options))
				{
					current_value = child;
					frames.emplace_back(::std::in_place_index<0>,
						::std::move(cursor), current_depth);
					++current_depth;
					descending = true;
					break;
				}
				if (cursor.empty())
				{
					if (options.pretty)
					{
						json_emit_layout(sink, current_depth, options);
					}
					json_sink_literal<u8']'>(sink);
					break;
				}
				json_sink_literal<u8','>(sink);
				if (options.pretty)
				{
					json_emit_layout(sink, current_depth + 1u, options);
				}
			}
			if (descending)
			{
				continue;
			}
			break;
		}
		case ::fast_io::json::json_kind::object:
		{
			if (options.max_depth <= current_depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
			}
			auto const &object{current_value.get_object()};
			json_sink_literal<u8'{'>(sink);
			{
				object_cursor cursor{object};
				if (cursor.empty())
				{
					json_sink_literal<u8'}'>(sink);
					break;
				}
				auto const &entry{cursor.take()};
				if (options.pretty)
				{
					json_emit_layout(sink, current_depth + 1u, options);
				}
				json_emit_quoted_string(sink, entry.first, options);
				if (options.pretty)
				{
					json_sink_literal<u8':', u8' '>(sink);
				}
				else
				{
					json_sink_literal<u8':'>(sink);
				}
				current_value = slice_type{entry.second};
				frames.emplace_back(::std::in_place_index<1>, ::std::move(cursor),
					current_depth);
				++current_depth;
				continue;
			}
		}
			default:
				::fast_io::fast_terminate();
			}
		}

		/* The current scalar or empty container is complete.  Exhausted
		ancestors are closed in one pass; otherwise select the next sibling. */
		for (;;)
		{
			if (frames.empty())
			{
				return;
			}

			auto &parent{frames.back()};
			current_depth = parent.depth;
			if (parent.state.index() == 0u)
			{
				auto &array{::std::get<0>(parent.state)};
				if (array.cursor.empty())
				{
					if (options.pretty)
					{
						json_emit_layout(sink, parent.depth, options);
					}
					json_sink_literal<u8']'>(sink);
					frames.pop_back();
					continue;
				}

				json_sink_literal<u8','>(sink);
				if (options.pretty)
				{
					json_emit_layout(sink, parent.depth + 1u, options);
				}
				current_value = slice_type{array.cursor.take()};
				++current_depth;
				break;
			}

			auto &object{::std::get<1>(parent.state)};
			if (object.cursor.empty())
			{
				if (options.pretty)
				{
					json_emit_layout(sink, parent.depth, options);
				}
				json_sink_literal<u8'}'>(sink);
				frames.pop_back();
				continue;
			}

			json_sink_literal<u8','>(sink);
			if (options.pretty)
			{
				json_emit_layout(sink, parent.depth + 1u, options);
			}
			auto const &entry{object.cursor.take()};
			json_emit_quoted_string(sink, entry.first, options);
			if (options.pretty)
			{
				json_sink_literal<u8':', u8' '>(sink);
			}
			else
			{
				json_sink_literal<u8':'>(sink);
			}
			current_value = slice_type{entry.second};
			++current_depth;
			break;
		}
	}
}

template <::std::integral char_type, typename node_type>
[[nodiscard]] inline ::std::size_t json_measure_document(
	::fast_io::json::basic_json_print_view<node_type> const &view)
{
	basic_json_size_sink<char_type> sink;
	json_walk_document(sink, view.reference, view.options);
	return sink.size();
}

template <::std::integral char_type, typename node_type>
[[nodiscard]] inline ::std::size_t json_bound_document(
	::fast_io::json::basic_json_print_view<node_type> const &view)
{
	basic_json_bound_sink<char_type> sink;
	json_walk_document(sink, view.reference, view.options);
	return sink.size();
}

template <::std::integral char_type, typename node_type>
[[nodiscard]] inline char_type *json_write_document(
	char_type *output,
	::fast_io::json::basic_json_print_view<node_type> const &view)
{
	basic_json_contiguous_sink<char_type> sink{output};
	json_walk_document(sink, view.reference, view.options);
	return sink.current();
}

template <::std::integral char_type, typename node_type>
[[nodiscard]] inline char_type *json_write_document_precise(
	char_type *output, ::std::size_t size,
	::fast_io::json::basic_json_print_view<node_type> const &view)
{
	/* The precise protocol owns an exactly sized destination produced by the
	preceding immutable-DOM measurement.  The second traversal has the same
	deterministic scalar formatters and options, so checking every append would
	restate the same invariant in the hot loop.  Verify the final cursor once. */
	basic_json_contiguous_sink<char_type> sink{output};
	json_walk_document(sink, view.reference, view.options);
	auto *const result{sink.current()};
	if (result != output + size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return result;
}

template <::std::integral char_type, typename output_type, typename node_type>
inline void json_write_document_to_output(
	output_type &output,
	::fast_io::json::basic_json_print_view<node_type> const &view)
{
	basic_json_output_sink<char_type, output_type> sink{output};
#if defined(__cpp_exceptions) && \
	!(defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0))
	try
	{
		json_walk_document(sink, view.reference, view.options);
		sink.flush();
	}
	catch (...)
	{
		// Direct streaming has already logically produced this prefix.  Publish
		// the final staged part before propagating the serialization failure.
		// A cleanup failure must not replace the producer's original exception.
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
	json_walk_document(sink, view.reference, view.options);
	sink.flush();
#endif
}

enum class json_frame_phase : unsigned char
{
	start,
	complete,
	string_content,
	array_prepare_first,
	array_push_child,
	array_after_child,
	array_prepare_next,
	array_emit_close,
	object_prepare_first,
	object_key_open,
	object_key_content,
	object_colon,
	object_push_value,
	object_after_value,
	object_prepare_next,
	object_emit_close
};

template <::std::integral char_type, typename node_type>
class basic_json_print_context
{
	using view_type = ::fast_io::json::basic_json_print_view<node_type>;
	using slice_type = ::fast_io::json::basic_const_json_slice<node_type>;
	using array_type = typename slice_type::array_type;
	using object_type = typename slice_type::object_type;
	using array_cursor = basic_json_array_cursor<array_type>;
	using object_cursor = basic_json_object_cursor<object_type>;

	struct frame
	{
		slice_type value{};
		json_frame_phase phase{json_frame_phase::start};
		::std::size_t depth{};
		::std::size_t string_offset{};
		array_cursor array{};
		object_cursor object{};
		typename object_type::value_type const *object_entry{};

		inline constexpr frame() noexcept = default;

		inline constexpr frame(slice_type value_argument, ::std::size_t depth_argument) noexcept
			: value(value_argument), depth(depth_argument)
		{
		}
	};

	json_vector<frame> frames_{};
	json_vector<char_type> scalar_pending_{};
	::fast_io::json::json_serialize_options options_{};
	char_type small_pending_[12u]{};
	::std::size_t small_size_{};
	::std::size_t small_position_{};
	::std::size_t scalar_position_{};
	::std::size_t indent_remaining_{};
	bool layout_active_{};
	bool initialized_{};
	bool finished_{};

	template <char8_t... characters>
	inline constexpr void stage_literal() noexcept
	{
		static_assert(sizeof...(characters) <= 12u);
		small_size_ = 0u;
		small_position_ = 0u;
		((small_pending_[small_size_++] =
			  ::fast_io::char_literal_v<characters, char_type>),
		 ...);
	}

	[[nodiscard]] inline constexpr char_type indent_character() const noexcept
	{
		switch (options_.indent_char)
		{
		case ' ':
			return ::fast_io::char_literal_v<u8' ', char_type>;
		case '\t':
			return ::fast_io::char_literal_v<u8'\t', char_type>;
		case '\r':
			return ::fast_io::char_literal_v<u8'\r', char_type>;
		case '\n':
			return ::fast_io::char_literal_v<u8'\n', char_type>;
		default:
			return static_cast<char_type>(static_cast<unsigned char>(options_.indent_char));
		}
	}

	inline void begin_layout(::std::size_t depth)
	{
		if (depth != 0u &&
			(::std::numeric_limits<::std::size_t>::max)() / depth < options_.indent_width) [[unlikely]]
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
		}
		indent_remaining_ = depth * options_.indent_width;
		layout_active_ = true;
		stage_literal<u8'\n'>();
	}

	inline void stage_indent_chunk() noexcept
	{
		small_size_ = indent_remaining_ < 12u ? indent_remaining_ : 12u;
		small_position_ = 0u;
		indent_remaining_ -= small_size_;
		auto const value{indent_character()};
		for (::std::size_t index{}; index != small_size_; ++index)
		{
			small_pending_[index] = value;
		}
	}

	inline void stage_escaped_code_point(::std::uint_least32_t code_point)
	{
		basic_json_escape_buffer<char_type> escaped{};
		auto const ascii_only{options_.escape == ::fast_io::json::json_escape_policy::ascii};
		auto const javascript_safe{
			options_.escape == ::fast_io::json::json_escape_policy::javascript_safe};
		if (!escape_json_code_point(escaped, code_point, ascii_only,
									javascript_safe, options_.escape_solidus)) [[unlikely]]
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_unicode);
		}
		small_size_ = escaped.size;
		small_position_ = 0u;
		for (::std::size_t index{}; index != escaped.size; ++index)
		{
			small_pending_[index] = escaped.buffer[index];
		}
	}

	template <typename string_type>
	[[nodiscard]] inline bool stage_next_string_unit(
		string_type const &string, ::std::size_t &position)
	{
		auto const decoded{decode_json_code_point(
			string.data(), static_cast<::std::size_t>(string.size()), position)};
		if (decoded.status == unicode_decode_status::end)
		{
			return false;
		}
		if (decoded.status == unicode_decode_status::invalid) [[unlikely]]
		{
			::fast_io::json::throw_json_error(::fast_io::json::json_errc::invalid_unicode);
		}
		position = decoded.next;
		stage_escaped_code_point(decoded.code_point);
		return true;
	}

	inline void stage_number(typename slice_type::number_type const &value)
	{
		json_check_finite<char_type>(value);
		using value_type = ::std::remove_cvref_t<decltype(value)>;
		if constexpr (::fast_io::details::my_floating_point<value_type>)
		{
			json_materialize_scalar(scalar_pending_,
									basic_json_floating_scalar<value_type>{__builtin_addressof(value)});
		}
		else
		{
			json_materialize_scalar(scalar_pending_,
									basic_json_custom_floating_scalar<value_type>{__builtin_addressof(value)});
			json_validate_custom_number_token(scalar_pending_, false, false);
		}
		scalar_position_ = 0u;
	}

	template <typename value_type>
	inline void stage_integer(value_type const &value)
	{
		using clean_type = ::std::remove_cvref_t<value_type>;
		json_materialize_scalar(scalar_pending_,
								basic_json_integer_scalar<clean_type>{__builtin_addressof(value)});
		if constexpr (!::fast_io::details::my_integral<clean_type>)
		{
			json_validate_custom_number_token(scalar_pending_, true,
											  ::fast_io::json_unsigned_integer<clean_type>);
		}
		scalar_position_ = 0u;
	}

	inline void process_start(frame &current)
	{
		switch (current.value.kind())
		{
		case ::fast_io::json::json_kind::undefined:
			switch (options_.undefined)
			{
			case ::fast_io::json::json_undefined_policy::error:
				::fast_io::json::throw_json_error(::fast_io::json::json_errc::is_undefined);
			case ::fast_io::json::json_undefined_policy::as_null:
				stage_literal<u8'n', u8'u', u8'l', u8'l'>();
				break;
			case ::fast_io::json::json_undefined_policy::as_literal:
				stage_literal<u8'u', u8'n', u8'd', u8'e', u8'f', u8'i', u8'n', u8'e', u8'd'>();
				break;
			}
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::null:
			stage_literal<u8'n', u8'u', u8'l', u8'l'>();
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::boolean:
			json_materialize_scalar(scalar_pending_,
									json_boolean_scalar{current.value.get_boolean()});
			scalar_position_ = 0u;
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::number:
			stage_number(current.value.get_number());
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::integer:
			if constexpr (slice_type::has_integer)
			{
				stage_integer(current.value.get_integer());
			}
			else
			{
				::fast_io::fast_terminate();
			}
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::uinteger:
			if constexpr (slice_type::has_uinteger)
			{
				stage_integer(current.value.get_uinteger());
			}
			else
			{
				::fast_io::fast_terminate();
			}
			current.phase = json_frame_phase::complete;
			break;
		case ::fast_io::json::json_kind::string:
			current.string_offset = 0u;
			current.phase = json_frame_phase::string_content;
			stage_literal<u8'"'>();
			break;
		case ::fast_io::json::json_kind::array:
			if (options_.max_depth <= current.depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
			}
			{
				auto const &array{current.value.get_array()};
				current.array = array_cursor{array};
			}
			current.phase = json_frame_phase::array_prepare_first;
			stage_literal<u8'['>();
			break;
		case ::fast_io::json::json_kind::object:
			if (options_.max_depth <= current.depth) [[unlikely]]
			{
				::fast_io::json::throw_json_error(::fast_io::json::json_errc::depth_exceeded);
			}
			{
				auto const &object{current.value.get_object()};
				current.object = object_cursor{object};
				current.object_entry = nullptr;
			}
			current.phase = json_frame_phase::object_prepare_first;
			stage_literal<u8'{'>();
			break;
		}
	}

	inline void process_frame()
	{
		auto &current{frames_.back()};
		switch (current.phase)
		{
		case json_frame_phase::start:
			process_start(current);
			break;
		case json_frame_phase::complete:
			frames_.pop_back();
			break;
		case json_frame_phase::string_content:
			if (!stage_next_string_unit(current.value.get_string(), current.string_offset))
			{
				current.phase = json_frame_phase::complete;
				stage_literal<u8'"'>();
			}
			break;
		case json_frame_phase::array_prepare_first:
			if (current.array.empty())
			{
				current.phase = json_frame_phase::complete;
				stage_literal<u8']'>();
			}
			else
			{
				current.phase = json_frame_phase::array_push_child;
				if (options_.pretty)
				{
					begin_layout(current.depth + 1u);
				}
			}
			break;
		case json_frame_phase::array_push_child:
		{
			auto const child{slice_type{current.array.take()}};
			auto const child_depth{current.depth + 1u};
			current.phase = json_frame_phase::array_after_child;
			frames_.emplace_back(child, child_depth);
		}
		break;
		case json_frame_phase::array_after_child:
			if (!current.array.empty())
			{
				current.phase = json_frame_phase::array_prepare_next;
				stage_literal<u8','>();
			}
			else if (options_.pretty)
			{
				current.phase = json_frame_phase::array_emit_close;
				begin_layout(current.depth);
			}
			else
			{
				current.phase = json_frame_phase::complete;
				stage_literal<u8']'>();
			}
			break;
		case json_frame_phase::array_prepare_next:
			current.phase = json_frame_phase::array_push_child;
			if (options_.pretty)
			{
				begin_layout(current.depth + 1u);
			}
			break;
		case json_frame_phase::array_emit_close:
			current.phase = json_frame_phase::complete;
			stage_literal<u8']'>();
			break;
		case json_frame_phase::object_prepare_first:
			if (current.object.empty())
			{
				current.phase = json_frame_phase::complete;
				stage_literal<u8'}'>();
			}
			else
			{
				current.object_entry =
					::std::addressof(current.object.take());
				current.phase = json_frame_phase::object_key_open;
				if (options_.pretty)
				{
					begin_layout(current.depth + 1u);
				}
			}
			break;
		case json_frame_phase::object_key_open:
			current.string_offset = 0u;
			current.phase = json_frame_phase::object_key_content;
			stage_literal<u8'"'>();
			break;
		case json_frame_phase::object_key_content:
			if (!stage_next_string_unit(current.object_entry->first,
					current.string_offset))
			{
				current.phase = json_frame_phase::object_colon;
				stage_literal<u8'"'>();
			}
			break;
		case json_frame_phase::object_colon:
			current.phase = json_frame_phase::object_push_value;
			if (options_.pretty)
			{
				stage_literal<u8':', u8' '>();
			}
			else
			{
				stage_literal<u8':'>();
			}
			break;
		case json_frame_phase::object_push_value:
		{
			auto const child{slice_type{current.object_entry->second}};
			auto const child_depth{current.depth + 1u};
			current.phase = json_frame_phase::object_after_value;
			frames_.emplace_back(child, child_depth);
		}
		break;
		case json_frame_phase::object_after_value:
			if (!current.object.empty())
			{
				current.phase = json_frame_phase::object_prepare_next;
				stage_literal<u8','>();
			}
			else if (options_.pretty)
			{
				current.phase = json_frame_phase::object_emit_close;
				begin_layout(current.depth);
			}
			else
			{
				current.phase = json_frame_phase::complete;
				stage_literal<u8'}'>();
			}
			break;
		case json_frame_phase::object_prepare_next:
			current.object_entry =
				::std::addressof(current.object.take());
			current.phase = json_frame_phase::object_key_open;
			if (options_.pretty)
			{
				begin_layout(current.depth + 1u);
			}
			break;
		case json_frame_phase::object_emit_close:
			current.phase = json_frame_phase::complete;
			stage_literal<u8'}'>();
			break;
		}
	}

	inline char_type *drain_small(char_type *output, char_type *output_last) noexcept
	{
		while (output != output_last && small_position_ != small_size_)
		{
			*output++ = small_pending_[small_position_++];
		}
		if (small_position_ == small_size_)
		{
			small_position_ = small_size_ = 0u;
		}
		return output;
	}

	inline char_type *drain_scalar(char_type *output, char_type *output_last) noexcept
	{
		while (output != output_last && scalar_position_ != scalar_pending_.size())
		{
			*output++ = scalar_pending_[scalar_position_++];
		}
		if (scalar_position_ == scalar_pending_.size())
		{
			scalar_position_ = 0u;
			scalar_pending_.clear();
		}
		return output;
	}

public:
	inline basic_json_print_context() = default;

	inline ::fast_io::context_print_result<char_type *> print_context_define(
		view_type const &view, char_type *output, char_type *output_last)
	{
		if (output == output_last)
		{
			return {output, finished_};
		}
		if (finished_)
		{
			return {output, true};
		}
		if (!initialized_)
		{
			options_ = view.options;
			frames_.emplace_back(view.reference, 0u);
			initialized_ = true;
		}

		for (;;)
		{
			if (small_position_ != small_size_)
			{
				output = drain_small(output, output_last);
				if (output == output_last)
				{
					return {output, false};
				}
				continue;
			}
			if (scalar_position_ != scalar_pending_.size())
			{
				output = drain_scalar(output, output_last);
				if (output == output_last)
				{
					return {output, false};
				}
				continue;
			}
			if (layout_active_)
			{
				if (indent_remaining_ != 0u)
				{
					stage_indent_chunk();
					continue;
				}
				layout_active_ = false;
			}
			if (frames_.empty())
			{
				finished_ = true;
				return {output, true};
			}
			process_frame();
		}
	}
};

} // namespace details

template <::std::integral char_type, typename node_type, bool transcoding>
[[nodiscard]] inline constexpr auto print_context_type(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return ::fast_io::io_type_t<details::basic_json_print_context<char_type, node_type>>{};
}

template <::std::integral char_type, typename node_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::size_t print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	constexpr ::std::size_t preferred_bytes{4096u};
	constexpr ::std::size_t result{preferred_bytes / sizeof(char_type)};
	return result == 0u ? 1u : result;
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>,
	basic_json_io_print_view<char_type, node_type, transcoding> const &view)
{
	return details::json_bound_document<char_type>(
		static_cast<basic_json_print_view<node_type> const &>(view));
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>,
	char_type *output,
	basic_json_io_print_view<char_type, node_type, transcoding> const &view)
{
	return details::json_write_document(
		output, static_cast<basic_json_print_view<node_type> const &>(view));
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>,
	basic_json_io_print_view<char_type, node_type, transcoding> const &view)
{
	return details::json_measure_document<char_type>(
		static_cast<basic_json_print_view<node_type> const &>(view));
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>,
	char_type *output, ::std::size_t size,
	basic_json_io_print_view<char_type, node_type, transcoding> const &view)
{
	return details::json_write_document_precise(
		output, size, static_cast<basic_json_print_view<node_type> const &>(view));
}

/*
Exact sizing is a complete DOM traversal followed by a second traversal for
emission.  A destination which value-initializes the exact range would add a
third full-memory write, so concat should prefer an uninitialized exact-growth
primitive when it must select this fallback.
*/
template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::true_type print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	constexpr ::std::size_t preferred_bytes{4096u};
	constexpr ::std::size_t result{preferred_bytes / sizeof(char_type)};
	return result == 0u ? 1u : result;
}

template <::std::integral char_type, typename output_type, typename node_type, bool transcoding>
inline void print_define(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>,
	output_type &output,
	basic_json_io_print_view<char_type, node_type, transcoding> const &view)
{
	details::json_write_document_to_output<char_type>(
		output, static_cast<basic_json_print_view<node_type> const &>(view));
}

/*
The direct serializer consumes every DOM edge and numeric leaf once, in source
order, and its byte sequence is independent of staging-window boundaries.
Consequently core may place one bounded put area in front of an otherwise
unbuffered destination without measuring or replaying the document.  This
proof remains valid for an unmarked custom number because that leaf is
normalized exactly once on this path.
*/
template <::std::integral char_type, typename node_type, bool transcoding>
[[nodiscard]] inline constexpr ::std::true_type print_single_pass_staging_safe(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::true_type print_put_area_preferred(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::true_type print_buffered_preferred(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::true_type print_one_pass_preferred(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

/*
A freshly constructed concat result has no reusable capacity to preserve.  For
this whole-document state machine, one forward traversal with geometric growth
is cheaper than measuring every node and scalar before a second traversal.
This construction preference is intentionally distinct from the ordinary
one-pass stream preference: range views, for example, can prefer a real put
area while still benefiting from contiguous staging and range construction.
*/
template <::std::integral char_type, typename node_type, bool transcoding>
	requires(details::json_node_print_replay_safe<node_type>)
[[nodiscard]] inline constexpr ::std::true_type print_concat_one_pass_preferred(
	::fast_io::io_reserve_type_t<
		char_type, basic_json_io_print_view<char_type, node_type, transcoding>>) noexcept
{
	return {};
}

template <typename node_type>
[[nodiscard]] inline constexpr basic_json_print_view<node_type> make_json_print_view(
	::fast_io::json::basic_json<node_type> const &value,
	::fast_io::json::json_serialize_options options = {}) noexcept
{
	return {value.slice(), options};
}

template <typename node_type>
auto make_json_print_view(
	::fast_io::json::basic_json<node_type> &&,
	::fast_io::json::json_serialize_options = {}) = delete;

template <typename node_type>
auto make_json_print_view(
	::fast_io::json::basic_json<node_type> const &&,
	::fast_io::json::json_serialize_options = {}) = delete;

template <typename node_type>
[[nodiscard]] inline constexpr basic_json_print_view<node_type> make_json_print_view(
	::fast_io::json::basic_json_slice<node_type> const &value,
	::fast_io::json::json_serialize_options options = {}) noexcept
{
	return {::fast_io::json::basic_const_json_slice<node_type>{value}, options};
}

template <typename node_type>
[[nodiscard]] inline constexpr basic_json_print_view<node_type> make_json_print_view(
	::fast_io::json::basic_const_json_slice<node_type> value,
	::fast_io::json::json_serialize_options options = {}) noexcept
{
	return {value, options};
}

} // namespace fast_io::json

namespace fast_io::json
{

template <typename node_type>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, basic_json<node_type> const &value) noexcept
{
	return ::fast_io::json::make_json_print_view(value);
}

template <typename node_type>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, basic_json_slice<node_type> const &value) noexcept
{
	return ::fast_io::json::make_json_print_view(value);
}

template <typename node_type>
[[nodiscard]] inline constexpr auto print_alias_define(
	::fast_io::io_alias_t, basic_const_json_slice<node_type> value) noexcept
{
	return ::fast_io::json::make_json_print_view(value);
}

} // namespace fast_io::json

namespace fast_io::manipulators
{

template <typename node_type>
auto json(::fast_io::json::basic_json<node_type> &&,
	::fast_io::json::json_serialize_options = {}) = delete;

template <typename node_type>
auto json(::fast_io::json::basic_json<node_type> const &&,
	::fast_io::json::json_serialize_options = {}) = delete;

template <typename node_type>
auto pretty_json(::fast_io::json::basic_json<node_type> &&,
	::std::size_t = 2u) = delete;

template <typename node_type>
auto pretty_json(::fast_io::json::basic_json<node_type> const &&,
	::std::size_t = 2u) = delete;

template <typename json_type>
	requires requires(json_type const &value, ::fast_io::json::json_serialize_options options) {
		::fast_io::json::make_json_print_view(value, options);
	}
[[nodiscard]] inline constexpr auto json(
	json_type const &value, ::fast_io::json::json_serialize_options options = {}) noexcept(noexcept(::fast_io::json::make_json_print_view(value, options)))
{
	return ::fast_io::json::make_json_print_view(value, options);
}

template <typename json_type>
	requires requires(json_type const &value, ::fast_io::json::json_serialize_options options) {
		::fast_io::json::make_json_print_view(value, options);
	}
[[nodiscard]] inline constexpr auto pretty_json(
	json_type const &value, ::std::size_t indent = 2u) noexcept(noexcept(::fast_io::json::make_json_print_view(value, ::fast_io::json::json_serialize_options{})))
{
	::fast_io::json::json_serialize_options options{};
	options.pretty = true;
	options.indent_width = indent;
	return ::fast_io::json::make_json_print_view(value, options);
}

} // namespace fast_io::manipulators
