#pragma once

#include "../operations/printimpl/scatter_copy.h"

namespace fast_io
{

/// @brief Marks fast_io's run-time precision floating scalar as profitable for one-pass bounded concat construction.
/// @details Computing this scalar's exact size performs the floating conversion which emission would otherwise repeat.
///          This marker promises that its dynamic reserve bound is a cheap, repeatable upper-bound query. Concat still
///          requires an independently opted-in fresh destination, proves every other component statically or from an
///          already materialized descriptor, and checks the bound against its fixed stack budget before selecting it.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type concat_single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Authorizes direct put-area emission for fast_io's run-time precision floating scalar.
/// @details This is intentionally separate from concat's cost marker: a cheap bounded-size query alone says nothing
///          about exceptions or observable output state. The print strategy requires this exact source opt-in in
///          addition to its structural noexcept proof and the destination's deferred-cursor promise.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Returns the scalar's non-fatal upper bound when it fits `maximum_size`, or `SIZE_MAX` otherwise.
/// @details The definition lives with the floating formatter, where the representation-specific fixed overhead is
///          available. This declaration is intentionally part of the source opt-in contract: speculative concat
///          selection must never reuse the ordinary reserve-size CPO, whose overflow policy is termination.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::size_t concat_single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>,
	::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
	::std::size_t maximum_size) noexcept;

} // namespace fast_io

namespace fast_io::details::decay
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t calculate_scatter_reserve_size_unit()
{
	using real_type = ::std::remove_cvref_t<T>;
	if constexpr (reserve_printable<char_type, real_type>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, real_type>)};
		return sz;
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_reserve_size()
{
	if constexpr (sizeof...(Args) == 0)
	{
		return calculate_scatter_reserve_size_unit<char_type, T>();
	}
	else
	{
		// This historical exact-sum helper is also consumed by non-concat code whose caller owns overflow handling.
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			calculate_scatter_reserve_size_unit<char_type, T>(), calculate_scatter_reserve_size<char_type, Args...>());
	}
}

/// @brief Computes concat's optional static reserve aggregate, or reports that the one-buffer plan is unavailable.
/// @details Unlike the shared exact-sum helper above, concat can preserve valid component protocols when conservative
///          upper bounds do not fit one C++ pointer domain. Keeping this classifier concat-specific prevents unrelated
///          consumers such as `to` and kernel printing from interpreting SIZE_MAX as an allocation request.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_concat_scatter_reserve_size_or_unavailable() noexcept
{
	::std::size_t const current{::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		0u, calculate_scatter_reserve_size_unit<char_type, T>())};
	if constexpr (sizeof...(Args) == 0)
	{
		return current;
	}
	else
	{
		return ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
			current, calculate_concat_scatter_reserve_size_or_unavailable<char_type, Args...>());
	}
}

template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *print_reserve_define_chain_impl(char_type *p, T &t, Args &...args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		p = print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, p, t);
		if constexpr (line)
		{
			*p = char_literal_v<u8'\n', char_type>;
			++p;
		}
		return p;
	}
	else
	{
		return print_reserve_define_chain_impl<line>(
			print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, p, t), args...);
	}
}

/// @brief Stores the one-shot metadata selected for a mixed concat component.
/// @details Component kind remains a compile-time property of the corresponding argument type, so no run-time tag is
///          needed. Reserve leaves store their bound in `scatter.len` and leave `base` unused; retained scatter leaves
///          use both descriptor fields. This two-word tagged-by-type representation avoids one redundant machine word
///          per leaf while giving the planning pass stable storage without replaying an object-dependent customization.
template <::std::integral char_type>
struct concat_mixed_component_cache
{
	::fast_io::basic_io_scatter_t<char_type> scatter{};
};

/// @brief Proves that an optional concat planning table fits the bounded hot stack budget.
/// @details The configured print limit is a safety ceiling; four KiB is a separate code-generation and page-pressure
///          ceiling for metadata that does not itself hold formatted output. Division before multiplication makes the
///          test valid even for an adversarial template argument count. Plans above the limit keep identical semantics
///          in dynamic storage rather than growing the caller's frame with the flattened pack length.
template <::std::size_t count, typename element_type>
inline consteval bool concat_metadata_stack_safe() noexcept
{
	constexpr ::std::size_t configured_budget{
		::fast_io::details::decay::print_stack_buffer_max_bytes()};
	constexpr ::std::size_t preferred_budget{4u * 1024u};
	constexpr ::std::size_t budget{
		configured_budget < preferred_budget ? configured_budget : preferred_budget};
	return budget != 0u && count <= budget / sizeof(element_type);
}

/// @brief Measures every mixed concat component exactly once and caches the selected representation.
/// @details Protocol priority is identical to emission: static reserve, then dynamic reserve, then retained scatter.
///          The all-retained-scatter strategy is selected before this helper, so a dual-protocol run can still prefer
///          its exact descriptors globally. Each individual extent is checked before it can participate in pointer
///          arithmetic. Aggregate overflow returns SIZE_MAX but does not stop measurement; the sequential fallback
///          needs the complete cache and may still emit every individually representable component.
template <::std::integral char_type, typename... Args>
inline constexpr ::std::size_t concat_measure_mixed_components_once(
	::fast_io::details::decay::concat_mixed_component_cache<char_type> *cache, Args &...args)
{
	::std::size_t total{};
	::std::size_t index{};
	auto measure_one = [&cache, &total, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		auto &component{cache[index++]};
		::std::size_t extent{};
		if constexpr (::fast_io::reserve_printable<char_type, arg_type>)
		{
			component.scatter.len = print_reserve_size(::fast_io::io_reserve_type<char_type, arg_type>);
			extent = component.scatter.len;
		}
		else if constexpr (::fast_io::dynamic_reserve_printable<char_type, arg_type>)
		{
			component.scatter.len = print_reserve_size(
				::fast_io::io_reserve_type<char_type, arg_type>, arg);
			extent = component.scatter.len;
		}
		else
		{
			// Spell the retained-scatter proof here because the public convenience trait is declared below this planning
			// machinery. This is the same exact named-lvalue expression and independent lifetime/replay contract.
			static_assert(::fast_io::scatter_printable_for<char_type, arg_type &> &&
						  ::fast_io::borrowed_scatter_source<char_type, arg_type>);
			component.scatter = print_scatter_define(
				::fast_io::io_reserve_type<char_type, arg_type>, arg);
			extent = component.scatter.len;
		}
		if (::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(0u, extent) == SIZE_MAX)
			[[unlikely]]
		{
			// One component alone cannot fit a C++ contiguous range. No aggregate or sequential strategy can make
			// its reserve contract or exact scatter length valid, so stop before forming any derived pointer.
			::fast_io::fast_terminate();
		}
		total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(total, extent);
	};
	(measure_one(args), ...);
	return total;
}

/// @brief Emits a cached concat plan either into one exact aggregate allocation or sequentially through one scratch.
/// @details Successful aggregation and the unavailable-plan fallback consume the same cached size/scatter metadata;
///          neither path replays a size or scatter CPO. The grouped path validates every reserve producer's returned
///          cursor against its own cached slice before advancing. If the aggregate is unavailable, one scratch buffer
///          sized to the maximum reserve bound is reused in argument order and only each producer's actual prefix is
///          appended to the growable destination. Scatter descriptors are consumed directly and a newline is appended
///          only after all cached components. This preserves the size-then-define contract and does not depend on a
///          second size/scatter query; retained scatters still carry their independent repeatability proof.
template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_cached_mixed_with_storage(
	::fast_io::details::decay::concat_mixed_component_cache<char_type> *cache, T &str, Args &...args)
{
	::std::size_t total{
		::fast_io::details::decay::concat_measure_mixed_components_once<char_type>(cache, args...)};
	total = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<char_type>(
		total, static_cast<::std::size_t>(line));
	if (total != SIZE_MAX) [[likely]]
	{
		if constexpr (::fast_io::sso_buffer_strlike<char_type, T>)
		{
			constexpr ::std::size_t local_capacity{
				strlike_sso_size(::fast_io::io_strlike_type<char_type, T>)};
			if (local_capacity < total)
			{
				strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
			}
		}
		else
		{
			strlike_reserve(::fast_io::io_strlike_type<char_type, T>, str, total);
		}

		char_type *current{strlike_begin(::fast_io::io_strlike_type<char_type, T>, str)};
		::std::size_t index{};
		auto emit_one = [cache, &current, &index](auto &arg) constexpr {
			using arg_type = ::std::remove_cvref_t<decltype(arg)>;
			auto const &component{cache[index++]};
			if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
						  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
			{
				char_type *const component_last{
					component.scatter.len == 0u ? current : current + component.scatter.len};
				char_type *const result{print_reserve_define(
					::fast_io::io_reserve_type<char_type, arg_type>, current, arg)};
				if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
						current, component_last, result)) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				current = result;
			}
			else
			{
				if (component.scatter.len != 0u)
				{
					current = ::fast_io::details::decay::small_scatter_copy_n(
						component.scatter.base, component.scatter.len, current);
				}
			}
		};
		(emit_one(args), ...);
		if constexpr (line)
		{
			*current++ = ::fast_io::char_literal_v<u8'\n', char_type>;
		}
		strlike_set_curr(::fast_io::io_strlike_type<char_type, T>, str, current);
		return;
	}

	::std::size_t maximum_reserve_bound{};
	::std::size_t index{};
	auto observe_bound = [cache, &maximum_reserve_bound, &index](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		auto const &component{cache[index++]};
		if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
					  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
		{
			if (maximum_reserve_bound < component.scatter.len)
			{
				maximum_reserve_bound = component.scatter.len;
			}
		}
	};
	(observe_bound(args), ...);

	// This branch is selected only for an exact writable-buffer result: the aggregate bound was unavailable, not the
	// result's cursor protocol. Use fast_io's maintained cursor adapter directly. An associated `io_strlike_ref` is not
	// evidence that its result supports primitive writes (it may be an unrelated or malformed customization), and
	// requiring such a customization here made a valid buffer-only result fail only on the rare overflow fallback.
	::fast_io::io_strlike_reference_wrapper<char_type, T> destination{__builtin_addressof(str)};
	auto emit_sequentially = [&](char_type *scratch) constexpr {
		::std::size_t component_index{};
		auto emit_one = [cache, &destination, &component_index, scratch](auto &arg) constexpr {
			using arg_type = ::std::remove_cvref_t<decltype(arg)>;
			auto const &component{cache[component_index++]};
			if constexpr (::fast_io::reserve_printable<char_type, arg_type> ||
						  ::fast_io::dynamic_reserve_printable<char_type, arg_type>)
			{
				char_type *const scratch_last{
					component.scatter.len == 0u ? scratch : scratch + component.scatter.len};
				char_type *const result{print_reserve_define(
					::fast_io::io_reserve_type<char_type, arg_type>, scratch, arg)};
				if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
						scratch, scratch_last, result)) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				::fast_io::operations::decay::write_all_decay(destination, scratch, result);
			}
			else if (component.scatter.len != 0u)
			{
				::fast_io::operations::decay::write_all_decay(
					destination, component.scatter.base, component.scatter.base + component.scatter.len);
			}
		};
		(emit_one(args), ...);
	};

	if (maximum_reserve_bound == 0u)
	{
		char_type scratch{};
		emit_sequentially(__builtin_addressof(scratch));
	}
	else
	{
		// Aggregate unavailability implies that this branch is already exceptional. One reusable dynamic scratch keeps
		// the frame bounded and avoids the sum-of-upper-bounds allocation that this fallback exists to reject.
		::fast_io::details::local_operator_new_array_ptr<char_type> scratch(maximum_reserve_bound);
		emit_sequentially(scratch.ptr);
	}
	if constexpr (line)
	{
		::fast_io::operations::decay::char_put_decay(
			destination, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

/// @brief Applies the shared stack-byte policy to cached mixed-concat metadata.
/// @details A component cache is an optimization frame, not part of the printable protocol. An argument pack may be
///          arbitrarily long, so placing one two-word scatter record per leaf on the stack would make concept
///          composition itself an unbounded frame-size hazard. Small plans retain the allocation-free array; large
///          plans use one exact dynamic metadata allocation and then execute the identical measurement and emission
///          implementation.
template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_cached_mixed(T &str, Args &...args)
{
	using cache_type = ::fast_io::details::decay::concat_mixed_component_cache<char_type>;
	static_assert(sizeof...(Args) != 0u);
	if constexpr (
		::fast_io::details::decay::concat_metadata_stack_safe<sizeof...(Args), cache_type>())
	{
		cache_type cache[sizeof...(Args)]{};
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed_with_storage<line, char_type>(
			cache, str, args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<cache_type> cache(sizeof...(Args));
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed_with_storage<line, char_type>(
			cache.ptr, str, args...);
	}
}

template <::std::integral ch_type, typename T>
inline constexpr basic_io_scatter_t<ch_type> print_scatter_define_extract_one(T &t)
{
	// This helper returns the descriptor to its caller, so it must borrow the phase-1 owner rather than create another
	// formatter copy. A valid borrowed producer may point into itself; returning a descriptor into a by-value local
	// would end that storage lifetime before the string constructor consumes the range.
	return print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<T>>, t);
}

/// @brief Tests the retained byte threshold without multiplying a descriptor length by its character width.
/// @details `ceil(minimum_bytes / sizeof(ch_type))` is the exact minimum character count whose object representation
///          spans the required byte extent. Division and remainder keep the proof valid even when an adversarial
///          descriptor length would overflow `size_t` if multiplied by `sizeof(ch_type)`.
template <::std::integral ch_type>
inline constexpr bool concat_scatter_payload_meets_read_prfch_threshold(::std::size_t length) noexcept
{
	constexpr ::std::size_t character_width{sizeof(ch_type)};
	constexpr ::std::size_t minimum_bytes{
		::fast_io::concat_scatter_chain_read_prfch_minimum_payload_bytes};
	constexpr ::std::size_t minimum_characters{
		minimum_bytes / character_width + static_cast<::std::size_t>(minimum_bytes % character_width != 0u)};
	return minimum_characters <= length;
}

/// @brief Sums an exact scatter prefix and, when admitted, classifies read-prefetch eligibility in the same traversal.
/// @details The strategy-enabled run counts nonempty descriptors up to the retained threshold and rejects the complete
///          chain if any nonempty payload is smaller than four KiB. This conservative all-large rule is intentional:
///          it prevents 32 ordinary 64-byte strings from paying the next-descriptor search whose isolated hot control
///          regressed by about 7--9 percent. No payload base is inspected here. The count saturates at 32 and the byte
///          predicate uses division, so neither statistic can overflow.
///
///          Constant evaluation and compile-time-disabled policies execute the historical summation loop below. They
///          construct no eligibility counters and perform no lookahead; `runtime_read_prfch` remains false. At run time
///          an admitted policy folds the two statistics into the summation that concat already required, avoiding a
///          third descriptor pass before copying.
template <bool read_prfch, ::std::integral ch_type>
inline constexpr ::std::size_t calculate_scatter_total_size(
	basic_io_scatter_t<ch_type> const *first, basic_io_scatter_t<ch_type> const *last,
	bool &runtime_read_prfch)
{
	runtime_read_prfch = false;
	if constexpr (read_prfch)
	{
		if (!::std::is_constant_evaluated())
		{
			::std::size_t total_size{};
			::std::size_t nonempty_count{};
			bool every_nonempty_payload_is_large{true};
			for (; first != last; ++first)
			{
				::std::size_t const length{first->len};
				total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					total_size, length);
				if (total_size == SIZE_MAX) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				if (length != 0u)
				{
					if (nonempty_count < ::fast_io::concat_scatter_chain_read_prfch_minimum_descriptor_count)
					{
						++nonempty_count;
					}
					every_nonempty_payload_is_large =
						every_nonempty_payload_is_large &&
						::fast_io::details::decay::concat_scatter_payload_meets_read_prfch_threshold<ch_type>(
							length);
				}
			}
			runtime_read_prfch =
				every_nonempty_payload_is_large &&
				nonempty_count == ::fast_io::concat_scatter_chain_read_prfch_minimum_descriptor_count;
			return total_size;
		}
	}
	::std::size_t total_size{};
	for (; first != last; ++first)
	{
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
			total_size, first->len);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			// Scatter lengths are exact output, not optional reserve bounds. An unrepresentable exact total cannot be
			// split into one result object, so reject it before allocation or any base-plus-length expression is formed.
			::fast_io::fast_terminate();
		}
	}
	return total_size;
}

/// @brief Copies an exact retained scatter prefix, optionally applying concat's measured next-source read hint.
/// @details The optimized loop is instantiated only after the caller proves platform, descriptor-capacity, and every
///          normalized producer's cacheable-read provenance. The explicit run-time gate comes from the existing size
///          traversal and proves at least 32 nonempty descriptors with every nonempty payload at least four KiB. A
///          false gate therefore enters the original loop without executing a next-descriptor search. When true, empty
///          descriptors are skipped by length before their `base` member is ever evaluated, and the only hinted
///          expression is exactly the next nonempty descriptor's live-range base.
///
///          Constant evaluation deliberately executes the original single traversal: it performs neither a prefetch
///          nor the next-nonempty search. At run time, three Linux P-core seeds retained about 1--2.4 percent for cold
///          discontinuous 4--16 KiB chains and about +/-0.2 percent for the hot controls. Earlier 256/512-byte hot
///          policies regressed by 17--31 percent, which is why this threshold and site remain independent from the
///          broad platform capability and why no write hint is mirrored here.
template <bool read_prfch, ::std::integral ch_type>
inline constexpr ch_type *copy_scatter_chain_to_buffer(ch_type *iter, basic_io_scatter_t<ch_type> const *first,
													   basic_io_scatter_t<ch_type> const *last,
													   bool runtime_read_prfch)
{
	if constexpr (read_prfch)
	{
		if (!::std::is_constant_evaluated() && runtime_read_prfch)
		{
			auto current{first};
			while (current != last && current->len == 0u)
			{
				// A zero-length descriptor carries no live payload premise; in particular, its base may be null.
				++current;
			}
			while (current != last)
			{
				auto next{current + 1u};
				while (next != last && next->len == 0u)
				{
					++next;
				}
				if (next != last)
				{
					::fast_io::prfch<
						::fast_io::prfch_mode::read, ::fast_io::concat_scatter_chain_read_prfch_level,
						::fast_io::concat_scatter_chain_read_prfch_retention>(next->base);
				}
				iter = ::fast_io::details::decay::small_scatter_copy_n(current->base, current->len, iter);
				current = next;
			}
			return iter;
		}
	}
	for (; first != last; ++first)
	{
		if (first->len != 0u)
		{
			// Preserve the same empty-descriptor rule in the baseline and constexpr paths.
			iter = ::fast_io::details::decay::small_scatter_copy_n(first->base, first->len, iter);
		}
	}
	return iter;
}

template <::std::integral ch_type, typename T>
inline constexpr bool direct_scatter_view_printable =
	::std::same_as<::std::remove_cvref_t<T>, basic_io_scatter_t<ch_type>>;

/// @brief Proves that concat may retain a produced scatter while it queries later arguments.
/// @details Multi-scatter concat first collects every descriptor and only then allocates and copies the complete
///          result. That ordering is valid for a raw caller-owned scatter view and for explicitly borrowed producers,
///          but not for an arbitrary scatter CPO backed by shared scratch storage. Unproved producers leave this
///          optimized branch and are emitted sequentially through the ordinary string-output dispatcher, which
///          consumes each descriptor before invoking the next producer. Concat owns normalized arguments once in its
///          phase-1 entry and every lower helper borrows those named objects; `T&` is therefore the exact expression
///          category whose overload must exist. Testing the public forwarding query here would both admit an rvalue-only
///          CPO that the body cannot call and miss a valid lvalue-only producer.
template <::std::integral ch_type, typename T>
inline constexpr bool concat_retained_scatter_printable_v =
	::fast_io::scatter_printable_for<ch_type, T &> &&
	::fast_io::borrowed_scatter_source<ch_type, T>;

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_direct(T &str, Args &...args)
{
	::std::size_t total_size{};
	auto add_exact_length = [&total_size](::std::size_t length) constexpr {
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(total_size, length);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	};
	(add_exact_length(args.len), ...);
	if constexpr (line)
	{
		add_exact_length(1u);
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto ptr{strlike_begin(io_strlike_type<ch_type, T>, str)};
	((ptr = ::fast_io::details::decay::small_scatter_copy_n(args.base, args.len, ptr)), ...);
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage(
	::fast_io::basic_io_scatter_t<ch_type> *scatters, T &str, Args &...args)
{
	constexpr bool read_prfch{
		::fast_io::concat_scatter_chain_read_prfch_strategy<
			::fast_io::details::native_prfch_platform, sizeof...(Args), Args...>};
	::std::size_t scatter_index{};
	((scatters[scatter_index++] =
		  print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>, args)),
	 ...);
	bool runtime_read_prfch{};
	::std::size_t total_size{calculate_scatter_total_size<read_prfch>(
		scatters, scatters + sizeof...(Args), runtime_read_prfch)};
	if constexpr (line)
	{
		total_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(total_size, 1u);
		if (total_size == SIZE_MAX) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto ptr{copy_scatter_chain_to_buffer<read_prfch>(
		strlike_begin(io_strlike_type<ch_type, T>, str), scatters, scatters + sizeof...(Args),
		runtime_read_prfch)};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief Materializes generic retained scatters without making pack length an unbounded stack cost.
/// @details Generic producers must be queried before destination allocation because their descriptors may point into
///          the normalized owner. The descriptor table is therefore necessary, but its storage class is only a cost
///          decision: compact tables stay local while large flattened packs use one exact metadata allocation.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter_generic(T &str, Args &...args)
{
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	if constexpr (
		::fast_io::details::decay::concat_metadata_stack_safe<sizeof...(Args), scatter_type>())
	{
		scatter_type scatters[sizeof...(Args)];
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage<
			line, ch_type>(scatters, str, args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(sizeof...(Args));
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_scatter_generic_with_storage<
			line, ch_type>(scatters.ptr, str, args...);
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter(T &str, Args &...args)
{
	if constexpr ((direct_scatter_view_printable<ch_type, Args> && ...))
	{
		basic_general_concat_decay_ref_impl_all_scatter_direct<line, ch_type>(str, args...);
	}
	else
	{
		basic_general_concat_decay_ref_impl_all_scatter_generic<line, ch_type>(str, args...);
	}
}

template <bool line>
inline constexpr ::std::size_t concat_precise_size_with_line(::std::size_t precise_size);

/// @brief Proves that concat may retain every descriptor produced by one static reserve-scatters leaf.
/// @details The reserve-scatters shape concept proves only compile-time capacities.  The independent borrowed-source
///          marker proves the missing lifetime property: invoking a later producer cannot invalidate descriptors
///          already collected from this object.  Without that marker concat must consume each object immediately.
template <::std::integral ch_type, typename T>
inline constexpr bool concat_retained_reserve_scatters_printable_v =
	::fast_io::reserve_scatters_printable<ch_type, ::std::remove_cvref_t<T>> &&
	::fast_io::details::decay::retained_reserve_scatters_printable_v<
		ch_type, ::std::remove_cvref_t<T>>;

/// @brief Computes the aggregate descriptor and reserve-storage capacities of a retained concat plan.
/// @details Both dimensions use the common contiguous-extent proof.  `SIZE_MAX` is an unavailable-plan sentinel, not
///          an allocation request: it records either arithmetic overflow or a sum that cannot participate in C++
///          array pointer arithmetic.  Individual reserve-scatters concepts already prove constant evaluation.
template <::std::integral ch_type, typename... Args>
inline consteval ::fast_io::reserve_scatters_size_result
concat_retained_reserve_scatters_capacity() noexcept
{
	::fast_io::reserve_scatters_size_result result{};
	((result.scatters_size = ::fast_io::details::decay::print_strategy_extent_add_or_unavailable(
		  result.scatters_size,
		  print_reserve_scatters_size(
			  ::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>)
			  .scatters_size),
	  result.reserve_size = ::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
		  result.reserve_size,
		  print_reserve_scatters_size(
			  ::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>)
			  .reserve_size)),
	 ...);
	return result;
}

/// @brief Selects the retained static reserve-scatters concat plan only when both arrays are representable.
/// @details Descriptor count and descriptor bytes are different bounds.  A count below `PTRDIFF_MAX` can still
///          overflow `count * sizeof(scatter)` on a wide-address target, so byte representability is proved separately
///          before either the stack array or the typed dynamic allocator is instantiated.
template <::std::integral ch_type, typename... Args>
inline constexpr bool concat_retained_reserve_scatters_run_v = []() consteval {
	if constexpr (sizeof...(Args) != 0u &&
				  (::fast_io::details::decay::concat_retained_reserve_scatters_printable_v<
					   ch_type, Args> &&
				   ...))
	{
		using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
		constexpr auto capacity{
			::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
		return capacity.scatters_size != SIZE_MAX && capacity.reserve_size != SIZE_MAX &&
			   capacity.scatters_size <= SIZE_MAX / sizeof(scatter_type) &&
			   capacity.reserve_size <= SIZE_MAX / sizeof(ch_type);
	}
	else
	{
		return false;
	}
}();

/// @brief Proves that both exact plan arrays fit in concat's bounded hot stack frame.
/// @details Four KiB is a cost ceiling, while the configurable print stack policy is a safety ceiling; the smaller
///          value controls this plan.  Division precedes multiplication, proving the byte calculation cannot wrap.
///          A zero reserve capacity still needs one sentinel character because producers may compare their cursor.
template <::std::integral ch_type, typename... Args>
inline consteval bool concat_retained_reserve_scatters_stack_safe() noexcept
{
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	constexpr auto capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	constexpr ::std::size_t configured_budget{
		::fast_io::details::decay::print_stack_buffer_max_bytes()};
	constexpr ::std::size_t preferred_budget{4u * 1024u};
	constexpr ::std::size_t budget{
		configured_budget < preferred_budget ? configured_budget : preferred_budget};
	if constexpr (budget == 0u || capacity.scatters_size > budget / sizeof(scatter_type))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t scatter_bytes{capacity.scatters_size * sizeof(scatter_type)};
		constexpr ::std::size_t reserve_slots{
			capacity.reserve_size == 0u ? 1u : capacity.reserve_size};
		return reserve_slots <= (budget - scatter_bytes) / sizeof(ch_type);
	}
}

/// @brief Tests whether a producer cursor lies in the closed range of its declared component slice.
/// @details Equality with `last` is valid because reserve-scatters returns one-past cursors.  At run time integer
///          addresses avoid subtracting an untrusted out-of-range pointer; the stride test additionally rejects a
///          forged descriptor cursor that points into the middle of an array element.  Constant evaluation uses the
///          language's same-array pointer ordering, which is valid for every conforming customization.
template <typename element_type>
inline constexpr bool concat_reserve_scatters_cursor_in_closed_range(
	element_type *first, element_type *last, element_type *cursor) noexcept
{
	if (__builtin_is_constant_evaluated())
	{
		return first <= cursor && cursor <= last;
	}
	auto const first_address{reinterpret_cast<::std::uintptr_t>(first)};
	auto const last_address{reinterpret_cast<::std::uintptr_t>(last)};
	auto const cursor_address{reinterpret_cast<::std::uintptr_t>(cursor)};
	return first_address <= cursor_address && cursor_address <= last_address &&
		   (cursor_address - first_address) % sizeof(element_type) == 0u;
}

/// @brief Materializes one retained plan and copies its actual descriptor prefix into a string destination.
/// @details Each producer is invoked exactly once.  Its returned cursors are validated against that producer's own
///          capacity slice before they become the starting cursors for the next producer.  This induction proves that
///          later producers receive disjoint remaining storage and that cursor chaining stays inside the aggregate
///          arrays. Descriptor liveness through the final copy is a separate premise, supplied by
///          `concat_retained_reserve_scatters_printable_v` and its borrowed-source marker. Payload lengths and the
///          optional newline are then checked before one destination reserve.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized(
	T &str, ::fast_io::basic_io_scatter_t<ch_type> *scatters, ch_type *reserve, Args &...args)
{
	constexpr auto aggregate_capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	constexpr bool read_prfch{
		::fast_io::concat_scatter_chain_read_prfch_strategy<
			::fast_io::details::native_prfch_platform, aggregate_capacity.scatters_size, Args...>};
	auto scatter_curr{scatters};
	auto reserve_curr{reserve};
	auto materialize_one = [&scatter_curr, &reserve_curr](auto &arg) constexpr {
		using arg_type = ::std::remove_cvref_t<decltype(arg)>;
		constexpr auto component_capacity{
			print_reserve_scatters_size(::fast_io::io_reserve_type<ch_type, arg_type>)};
		auto scatter_first{scatter_curr};
		auto reserve_first{reserve_curr};
		auto const result{print_reserve_scatters_define(
			::fast_io::io_reserve_type<ch_type, arg_type>, scatter_first, reserve_first, arg)};
		if (!::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
				scatter_first, scatter_first + component_capacity.scatters_size,
				result.scatters_pos_ptr) ||
			!::fast_io::details::decay::concat_reserve_scatters_cursor_in_closed_range(
				reserve_first, reserve_first + component_capacity.reserve_size,
				result.reserve_pos_ptr)) [[unlikely]]
		{
			// A customization that reports a cursor outside its advertised slice has already broken the storage contract;
			// terminating here prevents that pointer from participating in subtraction, iteration, or a later CPO call.
			::fast_io::fast_terminate();
		}
		scatter_curr = result.scatters_pos_ptr;
		reserve_curr = result.reserve_pos_ptr;
	};
	(materialize_one(args), ...);

	bool runtime_read_prfch{};
	::std::size_t const payload_size{
		::fast_io::details::decay::calculate_scatter_total_size<read_prfch>(
			scatters, scatter_curr, runtime_read_prfch)};
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{
			strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(::fast_io::io_strlike_type<ch_type, T>, str, total_size);
	}
	auto iter{::fast_io::details::decay::copy_scatter_chain_to_buffer<read_prfch>(
		strlike_begin(::fast_io::io_strlike_type<ch_type, T>, str), scatters, scatter_curr,
		runtime_read_prfch)};
	if constexpr (line)
	{
		*iter++ = ::fast_io::char_literal_v<u8'\n', ch_type>;
	}
	strlike_set_curr(::fast_io::io_strlike_type<ch_type, T>, str, iter);
}

/// @brief Owns the bounded descriptor/scratch storage for one retained reserve-scatters concat run.
/// @details Small plans use exact-size arrays whose combined frame is proven below four KiB.  Larger plans move both
///          arrays to isolated dynamic storage; zero reserve capacity uses a live one-character sentinel and performs
///          no pointless allocation.  In every case storage outlives descriptor summation and the destination copy.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_reserve_scatters(T &str, Args &...args)
{
	static_assert(
		::fast_io::details::decay::concat_retained_reserve_scatters_run_v<ch_type, Args...>);
	using scatter_type = ::fast_io::basic_io_scatter_t<ch_type>;
	constexpr auto capacity{
		::fast_io::details::decay::concat_retained_reserve_scatters_capacity<ch_type, Args...>()};
	if constexpr (
		::fast_io::details::decay::concat_retained_reserve_scatters_stack_safe<ch_type, Args...>())
	{
		scatter_type scatters[capacity.scatters_size];
		if constexpr (capacity.reserve_size == 0u)
		{
			ch_type reserve{};
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters, __builtin_addressof(reserve), args...);
		}
		else
		{
			ch_type reserve[capacity.reserve_size];
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters, reserve, args...);
		}
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(capacity.scatters_size);
		if constexpr (capacity.reserve_size == 0u)
		{
			ch_type reserve{};
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters.ptr, __builtin_addressof(reserve), args...);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<ch_type> reserve(capacity.reserve_size);
			::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters_materialized<
				line, ch_type>(str, scatters.ptr, reserve.ptr, args...);
		}
	}
}

/// @brief Adds concat's optional newline while preserving a representable contiguous pointer range.
/// @details Precise-size CPOs are run-time customization points and may return any `size_t`.  Checked addition prevents
///          `SIZE_MAX + 1` from wrapping to an undersized allocation, while the `PTRDIFF_MAX` bound proves that both
///          `first + size` and the later begin/end difference are representable for one C++ array object.
template <bool line>
inline constexpr ::std::size_t concat_precise_size_with_line(::std::size_t precise_size)
{
	::std::size_t const total_size{::fast_io::details::intrinsics::add_or_overflow_die(
		precise_size, static_cast<::std::size_t>(line))};
	if (static_cast<::std::size_t>(PTRDIFF_MAX) < total_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return total_size;
}

/// @brief Caps the ordinary exact-resize plan's sizing table and per-call template expansion.
/// @details Each leaf needs one cached `size_t` so its exact size is not recomputed during emission. Sixteen leaves
///          keep that table at 128 bytes on a 64-bit ABI and cover the measured 2/4/8/12/16 composition shapes. The
///          optional newline does not consume a table entry: it contributes one checked character to the aggregate
///          size and one final store after every measured slice. An isolated GCC 15 x86-64 benchmark specialization
///          added only eight instructions at the sixteen-leaf line boundary, while paired run-time measurements kept
///          exact resize profitable for both line modes. A smaller line-only cap would therefore preserve more staging
///          work without removing a demonstrated per-leaf cost. Beyond sixteen leaves, the ordinary destination
///          dispatcher remains the bounded code-size policy: unrolling another sizing and checked-writer step per leaf
///          is not accepted without separate performance evidence. Semantic packs have their own shape-aware threshold
///          and do not use this flat-run limit.
inline constexpr ::std::size_t basic_general_concat_precise_resize_leaf_limit{16u};

/// @brief Selects the bounded all-precise strategy for an already-normalized ordinary leaf run.
/// @details This predicate proves only source shape and cost. Destination exact-resize capability is deliberately a
///          separate requirement on the implementation so an argument concept cannot accidentally imply writable
///          storage for a result type.
template <::std::integral char_type, typename... Args>
inline constexpr bool basic_general_concat_precise_resize_run_v = []() consteval {
	if constexpr (
		sizeof...(Args) == 0u ||
		::fast_io::details::decay::basic_general_concat_precise_resize_leaf_limit < sizeof...(Args))
	{
		// Rejecting the shape before forming the capability fold also bounds constraint-instantiation work for very
		// large packs; a disabled cost plan has no reason to probe every leaf's ADL surface.
		return false;
	}
	else
	{
		return (::fast_io::precise_reserve_printable<char_type, Args> && ...);
	}
}();

/// @brief Pairs an ordinary precise leaf run with the exact-resize cost of its final destination.
/// @details Source exactness and destination writable lifetime remain the base capability proof. A leaf may additionally
///          state that value-initializing the final extent is a measured extra pass; such a run is admitted only when
///          the destination explicitly creates that live extent without initialization. This is deliberately consumed
///          only by the flat ordinary strategy. Semantic concat can eliminate staging for width/condition/pack graphs
///          and therefore retains its independent whole-output decision. The source-shape test is performed first so a
///          pack rejected by the bounded leaf policy does not instantiate unrelated source or destination ADL probes.
template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_precise_resize_destination_run_v = []() consteval {
	if constexpr (!::fast_io::details::decay::basic_general_concat_precise_resize_run_v<
					  char_type, Args...>)
	{
		return false;
	}
	else if constexpr (
		(::fast_io::precise_resize_initialization_sensitive_printable<char_type, Args> || ...))
	{
		return ::fast_io::precise_resize_without_initialization_strlike<char_type, T>;
	}
	else
	{
		return ::fast_io::precise_resize_writable_strlike<char_type, T>;
	}
}();

/// @brief Admits one initialization-sensitive exact source to C++23 callback-owned string construction.
/// @details The ordinary exact-resize strategy permits a throwing formatter because its local result is destroyed if
///          emission fails. `std::basic_string::resize_and_overwrite` has a different callback boundary, so this
///          narrower strategy requires the exact named-lvalue define expression itself to be `noexcept` and to report
///          its actual endpoint. The initialization-sensitive marker is a cost proof: without it, the established
///          scatter/static-reserve priorities already avoid the extra destination-initialization pass this strategy is
///          designed to remove. Restricting the shape to one normalized leaf preserves the measured range-view case
///          without introducing a component-size table or an unbounded callback expansion.
template <typename char_type, typename Arg>
concept basic_general_concat_exact_overwrite_source =
	::std::integral<char_type> &&
	::fast_io::precise_resize_initialization_sensitive_printable<char_type, Arg> &&
	::fast_io::nothrow_precise_reserve_printable<char_type, Arg>;

/// @brief Writes one proved exact source into storage owned by a resize-and-overwrite destination.
/// @details `concat_precise_size_with_line` has already proved `total_size <= PTRDIFF_MAX`, and `payload_size` is no
///          greater than that total. Consequently the guarded endpoint addition stays inside the callback's writable
///          array. The source cursor is compared with that exact endpoint before either the newline or logical size is
///          published. The precise-reserve contract, rather than this post-call cursor comparison, proves that the
///          producer confines its writes to `[buffer, expected_end)`; the comparison validates only the endpoint it
///          reports and cannot retroactively detect an already out-of-bounds write by a broken customization. A short
///          callback extent is a broken destination contract and terminates before any write. The source concept proves
///          that the only user-extensible expression executed here cannot throw; pointer arithmetic, comparison, the
///          optional character store, and `fast_terminate` are non-throwing operations.
template <bool line, ::std::integral char_type, typename Arg>
	requires ::fast_io::details::decay::basic_general_concat_exact_overwrite_source<char_type, Arg>
struct basic_general_concat_exact_overwrite_operation
{
	Arg *argument;
	::std::size_t payload_size;
	::std::size_t total_size;

	inline constexpr ::std::size_t operator()(char_type *buffer, ::std::size_t writable_size) const noexcept
	{
		// Keeping both extents is intentional even though it makes this state three words on LP64. A two-word variant was
		// motivated by the SysV AMD64/AAPCS64 small-aggregate register classes, not by a cross-ABI theorem: MS x64,
		// RISC-V, PPC64, and ILP32 targets classify aggregates differently. In the measured GCC 15/libstdc++ build the
		// complete callback was inlined, so no by-value ABI transfer remained; reconstruction instead enlarged the hot line
		// specialization and regressed the 128-element E-core case by about 6.7 percent. Passing the already checked values
		// avoids that optimizer cliff. Aggregate-size decay heuristics were therefore not predictive for this measured
		// inlined specialization; other ABI/library/compiler combinations remain governed by their own code generation.
		if (writable_size < total_size) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		char_type *expected_end{buffer};
		if (payload_size != 0u)
		{
			expected_end += payload_size;
		}
		using arg_type = ::std::remove_cvref_t<Arg>;
		char_type *const actual_end{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, buffer, payload_size, *argument)};
		if (actual_end != expected_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		if constexpr (line)
		{
			*expected_end = ::fast_io::char_literal_v<u8'\n', char_type>;
		}
		return total_size;
	}
};

/// @brief Pairs the exact source operation with the concrete destination callback CPO.
/// @details Testing the final operation type, rather than only the destination marker, closes the strategy/body gap:
///          a destination cannot opt in with a marker while rejecting the callback concat actually passes.
template <bool line, ::std::integral char_type, typename T, typename Arg>
inline constexpr bool basic_general_concat_exact_overwrite_run_v = []() constexpr {
	if constexpr (!line)
	{
		// Without a newline, the retained-scatter fallback traverses this range once into concat's 2-KiB inline staging
		// area and lets basic_string bulk-copy the completed bytes. Exact overwrite replaces that bulk copy with a full
		// sizing traversal. Formal paired P-core measurements found a 1.33-percent regression at 128 elements,
		// while E-core results moved in the opposite direction; there is therefore no portable cost proof for selecting
		// the callback path. The line case has independent evidence because it also fuses newline publication and avoids
		// the substantially larger legacy line helper. Keep this policy in the concept-level predicate so every caller
		// observes the same admission decision rather than duplicating a platform heuristic in the dispatch body.
		return false;
	}
	else if constexpr (!::fast_io::details::decay::basic_general_concat_exact_overwrite_source<
					  char_type, Arg>)
	{
		return false;
	}
	else
	{
		using operation = ::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
			line, char_type, Arg>;
		return ::fast_io::exact_resize_and_overwrite_strlike_for<char_type, T, operation>;
	}
}();

/// @brief Constructs the final string with one sizing pass and one direct overwrite pass.
/// @details Sizing remains outside the callback and may throw normally. Only after a representable final extent is
///          known is the local destination constructed and asked to allocate. The callback then writes directly into
///          that allocation, avoiding both value-initialization of the final range and concat's descriptor/staging
///          string. If allocation throws, the local result owns no published partial value; after callback entry, the
///          source and endpoint proofs above make the operation non-throwing.
template <bool line, ::std::integral char_type, typename T, typename Arg>
	requires ::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
		line, char_type, T, Arg>
inline constexpr T basic_general_concat_exact_overwrite_run(Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const payload_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, arg_type>, arg)};
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	using operation = ::fast_io::details::decay::basic_general_concat_exact_overwrite_operation<
		line, char_type, Arg>;
	operation overwrite{__builtin_addressof(arg), payload_size, total_size};
	T result;
	strlike_exact_resize_and_overwrite(
		::fast_io::io_strlike_type<char_type, T>, result, total_size, overwrite);
	return result;
}

template <::std::integral char_type, typename Arg>
inline constexpr void
basic_general_concat_precise_resize_measure_one(
	::std::size_t *component_sizes, ::std::size_t &payload_size,
	::std::size_t &component_index, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const component_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, arg_type>, arg)};
	component_sizes[component_index++] = component_size;
	payload_size = ::fast_io::details::intrinsics::add_or_overflow_die(
		payload_size, component_size);
}

template <::std::integral char_type, typename Arg>
inline constexpr void
basic_general_concat_precise_resize_emit_one(
	::std::size_t const *component_sizes, ::std::size_t &component_index,
	char_type *&current, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t const component_size{component_sizes[component_index++]};
	char_type *expected_end{current};
	if (component_size != 0u)
	{
		expected_end += component_size;
	}
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, arg_type>, current,
		component_size, arg));
	if constexpr (::std::same_as<define_result, char_type *>)
	{
		char_type *const actual_end{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, current,
			component_size, arg)};
		if (actual_end != expected_end) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
	else
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, arg_type>, current,
			component_size, arg);
	}
	current = expected_end;
}

/// @brief Constructs a non-buffer string-like result directly in one exactly resized logical range.
/// @details Exact source sizing and destination storage are orthogonal proofs: every
///          `precise_reserve_printable` supplies one component extent, while `precise_resize_writable_strlike` creates
///          live characters without pretending that spare capacity is a put area. Arguments are accepted by reference
///          because phase 1 already owns their decayed transports; copying the graph again could change an identity-
///          sensitive formatter or retain a reference to a temporary proxy.
///
///          Measurement is left-to-right. Each extent is cached and added with checked arithmetic before resize, and
///          `concat_precise_size_with_line` proves the complete allocation is a representable C++ array range. Emission
///          is then left-to-right into disjoint adjacent slices. A pointer-returning writer is checked against its own
///          promised slice end before the next writer runs; a void writer advances only by the exact protocol contract.
///          This induction proves that no writer can redirect a later writer or make concat publish an incorrect end.
///          The strategy invokes no scatter producer and therefore needs no descriptor-retention or borrowed-lifetime
///          assumption.
///
///          Resize publishes the final size only inside the local result, so no `strlike_set_curr` operation is needed.
///          A sizing exception occurs before resize or emission. A resize or writer exception propagates normally; the
///          local result owns any live, partially written characters and is destroyed before it can escape. C++23
///          `resize_and_overwrite` is not used because an arbitrary formatting CPO is permitted to throw.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
		char_type, T, Args...>
inline constexpr T
basic_general_concat_precise_resize_run(Args &...args)
{
	T result;
	::std::size_t component_sizes[sizeof...(Args)]{};
	::std::size_t payload_size{};
	::std::size_t component_index{};
	(::fast_io::details::decay::basic_general_concat_precise_resize_measure_one<
		 char_type>(component_sizes, payload_size, component_index, args),
	 ...);
	::std::size_t const total_size{
		::fast_io::details::decay::concat_precise_size_with_line<line>(payload_size)};
	char_type *const first{strlike_precise_resize_and_get_begin(
		::fast_io::io_strlike_type<char_type, T>, result, total_size)};

	component_index = 0u;
	char_type *current{first};
	(::fast_io::details::decay::basic_general_concat_precise_resize_emit_one<
		 char_type>(component_sizes, component_index, current, args),
	 ...);
	if constexpr (line)
	{
		*current = ::fast_io::char_literal_v<u8'\n', char_type>;
	}
	return result;
}

template <bool line, ::std::integral ch_type, typename T, typename Arg>
inline constexpr void basic_general_concat_decay_ref_impl_precise(T &str, Arg &arg)
{
	using arg_type = ::std::remove_cvref_t<Arg>;
	::std::size_t precise_size{
		print_reserve_precise_size(io_reserve_type<ch_type, arg_type>, arg)};
	::std::size_t const precise_size_with_line{
		::fast_io::details::decay::concat_precise_size_with_line<line>(precise_size)};
	constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
	if (local_cap < precise_size_with_line)
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size_with_line);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	print_reserve_precise_define(io_reserve_type<ch_type, arg_type>, first, precise_size, arg);
	auto ptr{first + precise_size};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief  Maximum semantic leaf count for which concat performs a separate exact-size traversal.
/// @details Benchmarked long packs are faster when a static reserve bound drives one-pass materialization. The strict
///          threshold keeps compact packs on exact allocation while routing eight-leaf and larger packs to bounded
///          generation, whose returned cursor supplies the actual constructed length.
inline constexpr ::std::size_t basic_general_concat_precise_leaf_threshold{
	::fast_io::details::decay::print_semantic_precise_materialization_leaf_threshold};

/// @brief    Counts the maximum semantic leaves in a concat argument list.
/// @tparam   Args the concat argument types
template <typename... Args>
inline constexpr ::std::size_t basic_general_concat_semantic_leaf_count =
	::fast_io::details::decay::print_semantic_leaf_count_sum<Args...>();

/// @brief    Tests whether every concat argument supplies a compile-time semantic upper bound.
/// @tparam   ch_type the concat character type
/// @tparam   Args    the concat argument types
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_static_bounded =
	(::fast_io::details::decay::print_semantic_static_bounded_size<ch_type, ::std::remove_cvref_t<Args>>::available &&
	 ...);

/// @brief    Selects exact-size semantic concat for compact compositions that can consume precise sizing.
/// @details  Long statically bounded packs deliberately bypass this path so bounded materialization can generate each
///           integer once. Compact conditions, width packs, and mixed packs retain exact allocation when it reduces
///           allocation size or output operations.
/// @tparam   ch_type the concat character type
/// @tparam   Args    the concat argument types
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_precise_ok =
	(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::details::decay::print_semantic_precise_size_ok<ch_type, Args &>::value && ...) &&
	(::fast_io::details::decay::basic_general_concat_semantic_leaf_count<Args...> <
		 ::fast_io::details::decay::basic_general_concat_precise_leaf_threshold ||
	 !::fast_io::details::decay::basic_general_concat_semantic_static_bounded<ch_type, Args...>);

/// @brief Selects one-pass upper-bound materialization for semantic runs that are not precisely measurable.
/// @details A width node around an ordinary reserve formatter has an object-dependent field extent but no precise
///          child protocol.  Treating that node as a generic destination fallback loses the semantic boundary on
///          some string adapters.  The bounded semantic engine already proves both capacity and returned-cursor
///          correctness, so concat can reserve the run-time bound once and construct exactly the produced prefix.
template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_bounded_ok =
	(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::details::decay::print_semantic_bounded_size_ok<ch_type, Args &>::value && ...);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl(T &str, Args &...args);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_impl(Args... args);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_ref_impl(Args &...args);

/// @brief Proves that the normalized fallback run can write directly into the result object's string adapter.
/// @details A construct-only string-like type has no `io_strlike_ref`, while append-oriented and writable-buffer
///          results do. Keeping that distinction in a SFINAE-friendly value lets non-buffer results avoid an internal
///          materialize-and-copy cycle when their real adapter accepts the complete run. `io_strlike_ref` is already
///          the string protocol's normalized output reference, so the proof deliberately tests that exact unprojected
///          type used by the body; reopening `output_stream_ref` here would admit an adapter object which the body never
///          invokes. Character-domain equality is independent evidence: a writable associated ref for another domain
///          cannot hide the maintained generic adapter for `ch_type`.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_direct_destination_ok = []() constexpr {
	if constexpr (requires(T &str) { io_strlike_ref(::fast_io::io_alias, str); })
	{
		using destination_type = ::std::remove_cvref_t<decltype(
			io_strlike_ref(::fast_io::io_alias, ::std::declval<T &>()))>;
		if constexpr (requires { typename destination_type::output_char_type; })
		{
			return ::std::same_as<ch_type, typename destination_type::output_char_type> &&
				   ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
					   line, destination_type, Args...>;
		}
		else
		{
			// Naming `io_strlike_ref` alone is not a complete output capability. Keep a malformed or unrelated associated
			// CPO inside the false branch so an independently valid generic writable-buffer adapter remains selectable.
			return false;
		}
	}
	else
	{
		return false;
	}
}();

/// @brief Proves a fallback run against fast_io's maintained adapter for an exact writable-buffer result.
/// @details `buffer_strlike` already supplies the cursor protocol needed by `io_strlike_reference_wrapper`; requiring a
///          separate user `io_strlike_ref` adds no semantic evidence and rejects otherwise complete buffer-only result
///          types. This predicate is deliberately separate from the custom-adapter proof above because an associated
///          adapter may have destination-specific direct CPOs which the generic cursor adapter neither gains nor hides.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_generic_buffer_destination_ok = []() constexpr {
	if constexpr (::fast_io::buffer_strlike<ch_type, T>)
	{
		using destination_type = ::fast_io::io_strlike_reference_wrapper<ch_type, T>;
		using destination_reference = decltype(
			::fast_io::operations::output_stream_ref(::std::declval<destination_type>()));
		return ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			line, destination_reference, Args...>;
	}
	else
	{
		return false;
	}
}();

/// @brief Requires one contiguous staging construction when exact resize would duplicate an initialization pass.
/// @details An initialization-sensitive ordinary precise run has already proved that every output character can be
///          measured before emission. If the result cannot establish that exact live range without first writing it,
///          the exact-resize plan correctly declines. Falling through to an append adapter would nevertheless turn the
///          same producer into one destination write per fragment (for example, every range element and separator),
///          which loses the reason for measuring the run and was substantially slower for long range views. Staging
///          instead performs the established measure/write passes into one contiguous growable buffer and constructs
///          the result once from the completed range. This predicate is a profitability rule only: the direct and
///          staging predicates below remain the capability and lifetime proofs. The ordinary-run test is deliberately
///          evaluated first, both to preserve unrelated direct-output behavior and to avoid probing marker CPOs for a
///          source shape that the bounded exact strategy has already rejected.
template <::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_initialization_sensitive_staging_required_v = []() consteval {
	if constexpr (!::fast_io::details::decay::basic_general_concat_precise_resize_run_v<
					  ch_type, Args...>)
	{
		return false;
	}
	else
	{
		return
			(::fast_io::precise_resize_initialization_sensitive_printable<ch_type, Args> || ...) &&
			!::fast_io::precise_resize_without_initialization_strlike<ch_type, T>;
	}
}();

/// @brief Proves the alternate construct-only fallback against concat's actual staging stream.
/// @details Non-buffer results that cannot accept direct output are formed from an internal `basic_concat_buffer`.
///          This is a distinct destination from the final string adapter; treating the two as interchangeable admits
///          destination-specific `print_define` overloads that fail only after phase selection. The paired direct and
///          staging predicates are therefore the complete destination-disjunction for non-buffer concat fallback.
template <bool line, ::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_staging_destination_ok = []() constexpr {
	using staging_type = ::fast_io::details::basic_concat_buffer<ch_type>;
	using destination_type = decltype(io_strlike_ref(::fast_io::io_alias, ::std::declval<staging_type &>()));
	using destination_reference = decltype(::fast_io::operations::output_stream_ref(::std::declval<destination_type>()));
	return ::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
		line, destination_reference, Args...>;
}();

/// @brief Recognizes an explicit destination cost-policy opt-in for single-pass context staging.
/// @details `print_context_static_buffer_size` bounds one refill window, not the complete output, so it cannot prove an
///          exact destination reserve.  A destination may instead return `std::true_type` from
///          `concat_context_staging_preferred(io_strlike_type<ch_type, T>)` when formatting once into concat's inline
///          buffer and then range-constructing `T` is cheaper than growing `T` directly.  Exact return-type matching
///          makes this an affirmative policy decision; structural string capabilities alone never enable the copy.
template <typename ch_type, typename T>
concept basic_general_concat_context_staging_preferred_destination =
	::std::integral<ch_type> && ::fast_io::range_constructible_strlike<ch_type, T> && requires {
		{
			concat_context_staging_preferred(::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Recognizes an expensive leaf whose exact-size protocol would repeat substantial formatting work.
/// @details This is an explicit source cost policy, not an inference from `dynamic_reserve_printable`: arbitrary dynamic
///          producers may be cheap, stateful, or deliberately optimized for exact sizing. A width semantic node is the
///          one non-dynamic representation admitted here because it supplies its own exact marker/bound pair and the
///          semantic emitter writes it directly. Exact `true_type` matching keeps an unrelated same-named customization
///          from silently changing concat's allocation strategy.
template <typename ch_type, typename T>
concept basic_general_concat_single_pass_bounded_source =
	::std::integral<ch_type> &&
	(::fast_io::dynamic_reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	 ::fast_io::details::decay::print_semantic_width_v<
		 ::std::remove_cvref_t<T>>) && requires(T &value) {
	{
		concat_single_pass_bounded_materialization_preferred(
			::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<T>>)
	} -> ::std::same_as<::std::true_type>;
	{
		concat_single_pass_bounded_materialization_size(
			::fast_io::io_reserve_type<ch_type, ::std::remove_cvref_t<T>>,
			value, ::std::size_t{})
	} noexcept -> ::std::same_as<::std::size_t>;
};

/// @brief Recognizes a fresh construct-only result whose cost policy prefers one bounded staging pass.
/// @details A writable-buffer destination already owns a direct reserve strategy and is excluded. The destination CPO
///          additionally limits this optimization to string implementations whose construction and allocation model
///          has been audited; range construction alone is not a profitability proof.
template <typename ch_type, typename T>
concept basic_general_concat_single_pass_bounded_destination =
	::std::integral<ch_type> && !::fast_io::buffer_strlike<ch_type, T> &&
	::fast_io::range_constructible_strlike<ch_type, T> && requires {
		{
			concat_single_pass_bounded_materialization_preferred(
				::fast_io::io_strlike_type<ch_type, T>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Caps one-pass concat output staging independently of the library-wide maximum stack policy.
/// @details One KiB matches the semantic emitter's existing inline-frame ceiling and covers both compact records and
///          measured 600-digit precision without turning a source marker into an unbounded caller-frame request. Wider
///          character domains receive the corresponding whole-element count, while a stricter configured print budget
///          remains authoritative.
template <::std::integral ch_type>
inline consteval ::std::size_t basic_general_concat_single_pass_bounded_stack_size() noexcept
{
	constexpr ::std::size_t preferred_bytes{1024u};
	constexpr ::std::size_t preferred_size{preferred_bytes / sizeof(ch_type)};
	constexpr ::std::size_t configured_size{
		::fast_io::details::decay::print_stack_buffer_max_size<ch_type>()};
	return configured_size < preferred_size ? configured_size : preferred_size;
}

/// @brief Selects the bounded frame from the normalized run shape.
/// @details A single expensive leaf may use the full one-KiB ceiling, which covers measured large-precision scalar
///          formatting without duplicating its conversion. Multi-leaf records use 512 bytes: their common result is
///          compact, and this smaller frame remains inside GCC's profitable inline threshold.
template <::std::integral ch_type, typename... Args>
inline consteval ::std::size_t basic_general_concat_single_pass_bounded_run_stack_size() noexcept
{
	constexpr ::std::size_t maximum_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_stack_size<ch_type>()};
	if constexpr (sizeof...(Args) == 1u)
	{
		return maximum_size;
	}
	else
	{
		constexpr ::std::size_t preferred_size{512u / sizeof(ch_type)};
		return maximum_size < preferred_size ? maximum_size : preferred_size;
	}
}

/// @brief Proves that one component can participate without tentatively consuming a stateful object query.
/// @details An explicitly marked source promises a repeatable cheap dynamic bound. Every other admitted component has a
///          compile-time reserve extent or is the final normalized scatter descriptor itself. In particular, borrowed
///          scatter producers and unrelated dynamic-reserve CPOs are not admitted merely because another leaf is marked:
///          a rejected stack attempt must never replay their object-dependent observation in the fallback strategy.
template <::std::integral ch_type, typename T>
inline constexpr bool basic_general_concat_single_pass_bounded_component_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T> ||
	::fast_io::reserve_printable<ch_type, ::std::remove_cvref_t<T>> ||
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::basic_io_scatter_t<ch_type>>;

/// @brief Selects the narrow one-pass bounded construction strategy after concat normalization.
/// @details The 32-leaf ceiling bounds template expansion and covers the measured 21-leaf formatted mixed record. At
///          least one explicitly expensive leaf supplies the reason to replace exact sizing or incremental destination
///          growth, and every companion must pass the no-tentative-object-query proof above. Ordinary integer/string
///          runs therefore retain their existing strategy and code generation.
template <::std::integral ch_type, typename T, typename... Args>
inline consteval bool basic_general_concat_single_pass_bounded_run() noexcept
{
	if constexpr (
		sizeof...(Args) == 0u || 32u < sizeof...(Args) ||
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			ch_type, Args...>() == 0u ||
		!::fast_io::details::decay::basic_general_concat_single_pass_bounded_destination<ch_type, T>)
	{
		return false;
	}
	else
	{
		return
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, Args> || ...) &&
			(::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<ch_type, Args> && ...);
	}
}

template <::std::integral ch_type, typename T, typename... Args>
inline constexpr bool basic_general_concat_single_pass_bounded_run_v =
	::fast_io::details::decay::basic_general_concat_single_pass_bounded_run<
		ch_type, T, Args...>();

/// @brief Returns one component's cheap upper bound under the strategy's repeatability proof.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
		ch_type, T>
inline constexpr ::std::size_t basic_general_concat_single_pass_bounded_component_size(
	T &value, ::std::size_t maximum_size)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_source<ch_type, T>)
	{
		return concat_single_pass_bounded_materialization_size(
			::fast_io::io_reserve_type<ch_type, value_type>, value, maximum_size);
	}
	else if constexpr (::fast_io::reserve_printable<ch_type, value_type>)
	{
		return print_reserve_size(::fast_io::io_reserve_type<ch_type, value_type>);
	}
	else
	{
		return value.len;
	}
}

/// @brief Adds one component to a still-viable bounded run.
/// @details `total` includes every preceding leaf (and the optional line feed), so a marked source receives exactly the
///          remaining budget. Once a predecessor rejects the plan, later candidate CPOs are not observed at all. This
///          keeps optional strategy probing narrower than fallback emission and makes the limit contract compositional.
template <::std::integral ch_type, typename T>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
		ch_type, T>
inline constexpr void basic_general_concat_single_pass_bounded_add_component(
	::std::size_t &total, ::std::size_t maximum_size, T &value)
{
	if (total == SIZE_MAX || maximum_size < total)
	{
		total = SIZE_MAX;
		return;
	}
	auto const remaining{maximum_size - total};
	auto const component_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_size<ch_type>(
			value, remaining)};
	if (component_size == SIZE_MAX || remaining < component_size)
	{
		total = SIZE_MAX;
		return;
	}
	total += component_size;
}

/// @brief Computes the complete cheap bound without invoking an unmarked object-dependent CPO.
template <bool line, ::std::integral ch_type, typename... Args>
	requires(
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_component_v<
			ch_type, Args> && ...)
inline constexpr ::std::size_t basic_general_concat_single_pass_bounded_total_size(Args &...args)
{
	constexpr ::std::size_t maximum_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			ch_type, Args...>()};
	::std::size_t total{line ? 1u : 0u};
	(::fast_io::details::decay::basic_general_concat_single_pass_bounded_add_component<ch_type>(
		total, maximum_size, args),
	 ...);
	return total;
}

/// @brief Emits one already-bounded normalized run once, then constructs the final result from its actual prefix.
/// @details Formatting occurs outside any destination callback, so a throwing CPO unwinds normally and cannot cross a
///          `resize_and_overwrite` boundary. The caller has checked `bounded_size <= stack_size`; validating the returned
///          cursor before construction prevents an invalid endpoint from becoming an observable range. As with every
///          reserve protocol, the declared bound is the producer's proof that writes themselves stay inside the slice.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
		ch_type, T, Args...>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr T basic_general_concat_single_pass_bounded_construct(
	::std::size_t bounded_size, Args &...args)
{
	constexpr ::std::size_t stack_size{
		::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
			ch_type, Args...>()};
	ch_type buffer[stack_size];
	ch_type *const first{buffer};
	ch_type *const last{first + bounded_size};
	ch_type *const actual_end{
		::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type, true>(
			first, args...)};
	if (!::fast_io::details::decay::print_reserve_scatters_cursor_in_closed_range(
			first, last, actual_end)) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return strlike_construct_define(
		::fast_io::io_strlike_type<ch_type, T>, first, actual_end);
}

template <typename T>
inline constexpr bool basic_general_concat_top_level_condition_v =
	::fast_io::details::decay::print_semantic_node<T> &&
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<decltype(::fast_io::details::decay::print_semantic_node_ref(::std::declval<T>()))>>;

template <typename... Args>
inline constexpr bool basic_general_concat_has_top_level_condition_v =
	(false || ... || ::fast_io::details::decay::basic_general_concat_top_level_condition_v<Args>);

template <typename continuation, typename T>
struct basic_general_concat_condition_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	template <typename... TailArgs>
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<T>(*valueptr), ::std::forward<TailArgs>(tail_args)...);
	}
};

template <::std::integral ch_type, typename continuation>
inline constexpr decltype(auto) basic_general_concat_select_condition(continuation &&cont)
{
	return ::std::forward<continuation>(cont)();
}

template <::std::integral ch_type, typename continuation, typename T, typename... Args>
inline constexpr decltype(auto) basic_general_concat_select_condition(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<T>)
	{
		auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
		if (node_ref.pred)
		{
			using branch_type = decltype(node_ref.t1);
			if constexpr (::std::same_as<::std::remove_cvref_t<branch_type>, ::fast_io::io_null_t>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
			}
			else if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<branch_type>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), node_ref.t1, ::std::forward<Args>(args)...);
			}
			else
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation,
																								  branch_type>{
						__builtin_addressof(cont), __builtin_addressof(node_ref.t1)},
					::std::forward<Args>(args)...);
			}
		}
		else
		{
			using branch_type = decltype(node_ref.t2);
			if constexpr (::std::same_as<::std::remove_cvref_t<branch_type>, ::fast_io::io_null_t>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
			}
			else if constexpr (::fast_io::details::decay::basic_general_concat_top_level_condition_v<branch_type>)
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::std::forward<continuation>(cont), node_ref.t2, ::std::forward<Args>(args)...);
			}
			else
			{
				return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
					::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation,
																								  branch_type>{
						__builtin_addressof(cont), __builtin_addressof(node_ref.t2)},
					::std::forward<Args>(args)...);
			}
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
	{
		return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::fast_io::details::decay::basic_general_concat_condition_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_select_condition_ref_continuation
{
	T *strptr;

	template <typename... Args>
	inline constexpr void operator()(Args &&...args) const
	{
		::fast_io::details::decay::basic_general_concat_decay_ref_impl<line, ch_type>(
			*strptr, ::std::forward<Args>(args)...);
	}
};

template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_select_condition_phase1_continuation
{
	template <typename... Args>
	inline constexpr T operator()(Args &&...args) const
	{
		// The enclosing phase already owns the normalized graph. Selected condition members are named lvalues, so recurse
		// through the borrowing body instead of reopening a by-value normalization boundary.
		return ::fast_io::details::decay::basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(
			args...);
	}
};

template <bool line, ::std::integral ch_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void
basic_general_concat_decay_ref_impl_semantic_precise(T &str, Args &...args)
{
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		constexpr ::std::size_t static_bound{
			::fast_io::operations::decay::print_semantic_static_bounded_total_size<line, ch_type, Args...>()};
		if constexpr (static_bound != SIZE_MAX)
		{
			if constexpr (static_bound <= local_cap)
			{
				auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
				auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type, true>(
					first, args...)};
				strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
				return;
			}
		}
	}
	::std::size_t const precise_size{
		::fast_io::operations::decay::print_semantic_precise_total_size<line, ch_type>(args...)};
	if (precise_size == SIZE_MAX) [[unlikely]]
	{
		// Stream output could split this exact run, but concat must return one contiguous object. Reject the
		// unrepresentable result before reserve or pointer arithmetic observes the sentinel as a real size.
		::fast_io::fast_terminate();
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < precise_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type>(first, args...)};
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

/// @brief Emits an exactly measurable semantic run directly into a portable resized destination.
/// @details A construct-only destination previously routed this case through `basic_concat_buffer`, whose inline-first
///          representation puts a 2-KiB character array in the caller and then range-constructs the real result. The
///          semantic precise-size proof is stronger: it describes the selected condition branches, width padding,
///          packs, and optional newline exactly. Combining that proof with `precise_resize_writable_strlike` permits
///          one resize and one direct emission without granting the destination a fictitious spare-capacity put area.
///
///          This implementation is intentionally separate from semantic bounded emission. A bound may exceed the
///          produced prefix and would publish uninitialized padding as string size; precise emission instead verifies
///          that its returned cursor equals the promised logical end. Sizing is complete before resize. If emission
///          throws, the local result owns any partially written characters and is destroyed before it can escape.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
	requires ::fast_io::precise_resize_writable_strlike<ch_type, T>
inline constexpr T basic_general_concat_semantic_precise_resize(Args &...args)
{
	T result;
	::std::size_t const precise_size{
		::fast_io::operations::decay::print_semantic_precise_total_size<line, ch_type>(args...)};
	if (static_cast<::std::size_t>(PTRDIFF_MAX) < precise_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	ch_type *const first{strlike_precise_resize_and_get_begin(
		::fast_io::io_strlike_type<ch_type, T>, result, precise_size)};
	ch_type *const actual_end{
		::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type>(
			first, args...)};
	ch_type *expected_end{first};
	if (precise_size != 0u)
	{
		expected_end += precise_size;
	}
	if (actual_end != expected_end) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	return result;
}

/// @brief Materializes a semantic concat run from its run-time upper bound.
/// @details The destination capacity is the sum produced by the same semantic graph used for emission.  Bounded
///          leaves may consume less than their reserve maximum; therefore only the returned cursor, never the bound,
///          becomes the observable string length.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_semantic_bounded(T &str, Args &...args)
{
	::std::size_t const bounded_size{
		::fast_io::operations::decay::print_semantic_bounded_total_size<line, ch_type>(args...)};
	if (bounded_size == SIZE_MAX) [[unlikely]]
	{
		// A conservative aggregate is only a capacity optimization. Preserve the valid semantic leaves through the
		// maintained cursor adapter, which emits them in order without requesting the impossible summed reserve bound.
		// `buffer_strlike` is the capability proof at every call site; an optional associated `io_strlike_ref` is neither
		// required nor sufficient and therefore cannot control whether this exceptional path is well formed.
		::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{__builtin_addressof(str)};
		::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		return;
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < bounded_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, bounded_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, bounded_size);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type, true>(
		first, args...)};
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl(T &str, Args &...args)
{
	if constexpr (basic_general_concat_has_top_level_condition_v<Args...>)
	{
		::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
			::fast_io::details::decay::basic_general_concat_select_condition_ref_continuation<line, ch_type, T>{
				__builtin_addressof(str)},
			args...);
	}
	else if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
	{
		basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
	}
	else if constexpr (basic_general_concat_semantic_bounded_ok<ch_type, Args...>)
	{
		// Semantic runs without a precise protocol still have an object-specific capacity proof.  Keeping them in the
		// semantic emitter preserves width/condition/pack meaning across every string adapter.
		basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(str, args...);
	}
	else if constexpr (
		::fast_io::details::decay::concat_retained_reserve_scatters_run_v<ch_type, Args...>)
	{
		// Unlike ordinary stream output, concat owns the final contiguous destination. Retaining the complete static
		// plan lets it replace per-descriptor destination growth with one checked reserve and one ordered copy.
		::fast_io::details::decay::basic_general_concat_decay_ref_impl_all_reserve_scatters<line, ch_type>(
			str, args...);
	}
	else if constexpr (((reserve_printable<ch_type, Args> ||
						 concat_retained_scatter_printable_v<ch_type, Args> ||
						 dynamic_reserve_printable<ch_type, Args>) &&
						...))
	{
		if constexpr ((concat_retained_scatter_printable_v<ch_type, Args> && ...))
		{
			// Exact retained descriptors outrank reserve upper bounds. A dual-protocol leaf may advertise an enormous
			// conservative reserve while its borrowed scatter is tiny; rejecting the exact representation because the
			// unused bound aggregate is unavailable would be a protocol-priority inversion.
			basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(str, args...);
		}
		else
		{
			constexpr ::std::size_t reserve_size{
				calculate_concat_scatter_reserve_size_or_unavailable<ch_type, Args...>()};
			constexpr ::std::size_t reserve_size_with_line{
				::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
			if constexpr (reserve_size_with_line == SIZE_MAX)
			{
				// The static sum is only a conservative grouped-allocation proof. Reuse a per-leaf cached plan so an
				// unavailable aggregate never re-queries a stateful size customization or allocates the impossible sum.
				::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
					str, args...);
			}
			else if constexpr ((reserve_printable<ch_type, Args> && ...))
			{
				if constexpr (sso_buffer_strlike<ch_type, T>)
				{
					constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
					constexpr bool not_enough_space{(local_cap < reserve_size_with_line)};
					if constexpr (not_enough_space &&
								  ((sizeof...(Args) == 1) && (precise_reserve_printable<ch_type, Args> && ...)))
					{
						basic_general_concat_decay_ref_impl_precise<line, ch_type, T>(str, args...);
					}
					else
					{
						if constexpr (not_enough_space)
						{
							strlike_reserve(io_strlike_type<ch_type, T>, str, reserve_size_with_line);
						}
						strlike_set_curr(io_strlike_type<ch_type, T>, str,
										 print_reserve_define_chain_impl<line>(
											 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
					}
				}
				else
				{
					strlike_reserve(io_strlike_type<ch_type, T>, str, reserve_size_with_line);
					strlike_set_curr(
						io_strlike_type<ch_type, T>, str,
						print_reserve_define_chain_impl<line>(
							strlike_begin(io_strlike_type<ch_type, T>, str), args...));
				}
			}
			else
			{
				// Dynamic bounds and retained descriptors are object-dependent. Measure each selected protocol once,
				// cache its metadata, and share it between the grouped and sequential strategies.
				::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
					str, args...);
			}
		}
	}
	else
	{
		if constexpr (
			::fast_io::details::decay::basic_general_concat_direct_destination_ok<
				line, ch_type, T, Args...>)
		{
			decltype(auto) destination = io_strlike_ref(::fast_io::io_alias, str);
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		}
		else
		{
			static_assert(
				::fast_io::details::decay::basic_general_concat_generic_buffer_destination_ok<
					line, ch_type, T, Args...>,
				"the normalized concat fallback is not printable to the result's generic cursor adapter");
			::fast_io::io_strlike_reference_wrapper<ch_type, T> destination{
				__builtin_addressof(str)};
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
		}
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_ref_impl(Args &...args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			if constexpr (single_character_constructible_strlike<ch_type, T>)
			{
				return strlike_construct_single_character_define(io_strlike_type<ch_type, T>,
																 char_literal_v<u8'\n', ch_type>);
			}
			else if constexpr (::fast_io::range_constructible_strlike<ch_type, T>)
			{
				return strlike_construct_define(io_strlike_type<ch_type, T>,
												__builtin_addressof(char_literal_v<u8'\n', ch_type>),
												__builtin_addressof(char_literal_v<u8'\n', ch_type>) + 1);
			}
			else
			{
				// A writable-buffer-only result has no range constructor. Reuse the ordinary line emitter so the
				// generic cursor adapter owns reserve/growth and the one committed newline exactly as it does for a
				// nonempty run.
				T str;
				::fast_io::details::decay::basic_general_concat_decay_ref_impl<true, ch_type>(str);
				return str;
			}
		}
		else
		{
			return {};
		}
	}
	else
	{
		if constexpr (
			::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_v<
				ch_type, T, Args...>)
		{
			constexpr ::std::size_t stack_size{
				::fast_io::details::decay::basic_general_concat_single_pass_bounded_run_stack_size<
					ch_type, Args...>()};
			::std::size_t const bounded_size{
				::fast_io::details::decay::basic_general_concat_single_pass_bounded_total_size<
					line, ch_type>(args...)};
			if (bounded_size <= stack_size) [[likely]]
			{
				return ::fast_io::details::decay::basic_general_concat_single_pass_bounded_construct<
					line, ch_type, T>(bounded_size, args...);
			}
		}
		if constexpr (basic_general_concat_has_top_level_condition_v<Args...>)
		{
			return ::fast_io::details::decay::basic_general_concat_select_condition<ch_type>(
				::fast_io::details::decay::basic_general_concat_select_condition_phase1_continuation<line, ch_type,
																									 T>{},
				args...);
		}
		else if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
		{
			if constexpr (buffer_strlike<ch_type, T>)
			{
				T str;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
				return str;
			}
			else if constexpr (precise_resize_writable_strlike<ch_type, T>)
			{
				return ::fast_io::details::decay::basic_general_concat_semantic_precise_resize<
					line, ch_type, T>(args...);
			}
			else
			{
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(buffer, args...);
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
		else if constexpr (basic_general_concat_semantic_bounded_ok<ch_type, Args...>)
		{
			if constexpr (buffer_strlike<ch_type, T>)
			{
				T str;
				basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(str, args...);
				return str;
			}
			else
			{
				// Construct-only destinations receive only the prefix reported by bounded semantic emission; unused reserve
				// capacity never leaks into their value.
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl_semantic_bounded<line, ch_type>(buffer, args...);
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
		else if constexpr (
			basic_general_concat_context_staging_preferred_destination<ch_type, T> &&
			(::fast_io::context_printable<ch_type, Args> && ...))
		{
			// Context output is intrinsically single-pass: no rewind or purity contract permits an exact-size replay.
			// Advance each state exactly once into concat's inline-first buffer, then publish only the completed prefix by
			// range construction. If a producer throws, the staging buffer is destroyed and no partial result escapes.
			basic_concat_buffer<ch_type> buffer;
			auto destination{io_strlike_ref(::fast_io::io_alias, buffer)};
			::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
			return strlike_construct_define(
				io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
		else if constexpr (buffer_strlike<ch_type, T>)
		{
			T str;
			basic_general_concat_decay_ref_impl<line, ch_type>(str, args...);
			return str;
		}
		else if constexpr (
			line && sizeof...(Args) == 1u &&
			(::fast_io::details::decay::direct_scatter_view_printable<ch_type, Args> && ...) &&
			requires(::fast_io::basic_io_scatter_t<ch_type> scatter) {
				{
					strlike_construct_scatter_with_line_feed_define(
						::fast_io::io_strlike_type<ch_type, T>, scatter,
						::std::size_t{})
				} -> ::std::same_as<T>;
			})
		{
			// The result is freshly constructed, so its allocation cannot own the borrowed source
			// descriptor. Reserve the complete text-plus-LF extent before either append and avoid the
			// exact-capacity first append followed by a second allocation for the line feed.
			::fast_io::basic_io_scatter_t<ch_type> const scatter{args...};
			::std::size_t const total_size{
				::fast_io::details::decay::concat_precise_size_with_line<true>(scatter.len)};
			return strlike_construct_scatter_with_line_feed_define(
				::fast_io::io_strlike_type<ch_type, T>, scatter, total_size);
		}
		else if constexpr (
			sizeof...(Args) == 1u &&
			(::fast_io::details::decay::basic_general_concat_exact_overwrite_run_v<
				 line, ch_type, T, Args> &&
			 ...))
		{
			// A newline-bearing run-time scatter range would otherwise be measured, copied into concat staging, and copied
			// again into the final standard string. The exact callback path owns the same two-pass source traversal but
			// writes the second pass and newline directly into the destination's uninitialized live extent. It precedes
			// retained-scatter and reserve fallbacks only for the independently proved one-leaf/noexcept line shape above.
			return ::fast_io::details::decay::basic_general_concat_exact_overwrite_run<
				line, ch_type, T>(args...);
		}
		else if constexpr ((!line) && sizeof...(args) == 1 &&
						   (::fast_io::scatter_printable_for<ch_type, Args &> && ...))
		{
			basic_io_scatter_t<ch_type> scatter{print_scatter_define_extract_one<ch_type>(args...)};
			if (::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(0u, scatter.len) == SIZE_MAX)
				[[unlikely]]
			{
				// Validate the exact descriptor before forming `base + len`; pointer arithmetic cannot itself be used as
				// the proof because an excessive length would already make that expression undefined.
				::fast_io::fast_terminate();
			}
			ch_type const *const first{
				scatter.len == 0u ? __builtin_addressof(::fast_io::char_literal_v<u8'\0', ch_type>) : scatter.base};
			return strlike_construct_define(io_strlike_type<ch_type, T>, first, first + scatter.len);
		}
		else if constexpr ((concat_retained_scatter_printable_v<ch_type, Args> && ...))
		{
			// Construct-only destinations cannot expose final writable storage. Materialize the exact retained scatter
			// run once in concat's growable staging string, including the optional newline, then construct from its used
			// prefix. This branch deliberately precedes reserve bounds for dual-protocol leaves.
			basic_concat_buffer<ch_type> buffer;
			basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(buffer, args...);
			return strlike_construct_define(
				io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
		else if constexpr ((reserve_printable<ch_type, Args> && ...))
		{
			constexpr ::std::size_t reserve_size{
				calculate_concat_scatter_reserve_size_or_unavailable<ch_type, Args...>()};
			constexpr ::std::size_t reserve_size_with_line{
				::fast_io::details::decay::print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
			if constexpr (reserve_size_with_line == SIZE_MAX)
			{
				if constexpr (
					::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
						ch_type, T, Args...>)
				{
					// A conservative reserve sum may be unavailable while the same leaves have a tiny exact protocol.
					// Prefer the destination's exact resize proof before allocating any per-leaf reserve scratch.
					return ::fast_io::details::decay::basic_general_concat_precise_resize_run<
						line, ch_type, T>(args...);
				}
				else
				{
					// Construct-only results cannot expose append storage. Cache the selected reserve metadata once in a
					// growable staging string and range-construct only from the prefixes actually returned by producers.
					basic_concat_buffer<ch_type> buffer;
					::fast_io::details::decay::basic_general_concat_decay_ref_impl_cached_mixed<line, ch_type>(
						buffer, args...);
					return strlike_construct_define(
						io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
				}
			}
			else if constexpr (
				::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_size_with_line, ch_type>)
			{
				// Non-buffer string-like destinations cannot expose writable storage, so a small all-reserve run is
				// staged once before construction.  The shared byte budget is the proof that this automatic array cannot
				// make concat's frame larger than the print/scan policy permits.
				ch_type buffer[reserve_size_with_line];
				auto p{print_reserve_define_chain_impl<line>(buffer, args...)};
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer, p);
			}
			else
			{
				// A compile-time reserve bound is not a stack-safety guarantee.  Keeping the large alternative in dynamic
				// storage prevents a custom fixed-capacity printable from injecting an arbitrarily large frame into an
				// otherwise allocation-free concat instantiation; construction still observes the same exact [begin,end)
				// sequence and therefore has identical output semantics.
				::fast_io::details::local_operator_new_array_ptr<ch_type> buffer(reserve_size_with_line);
				auto p{print_reserve_define_chain_impl<line>(buffer.ptr, args...)};
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.ptr, p);
			}
		}
		else if constexpr (
			::fast_io::details::decay::basic_general_concat_precise_resize_destination_run_v<
				ch_type, T, Args...>)
		{
			// Direct scatter construction and all-static reserve construction above retain priority. This later branch
			// targets a bounded ordinary run whose dynamic exact protocol can replace concat-buffer staging plus final
			// range construction with one logical resize and ordered in-place emission. Initialization-sensitive leaves
			// reach this branch only when the destination separately proves that establishing the live range does not
			// value-initialize characters which the formatter immediately overwrites.
			return ::fast_io::details::decay::basic_general_concat_precise_resize_run<line, ch_type, T>(args...);
		}
		else
		{
			if constexpr (
				!::fast_io::details::decay::basic_general_concat_initialization_sensitive_staging_required_v<
					ch_type, T, Args...> &&
				::fast_io::details::decay::basic_general_concat_direct_destination_ok<
					line, ch_type, T, Args...>)
			{
				// Append-oriented non-buffer results (notably std::basic_string on implementations where writable
				// internals are unavailable) can still receive the normalized run directly. This removes the intermediate
				// `basic_concat_buffer` allocation/materialization and the final range construction for direct/context
				// leaves; the destination may still allocate while it grows. Enter the ordinary print dispatcher on the
				// adapter itself: `basic_general_concat_decay_ref_impl` assumes that its `T` exposes writable strlike cursors
				// on reserve/scatter paths, which is exactly what this non-buffer branch does not promise.
				T str;
				auto destination{io_strlike_ref(::fast_io::io_alias, str)};
				::fast_io::operations::decay::print_freestanding_decay_impl<line>(destination, args...);
				return str;
			}
			else
			{
				static_assert(
					::fast_io::details::decay::basic_general_concat_staging_destination_ok<
						line, ch_type, Args...>,
					"the normalized concat fallback is printable to neither the result adapter nor its staging stream");
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl<line, ch_type>(buffer, args...);
				return strlike_construct_define(
					io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
	}
}

/// @brief Tests whether concat's active normalized leaf run has a useful compiler-constant replacement.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool basic_general_concat_compiler_constant_materialization_available() noexcept
{
	if constexpr (!(::fast_io::compiler_constant_printable<ch_type, Args> && ...))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t reserve_size{
			::fast_io::operations::decay::
				print_compiler_constant_materialization_reserve_size<
					line, ch_type, Args...>()};
		constexpr ::std::size_t proxy_bytes{
			::fast_io::operations::decay::
				print_compiler_constant_materialization_proxy_bytes<
					ch_type, Args...>()};
		constexpr bool compact_reserve_plan{
			reserve_size != SIZE_MAX &&
			reserve_size <=
				::fast_io::details::compiler_constant_materialization_max_bytes /
					sizeof(ch_type)};
		constexpr ::std::size_t retained_reserve_size{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes /
					sizeof(ch_type)};
			::std::size_t total{};
			((total = [](::std::size_t current) consteval {
				using source_type = ::std::remove_cvref_t<Args>;
				using replacement_type =
					::fast_io::details::compiler_constant_materialized_t<
						ch_type, Args>;
				if constexpr (::std::same_as<source_type, replacement_type>)
				{
					constexpr ::std::size_t extent{print_reserve_size(
						::fast_io::io_reserve_type<ch_type, replacement_type>)};
					return current > maximum || extent > maximum - current
						? SIZE_MAX
						: current + extent;
				}
				else
				{
					return current;
				}
			}(total)),
			 ...);
			return total;
		}()};
		constexpr bool exact_destination_plan{
			::fast_io::details::decay::
				basic_general_concat_precise_resize_destination_run_v<
					ch_type, T,
					::fast_io::details::compiler_constant_materialized_t<
						ch_type, Args>...>};
		return proxy_bytes != SIZE_MAX &&
			   proxy_bytes <=
				   ::fast_io::details::compiler_constant_materialization_max_bytes &&
			   (compact_reserve_plan ||
				(exact_destination_plan && retained_reserve_size != SIZE_MAX)) &&
			   (false || ... ||
			!::std::same_as<
				::std::remove_cvref_t<Args>,
				::fast_io::details::compiler_constant_materialized_t<ch_type, Args>>);
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T
basic_general_concat_compiler_constant_materialized(Args... args)
{
	constexpr ::std::size_t reserve_size{
		::fast_io::details::decay::
			calculate_concat_scatter_reserve_size_or_unavailable<
				ch_type, Args...>()};
	constexpr ::std::size_t reserve_size_with_line{
		::fast_io::details::decay::
			print_contiguous_char_extent_add_or_unavailable<ch_type>(
				reserve_size, static_cast<::std::size_t>(line))};
	constexpr bool compact_reserve_plan{
		reserve_size_with_line != SIZE_MAX &&
		reserve_size_with_line <=
			::fast_io::details::compiler_constant_materialization_max_bytes /
				sizeof(ch_type)};
	if constexpr (
		!compact_reserve_plan &&
		::fast_io::details::decay::
			basic_general_concat_precise_resize_destination_run_v<
				ch_type, T, Args...>)
	{
		// Compiler-constant replacement is an allocation strategy, not a scatter-output strategy.  When the replacement
		// run exposes exact sizes and the destination can establish one writable logical extent, write every proxy
		// directly into that extent.  This is also the bounded escape hatch for compact float proxy state whose mature
		// reserve capacity is intentionally much larger than the constant-materialization byte budget.
		return ::fast_io::details::decay::basic_general_concat_precise_resize_run<
			line, ch_type, T>(args...);
	}
	else if constexpr (compact_reserve_plan &&
					 ::fast_io::buffer_strlike<ch_type, T>)
	{
		// Keep the selected proxy run contiguous.  Re-entering the complete phase-1 dispatcher here is both unnecessary
		// (the gate has already proved an all-reserve run) and can make a large caller outline the value-sensitive writer.
		// A writable string result instead establishes its one destination capacity and receives every replacement in
		// order.  No static-fragment/scatter output protocol participates in concat.
		T result;
		if constexpr (::fast_io::sso_buffer_strlike<ch_type, T>)
		{
			constexpr ::std::size_t local_capacity{
				strlike_sso_size(::fast_io::io_strlike_type<ch_type, T>)};
			if constexpr (local_capacity < reserve_size_with_line)
			{
				strlike_reserve(
					::fast_io::io_strlike_type<ch_type, T>, result,
					reserve_size_with_line);
			}
		}
		else if constexpr (reserve_size_with_line != 0u)
		{
			strlike_reserve(
				::fast_io::io_strlike_type<ch_type, T>, result,
				reserve_size_with_line);
		}
		ch_type *const end{print_reserve_define_chain_impl<line>(
			strlike_begin(::fast_io::io_strlike_type<ch_type, T>, result),
			args...)};
		strlike_set_curr(
			::fast_io::io_strlike_type<ch_type, T>, result, end);
		return result;
	}
	else if constexpr (
		compact_reserve_plan &&
		::fast_io::range_constructible_strlike<ch_type, T>)
	{
		// Construct-only strings cannot expose their final storage in C++20.  The bounded local range is nevertheless a
		// single contiguous materialization, and the range constructor owns the sole destination allocation/copy.  Short
		// standard-string results are normally scalar-replaced into direct SSO stores by both supported compilers.
		ch_type buffer[reserve_size_with_line == 0u ? 1u : reserve_size_with_line];
		ch_type *const end{print_reserve_define_chain_impl<line>(buffer, args...)};
		return strlike_construct_define(
			::fast_io::io_strlike_type<ch_type, T>, buffer, end);
	}
	else
	{
		// Uncommon construct-only adapters retain the mature phase-1 continuation.  This branch is never selected solely
		// from a large compiler-constant reserve bound: that case requires the exact-resize destination proof above.
		return ::fast_io::details::decay::
			basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(args...);
	}
}

/// @brief Keeps concat's original phase-1 body as the sole unknown-value continuation.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T
basic_general_concat_compiler_constant_dispatch(Args &...args)
{
	if constexpr (
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_materialization_available<
				line, ch_type, T, Args...>())
	{
		if (::fast_io::operations::decay::
				print_compiler_constant_materialization_gate<ch_type>(args...))
		{
			return ::fast_io::details::decay::
				basic_general_concat_compiler_constant_materialized<
					line, ch_type, T>(
						print_compiler_constant_materialize(
							::fast_io::io_reserve_type<ch_type,
							::std::remove_cvref_t<Args>>, args)...);
		}
	}
	return ::fast_io::details::decay::
		basic_general_concat_phase1_decay_ref_impl<line, ch_type, T>(args...);
}

/// @brief Owns a structurally flat normalized run exactly once before all strategy helpers borrow it.
/// @details The direct public path supplies prvalue normalization results, so this boundary materializes their compact
///          decayed types and then exposes only named lvalues. Pack expansion and condition selection already own any
///          value-producing customization in their synchronous continuation frames and therefore bypass this boundary;
///          copying those selected lvalues here would be a second decay and would reject valid move-only transports.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T
basic_general_concat_phase1_decay_impl(Args... args)
{
	return ::fast_io::details::decay::basic_general_concat_compiler_constant_dispatch<line, ch_type, T>(
		args...);
}

/// @brief Models one source argument after the optional pre-normalization replacement and concat's ordinary flat
///        alias/status forwarding.
template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_replacement_t =
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_replacement_t<ch_type, T>;

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_alias_t = decltype(
	::fast_io::io_print_alias(::std::declval<
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_source_replacement_t<
				ch_type, T>>()));

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_forward_t = decltype(
	::fast_io::io_print_forward<ch_type>(::std::declval<
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_source_alias_t<
				ch_type, T>>()));

template <::std::integral ch_type, typename T>
using basic_general_concat_compiler_constant_source_normalized_t =
	::std::remove_cvref_t<::fast_io::details::decay::
		basic_general_concat_compiler_constant_source_forward_t<ch_type, T>>;

/// @brief Proves that concat may select a compiler-constant source replacement before alias normalization.
/// @details This is intentionally a flat-run gate. Semantic packs, conditions, and value-owning transports retain the
///          established normalization graph and may still use the normalized phase-1 gate. For a flat run, every
///          replacement is normalized exactly as the true-arm expression below, every resulting leaf must have a
///          contiguous reserve protocol, and the compact proxy state remains independently bounded. A floating proxy's
///          mature 5006-character reserve capacity is admitted only when all leaves expose exact sizes and the result
///          can establish the corresponding live destination range.
template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline consteval bool
basic_general_concat_compiler_constant_source_available() noexcept
{
	constexpr bool has_candidate{
		(false || ... ||
		 ::fast_io::operations::decay::
			 print_compiler_constant_pre_normalization_candidate_v<
				 ch_type, Args>)};
	constexpr bool source_semantic_run{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 ch_type, Args>)};
	constexpr bool replacement_semantic_run{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_input_argument_v<
			 ch_type,
			 ::fast_io::details::decay::
				 basic_general_concat_compiler_constant_source_replacement_t<
					 ch_type, Args>>)};
	if constexpr (!has_candidate || source_semantic_run || replacement_semantic_run)
	{
		return false;
	}
	else if constexpr (!(::fast_io::reserve_printable<
		ch_type,
		::fast_io::details::decay::
			basic_general_concat_compiler_constant_source_normalized_t<
				ch_type, Args>> && ...))
	{
		return false;
	}
	else
	{
		constexpr ::std::size_t maximum_bytes{
			::fast_io::details::compiler_constant_materialization_max_bytes};
		constexpr ::std::size_t proxy_bytes{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes};
			::std::size_t total{};
			((total = total > maximum || sizeof(::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>) > maximum - total
					  ? SIZE_MAX
					  : total + sizeof(::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>)),
			 ...);
			return total;
		}()};
		constexpr ::std::size_t reserve_size{
			::fast_io::details::decay::
				calculate_concat_scatter_reserve_size_or_unavailable<
					ch_type,
					::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>...>()};
		constexpr ::std::size_t reserve_size_with_line{
			::fast_io::details::decay::
				print_contiguous_char_extent_add_or_unavailable<ch_type>(
					reserve_size, static_cast<::std::size_t>(line))};
		constexpr bool compact_reserve_plan{
			reserve_size_with_line != SIZE_MAX &&
			reserve_size_with_line <= maximum_bytes / sizeof(ch_type)};
		constexpr ::std::size_t retained_reserve_size{[]() consteval {
			constexpr ::std::size_t maximum{
				::fast_io::details::compiler_constant_materialization_max_bytes /
					sizeof(ch_type)};
			::std::size_t total{};
			((total = [](::std::size_t current) consteval {
				if constexpr (!::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_candidate_v<
						ch_type, Args>)
				{
					using normalized_type =
						::fast_io::details::decay::
							basic_general_concat_compiler_constant_source_normalized_t<
								ch_type, Args>;
					constexpr ::std::size_t extent{print_reserve_size(
						::fast_io::io_reserve_type<ch_type, normalized_type>)};
					return current > maximum || extent > maximum - current
						? SIZE_MAX
						: current + extent;
				}
				else
				{
					return current;
				}
			}(total)),
			 ...);
			return total;
		}()};
		constexpr bool exact_destination_plan{
			::fast_io::details::decay::
				basic_general_concat_precise_resize_destination_run_v<
					ch_type, T,
					::fast_io::details::decay::
						basic_general_concat_compiler_constant_source_normalized_t<
							ch_type, Args>...>};
		return proxy_bytes != SIZE_MAX && proxy_bytes <= maximum_bytes &&
			(compact_reserve_plan ||
			 (exact_destination_plan && retained_reserve_size != SIZE_MAX));
	}
}

template <bool line, ::std::integral ch_type, typename T,
		  typename... ReplacedArgs>
inline constexpr T
basic_general_concat_compiler_constant_source_normalized(
	ReplacedArgs &&...args)
{
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_materialized<line, ch_type, T>(
			::fast_io::io_print_forward<ch_type>(::fast_io::io_print_alias(
				::std::forward<ReplacedArgs>(args)))...);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T
basic_general_concat_compiler_constant_source_materialized(Args &&...args)
{
	return ::fast_io::details::decay::
		basic_general_concat_compiler_constant_source_normalized<
			line, ch_type, T>(
			::fast_io::operations::decay::
				print_compiler_constant_pre_normalization_materialize_one<
					ch_type>(::std::forward<Args>(args))...);
}

/// @brief Enters concat sizing only after semantic structure has been normalized.
/// @details Phase 1 assumes that every argument is an active printable leaf. Keeping that assumption behind a
///          continuation prevents an inactive condition branch from contributing reserve capacity or selecting a
///          materialization strategy. Pack expansion and condition selection invoke this continuation synchronously,
///          so all forwarded references remain valid through sizing and the immediately following write pass.
template <bool line, ::std::integral ch_type, typename T>
struct basic_general_concat_normalized_phase1_continuation
{
	/// @brief Sizes and materializes the normalized leaf sequence.
	template <typename... Args>
	inline constexpr T operator()(Args &&...args) const
	{
		// Every argument is now a named object owned by an enclosing normalization/selection frame. The complete phase-1
		// call is nested synchronously inside that frame, so borrowing preserves lifetime while avoiding a second copy.
		return ::fast_io::details::decay::basic_general_concat_compiler_constant_dispatch<line, ch_type, T>(
			args...);
	}
};

/// @brief Selects conditions exposed by semantic pack expansion before concat phase 1.
/// @details A pack can contain conditions that were not visible in the original top-level argument list. The stored
///          pointer is non-owning but cannot escape: print_semantic_pack_expand completes the nested invocation before
///          returning, and the referenced continuation outlives that complete call chain.
template <::std::integral ch_type, typename continuation>
struct basic_general_concat_select_conditions_continuation
{
	::std::remove_reference_t<continuation> *contptr;

	/// @brief Recursively removes inactive branches and forwards the active leaves to the saved continuation.
	template <typename... Args>
	inline constexpr decltype(auto) operator()(Args &&...args) const
	{
		return ::fast_io::details::decay::print_semantic_select_conditions<ch_type>(
			*contptr, ::std::forward<Args>(args)...);
	}
};

} // namespace fast_io::details::decay

namespace fast_io
{

template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline constexpr T basic_general_concat(Args &&...args)
{
	// Normalization order is part of the allocation proof: alias/forward first, expand packs and remove nulls, select
	// active conditions recursively, and only then allow phase 1 to compute the output capacity. Public argument value
	// categories are preserved through the first CPO pair; every continuation below is synchronous and therefore keeps
	// a returned proxy/value alive until phase 1 finishes.
	using normalize_continuation =
		::fast_io::details::decay::basic_general_concat_normalized_phase1_continuation<line, char_type, T>;
	constexpr bool has_pack_or_null{
		(false || ... ||
		 (::fast_io::details::decay::print_semantic_pack_argument_v<
			  ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>> ||
		  ::std::same_as<::std::remove_cvref_t<
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>>,
						 ::fast_io::io_null_t>))};
	constexpr bool has_condition{
		(false || ... ||
		 ::fast_io::details::decay::print_semantic_top_level_condition_v<
			 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>>)};
	if constexpr (has_pack_or_null)
	{
		// Pack expansion comes first because it may reveal nested conditions. The following continuation then selects
		// those conditions, ensuring that neither inactive leaves nor nulls influence concat's capacity or cost model.
		normalize_continuation normalize;
		return ::fast_io::details::decay::print_semantic_pack_expand<true, char_type>(
			::fast_io::details::decay::basic_general_concat_select_conditions_continuation<
				char_type, normalize_continuation>{__builtin_addressof(normalize)},
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
	else if constexpr (has_condition)
	{
		// Without packs, direct recursive condition selection can enter phase 1 immediately after choosing active arms.
		return ::fast_io::details::decay::print_semantic_select_conditions<char_type>(
			normalize_continuation{},
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
	else
	{
		// A structurally flat run retains the original direct instantiation and pays no continuation machinery.
		return ::fast_io::details::decay::basic_general_concat_phase1_decay_impl<line, char_type, T>(
			io_print_forward<char_type>(io_print_alias(::std::forward<Args>(args)))...);
	}
}

/// @brief Validates and executes concat against every destination phase 1 may actually select.
/// @details Reserve/scatter protocols are destination-independent, but a direct `print_define` customization may be
///          constrained to one stream type. The former public wrappers probed a synthetic dummy stream and then called
///          phase 1 directly; this could both reject a valid string-specific leaf and bypass pack/condition
///          normalization. Writable-buffer results have one real fallback destination. Non-buffer results prefer their
///          append adapter when the complete normalized run is accepted and otherwise use `basic_concat_buffer` before
///          range construction. Testing exactly that disjunction keeps admission, strategy selection, and eventual ADL
///          calls consistent while the normalized front door preserves the public argument value categories.
/// @tparam line      true to append a newline
/// @tparam char_type destination character type
/// @tparam T         string-like result type
/// @tparam Args      public concat argument types
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline consteval bool basic_general_concat_checked_available() noexcept
{
	constexpr bool printable_to_result{
		::fast_io::details::decay::basic_general_concat_direct_destination_ok<
			line, char_type, T,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...> ||
		::fast_io::details::decay::basic_general_concat_generic_buffer_destination_ok<
			line, char_type, T,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...>};
	constexpr bool printable_to_staging{
		::fast_io::details::decay::basic_general_concat_staging_destination_ok<
			line, char_type,
			::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args &&>...>};
	// Writable-buffer results always execute fallback on their own adapter. A non-buffer result instead has two
	// destination-correct strategies: direct append when supported, otherwise internal staging followed by construction.
	constexpr bool printable_to_selected_destination{
		printable_to_result || (!buffer_strlike<char_type, T> && printable_to_staging)};
	return printable_to_selected_destination;
}

/// @brief Historical checked concat continuation used verbatim by the false arm of the source constant gate.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline constexpr T basic_general_concat_checked(Args &&...args)
{
	constexpr bool printable_to_selected_destination{
		::fast_io::basic_general_concat_checked_available<
			line, char_type, T, Args...>()};
	if constexpr (printable_to_selected_destination)
	{
		return ::fast_io::basic_general_concat<line, char_type, T>(::std::forward<Args>(args)...);
	}
	else
	{
		static_assert(printable_to_selected_destination,
					  "one or more arguments are not printable to any destination selected by concat");
		return {};
	}
}

/// @brief Keeps the optimizer-visible constant query at concat's public source boundary.
/// @details Only an explicitly pre-normalization-safe flat source run can enter the replacement arm. The false arm is
///          the ordinary checked implementation above, so an unknown integer/floating value performs no proxy
///          construction, size query, or branch at run time. Format lowering reaches this same entry with literals and
///          fields already translated to print/concat leaves; it owns no independent materialization policy.
template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr T
basic_general_concat_compiler_constant_checked_entry(Args &&...args)
{
	if constexpr (
		::fast_io::basic_general_concat_checked_available<
			line, char_type, T, Args...>())
	{
		if constexpr (
			::fast_io::details::decay::
				basic_general_concat_compiler_constant_source_available<
					line, char_type, T, Args &&...>())
		{
			if (::fast_io::operations::decay::
					print_compiler_constant_pre_normalization_gate<char_type>(
						args...))
			{
				return ::fast_io::details::decay::
					basic_general_concat_compiler_constant_source_materialized<
						line, char_type, T>(::std::forward<Args>(args)...);
			}
		}
	}
	return ::fast_io::basic_general_concat_checked<line, char_type, T>(
		::std::forward<Args>(args)...);
}

} // namespace fast_io
