#include <fast_io_device.h>
#include <fast_io.h>

#include <type_traits>
#include <utility>

int main()
{
	using output_ref = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(::std::declval<::fast_io::obuf_file &>()))>;
	// The removed `constant_buffer_output_stream` name conflated two independent contracts. Buffered dispatch needs a
	// valid mutable cursor protocol, while the fixed-reserve fast path separately needs a compile-time minimum put area
	// plus its matching refill operation. Test the exact normalized observer consumed by both strategies.
	static_assert(::fast_io::operations::decay::defines::has_obuffer_basic_operations<output_ref>);
	static_assert(::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<output_ref>);
#if 0
	using transcoded_output_ref = ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(::std::declval<::fast_io::u8ogb18030_file &>()))>;
	static_assert(::fast_io::operations::decay::defines::has_obuffer_basic_operations<transcoded_output_ref>);
	static_assert(::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<transcoded_output_ref>);
#endif
}
