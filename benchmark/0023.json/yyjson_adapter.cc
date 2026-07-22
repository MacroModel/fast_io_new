#include "benchmark_adapter.h"

#include <cstdlib>
#include <string>
#include <yyjson.h>

namespace fast_io_json_benchmark
{

namespace
{

struct yyjson_state
{
	::std::string_view input;
	::yyjson_doc *document;
};

struct yyjson_mutable_state
{
	::std::string_view input;
	::yyjson_mut_doc *document;
};

::std::size_t parse_yyjson(void *opaque)
{
	auto const input{static_cast<yyjson_state *>(opaque)->input};
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

::std::size_t serialize_yyjson(void *opaque)
{
	auto const *document{static_cast<yyjson_state *>(opaque)->document};
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

::std::string serialize_yyjson_text(void *opaque)
{
	auto const *document{static_cast<yyjson_state *>(opaque)->document};
	::std::size_t size{};
	auto *output{::yyjson_write(document, 0u, ::std::addressof(size))};
	if (output == nullptr)
	{
		benchmark_failure("yyjson validation serialize");
	}
	::std::string result{output, size};
	::std::free(output);
	return result;
}

void destroy_yyjson(void *opaque) noexcept
{
	auto *state{static_cast<yyjson_state *>(opaque)};
	::yyjson_doc_free(state->document);
	delete state;
}

::std::size_t parse_yyjson_mutable(void *opaque)
{
	auto const input{static_cast<yyjson_mutable_state *>(opaque)->input};
	auto *immutable{::yyjson_read(input.data(), input.size(), 0u)};
	if (immutable == nullptr)
	{
		benchmark_failure("yyjson mutable parse");
	}
	auto *document{::yyjson_doc_mut_copy(immutable, nullptr)};
	::yyjson_doc_free(immutable);
	if (document == nullptr)
	{
		benchmark_failure("yyjson mutable copy");
	}
	auto *root{::yyjson_mut_doc_get_root(document)};
	auto const count{::yyjson_mut_arr_size(root)};
	benchmark_barrier(root);
	::yyjson_mut_doc_free(document);
	return count;
}

::std::size_t serialize_yyjson_mutable(void *opaque)
{
	auto const *document{
		static_cast<yyjson_mutable_state *>(opaque)->document};
	::std::size_t size{};
	auto *output{::yyjson_mut_write(document, 0u, ::std::addressof(size))};
	if (output == nullptr)
	{
		benchmark_failure("yyjson mutable serialize");
	}
	benchmark_barrier(output);
	::std::free(output);
	return size;
}

::std::string serialize_yyjson_mutable_text(void *opaque)
{
	auto const *document{
		static_cast<yyjson_mutable_state *>(opaque)->document};
	::std::size_t size{};
	auto *output{::yyjson_mut_write(document, 0u, ::std::addressof(size))};
	if (output == nullptr)
	{
		benchmark_failure("yyjson mutable validation serialize");
	}
	::std::string result{output, size};
	::std::free(output);
	return result;
}

void destroy_yyjson_mutable(void *opaque) noexcept
{
	auto *state{static_cast<yyjson_mutable_state *>(opaque)};
	::yyjson_mut_doc_free(state->document);
	delete state;
}

} // namespace

adapter make_yyjson_immutable_adapter(::std::string_view input)
{
	auto *document{::yyjson_read(input.data(), input.size(), 0u)};
	if (document == nullptr)
	{
		benchmark_failure("yyjson setup parse");
	}
	auto *state{new yyjson_state{input, document}};
	return {"yyjson immutable", state, parse_yyjson, serialize_yyjson,
		serialize_yyjson_text, destroy_yyjson};
}

adapter make_yyjson_mutable_adapter(::std::string_view input)
{
	auto *immutable{::yyjson_read(input.data(), input.size(), 0u)};
	if (immutable == nullptr)
	{
		benchmark_failure("yyjson mutable setup parse");
	}
	auto *document{::yyjson_doc_mut_copy(immutable, nullptr)};
	::yyjson_doc_free(immutable);
	if (document == nullptr)
	{
		benchmark_failure("yyjson mutable setup copy");
	}
	auto *state{new yyjson_mutable_state{input, document}};
	return {"yyjson mutable", state, parse_yyjson_mutable,
		serialize_yyjson_mutable, serialize_yyjson_mutable_text,
		destroy_yyjson_mutable};
}

} // namespace fast_io_json_benchmark
