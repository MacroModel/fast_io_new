#include <cassert>
#include <cstdlib>
#include <exception>
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

	auto const name{::std::string{"fast_io_alpc_reject_"} +
					::std::to_string(::fast_io::win32::GetCurrentProcessId())};
	auto constexpr mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out | ::fast_io::ipc_mode::message};
	alpc_server server{name, mode};
	bool rejected{};

	::std::thread client_thread{[&] {
		try
		{
			alpc_client client{name, mode};
		}
		catch (...)
		{
			rejected = true;
		}
	}};

	auto request{::fast_io::alpc_receive(server)};
	require(::fast_io::alpc_message_type(request) ==
			::fast_io::nt_alpc_ipc_message_type::connection_request);
	auto connection{::fast_io::accept_connect(server, request, false)};
	require(!connection);

	client_thread.join();
	require(rejected);
}
