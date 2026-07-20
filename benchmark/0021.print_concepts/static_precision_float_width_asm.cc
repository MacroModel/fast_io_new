#include <fast_io_device.h>
#include <fast_io_format.h>

// A non-null 2-KiB put area makes the ordinary hot branch observable
// independently of the null/short-buffer continuation. The static
// internal-width wrappers should format once into that put area, then insert
// padding after the sign/prefix.
extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_static_fixed_internal_float(char *buffer, float value)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{:+020.6f}">(output, value);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_static_fixed_internal_double(char *buffer, double value)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{:+020.6f}">(output, value);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_static_fixed_right_double(char *buffer, double value)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{:>20.6f}">(output, value);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_static_fixed_no_width_double(char *buffer, double value)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{:+.6f}">(output, value);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_printf_static_fixed_internal_double(char *buffer, double value)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::printf<"%+020.6f">(output, value);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_dynamic_fixed_right_double(char *buffer, double value,
								   unsigned width, unsigned precision)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{0:{1}.{2}f}">(
		output, value, width, precision);
	return output.curr_ptr;
}

extern "C" [[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_dynamic_fixed_internal_double(char *buffer, double value,
									  unsigned width, unsigned precision)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"{0:+0{1}.{2}f}">(
		output, value, width, precision);
	return output.curr_ptr;
}
