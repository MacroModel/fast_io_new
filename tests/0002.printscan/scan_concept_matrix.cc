#include <array>
#include <cassert>
#include <cstddef>
#include <string_view>

#include "scan_concept_support.h"

namespace
{

using namespace ::scan_concept_harness;

static_assert(::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	::fast_io::basic_ibuffer_view<char>, fixed_record_proxy<1u>, fixed_record_proxy<1u>>);
static_assert(::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<
	::fast_io::basic_ibuffer_view<char>, fixed_record_proxy<1u>, fixed_record_proxy<1u>>());
static_assert(::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	status_source_ref, status_proxy, status_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<
	status_source_ref, status_proxy, status_proxy>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<
	::fast_io::basic_ibuffer_view<char>, literal_proxy<false>, literal_proxy<false>>());

template <::std::size_t count>
inline void test_precise_pack()
{
	::std::array<char, count> input_storage{};
	for (::std::size_t i{}; i != count; ++i)
	{
		input_storage[i] = static_cast<char>('A' + i % 26u);
	}
	::fast_io::basic_ibuffer_view<char> input(input_storage);
	::std::array<fixed_record_target<1u>, count> targets{};
	assert(scan_fixed_pack(input, targets));
	assert(input.curr_ptr == input.end_ptr);
	for (::std::size_t i{}; i != count; ++i)
	{
		assert(targets[i].calls == 1u);
		assert(targets[i].value[0] == input_storage[i]);
	}
}

inline void test_precise_and_reference_alias()
{
	// Counts 1, 9, and 64 cover the scalar path, the first nontrivial pack, and a pack large enough to expose
	// accidental recursive copying or quadratic template/run-time work.
	test_precise_pack<1u>();
	test_precise_pack<9u>();
	test_precise_pack<64u>();

	::std::string_view empty;
	::fast_io::basic_ibuffer_view<char> empty_input(empty);
	fixed_record_target<0u> zero;
	assert(::fast_io::io::scan<true>(empty_input, zero));
	assert(zero.calls == 1u);

	::std::string_view one{"R"};
	::fast_io::basic_ibuffer_view<char> one_input(one);
	reference_alias_target target;
	assert(::fast_io::io::scan<true>(one_input, target));
	assert(target.value == 'R');
}

inline void test_terminal_parse_and_to_protocol()
{
	char const delimited[]{'a', 'b', 'c', '|'};
	literal_target<false> parsed;
	auto const result{::fast_io::parse_by_scan(delimited, delimited + 4u, parsed)};
	assert(result.iter == delimited + 4u);
	assert(result.code == ::fast_io::parse_code::ok);
	assert(literal_equals(parsed, "abc"));
	assert(parsed.context_calls == 4u && parsed.commits == 1u);

	char const unterminated[]{'e', 'o', 'f'};
	literal_target<false> parsed_at_eof;
	auto const eof_result{::fast_io::parse_by_scan(unterminated, unterminated + 3u, parsed_at_eof)};
	assert(eof_result.iter == unterminated + 3u);
	assert(eof_result.code == ::fast_io::parse_code::ok);
	assert(literal_equals(parsed_at_eof, "eof"));
	assert(parsed_at_eof.context_calls == 3u && parsed_at_eof.commits == 1u);

	char const available[]{'x'};
	stalled_context_target stalled;
	auto const stalled_result{::fast_io::parse_by_scan(available, available + 1u, stalled)};
	assert(stalled_result.iter == available);
	assert(stalled_result.code == ::fast_io::parse_code::invalid);
	assert(stalled.context_calls == 1u && stalled.eof_calls == 0u);

	stalled_context_target empty;
	auto const empty_result{::fast_io::parse_by_scan(available, available, empty)};
	assert(empty_result.iter == available);
	assert(empty_result.code == ::fast_io::parse_code::end_of_file);
	assert(empty.context_calls == 0u && empty.eof_calls == 1u);

	char const bounded[]{'L', 'x', 'R'};
	escaped_context_target backward;
	backward.reported_iterator = bounded;
	auto const backward_result{::fast_io::parse_by_scan(bounded + 1u, bounded + 2u, backward)};
	assert(backward_result.iter == bounded + 1u);
	assert(backward_result.code == ::fast_io::parse_code::invalid);
	assert(backward.context_calls == 1u);

	escaped_context_target past_end;
	past_end.reported_iterator = bounded + 3u;
	past_end.reported_code = ::fast_io::parse_code::ok;
	auto const past_end_result{::fast_io::parse_by_scan(bounded + 1u, bounded + 2u, past_end)};
	assert(past_end_result.iter == bounded + 1u);
	assert(past_end_result.code == ::fast_io::parse_code::invalid);
	assert(past_end.context_calls == 1u);

	literal_target<false> direct_conversion;
	::fast_io::inplace_to(direct_conversion, "abc|");
	assert(literal_equals(direct_conversion, "abc"));
	assert(direct_conversion.context_calls == 4u && direct_conversion.commits == 1u);

	literal_target<false> split_conversion;
	::fast_io::inplace_to(split_conversion, "ab", "c|");
	assert(literal_equals(split_conversion, "abc"));
	assert(split_conversion.context_calls == 4u && split_conversion.commits == 1u);

	auto converted{::fast_io::to<literal_target<false>>("abc|")};
	assert(literal_equals(converted, "abc"));
	assert(converted.context_calls == 4u && converted.commits == 1u);

	// Both conversions normalize to `noncopyable_reference_proxy&`. Success proves that neither the public wrapper,
	// the decay strategy, nor its detection model attempts to materialize that alias by value.
	reference_alias_target referenced;
	::fast_io::inplace_to(referenced, "I");
	assert(referenced.value == 'I');
	auto referenced_value{::fast_io::to<reference_alias_target>("T")};
	assert(referenced_value.value == 'T');
}

inline void test_terminal_and_refill_dispatch()
{
	::std::string_view terminal_text{"terminal|suffix"};
	::fast_io::basic_ibuffer_view<char> terminal_input(terminal_text);
	literal_target<true> terminal;
	assert(::fast_io::io::scan<true>(terminal_input, terminal));
	assert(literal_equals(terminal, "terminal"));
	assert(terminal.contiguous_calls == 1u);
	assert(terminal.context_calls == 0u);
	assert(terminal.commits == 1u);
	assert(::std::string_view(terminal_input.curr_ptr, terminal_input.end_ptr) == "suffix");

	for (::std::size_t chunk_size{1u}; chunk_size <= refill_storage_size; ++chunk_size)
	{
		bounded_refill_source source;
		source.reset("cross-boundary|tail", chunk_size);
		literal_target<true> hybrid;
		assert(::fast_io::io::scan<true>(source, hybrid));
		assert(literal_equals(hybrid, "cross-boundary"));
		// A contiguous scanner cannot prove that a refillable chunk is the complete token.  Context-only dispatch is
		// therefore the safe composition even though this target also advertises the terminal fast path.
		assert(hybrid.contiguous_calls == 0u);
		assert(hybrid.context_calls != 0u);
		assert(hybrid.commits == 1u);
	}

	bounded_refill_source eof_source;
	eof_source.reset("eof-terminated", 3u);
	literal_target<false> eof_target;
	assert(::fast_io::io::scan<true>(eof_source, eof_target));
	assert(literal_equals(eof_target, "eof-terminated"));
	assert(eof_target.commits == 1u);

	bounded_refill_source empty_source;
	empty_source.reset({}, 2u);
	literal_target<false> empty_target;
	assert(!::fast_io::io::scan<true>(empty_source, empty_target));
	assert(empty_target.commits == 0u);

	bounded_refill_source short_precise_source;
	short_precise_source.reset("XY", 1u);
	fixed_record_target<4u> short_precise;
	assert(!::fast_io::io::scan<true>(short_precise_source, short_precise));
	assert(short_precise.calls == 0u);
}

inline void test_mixed_and_context_packs()
{
	bounded_refill_source mixed_source;
	mixed_source.reset("HEADbody|", 3u);
	fixed_record_target<4u> header;
	literal_target<false> body;
	assert(::fast_io::io::scan<true>(mixed_source, header, body));
	assert((header.value == ::std::array<char, 4u>{'H', 'E', 'A', 'D'}));
	assert(literal_equals(body, "body"));

	bounded_refill_source context_source;
	context_source.reset("first|second|", 2u);
	literal_target<false> first;
	literal_target<false> second;
	assert(::fast_io::io::scan<true>(context_source, first, second));
	assert(literal_equals(first, "first"));
	assert(literal_equals(second, "second"));
	assert(first.commits == 1u && second.commits == 1u);

	::std::string_view terminal_pack_text{"left|right|"};
	::fast_io::basic_ibuffer_view<char> terminal_pack_input(terminal_pack_text);
	literal_target<true> left;
	literal_target<true> right;
	assert(::fast_io::io::scan<true>(terminal_pack_input, left, right));
	assert(literal_equals(left, "left"));
	assert(literal_equals(right, "right"));
	assert(left.contiguous_calls == 1u && right.contiguous_calls == 1u);
}

inline void test_status_and_mutex_composition()
{
	static_assert(::fast_io::operations::decay::defines::has_status_scan_define<status_source_ref, status_proxy &>);
	static_assert(!::fast_io::operations::decay::defines::has_status_scan_define<status_source_ref, int &>);

	status_source direct_source;
	::std::array<status_target, 9u> direct_targets{};
	assert(scan_status_pack(direct_source, direct_targets));
	for (auto const &target : direct_targets)
	{
		assert(target.value);
	}
	assert(direct_source.calls == direct_targets.size());

	locked_status_source locked_source;
	locked_source.source.lock_observation = __builtin_addressof(locked_source.lock.locked);
	::std::array<status_target, 9u> locked_targets{};
	assert(scan_status_pack(locked_source, locked_targets));
	for (auto const &target : locked_targets)
	{
		assert(target.value);
	}
	// Pack recursion occurs below one stream-level critical section.  Locking once per argument both wastes work and
	// permits another thread to observe an incomplete semantic record between adjacent pack elements.
	assert(locked_source.lock.lock_calls == 1u);
	assert(locked_source.lock.unlock_calls == 1u);
	assert(!locked_source.lock.locked);
}

} // namespace

int main()
{
	test_precise_and_reference_alias();
	test_terminal_parse_and_to_protocol();
	test_terminal_and_refill_dispatch();
	test_mixed_and_context_packs();
	test_status_and_mutex_composition();
}
