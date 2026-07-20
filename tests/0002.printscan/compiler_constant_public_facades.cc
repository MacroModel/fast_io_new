#include <fast_io_format.h>

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace
{

template <typename range_type>
[[nodiscard]] bool equals_ascii(
	range_type const &range, ::std::string_view expected) noexcept
{
	using char_type = ::std::remove_cv_t<
		::std::remove_pointer_t<decltype(range.data())>>;
	if (range.size() != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		if (range.data()[index] != static_cast<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type, typename callback_type>
[[nodiscard]] bool output_equals_ascii(
	callback_type callback, ::std::string_view expected)
{
	char_type storage[256u]{};
	::fast_io::basic_obuffer_view<char_type> output{
		storage, storage + 256u};
	callback(output);
	return equals_ascii(output, expected);
}

template <::std::integral char_type>
[[nodiscard]] bool raw_print_facades_are_consistent()
{
	return output_equals_ascii<char_type>(
		[](auto &output) {
			::fast_io::print(output, 2);
			::fast_io::println(output, 3);
			::fast_io::operations::print_freestanding<false>(output, 4);
			::fast_io::operations::print_freestanding<true>(output, 5);
			::fast_io::debug_print(output, 6);
			::fast_io::debug_println(output, 7);
			::fast_io::perr(output, 8);
			::fast_io::perrln(output, 9);
			::fast_io::debug_perr(output, 0);
			::fast_io::debug_perrln(output, 1);
		},
		"23\n45\n67\n89\n01\n");
}

template <::std::integral char_type>
[[nodiscard]] bool raw_concat_facades_are_consistent()
{
	if constexpr (::std::same_as<char_type, char>)
	{
		return equals_ascii(::fast_io::concat_std(2, "|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::concatln_std(2), "2\n") &&
			equals_ascii(::fast_io::concat_fast_io(2, "|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::concatln_fast_io(2), "2\n") &&
			equals_ascii(::fast_io::tlc::concat_fast_io_tlc(2, "|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::tlc::concatln_fast_io_tlc(2), "2\n");
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return equals_ascii(::fast_io::wconcat_std(2, L"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::wconcatln_std(2), "2\n") &&
			equals_ascii(::fast_io::wconcat_fast_io(2, L"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::wconcatln_fast_io(2), "2\n") &&
			equals_ascii(::fast_io::tlc::wconcat_fast_io_tlc(2, L"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::tlc::wconcatln_fast_io_tlc(2), "2\n");
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return equals_ascii(::fast_io::u8concat_std(2, u8"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u8concatln_std(2), "2\n") &&
			equals_ascii(::fast_io::u8concat_fast_io(2, u8"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u8concatln_fast_io(2), "2\n") &&
			equals_ascii(::fast_io::tlc::u8concat_fast_io_tlc(2, u8"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::tlc::u8concatln_fast_io_tlc(2), "2\n");
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		return equals_ascii(::fast_io::u16concat_std(2, u"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u16concatln_std(2), "2\n") &&
			equals_ascii(::fast_io::u16concat_fast_io(2, u"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u16concatln_fast_io(2), "2\n") &&
			equals_ascii(::fast_io::tlc::u16concat_fast_io_tlc(2, u"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::tlc::u16concatln_fast_io_tlc(2), "2\n");
	}
	else
	{
		return equals_ascii(::fast_io::u32concat_std(2, U"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u32concatln_std(2), "2\n") &&
			equals_ascii(::fast_io::u32concat_fast_io(2, U"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::u32concatln_fast_io(2), "2\n") &&
			equals_ascii(::fast_io::tlc::u32concat_fast_io_tlc(2, U"|", 3.2), "2|3.2") &&
			equals_ascii(::fast_io::tlc::u32concatln_fast_io_tlc(2), "2\n");
	}
}

template <::std::integral char_type>
[[nodiscard]] bool format_facades_are_consistent()
{
	bool direct{};
	bool concat{};
	if constexpr (::std::same_as<char_type, char>)
	{
		direct = output_equals_ascii<char>(
			[](auto &output) {
				::fast_io::fmt::print<"{}|{}">(output, 2, 3.2);
				::fast_io::fmt::printf<"%d|%g">(output, 2, 3.2);
			},
			"2|3.22|3.2");
		concat = equals_ascii(
			::fast_io::fmt::concat_std<"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::concatf_std<"%d|%g">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::concat_fast_io<"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::concatf_fast_io<"%d|%g">(2, 3.2), "2|3.2");
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		direct = output_equals_ascii<wchar_t>(
			[](auto &output) {
				::fast_io::fmt::wprint<L"{}|{}">(output, 2, 3.2);
				::fast_io::fmt::wprintf<L"%d|%g">(output, 2, 3.2);
			},
			"2|3.22|3.2");
		concat = equals_ascii(
			::fast_io::fmt::wconcat_std<L"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::wconcatf_std<L"%d|%g">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::wconcat_fast_io<L"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::wconcatf_fast_io<L"%d|%g">(2, 3.2), "2|3.2");
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		direct = output_equals_ascii<char8_t>(
			[](auto &output) {
				::fast_io::fmt::u8print<u8"{}|{}">(output, 2, 3.2);
				::fast_io::fmt::u8printf<u8"%d|%g">(output, 2, 3.2);
			},
			"2|3.22|3.2");
		concat = equals_ascii(
			::fast_io::fmt::u8concat_std<u8"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u8concatf_std<u8"%d|%g">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u8concat_fast_io<u8"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u8concatf_fast_io<u8"%d|%g">(2, 3.2), "2|3.2");
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		direct = output_equals_ascii<char16_t>(
			[](auto &output) {
				::fast_io::fmt::u16print<u"{}|{}">(output, 2, 3.2);
				::fast_io::fmt::u16printf<u"%d|%g">(output, 2, 3.2);
			},
			"2|3.22|3.2");
		concat = equals_ascii(
			::fast_io::fmt::u16concat_std<u"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u16concatf_std<u"%d|%g">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u16concat_fast_io<u"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u16concatf_fast_io<u"%d|%g">(2, 3.2), "2|3.2");
	}
	else
	{
		direct = output_equals_ascii<char32_t>(
			[](auto &output) {
				::fast_io::fmt::u32print<U"{}|{}">(output, 2, 3.2);
				::fast_io::fmt::u32printf<U"%d|%g">(output, 2, 3.2);
			},
			"2|3.22|3.2");
		concat = equals_ascii(
			::fast_io::fmt::u32concat_std<U"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u32concatf_std<U"%d|%g">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u32concat_fast_io<U"{}|{}">(2, 3.2), "2|3.2") &&
			equals_ascii(
				::fast_io::fmt::u32concatf_fast_io<U"%d|%g">(2, 3.2), "2|3.2");
	}
	return direct && concat;
}

// These functions are compile-only instantiations. Calling either would
// intentionally terminate, so the runtime test does not reference them.
[[maybe_unused, noreturn]] void instantiate_panic(
	::fast_io::basic_obuffer_view<char> &output)
{
	::fast_io::panic(output, 2);
}

[[maybe_unused, noreturn]] void instantiate_panicln(
	::fast_io::basic_obuffer_view<char> &output)
{
	::fast_io::panicln(output, 2);
}

} // namespace

int main()
{
	return raw_print_facades_are_consistent<char>() &&
		raw_print_facades_are_consistent<wchar_t>() &&
		raw_print_facades_are_consistent<char8_t>() &&
		raw_print_facades_are_consistent<char16_t>() &&
		raw_print_facades_are_consistent<char32_t>() &&
		raw_concat_facades_are_consistent<char>() &&
		raw_concat_facades_are_consistent<wchar_t>() &&
		raw_concat_facades_are_consistent<char8_t>() &&
		raw_concat_facades_are_consistent<char16_t>() &&
		raw_concat_facades_are_consistent<char32_t>() &&
		format_facades_are_consistent<char>() &&
		format_facades_are_consistent<wchar_t>() &&
		format_facades_are_consistent<char8_t>() &&
		format_facades_are_consistent<char16_t>() &&
		format_facades_are_consistent<char32_t>()
		? 0
		: 1;
}
