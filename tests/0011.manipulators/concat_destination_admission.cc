#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct string_adapter_only_leaf
{};

struct concat_buffer_only_leaf
{};

struct construct_only_text
{
	::std::string value;
};

struct append_only_text
{
	::std::string value;
};

struct append_adapter_only_leaf
{};

struct incomplete_strlike;

struct wrong_construct_text
{};

inline int strlike_construct_define(
	::fast_io::io_strlike_type_t<char, wrong_construct_text>, char const *, char const *) noexcept
{
	return 0;
}

struct wrong_cursor_text
{};

inline long *strlike_begin(
	::fast_io::io_strlike_type_t<char, wrong_cursor_text>, wrong_cursor_text &) noexcept
{
	return nullptr;
}

inline char *strlike_curr(
	::fast_io::io_strlike_type_t<char, wrong_cursor_text>, wrong_cursor_text &) noexcept
{
	return nullptr;
}

inline char *strlike_end(
	::fast_io::io_strlike_type_t<char, wrong_cursor_text>, wrong_cursor_text &) noexcept
{
	return nullptr;
}

inline void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, wrong_cursor_text>, wrong_cursor_text &, char *) noexcept
{}

inline void strlike_reserve(
	::fast_io::io_strlike_type_t<char, wrong_cursor_text>, wrong_cursor_text &, ::std::size_t) noexcept
{}

struct wrong_effect_text
{
	char storage[4u]{};
};

inline char *strlike_begin(
	::fast_io::io_strlike_type_t<char, wrong_effect_text>, wrong_effect_text &text) noexcept
{
	return text.storage;
}

inline char *strlike_curr(
	::fast_io::io_strlike_type_t<char, wrong_effect_text>, wrong_effect_text &text) noexcept
{
	return text.storage;
}

inline char *strlike_end(
	::fast_io::io_strlike_type_t<char, wrong_effect_text>, wrong_effect_text &text) noexcept
{
	return text.storage + 4u;
}

inline bool strlike_set_curr(
	::fast_io::io_strlike_type_t<char, wrong_effect_text>, wrong_effect_text &, char *) noexcept
{
	return true;
}

inline ::std::size_t strlike_reserve(
	::fast_io::io_strlike_type_t<char, wrong_effect_text>, wrong_effect_text &, ::std::size_t value) noexcept
{
	return value;
}

struct throwing_buffer_text
{
	using value_type = char;
	char storage[16u]{};
	::std::size_t size{};
	bool fail_reserve{};
	bool fail_commit{};
};

struct unrelated_string_adapter
{
	using output_char_type = char;
};

struct wrong_character_string_adapter
{
	using output_char_type = wchar_t;
};

inline constexpr wrong_character_string_adapter output_stream_ref_define(
	wrong_character_string_adapter adapter) noexcept
{
	return adapter;
}

inline constexpr void write_all_overflow_define(
	wrong_character_string_adapter, wchar_t const *, wchar_t const *) noexcept
{}

struct construct_with_wrong_character_ref
{
	using value_type = char;
	::std::string value;
};

inline wrong_character_string_adapter io_strlike_ref(
	::fast_io::io_alias_t, construct_with_wrong_character_ref &) noexcept
{
	return {};
}

inline construct_with_wrong_character_ref strlike_construct_define(
	::fast_io::io_strlike_type_t<char, construct_with_wrong_character_ref>,
	char const *first, char const *last)
{
	return {::std::string(first, last)};
}

inline unrelated_string_adapter io_strlike_ref(
	::fast_io::io_alias_t, throwing_buffer_text &) noexcept
{
	// The name is deliberately present, but the result has no output-stream projection or primitive protocol. Concat
	// must keep this false positive inside capability discovery and select its maintained cursor adapter instead.
	return {};
}

inline constexpr ::std::true_type concat_context_staging_preferred(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>) noexcept
{
	// A cost-policy marker cannot manufacture a missing range constructor. The wrong-result constructor below and the
	// complete cursor protocol jointly prove that context staging remains capability-gated and buffer output wins.
	return {};
}

inline int strlike_construct_define(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, char const *, char const *) noexcept
{
	// A similarly named but wrong-result CPO must not hide the independently complete writable-buffer protocol.
	return 0;
}

inline char *strlike_begin(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, throwing_buffer_text &text) noexcept
{
	return text.storage;
}

inline char *strlike_curr(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, throwing_buffer_text &text) noexcept
{
	return text.storage + text.size;
}

inline char *strlike_end(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, throwing_buffer_text &text) noexcept
{
	return text.storage + 16u;
}

inline void strlike_set_curr(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, throwing_buffer_text &text,
	char *current) noexcept(false)
{
	if (text.fail_commit)
	{
		throw 41;
	}
	text.size = static_cast<::std::size_t>(current - text.storage);
}

inline void strlike_reserve(
	::fast_io::io_strlike_type_t<char, throwing_buffer_text>, throwing_buffer_text &text,
	::std::size_t requested) noexcept(false)
{
	if (text.fail_reserve)
	{
		throw 42;
	}
	if (requested > 16u)
	{
		throw 43;
	}
}

using throwing_buffer_ref =
	::fast_io::io_strlike_reference_wrapper<char, throwing_buffer_text>;

inline construct_only_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, construct_only_text>, char const *first, char const *last)
{
	return {::std::string(first, last)};
}

inline append_only_text strlike_construct_define(
	::fast_io::io_strlike_type_t<char, append_only_text>, char const *first, char const *last)
{
	return {::std::string(first, last)};
}

inline void strlike_append(
	::fast_io::io_strlike_type_t<char, append_only_text>, append_only_text &text,
	char const *first, char const *last)
{
	text.value.append(first, last);
}

inline void strlike_push_back(
	::fast_io::io_strlike_type_t<char, append_only_text>, append_only_text &text, char ch)
{
	text.value.push_back(ch);
}

using append_only_ref = ::fast_io::io_strlike_reference_wrapper<char, append_only_text>;

inline constexpr append_only_ref io_strlike_ref(::fast_io::io_alias_t, append_only_text &text) noexcept
{
	return {__builtin_addressof(text)};
}

inline void print_define(
	::fast_io::io_reserve_type_t<char, append_adapter_only_leaf>, append_only_ref output,
	append_adapter_only_leaf)
{
	output.ptr->value.push_back('A');
}

// This customization is intentionally tied to the real std::string output adapter. A dummy-stream probe must reject
// it, while concat must accept it because its fallback dispatcher invokes precisely this overload. The implementation
// mutates the adapter's referenced destination directly so the test exercises no numeric formatting algorithm.
inline void print_define(
	::fast_io::io_reserve_type_t<char, string_adapter_only_leaf>,
	::fast_io::ostring_ref_std output, string_adapter_only_leaf)
{
	output.ptr->push_back('D');
}

static_assert(!::fast_io::printable<char, string_adapter_only_leaf>);
static_assert(::fast_io::operations::defines::print_freestanding_okay<
			  ::fast_io::ostring_ref_std, string_adapter_only_leaf>);

using concat_buffer_ref = ::fast_io::io_strlike_reference_wrapper<
	char, ::fast_io::details::basic_concat_buffer<char>>;

// This complementary leaf proves the other legal non-buffer strategy. It is accepted only by concat's actual staging
// destination, then copied into the final std::string by range construction. Admission must use the disjunction of
// these two real destinations; a synthetic dummy stream represents neither one.
inline void print_define(
	::fast_io::io_reserve_type_t<char, concat_buffer_only_leaf>,
	concat_buffer_ref output, concat_buffer_only_leaf)
{
	constexpr char text[]{"S"};
	// Overflow hooks require a full put area. Use the ordinary write operation so the staging adapter may consume its
	// current inline capacity before the overflow layer is considered.
	::fast_io::operations::write_all(output, text, text + 1u);
}

static_assert(!::fast_io::printable<char, concat_buffer_only_leaf>);
static_assert(!::fast_io::operations::defines::print_freestanding_okay<
			  ::fast_io::ostring_ref_std, concat_buffer_only_leaf>);
static_assert(::fast_io::operations::defines::print_freestanding_okay<
			  concat_buffer_ref, concat_buffer_only_leaf>);
static_assert(::fast_io::strlike<char, append_only_text>);
static_assert(::fast_io::auxiliary_strlike<char, append_only_text>);
static_assert(!::fast_io::buffer_strlike<char, append_only_text>);
static_assert(::fast_io::operations::defines::print_freestanding_okay<
			  append_only_ref, append_adapter_only_leaf>);

// The concept must reject malformed protocol *results*, not merely missing names, and it must reject an incomplete
// result before a standard construction trait is instantiated. Conversely, throwing cursor/growth CPOs remain valid:
// exception behavior is an independent property which the generic output wrapper propagates conditionally.
static_assert(!::fast_io::strlike<char, incomplete_strlike>);
static_assert(!::fast_io::strlike<char, wrong_construct_text>);
static_assert(!::fast_io::buffer_strlike<char, wrong_cursor_text>);
static_assert(!::fast_io::buffer_strlike<char, wrong_effect_text>);
static_assert(::fast_io::buffer_strlike<char, throwing_buffer_text>);
static_assert(!::fast_io::range_constructible_strlike<char, throwing_buffer_text>);
static_assert(!::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
	false, unrelated_string_adapter, ::std::string_view>);
static_assert(!::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
	true, unrelated_string_adapter>);
static_assert(!::fast_io::details::decay::basic_general_concat_direct_destination_ok<
	false, char, throwing_buffer_text, char const (&)[3]>);
static_assert(::fast_io::strlike<char, construct_with_wrong_character_ref>);
static_assert(!::fast_io::details::decay::basic_general_concat_direct_destination_ok<
	false, char, construct_with_wrong_character_ref>);
static_assert(!noexcept(::fast_io::obuffer_set_curr(
	::std::declval<throwing_buffer_ref>(), static_cast<char *>(nullptr))));
static_assert(!noexcept(::fast_io::obuffer_flush_reserve_define(
	::std::declval<throwing_buffer_ref>(), ::std::size_t{})));
static_assert(!noexcept(::fast_io::output_stream_buffer_flush_define(
	::std::declval<throwing_buffer_ref>())));

template <typename string_type>
void test_checked_staging_destination_when_selected()
{
	if constexpr (!::fast_io::buffer_strlike<char, string_type>)
	{
		// libstdc++ normally exposes std::string as append/construct-only, so checked concat may select its internal
		// staging destination. libc++ exposes a writable put area and correctly discards this non-selected proof.
		assert((::fast_io::basic_general_concat_checked<false, char, string_type>(
					concat_buffer_only_leaf{}) == "S"));
	}
}

} // namespace

int main()
{
	assert(::fast_io::concat_std(string_adapter_only_leaf{}) == "D");
	test_checked_staging_destination_when_selected<::std::string>();
	auto staged{::fast_io::basic_general_concat<false, char, construct_only_text>(
		concat_buffer_only_leaf{})};
	assert(staged.value == "S");
	auto checked_staged{::fast_io::basic_general_concat_checked<false, char, construct_only_text>(
		concat_buffer_only_leaf{})};
	assert(checked_staged.value == "S");
	auto wrong_character_staged{
		::fast_io::basic_general_concat_checked<false, char, construct_with_wrong_character_ref>("CC")};
	assert(wrong_character_staged.value == "CC");
	auto appended{::fast_io::basic_general_concat<false, char, append_only_text>(
		::fast_io::mnp::pack(append_adapter_only_leaf{}, ::std::string_view{"X"}))};
	assert(appended.value == "AX");

	throwing_buffer_text throwing_destination;
	throwing_buffer_ref throwing_ref{__builtin_addressof(throwing_destination)};
	throwing_destination.fail_reserve = true;
	try
	{
		::fast_io::obuffer_flush_reserve_define(throwing_ref, 1u);
		assert(false);
	}
	catch (int value)
	{
		assert(value == 42);
	}
	try
	{
		::fast_io::output_stream_buffer_flush_define(throwing_ref);
		assert(false);
	}
	catch (int value)
	{
		assert(value == 42);
	}
	throwing_destination.fail_reserve = false;
	throwing_destination.fail_commit = true;
	try
	{
		::fast_io::obuffer_set_curr(throwing_ref, throwing_destination.storage);
		assert(false);
	}
	catch (int value)
	{
		assert(value == 41);
	}

	auto exact_throwing_adapter{
		::fast_io::basic_general_concat<false, char, throwing_buffer_text>("OK")};
	assert(::std::string_view(exact_throwing_adapter.storage, exact_throwing_adapter.size) == "OK");
	auto checked_throwing_adapter{
		::fast_io::basic_general_concat_checked<false, char, throwing_buffer_text>("CK")};
	assert(::std::string_view(checked_throwing_adapter.storage, checked_throwing_adapter.size) == "CK");
	auto buffer_only_line{
		::fast_io::basic_general_concat<true, char, throwing_buffer_text>()};
	assert(::std::string_view(buffer_only_line.storage, buffer_only_line.size) == "\n");
	auto checked_buffer_only_line{
		::fast_io::basic_general_concat_checked<true, char, throwing_buffer_text>()};
	assert(::std::string_view(
			   checked_buffer_only_line.storage, checked_buffer_only_line.size) == "\n");

	// Public concat must enter the normalized front door: pack expansion happens before condition selection, so the
	// inactive branch contributes neither output nor capacity and the null leaf disappears from the resulting run.
	auto composition{::fast_io::mnp::pack(
		::std::string_view{"A"},
		::fast_io::mnp::cond(false, ::std::string_view{"inactive"}, ::std::string_view{"B"}),
		::fast_io::io_null,
		::fast_io::mnp::right(::std::string_view{"C"}, 3u, '.'))};
	assert(::fast_io::concat_std(composition) == "AB..C");
	assert(::fast_io::concatln_std(::std::move(composition)) == "AB..C\n");
}
