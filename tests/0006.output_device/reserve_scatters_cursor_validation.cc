#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct capture_sink
{
	using output_char_type = char;
	::std::string *output;
};

inline constexpr capture_sink output_stream_ref_define(capture_sink sink) noexcept
{
	return sink;
}

inline void scatter_write_all_overflow_define(
	capture_sink sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count)
{
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.output->append(scatters[index].base, scatters[index].len);
	}
}

struct short_static_plan
{
	char value;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, short_static_plan>) noexcept
{
	// Deliberately advertise spare capacity in both arrays. Returning an earlier cursor is part of the protocol.
	return {3u, 4u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, short_static_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	short_static_plan value) noexcept
{
	*reserve = value.value;
	*scatters = {reserve, 1u};
	return {scatters + 1u, reserve + 1u};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, short_static_plan>) noexcept
{
	// Every descriptor names the caller-provided reserve slice, whose lifetime covers the final synchronous write.
	return {};
}

struct short_dynamic_plan
{
	::std::string_view text;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, short_dynamic_plan>,
	short_dynamic_plan value) noexcept
{
	return value.text.size() + 4u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, short_dynamic_plan>, char *destination,
	short_dynamic_plan value) noexcept
{
	for (char ch : value.text)
	{
		*destination++ = ch;
	}
	return destination;
}

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, short_dynamic_plan>,
	short_dynamic_plan value) noexcept
{
	return {3u, value.text.size() + 4u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, short_dynamic_plan>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	short_dynamic_plan value) noexcept
{
	char *const end{print_reserve_define(
		::fast_io::io_reserve_type<char, short_dynamic_plan>, reserve, value)};
	*scatters = {reserve, value.text.size()};
	return {scatters + 1u, end};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, short_dynamic_plan>) noexcept
{
	return {};
}

consteval bool constant_cursor_predicate_probes()
{
	::fast_io::basic_io_scatter_t<char> scatters[4u]{};
	char reserve[5u]{};
	return ::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			   scatters, scatters + 3u, scatters + 3u) &&
		   !::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			   scatters, scatters + 3u, scatters + 4u) &&
		   ::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			   reserve, reserve + 4u, reserve + 4u) &&
		   !::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			   reserve, reserve + 3u, reserve + 4u);
}

static_assert(constant_cursor_predicate_probes());
static_assert(::fast_io::reserve_scatters_printable<char, short_static_plan>);
static_assert(::fast_io::dynamic_reserve_scatters_printable<char, short_dynamic_plan>);

} // namespace

int main()
{
	// Exercise the run-time integer-address branch with a pointer that remains inside the real array but lies outside
	// the producer's declared component slice. Rejection happens without subtracting the untrusted cursor.
	::fast_io::basic_io_scatter_t<char> probe[4u]{};
	assert(!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
		probe, probe + 2u, probe + 3u));

	::std::string output;
	capture_sink sink{__builtin_addressof(output)};
	::fast_io::print(sink, short_static_plan{'A'}, short_static_plan{'B'});
	::fast_io::println(sink, short_static_plan{'C'});
	assert(output == "ABC\n");

	output.clear();
	::fast_io::print(
		sink, short_dynamic_plan{"left"}, short_dynamic_plan{"|"},
		short_dynamic_plan{"right"});
	::fast_io::println(sink, short_dynamic_plan{"!"});
	assert(output == "left|right!\n");
}
