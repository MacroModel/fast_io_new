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

inline bool message_equals(::fast_io::nt_alpc_ipc_message const &message, ::std::string_view value) noexcept
{
	return bytes_equal({message.bytes.data(), message.bytes.size()}, value);
}

} // namespace

int main()
{
	using namespace ::std::chrono_literals;
	using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
	using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

	auto const name{::std::string{"fast_io_alpc_multi_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out | ::fast_io::ipc_mode::message};
	alpc_server server{name, mode};
	::std::array<::std::exception_ptr, 2> client_exceptions{};
	::std::array<::std::thread, 2> client_threads;

	for (::std::size_t index{}; index != client_threads.size(); ++index)
	{
		client_threads[index] = ::std::thread{[&, index] {
			try
			{
				auto const handshake{::std::string{"client-"} + static_cast<char>('0' + index)};
				::fast_io::containers::basic_string_view<char> handshake_view{
					handshake.data(), handshake.size()};
				alpc_client client{name, mode, handshake_view};

				auto const accepted{::std::string{"accepted-"} + static_cast<char>('0' + index)};
				::std::array<::std::byte, 10> accepted_bytes{};
				::fast_io::operations::read_all_bytes(
					client, accepted_bytes.data(), accepted_bytes.data() + accepted_bytes.size());
				require(bytes_equal(accepted_bytes, accepted));

				auto const payload{::std::string{"payload-"} + static_cast<char>('0' + index)};
				::std::thread sender{[&] { ::fast_io::alpc_send(client, as_bytes(payload)); }};
				sender.join();

				::fast_io::nt_alpc_ipc_message response;
				require(::fast_io::alpc_receive_for(client, response, 2s));
				auto const expected{::std::string{"reply-"} + static_cast<char>('0' + index)};
				require(message_equals(response, expected));
			}
			catch (...)
			{
				client_exceptions[index] = ::std::current_exception();
			}
		}};
	}

	::std::array<alpc_client, 2> connections;
	::std::size_t accepted_count{};
	::std::size_t received_count{};
	while (accepted_count != connections.size() || received_count != connections.size())
	{
		auto message{::fast_io::alpc_receive(server)};
		auto const type{::fast_io::alpc_message_type(message)};
		if (type == ::fast_io::nt_alpc_ipc_message_type::connection_request)
		{
			::std::size_t index{connections.size()};
			for (::std::size_t candidate{}; candidate != connections.size(); ++candidate)
			{
				auto const expected{::std::string{"client-"} + static_cast<char>('0' + candidate)};
				if (message_equals(message, expected))
				{
					index = candidate;
					break;
				}
			}
			require(index != connections.size());
			require(!connections[index]);
			auto const response{::std::string{"accepted-"} + static_cast<char>('0' + index)};
			connections[index] = ::fast_io::accept_connect(server, message, true, as_bytes(response));
			++accepted_count;
			continue;
		}
		if (type == ::fast_io::nt_alpc_ipc_message_type::client_died ||
			type == ::fast_io::nt_alpc_ipc_message_type::port_closed ||
			type == ::fast_io::nt_alpc_ipc_message_type::cancel)
		{
			continue;
		}
		require(type == ::fast_io::nt_alpc_ipc_message_type::request);
		::std::size_t index{connections.size()};
		for (::std::size_t candidate{}; candidate != connections.size(); ++candidate)
		{
			if (message.port_context == connections[candidate].native_handle())
			{
				index = candidate;
				break;
			}
		}
		require(index != connections.size());
		auto const expected{::std::string{"payload-"} + static_cast<char>('0' + index)};
		require(message_equals(message, expected));
		auto const response{::std::string{"reply-"} + static_cast<char>('0' + index)};
		::fast_io::alpc_send(connections[index], as_bytes(response));
		++received_count;
	}

	for (auto &thread : client_threads)
	{
		thread.join();
	}
	for (auto const &exception : client_exceptions)
	{
		if (exception)
		{
			::std::rethrow_exception(exception);
		}
	}
}
