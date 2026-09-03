#include <concepts>
#include <cstddef>

#include <fast_io_dsal/string.h>

namespace fast_io_dynamic_decay_abi_probe
{

struct dynamic_word
{
	char const *data{};
	::std::size_t size{};
};

inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char, dynamic_word>, dynamic_word value) noexcept
{
	return value.size;
}

inline constexpr char *print_reserve_define(
	::fast_io::io_reserve_type_t<char, dynamic_word>, char *destination,
	dynamic_word value) noexcept
{
	for (::std::size_t index{}; index != value.size; ++index)
	{
		destination[index] = value.data[index];
	}
	return destination + value.size;
}

using result_type = ::fast_io::string;
using decay_entry_type = result_type (*)(dynamic_word, dynamic_word);
using planner_entry_type = result_type (*)(dynamic_word &, dynamic_word &);

/*
The normalization entry owns two decayed values, so its function type must
remain value-based and preserve the target ABI's aggregate register/stack
classification. Only the immediately nested planner borrows those already
owned objects. These two assertions make an accidental `Args&...` migration at
the decay boundary a compile-time failure instead of a benchmark observation.
*/
static_assert(::std::same_as<
			  decltype(&::fast_io::details::decay::basic_general_concat_phase1_decay_impl<
					   false, char, result_type, dynamic_word, dynamic_word>),
			  decay_entry_type>);
static_assert(::std::same_as<
			  decltype(&::fast_io::details::decay::basic_general_concat_phase1_decay_ref_impl<
					   false, char, result_type, dynamic_word, dynamic_word>),
			  planner_entry_type>);

} // namespace fast_io_dynamic_decay_abi_probe

extern "C" [[gnu::noinline]] ::std::size_t fast_io_concat_dynamic_decay_abi(
	fast_io_dynamic_decay_abi_probe::dynamic_word first,
	fast_io_dynamic_decay_abi_probe::dynamic_word second)
{
	auto result{::fast_io::concat_fast_io(first, second)};
	return result.size();
}
