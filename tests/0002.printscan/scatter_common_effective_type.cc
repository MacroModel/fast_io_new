#include <cassert>
#include <cstddef>
#include <limits>

#include <fast_io_core.h>

namespace scatter_common_effective_type
{

template <typename T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
::fast_io::scatter_total_size_overflow_result runtime_total(
	::fast_io::basic_io_scatter_t<T> const *scatters, ::std::size_t count) noexcept
{
	return ::fast_io::find_scatter_total_size_overflow(scatters, count);
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
::fast_io::intfpos_t runtime_offset(
	::fast_io::intfpos_t origin, ::fast_io::basic_io_scatter_t<T> const *scatters,
	::fast_io::io_scatter_status_t status) noexcept
{
	return ::fast_io::fposoffadd_scatters(origin, scatters, status);
}

constexpr char16_t constant_payload[9]{};
constexpr ::fast_io::basic_io_scatter_t<char16_t> constant_scatters[]{
	{constant_payload, 2u}, {constant_payload + 2u, 3u}, {constant_payload + 5u, 4u}};

constexpr auto constant_total{
	::fast_io::find_scatter_total_size_overflow(constant_scatters, 3u)};
static_assert(constant_total.total_size == 9u);
static_assert(constant_total.position == 3u);
static_assert(::fast_io::fposoffadd_scatters(5, constant_scatters, {2u, 1u}) == 11);
static_assert(::fast_io::fposoffadd(10, -3) == 7);
static_assert(::fast_io::fposoffadd(-5, ::std::size_t{3u}) == -2);
static_assert(::fast_io::fposoffadd(
				  ::std::numeric_limits<::fast_io::intfpos_t>::min() + 2, -3) ==
			  ::std::numeric_limits<::fast_io::intfpos_t>::min());
static_assert(::fast_io::fposoffadd(
				  ::std::numeric_limits<::fast_io::intfpos_t>::max() - 2, 3) ==
			  ::std::numeric_limits<::fast_io::intfpos_t>::max());

constexpr auto minimum_position{::std::numeric_limits<::fast_io::intfpos_t>::min()};
constexpr auto maximum_position{::std::numeric_limits<::fast_io::intfpos_t>::max()};
static_assert(::fast_io::details::scatter_fpos_mul<char>(minimum_position) == minimum_position);
static_assert(::fast_io::details::scatter_fpos_mul<char>(maximum_position) == maximum_position);
static_assert(::fast_io::details::scatter_fpos_mul<char16_t>(-3) ==
			  -3 * static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));
static_assert(::fast_io::details::scatter_fpos_mul<char32_t>(-3) ==
			  -3 * static_cast<::fast_io::intfpos_t>(sizeof(char32_t)));
static_assert(::fast_io::details::scatter_fpos_mul<char16_t>(minimum_position) == minimum_position);
static_assert(::fast_io::details::scatter_fpos_mul<char16_t>(maximum_position) == maximum_position);
static_assert(::fast_io::details::scatter_fpos_mul<char32_t>(minimum_position) == minimum_position);
static_assert(::fast_io::details::scatter_fpos_mul<char32_t>(maximum_position) == maximum_position);

} // namespace scatter_common_effective_type

int main()
{
	using namespace scatter_common_effective_type;

	char32_t payload[9]{};
	::fast_io::basic_io_scatter_t<char32_t> scatters[]{
		{payload, 2u}, {payload + 2u, 3u}, {payload + 5u, 4u}};

	auto const total{runtime_total(scatters, 3u)};
	assert(total.total_size == 9u);
	assert(total.position == 3u);
	assert(runtime_offset(5, scatters, {2u, 1u}) == 11);
	assert(::fast_io::fposoffadd(10, -3) == 7);
	assert(::fast_io::fposoffadd(10, 3) == 13);
	assert(::fast_io::fposoffadd(-5, ::std::size_t{3u}) == -2);
	assert(::fast_io::fposoffadd(
			   ::std::numeric_limits<::fast_io::intfpos_t>::min() + 2, -3) ==
		   ::std::numeric_limits<::fast_io::intfpos_t>::min());
	assert(::fast_io::fposoffadd(
			   ::std::numeric_limits<::fast_io::intfpos_t>::max() - 2, 3) ==
		   ::std::numeric_limits<::fast_io::intfpos_t>::max());
	assert(::fast_io::details::scatter_fpos_mul<char>(minimum_position) == minimum_position);
	assert(::fast_io::details::scatter_fpos_mul<char16_t>(minimum_position) == minimum_position);
	assert(::fast_io::details::scatter_fpos_mul<char32_t>(minimum_position) == minimum_position);
	assert(::fast_io::details::scatter_fpos_mul<char>(maximum_position) == maximum_position);
	assert(::fast_io::details::scatter_fpos_mul<char16_t>(maximum_position) == maximum_position);
	assert(::fast_io::details::scatter_fpos_mul<char32_t>(maximum_position) == maximum_position);
	assert(::fast_io::details::scatter_fpos_mul<char16_t>(-3) ==
		   -3 * static_cast<::fast_io::intfpos_t>(sizeof(char16_t)));
	assert(::fast_io::details::scatter_fpos_mul<char32_t>(-3) ==
		   -3 * static_cast<::fast_io::intfpos_t>(sizeof(char32_t)));
}
