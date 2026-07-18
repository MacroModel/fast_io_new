#include <fast_io_core.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_SCAN_PRFCH_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_SCAN_PRFCH_NOINLINE
#endif

namespace
{

enum class operation
{
	contiguous_read,
	owned_refill
};

enum class cache_mode
{
	hot,
	cold
};

struct options
{
	operation direction{operation::contiguous_read};
	cache_mode cache{cache_mode::hot};
	::std::size_t extent{16u * 1024u};
	::std::size_t distance{1024u};
	::std::size_t target_bytes{256u * 1024u * 1024u};
	::std::size_t cold_bytes{128u * 1024u * 1024u};
	::std::size_t scrub_bytes{96u * 1024u * 1024u};
	::std::size_t samples{9u};
	::std::size_t warmups{2u};
	::std::uint64_t seed{0x9e3779b97f4a7c15ULL};
};

inline void compiler_barrier(void const *address) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "g"(address) : "memory");
#else
	(void)address;
#endif
}

/// Same parser body for both dispatch probes. It intentionally models a branchy integer-token scan without claiming
/// anything about fast_io's integer algorithm: only the one bounded hint before this identical CPO-shaped call differs.
FAST_IO_SCAN_PRFCH_NOINLINE ::std::uint64_t parse_decimal_tokens(
	unsigned char const *first, ::std::size_t size) noexcept
{
	::std::uint64_t checksum{0xcbf29ce484222325ULL};
	::std::uint64_t value{};
	for (::std::size_t index{}; index != size; ++index)
	{
		unsigned char const ch{first[index]};
		unsigned int const digit{static_cast<unsigned int>(ch - static_cast<unsigned char>('0'))};
		if (digit < 10u)
		{
			value = value * 10u + digit;
		}
		else
		{
			checksum ^= value + 0x9e3779b97f4a7c15ULL;
			checksum *= 0x100000001b3ULL;
			value = 0u;
		}
	}
	return checksum ^ value;
}

extern "C" FAST_IO_SCAN_PRFCH_NOINLINE ::std::uint64_t fast_io_scan_prfch_read_baseline(
	unsigned char const *first, ::std::size_t size, ::std::size_t) noexcept
{
	compiler_barrier(first);
	return parse_decimal_tokens(first, size);
}

/// Models the only generic address a contiguous/context dispatcher can prove without entering the scanner algorithm.
/// `distance < size` proves the hinted byte lies inside the live ordinary-memory range; no one-past address is formed.
extern "C" FAST_IO_SCAN_PRFCH_NOINLINE ::std::uint64_t fast_io_scan_prfch_read_candidate(
	unsigned char const *first, ::std::size_t size, ::std::size_t distance) noexcept
{
	compiler_barrier(first);
	if (distance < size)
	{
		::fast_io::prfch<::fast_io::prfch_mode::read, ::fast_io::prfch_level::L1,
						 ::fast_io::prfch_retention::keep>(first + distance);
	}
	return parse_decimal_tokens(first, size);
}

extern "C" FAST_IO_SCAN_PRFCH_NOINLINE ::ssize_t fast_io_scan_prfch_refill_baseline(
	int descriptor, unsigned char *destination, ::std::size_t size, ::std::size_t) noexcept
{
	compiler_barrier(destination);
	return ::read(descriptor, destination, size);
}

/// Models a hint issued only after an owned refill buffer has been allocated and before the underlying read operation.
/// The benchmark deliberately keeps this separate from input consumption: write allocation and producer-read latency
/// are different directions and one result must never authorize the other.
extern "C" FAST_IO_SCAN_PRFCH_NOINLINE ::ssize_t fast_io_scan_prfch_refill_candidate(
	int descriptor, unsigned char *destination, ::std::size_t size, ::std::size_t distance) noexcept
{
	compiler_barrier(destination);
	if (distance < size)
	{
		::fast_io::prfch<::fast_io::prfch_mode::write, ::fast_io::prfch_level::L1,
						 ::fast_io::prfch_retention::keep>(destination + distance);
	}
	return ::read(descriptor, destination, size);
}

[[nodiscard]] bool parse_size(::std::string_view text, ::std::size_t &value) noexcept
{
	auto const result{::std::from_chars(text.data(), text.data() + text.size(), value)};
	return result.ec == ::std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] bool parse_u64(::std::string_view text, ::std::uint64_t &value) noexcept
{
	auto const result{::std::from_chars(text.data(), text.data() + text.size(), value)};
	return result.ec == ::std::errc{} && result.ptr == text.data() + text.size();
}

[[noreturn]] void usage(char const *program)
{
	::std::fprintf(stderr,
				   "usage: %s --operation read|refill --cache hot|cold --extent N --distance N "
				   "[--target-bytes N] [--cold-bytes N] [--scrub-bytes N] [--samples N] [--warmups N] [--seed N]\n",
				   program);
	::std::exit(2);
}

[[nodiscard]] options parse_options(int argc, char **argv)
{
	options result;
	for (int index{1}; index != argc; ++index)
	{
		::std::string_view const key{argv[index]};
		if (index + 1 == argc)
		{
			usage(argv[0]);
		}
		::std::string_view const value{argv[++index]};
		if (key == "--operation")
		{
			if (value == "read")
			{
				result.direction = operation::contiguous_read;
			}
			else if (value == "refill")
			{
				result.direction = operation::owned_refill;
			}
			else
			{
				usage(argv[0]);
			}
		}
		else if (key == "--cache")
		{
			if (value == "hot")
			{
				result.cache = cache_mode::hot;
			}
			else if (value == "cold")
			{
				result.cache = cache_mode::cold;
			}
			else
			{
				usage(argv[0]);
			}
		}
		else if (key == "--extent")
		{
			if (!parse_size(value, result.extent))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--distance")
		{
			if (!parse_size(value, result.distance))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--target-bytes")
		{
			if (!parse_size(value, result.target_bytes))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--cold-bytes")
		{
			if (!parse_size(value, result.cold_bytes))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--scrub-bytes")
		{
			if (!parse_size(value, result.scrub_bytes))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--samples")
		{
			if (!parse_size(value, result.samples))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--warmups")
		{
			if (!parse_size(value, result.warmups))
			{
				usage(argv[0]);
			}
		}
		else if (key == "--seed")
		{
			if (!parse_u64(value, result.seed))
			{
				usage(argv[0]);
			}
		}
		else
		{
			usage(argv[0]);
		}
	}
	if (result.extent == 0u || result.target_bytes == 0u || result.samples == 0u ||
		result.cold_bytes < result.extent ||
		result.extent > static_cast<::std::size_t>((::std::numeric_limits<::ssize_t>::max)()))
	{
		usage(argv[0]);
	}
	return result;
}

class fixture
{
	options config_;
	::std::vector<unsigned char> storage_;
	::std::vector<::std::size_t> order_;
	::std::vector<unsigned char> scrub_;
	::std::size_t operations_{};
	int zero_descriptor_{-1};

	inline void scrub_cache() noexcept
	{
		::std::uint64_t sum{};
		for (::std::size_t index{}; index < scrub_.size(); index += 64u)
		{
			sum += scrub_[index];
			scrub_[index] = static_cast<unsigned char>(sum);
		}
		compiler_barrier(scrub_.data());
	}

	template <typename kernel_type>
	[[nodiscard]] ::std::pair<double, ::std::uint64_t> measure_read(kernel_type kernel)
	{
		scrub_cache();
		::std::uint64_t checksum{};
		auto const begin{::std::chrono::steady_clock::now()};
		for (::std::size_t iteration{}; iteration != operations_; ++iteration)
		{
			auto const slot{order_[iteration % order_.size()]};
			checksum ^= kernel(storage_.data() + slot * config_.extent, config_.extent, config_.distance) + iteration;
		}
		auto const end{::std::chrono::steady_clock::now()};
		compiler_barrier(__builtin_addressof(checksum));
		return {::std::chrono::duration<double, ::std::nano>(end - begin).count(), checksum};
	}

	template <typename kernel_type>
	[[nodiscard]] ::std::pair<double, ::std::uint64_t> measure_refill(kernel_type kernel)
	{
		scrub_cache();
		::std::uint64_t checksum{};
		auto const begin{::std::chrono::steady_clock::now()};
		for (::std::size_t iteration{}; iteration != operations_; ++iteration)
		{
			auto const slot{order_[iteration % order_.size()]};
			auto *const destination{storage_.data() + slot * config_.extent};
			::ssize_t const transferred{kernel(zero_descriptor_, destination, config_.extent, config_.distance)};
			if (transferred != static_cast<::ssize_t>(config_.extent)) [[unlikely]]
			{
				__builtin_trap();
			}
			checksum += destination[0] + destination[config_.extent - 1u] + iteration;
		}
		auto const end{::std::chrono::steady_clock::now()};
		compiler_barrier(__builtin_addressof(checksum));
		return {::std::chrono::duration<double, ::std::nano>(end - begin).count(), checksum};
	}

public:
	explicit fixture(options config) : config_{config}
	{
		::std::size_t const slot_count{config_.cache == cache_mode::hot ? 1u : config_.cold_bytes / config_.extent};
		if (slot_count == 0u || slot_count > (::std::numeric_limits<::std::size_t>::max)() / config_.extent)
		{
			__builtin_trap();
		}
		storage_.resize(slot_count * config_.extent);
		order_.resize(slot_count);
		::std::iota(order_.begin(), order_.end(), ::std::size_t{});
		::std::mt19937_64 generator(config_.seed);
		if (config_.cache == cache_mode::cold)
		{
			::std::shuffle(order_.begin(), order_.end(), generator);
		}
		for (::std::size_t index{}; index != storage_.size(); ++index)
		{
			// Fixed-width decimal tokens keep the grammar stable while the digits vary with the seed.
			storage_[index] = index % 17u == 16u
								  ? static_cast<unsigned char>(',')
								  : static_cast<unsigned char>('0' + generator() % 10u);
		}
		scrub_.resize(config_.scrub_bytes, 1u);
		operations_ = config_.target_bytes / config_.extent;
		if (operations_ == 0u)
		{
			operations_ = 1u;
		}
		if (config_.direction == operation::owned_refill)
		{
			zero_descriptor_ = ::open("/dev/zero", O_RDONLY | O_CLOEXEC);
			if (zero_descriptor_ == -1)
			{
				__builtin_trap();
			}
		}
	}

	fixture(fixture const &) = delete;
	fixture &operator=(fixture const &) = delete;

	~fixture()
	{
		if (zero_descriptor_ != -1)
		{
			::close(zero_descriptor_);
		}
	}

	[[nodiscard]] ::std::pair<double, ::std::uint64_t> baseline()
	{
		if (config_.direction == operation::contiguous_read)
		{
			return measure_read(fast_io_scan_prfch_read_baseline);
		}
		return measure_refill(fast_io_scan_prfch_refill_baseline);
	}

	[[nodiscard]] ::std::pair<double, ::std::uint64_t> candidate()
	{
		if (config_.direction == operation::contiguous_read)
		{
			return measure_read(fast_io_scan_prfch_read_candidate);
		}
		return measure_refill(fast_io_scan_prfch_refill_candidate);
	}
};

[[nodiscard]] double median(::std::vector<double> values)
{
	::std::sort(values.begin(), values.end());
	auto const middle{values.size() / 2u};
	if (values.size() % 2u == 0u)
	{
		return (values[middle - 1u] + values[middle]) * 0.5;
	}
	return values[middle];
}

} // namespace

int main(int argc, char **argv)
{
	options const config{parse_options(argc, argv)};
	fixture state{config};
	for (::std::size_t warmup{}; warmup != config.warmups; ++warmup)
	{
		(void)state.baseline();
		(void)state.candidate();
	}
	::std::vector<double> baseline_samples;
	::std::vector<double> candidate_samples;
	::std::vector<double> paired_ratios;
	::std::uint64_t checksum{};
	baseline_samples.reserve(config.samples);
	candidate_samples.reserve(config.samples);
	paired_ratios.reserve(config.samples);
	for (::std::size_t sample{}; sample != config.samples; ++sample)
	{
		::std::pair<double, ::std::uint64_t> baseline;
		::std::pair<double, ::std::uint64_t> candidate;
		if (sample % 2u == 0u)
		{
			baseline = state.baseline();
			candidate = state.candidate();
		}
		else
		{
			candidate = state.candidate();
			baseline = state.baseline();
		}
		if (baseline.second != candidate.second)
		{
			__builtin_trap();
		}
		checksum = baseline.second;
		baseline_samples.push_back(baseline.first);
		candidate_samples.push_back(candidate.first);
		paired_ratios.push_back(candidate.first / baseline.first);
	}
	::std::printf(
		"operation,cache,extent,distance,seed,baseline_median_ns,candidate_median_ns,candidate_over_baseline,checksum\n"
		"%s,%s,%zu,%zu,%llu,%.3f,%.3f,%.9f,%llu\n",
		config.direction == operation::contiguous_read ? "read" : "refill",
		config.cache == cache_mode::hot ? "hot" : "cold", config.extent, config.distance,
		static_cast<unsigned long long>(config.seed), median(baseline_samples), median(candidate_samples),
		median(paired_ratios), static_cast<unsigned long long>(checksum));
}

#undef FAST_IO_SCAN_PRFCH_NOINLINE
