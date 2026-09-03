#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <vector>

#include <fast_io.h>

#ifndef FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN
#define FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN 2
#endif

#ifndef FAST_IO_PRECISE_POLICY_FUZZ_SHARD
#define FAST_IO_PRECISE_POLICY_FUZZ_SHARD 0
#endif

namespace
{

using ::fast_io::manipulators::floating_format;
using ::fast_io::manipulators::floating_precision;
using ::fast_io::manipulators::floating_rounding;

[[nodiscard]] ::std::uint_least64_t load64(
	unsigned char const *data, ::std::size_t size,
	::std::size_t offset = 0u) noexcept
{
	::std::uint_least64_t value{};
	for (::std::size_t index{}; index != 8u; ++index)
	{
		value |= static_cast<::std::uint_least64_t>(
					 index + offset < size ? data[index + offset] : 0u)
				 << (index * 8u);
	}
	return value;
}

template <floating_format format, floating_rounding rounding,
		  floating_precision precision = floating_precision::significant,
		  bool json = false>
[[nodiscard]] consteval auto make_flags() noexcept
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.floating = format;
	flags.rounding = rounding;
	flags.precision = precision;
	flags.json_float = json;
	return flags;
}

template <typename char_type, typename printable_type>
[[nodiscard]] bool check_character_protocol(
	printable_type const &printable, bool check_to_chars) noexcept
{
	using clean_type = ::std::remove_cvref_t<printable_type>;
	using tag = ::fast_io::io_reserve_type_t<char_type, clean_type>;
	static_assert(::fast_io::precise_reserve_printable<char_type, clean_type>);

	auto const precise_size{print_reserve_precise_size(tag{}, printable)};
	auto const reserve_size{[&]() noexcept {
		if constexpr (requires { print_reserve_size(tag{}, printable); })
		{
			return print_reserve_size(tag{}, printable);
		}
		else
		{
			return print_reserve_size(tag{});
		}
	}()};
	if (reserve_size < precise_size)
	{
		return false;
	}

	::std::vector<char_type> ordinary(reserve_size + 1u,
									  static_cast<char_type>(0x5a));
	auto *const ordinary_end{
		print_reserve_define(tag{}, ordinary.data(), printable)};
	if (ordinary_end != ordinary.data() + precise_size ||
		ordinary[reserve_size] != static_cast<char_type>(0x5a))
	{
		return false;
	}

	::std::vector<char_type> precise(precise_size + 1u,
									 static_cast<char_type>(0x5a));
	auto *const precise_end{print_reserve_precise_define(
		tag{}, precise.data(), precise_size, printable)};
	if (precise_end != precise.data() + precise_size ||
		precise[precise_size] != static_cast<char_type>(0x5a))
	{
		return false;
	}
	for (::std::size_t index{}; index != precise_size; ++index)
	{
		if (ordinary[index] != precise[index])
		{
			return false;
		}
	}

	if constexpr (::std::same_as<char_type, char>)
	{
		if constexpr (requires(char *first, char *last) {
						  ::fast_io::to_chars(first, last, printable);
					  })
		{
			if (check_to_chars)
			{
				::std::vector<char> exact_fit(precise_size + 1u, '!');
				auto const converted{::fast_io::to_chars(
					exact_fit.data(), exact_fit.data() + precise_size, printable)};
				if (converted.ec != ::std::errc{} ||
					converted.ptr != exact_fit.data() + precise_size ||
					exact_fit[precise_size] != '!')
				{
					return false;
				}
				for (::std::size_t index{}; index != precise_size; ++index)
				{
					if (ordinary[index] != exact_fit[index])
					{
						return false;
					}
				}
				if (precise_size != 0u)
				{
					::std::vector<char> rejected(precise_size + 1u, '!');
					auto const result{::fast_io::to_chars(
						rejected.data(), rejected.data() + precise_size - 1u,
						printable)};
					if (result.ec != ::std::errc::value_too_large ||
						result.ptr != rejected.data() + precise_size - 1u)
					{
						return false;
					}
					for (auto const character : rejected)
					{
						if (character != '!')
						{
							return false;
						}
					}
				}
			}
		}
	}
	return true;
}

template <typename printable_type>
[[nodiscard]] bool check_printable(printable_type const &printable,
								   ::std::uint_least64_t) noexcept
{
	/* Precise sizing must agree with the ordinary writer for every supported
	character element type.  Testing a randomly selected one only gives each
	combination a fraction of the campaign; execute the full character matrix
	for every fuzzer input instead. */
	return check_character_protocol<char>(printable, true) &&
		   check_character_protocol<wchar_t>(printable, false) &&
		   check_character_protocol<char8_t>(printable, false) &&
		   check_character_protocol<char16_t>(printable, false) &&
		   check_character_protocol<char32_t>(printable, false);
}

template <floating_format format, floating_rounding rounding, bool json,
		  floating_precision precision, typename floating_type>
[[nodiscard]] bool check_precision(
	floating_type value, ::std::size_t requested_precision,
	::std::uint_least64_t selector) noexcept
{
	constexpr auto flags{make_flags<format, rounding, precision, json>()};
	auto const printable{
		::fast_io::details::make_floating_scalar_manip_precision<flags>(
			value, requested_precision)};
	return check_printable(printable, selector);
}

template <floating_format format, floating_rounding rounding, bool json,
		  floating_precision precision, typename floating_type>
[[nodiscard]] bool check_precision_range(
	floating_type value, ::std::size_t minimum_precision,
	::std::size_t maximum_precision,
	::std::uint_least64_t selector) noexcept
{
	constexpr auto flags{make_flags<format, rounding, precision, json>()};
	auto const scalar{
		::fast_io::details::make_floating_scalar_manip<flags>(value)};
	return check_printable(
		::fast_io::mnp::precision_range(
			scalar, minimum_precision, maximum_precision),
		selector);
}

template <floating_format format, floating_rounding rounding, bool json,
		  typename floating_type>
[[nodiscard]] bool check_deterministic_policy(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	constexpr auto flags{make_flags<format, rounding,
									floating_precision::significant, json>()};
	auto const requested_precision{
		static_cast<::std::size_t>((selector >> 24u) & 63u)};
	(void)flags;
	(void)requested_precision;
#if FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 0
	return check_printable(
		::fast_io::details::make_floating_scalar_manip<flags>(value), selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 1
	return check_precision<format, rounding, json,
						   floating_precision::significant>(
		value, requested_precision, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 2
	return check_precision<format, rounding, json,
						   floating_precision::fractional>(
		value, requested_precision, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 3
	return check_precision<format, rounding, json,
						   floating_precision::significant_preserve_trailing_zero>(
		value, requested_precision, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 4
	return check_precision<format, rounding, json,
						   floating_precision::fractional_preserve_trailing_zero>(
		value, requested_precision, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 5
	auto const minimum{
		static_cast<::std::size_t>((selector >> 32u) & 31u)};
	auto const normalized_minimum{minimum == 0u ? 1u : minimum};
	auto const maximum{normalized_minimum +
					   static_cast<::std::size_t>((selector >> 40u) & 63u)};
	return check_precision_range<format, rounding, json,
								 floating_precision::significant>(
		value, minimum, maximum, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 6
	auto const minimum{
		static_cast<::std::size_t>((selector >> 32u) & 31u)};
	auto const normalized_minimum{minimum == 0u ? 1u : minimum};
	auto const maximum{normalized_minimum +
					   static_cast<::std::size_t>((selector >> 40u) & 63u)};
	return check_precision_range<format, rounding, json,
								 floating_precision::fractional>(
		value, minimum, maximum, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 7
	auto const minimum{
		static_cast<::std::size_t>((selector >> 32u) & 31u)};
	auto const normalized_minimum{minimum == 0u ? 1u : minimum};
	auto const maximum{normalized_minimum +
					   static_cast<::std::size_t>((selector >> 40u) & 63u)};
	return check_precision_range<format, rounding, json,
								 floating_precision::significant_preserve_trailing_zero>(
		value, minimum, maximum, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 8
	auto const minimum{
		static_cast<::std::size_t>((selector >> 32u) & 31u)};
	auto const normalized_minimum{minimum == 0u ? 1u : minimum};
	auto const maximum{normalized_minimum +
					   static_cast<::std::size_t>((selector >> 40u) & 63u)};
	return check_precision_range<format, rounding, json,
								 floating_precision::fractional_preserve_trailing_zero>(
		value, minimum, maximum, selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 9
	return false;
#else
#error FAST_IO_PRECISE_POLICY_FUZZ_SHARD must be in [0,9]
#endif
}

template <floating_format format, bool json, typename floating_type>
[[nodiscard]] bool check_rounding(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	/* Every fuzz input executes every deterministic rounding policy.  Thus a
	one-million-input campaign is one million tests per rounding policy, rather
	than relying on an approximately uniform selector to divide its budget. */
	return check_deterministic_policy<
			   format, floating_rounding::nearest_to_even, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::nearest_to_odd, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::nearest_toward_plus_infinity, json>(
			   value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::nearest_toward_minus_infinity, json>(
			   value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::nearest_toward_zero, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::nearest_away_from_zero, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::toward_plus_infinity, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::toward_minus_infinity, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::toward_zero, json>(value, selector) &&
		   check_deterministic_policy<
			   format, floating_rounding::away_from_zero, json>(value, selector);
}

template <floating_format format, bool json, typename floating_type>
[[nodiscard]] bool check_exact_decimal(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	constexpr auto flags{make_flags<format,
									floating_rounding::nearest_to_even,
									floating_precision::significant, json>()};
	auto const scalar{
		::fast_io::details::make_floating_scalar_manip<flags>(value)};
	return check_printable(
		::fast_io::mnp::exact_decimal(scalar), selector);
}

template <floating_format format, bool json, typename floating_type>
[[nodiscard]] bool check_format_family(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	// Exact decimal is a semantic mode orthogonal to decimal rounding and owns a
	// separate compile shard.  This avoids both misleadingly testing ten
	// identical exact-decimal proxy types and a multi-gigabyte monolithic TU.
#if FAST_IO_PRECISE_POLICY_FUZZ_SHARD == 9
	return check_exact_decimal<format, json>(value, selector);
#else
	return check_rounding<format, json>(value, selector);
#endif
}

template <typename floating_type>
[[nodiscard]] bool check_frontend_matrix(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	/* As with the character matrix, execute all presentation and JSON policies
	per input.  A one-million-input run is therefore a million cases for every
	format/JSON/rounding/character combination, not an expected fraction. */
	return check_format_family<floating_format::fixed, false>(value, selector) &&
		   check_format_family<floating_format::fixed, true>(value, selector) &&
		   check_format_family<floating_format::general, false>(value, selector) &&
		   check_format_family<floating_format::general, true>(value, selector) &&
		   check_format_family<floating_format::scientific, false>(value, selector) &&
		   check_format_family<floating_format::scientific, true>(value, selector) &&
		   check_format_family<floating_format::decimal, false>(value, selector) &&
		   check_format_family<floating_format::decimal, true>(value, selector);
}

template <floating_format format, typename floating_type>
consteval void check_current_environment_is_not_precise() noexcept
{
	constexpr auto scalar_flags{make_flags<format,
										   floating_rounding::current_environment>()};
	using scalar_type = ::fast_io::manipulators::scalar_manip_t<
		scalar_flags, floating_type>;
	static_assert(!::fast_io::precise_reserve_printable<char, scalar_type>);

	constexpr auto precision_flags{make_flags<format,
											  floating_rounding::current_environment,
											  floating_precision::significant>()};
	using precision_type = ::fast_io::manipulators::scalar_manip_precision_t<
		precision_flags, floating_type>;
	static_assert(!::fast_io::precise_reserve_printable<char, precision_type>);

	using range_type = ::fast_io::manipulators::
		floating_scalar_precision_range_manip_t<scalar_flags, floating_type>;
	static_assert(!::fast_io::precise_reserve_printable<char, range_type>);
}

template <typename floating_type>
consteval void check_current_environment_contract() noexcept
{
	check_current_environment_is_not_precise<floating_format::fixed,
											 floating_type>();
	check_current_environment_is_not_precise<floating_format::general,
											 floating_type>();
	check_current_environment_is_not_precise<floating_format::scientific,
											 floating_type>();
	check_current_environment_is_not_precise<floating_format::decimal,
											 floating_type>();
}

template <typename floating_type>
[[nodiscard]] bool deterministic_cartesian_gate(floating_type value) noexcept
{
	check_current_environment_contract<floating_type>();
	// This gate guarantees that fuzz coverage is not relied upon to discover a
	// particular policy tuple.  `check_frontend_matrix` already executes the
	// entire format/JSON/character matrix; random inputs vary the raw value,
	// requested precision and precision interval.
	auto const selector{UINT64_C(3) << 24u | UINT64_C(1) << 32u |
						UINT64_C(16) << 40u};
	return check_frontend_matrix(value, selector);
}

template <typename floating_type>
[[nodiscard]] bool run_selected_value(
	floating_type value, ::std::uint_least64_t selector) noexcept
{
	static bool const gate{deterministic_cartesian_gate(
		static_cast<floating_type>(1.53125))};
	return gate && check_frontend_matrix(value, selector);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(unsigned char const *data,
									  ::std::size_t size)
{
	auto const low{load64(data, size)};
	auto const high{load64(data, size, 8u)};
	auto const selector{load64(data, size, 16u)};
	bool result{};
#if FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 0
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
	result = run_selected_value(
		::std::bit_cast<__bf16>(static_cast<::std::uint_least16_t>(low)),
		selector);
#else
	result = true;
#endif
#elif FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 1
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	result = run_selected_value(
		::std::bit_cast<_Float16>(static_cast<::std::uint_least16_t>(low)),
		selector);
#else
	result = true;
#endif
#elif FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 2
	result = run_selected_value(
		::std::bit_cast<float>(static_cast<::std::uint_least32_t>(low)),
		selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 3
	result = run_selected_value(::std::bit_cast<double>(low), selector);
#elif FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 4
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64 && \
	defined(__LDBL_MAX_EXP__) && __LDBL_MAX_EXP__ == 16384
	using storage_type = ::fast_io::details::float80_storage<
		sizeof(long double) - sizeof(::std::uint_least64_t) -
		sizeof(::std::uint_least16_t)>;
	auto const exponent{
		static_cast<::std::uint_least16_t>((high >> 48u) & 0x7fffu)};
	storage_type storage{};
	storage.mantissa = low & UINT64_C(0x7fffffffffffffff);
	if (exponent != 0u)
	{
		storage.mantissa |= UINT64_C(0x8000000000000000);
	}
	storage.exponent = static_cast<::std::uint_least16_t>(
		exponent | ((high >> 63u) << 15u));
	result = run_selected_value(
		::std::bit_cast<long double>(storage), selector);
#else
	result = true;
#endif
#elif FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN == 5
#if defined(__SIZEOF_FLOAT128__) && __SIZEOF_FLOAT128__ == 16 && \
	defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
	auto const bits{static_cast<__uint128_t>(high) << 64u | low};
	result = run_selected_value(::std::bit_cast<__float128>(bits), selector);
#else
	result = true;
#endif
#else
#error FAST_IO_PRECISE_POLICY_FUZZ_DOMAIN must be in [0,5]
#endif
	if (!result)
	{
		::std::abort();
	}
	return 0;
}
