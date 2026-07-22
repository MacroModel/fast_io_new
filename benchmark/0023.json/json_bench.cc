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
#include <fast_io_driver/json.h>

#include <glaze/glaze.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <simdjson.h>
#include <yyjson.h>

namespace
{

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_JSON_BENCH_NOINLINE [[gnu::noinline]]
#else
#define FAST_IO_JSON_BENCH_NOINLINE
#endif

inline constexpr ::std::size_t corpus_rows{8192u};
inline constexpr ::std::size_t sample_count{5u};
inline constexpr ::std::size_t target_bytes_per_sample{64u * 1024u * 1024u};

template <typename value_type>
inline void benchmark_barrier(value_type const &value) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "g"(::std::addressof(value)) : "memory");
#else
	(void)value;
#endif
}

[[noreturn]] void benchmark_failure(char const *message)
{
	::fast_io::perrln("JSON benchmark validation failed: ", ::fast_io::mnp::os_c_str(message));
	::std::abort();
}

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

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t parse_fast_io(::std::string_view input)
{
	auto document{::fast_io::json::parse_json<::bizwen::json>(input)};
	auto const count{document.slice().get_array().size()};
	benchmark_barrier(document);
	return count;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t parse_yyjson(::std::string_view input)
{
	auto *document{::yyjson_read(input.data(), input.size(), 0u)};
	if (document == nullptr)
	{
		benchmark_failure("yyjson parse");
	}
	auto *root{::yyjson_doc_get_root(document)};
	auto const count{::yyjson_arr_size(root)};
	benchmark_barrier(root);
	::yyjson_doc_free(document);
	return count;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t parse_rapidjson(::std::string_view input)
{
	::rapidjson::Document document;
	document.Parse(input.data(), input.size());
	if (document.HasParseError() || !document.IsArray())
	{
		benchmark_failure("RapidJSON parse");
	}
	auto const count{static_cast<::std::size_t>(document.Size())};
	benchmark_barrier(document);
	return count;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t parse_simdjson(
	::simdjson::dom::parser &parser, ::simdjson::padded_string const &input)
{
	::simdjson::dom::element document;
	if (auto const error{parser.parse(input).get(document)}; error)
	{
		benchmark_failure("simdjson parse");
	}
	auto const count{document.get_array().size()};
	benchmark_barrier(document);
	return count;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t parse_glaze(::std::string_view input)
{
	::glz::generic_u64 document{};
	if (auto const error{::glz::read_json(document, input)}; error)
	{
		benchmark_failure("Glaze parse");
	}
	auto const *array{document.template get_if<typename ::glz::generic_u64::array_t>()};
	if (array == nullptr)
	{
		benchmark_failure("Glaze root kind");
	}
	auto const count{array->size()};
	benchmark_barrier(document);
	return count;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t serialize_fast_io(::bizwen::json const &document)
{
	auto output{::fast_io::concat_std(::fast_io::mnp::json(document))};
	benchmark_barrier(output);
	return output.size();
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t serialize_yyjson(::yyjson_doc const *document)
{
	::std::size_t size{};
	auto *output{::yyjson_write(document, 0u, ::std::addressof(size))};
	if (output == nullptr)
	{
		benchmark_failure("yyjson serialize");
	}
	benchmark_barrier(output);
	::std::free(output);
	return size;
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t serialize_rapidjson(::rapidjson::Document const &document)
{
	::rapidjson::StringBuffer output;
	::rapidjson::Writer<::rapidjson::StringBuffer> writer{output};
	if (!document.Accept(writer))
	{
		benchmark_failure("RapidJSON serialize");
	}
	benchmark_barrier(output);
	return output.GetSize();
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t serialize_simdjson(::simdjson::dom::element document)
{
	auto output{::simdjson::to_string(document)};
	benchmark_barrier(output);
	return output.size();
}

FAST_IO_JSON_BENCH_NOINLINE ::std::size_t serialize_glaze(::glz::generic_u64 const &document)
{
	auto output{::glz::write_json(document)};
	if (!output)
	{
		benchmark_failure("Glaze serialize");
	}
	benchmark_barrier(output.value());
	return output.value().size();
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

} // namespace

int main()
{
	auto const corpus{make_corpus()};
	auto const input{::std::string_view{corpus}};
	auto const padded_input{::simdjson::padded_string{input}};
	auto const iterations{(::std::max)(static_cast<::std::size_t>(3u),
		target_bytes_per_sample / corpus.size())};

	// Build all serialization DOMs and validate the five independent parsers
	// before entering a timed region.
	auto fast_io_document{::fast_io::json::parse_json<::bizwen::json>(input)};
	auto *yyjson_document{::yyjson_read(input.data(), input.size(), 0u)};
	if (yyjson_document == nullptr)
	{
		benchmark_failure("yyjson setup parse");
	}
	::rapidjson::Document rapidjson_document;
	rapidjson_document.Parse(input.data(), input.size());
	if (rapidjson_document.HasParseError())
	{
		benchmark_failure("RapidJSON setup parse");
	}
	::simdjson::dom::parser simdjson_document_parser;
	::simdjson::dom::element simdjson_document;
	if (auto const error{simdjson_document_parser.parse(padded_input).get(simdjson_document)}; error)
	{
		benchmark_failure("simdjson setup parse");
	}
	::glz::generic_u64 glaze_document{};
	if (auto const error{::glz::read_json(glaze_document, input)}; error)
	{
		benchmark_failure("Glaze setup parse");
	}

	::simdjson::dom::parser simdjson_parse_parser;
	if (parse_fast_io(input) != corpus_rows || parse_yyjson(input) != corpus_rows ||
		parse_rapidjson(input) != corpus_rows ||
		parse_simdjson(simdjson_parse_parser, padded_input) != corpus_rows ||
		parse_glaze(input) != corpus_rows)
	{
		benchmark_failure("root element count");
	}

	::std::array<benchmark_row, 5u> rows{{
		{"fast_io/bizwen"}, {"yyjson"}, {"RapidJSON"}, {"simdjson"}, {"Glaze"}}};

	rows[0].serialized_size = serialize_fast_io(fast_io_document);
	rows[1].serialized_size = serialize_yyjson(yyjson_document);
	rows[2].serialized_size = serialize_rapidjson(rapidjson_document);
	rows[3].serialized_size = serialize_simdjson(simdjson_document);
	rows[4].serialized_size = serialize_glaze(glaze_document);
	for (auto const &row : rows)
	{
		if (row.serialized_size == 0u)
		{
			benchmark_failure("empty serialization");
		}
	}

	rows[0].parse_mib_per_second = measure_mib_per_second(
		[&] { return parse_fast_io(input); }, corpus.size(), iterations);
	rows[1].parse_mib_per_second = measure_mib_per_second(
		[&] { return parse_yyjson(input); }, corpus.size(), iterations);
	rows[2].parse_mib_per_second = measure_mib_per_second(
		[&] { return parse_rapidjson(input); }, corpus.size(), iterations);
	rows[3].parse_mib_per_second = measure_mib_per_second(
		[&] { return parse_simdjson(simdjson_parse_parser, padded_input); }, corpus.size(), iterations);
	rows[4].parse_mib_per_second = measure_mib_per_second(
		[&] { return parse_glaze(input); }, corpus.size(), iterations);

	rows[0].serialize_mib_per_second = measure_mib_per_second(
		[&] { return serialize_fast_io(fast_io_document); }, rows[0].serialized_size, iterations);
	rows[1].serialize_mib_per_second = measure_mib_per_second(
		[&] { return serialize_yyjson(yyjson_document); }, rows[1].serialized_size, iterations);
	rows[2].serialize_mib_per_second = measure_mib_per_second(
		[&] { return serialize_rapidjson(rapidjson_document); }, rows[2].serialized_size, iterations);
	rows[3].serialize_mib_per_second = measure_mib_per_second(
		[&] { return serialize_simdjson(simdjson_document); }, rows[3].serialized_size, iterations);
	rows[4].serialize_mib_per_second = measure_mib_per_second(
		[&] { return serialize_glaze(glaze_document); }, rows[4].serialized_size, iterations);

	::fast_io::println("JSON DOM benchmark (median of ", sample_count,
		" samples, ", iterations, " iterations/sample)");
	::fast_io::println("corpus bytes: ", corpus.size(), ", rows: ", corpus_rows);
	::fast_io::println("library\tparse MiB/s\tserialize MiB/s\tserialized bytes");
	for (auto const &row : rows)
	{
		::fast_io::println(row.name, "\t", ::fast_io::mnp::fixed(row.parse_mib_per_second, 2u), "\t",
			::fast_io::mnp::fixed(row.serialize_mib_per_second, 2u), "\t", row.serialized_size);
	}

	::yyjson_doc_free(yyjson_document);
}
