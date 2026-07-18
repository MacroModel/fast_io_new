#include <concepts>
#include <string_view>
#include <type_traits>

#include <fast_io_legacy.h>

namespace
{

using normalized_filebuf_ref =
	decltype(::fast_io::operations::output_stream_ref(::std::declval<::fast_io::filebuf_file &>()));

static_assert(::std::same_as<normalized_filebuf_ref, ::fast_io::filebuf_io_observer>);
static_assert(::fast_io::semantic_plain_leaf_coalesce_preferred_stream<char, ::fast_io::filebuf_io_observer>);
static_assert(::fast_io::semantic_plain_leaf_coalesce_preferred_stream<char, ::fast_io::filebuf_io_observer const &>);
static_assert(::fast_io::semantic_plain_leaf_coalesce_preferred_stream<char, normalized_filebuf_ref>);

// The cost marker is intentionally narrower than the structural streambuf adapter. String buffers and custom
// streambufs may have cheap cursor access, while buffering/decorator wrappers establish their own cost model.
static_assert(!::fast_io::semantic_plain_leaf_coalesce_preferred_stream<char, ::fast_io::streambuf_io_observer>);
static_assert(!::fast_io::semantic_plain_leaf_coalesce_preferred_stream<char, ::fast_io::c_io_observer>);

[[maybe_unused]] inline void instantiate_plain_pack_dispatch(::fast_io::filebuf_io_observer output)
{
	using namespace ::std::literals;
	::fast_io::print(output, ::fast_io::mnp::pack("header="sv, "value"sv, ";tail"sv));
}

} // namespace

int main() {}
