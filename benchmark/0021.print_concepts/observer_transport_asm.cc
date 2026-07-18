#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io.h>

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_OBSERVER_ASM_NOINLINE [[gnu::noinline]]
#else
#define FAST_IO_OBSERVER_ASM_NOINLINE
#endif

namespace observer_transport_asm
{

struct transport_state
{
	::std::size_t copies{};
};

template <typename tag>
struct large_observer
{
	using output_char_type = char;
	transport_state *state{};
	::std::array<::std::uintptr_t, 15u> payload{};

	constexpr explicit large_observer(transport_state *state_pointer) noexcept : state{state_pointer} {}

	FAST_IO_OBSERVER_ASM_NOINLINE large_observer(large_observer const &other) noexcept
		: state{other.state}, payload{other.payload}
	{
		++state->copies;
	}

	large_observer &operator=(large_observer const &) = delete;
};

struct stable_tag
{};

struct owned_tag
{};

using stable_observer = large_observer<stable_tag>;
using owned_observer = large_observer<owned_tag>;

inline constexpr stable_observer &output_stream_ref_define(stable_observer &observer) noexcept
{
	return observer;
}

inline owned_observer output_stream_ref_define(owned_observer &observer) noexcept
{
	// This is the intentional one-owner control: the public normalization boundary must copy once, and no recursive
	// print strategy may copy that owned proxy again.
	return observer;
}

template <typename tag>
FAST_IO_OBSERVER_ASM_NOINLINE inline void write_all_overflow_define(
	large_observer<tag> &observer, char const *first, char const *last) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "r"(__builtin_addressof(observer)), "r"(first), "r"(last) : "memory");
#else
	(void)observer;
	(void)first;
	(void)last;
#endif
}

} // namespace observer_transport_asm

extern "C" FAST_IO_OBSERVER_ASM_NOINLINE ::std::size_t
fast_io_large_stable_observer_probe(observer_transport_asm::stable_observer &observer,
									char const *first, ::std::size_t size)
{
	auto const before{observer.state->copies};
	::fast_io::print(observer, ::std::string_view{first, size});
	return observer.state->copies - before;
}
extern "C" FAST_IO_OBSERVER_ASM_NOINLINE ::std::size_t
fast_io_large_owned_observer_probe(observer_transport_asm::owned_observer &observer,
								   char const *first, ::std::size_t size)
{
	auto const before{observer.state->copies};
	::fast_io::print(observer, ::std::string_view{first, size});
	return observer.state->copies - before;
}
