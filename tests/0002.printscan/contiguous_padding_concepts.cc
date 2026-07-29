#include <fast_io.h>

#include <cstdlib>

namespace
{

struct padded_only_target
{
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, padded_only_target>, char const *first,
	char const *last, ::std::size_t padding, padded_only_target &) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::end_of_file};
	}
	// The test customization observes the capability value without treating padding as semantic input.
	static_cast<void>(padding);
	return {first + 1u, ::fast_io::parse_code::ok};
}

struct wrong_padded_result
{
};

inline constexpr ::fast_io::parse_result<char *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, wrong_padded_result>, char const *, char const *,
	::std::size_t, wrong_padded_result &) noexcept
{
	return {};
}

struct throwing_padding_range
{
};

inline ::std::size_t contiguous_range_padding_size(throwing_padding_range const &)
{
	return 0u;
}

struct wrong_padding_range
{
};

inline constexpr unsigned
contiguous_range_padding_size(wrong_padding_range const &) noexcept
{
	return 0u;
}

static_assert(::fast_io::contiguous_scannable_with_padding<char, padded_only_target>);
static_assert(!::fast_io::contiguous_scannable<char, padded_only_target>);
static_assert(!::fast_io::context_scannable<char, padded_only_target>);
static_assert(!::fast_io::contiguous_scannable_with_padding<char, wrong_padded_result>);

static_assert(::fast_io::contiguous_range_with_padding<::fast_io::allocation_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::native_file_loader>);
#if defined(_WIN32) || defined(__CYGWIN__)
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::nt_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::zw_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader_9xa>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader_ntw>);
#endif
#if !defined(_WIN32) && !defined(__MSDOS__) && (!defined(__wasm__) || defined(_WASI_EMULATED_MMAN)) && \
	!defined(_PICOLIBC__) && (!defined(__NEWLIB__) || defined(__CYGWIN__))
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::posix_file_loader>);
#endif
static_assert(!::fast_io::contiguous_range_with_padding<throwing_padding_range>);
static_assert(!::fast_io::contiguous_range_with_padding<wrong_padding_range>);

} // namespace

int main()
{
	::fast_io::allocation_file_loader allocation;
	::fast_io::native_file_loader native;
	if (::fast_io::contiguous_range_padding_size(allocation) != 0u ||
		::fast_io::contiguous_range_padding_size(native) != 0u)
	{
		return 1;
	}

	auto memory{static_cast<char *>(::std::malloc(3u))};
	if (memory == nullptr)
	{
		return 2;
	}
	{
		memory[0] = 'x';
		::fast_io::allocation_file_loader padded{
			::fast_io::released_allocation_file_loader_mapping{
				memory, memory + 1u, memory + 3u, -1}};
		if (::fast_io::contiguous_range_padding_size(padded) != 2u ||
			padded.size() != 1u || padded.end() != memory + 1u)
		{
			return 3;
		}
	}

	char storage[2]{'x', '\0'};
	padded_only_target target;
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, padded_only_target>, storage, storage + 1u, 1u, target)};
	return result.iter == storage + 1u && result.code == ::fast_io::parse_code::ok ? 0 : 4;
}
