#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

#include <fast_io.h>

#if defined(_WIN32) && !defined(__WINE__)
#include <fast_io_hosted/process/ipc/win32/alpc_nt.h>
#include <fast_io_hosted/process/ipc/win32/named_pipe_win32.h>
#endif

static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char, ::fast_io::basic_general_io_io_observer<char, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_barrier_plan_stream<
		char, ::fast_io::basic_general_io_io_observer<char, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char8_t,
		::fast_io::basic_general_io_io_observer<char16_t, char8_t>>);

#if !defined(_WIN32) || defined(__CYGWIN__) || defined(__WINE__)
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char, ::fast_io::basic_posix_io_observer<char>>);
static_assert(
	::fast_io::semantic_optional_scatter_barrier_plan_stream<
		char, ::fast_io::basic_posix_io_observer<char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char, ::fast_io::basic_native_socket_io_observer<char>>);
#else
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char, ::fast_io::basic_win32_socket_io_observer<char>>);
static_assert(
	::fast_io::semantic_optional_scatter_barrier_plan_stream<
		char, ::fast_io::basic_win32_socket_io_observer<char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char,
		::fast_io::basic_win32_family_socket_io_observer<
			::fast_io::win32_family::wide_nt, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char,
		::fast_io::basic_nt_family_alpc_ipc_universal_observer<
			::fast_io::nt_family::nt, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_barrier_plan_stream<
		char,
		::fast_io::basic_nt_family_alpc_ipc_universal_observer<
			::fast_io::nt_family::nt, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char,
		::fast_io::basic_nt_family_alpc_ipc_universal_observer<
			::fast_io::nt_family::zw, char>>);
static_assert(
	::fast_io::semantic_optional_scatter_plan_stream<
		char,
		::fast_io::basic_win32_named_pipe_ipc_server_observer<char>>);
#endif

// C FILE, streambuf, buffering, decoration, and transcoding layers require separate provider proofs and must not gain
// these exact-observer markers merely because their underlying OS handle can normalize to a marked platform observer.
static_assert(
	!::fast_io::semantic_optional_scatter_plan_stream<
		char, ::fast_io::basic_c_io_observer<char>>);

namespace
{

struct capture_sink
{
	using output_char_type = char;
	::std::string *output{};
};

struct message_sink
{
	using output_char_type = char;
	::std::string *output{};
	::std::string *operations{};
};

struct message_barrier
{};

struct unmarked_message_barrier
{};

inline constexpr capture_sink output_stream_ref_define(
	capture_sink sink) noexcept
{
	return sink;
}

inline constexpr message_sink output_stream_ref_define(
	message_sink sink) noexcept
{
	return sink;
}

inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char, capture_sink>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char, message_sink>) noexcept
{
	// This fixture deliberately has no scatter or coalescing policy, matching message-oriented platform observers.
	return {};
}

inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char, message_sink>) noexcept
{
	// A direct-only barrier preserves the fixture's individual write boundaries and ordered destination state.
	return {};
}

inline constexpr ::std::true_type
print_semantic_optional_scatter_barrier_leaf(
	::fast_io::io_reserve_type_t<char, message_barrier>) noexcept
{
	return {};
}

inline void write_all_overflow_define(
	capture_sink sink, char const *first, char const *last)
{
	sink.output->append(first, static_cast<::std::size_t>(last - first));
}

inline void write_all_overflow_define(
	message_sink sink, char const *first, char const *last)
{
	sink.operations->push_back('w');
	sink.output->append(first, static_cast<::std::size_t>(last - first));
}

template <::std::integral char_type, typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type, message_barrier>,
	output &&out, message_barrier)
{
	::fast_io::operations::print_freestanding<false>(
		::std::forward<output>(out), "<B>");
}

template <::std::integral char_type, typename output>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<char_type, unmarked_message_barrier>,
	output &&out, unmarked_message_barrier)
{
	::fast_io::operations::print_freestanding<false>(
		::std::forward<output>(out), "<B>");
}

inline void scatter_write_all_overflow_define(
	capture_sink sink,
	::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count)
{
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.output->append(
			scatters[index].base, scatters[index].len);
	}
}

} // namespace

int main()
{
	::std::string output;
	::fast_io::io_file erased_file{
		::fast_io::io_cookie, capture_sink{__builtin_addressof(output)}};
	::fast_io::io_io_observer observer{erased_file.native_handle()};
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		output.clear();
		::fast_io::io::print(
			observer,
			::fast_io::mnp::cond((mask & 1u) != 0u, "A"), "|",
			::fast_io::mnp::cond((mask & 2u) != 0u, "B"), "|",
			::fast_io::mnp::cond((mask & 4u) != 0u, "C"), "|",
			::fast_io::mnp::cond((mask & 8u) != 0u, "D"));
		::std::string expected;
		if ((mask & 1u) != 0u)
		{
			expected.push_back('A');
		}
		expected.push_back('|');
		if ((mask & 2u) != 0u)
		{
			expected.push_back('B');
		}
		expected.push_back('|');
		if ((mask & 4u) != 0u)
		{
			expected.push_back('C');
		}
		expected.push_back('|');
		if ((mask & 8u) != 0u)
		{
			expected.push_back('D');
		}
		assert(output == expected);
	}

	::std::string marked_output;
	::std::string marked_operations;
	::std::string reference_output;
	::std::string reference_operations;
	message_sink marked_sink{
		__builtin_addressof(marked_output),
		__builtin_addressof(marked_operations)};
	message_sink reference_sink{
		__builtin_addressof(reference_output),
		__builtin_addressof(reference_operations)};
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		marked_output.clear();
		marked_operations.clear();
		reference_output.clear();
		reference_operations.clear();
		::fast_io::io::print(
			marked_sink,
			::fast_io::mnp::cond((mask & 1u) != 0u, "A"), "|",
			::fast_io::mnp::cond((mask & 2u) != 0u, "B"),
			message_barrier{},
			::fast_io::mnp::cond((mask & 4u) != 0u, "C"), "|",
			::fast_io::mnp::cond((mask & 8u) != 0u, "D"));
		::fast_io::io::print(
			reference_sink,
			::fast_io::mnp::cond((mask & 1u) != 0u, "A"), "|",
			::fast_io::mnp::cond((mask & 2u) != 0u, "B"),
			unmarked_message_barrier{},
			::fast_io::mnp::cond((mask & 4u) != 0u, "C"), "|",
			::fast_io::mnp::cond((mask & 8u) != 0u, "D"));
		assert(marked_output == reference_output);
		assert(marked_operations == reference_operations);
	}
}
