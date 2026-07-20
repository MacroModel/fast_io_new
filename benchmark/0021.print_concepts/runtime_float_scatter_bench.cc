#include <benchmark/benchmark.h>

#include <fast_io.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

namespace
{

enum class floating_case : std::int64_t
{
	general,
	fixed,
	runtime_precision
};

enum class transport_case
{
	contiguous_write,
	native_scatter,
	print_policy
};

// Volatile input is intentional: every timed conversion is a run-time floating conversion.  In particular, this
// benchmark must not accidentally measure print's independent compiler-constant replacement strategy.
double volatile runtime_value{3.2};
std::size_t volatile runtime_precision{2u};

struct fd_owner
{
	int fd{-1};

	fd_owner()
	{
		fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
		if (fd < 0)
		{
			std::perror("open(/dev/null)");
			std::abort();
		}
	}

	fd_owner(fd_owner const &) = delete;
	fd_owner &operator=(fd_owner const &) = delete;

	~fd_owner()
	{
		if (fd >= 0)
		{
			::close(fd);
		}
	}
};

[[nodiscard]] std::size_t format_floating(char *buffer, floating_case selected, double value,
										  std::size_t precision) noexcept
{
	::fast_io::obuffer_view output{buffer, buffer + 256u};
	switch (selected)
	{
	case floating_case::general:
		::fast_io::print(output, value);
		break;
	case floating_case::fixed:
		::fast_io::print(output, ::fast_io::mnp::fixed(value));
		break;
	case floating_case::runtime_precision:
		::fast_io::print(
			output,
			::fast_io::mnp::fixed<
				::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero>(
				value, precision));
		break;
	}
	return static_cast<std::size_t>(output.curr_ptr - buffer);
}

struct scatter_record
{
	std::array<::fast_io::basic_io_scatter_t<char>, 9u> scatters{};
	std::array<char, 8192u> filler{};
	std::array<char, 256u> floating{};
	std::size_t count{};
	std::size_t size{};
};

[[nodiscard]] scatter_record make_record(std::size_t requested_size, std::size_t count,
										 floating_case selected, double value, std::size_t precision)
{
	if (count < 2u || count > 9u || requested_size > 8192u)
	{
		std::abort();
	}
	scatter_record result;
	result.filler.fill('x');
	std::size_t const floating_size{
		format_floating(result.floating.data(), selected, value, precision)};
	if (requested_size < floating_size + count - 1u)
	{
		// Every descriptor is deliberately non-empty: empty iovecs make the advertised scatter count misleading and
		// do not model the print plans this benchmark is intended to calibrate.
		std::abort();
	}
	result.count = count;
	result.size = requested_size;
	std::size_t remaining{requested_size - floating_size};
	std::size_t filler_offset{};
	std::size_t const floating_index{count / 2u};
	for (std::size_t index{}; index != count; ++index)
	{
		if (index == floating_index)
		{
			result.scatters[index] = {result.floating.data(), floating_size};
			continue;
		}
		std::size_t const remaining_descriptors{
			count - index - (floating_index >= index ? 1u : 0u)};
		std::size_t const length{remaining / remaining_descriptors};
		result.scatters[index] = {result.filler.data() + filler_offset, length};
		filler_offset += length;
		remaining -= length;
	}
	return result;
}

[[nodiscard]] char *materialize(scatter_record const &record, char *destination) noexcept
{
	for (std::size_t index{}; index != record.count; ++index)
	{
		auto const [base, length]{record.scatters[index]};
		destination = static_cast<char *>(
						  std::memcpy(destination, base, length)) +
					  length;
	}
	return destination;
}

void validate_record(scatter_record const &record, int fd)
{
	std::array<char, 8192u> first{};
	std::array<char, 8192u> second{};
	char *const first_end{materialize(record, first.data())};
	char *second_end{second.data()};
	for (std::size_t index{}; index != record.count; ++index)
	{
		auto const [base, length]{record.scatters[index]};
		second_end = std::copy_n(base, length, second_end);
	}
	if (first_end != first.data() + record.size || second_end != second.data() + record.size ||
		!std::equal(first.data(), first_end, second.data()))
	{
		std::fputs("runtime-float scatter byte validation failed\n", stderr);
		std::abort();
	}

	::fast_io::posix_io_observer output{fd};
	::fast_io::operations::write_all(output, first.data(), first_end);
	::fast_io::operations::scatter_write_all(output, record.scatters.data(), record.count);
}

void runtime_float_transport(benchmark::State &state, transport_case transport,
							 std::size_t requested_size, std::size_t count,
							 floating_case selected)
{
	fd_owner file;
	double const initial_value{runtime_value};
	std::size_t const initial_precision{runtime_precision};
	auto record{make_record(requested_size, count, selected, initial_value, initial_precision)};
	validate_record(record, file.fd);
	::fast_io::posix_io_observer output{file.fd};
	alignas(64) std::array<char, 8192u> contiguous{};

	for (auto _ : state)
	{
		(void)_;
		double const value{runtime_value};
		std::size_t const precision{runtime_precision};
		std::size_t const floating_size{
			format_floating(record.floating.data(), selected, value, precision)};
		record.scatters[count / 2u].len = floating_size;
		benchmark::DoNotOptimize(record.floating.data());
		benchmark::ClobberMemory();
		switch (transport)
		{
		case transport_case::contiguous_write:
		{
			char *const end{materialize(record, contiguous.data())};
			::fast_io::operations::write_all(output, contiguous.data(), end);
			break;
		}
		case transport_case::native_scatter:
			::fast_io::operations::scatter_write_all(output, record.scatters.data(), record.count);
			break;
		case transport_case::print_policy:
			::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(
				output, record.scatters.data(), record.count);
			break;
		}
	}
	state.SetBytesProcessed(
		static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(record.size));
	state.counters["payload_bytes"] = static_cast<double>(record.size);
	state.counters["scatter_count"] = static_cast<double>(record.count);
}

[[nodiscard]] std::string case_name(transport_case transport, floating_case selected,
									std::size_t bytes, std::size_t scatters)
{
	std::string result{"runtime_float/"};
	switch (transport)
	{
	case transport_case::contiguous_write:
		result += "copy_write/";
		break;
	case transport_case::native_scatter:
		result += "writev/";
		break;
	case transport_case::print_policy:
		result += "print_policy/";
		break;
	}
	switch (selected)
	{
	case floating_case::general:
		result += "general/";
		break;
	case floating_case::fixed:
		result += "fixed/";
		break;
	case floating_case::runtime_precision:
		result += "dynamic_precision/";
		break;
	}
	result += std::to_string(bytes);
	result += "B/";
	result += std::to_string(scatters);
	result += "scatters";
	return result;
}

} // namespace

int main(int argc, char **argv)
{
	constexpr std::array<std::size_t, 11u> sizes{
		8u, 16u, 32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u};
	constexpr std::array<std::size_t, 4u> counts{2u, 3u, 5u, 9u};
	constexpr std::array<floating_case, 3u> floating_cases{
		floating_case::general, floating_case::fixed, floating_case::runtime_precision};
	constexpr std::array<transport_case, 3u> transports{
		transport_case::contiguous_write, transport_case::native_scatter, transport_case::print_policy};
	for (auto const selected : floating_cases)
	{
		for (auto const bytes : sizes)
		{
			for (auto const count : counts)
			{
				// Four characters cover the measured run-time-precision spelling.  Requiring one byte for every other
				// descriptor keeps all registered cases honest about their descriptor count.
				if (bytes < 4u + count - 1u)
				{
					continue;
				}
				for (auto const transport : transports)
				{
					auto const name{case_name(transport, selected, bytes, count)};
					::benchmark::RegisterBenchmark(
						name.c_str(), [transport, selected, bytes, count](::benchmark::State &state) {
							runtime_float_transport(state, transport, bytes, count, selected);
						})
						->MinTime(0.03)
						->Repetitions(7)
						->ReportAggregatesOnly(true);
				}
			}
		}
	}
	::benchmark::Initialize(&argc, argv);
	if (::benchmark::ReportUnrecognizedArguments(argc, argv))
	{
		return 1;
	}
	::benchmark::RunSpecifiedBenchmarks();
	::benchmark::Shutdown();
}
