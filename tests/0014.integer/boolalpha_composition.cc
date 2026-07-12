#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

#include <fast_io_core.h>
#include <fast_io_unit/string.h>

namespace
{

template <bool coalesce>
struct test_sink
{
	using output_char_type = char;
	std::string *output;
	std::size_t *calls;
};

template <bool coalesce>
inline constexpr test_sink<coalesce> output_stream_ref_define(test_sink<coalesce> sink) noexcept
{
	return sink;
}

template <bool coalesce>
inline constexpr std::size_t
full_output_coalesce_threshold(fast_io::io_reserve_type_t<char, test_sink<coalesce>>) noexcept
{
	return coalesce ? 2048u : 0u;
}

template <bool coalesce>
inline void write_all_overflow_define(test_sink<coalesce> sink, char const *first, char const *last)
{
	++*sink.calls;
	sink.output->append(first, last);
}

template <bool coalesce, typename T>
void emit_and_check(T &&value, std::string const &expected)
{
	std::string output;
	std::size_t calls{};
	test_sink<coalesce> sink{&output, &calls};
	fast_io::operations::print_freestanding<false>(sink, std::forward<T>(value));
	assert(output == expected);
	assert(calls == 1u);
}

} // namespace

int main()
{
	auto alpha{fast_io::mnp::boolalpha(true)};
	using alpha_type = decltype(fast_io::io_print_forward<char>(fast_io::io_print_alias(alpha)));
	static_assert(fast_io::reserve_printable<char, alpha_type>);
	static_assert(fast_io::precise_reserve_printable<char, alpha_type>);
	static_assert(fast_io::details::decay::print_semantic_static_bounded_size<char, alpha_type>::available);
	static_assert(fast_io::details::decay::print_semantic_static_bounded_size<char, alpha_type>::size == 5u);

	assert(fast_io::concat_std(fast_io::mnp::boolalpha(true)) == "true");
	assert(fast_io::concat_std(fast_io::mnp::boolalpha(false)) == "false");
	assert(fast_io::concat_std(fast_io::mnp::cond(
			   true, fast_io::mnp::boolalpha(true), fast_io::mnp::boolalpha(false))) == "true");
	assert(fast_io::concat_std(fast_io::mnp::middle(
			   fast_io::mnp::pack(fast_io::mnp::boolalpha(true), "/", fast_io::mnp::boolalpha(false)), 16u,
			   '_')) == "___true/false___");

	emit_and_check<true>(
		fast_io::mnp::pack("id=", 42, ",ok=", fast_io::mnp::boolalpha(true), ",tail=x"),
		"id=42,ok=true,tail=x");
	emit_and_check<false>(fast_io::mnp::middle(
							  fast_io::mnp::pack(fast_io::mnp::boolalpha(false), "/", fast_io::mnp::boolalpha(true)),
							  16u, '_'),
						  "___false/true___");
}
