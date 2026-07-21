#include <fast_io_format.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace
{

template <::std::size_t expected_size>
[[nodiscard]] bool matches_output(
	::std::array<char, 64u> const &storage,
	::fast_io::obuffer_view const &output,
	::std::string_view expected) noexcept
{
	if (output.size() != expected_size || expected.size() != expected_size)
	{
		return false;
	}
	for (::std::size_t index{}; index != expected_size; ++index)
	{
		if (storage[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

template <typename range_type>
[[nodiscard]] bool range_matches_exact(
	range_type const &range, ::std::string_view expected) noexcept
{
	if (range.size() != expected.size())
	{
		return false;
	}
	for (::std::size_t index{}; index != expected.size(); ++index)
	{
		if (range.data()[index] != expected[index])
		{
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	::std::array<char, 64u> plain_storage{};
	plain_storage.fill('#');
	::fast_io::obuffer_view plain_output{
		plain_storage.data(), plain_storage.data() + plain_storage.size()};
	::fast_io::fmt::print<"{}">(plain_output, 3.14);

	::std::array<char, 64u> width_storage{};
	width_storage.fill('#');
	::fast_io::obuffer_view width_output{
		width_storage.data(), width_storage.data() + width_storage.size()};
	::fast_io::fmt::print<"[{:8.2f}]">(width_output, 3.2);

	// Keep the source expressions in the API caller. GCC 11--15 evaluate
	// `__builtin_constant_p` after inlining, and a test-only out-of-line helper
	// can make a semantically identical call select a different COMDAT body.
	// Both owning backends consume the same brace-field lowering as print.
	auto standard{::fast_io::fmt::concat_std<"{}">(3.14)};
	auto native{::fast_io::fmt::concat_fast_io<"{}">(3.14)};

	// This test owns semantic lowering only. Reserve writers may initialize
	// unused capacity on compiler versions that do not prove the constant gate;
	// `compiler_constant_exact_obuffer.cc` separately audits exact-store paths.
	return matches_output<4u>(plain_storage, plain_output, "3.14") &&
				   matches_output<10u>(width_storage, width_output, "[    3.20]") &&
				   range_matches_exact(standard, "3.14") &&
				   range_matches_exact(native, "3.14")
			   ? 0
			   : 1;
}
