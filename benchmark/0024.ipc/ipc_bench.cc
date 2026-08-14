#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fast_io.h>
#include <fast_io_hosted/process/ipc/win32/alpc_nt.h>
#include <fast_io_hosted/process/ipc/win32/named_pipe_win32.h>

namespace
{

using clock_type = ::std::chrono::steady_clock;
using nanoseconds = ::std::chrono::nanoseconds;

struct benchmark_config
{
	::std::size_t latency_iterations{5000u};
	::std::size_t warmup_iterations{500u};
	::std::size_t throughput_bytes{32u * 1024u * 1024u};
	::std::size_t setup_iterations{50u};
	::std::vector<::std::size_t> sizes{1u, 8u, 64u, 256u, 1024u, 4096u, 16384u};
	::std::vector<::std::string> transports{};
	bool csv{};
	bool setup{true};
	bool process{true};
	bool latency{true};
	bool throughput{true};
};

struct distribution
{
	double median_ns{};
	double p95_ns{};
	double p99_ns{};
};

struct benchmark_result
{
	::std::string benchmark;
	::std::string transport;
	::std::size_t payload_bytes{};
	::std::size_t iterations{};
	double median_ns{};
	double p95_ns{};
	double p99_ns{};
	double one_way_ns{};
	double mib_per_second{};
	double messages_per_second{};
};

[[noreturn]] void fail(::std::string const &message)
{
	throw ::std::runtime_error(message);
}

void require(bool condition, ::std::string const &message)
{
	if (!condition) [[unlikely]]
	{
		fail(message);
	}
}

::std::size_t parse_size(::std::string_view value, ::std::string_view option)
{
	::std::size_t result{};
	auto const conversion{::std::from_chars(value.data(), value.data() + value.size(), result)};
	if (conversion.ec != ::std::errc{} || conversion.ptr != value.data() + value.size())
	{
		fail(::std::string{"invalid value for "} + ::std::string{option} + ": " + ::std::string{value});
	}
	return result;
}

::std::vector<::std::string_view> split(::std::string_view value)
{
	::std::vector<::std::string_view> values;
	while (!value.empty())
	{
		auto const comma{value.find(',')};
		auto const item{value.substr(0u, comma)};
		if (!item.empty())
		{
			values.push_back(item);
		}
		if (comma == ::std::string_view::npos)
		{
			break;
		}
		value.remove_prefix(comma + 1u);
	}
	return values;
}

void parse_sizes(benchmark_config &config, ::std::string_view value)
{
	config.sizes.clear();
	for (auto const item : split(value))
	{
		auto const size{parse_size(item, "--sizes")};
		if (size == 0u)
		{
			fail("stream-pipe benchmarks require payload sizes greater than zero");
		}
		config.sizes.push_back(size);
	}
	if (config.sizes.empty())
	{
		fail("--sizes must contain at least one payload size");
	}
}

void parse_transports(benchmark_config &config, ::std::string_view value)
{
	config.transports.clear();
	for (auto const item : split(value))
	{
		config.transports.emplace_back(item);
	}
}

bool transport_enabled(benchmark_config const &config, ::std::string_view name)
{
	return config.transports.empty() ||
		   ::std::find(config.transports.cbegin(), config.transports.cend(), name) != config.transports.cend();
}

void print_usage(char const *program)
{
	::std::printf(
		"usage: %s [options]\n"
		"  --iterations=N          measured ping-pong operations (default 5000)\n"
		"  --warmup=N              untimed ping-pong operations (default 500)\n"
		"  --throughput-bytes=N    target bytes per throughput case (default 33554432)\n"
		"  --setup-iterations=N    setup/teardown samples (default 50)\n"
		"  --sizes=A,B,...         payload sizes (default 1,8,64,256,1024,4096,16384)\n"
		"  --transport=A,B,...     nt_thread,win32_thread,nt_process,win32_process,named_pipe_thread,\n"
		"                            named_pipe_process,alpc_async_thread,alpc_async_process,alpc_sync_thread\n"
		"  --no-process            omit child-process pipe cases\n"
		"  --no-setup              omit creation/connection cases\n"
		"  --latency-only          omit throughput and setup cases\n"
		"  --throughput-only       omit latency and setup cases\n"
		"  --csv                   machine-readable CSV output\n"
		"  --smoke                 short correctness/performance smoke run\n",
		program);
}

benchmark_config parse_arguments(int argc, char **argv)
{
	benchmark_config config;
	for (int index{1}; index != argc; ++index)
	{
		::std::string_view argument{argv[index]};
		auto value_after = [&](::std::string_view prefix) -> ::std::string_view {
			if (argument.starts_with(prefix))
			{
				return argument.substr(prefix.size());
			}
			return {};
		};
		if (argument == "--help" || argument == "-h")
		{
			print_usage(argv[0]);
			::std::exit(0);
		}
		else if (argument == "--csv")
		{
			config.csv = true;
		}
		else if (argument == "--no-process")
		{
			config.process = false;
		}
		else if (argument == "--no-setup")
		{
			config.setup = false;
		}
		else if (argument == "--latency-only")
		{
			config.throughput = false;
			config.setup = false;
		}
		else if (argument == "--throughput-only")
		{
			config.latency = false;
			config.setup = false;
		}
		else if (argument == "--smoke")
		{
			config.latency_iterations = 100u;
			config.warmup_iterations = 20u;
			config.throughput_bytes = 64u * 1024u;
			config.setup_iterations = 3u;
			config.sizes = {8u, 1024u};
		}
		else if (auto const iterations_value{value_after("--iterations=")}; !iterations_value.empty())
		{
			config.latency_iterations = parse_size(iterations_value, "--iterations");
		}
		else if (auto const warmup_value{value_after("--warmup=")}; !warmup_value.empty())
		{
			config.warmup_iterations = parse_size(warmup_value, "--warmup");
		}
		else if (auto const throughput_value{value_after("--throughput-bytes=")}; !throughput_value.empty())
		{
			config.throughput_bytes = parse_size(throughput_value, "--throughput-bytes");
		}
		else if (auto const setup_value{value_after("--setup-iterations=")}; !setup_value.empty())
		{
			config.setup_iterations = parse_size(setup_value, "--setup-iterations");
		}
		else if (auto const sizes_value{value_after("--sizes=")}; !sizes_value.empty())
		{
			parse_sizes(config, sizes_value);
		}
		else if (auto const transports_value{value_after("--transport=")}; !transports_value.empty())
		{
			parse_transports(config, transports_value);
		}
		else
		{
			fail(::std::string{"unknown option: "} + ::std::string{argument});
		}
	}
	if (config.latency_iterations == 0u || config.throughput_bytes == 0u ||
		(config.setup && config.setup_iterations == 0u))
	{
		fail("iteration and byte counts must be greater than zero");
	}
	return config;
}

::std::vector<::std::byte> make_payload(::std::size_t size)
{
	::std::vector<::std::byte> payload(size);
	for (::std::size_t index{}; index != size; ++index)
	{
		payload[index] = static_cast<::std::byte>((index * 131u + 17u) & 0xffu);
	}
	return payload;
}

bool bytes_equal(::std::span<::std::byte const> left, ::std::span<::std::byte const> right) noexcept
{
	return left.size() == right.size() &&
		   (left.empty() || ::std::memcmp(left.data(), right.data(), left.size()) == 0);
}

distribution analyze(::std::vector<::std::int64_t> samples)
{
	require(!samples.empty(), "cannot analyze an empty sample set");
	::std::sort(samples.begin(), samples.end());
	auto const percentile = [&](::std::size_t numerator) {
		auto const rank{(samples.size() * numerator + 99u) / 100u};
		auto const index{rank == 0u ? 0u : rank - 1u};
		return static_cast<double>(samples[index]);
	};
	return {percentile(50u), percentile(95u), percentile(99u)};
}

benchmark_result latency_result(::std::string transport, ::std::size_t size,
								::std::size_t iterations, ::std::vector<::std::int64_t> samples)
{
	auto const stats{analyze(::std::move(samples))};
	return {"latency", ::std::move(transport), size, iterations, stats.median_ns, stats.p95_ns,
			stats.p99_ns, stats.median_ns / 2.0, 0.0, 0.0};
}

benchmark_result throughput_result(::std::string transport, ::std::size_t size,
								   ::std::size_t iterations, nanoseconds elapsed)
{
	auto const seconds{static_cast<double>(elapsed.count()) / 1'000'000'000.0};
	auto const bytes{static_cast<double>(size) * static_cast<double>(iterations)};
	return {"throughput", ::std::move(transport), size, iterations, 0.0, 0.0, 0.0, 0.0,
			bytes / (1024.0 * 1024.0) / seconds, static_cast<double>(iterations) / seconds};
}

benchmark_result setup_result(::std::string transport, ::std::size_t iterations,
							  ::std::vector<::std::int64_t> samples)
{
	auto const stats{analyze(::std::move(samples))};
	return {"setup", ::std::move(transport), 0u, iterations, stats.median_ns, stats.p95_ns,
			stats.p99_ns, 0.0, 0.0, 0.0};
}

::std::size_t throughput_iterations(benchmark_config const &config, ::std::size_t size)
{
	auto messages{config.throughput_bytes / size};
	if (messages < 1000u)
	{
		messages = 1000u;
	}
	if (messages > 200000u)
	{
		messages = 200000u;
	}
	return messages;
}

template <typename input_type, typename output_type>
int child_pipe_worker(input_type input, output_type output, ::std::string_view operation,
					  ::std::size_t size, ::std::size_t iterations)
{
	auto expected{make_payload(size)};
	::std::vector<::std::byte> buffer(size);
	if (operation == "latency")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			::fast_io::operations::read_all_bytes(input, buffer.data(), buffer.data() + buffer.size());
			::fast_io::operations::write_all_bytes(output, buffer.data(), buffer.data() + buffer.size());
		}
	}
	else if (operation == "throughput")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			::fast_io::operations::read_all_bytes(input, buffer.data(), buffer.data() + buffer.size());
		}
		if (!bytes_equal(buffer, expected))
		{
			return 3;
		}
		::std::byte acknowledgement{::std::byte{0x5a}};
		::fast_io::operations::write_all_bytes(output, __builtin_addressof(acknowledgement),
											   __builtin_addressof(acknowledgement) + 1u);
	}
	else
	{
		return 2;
	}
	return 0;
}

int child_main(int argc, char **argv)
{
	if (argc >= 3 && ::std::string_view{argv[2]} == "noop")
	{
		return 0;
	}
	if (argc != 7)
	{
		return 2;
	}
	auto const family{::std::string_view{argv[2]}};
	auto const operation{::std::string_view{argv[3]}};
	auto const size{parse_size(argv[4], "child size")};
	auto const iterations{parse_size(argv[5], "child iterations")};
	if (::std::string_view{argv[6]} != "v1")
	{
		return 2;
	}
	if (family == "nt")
	{
		return child_pipe_worker(::fast_io::nt_stdin(), ::fast_io::nt_stdout(), operation, size, iterations);
	}
	if (family == "win32")
	{
		return child_pipe_worker(::fast_io::win32_stdin(), ::fast_io::win32_stdout(), operation, size, iterations);
	}
	return 2;
}

template <typename pipe_type>
benchmark_result bench_thread_pipe_latency(::std::string transport, ::std::size_t size,
										   ::std::size_t warmup, ::std::size_t iterations)
{
	pipe_type requests;
	pipe_type responses;
	auto payload{make_payload(size)};
	::std::vector<::std::byte> response(size);
	::std::vector<::std::byte> worker_buffer(size);
	::std::exception_ptr worker_exception;
	auto const total{warmup + iterations};
	::std::thread worker{[&] {
		try
		{
			for (::std::size_t index{}; index != total; ++index)
			{
				::fast_io::operations::read_all_bytes(requests.in(), worker_buffer.data(),
													  worker_buffer.data() + worker_buffer.size());
				::fast_io::operations::write_all_bytes(responses.out(), worker_buffer.data(),
													   worker_buffer.data() + worker_buffer.size());
			}
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::operations::write_all_bytes(requests.out(), payload.data(), payload.data() + payload.size());
		::fast_io::operations::read_all_bytes(responses.in(), response.data(), response.data() + response.size());
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(bytes_equal(response, payload), transport + " returned a corrupt latency payload");
	return latency_result(::std::move(transport), size, iterations, ::std::move(samples));
}

template <typename pipe_type>
benchmark_result bench_thread_pipe_throughput(::std::string transport, ::std::size_t size,
											  ::std::size_t iterations)
{
	pipe_type data;
	pipe_type acknowledgements;
	auto payload{make_payload(size)};
	::std::vector<::std::byte> worker_buffer(size);
	::std::exception_ptr worker_exception;
	::std::thread worker{[&] {
		try
		{
			for (::std::size_t index{}; index != iterations; ++index)
			{
				::fast_io::operations::read_all_bytes(data.in(), worker_buffer.data(),
													  worker_buffer.data() + worker_buffer.size());
			}
			require(bytes_equal(worker_buffer, payload), transport + " returned a corrupt throughput payload");
			::std::byte acknowledgement{::std::byte{0x5a}};
			::fast_io::operations::write_all_bytes(acknowledgements.out(), __builtin_addressof(acknowledgement),
												   __builtin_addressof(acknowledgement) + 1u);
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	::std::byte acknowledgement{};
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::operations::write_all_bytes(data.out(), payload.data(), payload.data() + payload.size());
	}
	::fast_io::operations::read_all_bytes(acknowledgements.in(), __builtin_addressof(acknowledgement),
										  __builtin_addressof(acknowledgement) + 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(acknowledgement == ::std::byte{0x5a}, transport + " returned a corrupt acknowledgement");
	return throughput_result(::std::move(transport), size, iterations, elapsed);
}

template <typename pipe_type, typename process_type, typename process_args_type>
benchmark_result bench_process_pipe_latency(::std::string transport, ::std::string_view family,
											::std::string const &executable, ::std::size_t size,
											::std::size_t warmup, ::std::size_t iterations)
{
	pipe_type requests;
	pipe_type responses;
	auto const total{warmup + iterations};
	process_args_type args{"--child", family, "latency", size, total, "v1"};
	process_type process{executable, args, {}, {.in = requests, .out = responses, .err = ::fast_io::err()}, ::fast_io::process_mode::none};
	requests.in().close();
	responses.out().close();
	auto payload{make_payload(size)};
	::std::vector<::std::byte> response(size);
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::operations::write_all_bytes(requests.out(), payload.data(), payload.data() + payload.size());
		::fast_io::operations::read_all_bytes(responses.in(), response.data(), response.data() + response.size());
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, transport + " child failed");
	require(bytes_equal(response, payload), transport + " returned a corrupt latency payload");
	return latency_result(::std::move(transport), size, iterations, ::std::move(samples));
}

template <typename pipe_type, typename process_type, typename process_args_type>
benchmark_result bench_process_pipe_throughput(::std::string transport, ::std::string_view family,
											   ::std::string const &executable, ::std::size_t size,
											   ::std::size_t iterations)
{
	pipe_type data;
	pipe_type acknowledgements;
	process_args_type args{"--child", family, "throughput", size, iterations, "v1"};
	process_type process{executable, args, {}, {.in = data, .out = acknowledgements, .err = ::fast_io::err()}, ::fast_io::process_mode::none};
	data.in().close();
	acknowledgements.out().close();
	auto payload{make_payload(size)};
	::std::byte acknowledgement{};
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::operations::write_all_bytes(data.out(), payload.data(), payload.data() + payload.size());
	}
	::fast_io::operations::read_all_bytes(acknowledgements.in(), __builtin_addressof(acknowledgement),
										  __builtin_addressof(acknowledgement) + 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, transport + " child failed");
	require(acknowledgement == ::std::byte{0x5a}, transport + " returned a corrupt acknowledgement");
	return throughput_result(::std::move(transport), size, iterations, elapsed);
}

::std::string unique_ipc_name(::std::string_view prefix)
{
	static ::std::size_t sequence{};
	return ::std::string{prefix} + ::std::to_string(::fast_io::win32::GetCurrentProcessId()) + "_" +
		   ::std::to_string(++sequence);
}

inline constexpr auto duplex_mode{::fast_io::ipc_mode::in | ::fast_io::ipc_mode::out |
								  ::fast_io::ipc_mode::message};

benchmark_result bench_named_pipe_latency(::std::size_t size, ::std::size_t warmup,
										  ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_named_")};
	::fast_io::win32_named_pipe_ipc_server server{name, duplex_mode};
	auto payload{make_payload(size)};
	::std::vector<::std::byte> response(size);
	::std::vector<::std::byte> worker_buffer(size);
	::std::exception_ptr worker_exception;
	auto const total{warmup + iterations};
	::std::thread worker{[&] {
		try
		{
			auto pending{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, pending, true);
			for (::std::size_t index{}; index != total; ++index)
			{
				::fast_io::operations::read_all_bytes(server, worker_buffer.data(),
													  worker_buffer.data() + worker_buffer.size());
				::fast_io::operations::write_all_bytes(server, worker_buffer.data(),
													   worker_buffer.data() + worker_buffer.size());
			}
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	::fast_io::win32_named_pipe_ipc_client client{name, duplex_mode};
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::operations::write_all_bytes(client, payload.data(), payload.data() + payload.size());
		::fast_io::operations::read_all_bytes(client, response.data(), response.data() + response.size());
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(bytes_equal(response, payload), "named_pipe_thread returned a corrupt latency payload");
	return latency_result("named_pipe_thread", size, iterations, ::std::move(samples));
}

benchmark_result bench_named_pipe_throughput(::std::size_t size, ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_named_")};
	::fast_io::win32_named_pipe_ipc_server server{name, duplex_mode};
	auto payload{make_payload(size)};
	::std::vector<::std::byte> worker_buffer(size);
	::std::exception_ptr worker_exception;
	::std::thread worker{[&] {
		try
		{
			auto pending{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, pending, true);
			for (::std::size_t index{}; index != iterations; ++index)
			{
				::fast_io::operations::read_all_bytes(server, worker_buffer.data(),
													  worker_buffer.data() + worker_buffer.size());
			}
			require(bytes_equal(worker_buffer, payload), "named_pipe_thread returned a corrupt throughput payload");
			::std::byte acknowledgement{::std::byte{0x5a}};
			::fast_io::operations::write_all_bytes(server, __builtin_addressof(acknowledgement),
												   __builtin_addressof(acknowledgement) + 1u);
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	::fast_io::win32_named_pipe_ipc_client client{name, duplex_mode};
	::std::byte acknowledgement{};
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::operations::write_all_bytes(client, payload.data(), payload.data() + payload.size());
	}
	::fast_io::operations::read_all_bytes(client, __builtin_addressof(acknowledgement),
										  __builtin_addressof(acknowledgement) + 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(acknowledgement == ::std::byte{0x5a}, "named_pipe_thread returned a corrupt acknowledgement");
	return throughput_result("named_pipe_thread", size, iterations, elapsed);
}

using alpc_server = ::fast_io::basic_nt_family_alpc_ipc_server<::fast_io::nt_family::nt, char>;
using alpc_client = ::fast_io::basic_nt_family_alpc_ipc_client<::fast_io::nt_family::nt, char>;

bool is_alpc_data_message(::fast_io::nt_alpc_ipc_message const &message) noexcept
{
	auto const type{::fast_io::alpc_message_type(message)};
	return type == ::fast_io::nt_alpc_ipc_message_type::request ||
		   type == ::fast_io::nt_alpc_ipc_message_type::reply ||
		   type == ::fast_io::nt_alpc_ipc_message_type::datagram;
}

template <typename port_type>
void receive_alpc_data(port_type &port, ::fast_io::nt_alpc_ipc_message &message, ::std::size_t expected_size)
{
	for (;;)
	{
		::fast_io::alpc_receive(port, message);
		if (is_alpc_data_message(message))
		{
			require(message.bytes.size() == expected_size, "ALPC returned an unexpected message size");
			return;
		}
	}
}

template <typename client_type>
client_type connect_ipc_with_retry(::std::string const &name, ::fast_io::ipc_mode mode)
{
	auto const deadline{clock_type::now() + ::std::chrono::seconds{5}};
	for (;;)
	{
		try
		{
			return client_type{name, mode};
		}
		catch (...)
		{
			if (clock_type::now() >= deadline)
			{
				throw;
			}
			::std::this_thread::sleep_for(::std::chrono::milliseconds{1});
		}
	}
}

int named_pipe_child_worker(::std::string const &name, ::std::string_view operation,
							::std::size_t size, ::std::size_t iterations)
{
	::fast_io::win32_named_pipe_ipc_server server{name, duplex_mode};
	auto pending{::fast_io::wait_for_connect(server)};
	::fast_io::accept_connect(server, pending, true);
	auto expected{make_payload(size)};
	::std::vector<::std::byte> buffer(size);
	if (operation == "setup")
	{
	}
	else if (operation == "latency")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			::fast_io::operations::read_all_bytes(server, buffer.data(), buffer.data() + buffer.size());
			::fast_io::operations::write_all_bytes(server, buffer.data(), buffer.data() + buffer.size());
		}
	}
	else if (operation == "throughput")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			::fast_io::operations::read_all_bytes(server, buffer.data(), buffer.data() + buffer.size());
		}
		::std::byte acknowledgement{::std::byte{0x5a}};
		::fast_io::operations::write_all_bytes(server, __builtin_addressof(acknowledgement),
											   __builtin_addressof(acknowledgement) + 1u);
	}
	else
	{
		return 2;
	}
	if (operation != "setup" && !bytes_equal(buffer, expected))
	{
		return 3;
	}
	::std::byte completion{};
	::fast_io::operations::read_all_bytes(server, __builtin_addressof(completion),
										  __builtin_addressof(completion) + 1u);
	return completion == ::std::byte{0x33} ? 0 : 3;
}

int alpc_async_child_worker(::std::string const &name, ::std::string_view operation,
							::std::size_t size, ::std::size_t iterations)
{
	alpc_server server{name, duplex_mode};
	auto endpoint{::fast_io::wait_for_connect(server)};
	::fast_io::accept_connect(server, endpoint, true);
	auto expected{make_payload(size)};
	::fast_io::nt_alpc_ipc_message request;
	if (operation == "setup")
	{
	}
	else if (operation == "latency")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			receive_alpc_data(server, request, size);
			::fast_io::alpc_send(endpoint, {request.bytes.data(), request.bytes.size()});
		}
	}
	else if (operation == "throughput")
	{
		for (::std::size_t index{}; index != iterations; ++index)
		{
			receive_alpc_data(server, request, size);
		}
		::std::byte acknowledgement{::std::byte{0x5a}};
		::fast_io::alpc_send(endpoint, {__builtin_addressof(acknowledgement), 1u});
	}
	else
	{
		return 2;
	}
	if (operation != "setup" && !bytes_equal(request.bytes, expected))
	{
		return 3;
	}
	receive_alpc_data(server, request, 1u);
	return request.bytes[0] == ::std::byte{0x33} ? 0 : 3;
}

int ipc_child_main(int argc, char **argv)
{
	if (argc != 8 || ::std::string_view{argv[7]} != "v1")
	{
		return 2;
	}
	auto const transport{::std::string_view{argv[2]}};
	auto const operation{::std::string_view{argv[3]}};
	::std::string const name{argv[4]};
	auto const size{parse_size(argv[5], "IPC child size")};
	auto const iterations{parse_size(argv[6], "IPC child iterations")};
	if (transport == "named")
	{
		return named_pipe_child_worker(name, operation, size, iterations);
	}
	if (transport == "alpc_async")
	{
		return alpc_async_child_worker(name, operation, size, iterations);
	}
	return 2;
}

benchmark_result bench_named_pipe_process_latency(::std::string const &executable, ::std::size_t size,
												  ::std::size_t warmup, ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_named_process_")};
	auto const total{warmup + iterations};
	::fast_io::nt_process_args args{"--ipc-child", "named", "latency", name, size, total, "v1"};
	::fast_io::nt_process process{executable, args};
	auto client{connect_ipc_with_retry<::fast_io::win32_named_pipe_ipc_client>(name, duplex_mode)};
	auto payload{make_payload(size)};
	::std::vector<::std::byte> response(size);
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::operations::write_all_bytes(client, payload.data(), payload.data() + payload.size());
		::fast_io::operations::read_all_bytes(client, response.data(), response.data() + response.size());
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	::std::byte completion{::std::byte{0x33}};
	::fast_io::operations::write_all_bytes(client, __builtin_addressof(completion),
										   __builtin_addressof(completion) + 1u);
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, "named_pipe_process child failed");
	require(bytes_equal(response, payload), "named_pipe_process returned a corrupt latency payload");
	return latency_result("named_pipe_process", size, iterations, ::std::move(samples));
}

benchmark_result bench_named_pipe_process_throughput(::std::string const &executable, ::std::size_t size,
													 ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_named_process_")};
	::fast_io::nt_process_args args{"--ipc-child", "named", "throughput", name, size, iterations, "v1"};
	::fast_io::nt_process process{executable, args};
	auto client{connect_ipc_with_retry<::fast_io::win32_named_pipe_ipc_client>(name, duplex_mode)};
	auto payload{make_payload(size)};
	::std::byte acknowledgement{};
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::operations::write_all_bytes(client, payload.data(), payload.data() + payload.size());
	}
	::fast_io::operations::read_all_bytes(client, __builtin_addressof(acknowledgement),
										  __builtin_addressof(acknowledgement) + 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	::std::byte completion{::std::byte{0x33}};
	::fast_io::operations::write_all_bytes(client, __builtin_addressof(completion),
										   __builtin_addressof(completion) + 1u);
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, "named_pipe_process child failed");
	require(acknowledgement == ::std::byte{0x5a}, "named_pipe_process returned a corrupt acknowledgement");
	return throughput_result("named_pipe_process", size, iterations, elapsed);
}

benchmark_result bench_alpc_async_process_latency(::std::string const &executable, ::std::size_t size,
												  ::std::size_t warmup, ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_alpc_process_")};
	auto const total{warmup + iterations};
	::fast_io::nt_process_args args{"--ipc-child", "alpc_async", "latency", name, size, total, "v1"};
	::fast_io::nt_process process{executable, args};
	auto client{connect_ipc_with_retry<alpc_client>(name, duplex_mode)};
	auto payload{make_payload(size)};
	::fast_io::nt_alpc_ipc_message response;
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::alpc_send(client, payload);
		receive_alpc_data(client, response, size);
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	::std::byte completion{::std::byte{0x33}};
	::fast_io::alpc_send(client, {__builtin_addressof(completion), 1u});
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, "alpc_async_process child failed");
	require(bytes_equal(response.bytes, payload), "alpc_async_process returned a corrupt latency payload");
	return latency_result("alpc_async_process", size, iterations, ::std::move(samples));
}

benchmark_result bench_alpc_async_process_throughput(::std::string const &executable, ::std::size_t size,
													 ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_alpc_process_")};
	::fast_io::nt_process_args args{"--ipc-child", "alpc_async", "throughput", name, size, iterations, "v1"};
	::fast_io::nt_process process{executable, args};
	auto client{connect_ipc_with_retry<alpc_client>(name, duplex_mode)};
	auto payload{make_payload(size)};
	::fast_io::nt_alpc_ipc_message acknowledgement;
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::alpc_send(client, payload);
	}
	receive_alpc_data(client, acknowledgement, 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	::std::byte completion{::std::byte{0x33}};
	::fast_io::alpc_send(client, {__builtin_addressof(completion), 1u});
	auto const status{::fast_io::wait(process)};
	require(::fast_io::wait_status_to_int(status) == 0, "alpc_async_process child failed");
	require(acknowledgement.bytes[0] == ::std::byte{0x5a},
			"alpc_async_process returned a corrupt acknowledgement");
	return throughput_result("alpc_async_process", size, iterations, elapsed);
}

benchmark_result bench_alpc_async_latency(::std::size_t size, ::std::size_t warmup,
										  ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_alpc_")};
	alpc_server server{name, duplex_mode};
	auto payload{make_payload(size)};
	::std::exception_ptr worker_exception;
	::std::atomic_bool client_done{};
	auto const total{warmup + iterations};
	::std::thread worker{[&] {
		try
		{
			auto endpoint{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, endpoint, true);
			::fast_io::nt_alpc_ipc_message request;
			for (::std::size_t index{}; index != total; ++index)
			{
				receive_alpc_data(server, request, size);
				::fast_io::alpc_send(endpoint, {request.bytes.data(), request.bytes.size()});
			}
			while (!client_done.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	alpc_client client{name, duplex_mode};
	::fast_io::nt_alpc_ipc_message response;
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::alpc_send(client, payload);
		receive_alpc_data(client, response, size);
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	client_done.store(true, ::std::memory_order_release);
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(bytes_equal(response.bytes, payload), "alpc_async_thread returned a corrupt latency payload");
	return latency_result("alpc_async_thread", size, iterations, ::std::move(samples));
}

benchmark_result bench_alpc_async_throughput(::std::size_t size, ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_alpc_")};
	alpc_server server{name, duplex_mode};
	auto payload{make_payload(size)};
	::std::exception_ptr worker_exception;
	::std::atomic_bool client_done{};
	::std::thread worker{[&] {
		try
		{
			auto endpoint{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, endpoint, true);
			::fast_io::nt_alpc_ipc_message request;
			for (::std::size_t index{}; index != iterations; ++index)
			{
				receive_alpc_data(server, request, size);
			}
			require(bytes_equal(request.bytes, payload), "alpc_async_thread returned a corrupt throughput payload");
			::std::byte acknowledgement{::std::byte{0x5a}};
			::fast_io::alpc_send(endpoint, {__builtin_addressof(acknowledgement), 1u});
			while (!client_done.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	alpc_client client{name, duplex_mode};
	::fast_io::nt_alpc_ipc_message acknowledgement;
	auto const start{clock_type::now()};
	for (::std::size_t index{}; index != iterations; ++index)
	{
		::fast_io::alpc_send(client, payload);
	}
	receive_alpc_data(client, acknowledgement, 1u);
	auto const elapsed{::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start)};
	client_done.store(true, ::std::memory_order_release);
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(acknowledgement.bytes[0] == ::std::byte{0x5a}, "alpc_async_thread returned a corrupt acknowledgement");
	return throughput_result("alpc_async_thread", size, iterations, elapsed);
}

benchmark_result bench_alpc_sync_latency(::std::size_t size, ::std::size_t warmup,
										 ::std::size_t iterations)
{
	auto const name{unique_ipc_name("fast_io_bench_alpc_sync_")};
	auto constexpr mode{duplex_mode | ::fast_io::ipc_mode::sync};
	alpc_server server{name, mode};
	auto payload{make_payload(size)};
	::std::exception_ptr worker_exception;
	::std::atomic_bool client_done{};
	auto const total{warmup + iterations};
	::std::thread worker{[&] {
		try
		{
			auto endpoint{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, endpoint, true);
			::fast_io::nt_alpc_ipc_message request;
			for (::std::size_t index{}; index != total; ++index)
			{
				receive_alpc_data(server, request, size);
				::fast_io::alpc_reply(server, request, {request.bytes.data(), request.bytes.size()});
			}
			while (!client_done.load(::std::memory_order_acquire))
			{
				::std::this_thread::yield();
			}
		}
		catch (...)
		{
			worker_exception = ::std::current_exception();
		}
	}};
	alpc_client client{name, mode};
	::fast_io::nt_alpc_ipc_message response;
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != total; ++index)
	{
		auto const start{index >= warmup ? clock_type::now() : clock_type::time_point{}};
		::fast_io::alpc_request(client, payload, response);
		require(response.bytes.size() == size, "alpc_sync_thread returned an unexpected message size");
		if (index >= warmup)
		{
			samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
		}
	}
	client_done.store(true, ::std::memory_order_release);
	worker.join();
	if (worker_exception)
	{
		::std::rethrow_exception(worker_exception);
	}
	require(bytes_equal(response.bytes, payload), "alpc_sync_thread returned a corrupt latency payload");
	return latency_result("alpc_sync_thread", size, iterations, ::std::move(samples));
}

template <typename pipe_type>
benchmark_result bench_pipe_setup(::std::string transport, ::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const start{clock_type::now()};
		{
			pipe_type first;
			pipe_type second;
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result(::std::move(transport), iterations, ::std::move(samples));
}

template <typename process_type, typename process_args_type>
benchmark_result bench_process_setup(::std::string transport, ::std::string const &executable,
									 ::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const start{clock_type::now()};
		{
			process_args_type args{"--child", "noop"};
			process_type process{executable, args};
			auto const status{::fast_io::wait(process)};
			require(::fast_io::wait_status_to_int(status) == 0, transport + " setup child failed");
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result(::std::move(transport), iterations, ::std::move(samples));
}

benchmark_result bench_named_pipe_process_setup(::std::string const &executable, ::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const name{unique_ipc_name("fast_io_bench_named_process_setup_")};
		auto const start{clock_type::now()};
		{
			::fast_io::nt_process_args args{"--ipc-child", "named", "setup", name, 1u, 0u, "v1"};
			::fast_io::nt_process process{executable, args};
			auto client{connect_ipc_with_retry<::fast_io::win32_named_pipe_ipc_client>(name, duplex_mode)};
			::std::byte completion{::std::byte{0x33}};
			::fast_io::operations::write_all_bytes(client, __builtin_addressof(completion),
												   __builtin_addressof(completion) + 1u);
			auto const status{::fast_io::wait(process)};
			require(::fast_io::wait_status_to_int(status) == 0, "named pipe process setup child failed");
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result("named_pipe_process_start_connect_close", iterations, ::std::move(samples));
}

benchmark_result bench_alpc_process_setup(::std::string const &executable, ::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const name{unique_ipc_name("fast_io_bench_alpc_process_setup_")};
		auto const start{clock_type::now()};
		{
			::fast_io::nt_process_args args{"--ipc-child", "alpc_async", "setup", name, 1u, 0u, "v1"};
			::fast_io::nt_process process{executable, args};
			auto client{connect_ipc_with_retry<alpc_client>(name, duplex_mode)};
			::std::byte completion{::std::byte{0x33}};
			::fast_io::alpc_send(client, {__builtin_addressof(completion), 1u});
			auto const status{::fast_io::wait(process)};
			require(::fast_io::wait_status_to_int(status) == 0, "ALPC process setup child failed");
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result("alpc_process_start_connect_close", iterations, ::std::move(samples));
}

benchmark_result bench_named_pipe_setup(::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const name{unique_ipc_name("fast_io_bench_named_setup_")};
		auto const start{clock_type::now()};
		{
			::fast_io::win32_named_pipe_ipc_server server{name, duplex_mode};
			::std::exception_ptr client_exception;
			::std::thread client_thread{[&] {
				try
				{
					::fast_io::win32_named_pipe_ipc_client client{name, duplex_mode};
				}
				catch (...)
				{
					client_exception = ::std::current_exception();
				}
			}};
			auto pending{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, pending, true);
			client_thread.join();
			if (client_exception)
			{
				::std::rethrow_exception(client_exception);
			}
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result("named_pipe_thread_connect_close", iterations, ::std::move(samples));
}

benchmark_result bench_alpc_setup(::std::size_t iterations)
{
	::std::vector<::std::int64_t> samples;
	samples.reserve(iterations);
	for (::std::size_t index{}; index != iterations; ++index)
	{
		auto const name{unique_ipc_name("fast_io_bench_alpc_setup_")};
		auto const start{clock_type::now()};
		{
			alpc_server server{name, duplex_mode};
			::std::exception_ptr client_exception;
			::std::thread client_thread{[&] {
				try
				{
					alpc_client client{name, duplex_mode};
				}
				catch (...)
				{
					client_exception = ::std::current_exception();
				}
			}};
			auto endpoint{::fast_io::wait_for_connect(server)};
			::fast_io::accept_connect(server, endpoint, true);
			client_thread.join();
			if (client_exception)
			{
				::std::rethrow_exception(client_exception);
			}
		}
		samples.push_back(::std::chrono::duration_cast<nanoseconds>(clock_type::now() - start).count());
	}
	return setup_result("alpc_thread_connect_close", iterations, ::std::move(samples));
}

void print_results(::std::vector<benchmark_result> const &results, bool csv)
{
	if (csv)
	{
		::std::puts("benchmark,transport,payload_bytes,iterations,median_ns,p95_ns,p99_ns,one_way_estimate_ns,mib_per_second,messages_per_second");
		for (auto const &result : results)
		{
			::std::printf("%s,%s,%zu,%zu,%.3f,%.3f,%.3f,%.3f,%.6f,%.3f\n",
						  result.benchmark.c_str(), result.transport.c_str(), result.payload_bytes, result.iterations,
						  result.median_ns, result.p95_ns, result.p99_ns, result.one_way_ns,
						  result.mib_per_second, result.messages_per_second);
		}
		return;
	}

	::std::puts("fast_io Windows IPC benchmark\n");
	::std::puts("Latency (round trip; one-way is median/2 estimate)");
	::std::puts("transport          bytes   iterations   median_us     p95_us     p99_us   one_way_us");
	for (auto const &result : results)
	{
		if (result.benchmark == "latency")
		{
			::std::printf("%-18s %7zu %12zu %11.3f %10.3f %10.3f %12.3f\n",
						  result.transport.c_str(), result.payload_bytes, result.iterations, result.median_ns / 1000.0,
						  result.p95_ns / 1000.0, result.p99_ns / 1000.0, result.one_way_ns / 1000.0);
		}
	}

	::std::puts("\nOne-way throughput (includes final receiver acknowledgement)");
	::std::puts("transport          bytes     messages      MiB/s       messages/s");
	for (auto const &result : results)
	{
		if (result.benchmark == "throughput")
		{
			::std::printf("%-18s %7zu %12zu %10.2f %16.0f\n", result.transport.c_str(),
						  result.payload_bytes, result.iterations, result.mib_per_second, result.messages_per_second);
		}
	}

	::std::puts("\nSetup/teardown");
	::std::puts("operation                         samples   median_us     p95_us     p99_us");
	for (auto const &result : results)
	{
		if (result.benchmark == "setup")
		{
			::std::printf("%-32s %8zu %11.3f %10.3f %10.3f\n", result.transport.c_str(), result.iterations,
						  result.median_ns / 1000.0, result.p95_ns / 1000.0, result.p99_ns / 1000.0);
		}
	}
}

} // namespace

int main(int argc, char **argv)
{
	try
	{
		if (argc >= 2 && ::std::string_view{argv[1]} == "--child")
		{
			return child_main(argc, argv);
		}
		if (argc >= 2 && ::std::string_view{argv[1]} == "--ipc-child")
		{
			return ipc_child_main(argc, argv);
		}
		auto const config{parse_arguments(argc, argv)};
		::std::string const executable{argv[0]};
		::std::vector<benchmark_result> results;

		for (auto const size : config.sizes)
		{
			if (config.latency && transport_enabled(config, "nt_thread"))
			{
				results.push_back(bench_thread_pipe_latency<::fast_io::nt_pipe>(
					"nt_thread", size, config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && transport_enabled(config, "win32_thread"))
			{
				results.push_back(bench_thread_pipe_latency<::fast_io::win32_pipe>(
					"win32_thread", size, config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && config.process && transport_enabled(config, "nt_process"))
			{
				results.push_back(bench_process_pipe_latency<::fast_io::nt_pipe, ::fast_io::nt_process,
															 ::fast_io::nt_process_args>("nt_process", "nt", executable, size,
																						 config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && config.process && transport_enabled(config, "win32_process"))
			{
				results.push_back(bench_process_pipe_latency<::fast_io::win32_pipe, ::fast_io::win32_process,
															 ::fast_io::win32_process_args>("win32_process", "win32", executable, size,
																							config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && transport_enabled(config, "named_pipe_thread"))
			{
				results.push_back(bench_named_pipe_latency(size, config.warmup_iterations,
														   config.latency_iterations));
			}
			if (config.latency && config.process && transport_enabled(config, "named_pipe_process"))
			{
				results.push_back(bench_named_pipe_process_latency(executable, size,
																   config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && size <= ::fast_io::alpc_max_message_size() && transport_enabled(config, "alpc_async_thread"))
			{
				results.push_back(bench_alpc_async_latency(size, config.warmup_iterations,
														   config.latency_iterations));
			}
			if (config.latency && config.process && size <= ::fast_io::alpc_max_message_size() &&
				transport_enabled(config, "alpc_async_process"))
			{
				results.push_back(bench_alpc_async_process_latency(executable, size,
																   config.warmup_iterations, config.latency_iterations));
			}
			if (config.latency && size <= ::fast_io::alpc_max_message_size() && transport_enabled(config, "alpc_sync_thread"))
			{
				results.push_back(bench_alpc_sync_latency(size, config.warmup_iterations,
														  config.latency_iterations));
			}

			auto const throughput_count{throughput_iterations(config, size)};
			if (config.throughput && transport_enabled(config, "nt_thread"))
			{
				results.push_back(bench_thread_pipe_throughput<::fast_io::nt_pipe>(
					"nt_thread", size, throughput_count));
			}
			if (config.throughput && transport_enabled(config, "win32_thread"))
			{
				results.push_back(bench_thread_pipe_throughput<::fast_io::win32_pipe>(
					"win32_thread", size, throughput_count));
			}
			if (config.throughput && config.process && transport_enabled(config, "nt_process"))
			{
				results.push_back(bench_process_pipe_throughput<::fast_io::nt_pipe, ::fast_io::nt_process,
																::fast_io::nt_process_args>("nt_process", "nt", executable, size, throughput_count));
			}
			if (config.throughput && config.process && transport_enabled(config, "win32_process"))
			{
				results.push_back(bench_process_pipe_throughput<::fast_io::win32_pipe, ::fast_io::win32_process,
																::fast_io::win32_process_args>("win32_process", "win32", executable, size, throughput_count));
			}
			if (config.throughput && transport_enabled(config, "named_pipe_thread"))
			{
				results.push_back(bench_named_pipe_throughput(size, throughput_count));
			}
			if (config.throughput && config.process && transport_enabled(config, "named_pipe_process"))
			{
				results.push_back(bench_named_pipe_process_throughput(executable, size, throughput_count));
			}
			if (config.throughput && size <= ::fast_io::alpc_max_message_size() && transport_enabled(config, "alpc_async_thread"))
			{
				results.push_back(bench_alpc_async_throughput(size, throughput_count));
			}
			if (config.throughput && config.process && size <= ::fast_io::alpc_max_message_size() &&
				transport_enabled(config, "alpc_async_process"))
			{
				results.push_back(bench_alpc_async_process_throughput(executable, size, throughput_count));
			}
		}

		if (config.setup)
		{
			if (transport_enabled(config, "nt_thread") || transport_enabled(config, "nt_process"))
			{
				results.push_back(bench_pipe_setup<::fast_io::nt_pipe>("nt_pipe_duplex_create_close",
																	   config.setup_iterations));
			}
			if (transport_enabled(config, "win32_thread") || transport_enabled(config, "win32_process"))
			{
				results.push_back(bench_pipe_setup<::fast_io::win32_pipe>("win32_pipe_duplex_create_close",
																		  config.setup_iterations));
			}
			if (config.process && (transport_enabled(config, "nt_process") ||
								   transport_enabled(config, "named_pipe_process") || transport_enabled(config, "alpc_async_process")))
			{
				results.push_back(bench_process_setup<::fast_io::nt_process, ::fast_io::nt_process_args>(
					"nt_process_spawn_exit", executable, config.setup_iterations));
			}
			if (config.process && transport_enabled(config, "win32_process"))
			{
				results.push_back(bench_process_setup<::fast_io::win32_process, ::fast_io::win32_process_args>(
					"win32_process_spawn_exit", executable, config.setup_iterations));
			}
			if (transport_enabled(config, "named_pipe_thread"))
			{
				results.push_back(bench_named_pipe_setup(config.setup_iterations));
			}
			if (config.process && transport_enabled(config, "named_pipe_process"))
			{
				results.push_back(bench_named_pipe_process_setup(executable, config.setup_iterations));
			}
			if (transport_enabled(config, "alpc_async_thread") || transport_enabled(config, "alpc_sync_thread"))
			{
				results.push_back(bench_alpc_setup(config.setup_iterations));
			}
			if (config.process && transport_enabled(config, "alpc_async_process"))
			{
				results.push_back(bench_alpc_process_setup(executable, config.setup_iterations));
			}
		}

		print_results(results, config.csv);
	}
	catch (::fast_io::error error)
	{
		::fast_io::io::perrln("fast_io error: ", error);
		return 1;
	}
	catch (::std::exception const &error)
	{
		::std::fprintf(stderr, "error: %s\n", error.what());
		return 1;
	}
}
