#include <fast_io_format.h>

#include <array>
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

template <::std::integral char_type>
[[nodiscard]] bool untouched_suffix(
	::std::array<char_type, 64u> const &storage,
	::std::size_t used, char_type sentinel) noexcept
{
	for (::std::size_t index{used}; index != storage.size(); ++index)
	{
		if (storage[index] != sentinel)
		{
			return false;
		}
	}
	return true;
}

template <::std::integral char_type>
[[nodiscard]] bool raw_constant_float_writes_only_its_exact_extent()
{
	constexpr char_type sentinel{static_cast<char_type>('#')};
	::std::array<char_type, 64u> storage{};
	storage.fill(sentinel);
	::fast_io::basic_obuffer_view<char_type> output{
		storage.data(), storage.data() + storage.size()};
	::fast_io::print(output, 3.2);
	return output.size() == 3u &&
		equals_ascii(::std::basic_string_view<char_type>{storage.data(), 3u},
			"3.2") &&
		untouched_suffix(storage, 3u, sentinel);
}

template <::std::integral char_type>
[[nodiscard]] bool format_constant_float_writes_only_its_exact_extent()
{
	constexpr char_type sentinel{static_cast<char_type>('#')};
	::std::array<char_type, 64u> storage{};
	storage.fill(sentinel);
	::fast_io::basic_obuffer_view<char_type> output{
		storage.data(), storage.data() + storage.size()};
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::print<"i={}">(output, 3.2);
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		::fast_io::fmt::wprint<L"i={}">(output, 3.2);
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		::fast_io::fmt::u8print<u8"i={}">(output, 3.2);
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		::fast_io::fmt::u16print<u"i={}">(output, 3.2);
	}
	else
	{
		::fast_io::fmt::u32print<U"i={}">(output, 3.2);
	}
	return output.size() == 5u &&
		equals_ascii(::std::basic_string_view<char_type>{storage.data(), 5u},
			"i=3.2") &&
		untouched_suffix(storage, 5u, sentinel);
}

template <::std::integral char_type>
[[nodiscard]] bool concat_constant_float_is_exact()
{
	if constexpr (::std::same_as<char_type, char>)
	{
		return equals_ascii(::fast_io::concat_std(3.2), "3.2") &&
			equals_ascii(::fast_io::fmt::concat_std<"i={}">(3.2),
				"i=3.2") &&
			equals_ascii(::fast_io::fmt::concatf_std<"i=%g">(3.2),
				"i=3.2");
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return equals_ascii(::fast_io::wconcat_std(3.2), "3.2") &&
			equals_ascii(::fast_io::fmt::wconcat_std<L"i={}">(3.2),
				"i=3.2") &&
			equals_ascii(::fast_io::fmt::wconcatf_std<L"i=%g">(3.2),
				"i=3.2");
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return equals_ascii(::fast_io::u8concat_std(3.2), "3.2") &&
			equals_ascii(::fast_io::fmt::u8concat_std<u8"i={}">(3.2),
				"i=3.2") &&
			equals_ascii(::fast_io::fmt::u8concatf_std<u8"i=%g">(3.2),
				"i=3.2");
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		return equals_ascii(::fast_io::u16concat_std(3.2), "3.2") &&
			equals_ascii(::fast_io::fmt::u16concat_std<u"i={}">(3.2),
				"i=3.2") &&
			equals_ascii(::fast_io::fmt::u16concatf_std<u"i=%g">(3.2),
				"i=3.2");
	}
	else
	{
		return equals_ascii(::fast_io::u32concat_std(3.2), "3.2") &&
			equals_ascii(::fast_io::fmt::u32concat_std<U"i={}">(3.2),
				"i=3.2") &&
			equals_ascii(::fast_io::fmt::u32concatf_std<U"i=%g">(3.2),
				"i=3.2");
	}
}

template <::std::integral char_type>
[[nodiscard]] bool run_one_character_type()
{
	return raw_constant_float_writes_only_its_exact_extent<char_type>() &&
		format_constant_float_writes_only_its_exact_extent<char_type>() &&
		concat_constant_float_is_exact<char_type>();
}

} // namespace

int main()
{
	return run_one_character_type<char>() &&
		run_one_character_type<wchar_t>() &&
		run_one_character_type<char8_t>() &&
		run_one_character_type<char16_t>() &&
		run_one_character_type<char32_t>()
		? 0
		: 1;
}
