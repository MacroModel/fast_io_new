#include <cassert>
#include <string>
#include <string_view>
#include <type_traits>

#include <fast_io.h>

namespace
{

struct mixed_status_source
{};

struct mixed_status_spelling
{};

/// @brief Supplies a stable status-forwarded spelling for the mixed-source availability test.
inline constexpr mixed_status_spelling status_io_print_forward(
	::fast_io::io_alias_type_t<char>, mixed_status_source) noexcept
{
	return {};
}

/// @brief Declares the one-byte extent of the mixed status spelling.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, mixed_status_spelling>) noexcept
{
	return 1u;
}

/// @brief Emits the mixed status spelling into an exact reserve range.
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, mixed_status_spelling>, char *iter,
	mixed_status_spelling) noexcept
{
	*iter++ = 'S';
	return iter;
}

static_assert(::fast_io::details::
				  inplace_to_compiler_constant_source_available<
					  char, ::std::string,
					  decltype(::fast_io::mnp::static_arg<3>) &&,
					  mixed_status_source &&>());

struct status_source
{};

struct status_source_record
{};

struct status_proxy
{};

struct status_replacement_record
{};

struct status_tail
{};

struct contiguous_capture
{
	::std::string value;
};

/// @brief Captures the complete contiguous source range for later semantic assertions.
inline ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::parameter<contiguous_capture &>>,
	char const *first, char const *last,
	::fast_io::parameter<contiguous_capture &> output)
{
	output.reference.value.assign(first, last);
	return {last, ::fast_io::parse_code::ok};
}

/// @brief Normalizes the original status source into its dynamic-output record.
inline constexpr status_source_record print_alias_define(
	::fast_io::io_alias_t, status_source &) noexcept
{
	return {};
}

/// @brief Normalizes the materialized proxy into its replacement record.
inline constexpr status_replacement_record print_alias_define(
	::fast_io::io_alias_t, status_proxy &&) noexcept
{
	return {};
}

using dynamic_output = ::fast_io::basic_dynamic_output_buffer_ref<
	::fast_io::basic_dynamic_output_buffer<char>>;

/// @brief Appends one byte to the dynamic output buffer used by status tests.
inline void append(dynamic_output output, char value) noexcept
{
	auto iter{::fast_io::obuffer_curr(output)};
	*iter++ = value;
	::fast_io::obuffer_set_curr(output, iter);
}

/// @brief Emits the ordinary status-source record through the dynamic fallback.
inline void print_define(
	::fast_io::io_reserve_type_t<char, status_source_record>,
	dynamic_output output, status_source_record &) noexcept
{
	append(output, 'R');
}

/// @brief Emits the replacement status record through the dynamic fallback.
inline void print_define(
	::fast_io::io_reserve_type_t<char, status_replacement_record>,
	dynamic_output output, status_replacement_record &) noexcept
{
	append(output, 'R');
}

/// @brief Emits the trailing status record when no whole-run customization intercepts it.
inline void print_define(
	::fast_io::io_reserve_type_t<char, status_tail>, dynamic_output output,
	status_tail &) noexcept
{
	append(output, 'T');
}

/// @brief Provides an observable whole-run status customization for source and tail records.
template <bool line>
inline void status_print_define(
	dynamic_output output, status_source_record &, status_tail &) noexcept
{
	append(output, 'O');
	append(output, 'K');
	// The status formatter appends a line terminator only for the line-specialized dispatcher under test.
	if constexpr (line)
	{
		append(output, '\n');
	}
}

/// @brief Declares the one-byte reserve extent of a status replacement proxy.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, status_proxy>) noexcept
{
	return 1u;
}

/// @brief Emits the status replacement proxy's ordinary spelling.
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, status_proxy>, char *iter,
	status_proxy) noexcept
{
	*iter++ = 'R';
	return iter;
}

/// @brief Makes the synthetic status source deterministically eligible for replacement.
inline constexpr bool print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char, status_source>,
	status_source const &) noexcept
{
	return true;
}

/// @brief Materializes the synthetic status source into its reserve-printable proxy.
inline constexpr status_proxy print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char, status_source>,
	status_source const &) noexcept
{
	return {};
}

/// @brief Certifies that the synthetic eligibility query is safe at the public inline boundary.
inline constexpr ::std::true_type
	print_compiler_constant_materialization_query_inline_safe(
		::fast_io::io_reserve_type_t<char, status_source>) noexcept
{
	return {};
}

/// @brief Certifies the source's isolated pre-normalization replacement semantics.
inline constexpr ::std::true_type
	print_compiler_constant_pre_normalization_safe(
		::fast_io::io_reserve_type_t<char, status_source>) noexcept
{
	return {};
}

static_assert(!::fast_io::details::
				  inplace_to_compiler_constant_source_available<
					  char, contiguous_capture, status_source &&, status_tail &&>());

inline char order_events[2]{};
inline ::std::size_t order_event_count{};

struct order_source
{};

struct order_spelling
{};

/// @brief Records source aliasing so the test can compare it with target construction order.
inline order_spelling print_alias_define(
	::fast_io::io_alias_t, order_source &) noexcept
{
	order_events[order_event_count++] = 'A';
	return {};
}

/// @brief Declares the one-byte extent of the order-test spelling.
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, order_spelling>) noexcept
{
	return 1u;
}

/// @brief Emits the order-test spelling into its exact range.
inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, order_spelling>, char *iter,
	order_spelling) noexcept
{
	*iter++ = '7';
	return iter;
}

struct order_target
{
	char value{};

	/// @brief Records target construction after the public source alias boundary.
	order_target() noexcept
	{
		order_events[order_event_count++] = 'C';
	}
};

/// @brief Parses one character into the order target and reports an empty input as invalid.
inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char,
								 ::fast_io::parameter<order_target &>>,
	char const *first, char const *last,
	::fast_io::parameter<order_target &> output) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	output.reference.value = *first;
	return {first + 1, ::fast_io::parse_code::ok};
}

struct zero_target
{
	int value{17};
};

/// @brief Checks one public character-domain facade against the same decimal spelling.
template <::std::integral char_type>
void check_character_domain()
{
	using string_type = ::std::basic_string<char_type>;
	string_type expected;
	expected.push_back(static_cast<char_type>('4'));
	expected.push_back(static_cast<char_type>('2'));
	string_type actual;
	// The narrow-character facade is the reference domain for the shared value-returning conversion test.
	if constexpr (::std::same_as<char_type, char>)
	{
		actual = ::fast_io::to<string_type>(42);
	}
	// Wide characters use the dedicated public facade while retaining the same decimal spelling.
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		actual = ::fast_io::wto<string_type>(42);
	}
	// UTF-8 code units exercise the character-domain-specific public forwarding boundary.
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		actual = ::fast_io::u8to<string_type>(42);
	}
	// UTF-16 code units verify that compiler-constant replacement is independent of character width.
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		actual = ::fast_io::u16to<string_type>(42);
	}
	// The remaining supported domain is UTF-32 and uses its dedicated facade.
	else
	{
		actual = ::fast_io::u32to<string_type>(42);
	}
	assert(actual == expected);
}

/// @brief Exercises the value-returning facade during mandatory constant evaluation.
consteval int consteval_to()
{
	return ::fast_io::to<int>(::fast_io::mnp::static_arg<42>);
}

static_assert(consteval_to() == 42);

int volatile runtime_integer{17};
double volatile runtime_floating{2.5};

} // namespace

/// @brief Runs constant, volatile, status, ordering, empty-target, and character-domain conversion checks.
int main()
{
	assert(::fast_io::to<::std::string>(42) == "42");
	assert(::fast_io::to<::std::string>(3.14) == "3.14");
	assert(::fast_io::to<::std::string>(::fast_io::mnp::static_arg<42>) == "42");
	assert(::fast_io::to<::std::string>(::fast_io::mnp::static_arg<3.14>) == "3.14");
	assert(::fast_io::to<::std::string>(runtime_integer) == "17");
	assert(::fast_io::to<::std::string>(runtime_floating) == "2.5");
	assert(::fast_io::to<::std::string>(::std::string_view{"runtime"}) == "runtime");

	::std::string inplace_constant;
	::fast_io::inplace_to(
		inplace_constant, ::fast_io::mnp::static_arg<42>);
	assert(inplace_constant == "42");
	::std::string inplace_runtime;
	::fast_io::inplace_to(inplace_runtime, runtime_integer);
	assert(inplace_runtime == "17");
	::std::string basic_inplace_constant;
	::fast_io::basic_inplace_to<char>(
		basic_inplace_constant, ::fast_io::mnp::static_arg<3.14>);
	assert(basic_inplace_constant == "3.14");
	::std::string basic_inplace_runtime;
	::fast_io::basic_inplace_to<char>(
		basic_inplace_runtime, runtime_floating);
	assert(basic_inplace_runtime == "2.5");

	auto mixed{::fast_io::to<::std::string>(
		::fast_io::mnp::static_arg<3>, mixed_status_source{})};
	assert(mixed == "3S");

	auto status{::fast_io::to<contiguous_capture>(status_source{}, status_tail{})};
	assert(status.value == "OK");

	order_event_count = 0u;
	auto ordered{::fast_io::to<order_target>(order_source{})};
	assert(ordered.value == '7');
	assert(order_event_count == 2u);
	assert(order_events[0] == 'A');
	assert(order_events[1] == 'C');

	assert(::fast_io::to<int>() == 0);
	assert(::fast_io::to<zero_target>().value == 17);
	assert(::fast_io::to<::std::string>().empty());

	check_character_domain<char>();
	check_character_domain<wchar_t>();
	check_character_domain<char8_t>();
	check_character_domain<char16_t>();
	check_character_domain<char32_t>();
	assert((::fast_io::basic_to<char, ::std::string>(42) == "42"));
}
