#include <fast_io.h>

#include <cstdlib>

namespace
{

struct padded_only_target
{
	::std::size_t observed_padding{};
};

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, padded_only_target>, char const *first,
	char const *last, padded_only_target &) noexcept
{
	return first == last
			   ? ::fast_io::parse_result<char const *>{
					 first, ::fast_io::parse_code::end_of_file}
			   : ::fast_io::parse_result<char const *>{
					 first + 1u, ::fast_io::parse_code::ok};
}

inline constexpr ::fast_io::parse_result<char const *>
scan_contiguous_padding_define(
	::fast_io::io_reserve_type_t<char, padded_only_target>, char const *first,
	char const *last, ::std::size_t padding, padded_only_target &target) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::end_of_file};
	}
	/*
	The test scanner records the physical capability but advances only inside
	[first,last).  This makes protocol selection observable without granting the
	test permission to treat padding as input.
	*/
	target.observed_padding = padding;
	return {first + 1u, ::fast_io::parse_code::ok};
}

inline constexpr padded_only_target &
scan_alias_define(::fast_io::io_alias_t, padded_only_target &target) noexcept
{
	return target;
}

struct padded_context_target
{
	bool padded_called{};
	bool context_called{};
	bool context_eof_called{};
	::std::size_t observed_padding{};
};

struct padded_context_state
{
};

[[maybe_unused]] inline constexpr
::fast_io::io_type_t<padded_context_state>
scan_context_type(
	::fast_io::io_reserve_type_t<char, padded_context_target>) noexcept
{
	return {};
}

inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, padded_context_target>,
	padded_context_state &, char const *first, char const *last,
	padded_context_target &target) noexcept
{
	target.context_called = true;
	return first == last
			   ? ::fast_io::parse_result<char const *>{
					 first, ::fast_io::parse_code::partial}
			   : ::fast_io::parse_result<char const *>{
					 first + 1u, ::fast_io::parse_code::ok};
}

inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, padded_context_target>,
	padded_context_state &, padded_context_target &target) noexcept
{
	target.context_eof_called = true;
	return ::fast_io::parse_code::end_of_file;
}

inline constexpr ::fast_io::parse_result<char const *> scan_contiguous_define(
	::fast_io::io_reserve_type_t<char, padded_context_target>,
	char const *first, char const *last,
	padded_context_target &) noexcept
{
	return first == last
			   ? ::fast_io::parse_result<char const *>{
					 first, ::fast_io::parse_code::end_of_file}
			   : ::fast_io::parse_result<char const *>{
					 first + 1u, ::fast_io::parse_code::ok};
}

inline constexpr ::fast_io::parse_result<char const *>
scan_contiguous_padding_define(
	::fast_io::io_reserve_type_t<char, padded_context_target>,
	char const *first, char const *last, ::std::size_t padding,
	padded_context_target &target) noexcept
{
	target.padded_called = true;
	target.observed_padding = padding;
	return first == last
			   ? ::fast_io::parse_result<char const *>{
					 first, ::fast_io::parse_code::end_of_file}
			   : ::fast_io::parse_result<char const *>{
					 first + 1u, ::fast_io::parse_code::ok};
}

[[maybe_unused]] inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	::fast_io::io_reserve_type_t<char, padded_context_target>) noexcept
{
	return {};
}

inline constexpr padded_context_target &
scan_alias_define(::fast_io::io_alias_t, padded_context_target &target) noexcept
{
	return target;
}

struct wrong_padded_result
{
};

[[maybe_unused]] inline constexpr
::fast_io::parse_result<char *> scan_contiguous_padding_define(
	::fast_io::io_reserve_type_t<char, wrong_padded_result>, char const *, char const *,
	::std::size_t, wrong_padded_result &) noexcept
{
	return {};
}

struct throwing_padding_range
{
};

[[maybe_unused]] inline ::std::size_t
contiguous_range_padding_size(throwing_padding_range const &)
{
	return 0u;
}

struct wrong_padding_range
{
};

[[maybe_unused]] inline constexpr unsigned
contiguous_range_padding_size(wrong_padding_range const &) noexcept
{
	return 0u;
}

static_assert(::fast_io::contiguous_scannable_with_padding<char, padded_only_target>);
static_assert(::fast_io::contiguous_scannable<char, padded_only_target>);
static_assert(!::fast_io::context_scannable<char, padded_only_target>);
static_assert(::fast_io::contiguous_scannable_with_padding<char, padded_context_target>);
static_assert(::fast_io::context_scannable<char, padded_context_target>);
static_assert(::fast_io::contiguous_scannable<char, padded_context_target>);
static_assert(::fast_io::terminal_padding_context_scannable<
			  char, padded_context_target>);
static_assert(!::fast_io::terminal_contiguous_context_scannable<
			  char, padded_context_target>);
static_assert(!::fast_io::contiguous_scannable_with_padding<char, wrong_padded_result>);
static_assert(!::fast_io::contiguous_range_with_padding<::fast_io::basic_ibuffer_view<char>>);
static_assert(!::fast_io::contiguous_range_with_padding<::fast_io::basic_ibuffer_view_ref<char>>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::basic_padded_ibuffer_view<char>>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::basic_padded_ibuffer_view_ref<char>>);
static_assert(sizeof(::fast_io::basic_ibuffer_view<char>) == 3u * sizeof(void *));

using int_scan_type = decltype(
	::fast_io::scan_alias_define(::fast_io::io_alias, ::std::declval<int &>()));
using double_scan_type = decltype(
	::fast_io::scan_alias_define(::fast_io::io_alias, ::std::declval<double &>()));
static_assert(::fast_io::contiguous_scannable_with_padding<char, int_scan_type>);
static_assert(::fast_io::contiguous_scannable_with_padding<char, double_scan_type>);
static_assert(::fast_io::terminal_padding_context_scannable<
			  char, int_scan_type>);
static_assert(!::fast_io::terminal_contiguous_context_scannable<
			  char, int_scan_type>);
static_assert(!::fast_io::terminal_contiguous_context_scannable<
			  char, double_scan_type>);

static_assert(::fast_io::contiguous_range_with_padding<::fast_io::allocation_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::native_file_loader>);
#if defined(_WIN32) || defined(__CYGWIN__)
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::nt_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::zw_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader_9xa>);
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::win32_file_loader_ntw>);
#endif
#if !defined(_WIN32) && !defined(__MSDOS__) && (!defined(__wasm__) || defined(_WASI_EMULATED_MMAN)) && \
	!defined(_PICOLIBC__) && (!defined(__NEWLIB__) || defined(__CYGWIN__))
static_assert(::fast_io::contiguous_range_with_padding<::fast_io::posix_file_loader>);
#endif
static_assert(!::fast_io::contiguous_range_with_padding<throwing_padding_range>);
static_assert(!::fast_io::contiguous_range_with_padding<wrong_padding_range>);

} // namespace

int main()
{
	::fast_io::allocation_file_loader allocation;
	::fast_io::native_file_loader native;
	if (::fast_io::contiguous_range_padding_size(allocation) != 0u ||
		::fast_io::contiguous_range_padding_size(native) != 0u)
	{
		return 1;
	}
	{
		/*
		A default padded view has an equal null cursor pair and P=0.  Zero
		padding grants no terminal-tail capability, so the dispatcher must use
		the marked ordinary terminal CPO without forming a pointer offset.
		*/
		::fast_io::basic_padded_ibuffer_view<char> empty_input;
		padded_context_target empty_target;
		if (::fast_io::io::scan<true>(empty_input, empty_target) ||
			empty_target.padded_called || empty_target.context_called ||
			empty_target.context_eof_called ||
			empty_target.observed_padding != 0u)
		{
			return 2;
		}
	}

	auto memory{static_cast<char *>(::std::malloc(3u))};
	if (memory == nullptr)
	{
		return 3;
	}
	{
		memory[0] = 'x';
		::fast_io::allocation_file_loader padded{
			::fast_io::released_allocation_file_loader_mapping{
				memory, memory + 1u, memory + 3u, -1}};
		if (::fast_io::contiguous_range_padding_size(padded) != 2u ||
			padded.size() != 1u || padded.end() != memory + 1u)
		{
			return 4;
		}
		::fast_io::basic_padded_ibuffer_view<char> input{padded};
		auto input_ref{::fast_io::input_stream_ref_define(input)};
		if (::fast_io::contiguous_range_padding_size(input) != 2u ||
			::fast_io::contiguous_range_padding_size(input_ref) != 2u)
		{
			return 5;
		}
		padded_only_target target;
		if (!::fast_io::io::scan<true>(input, target) ||
			target.observed_padding != 2u ||
			::fast_io::ibuffer_curr(input_ref) != padded.end())
		{
			return 6;
		}
		::fast_io::basic_padded_ibuffer_view<char> hybrid_input{padded};
		padded_context_target hybrid;
		if (!::fast_io::io::scan<true>(hybrid_input, hybrid) ||
			!hybrid.padded_called || hybrid.context_called ||
			hybrid.observed_padding != 2u ||
			hybrid_input.curr_ptr != hybrid_input.end_ptr)
		{
			return 7;
		}
	}

	auto integer_memory{static_cast<char *>(::std::malloc(16u))};
	if (integer_memory == nullptr)
	{
		return 8;
	}
	for (::std::size_t i{}; i != 16u; ++i)
	{
		integer_memory[i] = i < 3u ? static_cast<char>('1' + i) : '9';
	}
	{
		::fast_io::allocation_file_loader padded_integer{
			::fast_io::released_allocation_file_loader_mapping{
				integer_memory, integer_memory + 3u, integer_memory + 16u, -1}};
		::fast_io::basic_padded_ibuffer_view<char> input{padded_integer};
		int value{};
		if (!::fast_io::io::scan<true>(input, value) || value != 123 ||
			input.curr_ptr != input.end_ptr)
		{
			return 9;
		}
	}

	{
		char digit_padding[16]{
			'1', '2', '3', '4', '5', '6', '7', '8',
			'9', '0', '9', '9', '9', '9', '9', '9'};
		char nondigit_padding[16]{
			'1', '2', '3', '4', '5', '6', '7', '8',
			'9', '0', '/', '/', '/', '/', '/', '/'};
		::std::uint_least64_t ordinary_value{};
		::std::uint_least64_t digit_value{};
		::std::uint_least64_t nondigit_value{};
		auto ordinary_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, ordinary_value)};
		auto digit_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, digit_value)};
		auto nondigit_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, nondigit_value)};
		using manipulator_type = decltype(ordinary_manipulator);
		auto const ordinary_result{scan_contiguous_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			digit_padding, digit_padding + 10u, ordinary_manipulator)};
		auto const digit_result{scan_contiguous_padding_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			digit_padding, digit_padding + 10u, 6u,
			digit_manipulator)};
		auto const nondigit_result{scan_contiguous_padding_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			nondigit_padding, nondigit_padding + 10u, 6u,
			nondigit_manipulator)};
		if (ordinary_result.code != digit_result.code ||
			ordinary_result.code != nondigit_result.code ||
			ordinary_result.iter - digit_padding !=
				digit_result.iter - digit_padding ||
			ordinary_result.iter - digit_padding !=
				nondigit_result.iter - nondigit_padding ||
			ordinary_value != digit_value ||
			ordinary_value != nondigit_value)
		{
			return 14;
		}
	}

	auto floating_memory{static_cast<char *>(::std::malloc(32u))};
	if (floating_memory == nullptr)
	{
		return 10;
	}
	constexpr char floating_text[]{"1.25"};
	for (::std::size_t i{}; i != 32u; ++i)
	{
		floating_memory[i] =
			i < sizeof(floating_text) - 1u ? floating_text[i] : '9';
	}
	{
		::fast_io::allocation_file_loader padded_floating{
			::fast_io::released_allocation_file_loader_mapping{
				floating_memory, floating_memory + sizeof(floating_text) - 1u,
				floating_memory + 32u, -1}};
		::fast_io::basic_padded_ibuffer_view<char> input{padded_floating};
		double value{};
		if (!::fast_io::io::scan<true>(input, value) || value != 1.25 ||
			input.curr_ptr != input.end_ptr)
		{
			return 11;
		}
	}

	{
		constexpr ::std::size_t semantic_size{785u};
		char digit_padding[800];
		char nondigit_padding[800];
		for (::std::size_t index{}; index != semantic_size; ++index)
		{
			auto const value{
				index == 0u ? '1' : static_cast<char>('0' + index % 10u)};
			digit_padding[index] = value;
			nondigit_padding[index] = value;
		}
		for (::std::size_t index{semantic_size}; index != 800u; ++index)
		{
			digit_padding[index] = '9';
			nondigit_padding[index] = '/';
		}
		double ordinary_value{};
		double digit_value{};
		double nondigit_value{};
		auto ordinary_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, ordinary_value)};
		auto digit_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, digit_value)};
		auto nondigit_manipulator{
			::fast_io::scan_alias_define(
				::fast_io::io_alias, nondigit_value)};
		using manipulator_type = decltype(ordinary_manipulator);
		auto const ordinary_result{scan_contiguous_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			digit_padding, digit_padding + semantic_size,
			ordinary_manipulator)};
		auto const digit_result{scan_contiguous_padding_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			digit_padding, digit_padding + semantic_size, 15u,
			digit_manipulator)};
		auto const nondigit_result{scan_contiguous_padding_define(
			::fast_io::io_reserve_type<char, manipulator_type>,
			nondigit_padding, nondigit_padding + semantic_size, 15u,
			nondigit_manipulator)};
		auto const ordinary_bits{
			::fast_io::details::get_punned_result(ordinary_value)};
		auto const digit_bits{
			::fast_io::details::get_punned_result(digit_value)};
		auto const nondigit_bits{
			::fast_io::details::get_punned_result(nondigit_value)};
		if (ordinary_result.code != digit_result.code ||
			ordinary_result.code != nondigit_result.code ||
			ordinary_result.iter - digit_padding !=
				digit_result.iter - digit_padding ||
			ordinary_result.iter - digit_padding !=
				nondigit_result.iter - nondigit_padding ||
			ordinary_bits.mantissa != digit_bits.mantissa ||
			ordinary_bits.exponent != digit_bits.exponent ||
			ordinary_bits.sign != digit_bits.sign ||
			ordinary_bits.mantissa != nondigit_bits.mantissa ||
			ordinary_bits.exponent != nondigit_bits.exponent ||
			ordinary_bits.sign != nondigit_bits.sign)
		{
			return 15;
		}
	}

#if defined(__SSE2__) &&                                                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) &&         \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	{
		/*
		All seven semantic lanes are zero while all nine padding lanes are
		nonzero digits.  A 16-byte unmasked load may see every lane, but the
		semantic cap must return exactly end and leave the sticky bit false.
		*/
		char simd_storage[16];
		for (::std::size_t i{}; i != 16u; ++i)
		{
			simd_storage[i] = i < 7u ? '0' : '9';
		}
		bool tail_nonzero{};
		auto const *const result{
			::fast_io::details::scan_decfloat_skip_after_exact_limit_padding_simd<16u>(
				simd_storage, simd_storage + 7u, tail_nonzero, 9u)};
		if (result != simd_storage + 7u || tail_nonzero)
		{
			return 12;
		}
	}
#endif

#if defined(__AVX2__) &&                                                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) &&         \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	{
		/*
		The AVX2 case repeats the proof at 32 bytes and inserts a semantic
		nondigit.  Padding availability permits the load, while the mask must
		return the nondigit position rather than any physical-tail position.
		*/
		char simd_storage[32];
		for (::std::size_t i{}; i != 32u; ++i)
		{
			simd_storage[i] = '9';
		}
		simd_storage[3] = 'x';
		bool tail_nonzero{};
		auto const *const result{
			::fast_io::details::scan_decfloat_skip_after_exact_limit_padding_simd<32u>(
				simd_storage, simd_storage + 5u, tail_nonzero, 27u)};
		if (result != simd_storage + 3u || !tail_nonzero)
		{
			return 13;
		}
	}
#endif
	return 0;
}
