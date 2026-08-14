#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <span>
#include <string>
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

} // namespace

int main()
{
	using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
	using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

	auto const name{::std::string{"fast_io_alpc_repeated_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out |
						::fast_io::ipc_mode::message};
	alpc_server server{name, mode};
	::std::atomic_bool client_done{};
	::std::exception_ptr server_exception;
	constexpr ::std::size_t count{128u};

	::std::thread server_thread{[&] {
		try
		{
			auto endpoint{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, endpoint, true);
			::fast_io::nt_alpc_ipc_message request;
			for (::std::size_t index{}; index != count; ++index)
			{
				::fast_io::alpc_receive(server, request);
				require(::fast_io::alpc_message_type(request) == ::fast_io::nt_alpc_ipc_message_type::request);
				require(request.bytes.size() == sizeof(index));
				::fast_io::alpc_send(endpoint, {request.bytes.data(), request.bytes.size()});
			}
			while (!client_done.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
		}
		catch (...)
		{
			server_exception = ::std::current_exception();
		}
	}};

	alpc_client client{name, mode};
	::fast_io::nt_alpc_ipc_message response;
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const bytes{::std::span<::std::byte const>{
			reinterpret_cast<::std::byte const *>(__builtin_addressof(index)), sizeof(index)}};
		::fast_io::alpc_send(client, bytes);
		::fast_io::alpc_receive(client, response);
		require(response.bytes.size() == sizeof(index));
		::std::size_t echoed{};
		::fast_io::freestanding::my_memcpy(__builtin_addressof(echoed), response.bytes.data(), sizeof(echoed));
		require(echoed == index);
	}
	client_done.store(true, ::std::memory_order_release);
	server_thread.join();
	if (server_exception)
	{
		::std::rethrow_exception(server_exception);
	}
}
