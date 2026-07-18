#include <array>
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *, ::std::size_t);

#if !defined(FAST_IO_STANDALONE_FUZZ_ROUNDS)
#define FAST_IO_STANDALONE_FUZZ_ROUNDS 20000u
#endif

namespace
{

inline constexpr ::std::uint_least64_t next_random(::std::uint_least64_t &state) noexcept
{
	// A fixed xorshift stream makes sanitizer regressions exactly reproducible without depending on a fuzzing runtime.
	state ^= state << 13u;
	state ^= state >> 7u;
	state ^= state << 17u;
	return state;
}

} // namespace

int main()
{
	::std::array<::std::uint8_t, 256u> bytes{};
	::std::uint_least64_t state{0x9e3779b97f4a7c15ULL};
	for (::std::size_t round{}; round != FAST_IO_STANDALONE_FUZZ_ROUNDS; ++round)
	{
		::std::size_t const size{
			static_cast<::std::size_t>(next_random(state) % (bytes.size() + 1u))};
		for (::std::size_t index{}; index != size; ++index)
		{
			bytes[index] = static_cast<::std::uint8_t>(next_random(state));
		}
		if (LLVMFuzzerTestOneInput(bytes.data(), size) != 0)
		{
			return 1;
		}
	}
	return 0;
}
