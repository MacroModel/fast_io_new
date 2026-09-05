// Cross-version byte equivalence for the exact runtime benchmark workload.
// Compile this same file against each fast_io include root. Operation counts
// are reported separately because library versions can choose different CPOs.
// UWVM_BENCH_SHARED_PREDICATE and UWVM_BENCH_TIMESTAMP retain their usual meaning.
#define main uwvm_condition_unused_benchmark_main
#include "uwvm_condition_bench.cc"
#undef main

int main()
{
	using namespace uwvm_condition_bench;
	::std::array<::std::byte, 32u> bytes{};
	for (::std::size_t index{}; index != bytes.size(); ++index)
	{
		bytes[index] = static_cast<::std::byte>(index);
	}
#if UWVM_BENCH_TIMESTAMP
	constexpr ::fast_io::iso8601_timestamp timestamp{2026, 9u, 5u, 12u, 34u, 56u, 0u, 0};
#endif
	::std::size_t calls{};
	::std::size_t records{};
	for (unsigned valid{}; valid != 2u; ++valid)
	{
		auto const source{valid != 0u ? ::uwvm2::uwvm::utils::memory::print_memory{bytes.data(), bytes.data() + 8u, bytes.data() + bytes.size()} : ::uwvm2::uwvm::utils::memory::print_memory{}};
		for (unsigned mask{}; mask != 256u; ++mask)
		{
			state storage{};
			emit(output{&storage}, mask, source
#if UWVM_BENCH_TIMESTAMP
				 ,
				 timestamp
#endif
			);
			::std::printf("mask=%u valid=%u size=%zu bytes=", mask, valid, storage.size);
			for (::std::size_t index{}; index != storage.size; ++index)
			{
				::std::printf("%02x", static_cast<unsigned>(storage.data[index]));
			}
			::std::putchar('\n');
			calls += storage.calls;
			++records;
		}
	}
	::std::fprintf(stderr, "records=%zu primitive_calls=%zu predicate=%s timestamp=%u\n", records, calls,
				   UWVM_BENCH_SHARED_PREDICATE ? "shared" : "independent", static_cast<unsigned>(UWVM_BENCH_TIMESTAMP != 0));
}
