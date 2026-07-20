#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{

struct counted_input_range
{
	struct sentinel
	{
		int const *end{};
	};

	struct iterator
	{
		using value_type = int;
		using difference_type = std::ptrdiff_t;
		using iterator_concept = std::input_iterator_tag;
		int const *current{};

		[[nodiscard]] int operator*() const noexcept
		{
			return *current;
		}
		iterator &operator++() noexcept
		{
			++current;
			return *this;
		}
		void operator++(int) noexcept
		{
			++current;
		}
		friend bool operator==(iterator left, sentinel right) noexcept
		{
			return left.current == right.end;
		}
	};

	std::array<int, 3u> values{1, 2, 3};
	std::size_t begin_calls{};

	iterator begin() noexcept
	{
		++begin_calls;
		return {values.data()};
	}
	sentinel end() noexcept
	{
		return {values.data() + values.size()};
	}
};

template <typename char_type>
[[nodiscard]] std::basic_string<char_type> slow_debug_string(
	std::basic_string_view<char_type> source)
{
	std::basic_string<char_type> result;
	result.resize(source.size() * 10u + 2u);
	auto output{result.data()};
	*output++ = ::fast_io::char_literal_v<u8'"', char_type>;
	std::size_t consumed{};
	while (consumed != source.size())
	{
		auto const rendering{
			::fast_io::fmt::details::classify_debug_scalar<
				::fast_io::fmt::details::debug_text_kind::string>(
				source.data() + consumed, source.size() - consumed)};
		output = ::fast_io::fmt::details::emit_debug_scalar(
			output, source.data() + consumed, rendering,
			rendering.storage_size);
		consumed += rendering.source_units;
	}
	*output++ = ::fast_io::char_literal_v<u8'"', char_type>;
	result.resize(static_cast<std::size_t>(output - result.data()));
	return result;
}

template <typename char_type>
[[nodiscard]] bool randomized_debug_fields_match_scalar_reference()
{
	std::uint_least64_t state{0x9e3779b97f4a7c15ULL};
	for (std::size_t length{}; length != 65u; ++length)
	{
		std::basic_string<char_type> source(length, char_type{});
		for (std::size_t index{}; index != length; ++index)
		{
			state = state * 6364136223846793005ULL + 1442695040888963407ULL;
			if constexpr (sizeof(char_type) == 1u)
			{
				source[index] = static_cast<char_type>(state >> 56u);
			}
			else if constexpr (sizeof(char_type) == 2u)
			{
				source[index] = static_cast<char_type>(state >> 48u);
			}
			else
			{
				source[index] = static_cast<char_type>(state >> 32u);
			}
		}

		auto const expected{slow_debug_string<char_type>(source)};
		auto const field{::fast_io::fmt::details::make_debug_string_field(
			::fast_io::basic_io_scatter_t<char_type>{source.data(), source.size()},
			{})};
		using field_type = std::remove_cv_t<decltype(field)>;
		auto const size{::fast_io::print_reserve_size(
			::fast_io::io_reserve_type<char_type, field_type>, field)};
		std::basic_string<char_type> actual(size, char_type{});
		auto const end{::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, field_type>,
			actual.data(), field)};
		if (end != actual.data() + actual.size() || actual != expected)
		{
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool direct_range_paths_and_capacity_fallback()
{
	using namespace std::literals;
	std::vector<std::string_view> values{
		"alpha"sv, "a\"b"sv, "slash\\"sv, "line\n\t"sv,
		std::string_view{"\x01", 1u}, "caf\xc3\xa9"sv,
		std::string_view{"\xff", 1u}};
	auto const expected{::fast_io::fmt::concat_std<"{}">(values)};

	std::array<char, 512u> roomy{};
	::fast_io::obuffer_view roomy_output{roomy.data(), roomy.data() + roomy.size()};
	::fast_io::fmt::print<"{}">(roomy_output, values);
	if (std::string_view{roomy.data(), roomy_output.size()} != expected)
	{
		return false;
	}

	// The exact output fits, but the conservative 10x proof does not.  This
	// exercises the unchanged measured fallback and checks the guard byte.
	std::vector<char> tight(expected.size() + 1u, '\x5a');
	::fast_io::obuffer_view tight_output{tight.data(),
										 tight.data() + expected.size()};
	::fast_io::fmt::print<"{}">(tight_output, values);
	if (tight_output.size() != expected.size() ||
		std::string_view{tight.data(), expected.size()} != expected ||
		tight.back() != '\x5a')
	{
		return false;
	}

	std::map<std::string_view, int> mapping{{"alpha", 1}, {"b\"eta", -2}};
	auto const expected_map{::fast_io::fmt::concat_std<"{}">(mapping)};
	::fast_io::obuffer_view map_output{roomy.data(), roomy.data() + roomy.size()};
	::fast_io::fmt::print<"{}">(map_output, mapping);
	if (std::string_view{roomy.data(), map_output.size()} != expected_map)
	{
		return false;
	}

	auto tuple{std::tuple{std::string_view{"cat\n"}, 7}};
	auto const expected_tuple{::fast_io::fmt::concat_std<"{}">(tuple)};
	::fast_io::obuffer_view tuple_output{roomy.data(), roomy.data() + roomy.size()};
	::fast_io::fmt::print<"{}">(tuple_output, tuple);
	return std::string_view{roomy.data(), tuple_output.size()} == expected_tuple;
}

template <typename char_type>
[[nodiscard]] bool direct_character_domain_matches_concat()
{
	std::basic_string<char_type> first{
		static_cast<char_type>('a'), static_cast<char_type>('"'),
		static_cast<char_type>('b')};
	std::basic_string<char_type> second{
		static_cast<char_type>('x'), static_cast<char_type>('\n'),
		static_cast<char_type>(1)};
	std::vector<std::basic_string_view<char_type>> values{first, second};
	std::basic_string<char_type> expected;
	std::array<char_type, 128u> storage{};
	::fast_io::basic_obuffer_view<char_type> output{
		storage.data(), storage.data() + storage.size()};
	if constexpr (std::same_as<char_type, char>)
	{
		expected = ::fast_io::fmt::concat_std<"{}">(values);
		::fast_io::fmt::print<"{}">(output, values);
	}
	else if constexpr (std::same_as<char_type, wchar_t>)
	{
		expected = ::fast_io::fmt::wconcat_std<L"{}">(values);
		::fast_io::fmt::wprint<L"{}">(output, values);
	}
	else if constexpr (std::same_as<char_type, char8_t>)
	{
		expected = ::fast_io::fmt::u8concat_std<u8"{}">(values);
		::fast_io::fmt::u8print<u8"{}">(output, values);
	}
	else if constexpr (std::same_as<char_type, char16_t>)
	{
		expected = ::fast_io::fmt::u16concat_std<u"{}">(values);
		::fast_io::fmt::u16print<u"{}">(output, values);
	}
	else
	{
		expected = ::fast_io::fmt::u32concat_std<U"{}">(values);
		::fast_io::fmt::u32print<U"{}">(output, values);
	}
	return std::basic_string_view<char_type>{storage.data(), output.size()} ==
		   expected;
}

[[nodiscard]] bool zero_capacity_and_single_pass_input_range()
{
	char zero_storage{};
	::fast_io::obuffer_view zero_output{&zero_storage, &zero_storage};
	::fast_io::basic_obuffer_view_ref<char> zero_output_ref{&zero_output};
	auto const field{::fast_io::fmt::details::make_debug_string_field(
		::fast_io::basic_io_scatter_t<char>{"x", 1u}, {})};
	if (::fast_io::fmt::details::emit_default_debug_string_to_obuffer(
			zero_output_ref, field) ||
		zero_output.size() != 0u || zero_storage != char{})
	{
		return false;
	}
	::fast_io::obuffer_view null_output{};
	::fast_io::basic_obuffer_view_ref<char> null_output_ref{&null_output};
	if (::fast_io::fmt::details::emit_default_debug_string_to_obuffer(
			null_output_ref, field))
	{
		return false;
	}
	char reversed_storage[2u]{};
	::fast_io::obuffer_view reversed_output{
		reversed_storage + 1u, reversed_storage};
	::fast_io::basic_obuffer_view_ref<char> reversed_output_ref{&reversed_output};
	if (::fast_io::fmt::details::emit_default_debug_string_to_obuffer(
			reversed_output_ref, field))
	{
		return false;
	}
	char policy_storage[32u]{};
	::fast_io::obuffer_view policy_output{
		policy_storage, policy_storage + 32u};
	::fast_io::basic_obuffer_view_ref<char> policy_output_ref{&policy_output};
	auto policy_field{field};
	policy_field.options.maximum_display_width = 1u;
	if (::fast_io::fmt::details::emit_default_debug_string_to_obuffer(
			policy_output_ref, policy_field) ||
		policy_output.size() != 0u)
	{
		return false;
	}

	counted_input_range input{};
	char storage[32u]{};
	::fast_io::obuffer_view output{storage, storage + 32u};
	::fast_io::fmt::print<"{}">(output, input);
	return input.begin_calls == 1u &&
		   std::string_view{storage, output.size()} == "[1, 2, 3]";
}

static_assert(::fast_io::fmt::details::debug_ascii_copy_run<
				  ::fast_io::fmt::details::debug_text_kind::string>(u8"abc\"", 4u) ==
			  3u);
static_assert(::fast_io::fmt::details::debug_ascii_copy_run<
				  ::fast_io::fmt::details::debug_text_kind::string>(u8"a'b", 3u) ==
			  3u);
static_assert(::fast_io::fmt::details::debug_ascii_copy_run<
				  ::fast_io::fmt::details::debug_text_kind::character>(u8"a'b", 3u) ==
			  1u);

} // namespace

int main()
{
	return randomized_debug_fields_match_scalar_reference<char>() &&
				   randomized_debug_fields_match_scalar_reference<wchar_t>() &&
				   randomized_debug_fields_match_scalar_reference<char8_t>() &&
				   randomized_debug_fields_match_scalar_reference<char16_t>() &&
				   randomized_debug_fields_match_scalar_reference<char32_t>() &&
				   direct_range_paths_and_capacity_fallback() &&
				   direct_character_domain_matches_concat<char>() &&
				   direct_character_domain_matches_concat<wchar_t>() &&
				   direct_character_domain_matches_concat<char8_t>() &&
				   direct_character_domain_matches_concat<char16_t>() &&
				   direct_character_domain_matches_concat<char32_t>() &&
				   zero_capacity_and_single_pass_input_range()
			   ? 0
			   : 1;
}
