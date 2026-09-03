#include <fast_io.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

template <typename value_type>
void check_one(char const *first, char const *last)
{
	value_type cpo_value{static_cast<value_type>(0x5a5a5a5au)};
	value_type generic_value{cpo_value};
	auto scanner{
		::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(cpo_value))};
	auto const cpo_result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, decltype(scanner)>, first, last,
		scanner)};
	auto const generic_result{
		::fast_io::details::scan_int_contiguous_define_impl<
			10u, false, false, false, false, false>(
			first, last, generic_value)};
	if (cpo_result.iter != generic_result.iter ||
		cpo_result.code != generic_result.code ||
		cpo_value != generic_value)
	{
		::std::abort();
	}
}

template <typename value_type, ::std::size_t size>
void check_literal(char const (&text)[size])
{
	check_one<value_type>(text, text + size - 1u);
}

inline constexpr bool constexpr_cpo_test()
{
	char const text[]{"4294967295"};
	::std::uint_least32_t value{};
	auto scanner{
		::fast_io::io_scan_forward<char>(::fast_io::io_scan_alias(value))};
	auto const result{scan_contiguous_define(
		::fast_io::io_reserve_type<char, decltype(scanner)>, text,
		text + sizeof(text) - 1u, scanner)};
	return result.iter == text + sizeof(text) - 1u &&
		result.code == ::fast_io::parse_code::ok &&
		value == UINT32_C(4294967295);
}

static_assert(constexpr_cpo_test());

int main()
{
	check_literal<::std::uint_least32_t>("0");
	check_literal<::std::uint_least32_t>("000000000");
	check_literal<::std::uint_least32_t>("999999999");
	check_literal<::std::uint_least32_t>("1000000000");
	check_literal<::std::uint_least32_t>("4294967295");
	check_literal<::std::uint_least32_t>("4294967296");
	check_literal<::std::uint_least32_t>("9999999999");
	check_literal<::std::uint_least32_t>("99999999999x");
	check_literal<::std::uint_least64_t>("18446744073709551615");
	check_literal<::std::uint_least64_t>("18446744073709551616");

	char buffer[96];
	::std::uint_least64_t state{UINT64_C(0x91e10da5c79e7b1d)};
	constexpr char alphabet[]{"0123456789 +-xA\t\n"};
	for (::std::size_t test{}; test != 1000000u; ++test)
	{
		state ^= state << 7u;
		state ^= state >> 9u;
		state ^= state << 8u;
		auto const size{static_cast<::std::size_t>(state % sizeof(buffer))};
		for (::std::size_t index{}; index != size; ++index)
		{
			state ^= state << 7u;
			state ^= state >> 9u;
			state ^= state << 8u;
			buffer[index] = alphabet[state % (sizeof(alphabet) - 1u)];
		}
		check_one<::std::uint_least32_t>(buffer, buffer + size);
		check_one<::std::uint_least64_t>(buffer, buffer + size);
	}
}
