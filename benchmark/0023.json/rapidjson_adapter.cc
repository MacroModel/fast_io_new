#include "benchmark_adapter.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>

namespace fast_io_json_benchmark
{

namespace
{

struct rapidjson_state
{
	::std::string_view input;
	::rapidjson::Document document;
};

::std::size_t parse_rapidjson(void *opaque)
{
	auto const input{static_cast<rapidjson_state *>(opaque)->input};
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

::std::size_t serialize_rapidjson(void *opaque)
{
	auto const &document{static_cast<rapidjson_state *>(opaque)->document};
	::rapidjson::StringBuffer output;
	::rapidjson::Writer<::rapidjson::StringBuffer> writer{output};
	if (!document.Accept(writer))
	{
		benchmark_failure("RapidJSON serialize");
	}
	benchmark_barrier(output);
	return output.GetSize();
}

::std::string serialize_rapidjson_text(void *opaque)
{
	auto const &document{static_cast<rapidjson_state *>(opaque)->document};
	::rapidjson::StringBuffer output;
	::rapidjson::Writer<::rapidjson::StringBuffer> writer{output};
	if (!document.Accept(writer))
	{
		benchmark_failure("RapidJSON validation serialize");
	}
	return {output.GetString(), output.GetSize()};
}

void destroy_rapidjson(void *opaque) noexcept
{
	delete static_cast<rapidjson_state *>(opaque);
}

} // namespace

adapter make_rapidjson_adapter(::std::string_view input)
{
	auto *state{new rapidjson_state{input, {}}};
	state->document.Parse(input.data(), input.size());
	if (state->document.HasParseError() || !state->document.IsArray())
	{
		delete state;
		benchmark_failure("RapidJSON setup parse");
	}
	return {"RapidJSON", state, parse_rapidjson, serialize_rapidjson,
		serialize_rapidjson_text, destroy_rapidjson};
}

} // namespace fast_io_json_benchmark
