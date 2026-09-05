#include <cassert>
#include <cstddef>
#include <type_traits>

#include <fast_io.h>

namespace native_observer_value_transport
{

using posix_observer = ::fast_io::basic_posix_family_io_observer<
	::fast_io::posix_family::api, char>;

// The semantic marker and the ABI classifier are separate obligations. This
// assertion proves that the concrete descriptor observer supplies the former;
// the equality proves that only the target-specific small-argument policy can
// still decline value transport.
static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
	posix_observer>);
static_assert(
	::fast_io::operations::defines::abi_value_io_stream_ref_result<
		posix_observer &> ==
	::fast_io::details::abi_small_trivial_argument_object<posix_observer>());
static_assert(::fast_io::details::abi_value_direct_read_some_bytes<
	posix_observer>);
static_assert(::fast_io::details::abi_value_direct_write_some_bytes<
	posix_observer>);
static_assert(::fast_io::details::abi_value_direct_pread_some_bytes<
	posix_observer>);
static_assert(::fast_io::details::abi_value_direct_pwrite_some_bytes<
	posix_observer>);
static_assert(::fast_io::details::abi_value_direct_scatter_read_some_bytes<
	posix_observer>);
static_assert(::fast_io::details::abi_value_direct_scatter_write_some_bytes<
	posix_observer>);

#if defined(_WIN32) || defined(__CYGWIN__)
using nt_observer = ::fast_io::basic_nt_family_io_observer<
	::fast_io::nt_family::nt, char>;
using win32_observer = ::fast_io::basic_win32_family_io_observer<
	::fast_io::win32_family::native, char>;

static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
	nt_observer>);
static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
	win32_observer>);
static_assert(
	::fast_io::operations::defines::abi_value_io_stream_ref_result<
		nt_observer &> ==
	::fast_io::details::abi_small_trivial_argument_object<nt_observer>());
static_assert(
	::fast_io::operations::defines::abi_value_io_stream_ref_result<
		win32_observer &> ==
	::fast_io::details::abi_small_trivial_argument_object<win32_observer>());
#endif

} // namespace native_observer_value_transport

int main()
{
	// Exercise the real public formatting and input APIs through a pipe. Both
	// normalizations produce descriptor observers by value, while the kernel
	// pipe remains the single shared stream state observed by both copies.
	::fast_io::posix_pipe channel;
	::fast_io::io::print(channel, "fd");
	char received[2]{};
	::fast_io::operations::read_all(channel, received, received + 2);
	assert(received[0] == 'f' && received[1] == 'd');

	// Native byte scatter exercises the direct POSIX provider plus its scalar
	// short-transfer completion leaf. Both copies retain one kernel pipe state.
	char const first_payload[]{'a', 'b'};
	char const second_payload[]{'c', 'd', 'e'};
	::fast_io::io_scatter_t const output_scatters[]{
		{first_payload, sizeof(first_payload)},
		{second_payload, sizeof(second_payload)}};
	::fast_io::operations::scatter_write_all_bytes(
		channel, output_scatters, 2u);

	char first_received[2]{};
	char second_received[3]{};
	::fast_io::io_scatter_t const input_scatters[]{
		{first_received, sizeof(first_received)},
		{second_received, sizeof(second_received)}};
	::fast_io::operations::scatter_read_all_bytes(
		channel, input_scatters, 2u);
	assert(first_received[0] == 'a' && first_received[1] == 'b');
	assert(second_received[0] == 'c' && second_received[1] == 'd' &&
		   second_received[2] == 'e');
}
