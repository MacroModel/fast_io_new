#include <fast_io.h>

#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace
{

struct source_a
{
	int value{};
};

struct source_b
{
	int value{};
};

struct digit_proxy
{
	char value{};
};

/// @brief Declares the one-byte reserve extent of a compiler-constant digit proxy.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, digit_proxy>) noexcept
{
	return 1u;
}

/// @brief Emits one materialized digit proxy into the caller's exact output range.
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, digit_proxy>, char *iter,
	digit_proxy value) noexcept
{
	*iter++ = value.value;
	return iter;
}

/// @brief Makes both synthetic sources deterministically eligible for compiler-constant replacement.
template <typename source>
inline constexpr bool print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char, source>, source const &) noexcept
	requires(::std::same_as<source, source_a> ||
			 ::std::same_as<source, source_b>)
{
	// An always-eligible test protocol makes the semantic regression
	// deterministic at every optimization level; the production CPO permits a
	// source to impose a stronger eligibility condition such as constant_p.
	return true;
}

/// @brief Converts a synthetic numeric source into its single-character replacement proxy.
template <typename source>
inline constexpr digit_proxy print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char, source>, source const &value) noexcept
	requires(::std::same_as<source, source_a> ||
			 ::std::same_as<source, source_b>)
{
	return {static_cast<char>('0' + value.value)};
}

/// @brief Certifies that the synthetic eligibility query is safe at the public inline boundary.
template <typename source>
inline constexpr ::std::true_type
	print_compiler_constant_materialization_query_inline_safe(
		::fast_io::io_reserve_type_t<char, source>) noexcept
	requires(::std::same_as<source, source_a> ||
			 ::std::same_as<source, source_b>)
{
	return {};
}

/// @brief Certifies that pre-normalization replacement preserves each synthetic source in isolation.
template <typename source>
inline constexpr ::std::true_type print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char, source>) noexcept
	requires(::std::same_as<source, source_a> ||
			 ::std::same_as<source, source_b>)
{
	return {};
}

struct text
{
	using value_type = char;
	char storage[16]{};
	::std::size_t size{};
};

/// @brief Constructs the test destination from one completed character range.
inline constexpr text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, text>, char const *first,
	char const *last) noexcept
{
	text result;
	for (; first != last; ++first)
	{
		result.storage[result.size++] = *first;
	}
	return result;
}

struct status_output
{
	using output_char_type = char;
	text *destination{};
};

/// @brief Exposes the test string as a status-owning output adapter.
inline constexpr status_output io_strlike_ref(
	::fast_io::io_alias_t, text &destination) noexcept
{
	return {&destination};
}

/// @brief Replaces the complete two-source run with an observable status spelling.
template <bool line>
inline constexpr void status_print_define(
	status_output output, source_a, source_b) noexcept
{
	constexpr char spelling[]{"STATUS\n"};
	constexpr ::std::size_t count{line ? 7u : 6u};
	for (::std::size_t index{}; index != count; ++index)
	{
		output.destination->storage[index] = spelling[index];
	}
	output.destination->size = count;
}

/// @brief Returns the initialized prefix of the fixed-capacity test result.
inline ::std::string_view view(text const &value) noexcept
{
	return {value.storage, value.size};
}

} // namespace

using normalized_a = ::fast_io::details::decay::print_semantic_forwarded_arg_t<
	char, source_a &&>;
using normalized_b = ::fast_io::details::decay::print_semantic_forwarded_arg_t<
	char, source_b &&>;

static_assert(::fast_io::operations::decay::defines::has_status_print_define<
			  false, status_output, normalized_a, normalized_b>);

// Replacing either source independently is printable, but replacing the whole
// run would bypass the destination's two-argument status owner and change
// observable output from "STATUS" to "12". Concat must reject that plan.
static_assert(!::fast_io::details::decay::
				  basic_general_concat_compiler_constant_source_available<
					  false, char, text, source_a &&, source_b &&>());
static_assert(!::fast_io::details::decay::
				  basic_general_concat_compiler_constant_materialization_available<
					  false, char, text, normalized_a, normalized_b>());

/// @brief Confirms that concat retains the whole-run status customization instead of direct proxy construction.
int main()
{
	auto result{
		::fast_io::basic_general_concat_compiler_constant_checked_entry<
			false, char, text>(source_a{1}, source_b{2})};
	return view(result) == "STATUS" ? 0 : 1;
}
