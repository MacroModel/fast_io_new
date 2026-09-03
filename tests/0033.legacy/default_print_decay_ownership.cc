#define FAST_IO_DISABLE_FLOATING_POINT

#include <cstddef>
#include <cstdlib>

#include <fast_io.h>

namespace default_print_decay_ownership
{

inline constexpr char empty_storage{};

struct move_only_proxy
{
	move_only_proxy() = default;
	move_only_proxy(move_only_proxy const &) = delete;
	move_only_proxy &operator=(move_only_proxy const &) = delete;
	move_only_proxy(move_only_proxy &&) = default;
	move_only_proxy &operator=(move_only_proxy &&) = default;
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, move_only_proxy>, move_only_proxy &) noexcept
{
	return {__builtin_addressof(empty_storage), 0u};
}

struct counted_proxy
{
	::std::size_t *copies{};

	inline explicit constexpr counted_proxy(::std::size_t &count) noexcept
		: copies(__builtin_addressof(count))
	{}

	inline counted_proxy(counted_proxy const &other) noexcept : copies(other.copies)
	{
		++*copies;
	}
};

inline constexpr ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, counted_proxy>, counted_proxy &) noexcept
{
	return {__builtin_addressof(empty_storage), 0u};
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

inline void check_move_only_owner()
{
	// Each helper is the sole owner of its already-normalized proxy. The empty scatter keeps the test observable only
	// through type validity: any nested by-value decay boundary would require the deleted copy constructor.
	::fast_io::details::print_after_io_print_forward<false>(move_only_proxy{});
	::fast_io::details::perr_after_io_print_forward<false>(move_only_proxy{});
	::fast_io::details::debug_print_after_io_print_forward<false>(move_only_proxy{});
}

inline void check_single_owner_copy_bound()
{
	::std::size_t copies{};
	counted_proxy proxy{copies};
	::fast_io::details::print_after_io_print_forward<false>(proxy);
	require(copies == 1u);
	::fast_io::details::perr_after_io_print_forward<false>(proxy);
	require(copies == 2u);
	::fast_io::details::debug_print_after_io_print_forward<false>(proxy);
	require(copies == 3u);
}

static_assert(::fast_io::scatter_printable_for<char, move_only_proxy &>);
static_assert(::fast_io::scatter_printable_for<char, counted_proxy &>);

} // namespace default_print_decay_ownership

int main()
{
	::default_print_decay_ownership::check_move_only_owner();
	::default_print_decay_ownership::check_single_owner_copy_bound();
}
