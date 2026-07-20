#include <fast_io.h>
#include <fast_io_format.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace
{

template <typename char_type>
struct literals;

#define FAST_IO_STATIC_RECORD_LITERALS(character_type, prefix)                        \
	template <>                                                                         \
	struct literals<character_type>                                                     \
	{                                                                                   \
		static inline constexpr character_type hello[]{prefix##"hello"};                \
		static inline constexpr character_type world[]{prefix##"world"};                \
		static inline constexpr character_type a[]{prefix##"a"};                        \
		static inline constexpr character_type b[]{prefix##"b"};                        \
		static inline constexpr character_type c[]{prefix##"c"};                        \
		static inline constexpr character_type d[]{prefix##"d"};                        \
		static inline constexpr character_type slash[]{prefix##"/"};                    \
		static inline constexpr character_type runtime[]{prefix##"run"};                \
		static inline constexpr character_type helloworld[]{prefix##"helloworld"};      \
		static inline constexpr character_type abcd[]{prefix##"abcd"};                  \
		static inline constexpr character_type two[]{prefix##"2"};                      \
		static inline constexpr character_type true_text[]{prefix##"true"};             \
		static inline constexpr character_type i_equals[]{prefix##"i="};                 \
		static inline constexpr character_type i_three_point_two[]{prefix##"i=3.2"};      \
		static inline constexpr character_type embedded_nul[]{prefix##"a\0b"};           \
		static inline constexpr character_type long_left[]{                              \
			prefix##"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}; \
		static inline constexpr character_type long_right[]{                             \
			prefix##"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}; \
	};

FAST_IO_STATIC_RECORD_LITERALS(char, )
FAST_IO_STATIC_RECORD_LITERALS(wchar_t, L)
FAST_IO_STATIC_RECORD_LITERALS(char8_t, u8)
FAST_IO_STATIC_RECORD_LITERALS(char16_t, u)
FAST_IO_STATIC_RECORD_LITERALS(char32_t, U)

#undef FAST_IO_STATIC_RECORD_LITERALS

template <typename char_type>
struct capture_state
{
	::std::array<char_type, 256u> bytes{};
	::std::array<char_type const *, 8u> sources{};
	::std::array<::std::size_t, 8u> sizes{};
	::std::size_t size{};
	::std::size_t write_calls{};
	::std::size_t scatter_calls{};
	char_type const *expected_direct_source{};
	bool direct_source_matched{};
};

template <typename char_type>
struct direct_sink
{
	using output_char_type = char_type;
	capture_state<char_type> *state{};
};

template <typename char_type>
inline constexpr direct_sink<char_type> output_stream_ref_define(
	direct_sink<char_type> sink) noexcept
{
	return sink;
}

template <typename char_type>
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char_type, direct_sink<char_type>>) noexcept
{
	return {};
}

template <typename char_type>
inline constexpr ::std::size_t scatter_direct_full_output_coalesce_threshold(
	::fast_io::io_reserve_type_t<char_type, direct_sink<char_type>>) noexcept
{
	return 128u;
}

template <typename char_type>
inline void write_all_overflow_define(
	direct_sink<char_type> sink, char_type const *first,
	char_type const *last) noexcept
{
	auto &state{*sink.state};
	if (state.expected_direct_source != nullptr)
	{
		state.direct_source_matched = first == state.expected_direct_source;
	}
	state.sources[0u] = first;
	state.sizes[0u] = static_cast<::std::size_t>(last - first);
	++state.write_calls;
	for (; first != last; ++first)
	{
		state.bytes[state.size++] = *first;
	}
}

template <typename char_type>
inline void scatter_write_all_overflow_define(
	direct_sink<char_type> sink,
	::fast_io::basic_io_scatter_t<char_type> const *scatters,
	::std::size_t count) noexcept
{
	auto &state{*sink.state};
	++state.scatter_calls;
	for (::std::size_t index{}; index != count; ++index)
	{
		state.sources[index] = scatters[index].base;
		state.sizes[index] = scatters[index].len;
		for (auto first{scatters[index].base},
			  last{first + scatters[index].len};
			 first != last; ++first)
		{
			state.bytes[state.size++] = *first;
		}
	}
}

template <typename char_type>
inline void reset(capture_state<char_type> &state) noexcept
{
	state = {};
}

template <typename char_type, ::std::size_t extent>
[[nodiscard]] inline bool equals(
	capture_state<char_type> const &state,
	char_type const (&expected)[extent]) noexcept
{
	return state.size == extent - 1u &&
		::std::basic_string_view<char_type>{state.bytes.data(), state.size} ==
			::std::basic_string_view<char_type>{expected, extent - 1u};
}

template <typename char_type>
inline void brace_literals(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::print<"{}{}">(sink, literals<char>::hello,
			literals<char>::world);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::print<L"{}{}">(sink, literals<wchar_t>::hello,
			literals<wchar_t>::world);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::print<u8"{}{}">(sink, literals<char8_t>::hello,
			literals<char8_t>::world);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::print<u"{}{}">(sink, literals<char16_t>::hello,
			literals<char16_t>::world);
	else
		::fast_io::fmt::print<U"{}{}">(sink, literals<char32_t>::hello,
			literals<char32_t>::world);
}

template <typename char_type>
inline void brace_single_literal(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::print<"{}">(sink, literals<char>::helloworld);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::print<L"{}">(sink, literals<wchar_t>::helloworld);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::print<u8"{}">(sink, literals<char8_t>::helloworld);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::print<u"{}">(sink, literals<char16_t>::helloworld);
	else
		::fast_io::fmt::print<U"{}">(sink, literals<char32_t>::helloworld);
}

template <typename char_type>
inline void printf_literals(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::printf<"%s%s">(sink, literals<char>::hello,
			literals<char>::world);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::printf<L"%s%s">(sink, literals<wchar_t>::hello,
			literals<wchar_t>::world);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::printf<u8"%s%s">(sink, literals<char8_t>::hello,
			literals<char8_t>::world);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::printf<u"%s%s">(sink, literals<char16_t>::hello,
			literals<char16_t>::world);
	else
		::fast_io::fmt::printf<U"%s%s">(sink, literals<char32_t>::hello,
			literals<char32_t>::world);
}

template <typename char_type>
inline void format_constant_float(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::print<"i={}">(sink, 3.2);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::print<L"i={}">(sink, 3.2);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::print<u8"i={}">(sink, 3.2);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::print<u"i={}">(sink, 3.2);
	else
		::fast_io::fmt::print<U"i={}">(sink, 3.2);
}

template <typename char_type>
inline void printf_embedded_nul(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::printf<"%s">(sink, literals<char>::embedded_nul);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::printf<L"%s">(sink, literals<wchar_t>::embedded_nul);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::printf<u8"%s">(sink, literals<char8_t>::embedded_nul);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::printf<u"%s">(sink, literals<char16_t>::embedded_nul);
	else
		::fast_io::fmt::printf<U"%s">(sink, literals<char32_t>::embedded_nul);
}

template <typename char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void printf_runtime_pointers(direct_sink<char_type> sink,
	char_type const *left, char_type const *right)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::printf<"%s%s">(sink, left, right);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::printf<L"%s%s">(sink, left, right);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::printf<u8"%s%s">(sink, left, right);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::printf<u"%s%s">(sink, left, right);
	else
		::fast_io::fmt::printf<U"%s%s">(sink, left, right);
}

template <typename char_type>
inline void static_reordered_format(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
		::fast_io::fmt::print<"a{1}c{0}">(sink,
			::fast_io::mnp::static_arg<"d">,
			::fast_io::mnp::static_arg<"b">);
	else if constexpr (::std::same_as<char_type, wchar_t>)
		::fast_io::fmt::print<L"a{1}c{0}">(sink,
			::fast_io::mnp::static_arg<L"d">,
			::fast_io::mnp::static_arg<L"b">);
	else if constexpr (::std::same_as<char_type, char8_t>)
		::fast_io::fmt::print<u8"a{1}c{0}">(sink,
			::fast_io::mnp::static_arg<u8"d">,
			::fast_io::mnp::static_arg<u8"b">);
	else if constexpr (::std::same_as<char_type, char16_t>)
		::fast_io::fmt::print<u"a{1}c{0}">(sink,
			::fast_io::mnp::static_arg<u"d">,
			::fast_io::mnp::static_arg<u"b">);
	else
		::fast_io::fmt::print<U"a{1}c{0}">(sink,
			::fast_io::mnp::static_arg<U"d">,
			::fast_io::mnp::static_arg<U"b">);
}

template <typename char_type>
inline void static_plan(direct_sink<char_type> sink)
{
	if constexpr (::std::same_as<char_type, char>)
	{
		constexpr auto plan{::fast_io::make_scatter_plan<char>(
			::fast_io::mnp::scatter_literal<"hello">,
			::fast_io::mnp::scatter_literal<"world">)};
		plan.print(sink);
		assert(sink.state->sources[0u] ==
			decltype(plan)::merged_static_storage.elements);
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		constexpr auto plan{::fast_io::make_scatter_plan<wchar_t>(
			::fast_io::mnp::scatter_literal<L"hello">,
			::fast_io::mnp::scatter_literal<L"world">)};
		plan.print(sink);
		assert(sink.state->sources[0u] ==
			decltype(plan)::merged_static_storage.elements);
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		constexpr auto plan{::fast_io::make_scatter_plan<char8_t>(
			::fast_io::mnp::scatter_literal<u8"hello">,
			::fast_io::mnp::scatter_literal<u8"world">)};
		plan.print(sink);
		assert(sink.state->sources[0u] ==
			decltype(plan)::merged_static_storage.elements);
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		constexpr auto plan{::fast_io::make_scatter_plan<char16_t>(
			::fast_io::mnp::scatter_literal<u"hello">,
			::fast_io::mnp::scatter_literal<u"world">)};
		plan.print(sink);
		assert(sink.state->sources[0u] ==
			decltype(plan)::merged_static_storage.elements);
	}
	else
	{
		constexpr auto plan{::fast_io::make_scatter_plan<char32_t>(
			::fast_io::mnp::scatter_literal<U"hello">,
			::fast_io::mnp::scatter_literal<U"world">)};
		plan.print(sink);
		assert(sink.state->sources[0u] ==
			decltype(plan)::merged_static_storage.elements);
	}
}

template <typename char_type>
inline void test_character_domain()
{
	static_assert(::fast_io::details::decay::
		print_output_retains_static_scatter<direct_sink<char_type>>);
	capture_state<char_type> state{};
	direct_sink<char_type> sink{__builtin_addressof(state)};

	state.expected_direct_source = literals<char_type>::helloworld;
	::fast_io::print(sink, literals<char_type>::helloworld);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		state.direct_source_matched &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	state.expected_direct_source = literals<char_type>::helloworld;
	brace_single_literal(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		state.direct_source_matched &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	::fast_io::print(sink, literals<char_type>::hello,
		literals<char_type>::world);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	brace_literals(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	printf_literals(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	static_reordered_format(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::abcd));

	reset(state);
	static_plan(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	::fast_io::print(sink, 2);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::two));

	reset(state);
	::fast_io::print(sink, ::fast_io::mnp::boolalpha(true));
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::true_text));

	reset(state);
	::fast_io::print(sink, 2.0);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::two));

	reset(state);
	::fast_io::print(sink, literals<char_type>::i_equals, 3.2);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::i_three_point_two));

	reset(state);
	format_constant_float(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::i_three_point_two));

	reset(state);
	printf_embedded_nul(sink);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::a));

	reset(state);
	printf_runtime_pointers(sink, literals<char_type>::hello,
		literals<char_type>::world);
	assert(state.write_calls == 1u && state.scatter_calls == 0u &&
		equals(state, literals<char_type>::helloworld));

	reset(state);
	printf_runtime_pointers(sink, literals<char_type>::long_left,
		literals<char_type>::long_right);
	constexpr ::std::size_t left_size{
		::std::size(literals<char_type>::long_left) - 1u};
	constexpr ::std::size_t right_size{
		::std::size(literals<char_type>::long_right) - 1u};
	assert((state.write_calls == 0u && state.scatter_calls == 1u &&
		state.size == left_size + right_size &&
		::std::basic_string_view<char_type>{state.bytes.data(), left_size} ==
			::std::basic_string_view<char_type>{
				literals<char_type>::long_left, left_size} &&
		::std::basic_string_view<char_type>{
			state.bytes.data() + left_size, right_size} ==
			::std::basic_string_view<char_type>{
				literals<char_type>::long_right, right_size}));
}

} // namespace

int main()
{
	test_character_domain<char>();
	test_character_domain<wchar_t>();
	test_character_domain<char8_t>();
	test_character_domain<char16_t>();
	test_character_domain<char32_t>();
}
