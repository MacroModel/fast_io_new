#include <string>

#include "benchmark_adapter.h"

#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <fast_io_driver/json.h>

namespace fast_io_json_benchmark
{

namespace
{

struct fast_io_state
{
	::std::string_view input;
	::fast_io::json::mutable_json document;
};

struct fast_io_immutable_state
{
	::std::string_view input;
	::fast_io::json::immutable_json document;
};

::std::size_t parse_fast_io(void *opaque)
{
	auto &state{*static_cast<fast_io_state *>(opaque)};
	auto document{
		::fast_io::json::parse_json<::fast_io::json::mutable_json>(state.input)};
	auto const count{document.slice().get_array().size()};
	benchmark_barrier(document);
	return count;
}

::std::size_t serialize_fast_io(void *opaque)
{
	auto const &document{static_cast<fast_io_state *>(opaque)->document};
	auto output{::fast_io::concat_fast_io(::fast_io::mnp::json(document))};
	benchmark_barrier(output);
	return output.size();
}

::std::size_t serialize_fast_io_std(void *opaque)
{
	auto const &document{static_cast<fast_io_state *>(opaque)->document};
	auto output{::fast_io::concat_std(::fast_io::mnp::json(document))};
	benchmark_barrier(output);
	return output.size();
}

::std::string serialize_fast_io_text(void *opaque)
{
	auto const &document{static_cast<fast_io_state *>(opaque)->document};
	auto output{::fast_io::concat_fast_io(::fast_io::mnp::json(document))};
	return {output.data(), output.size()};
}

::std::string serialize_fast_io_std_text(void *opaque)
{
	auto const &document{static_cast<fast_io_state *>(opaque)->document};
	return ::fast_io::concat_std(::fast_io::mnp::json(document));
}

void destroy_fast_io(void *opaque) noexcept
{
	delete static_cast<fast_io_state *>(opaque);
}

[[nodiscard]] fast_io_state *make_fast_io_state(::std::string_view input)
{
	return new fast_io_state{
		input, ::fast_io::json::parse_json<::fast_io::json::mutable_json>(input)};
}

::std::size_t parse_fast_io_immutable(void *opaque)
{
	auto &state{*static_cast<fast_io_immutable_state *>(opaque)};
	auto document{::fast_io::json::parse_immutable_json(state.input)};
	auto const root{document.slice()};
	if (!root.is_array())
	{
		benchmark_failure("fast_io immutable root kind");
	}
	auto const count{root.size()};
	benchmark_barrier(document);
	return count;
}

::std::size_t serialize_fast_io_immutable(void *opaque)
{
	auto const &document{
		static_cast<fast_io_immutable_state *>(opaque)->document};
	auto output{::fast_io::concat_fast_io(document)};
	benchmark_barrier(output);
	return output.size();
}

::std::size_t serialize_fast_io_immutable_std(void *opaque)
{
	auto const &document{
		static_cast<fast_io_immutable_state *>(opaque)->document};
	auto output{::fast_io::concat_std(document)};
	benchmark_barrier(output);
	return output.size();
}

::std::string serialize_fast_io_immutable_text(void *opaque)
{
	auto const &document{
		static_cast<fast_io_immutable_state *>(opaque)->document};
	auto output{::fast_io::concat_fast_io(document)};
	return {output.data(), output.size()};
}

::std::string serialize_fast_io_immutable_std_text(void *opaque)
{
	auto const &document{
		static_cast<fast_io_immutable_state *>(opaque)->document};
	return ::fast_io::concat_std(document);
}

void destroy_fast_io_immutable(void *opaque) noexcept
{
	delete static_cast<fast_io_immutable_state *>(opaque);
}

[[nodiscard]] fast_io_immutable_state *make_fast_io_immutable_state(
	::std::string_view input)
{
	return new fast_io_immutable_state{
		input, ::fast_io::json::parse_immutable_json(input)};
}

} // namespace

adapter make_fast_io_adapter(::std::string_view input)
{
	auto *state{make_fast_io_state(input)};
	return {"fast_io mutable -> fast_io::string", state, parse_fast_io,
		serialize_fast_io, serialize_fast_io_text, destroy_fast_io};
}

adapter make_fast_io_std_adapter(::std::string_view input)
{
	auto *state{make_fast_io_state(input)};
	return {"fast_io mutable -> std::string", state, parse_fast_io,
		serialize_fast_io_std, serialize_fast_io_std_text, destroy_fast_io};
}

adapter make_fast_io_immutable_adapter(::std::string_view input)
{
	auto *state{make_fast_io_immutable_state(input)};
	return {"fast_io immutable -> fast_io::string", state,
		parse_fast_io_immutable, serialize_fast_io_immutable,
		serialize_fast_io_immutable_text, destroy_fast_io_immutable};
}

adapter make_fast_io_immutable_std_adapter(::std::string_view input)
{
	auto *state{make_fast_io_immutable_state(input)};
	return {"fast_io immutable -> std::string", state,
		parse_fast_io_immutable, serialize_fast_io_immutable_std,
		serialize_fast_io_immutable_std_text, destroy_fast_io_immutable};
}

} // namespace fast_io_json_benchmark
