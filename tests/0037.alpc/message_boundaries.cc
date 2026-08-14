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
#include <vector>

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

inline bool message_equals(::fast_io::nt_alpc_ipc_message const &message, ::std::string_view value) noexcept
{
	return message.bytes.size() == value.size() &&
		   (value.empty() || ::std::memcmp(message.bytes.data(), value.data(), value.size()) == 0);
}

} // namespace

int main()
{
	using namespace ::std::chrono_literals;
	using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
	using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

	auto const name{::std::string{"fast_io_alpc_boundaries_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out | ::fast_io::ipc_mode::message};
	alpc_server server{name, mode};
	::std::exception_ptr client_exception;

	::std::thread client_thread{[&] {
		try
		{
			alpc_client client{name, mode};
			::fast_io::alpc_send(client, {});
			::fast_io::alpc_send(client, as_bytes("first"));
			::fast_io::alpc_send(client, as_bytes("second"));

			::std::vector<::std::byte> maximum_message(::fast_io::alpc_max_message_size());
			for (::std::size_t index{}; index != maximum_message.size(); ++index)
			{
				maximum_message[index] = static_cast<::std::byte>(index);
			}
			::fast_io::alpc_send(client, maximum_message);

			::fast_io::nt_alpc_ipc_message response;
			require(::fast_io::alpc_receive_for(client, response, 2s));
			require(message_equals(response, "ack"));

			maximum_message.push_back({});
			bool rejected{};
			try
			{
				::fast_io::alpc_send(client, maximum_message);
			}
			catch (...)
			{
				rejected = true;
			}
			require(rejected);
		}
		catch (...)
		{
			client_exception = ::std::current_exception();
		}
	}};

	auto connection_request{::fast_io::alpc_receive(server)};
	require(::fast_io::alpc_message_type(connection_request) ==
			::fast_io::nt_alpc_ipc_message_type::connection_request);
	auto connection{::fast_io::accept_connect(server, connection_request, true)};

	auto empty{::fast_io::alpc_receive(server)};
	require(::fast_io::alpc_message_type(empty) == ::fast_io::nt_alpc_ipc_message_type::request);
	require(empty.port_context == connection.native_handle());
	require(empty.bytes.empty());

	auto first{::fast_io::alpc_receive(server)};
	require(message_equals(first, "first"));
	auto second{::fast_io::alpc_receive(server)};
	require(message_equals(second, "second"));
	require(first.message_id != second.message_id);

	auto maximum{::fast_io::alpc_receive(server)};
	require(maximum.bytes.size() == ::fast_io::alpc_max_message_size());
	for (::std::size_t index{}; index != maximum.bytes.size(); ++index)
	{
		require(maximum.bytes[index] == static_cast<::std::byte>(index));
	}
	::fast_io::alpc_send(connection, as_bytes("ack"));

	client_thread.join();
	if (client_exception)
	{
		::std::rethrow_exception(client_exception);
	}
}
