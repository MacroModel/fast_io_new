#include "benchmark_adapter.h"

#include <glaze/glaze.hpp>

#include <string>
#include <utility>

namespace fast_io_json_benchmark
{

namespace
{

struct glaze_state
{
	::std::string_view input;
	::glz::generic_u64 document;
};

::std::size_t parse_glaze(void *opaque)
{
	auto const input{static_cast<glaze_state *>(opaque)->input};
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

::std::size_t serialize_glaze(void *opaque)
{
	auto const &document{static_cast<glaze_state *>(opaque)->document};
	auto output{::glz::write_json(document)};
	if (!output)
	{
		benchmark_failure("Glaze serialize");
	}
	benchmark_barrier(output.value());
	return output.value().size();
}

::std::string serialize_glaze_text(void *opaque)
{
	auto const &document{static_cast<glaze_state *>(opaque)->document};
	auto output{::glz::write_json(document)};
	if (!output)
	{
		benchmark_failure("Glaze validation serialize");
	}
	return ::std::move(output.value());
}

void destroy_glaze(void *opaque) noexcept
{
	delete static_cast<glaze_state *>(opaque);
}

} // namespace

adapter make_glaze_adapter(::std::string_view input)
{
	auto *state{new glaze_state{input, {}}};
	if (auto const error{::glz::read_json(state->document, input)}; error)
	{
		delete state;
		benchmark_failure("Glaze setup parse");
	}
	return {"Glaze", state, parse_glaze, serialize_glaze,
		serialize_glaze_text, destroy_glaze};
}

} // namespace fast_io_json_benchmark
