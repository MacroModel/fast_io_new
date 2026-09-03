#include <array>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <fast_io_dsal/string.h>
#include <fast_io_unit/string.h>

namespace runtime_exact_direct_test
{

struct exact_source
{
	::std::string_view view;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, exact_source>, exact_source source) noexcept
{
	return source.view.size() + 7u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, exact_source>, char *destination,
	exact_source source) noexcept
{
	for (char value : source.view)
	{
		*destination++ = value;
	}
	return destination;
}

inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char, exact_source>, exact_source source) noexcept
{
	return source.view.size();
}

inline constexpr char *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char, exact_source>, char *destination,
	::std::size_t precise_size, exact_source source) noexcept
{
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		destination[index] = source.view[index];
	}
	return destination + precise_size;
}

[[nodiscard]] inline constexpr ::std::true_type print_precise_reserve_size_cached(
	::fast_io::io_reserve_type_t<char, exact_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type print_precise_reserve_output_growth_independent(
	::fast_io::io_reserve_type_t<char, exact_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type print_concat_fresh_precise_resize_preferred(
	::fast_io::io_reserve_type_t<char, exact_source>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, exact_source>) noexcept
{
	return {};
}

struct unsafe_static_source
{
	char value;
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, unsafe_static_source>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, unsafe_static_source>, char *destination,
	unsafe_static_source source) noexcept
{
	*destination = source.value;
	return destination + 1;
}

struct safe_static_source : unsafe_static_source
{};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, safe_static_source>) noexcept
{
	return 1u;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, safe_static_source>, char *destination,
	safe_static_source source) noexcept
{
	*destination = source.value;
	return destination + 1;
}

[[nodiscard]] inline constexpr ::std::true_type print_eager_materialization_safe(
	::fast_io::io_reserve_type_t<char, safe_static_source>) noexcept
{
	return {};
}

struct audited_runtime_result
{
	::std::array<char, 2048u> storage{};
	::std::size_t size{};
	unsigned reserve_calls{};
	unsigned commit_calls{};
};

inline constexpr audited_runtime_result strlike_construct_define(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	char const *first, char const *last)
{
	audited_runtime_result result;
	for (; first != last; ++first)
	{
		result.storage[result.size++] = *first;
	}
	return result;
}

inline constexpr char *strlike_runtime_begin(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result) noexcept
{
	return result.storage.data();
}

inline constexpr char *strlike_runtime_curr(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result) noexcept
{
	return result.storage.data() + result.size;
}

inline constexpr char *strlike_runtime_end(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result) noexcept
{
	return result.storage.data() + result.storage.size();
}

inline constexpr void strlike_runtime_set_curr(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result, char *current) noexcept
{
	result.size = static_cast<::std::size_t>(current - result.storage.data());
	++result.commit_calls;
}

inline constexpr void strlike_runtime_reserve(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result, ::std::size_t requested)
{
	++result.reserve_calls;
	if (result.storage.size() < requested)
	{
		throw ::std::length_error{"audited runtime result capacity"};
	}
}

inline constexpr char *strlike_precise_resize_and_get_begin(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>,
	audited_runtime_result &result, ::std::size_t requested)
{
	if (result.storage.size() < requested)
	{
		throw ::std::length_error{"audited runtime result capacity"};
	}
	result.size = requested;
	return result.storage.data();
}

[[nodiscard]] inline constexpr ::std::true_type strlike_runtime_deferred_obuffer_commit_safe(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>) noexcept
{
	return {};
}

[[nodiscard]] inline constexpr ::std::true_type strlike_concat_fresh_runtime_exact_direct_safe(
	::fast_io::io_strlike_type_t<char, audited_runtime_result>) noexcept
{
	return {};
}

static_assert(::fast_io::output_growth_independent_precise_reserve_printable<char, exact_source>);
static_assert(::fast_io::details::decay::basic_general_concat_single_pass_exact_component_v<char, exact_source>);
static_assert(!::fast_io::eager_materialization_safe_printable<char, unsafe_static_source>);
static_assert(::fast_io::eager_materialization_safe_printable<char, safe_static_source>);
static_assert(::fast_io::concat_fresh_runtime_exact_direct_strlike<char, audited_runtime_result>);

// An exact leaf cannot grant pre-allocation formatting safety to an unrelated reserve companion.
static_assert(!::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
	char, ::std::string, exact_source, unsafe_static_source>);

inline bool equal(::std::string const &actual, ::std::string_view expected) noexcept
{
	return actual.size() == expected.size() &&
		::std::char_traits<char>::compare(actual.data(), expected.data(), expected.size()) == 0;
}

template <bool line, typename result_type, ::std::size_t extent>
[[nodiscard]] inline result_type concat_exact_array(
	::std::array<exact_source, extent> &sources)
{
	return ::std::apply(
		[](auto &...source) {
			return ::fast_io::basic_general_concat<line, char, result_type>(source...);
		},
		sources);
}

} // namespace runtime_exact_direct_test

int main()
{
	using runtime_exact_direct_test::exact_source;

	::std::array<char, 700u> first_storage{};
	::std::array<char, 503u> second_storage{};
	for (::std::size_t index{}; index != first_storage.size(); ++index)
	{
		first_storage[index] = static_cast<char>('a' + index % 26u);
	}
	for (::std::size_t index{}; index != second_storage.size(); ++index)
	{
		second_storage[index] = static_cast<char>('0' + index % 10u);
	}

	exact_source first{{first_storage.data(), first_storage.size()}};
	exact_source empty{{}};
	exact_source second{{second_storage.data(), second_storage.size()}};
	auto result{::fast_io::concat_std(first, empty, second)};
	::std::string expected;
	expected.append(first_storage.data(), first_storage.size());
	expected.append(second_storage.data(), second_storage.size());
	if (!runtime_exact_direct_test::equal(result, expected))
	{
		return EXIT_FAILURE;
	}

	auto line_result{::fast_io::concatln_std(empty, first, second)};
	expected.push_back('\n');
	if (!runtime_exact_direct_test::equal(line_result, expected))
	{
		return EXIT_FAILURE;
	}

	// The synthetic destination remains available under sanitizers even when a standard-string ABI put area is disabled.
	// On a supported compiler this proves that the all-exact strategy reserves once, writes the full unpublished range,
	// and commits exactly once; older compiler families deliberately retain the range-construction fallback.
	auto audited{::fast_io::basic_general_concat<false, char,
		runtime_exact_direct_test::audited_runtime_result>(first, empty, second)};
	if (audited.size != expected.size() - 1u ||
		::std::char_traits<char>::compare(audited.storage.data(), expected.data(), audited.size) != 0)
	{
		return EXIT_FAILURE;
	}
	if constexpr (::fast_io::details::decay::
				  basic_general_concat_single_pass_bounded_extended_codegen_supported())
	{
		if (audited.reserve_calls != 1u || audited.commit_calls != 1u)
		{
			return EXIT_FAILURE;
		}
	}

	/*
	The 8/9 boundary is a semantic control for the long-pack outlining policy,
	while 32 exercises its measured cardinality. Every leaf names the same
	immutable two-byte range, so the independent expected string proves that
	outlining changes neither left-to-right multiplicity nor endpoint commits.
	*/
	static constexpr char repeated_token[]{'x', 'y'};
	::std::array<exact_source, 9u> nine_sources{};
	for (auto &source : nine_sources)
	{
		source.view = {repeated_token, sizeof(repeated_token)};
	}
	auto nine_line{runtime_exact_direct_test::concat_exact_array<
		true, ::std::string>(nine_sources)};
	::std::string nine_expected;
	for (::std::size_t index{}; index != nine_sources.size(); ++index)
	{
		nine_expected.append(repeated_token, sizeof(repeated_token));
	}
	nine_expected.push_back('\n');
	if (!runtime_exact_direct_test::equal(nine_line, nine_expected))
	{
		return EXIT_FAILURE;
	}

	::std::array<exact_source, 32u> thirty_two_sources{};
	for (auto &source : thirty_two_sources)
	{
		source.view = {repeated_token, sizeof(repeated_token)};
	}
	auto long_audited{runtime_exact_direct_test::concat_exact_array<
		false, runtime_exact_direct_test::audited_runtime_result>(
		thirty_two_sources)};
	if (long_audited.size != thirty_two_sources.size() * sizeof(repeated_token))
	{
		return EXIT_FAILURE;
	}
	for (::std::size_t index{}; index != long_audited.size; ++index)
	{
		if (long_audited.storage[index] != repeated_token[index % sizeof(repeated_token)])
		{
			return EXIT_FAILURE;
		}
	}
	if constexpr (::fast_io::details::decay::
				  basic_general_concat_single_pass_bounded_extended_codegen_supported())
	{
		if (long_audited.reserve_calls != 1u || long_audited.commit_calls != 1u)
		{
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
