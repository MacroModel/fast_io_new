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
	using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
	using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

	auto const name{::std::string{"fast_io_alpc_sync_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out |
						::fast_io::ipc_mode::message | ::fast_io::ipc_mode::sync};
	alpc_server server{name, mode};
	::std::atomic_bool accepted{};
	::std::exception_ptr client_exception;

	::std::thread client_thread{[&] {
		try
		{
			alpc_client client{name, mode};
			while (!accepted.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
			auto response{::fast_io::alpc_request(client, as_bytes("request"))};
			require(message_equals(response, "reply"));
		}
		catch (...)
		{
			client_exception = ::std::current_exception();
		}
	}};

	auto client{::fast_io::wait_for_connect(server)};
	::fast_io::accept_connect(server, client, true);
	accepted.store(true, ::std::memory_order_release);

	auto request{::fast_io::alpc_receive(server)};
	require(message_equals(request, "request"));
	::fast_io::alpc_reply(server, request, as_bytes("reply"));

	client_thread.join();
	if (client_exception)
	{
		::std::rethrow_exception(client_exception);
	}
}
