#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include <fast_io.h>

namespace fast_io_read_write_decay_abi_probe
{

struct input_proxy
{
	using input_char_type = char;
	char const **current{};
};

struct output_proxy
{
	using output_char_type = char;
	::std::size_t *count{};
};

// Both descriptors hold only pointers to external mutable state. Copying them
// cannot fork stream position, so the semantic marker is valid independently
// of whether a particular target ABI accepts their representation in registers.
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<input_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<output_proxy>) noexcept
{
	return {};
}

inline char *read_some_underflow_define(
	input_proxy &input, char *first, char *last) noexcept
{
	for (char *iter{first}; iter != last; ++iter, ++*input.current)
	{
		*iter = **input.current;
	}
	return last;
}

inline void write_all_overflow_define(
	output_proxy &output, char const *first, char const *last) noexcept
{
	*output.count = static_cast<::std::size_t>(last - first);
}

inline void output_stream_finish_define(output_proxy output) noexcept
{
	++*output.count;
}

inline void input_stream_drain_and_finish_define(input_proxy input) noexcept
{
	++*input.current;
}

using read_value_entry = char *(*)(input_proxy, char *, char *);
using read_borrowed_entry = char *(*)(input_proxy &, char *, char *);
using write_value_entry = void (*)(output_proxy, char const *, char const *);
using write_borrowed_entry = void (*)(output_proxy &, char const *, char const *);
using range_type = ::std::array<char, 4u>;
using write_range_value_entry = void (*)(output_proxy, range_type &);
using write_range_borrowed_entry = void (*)(output_proxy &, range_type &);
using output_finish_value_entry = void (*)(output_proxy);
using output_finish_borrowed_entry = void (*)(output_proxy &);
using input_finish_value_entry = void (*)(input_proxy);
using input_finish_borrowed_entry = void (*)(input_proxy &);
using posix_proxy = ::fast_io::basic_posix_family_io_observer<
	::fast_io::posix_family::api, char>;
using pread_bytes_value_entry = ::std::byte *(*)(posix_proxy, ::std::byte *, ::std::byte *, ::fast_io::intfpos_t);
using pread_bytes_borrowed_entry = ::std::byte *(*)(posix_proxy &, ::std::byte *, ::std::byte *, ::fast_io::intfpos_t);
using pwrite_bytes_value_entry = ::std::byte const *(*)(posix_proxy, ::std::byte const *, ::std::byte const *,
														::fast_io::intfpos_t);
using pwrite_bytes_borrowed_entry = ::std::byte const *(*)(posix_proxy &, ::std::byte const *, ::std::byte const *,
														   ::fast_io::intfpos_t);
using scatter_read_bytes_value_entry = ::fast_io::io_scatter_status_t (*)(
	posix_proxy, ::fast_io::io_scatter_t const *, ::std::size_t);
using scatter_read_bytes_borrowed_entry = ::fast_io::io_scatter_status_t (*)(
	posix_proxy &, ::fast_io::io_scatter_t const *, ::std::size_t);
using scatter_write_bytes_value_entry = ::fast_io::io_scatter_status_t (*)(
	posix_proxy, ::fast_io::io_scatter_t const *, ::std::size_t);
using scatter_write_bytes_borrowed_entry = ::fast_io::io_scatter_status_t (*)(
	posix_proxy &, ::fast_io::io_scatter_t const *, ::std::size_t);

static_assert(::fast_io::operations::defines::stream_ref_value_transport_safe<
			  posix_proxy>);
static_assert(::fast_io::operations::defines::abi_value_io_stream_ref_result<
			  posix_proxy &>);

/*
 * Function types are the portable part of this ABI probe. The unsuffixed decay
 * entry must remain a value parameter, while only the explicitly named helper
 * may expose a reference parameter. Retaining each specialization forces
 * AArch64 and x86-64 assembly runs to show the actual aggregate lowering even
 * when normal direct calls would inline the primitive completely.
 */
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::read_some_decay<input_proxy>),
			  read_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::read_some_decay_borrowed<input_proxy>),
			  read_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::write_all_decay<output_proxy>),
			  write_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::write_all_decay_borrowed<output_proxy>),
			  write_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::write_all_range_decay<
					   output_proxy, range_type &>),
			  write_range_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::write_all_range_decay_borrowed_output<
					   output_proxy, range_type &>),
			  write_range_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::output_stream_finish_decay<
					   output_proxy>),
			  output_finish_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::output_stream_finish_decay_borrowed<
					   output_proxy>),
			  output_finish_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::input_stream_drain_and_finish_decay<
					   input_proxy>),
			  input_finish_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::
						   input_stream_drain_and_finish_decay_borrowed<input_proxy>),
			  input_finish_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::pread_some_bytes_decay<posix_proxy>),
			  pread_bytes_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::pread_some_bytes_decay_borrowed<posix_proxy>),
			  pread_bytes_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::pwrite_some_bytes_decay<posix_proxy>),
			  pwrite_bytes_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::pwrite_some_bytes_decay_borrowed<posix_proxy>),
			  pwrite_bytes_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::scatter_read_some_bytes_decay<posix_proxy>),
			  scatter_read_bytes_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::scatter_read_some_bytes_decay_borrowed<posix_proxy>),
			  scatter_read_bytes_borrowed_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::scatter_write_some_bytes_decay<posix_proxy>),
			  scatter_write_bytes_value_entry>);
static_assert(::std::same_as<
			  decltype(&::fast_io::operations::decay::scatter_write_some_bytes_decay_borrowed<posix_proxy>),
			  scatter_write_bytes_borrowed_entry>);

/*
 * These exported, non-inlined wrappers retain the actual public API lowering.
 * Assembly inspection must show the POSIX descriptor arriving as the scalar
 * `fd` value on both AArch64 and x86-64; neither wrapper may first load it from
 * an address supplied for the observer object.
 */
extern "C" [[gnu::used, gnu::noinline]] char *fast_io_public_read_fd_abi_probe(
	posix_proxy input, char *first, char *last)
{
	return ::fast_io::operations::read_some(input, first, last);
}

extern "C" [[gnu::used, gnu::noinline]] void fast_io_public_print_fd_abi_probe(
	posix_proxy output, ::std::uint_least64_t value)
{
	::fast_io::io::print(output, value);
}

extern "C" [[gnu::used, gnu::noinline]] void
fast_io_public_write_range_fd_abi_probe(posix_proxy output, range_type &range)
{
	// The scalar descriptor must stay in its native argument register while the
	// independently forwarded range remains one borrowed aggregate.
	::fast_io::operations::write_all_range(output, range);
}

extern "C" [[gnu::used, gnu::noinline]] ::std::byte *
fast_io_public_pread_fd_abi_probe(
	posix_proxy input, ::std::byte *first, ::std::byte *last,
	::fast_io::intfpos_t off)
{
	return ::fast_io::operations::pread_some_bytes(
		input, first, last, off);
}

extern "C" [[gnu::used, gnu::noinline]] ::std::byte const *
fast_io_public_pwrite_fd_abi_probe(
	posix_proxy output, ::std::byte const *first, ::std::byte const *last,
	::fast_io::intfpos_t off)
{
	return ::fast_io::operations::pwrite_some_bytes(
		output, first, last, off);
}

extern "C" [[gnu::used, gnu::noinline]] ::fast_io::io_scatter_status_t
fast_io_public_scatter_read_fd_abi_probe(
	posix_proxy input, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count)
{
	return ::fast_io::operations::scatter_read_some_bytes(
		input, scatters, count);
}

extern "C" [[gnu::used, gnu::noinline]] ::fast_io::io_scatter_status_t
fast_io_public_scatter_write_fd_abi_probe(
	posix_proxy output, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count)
{
	return ::fast_io::operations::scatter_write_some_bytes(
		output, scatters, count);
}

} // namespace fast_io_read_write_decay_abi_probe

extern "C"
{
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::read_value_entry
		fast_io_read_decay_value_entry{
			&::fast_io::operations::decay::read_some_decay<
				::fast_io_read_write_decay_abi_probe::input_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::read_borrowed_entry
		fast_io_read_decay_borrowed_entry{
			&::fast_io::operations::decay::read_some_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::input_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::write_value_entry
		fast_io_write_decay_value_entry{
			&::fast_io::operations::decay::write_all_decay<
				::fast_io_read_write_decay_abi_probe::output_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::write_borrowed_entry
		fast_io_write_decay_borrowed_entry{
			&::fast_io::operations::decay::write_all_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::output_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::write_range_value_entry
		fast_io_write_range_decay_value_entry{
			&::fast_io::operations::decay::write_all_range_decay<
				::fast_io_read_write_decay_abi_probe::output_proxy,
				::fast_io_read_write_decay_abi_probe::range_type &>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::write_range_borrowed_entry
		fast_io_write_range_decay_borrowed_entry{
			&::fast_io::operations::decay::write_all_range_decay_borrowed_output<
				::fast_io_read_write_decay_abi_probe::output_proxy,
				::fast_io_read_write_decay_abi_probe::range_type &>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::output_finish_value_entry
		fast_io_output_finish_decay_value_entry{
			&::fast_io::operations::decay::output_stream_finish_decay<
				::fast_io_read_write_decay_abi_probe::output_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::output_finish_borrowed_entry
		fast_io_output_finish_decay_borrowed_entry{
			&::fast_io::operations::decay::output_stream_finish_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::output_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::input_finish_value_entry
		fast_io_input_finish_decay_value_entry{
			&::fast_io::operations::decay::input_stream_drain_and_finish_decay<
				::fast_io_read_write_decay_abi_probe::input_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::input_finish_borrowed_entry
		fast_io_input_finish_decay_borrowed_entry{
			&::fast_io::operations::decay::
				input_stream_drain_and_finish_decay_borrowed<
					::fast_io_read_write_decay_abi_probe::input_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::pread_bytes_value_entry
		fast_io_pread_bytes_decay_value_entry{
			&::fast_io::operations::decay::pread_some_bytes_decay<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::pread_bytes_borrowed_entry
		fast_io_pread_bytes_decay_borrowed_entry{
			&::fast_io::operations::decay::pread_some_bytes_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::pwrite_bytes_value_entry
		fast_io_pwrite_bytes_decay_value_entry{
			&::fast_io::operations::decay::pwrite_some_bytes_decay<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::pwrite_bytes_borrowed_entry
		fast_io_pwrite_bytes_decay_borrowed_entry{
			&::fast_io::operations::decay::pwrite_some_bytes_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::scatter_read_bytes_value_entry
		fast_io_scatter_read_bytes_decay_value_entry{
			&::fast_io::operations::decay::scatter_read_some_bytes_decay<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::scatter_read_bytes_borrowed_entry
		fast_io_scatter_read_bytes_decay_borrowed_entry{
			&::fast_io::operations::decay::scatter_read_some_bytes_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::scatter_write_bytes_value_entry
		fast_io_scatter_write_bytes_decay_value_entry{
			&::fast_io::operations::decay::scatter_write_some_bytes_decay<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
	[[gnu::used]] ::fast_io_read_write_decay_abi_probe::scatter_write_bytes_borrowed_entry
		fast_io_scatter_write_bytes_decay_borrowed_entry{
			&::fast_io::operations::decay::scatter_write_some_bytes_decay_borrowed<
				::fast_io_read_write_decay_abi_probe::posix_proxy>};
}
