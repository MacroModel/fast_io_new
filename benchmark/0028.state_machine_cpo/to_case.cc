#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include <fast_io.h>

#include "case_driver.h"
#include "fixture.h"
#include "independent_oracle.h"

#ifndef FAST_IO_STATE_TO_OPERATION
#define FAST_IO_STATE_TO_OPERATION 0
#endif

namespace fast_io_state_machine_cpo
{

inline constexpr unsigned selected_to_operation{FAST_IO_STATE_TO_OPERATION};
static_assert(selected_to_operation <= 3u);

struct to_observation
{
	bool correct{};
	::std::uint_least64_t signature{};
};

template <::std::size_t extent>
[[nodiscard]] inline constexpr auto literal_view(
	char const (&text)[extent]) noexcept
{
	return ::fast_io::mnp::strvw(text, text + extent - 1u);
}

[[nodiscard]] inline constexpr auto record_view(
	text_record const &record) noexcept
{
	return ::fast_io::mnp::strvw(
		record.bytes.data(), record.bytes.data() + record.size);
}

template <unsigned operation, bool validate>
[[nodiscard]] inline to_observation to_selected_once(
	scalar_record const &record)
{
	/*
	`operation` must be a template argument rather than a namespace constant.
	This makes every discarded branch substitution-dependent, so a compiler may
	not instantiate an unavailable old-tree CPO (notably floating scan) while
	building an unrelated integer or string row.  The instantiated expression is
	therefore exactly the front door named by the executable's matrix key.  The
	floating target alias below is itself operation-dependent because Clang is
	permitted to resolve a nondependent template-id while parsing this definition.
	*/
	if constexpr (operation == 0u)
	{
		auto const value{::fast_io::to<::std::int_least64_t>(
			record_view(record.decimal))};
		return {!validate || value == record.decimal_value,
				static_cast<::std::uint_least64_t>(value)};
	}
	else if constexpr (operation == 1u)
	{
		using floating_type =
			::std::conditional_t<operation == 1u, double, void>;
		auto const value{
			::fast_io::to<floating_type>(record_view(record.floating))};
		return {!validate || oracle::same_double(value, record.floating_value),
				oracle::double_bits(value)};
	}
	else if constexpr (operation == 2u)
	{
		auto value{::fast_io::to<::std::string>(record.decimal_value)};
		compiler_observe_bytes(value.data(), value.size());
		if constexpr (validate)
		{
			return {oracle::equal_bytes(
						value.data(), value.size(), record.decimal),
					oracle::digest_bytes(
						UINT64_C(14695981039346656037), value.data(), value.size())};
		}
		else
		{
			return {true, static_cast<::std::uint_least64_t>(value.size())};
		}
	}
	else
	{
		::std::int_least64_t value{};
		::fast_io::inplace_to(value, record_view(record.decimal));
		return {!validate || value == record.decimal_value,
				static_cast<::std::uint_least64_t>(value)};
	}
}

template <typename function_type>
[[nodiscard]] inline bool throws_conversion(function_type &&function)
{
	try
	{
		::std::forward<function_type>(function)();
	}
	catch (...)
	{
		return true;
	}
	return false;
}

template <unsigned operation>
[[nodiscard]] inline bool validate_to_boundaries_and_failures()
{
	/*
	A `to` conversion is a print-to-scan composition.  These checks distinguish
	valid domain boundaries from lexical failure and overflow.  Each translation
	unit instantiates only its selected public front door: this is required for a
	well-formed old/new matrix because the official old tree has no floating scan
	CPO, while its integer and string conversion CPOs remain valid controls.  The
	checks intentionally do not impose a stronger rollback guarantee on a failed
	`inplace_to` target than the public scanner protocol specifies.
	*/
	[[maybe_unused]] static constexpr char signed_minimum[]{
		"-9223372036854775808"};
	[[maybe_unused]] static constexpr char signed_maximum[]{
		"9223372036854775807"};
	[[maybe_unused]] static constexpr char overflow[]{"9223372036854775808"};
	[[maybe_unused]] static constexpr char invalid[]{"x12"};
	[[maybe_unused]] static constexpr char empty[]{""};

	if constexpr (operation == 0u)
	{
		auto const minimum{::fast_io::to<::std::int_least64_t>(
			literal_view(signed_minimum))};
		auto const maximum{::fast_io::to<::std::int_least64_t>(
			literal_view(signed_maximum))};
		if (minimum != (::std::numeric_limits<::std::int_least64_t>::min)() ||
			maximum != (::std::numeric_limits<::std::int_least64_t>::max)())
		{
			return false;
		}
		return throws_conversion([] {
				   (void)::fast_io::to<::std::int_least64_t>(literal_view(invalid));
			   }) &&
			   throws_conversion([] {
				   (void)::fast_io::to<::std::int_least64_t>(literal_view(overflow));
			   }) &&
			   throws_conversion([] {
				   (void)::fast_io::to<::std::int_least64_t>(literal_view(empty));
			   });
	}
	else if constexpr (operation == 1u)
	{
		using floating_type =
			::std::conditional_t<operation == 1u, double, void>;
		auto const positive_quarter{
			::fast_io::to<floating_type>(literal_view("0.25"))};
		auto const negative_zero{
			::fast_io::to<floating_type>(literal_view("-0.00"))};
		return oracle::same_double(positive_quarter, 0.25) &&
			   oracle::same_double(negative_zero, -0.0);
	}
	else if constexpr (operation == 2u)
	{
		auto const minimum{
			(::std::numeric_limits<::std::int_least64_t>::min)()};
		auto const rendered_minimum{::fast_io::to<::std::string>(minimum)};
		return oracle::equal_bytes(
			rendered_minimum.data(), rendered_minimum.size(), signed_minimum,
			sizeof(signed_minimum) - 1u);
	}
	else
	{
		::std::int_least64_t inplaced{};
		::fast_io::inplace_to(inplaced, literal_view(signed_minimum));
		if (inplaced != (::std::numeric_limits<::std::int_least64_t>::min)())
		{
			return false;
		}
		return throws_conversion([] {
				   ::std::int_least64_t value{42};
				   ::fast_io::inplace_to(value, literal_view(invalid));
			   }) &&
			   throws_conversion([] {
				   ::std::int_least64_t value{};
				   ::fast_io::inplace_to(value, literal_view(overflow));
			   }) &&
			   throws_conversion([] {
				   ::std::int_least64_t value{};
				   ::fast_io::inplace_to(value, literal_view(empty));
			   });
	}
}

[[nodiscard]] inline constexpr char const *to_operation_name() noexcept
{
	if constexpr (selected_to_operation == 0u)
	{
		return "text-to-int";
	}
	else if constexpr (selected_to_operation == 1u)
	{
		return "text-to-double";
	}
	else if constexpr (selected_to_operation == 2u)
	{
		return "scalar-to-std-string";
	}
	else
	{
		return "inplace-text-to-int";
	}
}

[[nodiscard]] inline bool validate_to_corpus(
	scalar_corpus const &corpus, ::std::uint_least64_t &digest)
{
	digest = UINT64_C(14695981039346656037);
	for (::std::size_t index{}; index != corpus.size(); ++index)
	{
		auto const result{
			to_selected_once<selected_to_operation, true>(corpus[index])};
		if (!result.correct)
		{
			::std::fprintf(stderr,
						   "to preflight failed: operation=%s record=%zu\n",
						   to_operation_name(), index);
			return false;
		}
		digest = (digest ^ result.signature) * UINT64_C(1099511628211);
	}
	return true;
}

} // namespace fast_io_state_machine_cpo

int main(int argc, char **argv)
{
	::std::uint_least64_t seed{UINT64_C(7640891576956012809)};
	::std::uint_least64_t target_milliseconds{80u};
	if (argc > 3 ||
		(argc >= 2 &&
		 !fast_io_state_machine_cpo::parse_unsigned(argv[1], seed)) ||
		(argc == 3 &&
		 (!fast_io_state_machine_cpo::parse_unsigned(
			  argv[2], target_milliseconds) ||
		  !fast_io_state_machine_cpo::valid_target_milliseconds(
			  target_milliseconds))))
	{
		::std::fputs(
			"usage: to_case [decimal-seed] [target-ms:20..200]\n", stderr);
		return 2;
	}

	fast_io_state_machine_cpo::scalar_corpus corpus;
	fast_io_state_machine_cpo::build_scalar_corpus(corpus, seed);
	::std::uint_least64_t validation_digest{};
	if (!fast_io_state_machine_cpo::oracle::scalar_corpus_is_self_consistent(
			corpus) ||
		!fast_io_state_machine_cpo::validate_to_boundaries_and_failures<
			fast_io_state_machine_cpo::selected_to_operation>() ||
		!fast_io_state_machine_cpo::validate_to_corpus(
			corpus, validation_digest))
	{
		return 1;
	}

	auto timed_call = [&](::std::size_t iteration) -> ::std::uint_least64_t {
		return fast_io_state_machine_cpo::to_selected_once<
				   fast_io_state_machine_cpo::selected_to_operation, false>(
				   corpus[iteration &
						  (fast_io_state_machine_cpo::scalar_corpus_size - 1u)])
			.signature;
	};
	auto const measured{fast_io_state_machine_cpo::calibrate_and_measure(
		timed_call, target_milliseconds)};
	auto const seconds{
		static_cast<double>(measured.elapsed_nanoseconds) / 1.0e9};
	auto const nanoseconds_per_call{
		static_cast<double>(measured.elapsed_nanoseconds) /
		static_cast<double>(measured.iterations)};
	::std::printf(
		"to,%s,%llu,%zu,%.9f,%.3f,%llu,%llu\n",
		fast_io_state_machine_cpo::to_operation_name(),
		static_cast<unsigned long long>(seed), measured.iterations, seconds,
		nanoseconds_per_call,
		static_cast<unsigned long long>(measured.checksum),
		static_cast<unsigned long long>(validation_digest));
}
