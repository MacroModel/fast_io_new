#include <fast_io_core.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <numeric>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#ifndef FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES
#define FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES 256
#endif

static_assert(FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES > 0,
			  "The prefetch policy needs a positive amount of lead work.");

#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_PRFCH_BENCH_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_PRFCH_BENCH_NOINLINE
#endif

namespace
{

struct const_scatter
{
	std::byte const *base{};
	std::size_t len{};
};

struct mutable_scatter
{
	std::byte *base{};
	std::size_t len{};
};

inline void compiler_memory_barrier(void const *address) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	asm volatile("" : : "g"(address) : "memory");
#else
	(void)address;
	std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

} // namespace

/// Baseline for the print/concat-shaped operation: discontinuous sources are
/// concatenated into one already-proved contiguous destination.
extern "C" FAST_IO_PRFCH_BENCH_NOINLINE void
fast_io_prfch_read_baseline(const_scatter const *scatters, std::size_t count,
							std::byte *destination) noexcept
{
	compiler_memory_barrier(scatters);
	for (std::size_t i{}; i != count; ++i)
	{
		auto const current{scatters[i]};
		if (current.len == 0u)
		{
			continue;
		}
		std::memcpy(destination, current.base, current.len);
		destination += current.len;
	}
	compiler_memory_barrier(destination);
}

/// Conservative candidate policy.  The current segment supplies the latency
/// window for the next irregular source; no address beyond the next object's
/// proven base is formed.
extern "C" FAST_IO_PRFCH_BENCH_NOINLINE void
fast_io_prfch_read_next_nonempty(const_scatter const *scatters, std::size_t count,
								 std::byte *destination) noexcept
{
	compiler_memory_barrier(scatters);
	for (std::size_t i{}; i != count; ++i)
	{
		auto const current{scatters[i]};
		if (current.len == 0u)
		{
			continue;
		}

		if (current.len >= FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES)
		{
			std::size_t next{i + 1u};
			while (next != count && scatters[next].len == 0u)
			{
				++next;
			}
			if (next != count)
			{
				::fast_io::prfch<::fast_io::prfch_mode::read, ::fast_io::prfch_level::L1,
								 ::fast_io::prfch_retention::keep>(scatters[next].base);
			}
		}

		std::memcpy(destination, current.base, current.len);
		destination += current.len;
	}
	compiler_memory_barrier(destination);
}

/// Mirrored scan/readv-shaped baseline: one contiguous source is distributed
/// into a chain of writable destinations.
extern "C" FAST_IO_PRFCH_BENCH_NOINLINE void
fast_io_prfch_write_baseline(std::byte const *source, mutable_scatter const *scatters,
							 std::size_t count) noexcept
{
	compiler_memory_barrier(source);
	for (std::size_t i{}; i != count; ++i)
	{
		auto const current{scatters[i]};
		if (current.len == 0u)
		{
			continue;
		}
		std::memcpy(current.base, source, current.len);
		source += current.len;
	}
	compiler_memory_barrier(scatters);
}

/// Mirrored candidate policy.  Read and write eligibility remain separate in
/// fast_io because targets may lower or value these hints differently.
extern "C" FAST_IO_PRFCH_BENCH_NOINLINE void
fast_io_prfch_write_next_nonempty(std::byte const *source, mutable_scatter const *scatters,
								  std::size_t count) noexcept
{
	compiler_memory_barrier(source);
	for (std::size_t i{}; i != count; ++i)
	{
		auto const current{scatters[i]};
		if (current.len == 0u)
		{
			continue;
		}

		if (current.len >= FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES)
		{
			std::size_t next{i + 1u};
			while (next != count && scatters[next].len == 0u)
			{
				++next;
			}
			if (next != count)
			{
				::fast_io::prfch<::fast_io::prfch_mode::write, ::fast_io::prfch_level::L1,
								 ::fast_io::prfch_retention::keep>(scatters[next].base);
			}
		}

		std::memcpy(current.base, source, current.len);
		source += current.len;
	}
	compiler_memory_barrier(scatters);
}

namespace
{

constexpr std::size_t allocation_alignment{64u};
constexpr std::size_t discontinuous_gap{4096u};
constexpr std::uint64_t fnv_offset_basis{14695981039346656037ULL};
constexpr std::uint64_t fnv_prime{1099511628211ULL};

enum class direction
{
	read,
	write
};

enum class layout
{
	contiguous,
	discontinuous
};

enum class cache_condition
{
	hot,
	cold
};

struct options
{
	direction operation{direction::read};
	layout memory_layout{layout::contiguous};
	cache_condition cache{cache_condition::hot};
	std::size_t descriptor_count{32u};
	std::size_t payload_size{256u};
	std::size_t iterations{1000u};
	std::size_t samples{9u};
	std::size_t warmups{2u};
	std::size_t cold_bytes{64u * 1024u * 1024u};
	std::uint64_t seed{11400714819323198485ULL};
};

class aligned_buffer
{
	void *storage_{};
	std::size_t size_{};

	void release() noexcept
	{
		if (storage_ != nullptr)
		{
			::operator delete(storage_, std::align_val_t{allocation_alignment});
			storage_ = nullptr;
			size_ = 0u;
		}
	}

public:
	aligned_buffer() = default;

	explicit aligned_buffer(std::size_t size) : storage_{size == 0u ? nullptr : ::operator new(size, std::align_val_t{allocation_alignment})}, size_{size}
	{}

	aligned_buffer(aligned_buffer const &) = delete;
	aligned_buffer &operator=(aligned_buffer const &) = delete;

	aligned_buffer(aligned_buffer &&other) noexcept : storage_{other.storage_}, size_{other.size_}
	{
		other.storage_ = nullptr;
		other.size_ = 0u;
	}

	aligned_buffer &operator=(aligned_buffer &&other) noexcept
	{
		if (this != &other)
		{
			release();
			storage_ = other.storage_;
			size_ = other.size_;
			other.storage_ = nullptr;
			other.size_ = 0u;
		}
		return *this;
	}

	~aligned_buffer()
	{
		release();
	}

	[[nodiscard]] std::byte *data() noexcept
	{
		return static_cast<std::byte *>(storage_);
	}

	[[nodiscard]] std::byte const *data() const noexcept
	{
		return static_cast<std::byte const *>(storage_);
	}

	[[nodiscard]] std::size_t size() const noexcept
	{
		return size_;
	}
};

[[nodiscard]] constexpr bool add_overflow(std::size_t left, std::size_t right,
										  std::size_t &result) noexcept
{
	if (right > std::numeric_limits<std::size_t>::max() - left)
	{
		return true;
	}
	result = left + right;
	return false;
}

[[nodiscard]] constexpr bool multiply_overflow(std::size_t left, std::size_t right,
											   std::size_t &result) noexcept
{
	if (left != 0u && right > std::numeric_limits<std::size_t>::max() / left)
	{
		return true;
	}
	result = left * right;
	return false;
}

[[nodiscard]] constexpr std::size_t round_up_cacheline(std::size_t value) noexcept
{
	return (value + (allocation_alignment - 1u)) & ~(allocation_alignment - 1u);
}

[[nodiscard]] constexpr std::byte pattern_byte(std::size_t descriptor_index,
											   std::size_t byte_index) noexcept
{
	auto const value{descriptor_index * 131u + byte_index * 17u + (descriptor_index >> 3u)};
	return static_cast<std::byte>(value & 0xffu);
}

[[nodiscard]] std::uint64_t checksum_bytes(std::byte const *first, std::size_t size,
										   std::uint64_t value = fnv_offset_basis) noexcept
{
	for (std::size_t i{}; i != size; ++i)
	{
		value ^= static_cast<unsigned char>(first[i]);
		value *= fnv_prime;
	}
	return value;
}

class scatter_fixture
{
	options const &config_;
	std::size_t total_bytes_{};
	std::size_t stride_{};
	aligned_buffer scatter_storage_;
	aligned_buffer linear_storage_;
	std::vector<const_scatter> read_scatters_;
	std::vector<mutable_scatter> write_scatters_;
	std::uint64_t expected_checksum_{fnv_offset_basis};

	[[nodiscard]] static std::size_t checked_total_bytes(options const &config)
	{
		std::size_t result{};
		if (multiply_overflow(config.descriptor_count, config.payload_size, result))
		{
			throw std::bad_array_new_length{};
		}
		return result;
	}

	[[nodiscard]] static std::size_t checked_stride(options const &config)
	{
		if (config.memory_layout == layout::contiguous)
		{
			return config.payload_size;
		}
		std::size_t padded{};
		if (add_overflow(config.payload_size, discontinuous_gap, padded) ||
			padded > std::numeric_limits<std::size_t>::max() - (allocation_alignment - 1u))
		{
			throw std::bad_array_new_length{};
		}
		return round_up_cacheline(padded);
	}

	[[nodiscard]] static std::size_t checked_storage_bytes(options const &config, std::size_t stride)
	{
		std::size_t result{};
		if (multiply_overflow(config.descriptor_count, stride, result))
		{
			throw std::bad_array_new_length{};
		}
		return result;
	}

public:
	explicit scatter_fixture(options const &config) : config_{config}, total_bytes_{checked_total_bytes(config)}, stride_{checked_stride(config)},
													  scatter_storage_{checked_storage_bytes(config, stride_)}, linear_storage_{total_bytes_}
	{
		std::vector<std::size_t> slots(config.descriptor_count);
		std::iota(slots.begin(), slots.end(), std::size_t{});
		if (config.memory_layout == layout::discontinuous)
		{
			std::mt19937_64 generator{config.seed};
			std::shuffle(slots.begin(), slots.end(), generator);
		}

		if (config.operation == direction::read)
		{
			read_scatters_.reserve(config.descriptor_count);
			for (std::size_t i{}; i != config.descriptor_count; ++i)
			{
				auto *base{scatter_storage_.data() + slots[i] * stride_};
				for (std::size_t j{}; j != config.payload_size; ++j)
				{
					base[j] = pattern_byte(i, j);
				}
				read_scatters_.push_back({base, config.payload_size});
				expected_checksum_ = checksum_bytes(base, config.payload_size, expected_checksum_);
			}
			std::memset(linear_storage_.data(), 0, total_bytes_);
		}
		else
		{
			write_scatters_.reserve(config.descriptor_count);
			auto *source{linear_storage_.data()};
			for (std::size_t i{}; i != config.descriptor_count; ++i)
			{
				auto *base{scatter_storage_.data() + slots[i] * stride_};
				std::memset(base, 0, config.payload_size);
				write_scatters_.push_back({base, config.payload_size});
				for (std::size_t j{}; j != config.payload_size; ++j)
				{
					source[i * config.payload_size + j] = pattern_byte(i, j);
				}
				expected_checksum_ =
					checksum_bytes(source + i * config.payload_size, config.payload_size, expected_checksum_);
			}
		}
	}

	[[nodiscard]] std::size_t total_bytes() const noexcept
	{
		return total_bytes_;
	}

	void run_baseline() noexcept
	{
		if (config_.operation == direction::read)
		{
			fast_io_prfch_read_baseline(read_scatters_.data(), read_scatters_.size(), linear_storage_.data());
		}
		else
		{
			fast_io_prfch_write_baseline(linear_storage_.data(), write_scatters_.data(),
										 write_scatters_.size());
		}
	}

	void run_prefetch() noexcept
	{
		if (config_.operation == direction::read)
		{
			fast_io_prfch_read_next_nonempty(read_scatters_.data(), read_scatters_.size(),
											 linear_storage_.data());
		}
		else
		{
			fast_io_prfch_write_next_nonempty(linear_storage_.data(), write_scatters_.data(),
											  write_scatters_.size());
		}
	}

	[[nodiscard]] bool verify() const noexcept
	{
		if (config_.operation == direction::read)
		{
			auto const *output{linear_storage_.data()};
			for (std::size_t i{}; i != config_.descriptor_count; ++i)
			{
				for (std::size_t j{}; j != config_.payload_size; ++j)
				{
					if (output[i * config_.payload_size + j] != pattern_byte(i, j))
					{
						return false;
					}
				}
			}
		}
		else
		{
			for (std::size_t i{}; i != config_.descriptor_count; ++i)
			{
				for (std::size_t j{}; j != config_.payload_size; ++j)
				{
					if (write_scatters_[i].base[j] != pattern_byte(i, j))
					{
						return false;
					}
				}
			}
		}
		return checksum() == expected_checksum_;
	}

	[[nodiscard]] std::uint64_t checksum() const noexcept
	{
		if (config_.operation == direction::read)
		{
			return checksum_bytes(linear_storage_.data(), total_bytes_);
		}
		std::uint64_t value{fnv_offset_basis};
		for (auto const scatter : write_scatters_)
		{
			value = checksum_bytes(scatter.base, scatter.len, value);
		}
		return value;
	}
};

std::uint64_t volatile cache_scrub_sink{};

class cache_scrubber
{
	aligned_buffer storage_;

public:
	explicit cache_scrubber(std::size_t size) : storage_{size}
	{
		for (std::size_t i{}; i != size; ++i)
		{
			storage_.data()[i] = static_cast<std::byte>((i * 29u + 17u) & 0xffu);
		}
	}

	void scrub() const noexcept
	{
		auto const *bytes{reinterpret_cast<unsigned char const volatile *>(storage_.data())};
		std::uint64_t value{};
		for (std::size_t i{}; i < storage_.size(); i += allocation_alignment)
		{
			value += bytes[i];
		}
		cache_scrub_sink = value;
		compiler_memory_barrier(storage_.data());
	}
};

[[nodiscard]] bool sparse_chain_preflight() noexcept
{
	alignas(allocation_alignment) std::array<std::byte, 512u> source{};
	alignas(allocation_alignment) std::array<std::byte, 512u> baseline_output{};
	alignas(allocation_alignment) std::array<std::byte, 512u> prefetch_output{};
	for (std::size_t i{}; i != source.size(); ++i)
	{
		source[i] = static_cast<std::byte>((i * 37u + 11u) & 0xffu);
	}
	std::array<const_scatter, 3u> read_chain{{{source.data(), 256u}, {nullptr, 0u}, {source.data() + 256u, 256u}}};
	fast_io_prfch_read_baseline(read_chain.data(), read_chain.size(), baseline_output.data());
	fast_io_prfch_read_next_nonempty(read_chain.data(), read_chain.size(), prefetch_output.data());
	if (baseline_output != prefetch_output || baseline_output != source)
	{
		return false;
	}

	alignas(allocation_alignment) std::array<std::byte, 256u> baseline_first{};
	alignas(allocation_alignment) std::array<std::byte, 256u> baseline_second{};
	alignas(allocation_alignment) std::array<std::byte, 256u> prefetch_first{};
	alignas(allocation_alignment) std::array<std::byte, 256u> prefetch_second{};
	std::array<mutable_scatter, 3u> baseline_chain{{{baseline_first.data(), baseline_first.size()},
													{nullptr, 0u},
													{baseline_second.data(), baseline_second.size()}}};
	std::array<mutable_scatter, 3u> prefetch_chain{{{prefetch_first.data(), prefetch_first.size()},
													{nullptr, 0u},
													{prefetch_second.data(), prefetch_second.size()}}};
	fast_io_prfch_write_baseline(source.data(), baseline_chain.data(), baseline_chain.size());
	fast_io_prfch_write_next_nonempty(source.data(), prefetch_chain.data(), prefetch_chain.size());
	return baseline_first == prefetch_first && baseline_second == prefetch_second &&
		   std::memcmp(baseline_first.data(), source.data(), baseline_first.size()) == 0 &&
		   std::memcmp(baseline_second.data(), source.data() + baseline_first.size(), baseline_second.size()) == 0;
}

using clock_type = std::chrono::steady_clock;

template <typename operation_type>
[[nodiscard]] double measure(operation_type operation, options const &config,
							 cache_scrubber const *scrubber)
{
	if (config.cache == cache_condition::hot)
	{
		auto const begin{clock_type::now()};
		for (std::size_t i{}; i != config.iterations; ++i)
		{
			operation();
		}
		auto const end{clock_type::now()};
		auto const elapsed{std::chrono::duration<double, std::nano>(end - begin).count()};
		return elapsed / static_cast<double>(config.iterations);
	}

	double elapsed{};
	for (std::size_t i{}; i != config.iterations; ++i)
	{
		scrubber->scrub();
		auto const begin{clock_type::now()};
		operation();
		auto const end{clock_type::now()};
		elapsed += std::chrono::duration<double, std::nano>(end - begin).count();
	}
	return elapsed / static_cast<double>(config.iterations);
}

[[nodiscard]] double median(std::vector<double> values)
{
	std::sort(values.begin(), values.end());
	auto const middle{values.size() / 2u};
	if ((values.size() & 1u) != 0u)
	{
		return values[middle];
	}
	return (values[middle - 1u] + values[middle]) * 0.5;
}

[[nodiscard]] bool parse_size(std::string_view text, std::size_t &value) noexcept
{
	std::size_t parsed{};
	auto const result{std::from_chars(text.data(), text.data() + text.size(), parsed)};
	if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
	{
		return false;
	}
	value = parsed;
	return true;
}

[[nodiscard]] bool parse_u64(std::string_view text, std::uint64_t &value) noexcept
{
	std::uint64_t parsed{};
	auto const result{std::from_chars(text.data(), text.data() + text.size(), parsed)};
	if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
	{
		return false;
	}
	value = parsed;
	return true;
}

void print_usage(char const *program) noexcept
{
	std::fprintf(stderr,
				 "usage: %s [--header] --direction read|write --layout contiguous|discontinuous "
				 "--cache hot|cold --descriptors N --payload N --iterations N --samples N "
				 "--warmups N --cold-bytes N --seed N\n",
				 program);
}

enum class parse_result
{
	ok,
	header,
	help,
	error
};

[[nodiscard]] parse_result parse_arguments(int argc, char **argv, options &config)
{
	for (int i{1}; i != argc; ++i)
	{
		std::string_view argument{argv[i]};
		if (argument == "--header")
		{
			return parse_result::header;
		}
		if (argument == "--help")
		{
			return parse_result::help;
		}
		if (i + 1 == argc)
		{
			return parse_result::error;
		}
		std::string_view value{argv[++i]};
		if (argument == "--direction")
		{
			if (value == "read")
			{
				config.operation = direction::read;
			}
			else if (value == "write")
			{
				config.operation = direction::write;
			}
			else
			{
				return parse_result::error;
			}
		}
		else if (argument == "--layout")
		{
			if (value == "contiguous")
			{
				config.memory_layout = layout::contiguous;
			}
			else if (value == "discontinuous")
			{
				config.memory_layout = layout::discontinuous;
			}
			else
			{
				return parse_result::error;
			}
		}
		else if (argument == "--cache")
		{
			if (value == "hot")
			{
				config.cache = cache_condition::hot;
			}
			else if (value == "cold")
			{
				config.cache = cache_condition::cold;
			}
			else
			{
				return parse_result::error;
			}
		}
		else if (argument == "--descriptors")
		{
			if (!parse_size(value, config.descriptor_count))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--payload")
		{
			if (!parse_size(value, config.payload_size))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--iterations")
		{
			if (!parse_size(value, config.iterations))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--samples")
		{
			if (!parse_size(value, config.samples))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--warmups")
		{
			if (!parse_size(value, config.warmups))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--cold-bytes")
		{
			if (!parse_size(value, config.cold_bytes))
			{
				return parse_result::error;
			}
		}
		else if (argument == "--seed")
		{
			if (!parse_u64(value, config.seed))
			{
				return parse_result::error;
			}
		}
		else
		{
			return parse_result::error;
		}
	}

	if (config.descriptor_count == 0u || config.payload_size == 0u || config.iterations == 0u ||
		config.samples == 0u || (config.cache == cache_condition::cold && config.cold_bytes == 0u))
	{
		return parse_result::error;
	}
	return parse_result::ok;
}

[[nodiscard]] char const *name(direction value) noexcept
{
	return value == direction::read ? "read" : "write";
}

[[nodiscard]] char const *name(layout value) noexcept
{
	return value == layout::contiguous ? "contiguous" : "discontinuous";
}

[[nodiscard]] char const *name(cache_condition value) noexcept
{
	return value == cache_condition::hot ? "hot" : "cold";
}

void print_header() noexcept
{
	std::puts("direction,layout,cache,descriptors,payload_bytes,total_bytes,iterations,samples,"
			  "min_current_bytes,baseline_median_ns,prefetch_median_ns,prefetch_over_baseline,"
			  "baseline_gib_s,prefetch_gib_s,checksum");
}

} // namespace

int main(int argc, char **argv)
{
	try
	{
		options config;
		auto const parsed{parse_arguments(argc, argv, config)};
		if (parsed == parse_result::header)
		{
			print_header();
			return 0;
		}
		if (parsed == parse_result::help)
		{
			print_usage(argv[0]);
			return 0;
		}
		if (parsed == parse_result::error)
		{
			print_usage(argv[0]);
			return 2;
		}
		if (!sparse_chain_preflight())
		{
			std::fputs("sparse-chain correctness preflight failed\n", stderr);
			return 3;
		}

		scatter_fixture fixture{config};
		cache_scrubber scrubber{config.cache == cache_condition::cold ? config.cold_bytes : 0u};
		auto *scrubber_pointer{config.cache == cache_condition::cold ? &scrubber : nullptr};

		for (std::size_t i{}; i != config.warmups; ++i)
		{
			if (scrubber_pointer != nullptr)
			{
				scrubber_pointer->scrub();
			}
			fixture.run_baseline();
			if (scrubber_pointer != nullptr)
			{
				scrubber_pointer->scrub();
			}
			fixture.run_prefetch();
		}
		if (!fixture.verify())
		{
			std::fputs("fixture correctness preflight failed\n", stderr);
			return 3;
		}

		std::vector<double> baseline_samples;
		std::vector<double> prefetch_samples;
		std::vector<double> paired_ratios;
		baseline_samples.reserve(config.samples);
		prefetch_samples.reserve(config.samples);
		paired_ratios.reserve(config.samples);

		for (std::size_t sample{}; sample != config.samples; ++sample)
		{
			double baseline{};
			double prefetch{};
			if ((sample & 1u) == 0u)
			{
				baseline = measure([&fixture] { fixture.run_baseline(); }, config, scrubber_pointer);
				prefetch = measure([&fixture] { fixture.run_prefetch(); }, config, scrubber_pointer);
			}
			else
			{
				prefetch = measure([&fixture] { fixture.run_prefetch(); }, config, scrubber_pointer);
				baseline = measure([&fixture] { fixture.run_baseline(); }, config, scrubber_pointer);
			}
			if (!fixture.verify())
			{
				std::fputs("timed output verification failed\n", stderr);
				return 3;
			}
			baseline_samples.push_back(baseline);
			prefetch_samples.push_back(prefetch);
			paired_ratios.push_back(prefetch / baseline);
		}

		auto const baseline_median{median(std::move(baseline_samples))};
		auto const prefetch_median{median(std::move(prefetch_samples))};
		auto const ratio_median{median(std::move(paired_ratios))};
		auto const gib_scale{1.0e9 / static_cast<double>(std::uint64_t{1} << 30u)};
		auto const bytes{static_cast<double>(fixture.total_bytes())};
		auto const checksum{fixture.checksum()};

		std::printf("%s,%s,%s,%zu,%zu,%zu,%zu,%zu,%u,%.3f,%.3f,%.8f,%.6f,%.6f,%llu\n",
					name(config.operation), name(config.memory_layout), name(config.cache),
					config.descriptor_count, config.payload_size, fixture.total_bytes(), config.iterations,
					config.samples, static_cast<unsigned>(FAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES),
					baseline_median, prefetch_median, ratio_median, bytes / baseline_median * gib_scale,
					bytes / prefetch_median * gib_scale, static_cast<unsigned long long>(checksum));
	}
	catch (std::exception const &exception)
	{
		std::fprintf(stderr, "benchmark setup failed: %s\n", exception.what());
		return 4;
	}
}
