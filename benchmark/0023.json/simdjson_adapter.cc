#include "benchmark_adapter.h"

#include <simdjson.h>

#include <string>

namespace fast_io_json_benchmark
{

namespace
{

struct simdjson_state
{
	::simdjson::padded_string input;
	::simdjson::dom::parser parse_parser;
	::simdjson::dom::parser document_parser;
	::simdjson::dom::element document;

	explicit simdjson_state(::std::string_view source) : input(source)
	{}
};

::std::size_t parse_simdjson(void *opaque)
{
	auto &state{*static_cast<simdjson_state *>(opaque)};
	::simdjson::dom::element document;
	if (auto const error{state.parse_parser.parse(state.input).get(document)}; error)
	{
		benchmark_failure("simdjson parse");
	}
	auto const count{document.get_array().size()};
	benchmark_barrier(document);
	return count;
}

::std::size_t serialize_simdjson(void *opaque)
{
	auto document{static_cast<simdjson_state *>(opaque)->document};
	auto output{::simdjson::to_string(document)};
	benchmark_barrier(output);
	return output.size();
}

::std::string serialize_simdjson_text(void *opaque)
{
	auto document{static_cast<simdjson_state *>(opaque)->document};
	return ::simdjson::to_string(document);
}

void destroy_simdjson(void *opaque) noexcept
{
	delete static_cast<simdjson_state *>(opaque);
}

} // namespace

adapter make_simdjson_adapter(::std::string_view input)
{
	auto *state{new simdjson_state{input}};
	if (auto const error{state->document_parser.parse(state->input).get(state->document)}; error)
	{
		delete state;
		benchmark_failure("simdjson setup parse");
	}
	return {"simdjson", state, parse_simdjson, serialize_simdjson,
		serialize_simdjson_text, destroy_simdjson};
}

} // namespace fast_io_json_benchmark
