#include <cstddef>
#include <type_traits>

#include <fast_io_freestanding.h>

namespace
{

inline constexpr auto flags = []() consteval {
	auto value{::fast_io::manipulators::floating_point_default_scalar_flags};
	value.floating = ::fast_io::manipulators::floating_format::fixed;
	value.precision = ::fast_io::manipulators::floating_precision::
		fractional_preserve_trailing_zero;
	value.rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_odd;
	value.showpos = true;
	value.comma = true;
	value.json_float = true;
	return value;
}();

template <::std::integral char_type>
[[nodiscard]] bool check() noexcept
{
	using source_type =
		::fast_io::manipulators::scalar_manip_precision_t<flags, double>;
	using source_tag = ::fast_io::io_reserve_type_t<char_type, source_type>;
	constexpr source_type source{1.53125, 3u};
	static_assert(print_compiler_constant_materialization_eligible(
		source_tag{}, source));
	constexpr auto proxy{
		print_compiler_constant_materialize(source_tag{}, source)};
	using proxy_type = ::std::remove_cv_t<decltype(proxy)>;
	using proxy_tag = ::fast_io::io_reserve_type_t<char_type, proxy_type>;
	char_type ordinary[32u]{};
	char_type replacement[32u]{};
	char_type fragmented[32u]{};
	auto const ordinary_size{print_reserve_precise_size(source_tag{}, source)};
	auto const proxy_size{print_reserve_precise_size(proxy_tag{}, proxy)};
	auto const ordinary_end{print_reserve_precise_define(
		source_tag{}, ordinary, ordinary_size, source)};
	auto const proxy_end{print_reserve_precise_define(
		proxy_tag{}, replacement, proxy_size, proxy)};
	if (ordinary_size != proxy_size || ordinary_end != ordinary + ordinary_size ||
		proxy_end != replacement + proxy_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != ordinary_size; ++index)
	{
		if (ordinary[index] != replacement[index])
		{
			return false;
		}
	}
	::fast_io::basic_io_scatter_t<char_type> scatters[32u]{};
	auto const scatter_end{print_compiler_constant_static_fragments_define(
		proxy_tag{}, scatters, proxy)};
	auto iter{fragmented};
	for (auto scatter{scatters}; scatter != scatter_end; ++scatter)
	{
		for (::std::size_t index{}; index != scatter->len; ++index)
		{
			*iter++ = scatter->base[index];
		}
	}
	if (iter != fragmented + ordinary_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != ordinary_size; ++index)
	{
		if (ordinary[index] != fragmented[index])
		{
			return false;
		}
	}
	// These assertions intentionally use char_literal_v.  Under IBM1047 the
	// values are EBCDIC code units, while the same source remains ASCII/Unicode
	// on ordinary builds.
	return ordinary[0u] == ::fast_io::char_literal_v<u8'+', char_type> &&
		ordinary[1u] == ::fast_io::char_literal_v<u8'1', char_type> &&
		ordinary[2u] == ::fast_io::char_literal_v<u8',', char_type>;
}

} // namespace

int main()
{
	return check<char>() && check<wchar_t>() ? 0 : 1;
}
