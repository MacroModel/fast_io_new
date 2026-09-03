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
#define FAST_IO_CPO_MATRIX_PACK 8
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
The three-character slack prevents this experiment from accidentally becoming
an exact-size protocol.  The returned cursor remains the sole published extent
for both implementations, including empty and very large fragments.
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
[[nodiscard]] inline ::std::string invoke_concat(
	dynamic_source source, ::std::index_sequence<indices...>)
{
	if constexpr (selected_line)
	{
		return ::fast_io::concatln_std(
			(static_cast<void>(indices), source)...);
	}
	else
	{
		return ::fast_io::concat_std(
			(static_cast<void>(indices), source)...);
	}
}

[[nodiscard]] inline ::std::string invoke_concat(dynamic_source source)
{
	return invoke_concat(
		source, ::std::make_index_sequence<selected_pack_count>{});
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
			"usage: concat_dynamic_length_case length [target-ms:20..100]\n",
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
	auto const preflight{invoke_concat(source)};
	if (!validate_result(preflight, token))
	{
		::std::fputs("concat dynamic-length preflight mismatch\n", stderr);
		return 1;
	}
	auto const digest{fast_io_cpo_matrix::oracle::digest_bytes(
		UINT64_C(14695981039346656037), preflight.data(), preflight.size())};

	auto timed_call = [&](::std::size_t) -> ::std::size_t {
		auto result{invoke_concat(source)};
		fast_io_cpo_matrix::compiler_observe_bytes(result.data(), result.size());
		return result.size();
	};
	auto reset_timed_state = []() noexcept {};
	auto const measured{fast_io_cpo_matrix::calibrate_and_measure(
		timed_call, reset_timed_state, target_milliseconds)};
	auto const elapsed_seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"concat-dynamic-length,%zu,%u,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		selected_pack_count, selected_line ? 1u : 0u,
		static_cast<unsigned long long>(length), measured.iterations,
		elapsed_seconds, nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(digest));
}
