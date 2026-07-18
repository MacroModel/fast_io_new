#include <cstddef>
#include <cstdint>

#include <fast_io.h>

namespace
{

struct line_target;

struct line_proxy
{
	line_target *target;
};

struct line_target
{
	::std::size_t size{};
	::std::uint_least64_t checksum{};
};

inline constexpr line_proxy scan_alias_define(::fast_io::io_alias_t, line_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

/// @brief Minimal terminal line protocol used to fuzz contiguous scan dispatch.
/// @details This harness used to call the disabled experimental `line_scanner` range subsystem and therefore did not
///          compile against the public build. The replacement deliberately owns only the ordinary contiguous scanner
///          CPO: it validates alias normalization, exact result typing, iterator commit, successful delimiter
///          consumption, and clean terminal failure without importing an unrelated numeric parsing algorithm.
inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, line_proxy>, char const *first, char const *last,
	line_proxy &proxy) noexcept
{
	::std::uint_least64_t checksum{};
	auto current{first};
	for (; current != last && *current != '\n'; ++current)
	{
		checksum = checksum * 131u + static_cast<unsigned char>(*current);
	}
	if (current == last)
	{
		return {last, ::fast_io::parse_code::end_of_file};
	}
	proxy.target->size = static_cast<::std::size_t>(current - first);
	proxy.target->checksum = checksum;
	return {current + 1, ::fast_io::parse_code::ok};
}

[[noreturn]] inline void fail() noexcept
{
	__builtin_trap();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		fail();
	}
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data, ::std::size_t size) noexcept
{
	auto const first{reinterpret_cast<char const *>(data)};
	auto const last{size == 0u ? first : first + size};
	::fast_io::basic_ibuffer_view<char> input(first, last);
	if (size == 0u)
	{
		line_target target;
		require(!::fast_io::io::scan<true>(input, target));
		return 0;
	}
	auto expected_begin{first};
	while (expected_begin != last)
	{
		auto delimiter{expected_begin};
		::std::uint_least64_t expected_checksum{};
		for (; delimiter != last && *delimiter != '\n'; ++delimiter)
		{
			expected_checksum = expected_checksum * 131u + static_cast<unsigned char>(*delimiter);
		}

		line_target target;
		bool const completed{::fast_io::io::scan<true>(input, target)};
		if (delimiter == last)
		{
			require(!completed);
			require(input.curr_ptr == input.end_ptr);
			break;
		}

		require(completed);
		require(target.size == static_cast<::std::size_t>(delimiter - expected_begin));
		require(target.checksum == expected_checksum);
		require(input.curr_ptr == delimiter + 1);
		expected_begin = delimiter + 1;
	}
	return 0;
}
