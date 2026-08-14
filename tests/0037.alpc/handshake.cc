#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include <fast_io.h>
#include <fast_io_hosted/process/ipc/win32/alpc_nt.h>

namespace
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

inline ::std::span<::std::byte const> as_bytes(::std::string_view value) noexcept
{
	return {reinterpret_cast<::std::byte const *>(value.data()), value.size()};
}

inline bool bytes_equal(::std::span<::std::byte const> bytes, ::std::string_view value) noexcept
{
	return bytes.size() == value.size() &&
		   (value.empty() || ::std::memcmp(bytes.data(), value.data(), value.size()) == 0);
}

} // namespace

int main()
{
	using namespace ::std::chrono_literals;
	using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
	using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

	auto const name{::std::string{"fast_io_alpc_handshake_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out | ::fast_io::ipc_mode::message};
	alpc_server server{name, mode};
	::std::exception_ptr client_exception;

	::std::thread client_thread{[&] {
		try
		{
			::fast_io::containers::basic_string_view<char> request{"hello", 5u};
			alpc_client client{name, mode, request};
			::std::array<::std::byte, 5> response{};
			::fast_io::operations::read_all_bytes(
				client, response.data(), response.data() + response.size());
			require(bytes_equal(response, "world"));
			::fast_io::alpc_send(client, as_bytes("data"));
			::fast_io::nt_alpc_ipc_message acknowledgement;
			require(::fast_io::alpc_receive_for(client, acknowledgement, 2s));
			require(bytes_equal({acknowledgement.bytes.data(), acknowledgement.bytes.size()}, "ack"));
		}
		catch (...)
		{
			client_exception = ::std::current_exception();
		}
	}};

	auto connection{::fast_io::wait_for_connect(server)};
	::std::array<::std::byte, 5> request{};
	::fast_io::operations::read_all_bytes(server, request.data(), request.data() + request.size());
	require(bytes_equal(request, "hello"));
	auto const response{as_bytes("world")};
	::fast_io::operations::write_all_bytes(server, response.data(), response.data() + response.size());
	::fast_io::accept_connect(server, connection, true);

	auto message{::fast_io::alpc_receive(server)};
	require(bytes_equal({message.bytes.data(), message.bytes.size()}, "data"));
	::fast_io::alpc_send(connection, as_bytes("ack"));

	client_thread.join();
	if (client_exception)
	{
		::std::rethrow_exception(client_exception);
	}
}
