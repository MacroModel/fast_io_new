#include <fast_io_core.h>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

template <::std::unsigned_integral element_type, ::std::size_t lanes>
void test_mask_countr_lane(::std::size_t position)
{
	using vector_type = ::fast_io::intrinsics::simd_vector<element_type, lanes>;
	require(position < lanes);
	::std::array<element_type, lanes> zero_prefix{};
	zero_prefix[position] = ::std::numeric_limits<element_type>::max();
	vector_type zero_mask;
	zero_mask.load(zero_prefix.data());
	require(::fast_io::intrinsics::vector_mask_countr_zero(zero_mask) == position);

	::std::array<element_type, lanes> one_prefix{};
	one_prefix.fill(::std::numeric_limits<element_type>::max());
	one_prefix[position] = 0u;
	vector_type one_mask;
	one_mask.load(one_prefix.data());
	require(::fast_io::intrinsics::vector_mask_countr_one(one_mask) == position);
}

template <::std::unsigned_integral element_type>
void test_mask_countr_width()
{
	constexpr ::std::size_t lanes{16u / sizeof(element_type)};
	// Interior positions distinguish element indexing from the underlying byte-mask bit count.
	test_mask_countr_lane<element_type, lanes>(lanes / 2u);
	test_mask_countr_lane<element_type, lanes>(lanes - 1u);
}

} // namespace

int main()
{
	test_mask_countr_width<::std::uint16_t>();
	test_mask_countr_width<::std::uint32_t>();
	test_mask_countr_width<::std::uint64_t>();
}
