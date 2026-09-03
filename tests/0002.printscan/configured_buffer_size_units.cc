#define FAST_IO_BUFFER_SIZE 64u
#define FAST_IO_DISABLE_FLOATING_POINT

#include <cstddef>

#include <fast_io.h>

namespace
{

/*
`FAST_IO_BUFFER_SIZE` denotes one byte budget across core transmit,
freestanding buffered I/O, and transcoder adapters.  For an endpoint unit U,
every policy must first prove sizeof(U) <= budget and then expose exactly
budget / sizeof(U) complete elements.  Treating either operand as an element
count reverses that dimensional relation and can produce a zero-sized buffer.
*/
static_assert(::fast_io::details::transmit_buffer_size_cache<sizeof(char)> == 64u);
static_assert(::fast_io::details::transmit_buffer_size_cache<sizeof(char16_t)> == 32u);
static_assert(::fast_io::details::transmit_buffer_size_cache<sizeof(char32_t)> == 16u);

static_assert(::fast_io::details::compute_default_buffer_size<char>() == 64u);
static_assert(::fast_io::details::compute_default_buffer_size<char16_t>() == 32u);
static_assert(::fast_io::details::compute_default_buffer_size<char32_t>() == 16u);

static_assert(::fast_io::details::cal_buffer_size<char, true>() == 64u);
static_assert(::fast_io::details::cal_buffer_size<char16_t, true>() == 32u);
static_assert(::fast_io::details::cal_buffer_size<char32_t, true>() == 16u);

static_assert(::fast_io::details::default_transcode_buffer_size<char>() == 64u);
static_assert(::fast_io::details::default_transcode_buffer_size<char16_t>() == 32u);
static_assert(::fast_io::details::default_transcode_buffer_size<char32_t>() == 16u);

using output_traits = ::fast_io::basic_otranscoder_traits<char, char16_t>;
using input_traits = ::fast_io::basic_itranscoder_traits<char32_t, char16_t, char>;
static_assert(output_traits::public_buffer_size == 64u);
static_assert(output_traits::transform_buffer_size == 32u);
static_assert(input_traits::public_buffer_size == 16u);
static_assert(input_traits::source_buffer_size == 32u);
static_assert(input_traits::transform_buffer_size == 64u);

} // namespace

int main()
{}
