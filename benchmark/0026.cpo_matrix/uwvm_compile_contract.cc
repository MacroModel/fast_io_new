#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#include <uwvm2/uwvm/utils/memory/print.h>
#include <uwvm2/uwvm/wasm/type/memory_limit.h>

#if defined(FAST_IO_UWVM_PROVIDER_PROOFS) && FAST_IO_UWVM_PROVIDER_PROOFS
#include "uwvm_compile_proofs.h"
#endif

namespace uwvm_compile_contract
{
struct capture
{
	::std::u8string bytes;
	::std::vector<::std::size_t> operations;
};

struct output
{
	using output_char_type = char8_t;
	capture *state;
};

inline constexpr output output_stream_ref_define(output out) noexcept
{
	return out;
}

// The fixture consumes descriptors synchronously and has no status-print owner. Its trace records operation kind,
// descriptor count, and every descriptor extent: byte equality alone would not detect a changed primitive boundary.
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char8_t, output>) noexcept
{
	return {};
}
inline constexpr ::std::true_type print_semantic_optional_scatter_plan_stream(
	::fast_io::io_reserve_type_t<char8_t, output>) noexcept
{
	return {};
}
inline constexpr ::std::true_type print_semantic_optional_scatter_barrier_plan_stream(
	::fast_io::io_reserve_type_t<char8_t, output>) noexcept
{
	return {};
}

inline void write_all_overflow_define(output out, char8_t const *first, char8_t const *last)
{
	out.state->operations.push_back(0u);
	out.state->operations.push_back(static_cast<::std::size_t>(last - first));
	out.state->bytes.append(first, last);
}

inline void scatter_write_all_overflow_define(output out, ::fast_io::basic_io_scatter_t<char8_t> const *scatters,
											  ::std::size_t count)
{
	out.state->operations.push_back(1u);
	out.state->operations.push_back(count);
	for (::std::size_t index{}; index != count; ++index)
	{
		auto const scatter{scatters[index]};
		out.state->operations.push_back(scatter.len);
		if (scatter.len != 0u)
		{
			assert(scatter.base != nullptr);
			out.state->bytes.append(scatter.base, scatter.len);
		}
	}
}

template <bool Line, typename T>
void emit(output out, unsigned mask, T const &source)
{
	// Four independent predicates exercise all active shapes without requiring the unmarked control to enumerate
	// a huge product. Mandatory literals preserve ordinary multi-leaf policies on both sides of source classification.
	::fast_io::operations::print_freestanding<Line>(out,
													::fast_io::mnp::cond((mask & 1u) != 0u, u8"AA"), u8"|",
													::fast_io::mnp::cond((mask & 2u) != 0u, u8"BB"), u8"|",
													::fast_io::mnp::cond((mask & 4u) != 0u, u8"CC"), u8"|",
													::fast_io::mnp::cond((mask & 8u) != 0u, u8"DD"), u8"|", source, u8"!");
}

template <typename T>
void check(T const &source)
{
	for (unsigned mask{}; mask != 16u; ++mask)
	{
		for (bool line : {false, true})
		{
			capture state;
			if (line)
			{
				emit<true>(output{&state}, mask, source);
			}
			else
			{
				emit<false>(output{&state}, mask, source);
			}
			::std::printf("%u %u %zu %zu\n", mask, static_cast<unsigned>(line), state.bytes.size(), state.operations.size());
			::std::fwrite(state.bytes.data(), sizeof(char8_t), state.bytes.size(), stdout);
			::std::putchar('\n');
			for (auto operation : state.operations)
			{
				::std::printf("%zu,", operation);
			}
			::std::putchar('\n');
		}
	}
}
} // namespace uwvm_compile_contract

int main()
{
	using namespace uwvm_compile_contract;
	using namespace ::uwvm2::parser::wasm::standard;
	::std::array<::std::byte, 32> memory{};
	for (::std::size_t index{}; index != memory.size(); ++index)
	{
		memory[index] = static_cast<::std::byte>(index);
	}
	check(::uwvm2::uwvm::utils::memory::print_memory{});
	check(::uwvm2::uwvm::utils::memory::print_memory{memory.data(), memory.data() + 10u, memory.data() + memory.size()});
	check(wasm1::type::section_details(wasm1::type::limits_type{1u, 0xffffffffu, true}));
	check(wasm1::type::section_details(wasm1::type::limits_type{0u, 0u, false}));
	check(::uwvm2::uwvm::wasm::type::section_details(
		::uwvm2::uwvm::wasm::type::module_memory_limit_t{1u, SIZE_MAX, true}));

	using first_feature = wasm1::features::wasm1;
	using second_feature = wasm1p1::features::wasm1p1;
	using value_type = wasm1::features::final_value_type_t<first_feature, second_feature>;
	::std::array<value_type, 128> parameters{};
	parameters.fill(value_type::i32);
	wasm1::features::final_function_type<first_feature, second_feature> signature{
		{parameters.data(), parameters.data() + parameters.size()}, {parameters.data(), parameters.data() + 1u}};
	// This source exceeds a context window, checking ordered resumption as well as the short and empty signatures.
	check(wasm1::features::section_details(signature));
	signature.parameter.end = parameters.data() + 1u;
	check(wasm1::features::section_details(signature));
	signature.parameter.end = signature.parameter.begin;
	signature.result.end = signature.result.begin;
	check(wasm1::features::section_details(signature));
}
