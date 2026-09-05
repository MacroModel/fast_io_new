#include <concepts>
#include <cstddef>

#include <fast_io_core.h>

namespace fast_io_to_decay_abi_probe
{

struct source_word
{
	char const *data{};
	::std::size_t size{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, source_word>, source_word source) noexcept
{
	return source.size;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, source_word>, char *destination,
	source_word source) noexcept
{
	for (::std::size_t index{}; index != source.size; ++index)
		destination[index] = source.data[index];
	return destination + source.size;
}

struct target_proxy
{
	char *destination{};
	::std::size_t *size{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, target_proxy>, char const *first,
	char const *last, target_proxy &target) noexcept
{
	::std::size_t const length{static_cast<::std::size_t>(last - first)};
	for (::std::size_t index{}; index != length; ++index)
		target.destination[index] = first[index];
	*target.size = length;
	return {last, ::fast_io::parse_code::ok};
}

using value_decay_entry = void (*)(target_proxy, source_word);
using borrowed_target_entry = void (*)(target_proxy &, source_word);

// These types are deliberately two-word aggregates, which are directly transported by the common AAPCS64 and SysV
// AMD64 ABIs. The assertions pin the public decay owner to that value classification while retaining a distinct entry
// for an alias CPO that explicitly returns a stable lvalue.
static_assert(::std::same_as<
	decltype(&::fast_io::basic_inplace_to_decay<
		char, target_proxy, source_word>),
	value_decay_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::basic_inplace_to_decay_borrowed_target<
		char, target_proxy, source_word>),
	borrowed_target_entry>);

} // namespace fast_io_to_decay_abi_probe

// Retaining the specialization as an object-file symbol lets cross-target assembly checks inspect the real parameter
// lowering independently of inlining at the public call site. The relocation is evidence-only and is never read by the
// timed entry below.
extern "C"
{
[[gnu::used]] ::fast_io_to_decay_abi_probe::value_decay_entry
	fast_io_to_decay_value_entry{
		&::fast_io::basic_inplace_to_decay<
			char, ::fast_io_to_decay_abi_probe::target_proxy,
			::fast_io_to_decay_abi_probe::source_word>};
}

extern "C" [[gnu::noinline]] ::std::size_t fast_io_to_decay_abi(
	char *destination, char const *source, ::std::size_t size)
{
	::std::size_t written{};
	::fast_io::basic_inplace_to_decay<char>(
		::fast_io_to_decay_abi_probe::target_proxy{
			destination, __builtin_addressof(written)},
		::fast_io_to_decay_abi_probe::source_word{source, size});
	return written;
}
