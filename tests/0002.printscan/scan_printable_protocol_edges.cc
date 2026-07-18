#include <cassert>
#include <cstddef>
#include <type_traits>

#if !defined(FAST_IO_DISABLE_FLOATING_POINT)
#define FAST_IO_DISABLE_FLOATING_POINT
#define FAST_IO_PROTOCOL_EDGE_RESTORE_FLOATING
#endif
#include <fast_io.h>
#if defined(FAST_IO_PROTOCOL_EDGE_RESTORE_FLOATING)
#undef FAST_IO_PROTOCOL_EDGE_RESTORE_FLOATING
#undef FAST_IO_DISABLE_FLOATING_POINT
#endif

// This file exercises protocol recognition and dispatch invariants only. Every producer emits at most one literal
// character and every scanner merely changes protocol state; no numeric conversion algorithm participates.
namespace scan_printable_protocol_edges
{

struct wrong_dynamic_size
{};

inline int print_reserve_size(::fast_io::io_reserve_type_t<char, wrong_dynamic_size>, wrong_dynamic_size) noexcept
{
	return 1;
}

inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_dynamic_size>, char *iter, wrong_dynamic_size) noexcept
{
	*iter = 'x';
	return iter + 1u;
}

struct cursor_proxy
{
	char *pointer;
	inline constexpr operator char *() const noexcept
	{
		return pointer;
	}
};

struct wrong_dynamic_cursor
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_dynamic_cursor>, wrong_dynamic_cursor) noexcept
{
	return 1u;
}

inline constexpr cursor_proxy print_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_dynamic_cursor>, char *iter, wrong_dynamic_cursor) noexcept
{
	return {iter + 1u};
}

template <typename type>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char, type>) noexcept
	requires(::std::same_as<type, struct wrong_precise_size> ||
			 ::std::same_as<type, struct wrong_precise_define> ||
			 ::std::same_as<type, struct wrong_static_precise_type> ||
			 ::std::same_as<type, struct oversized_static_precise>)
{
	return 1u;
}

template <typename type>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, type>, char *iter, type) noexcept
	requires(::std::same_as<type, struct wrong_precise_size> ||
			 ::std::same_as<type, struct wrong_precise_define> ||
			 ::std::same_as<type, struct wrong_static_precise_type> ||
			 ::std::same_as<type, struct oversized_static_precise>)
{
	*iter = 'p';
	return iter + 1u;
}

struct wrong_precise_size
{};

inline constexpr int print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, wrong_precise_size>, wrong_precise_size) noexcept
{
	return 1;
}

inline constexpr void print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, wrong_precise_size>, char *, ::std::size_t,
	wrong_precise_size) noexcept
{}

struct wrong_precise_define
{};

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, wrong_precise_define>, wrong_precise_define) noexcept
{
	return 1u;
}

inline constexpr int print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, wrong_precise_define>, char *, ::std::size_t,
	wrong_precise_define) noexcept
{
	return 0;
}

struct wrong_static_precise_type
{};

inline constexpr int print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<char, wrong_static_precise_type>) noexcept
{
	return 1;
}

struct oversized_static_precise
{};

inline constexpr ::std::size_t print_reserve_static_precise_size(
	::fast_io::io_reserve_type_t<char, oversized_static_precise>) noexcept
{
	return static_cast<::std::size_t>(PTRDIFF_MAX);
}

struct wrong_stack_hint
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, wrong_stack_hint>, wrong_stack_hint) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, wrong_stack_hint>, char *iter, wrong_stack_hint) noexcept
{
	return iter + 1u;
}

inline constexpr int print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, wrong_stack_hint>) noexcept
{
	return 16;
}

struct wrong_context_window;

struct wrong_context_window_state
{
	inline constexpr ::fast_io::context_print_result<char *> print_context_define(
		wrong_context_window, char *first, char *) noexcept;
};

struct wrong_context_window
{};

inline constexpr ::fast_io::context_print_result<char *>
wrong_context_window_state::print_context_define(wrong_context_window, char *first, char *) noexcept
{
	return {first, true};
}

inline constexpr auto print_context_type(
	::fast_io::io_reserve_type_t<char, wrong_context_window>) noexcept
{
	return ::fast_io::io_type_t<wrong_context_window_state>{};
}

inline constexpr int print_context_static_buffer_size(
	::fast_io::io_reserve_type_t<char, wrong_context_window>) noexcept
{
	return 16;
}

struct wrong_direct_print
{};

template <typename output>
inline int print_define(
	::fast_io::io_reserve_type_t<char, wrong_direct_print>, output, wrong_direct_print) noexcept
{
	return 0;
}

struct wrong_status_output
{
	using output_char_type = char;
};

template <bool>
inline int status_print_define(wrong_status_output, wrong_direct_print) noexcept
{
	return 0;
}

struct wrong_input_cursor
{
	using input_char_type = char;
};

inline char const *ibuffer_begin(wrong_input_cursor) noexcept { return nullptr; }
inline char const *ibuffer_curr(wrong_input_cursor) noexcept { return nullptr; }
inline char *ibuffer_end(wrong_input_cursor) noexcept { return nullptr; }
inline void ibuffer_set_curr(wrong_input_cursor, char const *) noexcept {}
inline bool ibuffer_underflow(wrong_input_cursor) noexcept { return false; }

struct wrong_input_commit
{
	using input_char_type = char;
};

inline char const *ibuffer_begin(wrong_input_commit) noexcept { return nullptr; }
inline char const *ibuffer_curr(wrong_input_commit) noexcept { return nullptr; }
inline char const *ibuffer_end(wrong_input_commit) noexcept { return nullptr; }
inline int ibuffer_set_curr(wrong_input_commit, char const *) noexcept { return 0; }
inline bool ibuffer_underflow(wrong_input_commit) noexcept { return false; }

struct valid_const_input
{
	using input_char_type = char;
};

inline char const *ibuffer_begin(valid_const_input) noexcept { return nullptr; }
inline char const *ibuffer_curr(valid_const_input) noexcept { return nullptr; }
inline char const *ibuffer_end(valid_const_input) noexcept { return nullptr; }
inline void ibuffer_set_curr(valid_const_input, char const *) noexcept {}
inline bool ibuffer_underflow(valid_const_input) noexcept { return false; }

struct wrong_volatile_input
{
	using input_char_type = char;
};

inline char volatile *ibuffer_begin(wrong_volatile_input) noexcept { return nullptr; }
inline char volatile *ibuffer_curr(wrong_volatile_input) noexcept { return nullptr; }
inline char volatile *ibuffer_end(wrong_volatile_input) noexcept { return nullptr; }
inline void ibuffer_set_curr(wrong_volatile_input, char volatile *) noexcept {}
inline bool ibuffer_underflow(wrong_volatile_input) noexcept { return false; }

struct valid_input_minimum
{
	using input_char_type = char;
};

inline constexpr ::std::size_t ibuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, valid_input_minimum>) noexcept
{
	return 8u;
}
inline constexpr void ibuffer_minimum_size_underflow_all_prepare_define(valid_input_minimum) noexcept {}

struct runtime_input_minimum
{
	using input_char_type = char;
};

inline ::std::size_t ibuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, runtime_input_minimum>) noexcept
{
	return 8u;
}
inline constexpr void ibuffer_minimum_size_underflow_all_prepare_define(runtime_input_minimum) noexcept {}

struct zero_input_minimum
{
	using input_char_type = char;
};

inline constexpr ::std::size_t ibuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, zero_input_minimum>) noexcept
{
	return 0u;
}
inline constexpr void ibuffer_minimum_size_underflow_all_prepare_define(zero_input_minimum) noexcept {}

struct wrong_input_minimum_prepare
{
	using input_char_type = char;
};

inline constexpr ::std::size_t ibuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, wrong_input_minimum_prepare>) noexcept
{
	return 8u;
}
inline constexpr int ibuffer_minimum_size_underflow_all_prepare_define(wrong_input_minimum_prepare) noexcept
{
	return 0;
}

struct wrong_output_cursor
{
	using output_char_type = char;
};

inline char *obuffer_begin(wrong_output_cursor) noexcept { return nullptr; }
inline char *obuffer_curr(wrong_output_cursor) noexcept { return nullptr; }
inline char const *obuffer_end(wrong_output_cursor) noexcept { return nullptr; }
inline void obuffer_set_curr(wrong_output_cursor, char *) noexcept {}

struct wrong_char_put
{
	using output_char_type = char;
};

inline int output_stream_char_put_overflow_define(wrong_char_put, char) noexcept { return 0; }

struct lvalue_alias_target
{};
struct lvalue_alias_proxy
{};
inline lvalue_alias_proxy scan_alias_define(::fast_io::io_alias_t, lvalue_alias_target &) noexcept { return {}; }

struct wrong_rewind_state
{};
struct wrong_rewind_arg
{};
inline int scan_context_eof_rewind_size(
	::fast_io::io_reserve_type_t<char, wrong_rewind_arg>, wrong_rewind_state &, wrong_rewind_arg &) noexcept
{
	return 1;
}

struct escaped_contiguous_target
{};
struct escaped_contiguous_proxy
{};
inline escaped_contiguous_proxy scan_alias_define(
	::fast_io::io_alias_t, escaped_contiguous_target &) noexcept { return {}; }
inline ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, escaped_contiguous_proxy>, char const *, char const *last,
	escaped_contiguous_proxy &) noexcept
{
	return {last + 1u, ::fast_io::parse_code::ok};
}

struct eof_partial_state
{};
struct eof_partial_target
{};
struct eof_partial_proxy
{};
inline eof_partial_proxy scan_alias_define(::fast_io::io_alias_t, eof_partial_target &) noexcept { return {}; }
inline constexpr auto scan_context_type(
	::fast_io::io_reserve_type_t<char, eof_partial_proxy>) noexcept
{
	return ::fast_io::io_type_t<eof_partial_state>{};
}
inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, eof_partial_proxy>, eof_partial_state &, char const *first,
	char const *, eof_partial_proxy &) noexcept
{
	return {first, ::fast_io::parse_code::partial};
}
inline ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, eof_partial_proxy>, eof_partial_state &, eof_partial_proxy &) noexcept
{
	return ::fast_io::parse_code::partial;
}

struct zero_precise_target
{
	bool received_valid_pointer{};
};
struct zero_precise_proxy
{
	zero_precise_target *target;
};
inline zero_precise_proxy scan_alias_define(::fast_io::io_alias_t, zero_precise_target &target) noexcept
{
	return {__builtin_addressof(target)};
}
inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, zero_precise_proxy>) noexcept
{
	return 0u;
}
inline void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, zero_precise_proxy>, char const *buffer,
	zero_precise_proxy &proxy) noexcept
{
	proxy.target->received_valid_pointer = buffer != nullptr;
}

struct empty_success_refill_source
{
	using input_char_type = char;
	char storage{'x'};
	char const *current{__builtin_addressof(storage)};
	char const *end{__builtin_addressof(storage) + 1u};
	::std::size_t underflows{};
};

struct empty_success_refill_ref
{
	using input_char_type = char;
	empty_success_refill_source *source;
};

inline empty_success_refill_ref input_stream_ref_define(empty_success_refill_source &source) noexcept
{
	return {__builtin_addressof(source)};
}
inline char const *ibuffer_begin(empty_success_refill_ref ref) noexcept
{
	return __builtin_addressof(ref.source->storage);
}
inline char const *ibuffer_curr(empty_success_refill_ref ref) noexcept { return ref.source->current; }
inline char const *ibuffer_end(empty_success_refill_ref ref) noexcept { return ref.source->end; }
inline void ibuffer_set_curr(empty_success_refill_ref ref, char const *current) noexcept
{
	ref.source->current = current;
}
inline bool ibuffer_underflow(empty_success_refill_ref ref) noexcept
{
	++ref.source->underflows;
	ref.source->current = ref.source->end;
	return true;
}

struct consume_then_partial_state
{};
struct consume_then_partial_target
{};
struct consume_then_partial_proxy
{};
inline consume_then_partial_proxy scan_alias_define(
	::fast_io::io_alias_t, consume_then_partial_target &) noexcept { return {}; }
inline constexpr auto scan_context_type(
	::fast_io::io_reserve_type_t<char, consume_then_partial_proxy>) noexcept
{
	return ::fast_io::io_type_t<consume_then_partial_state>{};
}
inline ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, consume_then_partial_proxy>, consume_then_partial_state &,
	char const *, char const *last, consume_then_partial_proxy &) noexcept
{
	return {last, ::fast_io::parse_code::partial};
}
inline ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, consume_then_partial_proxy>, consume_then_partial_state &,
	consume_then_partial_proxy &) noexcept
{
	return ::fast_io::parse_code::partial;
}

static_assert(!::fast_io::dynamic_reserve_printable<char, wrong_dynamic_size>);
static_assert(!::fast_io::dynamic_reserve_printable<char, wrong_dynamic_cursor>);
static_assert(!::fast_io::precise_reserve_printable<char, wrong_precise_size>);
static_assert(!::fast_io::precise_reserve_printable<char, wrong_precise_define>);
static_assert(!::fast_io::static_precise_reserve_printable<char, wrong_static_precise_type>);
static_assert(!::fast_io::static_precise_reserve_printable<char, oversized_static_precise>);
static_assert(!::fast_io::dynamic_reserve_with_possible_static_stack_size<char, wrong_stack_hint>);
static_assert(::fast_io::context_printable<char, wrong_context_window>);
static_assert(!::fast_io::context_printable_with_static_buffer_size<char, wrong_context_window>);
static_assert(!::fast_io::printable<char, wrong_direct_print>);
static_assert(!::fast_io::details::direct_printable_to<char, wrong_status_output, wrong_direct_print>);
static_assert(!::fast_io::operations::decay::defines::has_status_print_define<
	false, wrong_status_output, wrong_direct_print>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_basic_operations<wrong_input_cursor>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_basic_operations<wrong_input_commit>);
static_assert(::fast_io::operations::decay::defines::has_ibuffer_basic_operations<valid_const_input>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_basic_operations<wrong_volatile_input>);
static_assert(::fast_io::operations::decay::defines::has_ibuffer_minimum_size_operations<valid_input_minimum>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_minimum_size_operations<runtime_input_minimum>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_minimum_size_operations<zero_input_minimum>);
static_assert(!::fast_io::operations::decay::defines::has_ibuffer_minimum_size_operations<wrong_input_minimum_prepare>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_basic_operations<wrong_output_cursor>);
static_assert(!::fast_io::operations::decay::defines::has_output_stream_char_put_overflow_define<wrong_char_put>);
static_assert(::fast_io::alias_scannable<lvalue_alias_target &>);
static_assert(!::fast_io::alias_scannable<lvalue_alias_target>);
static_assert(!::fast_io::details::scan_context_eof_rewindable<
	char, wrong_rewind_arg, wrong_rewind_state>);

inline bool throws_invalid(auto &&operation)
{
#if defined(__cpp_exceptions)
	try
	{
		operation();
	}
	catch (::fast_io::error const &error)
	{
		return error == ::fast_io::parse_code::invalid;
	}
	return false;
#else
	(void)operation;
	return true;
#endif
}

} // namespace scan_printable_protocol_edges

int main()
{
	using namespace scan_printable_protocol_edges;
	char storage[4]{'a', 'b', 'c', 'd'};
	{
		// `basic_ibuffer_view` exposes read-only cursors. Primitive scalar, byte, and scatter reads must preserve that
		// cursor representation while copying into mutable caller-owned destinations.
		::fast_io::basic_ibuffer_view<char> input(storage, storage + 4u);
		char scalar[2]{};
		::fast_io::operations::read_all(input, scalar, scalar + 2u);
		assert(scalar[0] == 'a' && scalar[1] == 'b');

		::std::byte bytes[1]{};
		::fast_io::operations::read_all_bytes(input, bytes, bytes + 1u);
		assert(bytes[0] == static_cast<::std::byte>('c'));

		char scatter_character{};
		::fast_io::basic_io_scatter_t<char> scatter{__builtin_addressof(scatter_character), 1u};
		::fast_io::operations::scatter_read_all(input, __builtin_addressof(scatter), 1u);
		assert(scatter_character == 'd');
	}
	escaped_contiguous_target escaped;
	auto const escaped_result{::fast_io::parse_by_scan(storage + 1u, storage + 2u, escaped)};
	assert(escaped_result.iter == storage + 1u);
	assert(escaped_result.code == ::fast_io::parse_code::invalid);

	eof_partial_target eof_partial;
	auto const eof_result{::fast_io::parse_by_scan(storage, storage, eof_partial)};
	assert(eof_result.iter == storage);
	assert(eof_result.code == ::fast_io::parse_code::invalid);

	zero_precise_target zero;
	char const *const null_pointer{};
	auto const zero_result{::fast_io::parse_by_scan(null_pointer, null_pointer, zero)};
	assert(zero_result.iter == nullptr && zero_result.code == ::fast_io::parse_code::ok);
	assert(zero.received_valid_pointer);

	empty_success_refill_source source;
	consume_then_partial_target partial;
	assert(throws_invalid([&] { (void)::fast_io::io::scan<true>(source, partial); }));
	assert(source.underflows == 1u);
}
