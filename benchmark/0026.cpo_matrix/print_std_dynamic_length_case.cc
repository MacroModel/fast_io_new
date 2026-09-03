#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include <fast_io.h>
#include <fast_io_unit/string.h>

#include "case_driver.h"
#include "independent_oracle.h"

#ifndef FAST_IO_CPO_MATRIX_PACK
#define FAST_IO_CPO_MATRIX_PACK 32
#endif

#ifndef FAST_IO_CPO_MATRIX_LINE
#define FAST_IO_CPO_MATRIX_LINE 0
#endif

namespace
{

inline constexpr ::std::size_t selected_pack_count{FAST_IO_CPO_MATRIX_PACK};
inline constexpr bool selected_line{FAST_IO_CPO_MATRIX_LINE != 0};
static_assert(selected_pack_count >= 2u && selected_pack_count <= 64u);
static_assert(FAST_IO_CPO_MATRIX_LINE == 0 || FAST_IO_CPO_MATRIX_LINE == 1);

struct dynamic_source
{
	::std::string_view text{};
};

/*
This is the same unbounded dynamic-reserve shape as the concat length control.
The three-character slack and actual returned cursor make put-area capacity,
not an exact-size protocol, responsible for every direct write.
*/
[[nodiscard]] inline ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_source>,
	dynamic_source source) noexcept
{
	return source.text.size() + 3u;
}

[[nodiscard]] inline char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_source>, char *destination,
	dynamic_source source) noexcept
{
	for (char character : source.text)
	{
		*destination++ = character;
	}
	return destination;
}

template <::std::size_t... indices>
inline void invoke_print(
	::std::string &destination, dynamic_source source,
	::std::index_sequence<indices...>)
{
	auto output{
		::fast_io::io_strlike_ref(::fast_io::io_alias, destination)};
	if constexpr (selected_line)
	{
		::fast_io::println(
			output, (static_cast<void>(indices), source)...);
	}
	else
	{
		::fast_io::print(
			output, (static_cast<void>(indices), source)...);
	}
}

inline void invoke_print(
	::std::string &destination, dynamic_source source)
{
	invoke_print(
		destination, source,
		::std::make_index_sequence<selected_pack_count>{});
}

[[nodiscard]] bool validate_result(
	::std::string const &result, ::std::string_view token) noexcept
{
	auto const expected_size{
		token.size() * selected_pack_count + (selected_line ? 1u : 0u)};
	if (result.size() != expected_size)
	{
		return false;
	}
	for (::std::size_t leaf{}; leaf != selected_pack_count; ++leaf)
	{
		auto const offset{leaf * token.size()};
		for (::std::size_t index{}; index != token.size(); ++index)
		{
			if (result[offset + index] != token[index])
			{
				return false;
			}
		}
	}
	return !selected_line || result.back() == '\n';
}

} // namespace

int main(int argc, char **argv)
{
	fast_io_cpo_matrix::process_deadline_guard process_deadline;
	if (!process_deadline.armed())
	{
		::std::fputs("unable to arm the 800 ms process deadline\n", stderr);
		return 2;
	}
	::std::uint_least64_t length{};
	::std::uint_least64_t target_milliseconds{20u};
	if (argc != 2 && argc != 3)
	{
		::std::fputs(
			"usage: print_std_dynamic_length_case length [target-ms:20..100]\n",
			stderr);
		return 2;
	}
	if (!fast_io_cpo_matrix::parse_unsigned(argv[1], length) ||
		length > UINT64_C(65536) ||
		(argc == 3 &&
		 (!fast_io_cpo_matrix::parse_unsigned(argv[2], target_milliseconds) ||
		  target_milliseconds < 20u || target_milliseconds > 100u)))
	{
		::std::fputs("invalid length or target duration\n", stderr);
		return 2;
	}

	::std::string token(static_cast<::std::size_t>(length), '\0');
	for (::std::size_t index{}; index != token.size(); ++index)
	{
		token[index] = static_cast<char>('!' + (index * 29u + 7u) % 90u);
	}
	dynamic_source source{token};
	::std::string destination;
	invoke_print(destination, source);
	if (!validate_result(destination, token))
	{
		::std::fputs("print std dynamic-length preflight mismatch\n", stderr);
		return 1;
	}
	auto const digest{fast_io_cpo_matrix::oracle::digest_bytes(
		UINT64_C(14695981039346656037), destination.data(),
		destination.size())};

	/*
	The validated preflight establishes enough capacity for the largest timed
	result. clear() retains that allocation, isolating cursor dispatch and payload
	writes from fresh-result construction and allocator-growth effects.
	*/
	auto timed_call = [&](::std::size_t) -> ::std::size_t {
		destination.clear();
		invoke_print(destination, source);
		fast_io_cpo_matrix::compiler_observe_bytes(
			destination.data(), destination.size());
		return destination.size();
	};
	auto reset_timed_state = [&]() noexcept { destination.clear(); };
	auto const measured{fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset_timed_state, target_milliseconds)};
	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"print-std-dynamic-length,%zu,%u,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		selected_pack_count, selected_line ? 1u : 0u,
		static_cast<unsigned long long>(length), measured.iterations,
		elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(digest));
}
