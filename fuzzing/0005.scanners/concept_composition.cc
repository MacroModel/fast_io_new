#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../tests/0002.printscan/scan_concept_support.h"

namespace
{

using namespace ::scan_concept_harness;

[[noreturn]] inline void fail() noexcept
{
	__builtin_trap();
}

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		fail();
	}
}

template <::std::size_t count>
inline void fuzz_precise_pack(::std::array<char, 64u> const &bytes)
{
	::fast_io::basic_ibuffer_view<char> input(bytes.begin(), bytes.begin() + count);
	::std::array<fixed_record_target<1u>, count> targets{};
	require(scan_fixed_pack(input, targets));
	for (::std::size_t i{}; i != count; ++i)
	{
		require(targets[i].value[0] == bytes[i]);
		require(targets[i].calls == 1u);
	}
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data, ::std::size_t size)
{
	// Reserve one byte for the runtime refill width and bound semantic work.  The input bytes remain uninterpreted text;
	// replacing the delimiter only makes the generated trailing '|' the unique record boundary.
	auto const chunk_size{size == 0u ? 1u : static_cast<::std::size_t>(data[0] % refill_storage_size) + 1u};
	auto const available_payload{size == 0u ? 0u : size - 1u};
	auto const payload_size{available_payload < 128u ? available_payload : 128u};
	::std::array<char, 129u> delimited{};
	for (::std::size_t i{}; i != payload_size; ++i)
	{
		auto const value{static_cast<char>(data[i + 1u])};
		delimited[i] = value == '|' ? '~' : value;
	}
	delimited[payload_size] = '|';
	::std::string_view const record(delimited.data(), payload_size);
	::std::string_view const record_with_delimiter(delimited.data(), payload_size + 1u);

	::fast_io::basic_ibuffer_view<char> terminal_input(record_with_delimiter);
	literal_target<true> terminal;
	require(::fast_io::io::scan<true>(terminal_input, terminal));
	require(literal_equals(terminal, record));
	require(terminal.contiguous_calls == 1u && terminal.context_calls == 0u && terminal.commits == 1u);

	bounded_refill_source context_source;
	context_source.reset(record_with_delimiter, chunk_size);
	literal_target<false> context;
	require(::fast_io::io::scan<true>(context_source, context));
	require(literal_equals(context, record));
	require(context.contiguous_calls == 0u && context.context_calls != 0u && context.commits == 1u);

	// Two context proxies select the bounded one-word value-transport ABI. Use two independently delimited copies so
	// randomized refill positions cover both the inter-target cursor handoff and each scanner's partial state machine.
	::std::array<char, 258u> context_pair_text{};
	for (::std::size_t i{}; i != payload_size; ++i)
	{
		context_pair_text[i] = delimited[i];
		context_pair_text[payload_size + 1u + i] = delimited[i];
	}
	context_pair_text[payload_size] = '|';
	context_pair_text[2u * payload_size + 1u] = '|';
	bounded_refill_source context_pair_source;
	context_pair_source.reset(
		::std::string_view(context_pair_text.data(), 2u * (payload_size + 1u)), chunk_size);
	literal_target<false> context_first;
	literal_target<false> context_second;
	require(::fast_io::io::scan<true>(context_pair_source, context_first, context_second));
	require(literal_equals(context_first, record));
	require(literal_equals(context_second, record));
	require(context_first.commits == 1u && context_second.commits == 1u);

	bounded_refill_source eof_source;
	eof_source.reset(record, chunk_size);
	literal_target<false> eof_context;
	if (record.empty())
	{
		require(!::fast_io::io::scan<true>(eof_source, eof_context));
		require(eof_context.commits == 0u);
	}
	else
	{
		require(::fast_io::io::scan<true>(eof_source, eof_context));
		require(literal_equals(eof_context, record));
		require(eof_context.commits == 1u);
	}

	bounded_refill_source hybrid_source;
	hybrid_source.reset(record_with_delimiter, chunk_size);
	literal_target<true> refill_hybrid;
	require(::fast_io::io::scan<true>(hybrid_source, refill_hybrid));
	require(literal_equals(refill_hybrid, record));
	require(refill_hybrid.contiguous_calls == 0u && refill_hybrid.context_calls != 0u &&
			refill_hybrid.commits == 1u);

	// A mixed pack exercises precise staging followed by a context scanner on the same refill object.  The fixed prefix
	// is protocol framing, not a number, so failures isolate cursor ownership and pack forwarding.
	::std::array<char, 133u> framed{};
	framed[0] = 'H';
	framed[1] = 'E';
	framed[2] = 'A';
	framed[3] = 'D';
	for (::std::size_t i{}; i != payload_size + 1u; ++i)
	{
		framed[i + 4u] = delimited[i];
	}
	bounded_refill_source mixed_source;
	mixed_source.reset(::std::string_view(framed.data(), payload_size + 5u), chunk_size);
	fixed_record_target<4u> header;
	literal_target<false> body;
	require(::fast_io::io::scan<true>(mixed_source, header, body));
	require((header.value == ::std::array<char, 4u>{'H', 'E', 'A', 'D'}));
	require(literal_equals(body, record));

	::std::array<char, 64u> pack_bytes{};
	for (::std::size_t i{}; i != pack_bytes.size(); ++i)
	{
		pack_bytes[i] = payload_size == 0u ? static_cast<char>(i) : delimited[i % payload_size];
	}
	switch (size == 0u ? 0u : data[0] % 3u)
	{
	case 0u:
		fuzz_precise_pack<1u>(pack_bytes);
		break;
	case 1u:
		fuzz_precise_pack<9u>(pack_bytes);
		break;
	default:
		fuzz_precise_pack<64u>(pack_bytes);
		break;
	}

	::fast_io::basic_ibuffer_view<char> alias_input(pack_bytes.begin(), pack_bytes.begin() + 1u);
	reference_alias_target alias_target;
	require(::fast_io::io::scan<true>(alias_input, alias_target));
	require(alias_target.value == pack_bytes[0]);

	status_source direct_status_source;
	status_target direct_status_target;
	require(::fast_io::io::scan<true>(direct_status_source, direct_status_target));
	require(direct_status_source.calls == 1u && direct_status_target.value);

	locked_status_source locked_source;
	locked_source.source.lock_observation = __builtin_addressof(locked_source.lock.locked);
	status_target locked_first;
	status_target locked_second;
	require(::fast_io::io::scan<true>(locked_source, locked_first, locked_second));
	require(locked_first.value && locked_second.value);
	require(locked_source.lock.lock_calls == 1u && locked_source.lock.unlock_calls == 1u &&
			!locked_source.lock.locked);
	return 0;
}
