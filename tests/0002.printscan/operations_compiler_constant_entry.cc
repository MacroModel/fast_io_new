#include <fast_io.h>

#include <cstddef>

namespace
{

int volatile runtime_integer{17};
double volatile runtime_floating{2.5};

/// @brief Verifies that the operations facade preserves a constant mixed scalar record in one output buffer.
template <typename char_type>
constexpr bool constant_record_is_correct()
{
	char_type storage[32u]{};
	::fast_io::basic_obuffer_view<char_type> output{
		storage, storage + 32u};
	::fast_io::operations::print_freestanding<false>(output, 3, 3.14);
	constexpr char expected[]{"33.14"};
	if (output.curr_ptr != storage + 5u)
	{
		return false;
	}
	for (::std::size_t index{}; index != 5u; ++index)
	{
		if (storage[index] != static_cast<char_type>(expected[index]))
		{
			return false;
		}
	}
	return true;
}

// The public operations boundary must retain its constexpr contract for every
// core character domain. Optimizer visibility is checked separately from this
// semantic test because constant evaluation alone cannot prove run-time inlining.
static_assert(constant_record_is_correct<char>());
static_assert(constant_record_is_correct<wchar_t>());
static_assert(constant_record_is_correct<char8_t>());
static_assert(constant_record_is_correct<char16_t>());
static_assert(constant_record_is_correct<char32_t>());

} // namespace

/// @brief Exercises both constant-evaluated and volatile run-time operation sources.
int main()
{
	if (!constant_record_is_correct<char>())
	{
		return 1;
	}

	// Volatile sources are observable and must remain on the ordinary run-time
	// integer/floating path rather than being inspected by the constant gate.
	char storage[32u]{};
	::fast_io::obuffer_view output{storage, storage + 32u};
	::fast_io::operations::print_freestanding<false>(
		output, runtime_integer, runtime_floating);
	constexpr char expected[]{"172.5"};
	if (output.curr_ptr != storage + 5u)
	{
		return 2;
	}
	for (::std::size_t index{}; index != 5u; ++index)
	{
		if (storage[index] != expected[index])
		{
			return 3;
		}
	}
	return 0;
}
