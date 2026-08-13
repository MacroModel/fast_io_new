#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace to_terminal_stack_harness
{

struct hybrid_target
{
	char value[32]{};
	::std::size_t size{};
	::std::size_t contiguous_calls{};
	::std::size_t context_calls{};
	::std::size_t eof_calls{};
};

struct hybrid_context
{
};

struct hybrid_proxy
{
	hybrid_target *target{};
};

inline constexpr hybrid_proxy scan_alias_define(::fast_io::io_alias_t, hybrid_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::io_type_t<hybrid_context>
	scan_context_type(::fast_io::io_reserve_type_t<char, hybrid_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, hybrid_proxy>, char const *first, char const *last,
	hybrid_proxy proxy) noexcept
{
	auto &target{*proxy.target};
	++target.contiguous_calls;
	target.size = 0u;
	auto current{first};
	for (; current != last && *current != '|'; ++current)
	{
		if (target.size == sizeof(target.value))
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		target.value[target.size++] = *current;
	}
	return {current == last ? current : current + 1u, ::fast_io::parse_code::ok};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, hybrid_proxy>, hybrid_context &, char const *first,
	char const *last, hybrid_proxy proxy) noexcept
{
	auto &target{*proxy.target};
	++target.context_calls;
	auto current{first};
	for (; current != last; ++current)
	{
		if (*current == '|')
		{
			return {current + 1u, ::fast_io::parse_code::ok};
		}
		if (target.size == sizeof(target.value))
		{
			return {current, ::fast_io::parse_code::invalid};
		}
		target.value[target.size++] = *current;
	}
	return {current, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, hybrid_proxy>, hybrid_context &,
	hybrid_proxy proxy) noexcept
{
	auto &target{*proxy.target};
	++target.eof_calls;
	return target.size == 0u ? ::fast_io::parse_code::end_of_file : ::fast_io::parse_code::ok;
}

inline constexpr ::std::true_type scan_context_terminal_contiguous_equivalent(
	::fast_io::io_reserve_type_t<char, hybrid_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type to_terminal_contiguous_staging_preferred(
	::fast_io::io_reserve_type_t<char, hybrid_proxy>) noexcept
{
	return {};
}

template <::std::size_t capacity, bool eager>
struct fixed_fragment
{
	char const *data{};
	::std::size_t size{};
	::std::size_t *define_calls{};
};

template <::std::size_t capacity, bool eager>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, fixed_fragment<capacity, eager>>) noexcept
{
	return capacity;
}

template <::std::size_t capacity, bool eager>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, fixed_fragment<capacity, eager>>, char *out,
	fixed_fragment<capacity, eager> fragment) noexcept
{
	if (fragment.define_calls != nullptr)
	{
		++*fragment.define_calls;
	}
	for (::std::size_t i{}; i != fragment.size; ++i)
	{
		out[i] = fragment.data[i];
	}
	return out + fragment.size;
}

template <::std::size_t capacity>
inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, fixed_fragment<capacity, true>>) noexcept
{
	return {};
}

struct dynamic_fragment
{
	char const *data{};
	::std::size_t size{};
	::std::size_t *size_calls{};
	::std::size_t *define_calls{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_fragment>, dynamic_fragment fragment) noexcept
{
	++*fragment.size_calls;
	return fragment.size;
}

inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, dynamic_fragment>) noexcept
{
	return 32u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_fragment>, char *out,
	dynamic_fragment fragment) noexcept
{
	++*fragment.define_calls;
	for (::std::size_t i{}; i != fragment.size; ++i)
	{
		out[i] = fragment.data[i];
	}
	return out + fragment.size;
}

template <bool eager>
struct bounded_dynamic_fragment
{
	char const *data{};
	::std::size_t size{};
	::std::size_t *bounded_size_calls{};
	::std::size_t *ordinary_size_calls{};
	::std::size_t *define_calls{};
	::std::size_t *maximum_size_seen{};
};

template <bool eager>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<eager>>) noexcept
{
	return {};
}

template <bool eager>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<eager>>,
	bounded_dynamic_fragment<eager> const &fragment, ::std::size_t maximum_size) noexcept
{
	++*fragment.bounded_size_calls;
	if (fragment.maximum_size_seen != nullptr)
	{
		*fragment.maximum_size_seen = maximum_size;
	}
	return fragment.size <= maximum_size ? fragment.size : SIZE_MAX;
}

template <bool eager>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<eager>>,
	bounded_dynamic_fragment<eager> fragment) noexcept
{
	++*fragment.ordinary_size_calls;
	return fragment.size;
}

template <bool eager>
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<eager>>) noexcept
{
	return 32u;
}

template <bool eager>
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<eager>>, char *out,
	bounded_dynamic_fragment<eager> fragment) noexcept
{
	++*fragment.define_calls;
	for (::std::size_t i{}; i != fragment.size; ++i)
	{
		out[i] = fragment.data[i];
	}
	return out + fragment.size;
}

inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, bounded_dynamic_fragment<true>>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr bool equals(hybrid_target const &target, ::std::string_view expected) noexcept
{
	if (target.size != expected.size())
	{
		return false;
	}
	for (::std::size_t i{}; i != expected.size(); ++i)
	{
		if (target.value[i] != expected[i])
		{
			return false;
		}
	}
	return true;
}

} // namespace to_terminal_stack_harness

int main()
{
	using namespace ::to_terminal_stack_harness;

	static_assert(::fast_io::eager_materialization_safe_printable<char, fixed_fragment<8u, true>>);
	static_assert(!::fast_io::eager_materialization_safe_printable<char, fixed_fragment<8u, false>>);
	static_assert(::fast_io::eager_materialization_safe_printable<char, ::std::string>);
	static_assert(::fast_io::eager_materialization_safe_printable<char, ::std::string_view>);
	static_assert(::fast_io::details::to_terminal_stack_component_v<char, bounded_dynamic_fragment<true>>);
	static_assert(::fast_io::to_terminal_contiguous_staging_preferred_target<char, hybrid_proxy>);
	static_assert(::fast_io::details::to_terminal_stack_candidate<
				  char, hybrid_proxy, fixed_fragment<8u, true>, fixed_fragment<8u, true>>);

	// The first production target opt-in is the built-in integer scanner. Library-owned literal and scalar leaves are
	// eager-safe, so this representative mixed pack must select the new relation plan and retain integer parse semantics.
	int integer{};
	::fast_io::inplace_to(integer, "32", 3, "he", "b");
	if (integer != 323)
	{
		::fast_io::fast_terminate();
	}
	static_assert(::fast_io::to<int>("3", "2") == 32);
	static_assert(::fast_io::to<int>("-", "42") == -42);

	::std::size_t first_calls{};
	::std::size_t second_calls{};
	hybrid_target coalesced;
	::fast_io::inplace_to(coalesced,
						  fixed_fragment<8u, true>{"32", 2u, __builtin_addressof(first_calls)},
						  fixed_fragment<8u, true>{"3|tail", 6u, __builtin_addressof(second_calls)});
	if (!equals(coalesced, "323") || coalesced.contiguous_calls != 1u ||
		coalesced.context_calls != 0u || coalesced.eof_calls != 0u ||
		first_calls != 1u || second_calls != 1u)
	{
		::fast_io::fast_terminate();
	}

	::std::size_t coalesced_dynamic_first_size_calls{};
	::std::size_t coalesced_dynamic_first_ordinary_size_calls{};
	::std::size_t coalesced_dynamic_first_define_calls{};
	::std::size_t coalesced_dynamic_second_size_calls{};
	::std::size_t coalesced_dynamic_second_ordinary_size_calls{};
	::std::size_t coalesced_dynamic_second_define_calls{};
	::std::size_t coalesced_dynamic_first_maximum_size{(::std::numeric_limits<::std::size_t>::max)()};
	::std::size_t coalesced_dynamic_second_maximum_size{(::std::numeric_limits<::std::size_t>::max)()};
	hybrid_target coalesced_dynamic;
	::fast_io::inplace_to(
		coalesced_dynamic,
		bounded_dynamic_fragment<true>{"4", 1u, __builtin_addressof(coalesced_dynamic_first_size_calls),
								   __builtin_addressof(coalesced_dynamic_first_ordinary_size_calls),
								   __builtin_addressof(coalesced_dynamic_first_define_calls),
								   __builtin_addressof(coalesced_dynamic_first_maximum_size)},
		bounded_dynamic_fragment<true>{"2|tail", 6u, __builtin_addressof(coalesced_dynamic_second_size_calls),
								   __builtin_addressof(coalesced_dynamic_second_ordinary_size_calls),
								   __builtin_addressof(coalesced_dynamic_second_define_calls),
								   __builtin_addressof(coalesced_dynamic_second_maximum_size)});
	if (!equals(coalesced_dynamic, "42") || coalesced_dynamic.contiguous_calls != 1u ||
		coalesced_dynamic.context_calls != 0u || coalesced_dynamic.eof_calls != 0u ||
		coalesced_dynamic_first_size_calls != 1u || coalesced_dynamic_first_ordinary_size_calls != 0u ||
		coalesced_dynamic_first_define_calls != 1u || coalesced_dynamic_second_size_calls != 1u ||
		coalesced_dynamic_second_ordinary_size_calls != 0u || coalesced_dynamic_second_define_calls != 1u ||
		coalesced_dynamic_first_maximum_size != 256u || coalesced_dynamic_second_maximum_size != 255u)
	{
		::fast_io::fast_terminate();
	}

	::std::string owned{"ab"};
	::std::string_view viewed{"c|tail"};
	hybrid_target standard_text;
	::fast_io::inplace_to(standard_text, owned, viewed);
	if (!equals(standard_text, "abc") || standard_text.contiguous_calls != 1u ||
		standard_text.context_calls != 0u || standard_text.eof_calls != 0u)
	{
		::fast_io::fast_terminate();
	}

	// An unmarked suffix keeps the historical lazy fragment state machine. The delimiter in the first fragment finishes
	// the target, so neither sizing nor formatting of the observable suffix is permitted.
	::std::size_t prefix_calls{};
	::std::size_t suffix_calls{};
	hybrid_target lazy_suffix;
	::fast_io::inplace_to(lazy_suffix,
						  fixed_fragment<8u, true>{"7|", 2u, __builtin_addressof(prefix_calls)},
						  fixed_fragment<8u, false>{"never", 5u, __builtin_addressof(suffix_calls)});
	if (!equals(lazy_suffix, "7") || lazy_suffix.contiguous_calls != 0u ||
		lazy_suffix.context_calls == 0u || prefix_calls != 1u || suffix_calls != 0u)
	{
		::fast_io::fast_terminate();
	}

	// A type-level bound beyond the 256-byte operation cap rejects the optional plan before formatting. Context fallback
	// then formats only the first fragment, proving that a failed stack attempt does not replay a formatter or touch the
	// pure suffix after the scanner has completed.
	::std::size_t large_calls{};
	::std::size_t large_suffix_calls{};
	hybrid_target oversized;
	::fast_io::inplace_to(oversized,
						  fixed_fragment<300u, true>{"12|", 3u, __builtin_addressof(large_calls)},
						  fixed_fragment<8u, true>{"unused", 6u, __builtin_addressof(large_suffix_calls)});
	if (!equals(oversized, "12") || oversized.contiguous_calls != 0u ||
		oversized.context_calls == 0u || large_calls != 1u || large_suffix_calls != 0u)
	{
		::fast_io::fast_terminate();
	}

	// Dynamic reserve sizing is now fragment-local. Completion in the first small hinted fragment prevents the old
	// pack-wide maximum query from observing the suffix at all.
	::std::size_t dynamic_first_size_calls{};
	::std::size_t dynamic_first_define_calls{};
	::std::size_t dynamic_suffix_size_calls{};
	::std::size_t dynamic_suffix_define_calls{};
	hybrid_target dynamic;
	::fast_io::inplace_to(dynamic,
						  dynamic_fragment{"ok|", 3u, __builtin_addressof(dynamic_first_size_calls),
										   __builtin_addressof(dynamic_first_define_calls)},
						  dynamic_fragment{"unused", 6u, __builtin_addressof(dynamic_suffix_size_calls),
										   __builtin_addressof(dynamic_suffix_define_calls)});
	if (!equals(dynamic, "ok") || dynamic.contiguous_calls != 0u || dynamic.context_calls == 0u ||
		dynamic_first_size_calls != 1u || dynamic_first_define_calls != 1u ||
		dynamic_suffix_size_calls != 0u || dynamic_suffix_define_calls != 0u)
	{
		::fast_io::fast_terminate();
	}

	// The destination-neutral bounded query is sufficient for a small current fragment and avoids the ordinary reserve
	// sizing CPO. A completed scan must still leave every query and formatter of the suffix untouched.
	::std::size_t bounded_first_size_calls{};
	::std::size_t bounded_first_ordinary_size_calls{};
	::std::size_t bounded_first_define_calls{};
	::std::size_t bounded_suffix_size_calls{};
	::std::size_t bounded_suffix_ordinary_size_calls{};
	::std::size_t bounded_suffix_define_calls{};
	hybrid_target bounded_dynamic;
	::fast_io::inplace_to(
		bounded_dynamic,
		bounded_dynamic_fragment<false>{"go|", 3u, __builtin_addressof(bounded_first_size_calls),
									__builtin_addressof(bounded_first_ordinary_size_calls),
									__builtin_addressof(bounded_first_define_calls), nullptr},
		bounded_dynamic_fragment<false>{"unused", 6u, __builtin_addressof(bounded_suffix_size_calls),
									__builtin_addressof(bounded_suffix_ordinary_size_calls),
									__builtin_addressof(bounded_suffix_define_calls), nullptr});
	if (!equals(bounded_dynamic, "go") || bounded_dynamic.contiguous_calls != 0u ||
		bounded_dynamic.context_calls == 0u || bounded_first_size_calls != 1u ||
		bounded_first_ordinary_size_calls != 0u || bounded_first_define_calls != 1u ||
		bounded_suffix_size_calls != 0u || bounded_suffix_ordinary_size_calls != 0u ||
		bounded_suffix_define_calls != 0u)
	{
		::fast_io::fast_terminate();
	}
}
