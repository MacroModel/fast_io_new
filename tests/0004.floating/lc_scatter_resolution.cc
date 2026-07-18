#include <fast_io_i18n.h>

#include <array>
#include <vector>

namespace
{

struct throwing_storage_value
{
	int value{};
	inline static int successful_copies_before_throw{};

	throwing_storage_value() = default;
	explicit throwing_storage_value(int input) noexcept : value(input) {}
	throwing_storage_value(throwing_storage_value const &other) : value(other.value)
	{
		if (successful_copies_before_throw == 0)
		{
			throw 1;
		}
		--successful_copies_before_throw;
	}
	throwing_storage_value(throwing_storage_value &&) noexcept = default;
	throwing_storage_value &operator=(throwing_storage_value const &) = default;
	throwing_storage_value &operator=(throwing_storage_value &&) noexcept = default;
};

bool test_append_rollback()
{
	::std::vector<throwing_storage_value> destination;
	destination.emplace_back(7);
	::std::array<throwing_storage_value, 3u> source{
		throwing_storage_value{1}, throwing_storage_value{2}, throwing_storage_value{3}};
	throwing_storage_value::successful_copies_before_throw = 1;
	try
	{
		(void)::fast_io::basic_scatter<throwing_storage_value>::append_range(
			destination, source);
	}
	catch (int)
	{
		// The first appended element was constructed before the second copy failed. The descriptor builder must erase
		// precisely that suffix so no partial locale field becomes observable after an allocation/element exception.
		return destination.size() == 1u && destination.front().value == 7;
	}
	return false;
}

template <::std::integral char_type, typename localized_type, typename ordinary_type>
bool equal_rendering(::fast_io::basic_lc_object<char_type> const &locale,
	localized_type localized, ordinary_type ordinary)
{
	char_type localized_buffer[1024u]{};
	char_type ordinary_buffer[1024u]{};
	auto const localized_end{
		::fast_io::print_reserve_define(&locale.all, localized_buffer, localized)};
	auto const ordinary_end{::fast_io::print_reserve_define(
		::fast_io::io_reserve_type<char_type, ordinary_type>, ordinary_buffer, ordinary)};
	auto const localized_size{static_cast<::std::size_t>(localized_end - localized_buffer)};
	auto const ordinary_size{static_cast<::std::size_t>(ordinary_end - ordinary_buffer)};
	if (localized_size != ordinary_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != localized_size; ++index)
	{
		if (localized_buffer[index] != ordinary_buffer[index])
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type>
bool test_character_type()
{
	::fast_io::basic_lc_object<char_type> locale;
	locale.data_storage.chars.push_back(
		::fast_io::char_literal_v<u8',', char_type>);
	locale.all.numeric.decimal_point = {0u, 1u};

	using namespace ::fast_io::mnp;
	if (!equal_rendering(locale, general(1234.5), comma_general(1234.5)) ||
		!equal_rendering(locale, fixed(0.125), comma_fixed(0.125)) ||
		!equal_rendering(locale, scientific(1234.5), comma_scientific(1234.5)) ||
		!equal_rendering(locale, hexfloat(1.5), comma_hexfloat(1.5)))
	{
		return false;
	}

	// Relative descriptors must follow their complete locale owner, not the object from which the facet aggregate was
	// copied. Exercise every complete-object transfer while the source remains live, then verify that the destination
	// resolves into its own vector. This is the portability regression for implementations where `std::vector` is not
	// standard-layout and container-of recovery from `&locale.all` is therefore unavailable.
	::fast_io::basic_lc_object<char_type> copied{locale};
	locale.data_storage.chars[0u] = ::fast_io::char_literal_v<u8'.', char_type>;
	auto copied_scatter{::fast_io::details::lc_resolve_scatter(
		&copied.all, copied.all.numeric.decimal_point)};
	if (copied_scatter.len != 1u || copied_scatter.base[0u] !=
			::fast_io::char_literal_v<u8',', char_type> ||
		copied_scatter.base == locale.data_storage.chars.data())
	{
		return false;
	}
	::fast_io::basic_lc_object<char_type> copy_assigned;
	copy_assigned = copied;
	auto copy_assigned_scatter{::fast_io::details::lc_resolve_scatter(
		&copy_assigned.all, copy_assigned.all.numeric.decimal_point)};
	if (copy_assigned_scatter.len != 1u || copy_assigned_scatter.base[0u] !=
			::fast_io::char_literal_v<u8',', char_type>)
	{
		return false;
	}
	::fast_io::basic_lc_object<char_type> moved{::std::move(copied)};
	auto moved_scatter{::fast_io::details::lc_resolve_scatter(
		&moved.all, moved.all.numeric.decimal_point)};
	if (moved_scatter.len != 1u || moved_scatter.base[0u] !=
			::fast_io::char_literal_v<u8',', char_type> ||
		::fast_io::details::lc_resolve_scatter(
			&copied.all, copied.all.numeric.decimal_point).len != 0u)
	{
		return false;
	}
	::fast_io::basic_lc_object<char_type> move_assigned;
	move_assigned = ::std::move(copy_assigned);
	auto move_assigned_scatter{::fast_io::details::lc_resolve_scatter(
		&move_assigned.all, move_assigned.all.numeric.decimal_point)};
	if (move_assigned_scatter.len != 1u || move_assigned_scatter.base[0u] !=
			::fast_io::char_literal_v<u8',', char_type> ||
		::fast_io::details::lc_resolve_scatter(
			&copy_assigned.all, copy_assigned.all.numeric.decimal_point).len != 0u)
	{
		return false;
	}

	// Empty locale fields are represented by a zero-length relative scatter.
	// Resolving one must not perform pointer arithmetic on an empty vector.
	locale.data_storage.chars.clear();
	locale.all.numeric.decimal_point = {};
	auto const direct_empty{locale.all.numeric.decimal_point.get_from(
		locale.data_storage.chars)};
	auto const empty{::fast_io::details::lc_resolve_scatter(
		&locale.all, locale.all.numeric.decimal_point)};
	return direct_empty.data() == nullptr && direct_empty.empty() &&
		empty.base == nullptr && empty.len == 0u;
}

} // namespace

int main()
{
	return !(test_append_rollback() && test_character_type<char>() && test_character_type<wchar_t>() &&
		test_character_type<char8_t>() && test_character_type<char16_t>() &&
		test_character_type<char32_t>());
}
