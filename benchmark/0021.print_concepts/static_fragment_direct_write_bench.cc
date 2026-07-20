#include <fast_io.h>
#include <fast_io_dsal/array.h>
#include <fast_io_format.h>

#include <benchmark/benchmark.h>

#if defined(__linux__) && defined(__NR_writev)

#include <fcntl.h>
#include <unistd.h>

namespace
{

int output_fd{-1};

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void current_two_static_fragments(int fd)
{
	::fast_io::fmt::print<"i = {}">(::fast_io::posix_io_observer{fd}, 32);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void outlined_cold_writev_control(int fd)
{
	static constexpr char prefix[]{"i = "};
	static constexpr char digits[]{"32"};
	::fast_io::io_scatter_t scatters[2u]{{prefix, 4u}, {digits, 2u}};
	auto output{::fast_io::posix_io_observer{fd}};
	::fast_io::details::scatter_write_all_bytes_cold_impl(
		output, scatters, 2u);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void compact_stack_then_write(int fd)
{
	::fast_io::array<char, 6u> buffer{{'i', ' ', '=', ' ', '3', '2'}};
	::fast_io::operations::write_all(
		::fast_io::posix_io_observer{fd}, buffer.data(), buffer.data() + buffer.size());
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void merged_static_arg_then_write(int fd)
{
	::fast_io::fmt::print<"i = {}">(
		::fast_io::posix_io_observer{fd}, ::fast_io::fmt::static_arg<32>);
}

#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void writev_partial_or_error(
	::fast_io::posix_io_observer output, ::fast_io::io_scatter_t *scatters,
	::std::ptrdiff_t result)
{
	::fast_io::linux_system_call_throw_error(result);
	auto consumed{static_cast<::std::size_t>(result)};
	::std::size_t position{};
	for (; position != 2u; ++position)
	{
		if (consumed < scatters[position].len)
		{
			break;
		}
		consumed -= scatters[position].len;
	}
	if (position == 2u)
	{
		return;
	}
	if (consumed != 0u)
	{
		auto &first{scatters[position]};
		first.base = static_cast<::std::byte const *>(first.base) + consumed;
		first.len -= consumed;
	}
	::fast_io::details::scatter_write_all_bytes_cold_impl(
		output, scatters + position, 2u - position);
}

#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void scatter_status_partial(
	::fast_io::posix_io_observer output, ::fast_io::io_scatter_t *scatters,
	::fast_io::io_scatter_status_t status)
{
	auto position{status.position};
	if (status.position_in_scatter != 0u)
	{
		auto &first{scatters[position]};
		first.base = static_cast<::std::byte const *>(first.base) +
			status.position_in_scatter;
		first.len -= status.position_in_scatter;
	}
	::fast_io::details::scatter_write_all_bytes_cold_impl(
		output, scatters + position, 2u - position);
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void hot_first_writev(int fd)
{
	static constexpr char prefix[]{"i = "};
	static constexpr char digits[]{"32"};
	::fast_io::io_scatter_t scatters[2u]{{prefix, 4u}, {digits, 2u}};
	auto const result{::fast_io::system_call<__NR_writev, ::std::ptrdiff_t>(
		fd, scatters, 2u)};
	if (result != 6) [[unlikely]]
	{
		writev_partial_or_error(
			::fast_io::posix_io_observer{fd}, scatters, result);
	}
}

#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
void hot_first_scatter_cpo(int fd)
{
	static constexpr char prefix[]{"i = "};
	static constexpr char digits[]{"32"};
	::fast_io::io_scatter_t scatters[2u]{{prefix, 4u}, {digits, 2u}};
	auto const status{scatter_write_some_bytes_overflow_define(
		::fast_io::posix_io_observer{fd}, scatters, 2u)};
	if (status.position != 2u) [[unlikely]]
	{
		scatter_status_partial(
			::fast_io::posix_io_observer{fd}, scatters, status);
	}
}

template <auto function>
void run(benchmark::State &state)
{
	for (auto ignored : state)
	{
		(void)ignored;
		function(output_fd);
	}
	state.SetItemsProcessed(state.iterations());
	state.SetBytesProcessed(state.iterations() * 6);
}

BENCHMARK(run<outlined_cold_writev_control>)->Name("static-fragments/baseline-outlined-cold-writev");
BENCHMARK(run<current_two_static_fragments>)->Name("static-fragments/library-hot-first-writev");
BENCHMARK(run<compact_stack_then_write>)->Name("static-fragments/stack-compact-write");
BENCHMARK(run<merged_static_arg_then_write>)->Name("static-fragments/merged-static-arg-write");
BENCHMARK(run<hot_first_writev>)->Name("static-fragments/hot-first-writev");
BENCHMARK(run<hot_first_scatter_cpo>)->Name("static-fragments/hot-first-scatter-cpo");

} // namespace

int main(int argc, char **argv)
{
	output_fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
	if (output_fd == -1)
	{
		return 1;
	}
	current_two_static_fragments(output_fd);
	outlined_cold_writev_control(output_fd);
	compact_stack_then_write(output_fd);
	merged_static_arg_then_write(output_fd);
	hot_first_writev(output_fd);
	hot_first_scatter_cpo(output_fd);
	::benchmark::Initialize(&argc, argv);
	if (::benchmark::ReportUnrecognizedArguments(argc, argv))
	{
		::close(output_fd);
		return 1;
	}
	::benchmark::RunSpecifiedBenchmarks();
	::benchmark::Shutdown();
	auto const result{::close(output_fd)};
	return result == 0 ? 0 : 1;
}

#else

int main()
{
	return 0;
}

#endif
