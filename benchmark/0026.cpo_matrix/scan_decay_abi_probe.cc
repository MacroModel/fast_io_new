#include <concepts>
#include <cstddef>
#include <utility>

#include <fast_io_core.h>

namespace fast_io_scan_decay_abi_probe
{

using input_ref = ::fast_io::basic_ibuffer_view_ref<char>;
using stack_policy = ::fast_io::details::default_print_stack_policy;

struct target_proxy
{
	char *target{};
};

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, target_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, target_proxy>, char const *source,
	target_proxy &proxy) noexcept
{
	*proxy.target = *source;
}

inline constexpr ::std::true_type scan_precise_reserve_aggregate_commit_safe(
	::fast_io::io_reserve_type_t<char, target_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, target_proxy>) noexcept
{
	// Every copy refers to the same external target. Neither cursor state nor
	// observable identity lives in the proxy, so value substitution is exact.
	return {};
}

using input_owner_entry = bool (*)(input_ref, target_proxy &&, target_proxy &&);
using proxy_owner_entry = bool (*)(input_ref &, target_proxy, target_proxy);
using borrowed_input_entry = bool (*)(input_ref &, target_proxy &&, target_proxy &&);

/*
 * Input ownership and scanner-proxy ownership are separate ABI boundaries.
 * The compatibility entry owns its input value but retains the incoming proxy
 * expressions for their complete full expression. The selected compact-pack
 * entry borrows that stable input and owns two substitutable proxy values.
 * These types pin both distinctions independently of caller-side inlining.
 */
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::scan_freestanding_decay<
		stack_policy, input_ref, target_proxy, target_proxy>), input_owner_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::scan_freestanding_decay_owned<
		stack_policy, input_ref, target_proxy, target_proxy>), proxy_owner_entry>);
static_assert(::std::same_as<
	decltype(&::fast_io::operations::decay::scan_freestanding_decay_borrowed_input<
		stack_policy, input_ref, target_proxy, target_proxy>), borrowed_input_entry>);

} // namespace fast_io_scan_decay_abi_probe

extern "C"
{
	// Actual retained relocations, rather than unevaluated address expressions,
	// force all three library specializations into cross-target object files.
	[[gnu::used]] ::fast_io_scan_decay_abi_probe::input_owner_entry
		fast_io_scan_decay_input_owner_entry{
			&::fast_io::operations::decay::scan_freestanding_decay<
				::fast_io_scan_decay_abi_probe::stack_policy,
				::fast_io_scan_decay_abi_probe::input_ref,
				::fast_io_scan_decay_abi_probe::target_proxy,
				::fast_io_scan_decay_abi_probe::target_proxy>};

	[[gnu::used]] ::fast_io_scan_decay_abi_probe::proxy_owner_entry
		fast_io_scan_decay_proxy_owner_entry{
			&::fast_io::operations::decay::scan_freestanding_decay_owned<
				::fast_io_scan_decay_abi_probe::stack_policy,
				::fast_io_scan_decay_abi_probe::input_ref,
				::fast_io_scan_decay_abi_probe::target_proxy,
				::fast_io_scan_decay_abi_probe::target_proxy>};

	[[gnu::used]] ::fast_io_scan_decay_abi_probe::borrowed_input_entry
		fast_io_scan_decay_borrowed_input_entry{
			&::fast_io::operations::decay::scan_freestanding_decay_borrowed_input<
				::fast_io_scan_decay_abi_probe::stack_policy,
				::fast_io_scan_decay_abi_probe::input_ref,
				::fast_io_scan_decay_abi_probe::target_proxy,
				::fast_io_scan_decay_abi_probe::target_proxy>};
}

int main()
{
	char const source[]{'A', 'B'};
	::fast_io::basic_ibuffer_view<char> view{source, source + 2u};
	::fast_io_scan_decay_abi_probe::input_ref input{__builtin_addressof(view)};
	char first{};
	char second{};
	using proxy = ::fast_io_scan_decay_abi_probe::target_proxy;
	if (!fast_io_scan_decay_input_owner_entry(
			input, proxy{__builtin_addressof(first)}, proxy{__builtin_addressof(second)}) ||
		first != 'A' || second != 'B' || view.curr_ptr != view.end_ptr)
	{
		return 1;
	}
	view.curr_ptr = view.begin_ptr;
	first = second = 0;
	if (!fast_io_scan_decay_proxy_owner_entry(
			input, proxy{__builtin_addressof(first)}, proxy{__builtin_addressof(second)}) ||
		first != 'A' || second != 'B' || view.curr_ptr != view.end_ptr)
	{
		return 1;
	}
	return 0;
}
