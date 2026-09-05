#include <fast_io_dsal/vector.h>

#include <cstddef>
#include <limits>
#include <type_traits>

namespace
{

// A non-trivial element selects the element-counted vector growth path whose
// maximum capacity is SIZE_MAX / sizeof(T), rather than the byte-counted path.
struct non_trivial_element
{
	unsigned char payload[16]{};
	~non_trivial_element()
	{}
};

static_assert(!::std::is_trivially_copyable_v<non_trivial_element>);
inline constexpr ::std::size_t element_size{sizeof(non_trivial_element)};
inline constexpr ::std::size_t maximum_capacity{
	::std::numeric_limits<::std::size_t>::max() / element_size};
inline constexpr ::std::size_t half_capacity{maximum_capacity >> 1u};

template <::std::size_t capacity>
inline constexpr ::std::size_t grown_capacity{
	::fast_io::containers::details::cal_grow_twice_size<element_size, false>(
		capacity)};

// Boundary proof: the typed path measures capacity in elements and therefore
// allocates exactly one element from zero; the last exactly-doublable value
// doubles; every larger live value saturates at the representable element
// maximum instead of wrapping or shrinking.
static_assert(grown_capacity<0u> == 1u);
static_assert(grown_capacity<half_capacity> == (half_capacity << 1u));
static_assert(grown_capacity<half_capacity + 1u> == maximum_capacity);
static_assert(grown_capacity<maximum_capacity - 1u> == maximum_capacity);

// The type-erased trivial path measures the same initial request in bytes.
static_assert(
	::fast_io::containers::details::cal_grow_twice_size<element_size, true>(
		0u) == element_size);

} // namespace

int main()
{
	// Exercise the public typed-allocation route in addition to the constexpr
	// policy proof, so a future unit mismatch cannot hide behind direct helper
	// instantiation.
	::fast_io::vector<non_trivial_element> values;
	values.emplace_back();
	return values.capacity() == 1u ? 0 : 1;
}
