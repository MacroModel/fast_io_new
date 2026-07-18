#include <fast_io_legacy.h>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace print_file_protocol_test
{

struct opaque_record
{
	char value{};
};

struct status_state
{
	char value{};
	::std::size_t calls{};
	::std::size_t locks{};
	::std::size_t unlocks{};
	bool locked{};
};

struct status_only_sink
{
	using output_char_type = char;
	status_state *state{};
};

inline constexpr status_only_sink output_stream_ref_define(status_only_sink sink) noexcept
{
	return sink;
}

template <bool line>
	requires(!line)
inline void status_print_define(status_only_sink sink, opaque_record record) noexcept
{
	sink.state->value = record.value;
	++sink.state->calls;
}

struct unlocked_status_sink
{
	using output_char_type = char;
	status_state *state{};
};

struct locked_status_sink
{
	using output_char_type = char;
	status_state *state{};
};

struct mutex_proxy
{
	status_state *state{};

	inline void lock() noexcept
	{
		assert(!state->locked);
		state->locked = true;
		++state->locks;
	}

	inline void unlock() noexcept
	{
		assert(state->locked);
		state->locked = false;
		++state->unlocks;
	}
};

inline constexpr locked_status_sink output_stream_ref_define(locked_status_sink sink) noexcept
{
	return sink;
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

inline constexpr unlocked_status_sink output_stream_unlocked_ref_define(locked_status_sink sink) noexcept
{
	return {sink.state};
}

template <bool line>
inline void status_print_define(unlocked_status_sink sink, opaque_record record) noexcept
{
	assert(sink.state->locked);
	sink.state->value = record.value;
	++sink.state->calls;
	if constexpr (line)
	{
		++sink.state->value;
	}
}

struct missing_unlocked_sink
{
	using output_char_type = char;
};

inline constexpr missing_unlocked_sink output_stream_ref_define(missing_unlocked_sink sink) noexcept
{
	return sink;
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(missing_unlocked_sink) noexcept
{
	return {};
}

struct wide_unlocked_sink
{
	using output_char_type = wchar_t;
};

struct wrong_character_mutex_sink
{
	using output_char_type = char;
};

inline constexpr wrong_character_mutex_sink output_stream_ref_define(wrong_character_mutex_sink sink) noexcept
{
	return sink;
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(wrong_character_mutex_sink) noexcept
{
	return {};
}

inline constexpr wide_unlocked_sink output_stream_unlocked_ref_define(wrong_character_mutex_sink) noexcept
{
	return {};
}

struct self_unlocked_sink
{
	using output_char_type = char;
};

inline constexpr self_unlocked_sink output_stream_ref_define(self_unlocked_sink sink) noexcept
{
	return sink;
}

inline constexpr mutex_proxy output_stream_mutex_ref_define(self_unlocked_sink) noexcept
{
	return {};
}

inline constexpr self_unlocked_sink output_stream_unlocked_ref_define(self_unlocked_sink sink) noexcept
{
	return sink;
}

struct malformed_mutex_proxy
{
	inline int lock() noexcept
	{
		return 0;
	}

	inline void unlock() noexcept
	{}
};

struct malformed_mutex_sink
{
	using output_char_type = char;
};

struct malformed_mutex_unlocked_sink
{
	using output_char_type = char;
};

inline constexpr malformed_mutex_sink output_stream_ref_define(malformed_mutex_sink sink) noexcept
{
	return sink;
}

inline constexpr malformed_mutex_proxy output_stream_mutex_ref_define(malformed_mutex_sink) noexcept
{
	return {};
}

inline constexpr malformed_mutex_unlocked_sink
output_stream_unlocked_ref_define(malformed_mutex_sink) noexcept
{
	return {};
}

template <typename tag>
struct buffer_sink
{
	using output_char_type = char;
	char *begin{};
	char *current{};
	char *end{};
};

struct valid_buffer_tag
{};

struct const_begin_tag
{};

struct value_setter_tag
{};

template <typename tag>
inline constexpr char *obuffer_begin(buffer_sink<tag> sink) noexcept
{
	return sink.begin;
}

template <typename tag>
inline constexpr char *obuffer_curr(buffer_sink<tag> sink) noexcept
{
	return sink.current;
}

template <typename tag>
inline constexpr char *obuffer_end(buffer_sink<tag> sink) noexcept
{
	return sink.end;
}

template <typename tag>
inline constexpr void obuffer_set_curr(buffer_sink<tag>, char *) noexcept
{}

inline constexpr char const *obuffer_begin(buffer_sink<const_begin_tag> sink) noexcept
{
	return sink.begin;
}

inline constexpr int obuffer_set_curr(buffer_sink<value_setter_tag>, char *) noexcept
{
	return 0;
}

struct constexpr_minimum_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t obuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, constexpr_minimum_sink>) noexcept
{
	return 64u;
}

inline constexpr void obuffer_minimum_size_flush_prepare_define(constexpr_minimum_sink) noexcept
{}

struct runtime_minimum_sink
{
	using output_char_type = char;
};

inline ::std::size_t obuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, runtime_minimum_sink>) noexcept
{
	return 64u;
}

inline constexpr void obuffer_minimum_size_flush_prepare_define(runtime_minimum_sink) noexcept
{}

struct zero_minimum_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t obuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, zero_minimum_sink>) noexcept
{
	return 0u;
}

inline constexpr void obuffer_minimum_size_flush_prepare_define(zero_minimum_sink) noexcept
{}

struct invalid_refill_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t obuffer_minimum_size_define(
	::fast_io::io_reserve_type_t<char, invalid_refill_sink>) noexcept
{
	return 64u;
}

inline constexpr int obuffer_minimum_size_flush_prepare_define(invalid_refill_sink) noexcept
{
	return 0;
}

struct valid_reserve_sink
{
	using output_char_type = char;
};

inline constexpr void obuffer_flush_reserve_define(valid_reserve_sink, ::std::size_t) noexcept
{}

struct invalid_reserve_sink
{
	using output_char_type = char;
};

inline constexpr ::std::size_t obuffer_flush_reserve_define(
	invalid_reserve_sink, ::std::size_t size) noexcept
{
	return size;
}

static_assert(!::fast_io::printable<char, opaque_record>);
static_assert(::fast_io::operations::decay::defines::has_status_print_define<
	false, status_only_sink, opaque_record>);
static_assert(!::fast_io::operations::decay::defines::has_status_print_define<
	true, status_only_sink, opaque_record>);
static_assert(::fast_io::operations::defines::print_freestanding_okay_for_line<
	false, status_only_sink, opaque_record>);
static_assert(!::fast_io::operations::defines::print_freestanding_okay_for_line<
	true, status_only_sink, opaque_record>);

static_assert(::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	locked_status_sink>);
static_assert(::fast_io::operations::defines::print_freestanding_okay_for_line<
	true, locked_status_sink, opaque_record>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	missing_unlocked_sink>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	wrong_character_mutex_sink>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	self_unlocked_sink>);
static_assert(!::fast_io::operations::decay::defines::has_complete_output_stream_mutex_protocol<
	malformed_mutex_sink>);
static_assert(!::fast_io::operations::defines::print_freestanding_okay_for_line<
	false, missing_unlocked_sink, opaque_record>);

static_assert(::fast_io::operations::decay::defines::has_obuffer_basic_operations<
	buffer_sink<valid_buffer_tag>>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_basic_operations<
	buffer_sink<const_begin_tag>>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_basic_operations<
	buffer_sink<value_setter_tag>>);
static_assert(::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<
	constexpr_minimum_sink>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<
	runtime_minimum_sink>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<
	zero_minimum_sink>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<
	invalid_refill_sink>);
static_assert(::fast_io::operations::decay::defines::has_obuffer_flush_reserve_define<
	valid_reserve_sink>);
static_assert(!::fast_io::operations::decay::defines::has_obuffer_flush_reserve_define<
	invalid_reserve_sink>);

using posix_owner_output_ref = ::std::remove_cvref_t<decltype(
	::fast_io::operations::output_stream_ref(::std::declval<::fast_io::posix_file &>()))>;
using c_owner_output_ref = ::std::remove_cvref_t<decltype(
	::fast_io::operations::output_stream_ref(::std::declval<::fast_io::c_file &>()))>;
using filebuf_owner_output_ref = ::std::remove_cvref_t<decltype(
	::fast_io::operations::output_stream_ref(::std::declval<::fast_io::filebuf_file &>()))>;

// Owners are intentionally normalized to their non-owning operation layer. This prevents a file's lifetime and move
// API from entering the print strategy type and lets the POSIX/C/filebuf observer advertise the lowest valid primitive
// and cost policy without duplicating every customization on its owner.
static_assert(::std::same_as<posix_owner_output_ref, ::fast_io::posix_io_observer>);
static_assert(::std::same_as<c_owner_output_ref, ::fast_io::c_io_observer>);
static_assert(::std::same_as<filebuf_owner_output_ref, ::fast_io::filebuf_io_observer>);

} // namespace print_file_protocol_test

int main()
{
	using namespace print_file_protocol_test;

	status_state direct{};
	::fast_io::print(status_only_sink{__builtin_addressof(direct)}, opaque_record{'D'});
	assert(direct.value == 'D' && direct.calls == 1u);

	status_state locked{};
	::fast_io::println(locked_status_sink{__builtin_addressof(locked)}, opaque_record{'L'});
	assert(locked.value == 'M' && locked.calls == 1u);
	assert(locked.locks == 1u && locked.unlocks == 1u && !locked.locked);
}
