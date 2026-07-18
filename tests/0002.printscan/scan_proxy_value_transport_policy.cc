#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <fast_io.h>

namespace
{

struct terminal_input;

struct terminal_input_ref
{
	using input_char_type = char;
	terminal_input *input;
};

struct terminal_input
{
	using input_char_type = char;
	char *begin;
	char *current;
	char *end;
	::std::size_t commits{};
};

inline constexpr terminal_input_ref input_stream_ref_define(terminal_input &input) noexcept
{
	return {__builtin_addressof(input)};
}

inline constexpr char *ibuffer_begin(terminal_input_ref ref) noexcept
{
	return ref.input->begin;
}

inline constexpr char *ibuffer_curr(terminal_input_ref ref) noexcept
{
	return ref.input->current;
}

inline constexpr char *ibuffer_end(terminal_input_ref ref) noexcept
{
	return ref.input->end;
}

inline constexpr void ibuffer_set_curr(terminal_input_ref ref, char *current) noexcept
{
	ref.input->current = current;
	++ref.input->commits;
}

inline constexpr bool ibuffer_underflow(terminal_input_ref) noexcept
{
	return false;
}

inline constexpr bool ibuffer_underflow_never(terminal_input_ref) noexcept
{
	return true;
}

struct safe_target
{
	char value{};
	::std::size_t calls{};
};

struct safe_proxy
{
	safe_target *target;
};

inline constexpr safe_proxy
scan_alias_define(::fast_io::io_alias_t, safe_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, safe_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, safe_proxy>, char const *buffer, safe_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
	++proxy.target->calls;
}

inline constexpr ::std::true_type scan_precise_reserve_aggregate_commit_safe(
	::fast_io::io_reserve_type_t<char, safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, safe_proxy>) noexcept
{
	// Every observable mutation is reached through `target`; the CPO neither changes nor observes the descriptor
	// object itself. A copied pointer therefore denotes exactly the same scan state and has no identity dependency.
	return {};
}

struct context_safe_target
{
	char value{};
	::std::size_t calls{};
};

struct context_safe_proxy
{
	context_safe_target *target;
};

struct wide_context_safe_proxy
{
	context_safe_target *target;
	::std::size_t metadata;
};

struct context_safe_state
{};

inline constexpr context_safe_proxy
scan_alias_define(::fast_io::io_alias_t, context_safe_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::fast_io::io_type_t<context_safe_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, context_safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::fast_io::io_type_t<context_safe_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, wide_context_safe_proxy>) noexcept
{
	return {};
}

template <typename proxy_type>
	requires(::std::same_as<proxy_type, context_safe_proxy> ||
			 ::std::same_as<proxy_type, wide_context_safe_proxy>)
inline constexpr ::fast_io::parse_result<char const *> scan_context_define(
	::fast_io::io_reserve_type_t<char, proxy_type>, context_safe_state &, char const *first,
	char const *last, proxy_type &proxy) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::partial};
	}
	proxy.target->value = *first;
	++proxy.target->calls;
	return {first + 1u, ::fast_io::parse_code::ok};
}

template <typename proxy_type>
	requires(::std::same_as<proxy_type, context_safe_proxy> ||
			 ::std::same_as<proxy_type, wide_context_safe_proxy>)
inline constexpr ::fast_io::parse_code scan_context_eof_define(
	::fast_io::io_reserve_type_t<char, proxy_type>, context_safe_state &, proxy_type &) noexcept
{
	return ::fast_io::parse_code::end_of_file;
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, context_safe_proxy>) noexcept
{
	// This is the context analogue of `safe_proxy`: state lives in dispatch, while every persistent effect is reached
	// through the external target pointer. Repeated descriptor copies therefore preserve both identity and ordering.
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, wide_context_safe_proxy>) noexcept
{
	return {};
}

struct transport_test_exception
{};

struct throwing_safe_target
{
	char value{};
	::std::size_t calls{};
	bool throws{};
};

struct throwing_safe_proxy
{
	throwing_safe_target *target;
};

inline constexpr throwing_safe_proxy
scan_alias_define(::fast_io::io_alias_t, throwing_safe_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, throwing_safe_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, throwing_safe_proxy>, char const *buffer,
	throwing_safe_proxy &proxy)
{
	if (proxy.target->throws)
	{
		throw transport_test_exception{};
	}
	proxy.target->value = *buffer;
	++proxy.target->calls;
}

inline constexpr ::std::true_type scan_precise_reserve_aggregate_commit_safe(
	::fast_io::io_reserve_type_t<char, throwing_safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, throwing_safe_proxy>) noexcept
{
	// Descriptor copies only duplicate the external target pointer. Throwing before a target mutation therefore has the
	// same observable prefix and cursor schedule whether the fallback retained the original descriptor or copied it.
	return {};
}

struct move_only_status_source
{
	::std::size_t calls{};
};

struct move_only_status_ref
{
	using input_char_type = char;
	move_only_status_source *source;

	explicit constexpr move_only_status_ref(move_only_status_source *address) noexcept
		: source(address)
	{}

	move_only_status_ref(move_only_status_ref const &) = delete;
	move_only_status_ref &operator=(move_only_status_ref const &) = delete;

	constexpr move_only_status_ref(move_only_status_ref &&other) noexcept
		: source(::std::exchange(other.source, nullptr))
	{}

	move_only_status_ref &operator=(move_only_status_ref &&) = delete;
};

inline constexpr move_only_status_ref
input_stream_ref_define(move_only_status_source &source) noexcept
{
	return move_only_status_ref{__builtin_addressof(source)};
}

template <typename... proxies>
	requires(sizeof...(proxies) != 0u &&
			 (::std::same_as<::std::remove_cvref_t<proxies>, safe_proxy> && ...))
inline bool status_scan_define(move_only_status_ref &input, proxies &...proxy) noexcept
{
	++input.source->calls;
	((proxy.target->value = 'S', ++proxy.target->calls), ...);
	return true;
}

static_assert(::fast_io::operations::defines::storable_input_stream_ref_result<
	decltype(input_stream_ref_define(::std::declval<move_only_status_source &>()))>);

struct unmarked_target
{
	char value{};
	::std::size_t calls{};
};

struct unmarked_proxy
{
	unmarked_target *target;
};

inline constexpr unmarked_proxy
scan_alias_define(::fast_io::io_alias_t, unmarked_target &target) noexcept
{
	return {__builtin_addressof(target)};
}

inline constexpr ::std::size_t
scan_precise_reserve_size(::fast_io::io_reserve_type_t<char, unmarked_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, unmarked_proxy>, char const *buffer,
	unmarked_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
	++proxy.target->calls;
}

inline constexpr ::std::true_type scan_precise_reserve_aggregate_commit_safe(
	::fast_io::io_reserve_type_t<char, unmarked_proxy>) noexcept
{
	return {};
}

struct reference_target;

struct noncopyable_reference_proxy
{
	reference_target *target;

	explicit constexpr noncopyable_reference_proxy(reference_target *address) noexcept
		: target(address)
	{}

	noncopyable_reference_proxy(noncopyable_reference_proxy const &) = delete;
	noncopyable_reference_proxy &operator=(noncopyable_reference_proxy const &) = delete;
	noncopyable_reference_proxy(noncopyable_reference_proxy &&) = delete;
	noncopyable_reference_proxy &operator=(noncopyable_reference_proxy &&) = delete;
};

struct reference_target
{
	char value{};
	::std::size_t calls{};
	noncopyable_reference_proxy proxy{this};
};

inline constexpr noncopyable_reference_proxy &
scan_alias_define(::fast_io::io_alias_t, reference_target &target) noexcept
{
	return target.proxy;
}

inline constexpr ::std::size_t scan_precise_reserve_size(
	::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>) noexcept
{
	return 1u;
}

inline constexpr void scan_precise_reserve_define(
	::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>, char const *buffer,
	noncopyable_reference_proxy &proxy) noexcept
{
	proxy.target->value = *buffer;
	++proxy.target->calls;
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, noncopyable_reference_proxy>) noexcept
{
	// Marking the representation does not grant permission to copy an lvalue. The entry policy independently requires
	// an exact unqualified non-lvalue expression plus trivial special members, so this alias must remain on the ref path.
	return {};
}

struct oversized_safe_proxy
{
	::std::size_t words[3u];
};

struct two_word_safe_proxy
{
	::std::size_t first;
	::std::size_t second;
};

struct one_word_plus_one_safe_proxy
{
	unsigned char bytes[sizeof(::std::size_t) + 1u];
};

struct alignas(2u * alignof(::std::size_t)) over_aligned_safe_proxy
{
	::std::size_t word;
};

struct incomplete_proxy;

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, oversized_safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, two_word_safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, one_word_plus_one_safe_proxy>) noexcept
{
	return {};
}

inline constexpr ::std::true_type scan_proxy_value_transport_safe(
	::fast_io::io_reserve_type_t<char, over_aligned_safe_proxy>) noexcept
{
	return {};
}

static_assert(::fast_io::value_transport_safe_scan_proxy<char, safe_proxy>);
static_assert(::fast_io::value_transport_safe_scan_proxy<char, throwing_safe_proxy>);
static_assert(!::fast_io::value_transport_safe_scan_proxy<char, unmarked_proxy>);
static_assert(::fast_io::value_transport_safe_scan_proxy<
	char, ::fast_io::parameter<int &>>);
static_assert(!::fast_io::value_transport_safe_scan_proxy<
	char, ::fast_io::parameter<int>>);
static_assert(!::fast_io::details::decay::batch_precise_aggregate_commit_safe<
	char, throwing_safe_proxy>);

static_assert(::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy, safe_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy, unmarked_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy &, safe_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy const, safe_proxy const>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, safe_proxy volatile, safe_proxy volatile>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, incomplete_proxy, incomplete_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, unmarked_proxy, unmarked_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, noncopyable_reference_proxy &, noncopyable_reference_proxy &>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, oversized_safe_proxy, oversized_safe_proxy>);
static_assert(!::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, over_aligned_safe_proxy, over_aligned_safe_proxy>);
static_assert(::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<
	terminal_input_ref, safe_proxy, safe_proxy>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_precise_prefix_available<
	terminal_input_ref, context_safe_proxy, context_safe_proxy>());
static_assert(::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
	terminal_input_ref, context_safe_proxy, context_safe_proxy>);
static_assert(::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, context_safe_proxy, context_safe_proxy>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, context_safe_proxy>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, context_safe_proxy, context_safe_proxy, context_safe_proxy>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, context_safe_proxy &, context_safe_proxy &>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, context_safe_proxy const, context_safe_proxy const>());
static_assert(!::fast_io::operations::decay::scan_owned_proxy_context_pair_available<
	terminal_input_ref, wide_context_safe_proxy, wide_context_safe_proxy>());

template <::std::size_t>
using repeated_safe_proxy = safe_proxy;

template <::std::size_t>
using repeated_two_word_proxy = two_word_safe_proxy;

template <::std::size_t>
using repeated_one_word_plus_one_proxy = one_word_plus_one_safe_proxy;

template <::std::size_t... indices>
inline consteval bool safe_count_pack_eligible(::std::index_sequence<indices...>)
{
	return ::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
		terminal_input_ref, repeated_safe_proxy<indices>...>;
}

template <::std::size_t... indices>
inline consteval bool safe_count_chunked_eligible(::std::index_sequence<indices...>)
{
	return ::fast_io::operations::decay::scan_chunked_proxy_pack_eligible<
		terminal_input_ref, repeated_safe_proxy<indices>...>;
}

template <::std::size_t... indices>
inline consteval bool two_word_pack_eligible(::std::index_sequence<indices...>)
{
	return ::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
		terminal_input_ref, repeated_two_word_proxy<indices>...>;
}

template <::std::size_t... indices>
inline consteval bool two_word_chunked_eligible(::std::index_sequence<indices...>)
{
	return ::fast_io::operations::decay::scan_chunked_proxy_pack_eligible<
		terminal_input_ref, repeated_two_word_proxy<indices>...>;
}

template <::std::size_t... indices>
inline consteval bool one_word_plus_one_pack_eligible(::std::index_sequence<indices...>)
{
	return ::fast_io::operations::decay::scan_owned_proxy_pack_eligible<
		terminal_input_ref, repeated_one_word_plus_one_proxy<indices>...>;
}

template <typename rejected_type, ::std::size_t... indices>
inline consteval bool chunked_pack_with_rejected_type(
	::std::index_sequence<indices...>)
{
	// One rejected representation plus `policy_max_count` marked descriptors crosses the large-pack boundary on every
	// supported ABI. This ensures each negative tests individual semantic admission rather than failing merely by count.
	return ::fast_io::operations::decay::scan_chunked_proxy_pack_eligible<
		terminal_input_ref, rejected_type, repeated_safe_proxy<indices>...>;
}

inline constexpr ::std::size_t policy_max_count{
	::fast_io::operations::decay::scan_owned_proxy_max_count};
inline constexpr ::std::size_t fallback_chunk_max_count{
	::fast_io::operations::decay::scan_proxy_value_fallback_chunk_max_count};
inline constexpr ::std::size_t two_word_budget_count{
	::fast_io::operations::decay::scan_owned_proxy_max_total_size /
		sizeof(two_word_safe_proxy)};
inline constexpr ::std::size_t one_word_plus_one_abi_extent{
	2u * sizeof(::std::size_t)};
inline constexpr ::std::size_t one_word_plus_one_budget_count{
	::fast_io::operations::decay::scan_owned_proxy_max_total_size /
		one_word_plus_one_abi_extent};

static_assert(policy_max_count >= 9u);
static_assert(fallback_chunk_max_count != 0u);
static_assert(fallback_chunk_max_count <= policy_max_count);
static_assert(fallback_chunk_max_count *
				  ::fast_io::operations::decay::scan_owned_proxy_max_object_size <=
			  ::fast_io::operations::decay::scan_owned_proxy_max_total_size);
static_assert(safe_count_pack_eligible(::std::make_index_sequence<policy_max_count>{}));
static_assert(!safe_count_pack_eligible(::std::make_index_sequence<policy_max_count + 1u>{}));
static_assert(!safe_count_chunked_eligible(::std::make_index_sequence<policy_max_count>{}));
static_assert(safe_count_chunked_eligible(
	::std::make_index_sequence<policy_max_count + 1u>{}));
static_assert(safe_count_chunked_eligible(::std::make_index_sequence<64u>{}));
static_assert(two_word_pack_eligible(::std::make_index_sequence<two_word_budget_count>{}));
static_assert(!two_word_pack_eligible(::std::make_index_sequence<two_word_budget_count + 1u>{}));
static_assert(two_word_chunked_eligible(
	::std::make_index_sequence<policy_max_count + 1u>{}));
static_assert(one_word_plus_one_pack_eligible(
	::std::make_index_sequence<one_word_plus_one_budget_count>{}));
static_assert(!one_word_plus_one_pack_eligible(
	::std::make_index_sequence<one_word_plus_one_budget_count + 1u>{}));
static_assert(!chunked_pack_with_rejected_type<unmarked_proxy>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<safe_proxy &>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<safe_proxy const>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<safe_proxy volatile>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<incomplete_proxy>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<noncopyable_reference_proxy &>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<oversized_safe_proxy>(
	::std::make_index_sequence<policy_max_count>{}));
static_assert(!chunked_pack_with_rejected_type<over_aligned_safe_proxy>(
	::std::make_index_sequence<policy_max_count>{}));

template <typename target_type, ::std::size_t count, ::std::size_t... indices>
inline bool scan_pack(
	terminal_input &input, ::std::array<target_type, count> &targets,
	::std::index_sequence<indices...>)
{
	return ::fast_io::io::scan<true>(input, targets[indices]...);
}

template <typename target_type, ::std::size_t count>
inline bool complete_pack_is_correct()
{
	::std::array<char, count> storage{};
	::std::array<target_type, count> targets{};
	for (::std::size_t i{}; i != count; ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	if (!scan_pack(input, targets, ::std::make_index_sequence<count>{}) ||
		input.current != input.end || input.commits != 1u)
	{
		return false;
	}
	for (::std::size_t i{}; i != count; ++i)
	{
		if (targets[i].value != storage[i] || targets[i].calls != 1u)
		{
			return false;
		}
	}
	return true;
}

template <::std::size_t count>
inline bool short_cold_fallback_is_correct()
{
	static_assert(count > 1u);
	::std::array<char, count - 1u> storage{};
	for (::std::size_t i{}; i != storage.size(); ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	::std::array<safe_target, count> targets{};
	terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	if (scan_pack(input, targets, ::std::make_index_sequence<count>{}) ||
		input.current != input.end || input.commits != storage.size())
	{
		return false;
	}
	for (::std::size_t i{}; i != targets.size(); ++i)
	{
		::std::size_t const expected_calls{i < storage.size() ? 1u : 0u};
		if (targets[i].calls != expected_calls)
		{
			return false;
		}
		if (i < storage.size() && targets[i].value != storage[i])
		{
			return false;
		}
	}
	return true;
}

inline bool chunked_exception_prefix_is_correct()
{
	// Thirty-three proxies select the large policy even on Linux's 32-object whole-owner ABI. Supplying only 32 bytes
	// forces the controller into scalar fallback; throwing at the first target of Linux's second conservative chunk
	// checks both the commit-before-CPO schedule and cross-chunk exception short-circuiting. On smaller ABIs the same
	// target lies at a later chunk boundary and proves the identical property.
	::std::array<char, 32u> storage{};
	::std::array<throwing_safe_target, 33u> targets{};
	for (::std::size_t i{}; i != storage.size(); ++i)
	{
		storage[i] = static_cast<char>('a' + i % 26u);
	}
	targets[16u].throws = true;
	terminal_input input{storage.data(), storage.data(), storage.data() + storage.size()};
	bool caught{};
	try
	{
		(void)scan_pack(input, targets, ::std::make_index_sequence<targets.size()>{});
	}
	catch (transport_test_exception const &)
	{
		caught = true;
	}
	if (!caught || input.current != storage.data() + 17u || input.commits != 17u)
	{
		return false;
	}
	for (::std::size_t i{}; i != targets.size(); ++i)
	{
		::std::size_t const expected_calls{i < 16u ? 1u : 0u};
		if (targets[i].calls != expected_calls)
		{
			return false;
		}
		if (i < 16u && targets[i].value != storage[i])
		{
			return false;
		}
	}
	return true;
}

inline bool noncopyable_reference_is_correct()
{
	char storage{'R'};
	reference_target target;
	terminal_input input{__builtin_addressof(storage), __builtin_addressof(storage),
						 __builtin_addressof(storage) + 1u};
	return ::fast_io::io::scan<true>(input, target) && input.current == input.end &&
		   target.value == storage && target.calls == 1u;
}

inline bool move_only_status_observer_is_correct()
{
	move_only_status_source source;
	safe_target first;
	safe_target second;
	return ::fast_io::io::scan<true>(source, first, second) && source.calls == 1u &&
		   first.value == 'S' && second.value == 'S' && first.calls == 1u && second.calls == 1u;
}

inline bool context_pair_is_correct()
{
	char storage[2u]{'L', 'R'};
	terminal_input input{storage, storage, storage + 2u};
	context_safe_target first;
	context_safe_target second;
	return ::fast_io::io::scan<true>(input, first, second) && input.current == input.end &&
		   input.commits == 2u && first.value == storage[0u] && second.value == storage[1u] &&
		   first.calls == 1u && second.calls == 1u;
}

} // namespace

int main()
{
	// Counts 16/17 and 32/33 straddle the conservative native and Linux whole-owner boundaries; 64 proves that repeated
	// cold chunks preserve one linear controller. The unmarked trivial pack proves object traits alone do not opt a
	// customization in, while the deleted-copy alias proves exact lvalue identity remains accepted.
	return complete_pack_is_correct<safe_target, 9u>() &&
			   short_cold_fallback_is_correct<9u>() &&
			   complete_pack_is_correct<safe_target, 16u>() &&
			   complete_pack_is_correct<safe_target, 17u>() &&
			   complete_pack_is_correct<safe_target, 32u>() &&
			   complete_pack_is_correct<safe_target, 33u>() &&
			   complete_pack_is_correct<safe_target, 64u>() &&
			   short_cold_fallback_is_correct<17u>() &&
			   short_cold_fallback_is_correct<33u>() &&
			   short_cold_fallback_is_correct<64u>() &&
			   chunked_exception_prefix_is_correct() &&
			   complete_pack_is_correct<unmarked_target, 2u>() &&
			   noncopyable_reference_is_correct() && move_only_status_observer_is_correct() &&
			   context_pair_is_correct()
			   ? 0
			   : 1;
}
