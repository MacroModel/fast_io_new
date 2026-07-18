#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

#include <fast_io.h>

namespace
{

using scatter = ::fast_io::basic_io_scatter_t<char>;

struct scalar_state
{
	::std::string output;
	::std::array<char const *, 4u> firsts{};
	::std::array<::std::size_t, 4u> sizes{};
	::std::size_t calls{};
	::std::size_t char_put_calls{};
};

struct direct_sink
{
	using output_char_type = char;
	scalar_state *state;
};

inline constexpr direct_sink output_stream_ref_define(direct_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, direct_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(direct_sink sink, char const *first, char const *last)
{
	assert(first != nullptr);
	assert(last != nullptr);
	auto const size{static_cast<::std::size_t>(last - first)};
	assert(sink.state->calls < sink.state->firsts.size());
	sink.state->firsts[sink.state->calls] = first;
	sink.state->sizes[sink.state->calls] = size;
	++sink.state->calls;
	sink.state->output.append(first, size);
}

[[maybe_unused]] inline void output_stream_char_put_overflow_define(direct_sink sink, char ch)
{
	++sink.state->char_put_calls;
	sink.state->output.push_back(ch);
}

struct status_state
{
	::std::size_t status_calls[2]{};
	::std::size_t scalar_calls{};
};

struct status_sink
{
	using output_char_type = char;
	status_state *state;
};

inline constexpr status_sink output_stream_ref_define(status_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, status_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline void write_all_overflow_define(
	status_sink sink, char const *, char const *) noexcept
{
	++sink.state->scalar_calls;
}

template <bool line>
inline void status_print_define(status_sink sink, scatter, scatter) noexcept
{
	++sink.state->status_calls[static_cast<::std::size_t>(line)];
}

struct byte_state
{
	::std::string output;
	::std::size_t typed_calls{};
	::std::size_t byte_calls{};
};

struct byte_sink
{
	using output_char_type = char;
	byte_state *state;
};

inline constexpr byte_sink output_stream_ref_define(byte_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, byte_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline void write_all_overflow_define(
	byte_sink sink, char const *, char const *) noexcept
{
	++sink.state->typed_calls;
}

inline void write_all_bytes_overflow_define(
	byte_sink sink, ::std::byte const *first, ::std::byte const *last)
{
	++sink.state->byte_calls;
	sink.state->output.append(
		reinterpret_cast<char const *>(first), static_cast<::std::size_t>(last - first));
}

struct native_scatter_state
{
	::std::string output;
	::std::size_t scalar_calls{};
	::std::size_t scatter_calls{};
	::std::size_t descriptors{};
};

struct native_scatter_sink
{
	using output_char_type = char;
	native_scatter_state *state;
};

inline constexpr native_scatter_sink output_stream_ref_define(native_scatter_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, native_scatter_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(native_scatter_sink sink, char const *, char const *) noexcept
{
	++sink.state->scalar_calls;
}

inline void scatter_write_all_overflow_define(
	native_scatter_sink sink, scatter const *scatters, ::std::size_t count)
{
	++sink.state->scatter_calls;
	sink.state->descriptors += count;
	for (::std::size_t index{}; index != count; ++index)
	{
		sink.state->output.append(scatters[index].base, scatters[index].len);
	}
}

struct wide_byte_scatter_state
{
	::std::size_t typed_calls{};
	::std::size_t byte_scatter_calls{};
	::std::size_t descriptors{};
};

struct wide_byte_scatter_sink
{
	using output_char_type = char16_t;
	wide_byte_scatter_state *state;
};

inline constexpr wide_byte_scatter_sink output_stream_ref_define(wide_byte_scatter_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char16_t, wide_byte_scatter_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline void write_all_overflow_define(
	wide_byte_scatter_sink sink, char16_t const *, char16_t const *) noexcept
{
	++sink.state->typed_calls;
}

inline void scatter_write_all_bytes_overflow_define(
	wide_byte_scatter_sink sink, ::fast_io::io_scatter_t const *, ::std::size_t count) noexcept
{
	++sink.state->byte_scatter_calls;
	sink.state->descriptors += count;
}

struct throwing_state
{
	::std::size_t calls{};
	::std::string prefix;
};

struct throwing_sink
{
	using output_char_type = char;
	throwing_state *state;
};

inline constexpr throwing_sink output_stream_ref_define(throwing_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, throwing_sink>) noexcept
{
	return {};
}

inline void write_all_overflow_define(throwing_sink sink, char const *first, char const *last)
{
	++sink.state->calls;
	if (sink.state->calls == 2u)
	{
		throw 42;
	}
	sink.state->prefix.append(first, static_cast<::std::size_t>(last - first));
}

struct coalesce_sink
{
	using output_char_type = char;
};

[[maybe_unused]] inline constexpr ::std::true_type print_direct_streaming_preferred_stream(
	::fast_io::io_reserve_type_t<char, coalesce_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline constexpr ::std::true_type print_semantic_plain_leaf_coalesce_preferred_stream(
	::fast_io::io_reserve_type_t<char, coalesce_sink>) noexcept
{
	return {};
}

[[maybe_unused]] inline void write_all_overflow_define(
	coalesce_sink, char const *, char const *) noexcept
{}

static_assert(
	::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, direct_sink, scatter, scatter>);
static_assert(
	::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		true, char, direct_sink, scatter, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, direct_sink, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, direct_sink, scatter, scatter, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, byte_sink, scatter, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, native_scatter_sink, scatter, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, status_sink, scatter, scatter>);
static_assert(
	!::fast_io::operations::decay::print_semantic_exact_two_passive_scatter_direct_write_v<
		false, char, coalesce_sink, scatter, scatter>);

inline void verify_direct_and_line_boundaries()
{
	using namespace ::std::literals;
	auto value{::fast_io::mnp::pack("a"sv, "bc"sv)};
	scalar_state plain;
	::fast_io::print(direct_sink{&plain}, value);
	assert(plain.output == "abc");
	assert(plain.calls == 2u);
	assert(plain.sizes[0] == 1u && plain.sizes[1] == 2u);
	assert(plain.char_put_calls == 0u);

	scalar_state line;
	::fast_io::println(direct_sink{&line}, value);
	assert(line.output == "abc\n");
	assert(line.calls == 3u);
	assert(line.sizes[0] == 1u && line.sizes[1] == 2u && line.sizes[2] == 1u);
	assert(line.firsts[2] == ::fast_io::details::decay::line_scatter_common<char>.base);
	assert(line.char_put_calls == 0u);
}

inline void verify_zero_falls_back_without_losing_empty_calls()
{
	scatter empty{nullptr, 0u};
	scatter full{"x", 1u};
	auto value{::fast_io::mnp::pack(empty, full)};
	scalar_state plain;
	::fast_io::print(direct_sink{&plain}, value);
	assert(plain.output == "x");
	assert(plain.calls == 2u);
	assert(plain.firsts[0] != nullptr && plain.sizes[0] == 0u);

	scalar_state line;
	::fast_io::println(direct_sink{&line}, value);
	assert(line.output == "x\n");
	assert(line.calls == 3u);
	assert(line.firsts[0] != nullptr && line.sizes[0] == 0u);
}

inline void verify_status_and_representation_gates()
{
	using namespace ::std::literals;
	auto value{::fast_io::mnp::pack("a"sv, "bc"sv)};
	status_state status;
	::fast_io::print(status_sink{&status}, value);
	::fast_io::println(status_sink{&status}, value);
	assert(status.status_calls[0] == 1u && status.status_calls[1] == 1u);
	assert(status.scalar_calls == 0u);

	byte_state bytes;
	::fast_io::print(byte_sink{&bytes}, value);
	assert(bytes.output == "abc");
	assert(bytes.typed_calls == 0u && bytes.byte_calls == 2u);
	bytes = {};
	::fast_io::println(byte_sink{&bytes}, value);
	assert(bytes.output == "abc\n");
	assert(bytes.typed_calls == 0u && bytes.byte_calls == 3u);

	native_scatter_state native;
	::fast_io::print(native_scatter_sink{&native}, value);
	assert(native.output == "abc");
	assert(native.scalar_calls == 0u && native.scatter_calls == 1u && native.descriptors == 2u);
	native = {};
	::fast_io::println(native_scatter_sink{&native}, value);
	assert(native.output == "abc\n");
	assert(native.scalar_calls == 0u && native.scatter_calls == 1u && native.descriptors == 3u);

	auto wide_value{::fast_io::mnp::pack(u"a"sv, u"bc"sv)};
	wide_byte_scatter_state wide;
	::fast_io::print(wide_byte_scatter_sink{&wide}, wide_value);
	assert(wide.typed_calls == 0u && wide.byte_scatter_calls == 1u && wide.descriptors == 2u);
	wide = {};
	::fast_io::println(wide_byte_scatter_sink{&wide}, wide_value);
	assert(wide.typed_calls == 0u && wide.byte_scatter_calls == 1u && wide.descriptors == 3u);
}

inline void verify_exception_prefix_and_order()
{
	using namespace ::std::literals;
	auto value{::fast_io::mnp::pack("a"sv, "bc"sv)};
	for (bool line : {false, true})
	{
		throwing_state state;
		try
		{
			if (line)
			{
				::fast_io::println(throwing_sink{&state}, value);
			}
			else
			{
				::fast_io::print(throwing_sink{&state}, value);
			}
			assert(false);
		}
		catch (int error)
		{
			assert(error == 42);
		}
		assert(state.calls == 2u);
		assert(state.prefix == "a");
	}
}

} // namespace

int main()
{
	verify_direct_and_line_boundaries();
	verify_zero_falls_back_without_losing_empty_calls();
	verify_status_and_representation_gates();
	verify_exception_prefix_and_order();
}
