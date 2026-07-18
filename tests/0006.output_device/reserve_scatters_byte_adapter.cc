#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <fast_io.h>
#include <fast_io_legacy.h>

namespace
{

struct scatter_plan_token
{
	::std::string_view first;
	::std::string_view second;
};

inline ::std::size_t typed_define_calls{};
inline ::std::size_t byte_define_calls{};
inline ::std::size_t wide_typed_define_calls{};
inline ::std::size_t wide_byte_define_calls{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	// The zero reserve extent is intentional: all three descriptors borrow immutable caller-owned character storage.
	return {3u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	scatter_plan_token token) noexcept
{
	++typed_define_calls;
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>, ::fast_io::io_scatter_t *scatters,
	char *reserve, scatter_plan_token token) noexcept
{
	++byte_define_calls;
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, scatter_plan_token>) noexcept
{
	// Every invocation observes stable string literals. Retaining one plan while later tokens are materialized is safe.
	return {};
}

static_assert(::fast_io::reserve_scatters_bytes_printable<char, scatter_plan_token>);
static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char, ::fast_io::parameter<scatter_plan_token &>>);

struct wide_scatter_plan_token
{
	::std::u16string_view first;
	::std::u16string_view second;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char16_t, wide_scatter_plan_token>) noexcept
{
	return {3u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char16_t>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char16_t, wide_scatter_plan_token>,
	::fast_io::basic_io_scatter_t<char16_t> *scatters, char16_t *reserve,
	wide_scatter_plan_token token) noexcept
{
	++wide_typed_define_calls;
	*scatters++ = {token.first.data(), token.first.size()};
	*scatters++ = {u"|", 1u};
	*scatters++ = {token.second.data(), token.second.size()};
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_bytes_define_result<char16_t>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char16_t, wide_scatter_plan_token>,
	::fast_io::io_scatter_t *scatters, char16_t *reserve,
	wide_scatter_plan_token token) noexcept
{
	++wide_byte_define_calls;
	// The native descriptor protocol is byte-oriented even though reserve storage and its cursor remain char16_t.
	*scatters++ = {token.first.data(), token.first.size() * sizeof(char16_t)};
	*scatters++ = {u"|", sizeof(char16_t)};
	*scatters++ = {token.second.data(), token.second.size() * sizeof(char16_t)};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char16_t, wide_scatter_plan_token>) noexcept
{
	return {};
}

static_assert(::fast_io::reserve_scatters_bytes_printable<char16_t, wide_scatter_plan_token>);
static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char16_t, ::fast_io::parameter<wide_scatter_plan_token &>>);

struct native_self_token
{
	char value;
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, native_self_token>) noexcept
{
	return {1u, 0u};
}

inline constexpr ::fast_io::basic_reserve_scatters_define_result<char>
print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, native_self_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	native_self_token const &token) noexcept
{
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, native_self_token>, ::fast_io::io_scatter_t *scatters,
	char *reserve, native_self_token const &token) noexcept
{
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, native_self_token>) noexcept
{
	return {};
}

static_assert(::fast_io::reserve_scatters_bytes_printable<char, native_self_token &>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<char, native_self_token>);
static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char, ::fast_io::parameter<native_self_token &>>);
static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char, ::fast_io::parameter<native_self_token const &>>);

struct tracked_native_storage
{
	char value{};
};

inline tracked_native_storage tracked_native_pool[32u]{};
inline ::std::size_t tracked_native_pool_position{};
inline ::std::size_t tracked_native_typed_calls{};
inline ::std::size_t tracked_native_byte_calls{};

struct fallible_byte_refinement_token
{
	char value;
};

inline ::std::size_t fallible_byte_typed_calls{};
inline ::std::size_t fallible_byte_calls{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, fallible_byte_refinement_token>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, fallible_byte_refinement_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	fallible_byte_refinement_token &token) noexcept
{
	++fallible_byte_typed_calls;
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, fallible_byte_refinement_token>,
	::fast_io::io_scatter_t *scatters, char *reserve,
	fallible_byte_refinement_token &token)
{
	// Absence of noexcept is intentional. A retained multi-producer plan must not move this potential exception past
	// earlier output merely because the destination supports byte descriptors.
	++fallible_byte_calls;
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, fallible_byte_refinement_token>) noexcept
{
	return {};
}

struct typed_only_token
{
	char value;
};

inline ::std::size_t typed_only_calls{};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, typed_only_token>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, typed_only_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	typed_only_token &token) noexcept
{
	++typed_only_calls;
	*scatters++ = {__builtin_addressof(token.value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, typed_only_token>) noexcept
{
	return {};
}

static_assert(::fast_io::reserve_scatters_printable<char, typed_only_token &>);
static_assert(!::fast_io::reserve_scatters_bytes_printable<char, typed_only_token &>);
static_assert(::fast_io::details::decay::
				  print_retained_buffered_reserve_scatters_exact_byte_nothrow_v<
					  char, scatter_plan_token>);
static_assert(!::fast_io::details::decay::
				   print_retained_buffered_reserve_scatters_exact_byte_nothrow_v<
					   char, typed_only_token>);
static_assert(!::fast_io::details::decay::
				   print_retained_buffered_reserve_scatters_exact_byte_nothrow_v<
					   char, fallible_byte_refinement_token>);
static_assert(!::fast_io::details::decay::
				   print_retained_buffered_reserve_scatters_exact_byte_prefix_nothrow<
					   2u, char, scatter_plan_token, typed_only_token>());

inline tracked_native_storage *acquire_tracked_native_storage(char value) noexcept
{
	auto storage{__builtin_addressof(tracked_native_pool[tracked_native_pool_position++])};
	storage->value = value;
	return storage;
}

struct rvalue_tracked_native_token
{
	tracked_native_storage *storage{};
	bool active{};

	explicit rvalue_tracked_native_token(char value) noexcept
		: storage(acquire_tracked_native_storage(value)), active(true)
	{
	}

	rvalue_tracked_native_token(rvalue_tracked_native_token const &other) noexcept
		: storage(acquire_tracked_native_storage(other.storage->value)), active(true)
	{
	}

	rvalue_tracked_native_token(rvalue_tracked_native_token &&other) noexcept
		: storage(other.storage), active(other.active)
	{
		other.active = false;
	}

	~rvalue_tracked_native_token()
	{
		if (active)
		{
			storage->value = '!';
		}
	}
};

inline constexpr ::fast_io::reserve_scatters_size_result print_reserve_scatters_size(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_native_token>) noexcept
{
	return {1u, 0u};
}

inline ::fast_io::basic_reserve_scatters_define_result<char> print_reserve_scatters_define(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_native_token>,
	::fast_io::basic_io_scatter_t<char> *scatters, char *reserve,
	rvalue_tracked_native_token &token) noexcept
{
	++tracked_native_typed_calls;
	*scatters++ = {__builtin_addressof(token.storage->value), 1u};
	return {scatters, reserve};
}

inline ::fast_io::basic_reserve_scatters_bytes_define_result<char>
print_reserve_scatters_bytes_define(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_native_token>,
	::fast_io::io_scatter_t *scatters, char *reserve,
	rvalue_tracked_native_token &token) noexcept
{
	++tracked_native_byte_calls;
	*scatters++ = {__builtin_addressof(token.storage->value), 1u};
	return {scatters, reserve};
}

inline constexpr ::std::true_type print_borrowed_reserve_scatters_source(
	::fast_io::io_reserve_type_t<char, rvalue_tracked_native_token>) noexcept
{
	return {};
}

static_assert(::fast_io::reserve_scatters_bytes_printable<
			  char, rvalue_tracked_native_token &>);
static_assert(::fast_io::borrowed_reserve_scatters_source<
			  char, rvalue_tracked_native_token>);
static_assert(!::fast_io::copy_stable_borrowed_print_source<
			  char, rvalue_tracked_native_token>);

struct byte_capture
{
	using output_char_type = char;
	::std::string *output;
};

inline constexpr byte_capture output_stream_ref_define(byte_capture sink) noexcept
{
	return sink;
}

struct typed_capture
{
	using output_char_type = char;
	::std::string *output;
};

inline constexpr typed_capture output_stream_ref_define(typed_capture sink) noexcept
{
	return sink;
}

inline void write_all_overflow_define(
	typed_capture sink, char const *first, char const *last)
{
	sink.output->append(first, last);
}

inline void scatter_write_all_overflow_define(
	typed_capture sink, ::fast_io::basic_io_scatter_t<char> const *scatters,
	::std::size_t count)
{
	for (::std::size_t i{}; i != count; ++i)
	{
		sink.output->append(scatters[i].base, scatters[i].len);
	}
}

inline void write_all_bytes_overflow_define(
	byte_capture sink, ::std::byte const *first, ::std::byte const *last)
{
	auto const char_first{reinterpret_cast<char const *>(first)};
	auto const char_last{reinterpret_cast<char const *>(last)};
	sink.output->append(char_first, char_last);
}

struct wide_byte_capture
{
	using output_char_type = char16_t;
	::std::string *bytes;
};

inline constexpr wide_byte_capture output_stream_ref_define(wide_byte_capture sink) noexcept
{
	return sink;
}

inline void write_all_bytes_overflow_define(
	wide_byte_capture sink, ::std::byte const *first, ::std::byte const *last)
{
	auto const byte_first{reinterpret_cast<char const *>(first)};
	auto const byte_last{reinterpret_cast<char const *>(last)};
	sink.bytes->append(byte_first, byte_last);
}

struct byte_scatter_only_capture
{
	using output_char_type = char;
	::std::string *output;
	::std::size_t *all_calls;
	::std::size_t *some_calls;
};

inline void scatter_write_all_bytes_overflow_define(
	byte_scatter_only_capture sink, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count)
{
	++*sink.all_calls;
	for (::std::size_t i{}; i != count; ++i)
	{
		auto const first{static_cast<char const *>(scatters[i].base)};
		sink.output->append(first, scatters[i].len);
	}
}

inline ::fast_io::io_scatter_status_t scatter_write_some_bytes_overflow_define(
	byte_scatter_only_capture sink, ::fast_io::io_scatter_t const *scatters,
	::std::size_t count)
{
	++*sink.some_calls;
	for (::std::size_t i{}; i != count; ++i)
	{
		auto const first{static_cast<char const *>(scatters[i].base)};
		sink.output->append(first, scatters[i].len);
	}
	return {count, 0u};
}

inline auto make_plan()
{
	return ::fast_io::mnp::pack(
		scatter_plan_token{"a", "A"}, scatter_plan_token{"b", "B"},
		scatter_plan_token{"c", "C"}, scatter_plan_token{"d", "D"},
		scatter_plan_token{"e", "E"}, scatter_plan_token{"f", "F"},
		scatter_plan_token{"g", "G"}, scatter_plan_token{"h", "H"},
		scatter_plan_token{"i", "I"});
}

inline constexpr ::std::string_view payload{"a|Ab|Bc|Cd|De|Ef|Fg|Gh|Hi|I"};

inline auto make_wide_plan()
{
	return ::fast_io::mnp::pack(
		wide_scatter_plan_token{u"a", u"A"}, wide_scatter_plan_token{u"b", u"B"},
		wide_scatter_plan_token{u"c", u"C"});
}

inline constexpr ::std::u16string_view wide_payload{u"a|Ab|Bc|C"};

[[noreturn]] inline void fail() noexcept
{
	::std::abort();
}

inline void verify_byte_only_grouped_path()
{
	::std::string observed;
	byte_capture sink{__builtin_addressof(observed)};
	auto plan{make_plan()};
	::fast_io::print(sink, plan);
	::fast_io::println(sink, plan);
	::std::string expected{payload};
	expected.append(payload);
	expected.push_back('\n');
	if (observed != expected)
	{
		fail();
	}
}

inline void verify_wide_byte_units_and_forwarding()
{
	::std::string observed_bytes;
	wide_byte_capture sink{__builtin_addressof(observed_bytes)};
	auto plan{make_wide_plan()};
	// Passing an lvalue pack exercises the actual parameter<T&> forwarding route, not only raw-token recognition.
	::fast_io::print(sink, plan);
	::fast_io::println(sink, plan);
	::std::u16string expected{wide_payload};
	expected.append(wide_payload);
	expected.push_back(u'\n');
	auto const expected_first{reinterpret_cast<char const *>(expected.data())};
	::std::string expected_bytes{expected_first, expected.size() * sizeof(char16_t)};
	if (observed_bytes != expected_bytes)
	{
		fail();
	}
}

inline void verify_owned_and_const_native_byte_adapters()
{
	::fast_io::parameter<native_self_token> owned{{'N'}};
	::fast_io::io_scatter_t scatter;
	char unused_reserve{};
	auto result{print_reserve_scatters_bytes_define(
		::fast_io::io_reserve_type<char, decltype(owned)>, __builtin_addressof(scatter),
		__builtin_addressof(unused_reserve), owned)};
	if (result.scatters_pos_ptr != __builtin_addressof(scatter) + 1u ||
		result.reserve_pos_ptr != __builtin_addressof(unused_reserve) ||
		scatter.base != __builtin_addressof(owned.reference.value) || scatter.len != 1u)
	{
		fail();
	}

	::fast_io::parameter<native_self_token> const const_owned{{'C'}};
	result = print_reserve_scatters_bytes_define(
		::fast_io::io_reserve_type<char, ::std::remove_cv_t<decltype(const_owned)>>,
		__builtin_addressof(scatter), __builtin_addressof(unused_reserve), const_owned);
	if (result.scatters_pos_ptr != __builtin_addressof(scatter) + 1u ||
		result.reserve_pos_ptr != __builtin_addressof(unused_reserve) ||
		scatter.base != __builtin_addressof(const_owned.reference.value) || scatter.len != 1u)
	{
		fail();
	}
}

inline void verify_rvalue_owner_outlives_descriptor_builders()
{
	tracked_native_pool_position = 0u;
	tracked_native_byte_calls = 0u;
	::std::string byte_output;
	::fast_io::print(byte_capture{__builtin_addressof(byte_output)},
					 rvalue_tracked_native_token{'X'}, rvalue_tracked_native_token{'Y'});
	if (byte_output != "XY" || tracked_native_byte_calls == 0u)
	{
		fail();
	}

	tracked_native_pool_position = 0u;
	tracked_native_typed_calls = 0u;
	::std::string typed_output;
	::fast_io::print(typed_capture{__builtin_addressof(typed_output)},
					 rvalue_tracked_native_token{'A'}, rvalue_tracked_native_token{'B'});
	if (typed_output != "AB" || tracked_native_typed_calls == 0u)
	{
		fail();
	}
}

template <typename output>
inline void verify_c_file_path(output &out, ::FILE *fp)
{
	auto plan{make_plan()};
	::std::string expected;
	// Repetition makes optimization-sensitive descriptor lifetime and initialization failures deterministic without
	// introducing threads. Alternating print and println also proves that the optional descriptor is accounted for.
	for (::std::size_t i{}; i != 32u; ++i)
	{
		if ((i & 1u) == 0u)
		{
			::fast_io::print(out, plan);
			expected.append(payload);
		}
		else
		{
			::fast_io::println(out, plan);
			expected.append(payload);
			expected.push_back('\n');
		}
	}
	if (::std::fflush(fp) != 0 || ::std::fseek(fp, 0, SEEK_SET) != 0)
	{
		fail();
	}
	::std::string observed(expected.size(), '\0');
	if (::std::fread(observed.data(), 1u, observed.size(), fp) != observed.size() || observed != expected)
	{
		fail();
	}
}

inline ::FILE *open_temporary_file()
{
	::FILE *fp{::std::tmpfile()};
	if (fp == nullptr)
	{
		fail();
	}
	return fp;
}

inline void verify_mixed_prefix_uses_one_typed_representation()
{
	typed_define_calls = 0u;
	byte_define_calls = 0u;
	typed_only_calls = 0u;
	::FILE *fp{open_temporary_file()};
	{
		::fast_io::c_io_observer_unlocked observer{fp};
		auto plan{::fast_io::mnp::pack(
			scatter_plan_token{"a", "A"}, typed_only_token{'b'},
			scatter_plan_token{"c", "C"})};
		::fast_io::print(observer, plan);
	}
	if (::std::fflush(fp) != 0 || ::std::fseek(fp, 0, SEEK_SET) != 0)
	{
		fail();
	}
	char observed[7u]{};
	if (::std::fread(observed, 1u, sizeof(observed), fp) != sizeof(observed) ||
		::std::string_view{observed, sizeof(observed)} != "a|Abc|C" ||
		typed_define_calls != 2u || byte_define_calls != 0u || typed_only_calls != 1u)
	{
		fail();
	}
	::std::fclose(fp);
	// Later exact-byte tests retain independent counter assertions.
	typed_define_calls = 0u;
	byte_define_calls = 0u;
	typed_only_calls = 0u;
}

inline void verify_typed_to_byte_cold_adapter_owns_its_descriptor_type()
{
	constexpr ::std::size_t conversion_capacity{
		::fast_io::details::scatter_byte_conversion_stack_capacity};
	constexpr ::std::size_t descriptor_count{conversion_capacity + 3u};
	::std::array<char, descriptor_count> payload{};
	::std::array<::fast_io::basic_io_scatter_t<char>, descriptor_count> typed_scatters{};
	for (::std::size_t i{}; i != descriptor_count; ++i)
	{
		payload[i] = static_cast<char>('a' + i % 26u);
		typed_scatters[i] = {__builtin_addressof(payload[i]), 1u};
	}
	::std::string expected(payload.begin(), payload.end());

	::std::string all_output;
	::std::size_t all_calls{};
	::std::size_t some_calls{};
	byte_scatter_only_capture all_sink{
		__builtin_addressof(all_output), __builtin_addressof(all_calls),
		__builtin_addressof(some_calls)};
	::fast_io::details::scatter_write_all_impl(
		all_sink, typed_scatters.data(), typed_scatters.size());
	// The extra three descriptors force a second bounded conversion batch. Both calls receive arrays whose actual
	// element type is io_scatter_t; no layout reinterpretation participates in the test path.
	if (all_output != expected || all_calls != 2u || some_calls != 0u)
	{
		fail();
	}

	::std::string some_output;
	all_calls = 0u;
	some_calls = 0u;
	byte_scatter_only_capture some_sink{
		__builtin_addressof(some_output), __builtin_addressof(all_calls),
		__builtin_addressof(some_calls)};
	auto const status{::fast_io::details::scatter_write_some_impl(
		some_sink, typed_scatters.data(), typed_scatters.size())};
	if (status.position != conversion_capacity || status.position_in_scatter != 0u ||
		some_output != expected.substr(0u, conversion_capacity) || all_calls != 0u ||
		some_calls != 1u)
	{
		fail();
	}
}

inline void verify_fallible_byte_refinement_keeps_typed_exception_boundary()
{
	fallible_byte_typed_calls = 0u;
	fallible_byte_calls = 0u;
	::FILE *fp{open_temporary_file()};
	{
		::fast_io::c_io_observer_unlocked observer{fp};
		auto plan{::fast_io::mnp::pack(
			fallible_byte_refinement_token{'T'}, fallible_byte_refinement_token{'U'})};
		::fast_io::print(observer, plan);
	}
	if (::std::fflush(fp) != 0 || ::std::fseek(fp, 0, SEEK_SET) != 0)
	{
		fail();
	}
	char observed[2]{};
	if (::std::fread(observed, 1u, sizeof(observed), fp) != sizeof(observed) ||
		observed[0] != 'T' || observed[1] != 'U' || fallible_byte_typed_calls != 2u ||
		fallible_byte_calls != 0u)
	{
		fail();
	}
	::std::fclose(fp);
}

inline void verify_c_file_families()
{
	{
		::FILE *fp{open_temporary_file()};
		::fast_io::c_file file{fp};
		verify_c_file_path(file, fp);
	}
	{
		::FILE *fp{open_temporary_file()};
		{
			::fast_io::c_io_observer observer{fp};
			verify_c_file_path(observer, fp);
		}
		::std::fclose(fp);
	}
	{
		::FILE *fp{open_temporary_file()};
		::fast_io::c_file_unlocked file{fp};
		verify_c_file_path(file, fp);
	}
	{
		::FILE *fp{open_temporary_file()};
		{
			::fast_io::c_io_observer_unlocked observer{fp};
			verify_c_file_path(observer, fp);
		}
		::std::fclose(fp);
	}
}

} // namespace

int main()
{
	typed_define_calls = 0u;
	byte_define_calls = 0u;
	wide_typed_define_calls = 0u;
	wide_byte_define_calls = 0u;
	verify_mixed_prefix_uses_one_typed_representation();
	verify_typed_to_byte_cold_adapter_owns_its_descriptor_type();
	verify_byte_only_grouped_path();
	verify_wide_byte_units_and_forwarding();
	verify_owned_and_const_native_byte_adapters();
	verify_rvalue_owner_outlives_descriptor_builders();
	verify_fallible_byte_refinement_keeps_typed_exception_boundary();
	verify_c_file_families();
	if (byte_define_calls == 0u || typed_define_calls != 0u || wide_byte_define_calls == 0u ||
		wide_typed_define_calls != 0u)
	{
		fail();
	}
}
