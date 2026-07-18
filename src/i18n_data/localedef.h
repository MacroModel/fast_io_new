#pragma once

#include "../../include/fast_io_i18n/locale/module.h"

namespace fast_io_i18n
{

// Locale source files retain their historical short type names, but their
// actual definitions now come from the installed module-ABI header.  Having a
// single definition on both sides of the DSO boundary prevents the previous
// accidental reinterpretation of direct-pointer data as `fast_io::lc_locale`.
using namespace ::fast_io::i18n_module_v1;
using lc_locale = ::fast_io::i18n_module_v1::locale_data;

template <typename value_type>
using basic_io_scatter_t = ::fast_io::i18n_module_v1::basic_scatter<value_type>;

template <typename char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr basic_scatter<char_type> tsc(char_type const (&array)[extent]) noexcept
{
	return {array, extent - 1u};
}

} // namespace fast_io_i18n
