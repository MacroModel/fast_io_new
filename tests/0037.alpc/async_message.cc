#include <atomic>
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

	auto const name{::std::string{"fast_io_alpc_async_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out |
						::fast_io::ipc_mode::message | ::fast_io::ipc_mode::no_block};
	alpc_server server{name, mode};
	::fast_io::win32_file completion_port{::fast_io::io_async};
	auto constexpr completion_key{static_cast<::std::size_t>(0xa11cu)};
	::fast_io::alpc_associate_io_completion_port(server, completion_port, completion_key);
	::std::atomic_bool associated{};
	::std::exception_ptr client_exception;

	::std::thread client_thread{[&] {
		try
		{
			alpc_client client{name, mode};
			while (!associated.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
			::fast_io::alpc_send(client, as_bytes("ping"));
			::fast_io::nt_alpc_ipc_message response;
			require(::fast_io::alpc_receive_for(client, response, 2s));
			require(message_equals(response, "pong"));
		}
		catch (...)
		{
			client_exception = ::std::current_exception();
		}
	}};

	::fast_io::nt_alpc_ipc_completion completion;
	require(::fast_io::alpc_wait_for(completion_port, completion, 2s));
	require(completion.completion_key == completion_key);
	auto client{::fast_io::wait_for_connect(server)};
	::fast_io::accept_connect(server, client, true);

	::fast_io::nt_alpc_ipc_message no_message;
	require(!::fast_io::alpc_try_receive(server, no_message));
	::std::byte no_stream_byte{};
	require(::fast_io::operations::read_some_bytes(server, __builtin_addressof(no_stream_byte),
												   __builtin_addressof(no_stream_byte) + 1) == __builtin_addressof(no_stream_byte));

	associated.store(true, ::std::memory_order_release);

	::fast_io::nt_alpc_ipc_message request;
	bool received{};
	for (unsigned attempts{}; attempts != 4u && !received; ++attempts)
	{
		require(::fast_io::alpc_wait_for(completion_port, completion, 2s));
		require(completion.completion_key == completion_key);
		received = ::fast_io::alpc_try_receive(server, request);
	}
	require(received);
	require(message_equals(request, "ping"));
	::fast_io::alpc_send(client, as_bytes("pong"));

	client_thread.join();
	if (client_exception)
	{
		::std::rethrow_exception(client_exception);
	}
}
