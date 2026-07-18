#pragma once

namespace fast_io
{

// Chrono facet descriptors are compact RVAs owned by `basic_lc_object`, not process pointers. Size-only paths read the
// descriptor count directly because they observe no storage. Every materialization path resolves the selected RVA
// through `lc_resolve_scatter`, which both re-establishes the pointer and validates the closed interval before copying.
// Keeping that translation at the locale boundary leaves the ordinary chrono fallback and character kernels unchanged.

/// @feature concept:runtime_precise_size
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  ::std::chrono::weekday wkd) noexcept
{
	unsigned value(wkd.c_encoding());
	if (7 < value)
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
	else
	{
		if (value == 7)
		{
			value = 0;
		}
		return all->time.day[value].length;
	}
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(basic_lc_all<char_type> const *__restrict all, char_type *it,
												 ::std::chrono::weekday wkd) noexcept
{
	unsigned value{wkd.c_encoding()};
	if (7u < value)
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, it, value);
	}
	else
	{
		if (value == 7u)
		{
			value = {};
		}
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.day[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, it);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  ::std::chrono::month m) noexcept
{
	unsigned value(m);
	--value;
	if (value < 12u)
	{
		return all->time.mon[value].length;
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   ::std::chrono::month m) noexcept
{
	unsigned value(m);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.mon[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

/*
Referenced from IBM
LC_TIME Category for the Locale Definition Source File Format
https://www.ibm.com/support/knowledgecenter/ssw_aix_71/filesreference/LC_TIME.html
*/

namespace manipulators
{

template <typename T>
struct abbr_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

template <typename T>
struct alt_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

template <typename T>
struct alt_num_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

template <typename T>
struct am_pm_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

inline constexpr am_pm_t<bool> am_pm(bool is_pm) noexcept
{
	return {is_pm};
}

inline constexpr abbr_t<::std::chrono::weekday> abbr(::std::chrono::weekday wkd) noexcept
{
	return {wkd};
}

inline constexpr abbr_t<::std::chrono::month> abbr(::std::chrono::month wkd) noexcept
{
	return {wkd};
}

inline constexpr abbr_t<alt_t<::std::chrono::month>> abbr_alt(::std::chrono::month wkd) noexcept
{
	return {{wkd}};
}

template <typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr alt_num_t<T> alt_num(T m) noexcept
{
	return {m};
}

template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(basic_lc_all<char_type> const *__restrict all,
																	am_pm_t<bool> ampm) noexcept
{
	return ::fast_io::details::lc_resolve_scatter(all, all->time.am_pm[ampm.reference]);
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	io_reserve_type_t<char_type, am_pm_t<bool>>) noexcept
{
	// AM/PM names are stable descriptors in the enclosing locale object, and an unchanged boolean index is repeatable.
	// The marker is intentionally limited to this exact manipulator rather than inferred for every locale scatter.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<::std::chrono::weekday> wkd) noexcept
{
	unsigned value(wkd.reference.c_encoding());
	if (7 < value)
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
	else
	{
		if (value == 7)
		{
			value = 0;
		}
		return all->time.abday[value].length;
	}
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(basic_lc_all<char_type> const *__restrict all, char_type *it,
												 abbr_t<::std::chrono::weekday> wkd) noexcept
{
	unsigned value(wkd.reference.c_encoding());
	if (7u < value)
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, it, value);
	}
	else
	{
		if (value == 7u)
		{
			value = {};
		}
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.abday[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, it);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<::std::chrono::month> m) noexcept
{
	unsigned value(m.reference);
	--value;
	if (value < 12u)
	{
		return all->time.abmon[value].length;
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   abbr_t<::std::chrono::month> m) noexcept
{
	unsigned value(m.reference);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.abmon[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<alt_t<::std::chrono::month>> m) noexcept
{
	unsigned value(m.reference.reference);
	--value;
	if (value < 12u)
	{
		if (all->time.ab_alt_mon[value].length == 0)
		{
			return all->time.abmon[value].length;
		}
		else
		{
			return all->time.ab_alt_mon[value].length;
		}
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   abbr_t<alt_t<::std::chrono::month>> m) noexcept
{
	unsigned value(m.reference.reference);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		if (all->time.ab_alt_mon[value].length == 0)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, all->time.abmon[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, all->time.ab_alt_mon[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

template <::std::integral char_type, typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
											  alt_num_t<T> m) noexcept
{
	using namespace ::std::chrono;
	// `alt_digits` is a relative descriptor into the locale's descriptor table. Resolve that outer table once; the
	// selected entry remains a compact character RVA, so a size query can read its count without touching character
	// storage. Materialization below performs the second, character-table resolution only for the selected entry.
	auto const alt_digits{
		::fast_io::details::lc_resolve_scatter(all, all->time.alt_digits)};
	if constexpr (::std::same_as<month, T> || ::std::same_as<day, T>)
	{
		unsigned value(m.reference);
		if (value < alt_digits.len)
		{
			return alt_digits.base[value].length;
		}
		else
		{
			constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, T>)};
			return unsigned_size;
		}
	}
	else
	{
		unsigned value(m.reference.iso_encoding());
		if (value < alt_digits.len)
		{
			return alt_digits.base[value].length;
		}
		else
		{
			constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, T>)};
			return unsigned_size;
		}
	}
}

template <::std::integral char_type, ::std::random_access_iterator Iter, typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   alt_num_t<T> m) noexcept
{
	using namespace ::std::chrono;
	auto const alt_digits{
		::fast_io::details::lc_resolve_scatter(all, all->time.alt_digits)};
	if constexpr (::std::same_as<month, T> || ::std::same_as<day, T>)
	{
		unsigned value(m.reference);
		if (value < alt_digits.len)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, alt_digits.base[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			return print_reserve_define(io_reserve_type<char_type, T>, iter, m.reference);
		}
	}
	else
	{
		unsigned value(m.reference.iso_encoding());
		if (value < alt_digits.len)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, alt_digits.base[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			return print_reserve_define(io_reserve_type<char_type, T>, iter, m.reference);
		}
	}
}

#if 0
template<::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const* __restrict all,::std::chrono::year_month_day m) noexcept
{
	constexpr ::std::size_t unitsize{::fast_io::freestanding::max(print_reserve_size(io_reserve_type<char_type,int>),print_reserve_size(io_reserve_type<char_type,unsigned>))};
	return unitsize*all->time.d_fmt.len;
}

template<::std::integral char_type>
inline constexpr char_type* print_reserve_define(basic_lc_all<char_type> const* __restrict all,char_type* iter,::std::chrono::year_month_day m) noexcept
{

}
#endif
} // namespace manipulators
} // namespace fast_io
