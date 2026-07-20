#include <fast_io_format.h>

#include <string>
#include <string_view>

struct runtime_int_control
{
	int value;
};

struct runtime_double_control
{
	double value;
};

inline constexpr decltype(auto) print_alias_define(
	::fast_io::io_alias_t, runtime_int_control &value) noexcept
{
	return ::fast_io::io_print_alias(value.value);
}

inline constexpr decltype(auto) print_alias_define(
	::fast_io::io_alias_t, runtime_double_control &value) noexcept
{
	return ::fast_io::io_print_alias(value.value);
}

namespace
{

using output_type = ::fast_io::posix_io_observer;

[[nodiscard]] inline constexpr output_type make_output(int descriptor) noexcept
{
	return {descriptor};
}

template <typename value_type>
[[gnu::always_inline]] inline void historical_print(
	output_type output, value_type &value)
{
	decltype(auto) output_reference{
		::fast_io::operations::output_stream_ref(output)};
	::fast_io::operations::decay::
		print_freestanding_decay_unforwarded<false>(
			output_reference, value);
}

struct historical_passive_fixed_callback
{
	::fast_io::obuffer_view &output;

	template <typename... component_types>
	[[gnu::always_inline]] inline void operator()(
		component_types &&...components) const
	{
		decltype(auto) output_reference{
			::fast_io::operations::output_stream_ref(output)};
		using char_type = typename ::std::remove_cvref_t<
			decltype(output_reference)>::output_char_type;
		::fast_io::operations::decay::
			print_passive_mixed_put_area_fast_entry<false>(
				output_reference,
				::fast_io::io_print_forward<char_type>(
					::fast_io::io_print_alias(components))...);
	}
};

} // namespace

extern "C"
{

[[gnu::noinline]] void fast_io_facade_public_print_int(
	int descriptor, int value)
{
	auto output{make_output(descriptor)};
	::fast_io::print(output, value);
}

[[gnu::noinline]] void fast_io_facade_historical_print_int(
	int descriptor, int value)
{
	auto output{make_output(descriptor)};
	historical_print(output, value);
}

[[gnu::noinline]] void fast_io_facade_control_print_int(
	int descriptor, int value)
{
	auto output{make_output(descriptor)};
	runtime_int_control control{value};
	::fast_io::print(output, control);
}

[[gnu::noinline]] void fast_io_facade_public_print_double(
	int descriptor, double value)
{
	auto output{make_output(descriptor)};
	::fast_io::print(output, value);
}

[[gnu::noinline]] void fast_io_facade_historical_print_double(
	int descriptor, double value)
{
	auto output{make_output(descriptor)};
	historical_print(output, value);
}

[[gnu::noinline]] void fast_io_facade_control_print_double(
	int descriptor, double value)
{
	auto output{make_output(descriptor)};
	runtime_double_control control{value};
	::fast_io::print(output, control);
}

[[gnu::noinline]] void fast_io_facade_operations_print_int(
	int descriptor, int value)
{
	auto output{make_output(descriptor)};
	::fast_io::operations::print_freestanding<false>(output, value);
}

[[gnu::noinline]] void fast_io_facade_operations_print_double(
	int descriptor, double value)
{
	auto output{make_output(descriptor)};
	::fast_io::operations::print_freestanding<false>(output, value);
}

[[gnu::noinline]] ::std::string fast_io_facade_public_concat_std_int(
	int value)
{
	return ::fast_io::concat_std(value);
}

[[gnu::noinline]] ::std::string fast_io_facade_historical_concat_std_int(
	int value)
{
	return ::fast_io::basic_general_concat_checked<
		false, char, ::std::string>(value);
}

[[gnu::noinline]] ::std::string fast_io_facade_control_concat_std_int(
	int value)
{
	runtime_int_control control{value};
	return ::fast_io::concat_std(control);
}

[[gnu::noinline]] ::std::string fast_io_facade_public_concat_std_double(
	double value)
{
	return ::fast_io::concat_std(value);
}

[[gnu::noinline]] ::std::string fast_io_facade_historical_concat_std_double(
	double value)
{
	return ::fast_io::basic_general_concat_checked<
		false, char, ::std::string>(value);
}

[[gnu::noinline]] ::std::string fast_io_facade_control_concat_std_double(
	double value)
{
	runtime_double_control control{value};
	return ::fast_io::concat_std(control);
}

[[gnu::noinline]] void fast_io_facade_fmt_print_int(
	int descriptor, int value)
{
	auto output{make_output(descriptor)};
	::fast_io::fmt::print<"{}">(output, value);
}

[[gnu::noinline]] void fast_io_facade_fmt_print_double(
	int descriptor, double value)
{
	auto output{make_output(descriptor)};
	::fast_io::fmt::print<"{}">(output, value);
}

[[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_facade_fmt_passive_runtime_text(
	char *buffer, char const *text, ::std::size_t size)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::std::string_view value{text, size};
	::fast_io::fmt::print<"a{0}b{0}c{0}d{0}">(output, value);
	return output.curr_ptr;
}

[[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_facade_direct_passive_runtime_text(
	char *buffer, char const *text, ::std::size_t size)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::std::string_view value{text, size};
	::fast_io::fmt::details::lower_format_program<
		::fast_io::fmt::basic_fixed_string{"a{0}b{0}c{0}d{0}"},
		::fast_io::fmt::brace_fmt_t>(
			historical_passive_fixed_callback{output}, value);
	return output.curr_ptr;
}

[[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_facade_fmt_passive_constant_double(char *buffer)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"a{}b{}c{}d{}">(output, 3.2, 1, 2, 3);
	return output.curr_ptr;
}

[[gnu::noinline, gnu::nonnull(1)]] char *
fast_io_facade_fmt_constant_double(char *buffer)
{
	::fast_io::obuffer_view output{buffer, buffer + 2048u};
	::fast_io::fmt::print<"i={}">(output, 3.2);
	return output.curr_ptr;
}

[[gnu::noinline]] void fast_io_facade_default_raw_int()
{
	::fast_io::print(2);
}

[[gnu::noinline]] void fast_io_facade_default_fmt_int()
{
	::fast_io::fmt::print<"{}">(2);
}

[[gnu::noinline]] void fast_io_facade_default_raw_boolalpha()
{
	::fast_io::print(::fast_io::mnp::boolalpha(true));
}

[[gnu::noinline]] void fast_io_facade_default_raw_double()
{
	::fast_io::print(3.2);
}

[[gnu::noinline]] void fast_io_facade_default_fmt_double()
{
	::fast_io::fmt::print<"{}">(3.2);
}

} // extern "C"
