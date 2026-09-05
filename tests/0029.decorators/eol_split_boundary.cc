#include <fast_io.h>
#include <array>
#include <cstdlib>

namespace
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

template <::std::integral char_type>
consteval bool test_constexpr_state_machine()
{
	constexpr char_type lf_character{::fast_io::char_literal_v<u8'\n', char_type>};
	constexpr char_type cr_character{::fast_io::char_literal_v<u8'\r', char_type>};
	char_type const split_input[]{lf_character};
	char_type first_output[1]{};
	char_type second_output[1]{};
	::fast_io::decorators::basic_eol_converter<
		::fast_io::decorators::eol_scheme::lf,
		::fast_io::decorators::eol_scheme::crlf>
		expander;
	auto const first{expander.process_chars(split_input, split_input + 1, first_output, first_output + 1)};
	auto const second{
		expander.process_chars(first.input_result_ptr, split_input + 1, second_output, second_output + 1)};
	if (first.input_result_ptr != split_input || first.output_result_ptr != first_output + 1 ||
		first_output[0] != cr_character || second.input_result_ptr != split_input + 1 ||
		second.output_result_ptr != second_output + 1 || second_output[0] != lf_character ||
		expander.last_unfinished)
	{
		return false;
	}

	char_type const pair_input[]{cr_character, lf_character};
	char_type pair_output[1]{};
	::fast_io::decorators::basic_eol_converter<
		::fast_io::decorators::eol_scheme::crlf,
		::fast_io::decorators::eol_scheme::lf>
		contractor;
	auto const pair{
		contractor.process_chars(pair_input, pair_input + 2, pair_output, pair_output + 1)};
	return pair.input_result_ptr == pair_input + 2 && pair.output_result_ptr == pair_output + 1 &&
		   pair_output[0] == lf_character && !contractor.last_unfinished;
}

static_assert(test_constexpr_state_machine<char>());
static_assert(test_constexpr_state_machine<char8_t>());
static_assert(test_constexpr_state_machine<char16_t>());
static_assert(test_constexpr_state_machine<char32_t>());

template <::std::integral char_type>
void test_bounded_simd_expansion()
{
	constexpr char_type lf_character{::fast_io::char_literal_v<u8'\n', char_type>};
	constexpr char_type cr_character{::fast_io::char_literal_v<u8'\r', char_type>};
	if constexpr (
		::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size >= sizeof(char_type))
	{
		char_type const lf[]{lf_character};
		char_type lf_output[]{static_cast<char_type>('x'), static_cast<char_type>('g')};
		auto const lf_result{
			::fast_io::details::simd_lf_crlf_process_chars(lf, lf + 1, lf_output, lf_output + 1)};
		require(lf_result.input_result_ptr == lf);
		require(lf_result.output_result_ptr == lf_output);
		require(lf_output[0] == static_cast<char_type>('x'));
		require(lf_output[1] == static_cast<char_type>('g'));

		char_type const cr[]{cr_character};
		char_type cr_output[]{static_cast<char_type>('y'), static_cast<char_type>('g')};
		auto const cr_result{
			::fast_io::details::simd_lf_crlf_process_chars<true>(cr, cr + 1, cr_output, cr_output + 1)};
		require(cr_result.input_result_ptr == cr);
		require(cr_result.output_result_ptr == cr_output);
		require(cr_output[0] == static_cast<char_type>('y'));
		require(cr_output[1] == static_cast<char_type>('g'));
	}
}

template <::std::integral char_type>
void test_vector_predicate_polarity()
{
	constexpr ::std::size_t vector_units{
		::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size /
		sizeof(char_type)};
	constexpr char_type lf_character{::fast_io::char_literal_v<u8'\n', char_type>};
	constexpr char_type cr_character{::fast_io::char_literal_v<u8'\r', char_type>};
	if constexpr (vector_units != 0u)
	{
		::std::array<char_type, vector_units + 1u> expansion_input{};
		expansion_input[0] = static_cast<char_type>('a');
		for (::std::size_t i{1u}; i != expansion_input.size(); ++i)
		{
			expansion_input[i] = lf_character;
		}
		::std::array<char_type, vector_units * 2u + 2u> expansion_output{};
		expansion_output.back() = static_cast<char_type>('g');
		::fast_io::decorators::basic_eol_converter<
			::fast_io::decorators::eol_scheme::lf,
			::fast_io::decorators::eol_scheme::crlf>
			expander;
		auto const expanded{expander.process_chars(
			expansion_input.data(), expansion_input.data() + expansion_input.size(),
			expansion_output.data(), expansion_output.data() + expansion_output.size() - 1u)};
		require(expanded.input_result_ptr == expansion_input.data() + expansion_input.size());
		require(expanded.output_result_ptr == expansion_output.data() + expansion_output.size() - 1u);
		require(expansion_output[0] == static_cast<char_type>('a'));
		for (::std::size_t i{0u}; i != vector_units; ++i)
		{
			require(expansion_output[1u + i * 2u] == cr_character);
			require(expansion_output[2u + i * 2u] == lf_character);
		}
		require(expansion_output.back() == static_cast<char_type>('g'));

		::std::array<char_type, vector_units + 1u> contraction_input{};
		::std::array<char_type, vector_units + 2u> contraction_output{};
		for (auto &ch : contraction_input)
		{
			ch = static_cast<char_type>('q');
		}
		contraction_output.back() = static_cast<char_type>('g');
		::fast_io::decorators::basic_eol_converter<
			::fast_io::decorators::eol_scheme::crlf,
			::fast_io::decorators::eol_scheme::lf>
			contractor;
		auto const contracted{contractor.process_chars(
			contraction_input.data(), contraction_input.data() + contraction_input.size(),
			contraction_output.data(), contraction_output.data() + contraction_input.size())};
		require(contracted.input_result_ptr == contraction_input.data() + contraction_input.size());
		require(contracted.output_result_ptr == contraction_output.data() + contraction_input.size());
		for (::std::size_t i{}; i != contraction_input.size(); ++i)
		{
			require(contraction_output[i] == static_cast<char_type>('q'));
		}
		require(contraction_output.back() == static_cast<char_type>('g'));

		constexpr ::std::size_t match_position{vector_units / 2u};
		contraction_input[match_position] = cr_character;
		contraction_input[match_position + 1u] = lf_character;
		contraction_output.fill(char_type{});
		contraction_output.back() = static_cast<char_type>('g');
		::fast_io::decorators::basic_eol_converter<
			::fast_io::decorators::eol_scheme::crlf,
			::fast_io::decorators::eol_scheme::lf>
			matched_contractor;
		auto const matched{matched_contractor.process_chars(
			contraction_input.data(), contraction_input.data() + contraction_input.size(),
			contraction_output.data(), contraction_output.data() + contraction_input.size())};
		require(matched.input_result_ptr == contraction_input.data() + contraction_input.size());
		require(matched.output_result_ptr == contraction_output.data() + vector_units);
		for (::std::size_t i{}; i != vector_units; ++i)
		{
			require(contraction_output[i] ==
					(i == match_position ? lf_character : static_cast<char_type>('q')));
		}
		require(contraction_output.back() == static_cast<char_type>('g'));

		if constexpr (vector_units >= 4u)
		{
			// Two pairs make the committed output shorter than the first physical vector store. Every element after
			// the returned cursor is therefore a direct detector for speculative writes outside the process contract.
			::std::array<char_type, vector_units + 1u> speculative_input{};
			speculative_input.fill(static_cast<char_type>('q'));
			speculative_input[0] = cr_character;
			speculative_input[1] = lf_character;
			constexpr ::std::size_t second_pair{vector_units / 2u};
			speculative_input[second_pair] = cr_character;
			speculative_input[second_pair + 1u] = lf_character;
			::std::array<char_type, vector_units + 2u> guarded_output{};
			guarded_output.fill(static_cast<char_type>('g'));
			::fast_io::decorators::basic_eol_converter<
				::fast_io::decorators::eol_scheme::crlf,
				::fast_io::decorators::eol_scheme::lf>
				guarded_contractor;
			auto const guarded{guarded_contractor.process_chars(
				speculative_input.data(), speculative_input.data() + speculative_input.size(),
				guarded_output.data(), guarded_output.data() + speculative_input.size())};
			auto const expected_next{guarded_output.data() + speculative_input.size() - 2u};
			require(guarded.input_result_ptr == speculative_input.data() + speculative_input.size());
			require(guarded.output_result_ptr == expected_next);
			for (auto cursor{expected_next}; cursor != guarded_output.data() + guarded_output.size(); ++cursor)
			{
				require(*cursor == static_cast<char_type>('g'));
			}
		}
	}
}

void test_split_delimiter()
{
	::fast_io::decorators::basic_eol_converter<
		::fast_io::decorators::eol_scheme::lf,
		::fast_io::decorators::eol_scheme::crlf>
		converter;
	char const input[]{'\n'};
	char first_output[]{'x', 'g'};
	auto const first{converter.process_chars(input, input + 1, first_output, first_output + 1)};
	require(first.input_result_ptr == input);
	require(first.output_result_ptr == first_output + 1);
	require(first_output[0] == '\r');
	require(first_output[1] == 'g');
	require(converter.last_unfinished);

	char second_output[]{'x', 'g'};
	auto const second{
		converter.process_chars(first.input_result_ptr, input + 1, second_output, second_output + 1)};
	require(second.input_result_ptr == input + 1);
	require(second.output_result_ptr == second_output + 1);
	require(second_output[0] == '\n');
	require(second_output[1] == 'g');
	require(!converter.last_unfinished);
}

void test_prefix_then_split_delimiter()
{
	::fast_io::decorators::basic_eol_converter<
		::fast_io::decorators::eol_scheme::lf,
		::fast_io::decorators::eol_scheme::crlf>
		converter;
	char const input[]{'a', 'b', 'c', '\n'};
	char first_output[]{'x', 'x', 'x', 'x', 'g'};
	auto const first{converter.process_chars(input, input + 4, first_output, first_output + 4)};
	require(first.input_result_ptr == input + 3);
	require(first.output_result_ptr == first_output + 4);
	require(first_output[0] == 'a');
	require(first_output[1] == 'b');
	require(first_output[2] == 'c');
	require(first_output[3] == '\r');
	require(first_output[4] == 'g');
	require(converter.last_unfinished);

	char second_output[]{'x', 'g'};
	auto const second{
		converter.process_chars(first.input_result_ptr, input + 4, second_output, second_output + 1)};
	require(second.input_result_ptr == input + 4);
	require(second.output_result_ptr == second_output + 1);
	require(second_output[0] == '\n');
	require(second_output[1] == 'g');
	require(!converter.last_unfinished);
}

} // namespace

int main()
{
	// The legacy kernels derive lane count from byte width, so exercise each supported code-unit width independently.
	test_bounded_simd_expansion<char>();
	test_bounded_simd_expansion<char8_t>();
	test_bounded_simd_expansion<char16_t>();
	test_bounded_simd_expansion<char32_t>();
	test_vector_predicate_polarity<char>();
	test_vector_predicate_polarity<char8_t>();
	test_vector_predicate_polarity<char16_t>();
	test_vector_predicate_polarity<char32_t>();
	test_split_delimiter();
	test_prefix_then_split_delimiter();
}
