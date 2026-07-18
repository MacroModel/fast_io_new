#include <cstdlib>
#include <string>
#include <string_view>

#include <fast_io.h>
#include <fast_io_i18n.h>

#if ((!defined(_WIN32) || defined(__WINE__)) && (!defined(__wasi__) || !defined(__NEWLIB__))) || defined(__CYGWIN__)

namespace
{

struct throwing_output_reference
{
	using output_char_type = char;
};

struct throwing_output_stream
{};

inline throwing_output_reference output_stream_ref_define(throwing_output_stream &) noexcept(false)
{
	return {};
}

template <typename char_type>
concept native_locale_status_forwardable = requires(::fast_io::posix_l10n const &locale) {
	::fast_io::status_io_print_forward(::fast_io::io_alias_type<char_type>, locale);
};

static_assert(native_locale_status_forwardable<char>);
static_assert(!native_locale_status_forwardable<signed char>);
static_assert(!noexcept(::fast_io::imbue(
	::std::declval<::fast_io::posix_l10n &>(), ::std::declval<throwing_output_stream &>())));

template <typename char_type>
[[nodiscard]] bool equals_ascii(
	::fast_io::basic_io_scatter_t<char_type> scatter, ::std::string_view expected) noexcept
{
	if (scatter.len != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != scatter.len; ++index)
	{
		if (scatter.base[index] != static_cast<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool validate(
	::fast_io::posix_l10n &locale, ::std::string_view expected_name,
	::std::uint_least64_t expected_country, bool expect_nested_time)
{
	if (locale.loc.lc != __builtin_addressof(locale.owner.lc) ||
		locale.loc.wlc != __builtin_addressof(locale.owner.wlc) ||
		locale.loc.u8lc != __builtin_addressof(locale.owner.u8lc) ||
		locale.loc.u16lc != __builtin_addressof(locale.owner.u16lc) ||
		locale.loc.u32lc != __builtin_addressof(locale.owner.u32lc))
	{
		return false;
	}
	auto const *all{__builtin_addressof(locale.loc.lc->all)};
	if (all->data_storage != __builtin_addressof(locale.loc.lc->data_storage) ||
		all->address.country_num != expected_country ||
		!equals_ascii(::fast_io::details::lc_resolve_scatter(all, all->identification.name), expected_name) ||
		!equals_ascii(::fast_io::details::lc_resolve_scatter(all, all->identification.encoding), "UTF-8") ||
		!equals_ascii(::fast_io::details::lc_resolve_scatter(all, all->numeric.decimal_point), "."))
	{
		return false;
	}
	auto const *u8all{__builtin_addressof(locale.loc.u8lc->all)};
	auto const yes{::fast_io::print_scatter_define(u8all, ::fast_io::mnp::boolalpha(true))};
	auto const no{::fast_io::print_scatter_define(u8all, ::fast_io::mnp::boolalpha(false))};
	if (yes.len == 0u || no.len == 0u)
	{
		return false;
	}
	auto const eras{::fast_io::details::lc_resolve_scatter(all, all->time.era)};
	auto const alternate_digits{
		::fast_io::details::lc_resolve_scatter(all, all->time.alt_digits)};
	if (expect_nested_time)
	{
		if (eras.len == 0u || alternate_digits.len == 0u ||
			::fast_io::details::lc_resolve_scatter(all, eras.base[0u].era_name).len == 0u ||
			::fast_io::details::lc_resolve_scatter(all, eras.base[0u].era_format).len == 0u ||
			::fast_io::details::lc_resolve_scatter(all, alternate_digits.base[0u]).len == 0u)
		{
			return false;
		}
	}
	else if (eras.len != 0u || alternate_digits.len != 0u ||
		!equals_ascii(yes, "yes") || !equals_ascii(no, "no"))
	{
		return false;
	}

	// Exercise the complete status-forward/imbuer/print path, not merely the
	// converter's descriptor resolver.  The expected bytes are independently
	// obtained from the imported character-domain aggregate.
	::std::string rendered;
	::fast_io::ostring_ref_std sink{__builtin_addressof(rendered)};
	::fast_io::print(
		::fast_io::imbue(locale, sink), ::fast_io::mnp::boolalpha(true),
		::fast_io::mnp::boolalpha(false));
	auto const char_yes{::fast_io::print_scatter_define(all, ::fast_io::mnp::boolalpha(true))};
	auto const char_no{::fast_io::print_scatter_define(all, ::fast_io::mnp::boolalpha(false))};
	::std::string expected_render;
	expected_render.append(char_yes.base, char_yes.len);
	expected_render.append(char_no.base, char_no.len);
	return rendered == expected_render;
}

} // namespace

int main(int argc, char **argv)
{
	// Normal test discovery does not build hundreds of locale DSOs.  Passing a
	// locale name enables the integration half of this test; the focused
	// regression invocation builds one module and invokes this binary with
	// `en_US.UTF-8` while its directory is on the dynamic-loader search path.
	if (argc != 2)
	{
		return 0;
	}
	auto const locale_name{::std::string_view(argv[1])};
	bool const japanese{locale_name.starts_with("ja_JP")};
	auto const expected_name{japanese ? ::std::string_view("ja_JP") : ::std::string_view("en_US")};
	auto const expected_country{japanese ? ::std::uint_least64_t{392u} : ::std::uint_least64_t{840u}};
	::fast_io::posix_l10n loaded{::std::string_view(argv[1])};
	if (!validate(loaded, expected_name, expected_country, japanese))
	{
		return 1;
	}
	::fast_io::posix_l10n moved(::std::move(loaded));
	if (loaded.loc.lc != nullptr || !validate(moved, expected_name, expected_country, japanese))
	{
		return 2;
	}
	::fast_io::posix_l10n assigned;
	assigned = ::std::move(moved);
	if (moved.loc.lc != nullptr || !validate(assigned, expected_name, expected_country, japanese))
	{
		return 3;
	}
	assigned.close();
	// The public locale owns the converted storage.  Rendering after dlclose is
	// the decisive regression against retaining any pointer into module data.
	return validate(assigned, expected_name, expected_country, japanese) ? 0 : 4;
}

#else

int main() {}

#endif
