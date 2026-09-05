#include <concepts>
#include <cstddef>
#include <type_traits>

#include <fast_io_core.h>

namespace fast_io_seek_decay_abi_probe
{

struct proxy
{
	using input_char_type = char;
	using output_char_type = char;
	::fast_io::intfpos_t *position{};
	::std::size_t *flushes{};
};

// Copies are substitutable because every operation mutates only the shared
// state addressed by the descriptor. This semantic proof is intentionally
// separate from the target-specific aggregate argument classification.
inline constexpr ::std::true_type stream_ref_value_transport_safe_define(
	::fast_io::io_type_t<proxy>) noexcept
{
	return {};
}

inline constexpr proxy &io_stream_ref_define(proxy &stream) noexcept
{
	return stream;
}

inline ::fast_io::intfpos_t io_stream_seek_define(
	proxy stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	if (direction == ::fast_io::seekdir::beg)
	{
		*stream.position = offset;
	}
	else
	{
		*stream.position += offset;
	}
	return *stream.position;
}

inline ::fast_io::intfpos_t io_stream_seek_bytes_define(
	proxy stream, ::fast_io::intfpos_t offset,
	::fast_io::seekdir direction) noexcept
{
	return io_stream_seek_define(stream, offset, direction);
}

inline void io_stream_buffer_flush_define(proxy stream) noexcept
{
	++*stream.flushes;
}

using seek_value_entry = ::fast_io::intfpos_t (*)(
	proxy, ::fast_io::intfpos_t, ::fast_io::seekdir);
using seek_borrowed_entry = ::fast_io::intfpos_t (*)(
	proxy &, ::fast_io::intfpos_t, ::fast_io::seekdir);
using flush_value_entry = void (*)(proxy);
using flush_borrowed_entry = void (*)(proxy &);
using rewind_value_entry = void (*)(proxy);

static_assert(
	::fast_io::operations::defines::abi_value_io_stream_ref_result<proxy &> ==
	::fast_io::details::abi_small_trivial_argument_object<proxy>());

/*
 * Function types prove the portable API boundary. Retaining their addresses
 * additionally forces both AArch64 and x86-64 assembly to expose the concrete
 * lowering: the value owner receives the two-word descriptor in aggregate
 * argument registers, while the explicitly borrowed entry receives one object
 * address. The dispatch bridge itself remains inlined at named call sites.
 */
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::io_stream_seek_bytes_decay<proxy>),
	seek_value_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed<
		proxy>),
	seek_borrowed_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::output_stream_buffer_flush_decay<
		proxy>),
	flush_value_entry>);
static_assert(::std::same_as<
	decltype(
		&::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed<
			proxy>),
	flush_borrowed_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::io_stream_rewind_bytes_decay<proxy>),
	rewind_value_entry>);

extern "C" [[gnu::used, gnu::noinline]] ::fast_io::intfpos_t
fast_io_public_seek_value_abi_probe(
	proxy stream, ::fast_io::intfpos_t offset) noexcept
{
	return ::fast_io::operations::io_stream_seek_bytes(
		stream, offset, ::fast_io::seekdir::beg);
}

extern "C" [[gnu::used, gnu::noinline]] ::fast_io::intfpos_t
fast_io_named_seek_dispatch_abi_probe(
	proxy &stream, ::fast_io::intfpos_t offset) noexcept
{
	return ::fast_io::operations::decay::io_stream_seek_bytes_decay_dispatch(
		stream, offset, ::fast_io::seekdir::beg);
}

} // namespace fast_io_seek_decay_abi_probe

extern "C"
{
	[[gnu::used]] ::fast_io_seek_decay_abi_probe::seek_value_entry
		fast_io_seek_decay_value_entry{
			&::fast_io::operations::decay::io_stream_seek_bytes_decay<
				::fast_io_seek_decay_abi_probe::proxy>};
	[[gnu::used]] ::fast_io_seek_decay_abi_probe::seek_borrowed_entry
		fast_io_seek_decay_borrowed_entry{
			&::fast_io::operations::decay::io_stream_seek_bytes_decay_borrowed<
				::fast_io_seek_decay_abi_probe::proxy>};
	[[gnu::used]] ::fast_io_seek_decay_abi_probe::flush_value_entry
		fast_io_seek_flush_decay_value_entry{
			&::fast_io::operations::decay::output_stream_buffer_flush_decay<
				::fast_io_seek_decay_abi_probe::proxy>};
	[[gnu::used]] ::fast_io_seek_decay_abi_probe::flush_borrowed_entry
		fast_io_seek_flush_decay_borrowed_entry{
			&::fast_io::operations::decay::output_stream_buffer_flush_decay_borrowed<
				::fast_io_seek_decay_abi_probe::proxy>};
}

int main()
{
	::fast_io::intfpos_t position{};
	::std::size_t flushes{};
	::fast_io_seek_decay_abi_probe::proxy stream{
		__builtin_addressof(position), __builtin_addressof(flushes)};
	return ::fast_io_seek_decay_abi_probe::fast_io_public_seek_value_abi_probe(
			   stream, 42) == 42 &&
			   position == 42 && flushes == 1u
		   ? 0
		   : 1;
}
