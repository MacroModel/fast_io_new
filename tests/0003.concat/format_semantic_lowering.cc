#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace
{

using namespace ::std::literals;

inline constexpr ::std::string_view expected_record{
	"<a42b>|yes|no|0042||S|7"};

template <typename string_type>
[[nodiscard]] constexpr bool equals_expected(string_type const &value) noexcept
{
	return ::std::string_view{value.data(), value.size()} == expected_record;
}

[[nodiscard]] consteval bool semantic_nodes_are_constant_evaluated()
{
	::std::array<char, 64u> storage{};
	::fast_io::obuffer_view output{storage};
	auto const pack{::fast_io::mnp::pack("a", 42, "b")};
	auto const selected{::fast_io::mnp::cond(true, "yes", "no")};
	auto const rejected{::fast_io::mnp::cond(false, "yes", "no")};
	auto const width{::fast_io::mnp::right(42, 4u, '0')};
	::fast_io::fmt::print<"<{}>|{}|{}|{}|{}|{}|{fixed}">(
		output, pack, selected, rejected, width, ::fast_io::io_null,
		::fast_io::mnp::static_arg<"S">,
		::fast_io::mnp::static_arg<"fixed", 7>);
	return ::std::string_view{storage.data(), output.size()} == expected_record;
}

static_assert(semantic_nodes_are_constant_evaluated());

} // namespace

int main()
{
	auto const pack{::fast_io::mnp::pack("a", 42, "b")};
	auto const selected{::fast_io::mnp::cond(true, "yes", "no")};
	auto const rejected{::fast_io::mnp::cond(false, "yes", "no")};
	auto const width{::fast_io::mnp::right(42, 4u, '0')};

	auto const formatted_std{
		::fast_io::fmt::concat_std<"<{}>|{}|{}|{}|{}|{}|{fixed}">(
			pack, selected, rejected, width, ::fast_io::io_null,
			::fast_io::mnp::static_arg<"S">,
			::fast_io::mnp::static_arg<"fixed", 7>)};
	if (!equals_expected(formatted_std))
	{
		return 1;
	}

	auto const formatted_fast_io{
		::fast_io::fmt::concat_fast_io<"<{}>|{}|{}|{}|{}|{}|{fixed}">(
			pack, selected, rejected, width, ::fast_io::io_null,
			::fast_io::mnp::static_arg<"S">,
			::fast_io::mnp::static_arg<"fixed", 7>)};
	if (!equals_expected(formatted_fast_io))
	{
		return 2;
	}

	::std::array<char, 64u> formatted_storage{};
	::fast_io::obuffer_view formatted_output{formatted_storage};
	::fast_io::fmt::print<"<{}>|{}|{}|{}|{}|{}|{fixed}">(
		formatted_output, pack, selected, rejected, width,
		::fast_io::io_null, ::fast_io::mnp::static_arg<"S">,
		::fast_io::mnp::static_arg<"fixed", 7>);
	if (::std::string_view{formatted_storage.data(), formatted_output.size()} !=
		expected_record)
	{
		return 3;
	}

	::std::array<char, 64u> raw_storage{};
	::fast_io::obuffer_view raw_output{raw_storage};
	::fast_io::io::print(
		raw_output, "<", pack, ">|", selected, "|", rejected, "|", width,
		"|", ::fast_io::io_null, "|", ::fast_io::mnp::static_arg<"S">,
		"|", ::fast_io::mnp::static_arg<"fixed", 7>);
	if (::std::string_view{raw_storage.data(), raw_output.size()} !=
		expected_record)
	{
		return 4;
	}

	auto const raw_concat{::fast_io::concat_std(
		"<", pack, ">|", selected, "|", rejected, "|", width, "|",
		::fast_io::io_null, "|", ::fast_io::mnp::static_arg<"S">, "|",
		::fast_io::mnp::static_arg<"fixed", 7>)};
	return equals_expected(raw_concat) ? 0 : 5;
}
