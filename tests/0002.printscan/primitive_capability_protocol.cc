#include <array>
#include <cassert>
#include <cstddef>

#include <fast_io_core.h>

namespace
{

struct capture_state
{
	std::size_t calls{};
	std::size_t descriptors{};
	std::size_t elements{};
	fast_io::intfpos_t offset{};
};

struct only_scatter_write_all
{
	using output_char_type = char;
	capture_state *state;
};

inline void scatter_write_all_overflow_define(only_scatter_write_all sink,
											  fast_io::basic_io_scatter_t<char> const *scatter, std::size_t count)
{
	++sink.state->calls;
	sink.state->descriptors += count;
	for (std::size_t i{}; i != count; ++i)
	{
		sink.state->elements += scatter[i].len;
	}
}

struct only_scatter_read_all
{
	using input_char_type = char;
	capture_state *state;
};

inline void scatter_read_all_underflow_define(only_scatter_read_all source,
											 fast_io::basic_io_scatter_t<char> const *scatter, std::size_t count)
{
	++source.state->calls;
	source.state->descriptors += count;
	for (std::size_t i{}; i != count; ++i)
	{
		source.state->elements += scatter[i].len;
		auto *base{const_cast<char *>(scatter[i].base)};
		for (std::size_t j{}; j != scatter[i].len; ++j)
		{
			base[j] = 'r';
		}
	}
}

struct only_scatter_pwrite_all
{
	using output_char_type = wchar_t;
	capture_state *state;
};

inline void scatter_pwrite_all_overflow_define(only_scatter_pwrite_all sink,
											   fast_io::basic_io_scatter_t<wchar_t> const *scatter, std::size_t count,
											   fast_io::intfpos_t offset)
{
	++sink.state->calls;
	sink.state->offset = offset;
	sink.state->descriptors += count;
	for (std::size_t i{}; i != count; ++i)
	{
		sink.state->elements += scatter[i].len;
	}
}

struct only_scatter_pread_all
{
	using input_char_type = wchar_t;
	capture_state *state;
};

inline void scatter_pread_all_underflow_define(only_scatter_pread_all source,
											  fast_io::basic_io_scatter_t<wchar_t> const *scatter, std::size_t count,
											  fast_io::intfpos_t offset)
{
	++source.state->calls;
	source.state->offset = offset;
	source.state->descriptors += count;
	for (std::size_t i{}; i != count; ++i)
	{
		source.state->elements += scatter[i].len;
		auto *base{const_cast<wchar_t *>(scatter[i].base)};
		for (std::size_t j{}; j != scatter[i].len; ++j)
		{
			base[j] = L'r';
		}
	}
}

struct bad_output_results
{
	using output_char_type = char;
};

inline int write_some_overflow_define(bad_output_results, char const *, char const *) { return 0; }
inline int write_all_overflow_define(bad_output_results, char const *, char const *) { return 0; }
inline void scatter_write_some_overflow_define(bad_output_results, fast_io::basic_io_scatter_t<char> const *,
												  std::size_t)
{}

struct bad_input_results
{
	using input_char_type = char;
};

inline int read_some_underflow_define(bad_input_results, char *, char *) { return 0; }
inline int read_all_underflow_define(bad_input_results, char *, char *) { return 0; }
inline void scatter_read_some_underflow_define(bad_input_results, fast_io::basic_io_scatter_t<char> const *,
												 std::size_t)
{}

struct exact_bool_line_buffer_query
{};

inline bool obuffer_is_line_buffering_define(exact_bool_line_buffer_query) noexcept { return false; }

struct integer_line_buffer_query
{};

inline int obuffer_is_line_buffering_define(integer_line_buffer_query) noexcept { return 0; }

struct void_line_buffer_query
{};

inline void obuffer_is_line_buffering_define(void_line_buffer_query) noexcept {}

using namespace fast_io::operations::decay::defines;

static_assert(has_scatter_write_all_overflow_define<only_scatter_write_all>);
static_assert(!has_scatter_write_some_overflow_define<only_scatter_write_all>);
static_assert(has_scatter_read_all_underflow_define<only_scatter_read_all>);
static_assert(!has_scatter_read_some_underflow_define<only_scatter_read_all>);
static_assert(has_scatter_pwrite_all_overflow_define<only_scatter_pwrite_all>);
static_assert(!has_scatter_pwrite_some_overflow_define<only_scatter_pwrite_all>);
static_assert(has_scatter_pread_all_underflow_define<only_scatter_pread_all>);
static_assert(!has_scatter_pread_some_underflow_define<only_scatter_pread_all>);

// Recognition must reject a callable expression when its result cannot satisfy the selected decay protocol.
static_assert(!has_write_some_overflow_define<bad_output_results>);
static_assert(!has_write_all_overflow_define<bad_output_results>);
static_assert(!has_scatter_write_some_overflow_define<bad_output_results>);
static_assert(!has_read_some_underflow_define<bad_input_results>);
static_assert(!has_read_all_underflow_define<bad_input_results>);
static_assert(!has_scatter_read_some_underflow_define<bad_input_results>);

// The query is a run-time bool protocol. Merely finding an identically named callable must not opt a stream into the
// defensive cursor policy used by line-buffer-aware write paths.
static_assert(has_obuffer_is_line_buffering_define<exact_bool_line_buffer_query>);
static_assert(!has_obuffer_is_line_buffering_define<integer_line_buffer_query>);
static_assert(!has_obuffer_is_line_buffering_define<void_line_buffer_query>);

} // namespace

int main()
{
	std::array<char, 4> out_chars{'a', 'b', 'c', 'd'};
	std::array<char, 4> in_chars{};
	capture_state write_state{};
	capture_state read_state{};

	// Scalar decay synthesized from scatter-all must use successful void return as completion, never as a status.
	auto const *write_end{fast_io::operations::decay::write_some_decay(
		only_scatter_write_all{&write_state}, out_chars.data(), out_chars.data() + out_chars.size())};
	auto *read_end{fast_io::operations::decay::read_some_decay(
		only_scatter_read_all{&read_state}, in_chars.data(), in_chars.data() + in_chars.size())};
	assert(write_end == out_chars.data() + out_chars.size());
	assert(read_end == in_chars.data() + in_chars.size());
	assert(write_state.calls == 1u && write_state.descriptors == 1u && write_state.elements == out_chars.size());
	assert(read_state.calls == 1u && read_state.descriptors == 1u && read_state.elements == in_chars.size());

	std::array<wchar_t, 3> wide_out{L'a', L'b', L'c'};
	std::array<wchar_t, 3> wide_in{};
	capture_state pwrite_state{};
	capture_state pread_state{};
	fast_io::operations::decay::pwrite_all_decay(only_scatter_pwrite_all{&pwrite_state}, wide_out.data(),
												 wide_out.data() + wide_out.size(), 7);
	fast_io::operations::decay::pread_all_decay(only_scatter_pread_all{&pread_state}, wide_in.data(),
												wide_in.data() + wide_in.size(), 9);
	assert(pwrite_state.calls == 1u && pwrite_state.offset == 7 && pwrite_state.elements == wide_out.size());
	assert(pread_state.calls == 1u && pread_state.offset == 9 && pread_state.elements == wide_in.size());
}
