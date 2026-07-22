#include "benchmark_adapter.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <fast_io.h>
#include <yyjson.h>

namespace fast_io_json_benchmark
{

namespace
{

inline constexpr ::std::size_t corpus_rows{8192u};
inline constexpr ::std::size_t sample_count{5u};
inline constexpr ::std::size_t target_bytes_per_sample{64u * 1024u * 1024u};

void append_unsigned(::std::string &output, ::std::size_t value)
{
	char buffer[32u];
	auto const converted{::std::to_chars(buffer, buffer + sizeof(buffer), value)};
	if (converted.ec != ::std::errc{})
	{
		benchmark_failure("std::to_chars failed while constructing the corpus");
	}
	output.append(buffer, converted.ptr);
}

[[nodiscard]] ::std::string make_corpus()
{
	::std::string output;
	output.reserve(corpus_rows * 210u);
	output.push_back('[');
	for (::std::size_t index{}; index != corpus_rows; ++index)
	{
		if (index != 0u)
		{
			output.push_back(',');
		}
		output.append("{\"id\":");
		append_unsigned(output, index);
		output.append(",\"signed\":-");
		append_unsigned(output, index + 1u);
		output.append(",\"name\":\"record-");
		append_unsigned(output, index);
		output.append("-\\u4e2d\\u6587\",\"utf8\":\"");
		output.append("\xE4\xB8\xAD\xE6\x96\x87-\xCE\xB2");
		output.append("\",\"active\":");
		output.append((index & 1u) == 0u ? "true" : "false");
		output.append(",\"score\":");
		append_unsigned(output, index % 10000u);
		output.append(".125e-2,\"escaped\":\"quote:\\\" slash:\\\\ line:\\n\"");
		output.append(",\"tags\":[\"alpha\",\"beta\",\"gamma\"],\"meta\":{\"none\":null,\"code\":\"");
		append_unsigned(output, index % 97u);
		output.append("\"}}");
	}
	output.push_back(']');
	return output;
}

template <typename operation_type>
[[nodiscard]] double measure_median_seconds(operation_type &&operation, ::std::size_t iterations)
{
	::std::size_t warmup_checksum{};
	for (::std::size_t iteration{}; iteration != 2u; ++iteration)
	{
		warmup_checksum += operation();
	}
	benchmark_barrier(warmup_checksum);

	::std::array<double, sample_count> samples{};
	::std::size_t checksum{};
	for (auto &sample : samples)
	{
		auto const start{::std::chrono::steady_clock::now()};
		for (::std::size_t iteration{}; iteration != iterations; ++iteration)
		{
			checksum += operation();
		}
		auto const finish{::std::chrono::steady_clock::now()};
		sample = ::std::chrono::duration<double>(finish - start).count();
	}
	benchmark_barrier(checksum);
	::std::ranges::sort(samples);
	return samples[sample_count / 2u];
}

template <typename operation_type>
[[nodiscard]] double measure_mib_per_second(
	operation_type &&operation, ::std::size_t bytes_per_operation, ::std::size_t iterations)
{
	auto const seconds{measure_median_seconds(
		::std::forward<operation_type>(operation), iterations)};
	auto const bytes{static_cast<double>(bytes_per_operation) * static_cast<double>(iterations)};
	return bytes / seconds / (1024.0 * 1024.0);
}

struct benchmark_row
{
	::std::string_view name;
	double parse_mib_per_second{};
	double serialize_mib_per_second{};
	::std::size_t serialized_size{};
};

void validate_serialization(
	::std::string_view output, ::yyjson_val const *reference)
{
	auto *document{::yyjson_read(output.data(), output.size(), 0u)};
	if (document == nullptr ||
		!::yyjson_equals(::yyjson_doc_get_root(document), reference))
	{
		if (document != nullptr)
		{
			::yyjson_doc_free(document);
		}
		benchmark_failure("deep semantic serialization equality");
	}
	::yyjson_doc_free(document);
}

} // namespace

[[noreturn]] void benchmark_failure(char const *message)
{
	::fast_io::perrln("JSON benchmark validation failed: ", ::fast_io::mnp::os_c_str(message));
	::std::abort();
}

} // namespace fast_io_json_benchmark

int main()
{
	using namespace ::fast_io_json_benchmark;
	auto const corpus{make_corpus()};
	auto const input{::std::string_view{corpus}};
	auto const iterations{(::std::max)(static_cast<::std::size_t>(3u),
									   target_bytes_per_sample / corpus.size())};

	::std::array<adapter, 9u> adapters{{
		make_fast_io_adapter(input), make_fast_io_std_adapter(input),
		make_fast_io_immutable_adapter(input),
		make_fast_io_immutable_std_adapter(input),
		make_yyjson_mutable_adapter(input), make_yyjson_immutable_adapter(input),
		make_rapidjson_adapter(input),
		make_simdjson_adapter(input), make_glaze_adapter(input)}};
	::std::array<benchmark_row, 9u> rows{};
	auto *reference_document{::yyjson_read(input.data(), input.size(), 0u)};
	if (reference_document == nullptr)
	{
		benchmark_failure("reference parse");
	}
	auto const *reference{::yyjson_doc_get_root(reference_document)};

	for (::std::size_t index{}; index != adapters.size(); ++index)
	{
		auto &implementation{adapters[index]};
		if (implementation.parse(implementation.state) != corpus_rows)
		{
			benchmark_failure("root element count");
		}
		auto &row{rows[index]};
		row.name = implementation.name;
		auto const validation_output{
			implementation.serialize_text(implementation.state)};
		validate_serialization(validation_output, reference);
		row.serialized_size = validation_output.size();
		if (row.serialized_size == 0u)
		{
			benchmark_failure("empty serialization");
		}
		if (implementation.serialize(implementation.state) != row.serialized_size)
		{
			benchmark_failure("unstable serialized size");
		}
	}

	for (::std::size_t index{}; index != adapters.size(); ++index)
	{
		auto &implementation{adapters[index]};
		rows[index].parse_mib_per_second = measure_mib_per_second(
			[&] { return implementation.parse(implementation.state); }, corpus.size(), iterations);
	}

	for (::std::size_t index{}; index != adapters.size(); ++index)
	{
		auto &implementation{adapters[index]};
		auto &row{rows[index]};
		row.serialize_mib_per_second = measure_mib_per_second(
			[&] { return implementation.serialize(implementation.state); },
			row.serialized_size, iterations);
	}

	::fast_io::println("JSON DOM benchmark (separate adapter TUs, median of ", sample_count,
					   " samples, ", iterations, " iterations/sample)");
	::fast_io::println("corpus bytes: ", corpus.size(), ", rows: ", corpus_rows);
	::fast_io::println("library\tparse MiB/s\tserialize MiB/s\tserialized bytes");
	for (auto const &row : rows)
	{
		::fast_io::println(row.name, "\t", ::fast_io::mnp::fixed(row.parse_mib_per_second, 2u), "\t",
						   ::fast_io::mnp::fixed(row.serialize_mib_per_second, 2u), "\t", row.serialized_size);
	}

	for (auto &implementation : adapters)
	{
		implementation.destroy(implementation.state);
	}
	::yyjson_doc_free(reference_document);
	return 0;
}
