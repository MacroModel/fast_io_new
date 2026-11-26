#pragma once

#include "transcode_callback.h"

namespace fast_io
{

/** @brief Combines borrowed text and reserve formatters in the encoder's public character domain. */
template <bool line, bool Checked, typename Owner, typename... Args>
	requires (sizeof...(Args) != 0 && transcode_buffer_sizes_provider<typename Owner::engine_ref_type> &&
			  ((scatter_printable_for<typename Owner::output_char_type, Args &> ||
				reserve_printable<typename Owner::output_char_type, Args &> ||
				dynamic_reserve_printable<typename Owner::output_char_type, Args &>) && ...))
inline details::staged_print_equivalent transcode_status_print(basic_otranscoder_ref<Owner> ref, Args &...args)
#if defined(__HERBCEPTIONS__)
#if defined(__cpp_exceptions)
	throws
#else
	throws(Checked)
#endif
#endif
{
	using Char = typename Owner::output_char_type;
	constexpr auto target{Owner::traits_type::public_buffer_size};
	auto &owner{*ref.ptr};
	owner.require_typed_write_boundary();
	auto &put{owner.public_buffer};
	constexpr auto budget{details::default_transcode_buffer_size<Char>()};
	constexpr auto page{4096u / sizeof(Char)};
	constexpr auto growth_limit{page < budget ? page : budget};
	auto capacity = [&] {
		return put.buffer_begin ? static_cast<::std::size_t>(put.buffer_end - put.buffer_begin) : 0u;
	};
	auto reserve = [&](::std::size_t n)
#if defined(__HERBCEPTIONS__)
#if defined(__cpp_exceptions)
		throws
#else
		throws(Checked)
#endif
#endif
	{
		if (!put.buffer_curr || static_cast<::std::size_t>(put.buffer_end - put.buffer_curr) < n)
		{
			auto next{target};
			auto const previous{capacity()};
			if (previous && previous < growth_limit)
			{
				// Repeated short fields amortize the next engine call. The committed prefix is processed once.
				next = previous <= growth_limit / 2u ? previous * 2u : growth_limit;
			}
			owner.template prepare_put_area_impl<Checked>(n > next ? n : next);
		}
		return put.buffer_curr;
	};
	auto emit = [&]<typename T>(T &arg)
#if defined(__HERBCEPTIONS__)
#if defined(__cpp_exceptions)
		throws
#else
		throws(Checked)
#endif
#endif
	{
		constexpr auto tag{io_reserve_type<Char, ::std::remove_cvref_t<T>>};
		if constexpr (scatter_printable_for<Char, T &>)
		{
			auto const view{print_scatter_define(tag, arg)};
			// With no pending fields a whole preferred block can already run directly. When
			// appending a nearby short span to a record, one copy can avoid a separate engine call.
			auto threshold{Owner::direct_write_size};
			if (put.buffer_curr != put.buffer_begin && target < growth_limit)
			{
				auto const current{capacity()};
				threshold = current <= growth_limit / 2u ? current * 2u : growth_limit;
			}
			if (view.len >= threshold)
			{
				owner.template write_all_overflow_impl<Checked>(view.base, view.base + view.len);
			}
			else if (view.len)
			{
				auto dest{reserve(view.len)};
				put.buffer_curr = ::fast_io::details::non_overlapped_copy_n(view.base, view.len, dest);
			}
		}
		else
		{
			auto const bound{[&]()
#if defined(__HERBCEPTIONS__)
				throws(Checked)
#endif
			{
				if constexpr (reserve_printable<Char, T &>) return print_reserve_size(tag);
				else return print_reserve_size(tag, arg);
			}()};
			auto dest{reserve(bound)};
			auto next{print_reserve_define(tag, dest, arg)};
			auto const checked{details::validate_transcode_closed_range_offsets(dest, dest + bound, next)};
			if (!checked.valid) [[unlikely]]
			{
				throw_transcode_stream_error(transcode_stream_errc::protocol_violation);
			}
			put.buffer_curr = checked.current;
		}
	};
	(emit(args), ...);
	if constexpr (line)
	{
		*reserve(1u) = char_literal_v<u8'\n', Char>;
		++put.buffer_curr;
	}
	return {};
}

template <bool line, typename Owner, typename... Args>
	requires (sizeof...(Args) != 0 && transcode_buffer_sizes_provider<typename Owner::engine_ref_type> &&
			  ((scatter_printable_for<typename Owner::output_char_type, Args &> ||
				reserve_printable<typename Owner::output_char_type, Args &> ||
				dynamic_reserve_printable<typename Owner::output_char_type, Args &>) && ...))
inline details::staged_print_equivalent status_print_define(basic_otranscoder_ref<Owner> ref, Args &...args)
{
#if defined(__HERBCEPTIONS__) && defined(__cpp_exceptions)
	try { return transcode_status_print<line, false>(ref, args...); }
	catch throws (::std::error e) { e.throw_legacy_exception(); }
#else
	return transcode_status_print<line, false>(ref, args...);
#endif
}

#if defined(__HERBCEPTIONS__)
namespace details
{

/** @brief Emits one field into a checked encoder, keeping direct user callbacks on their own ABI.
 *  @details A successful callback boundary permits the next field to begin. A failed legacy write
 *  is reported as soon as that callback returns. The selected branch of a condition is the only
 *  branch whose formatter is evaluated.
 */
template <typename Owner, typename Arg>
inline void transcode_print_field(basic_otranscoder_ref<Owner> ref, Arg &arg) throws
{
	using Char = typename Owner::output_char_type;
	using Value = ::std::remove_cvref_t<Arg>;
	using immediate_ref = transcode_callback_ref<Owner, true>;
	using legacy_ref = transcode_callback_ref<Owner, false>;
	constexpr auto tag{io_reserve_type<Char, Value>};

	if constexpr (checked_print_control<Arg>)
	{
		visit_checked_print_control<Char>(arg, [&](auto &field) throws { transcode_print_field(ref, field); });
	}
	else if constexpr (requires { transcode_status_print<false, true>(ref, arg); })
	{
		transcode_status_print<false, true>(ref, arg);
	}
	else if constexpr (print_requires_exact_ref<Char, decltype(ref), Value>)
	{
		// The field's declared observer identity participates in overload resolution.
		print_define(tag, ref, arg);
	}
	else if constexpr ([] {
		if constexpr (direct_printable_to<Char, immediate_ref, Arg &>)
			return throws((print_define(io_reserve_type<Char, Value>,
				::std::declval<immediate_ref &>(), ::std::declval<Arg &>())));
		else
			return false;
	}())
	{
		immediate_ref checked{ref.ptr, {}};
		print_define(tag, checked, arg);
	}
	else if constexpr (direct_printable_to<Char, legacy_ref, Arg &>)
	{
		transcode_callback_error errors;
		legacy_ref ordinary{ref.ptr, __builtin_addressof(errors)};
		print_define(tag, ordinary, arg);
		errors.deliver();
	}
	else if constexpr (direct_printable_to<Char, decltype(ref), Arg &>)
	{
		// An endpoint-specific callback retains its concrete observer and its declared ABI.
		print_define(tag, ref, arg);
	}
	else
	{
		// The remaining context and scatter-reserve protocols use the ordinary field planner.
		::fast_io::operations::decay::print_freestanding_decay_borrowed_output<false>(ref, arg);
	}
}

/** @brief Runs a checked output record and finishes an owned temporary only after successful emission. */
template <bool line, typename Output, typename... Args>
inline void sync_print_transcode(Output &&out, Args &&...args) throws
{
	decltype(auto) ref{::fast_io::operations::output_stream_ref(out)};
	auto &owner{transcode_output_owner(ref)};
	auto borrow{::fast_io::operations::output_stream_ref(owner)};
	using Char = typename ::std::remove_reference_t<decltype(borrow)>::output_char_type;
	auto emit = [&](auto &&...source) throws {
		using ::fast_io::details::decay::print_runtime_scatter_plan_unwrap;
		if constexpr ((!checked_print_control<decltype(print_runtime_scatter_plan_unwrap(source))> && ...) &&
			requires { ::fast_io::transcode_status_print<line, true>(borrow, print_runtime_scatter_plan_unwrap(source)...); })
		{
			::fast_io::transcode_status_print<line, true>(borrow, print_runtime_scatter_plan_unwrap(source)...);
		}
		else
		{
			(transcode_print_field(borrow, print_runtime_scatter_plan_unwrap(source)), ...);
			if constexpr (line)
			{
				auto const *lf{&::fast_io::char_literal_v<u8'\n', Char>};
				owner.write_checked(lf, lf + 1);
			}
		}
	};
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line) owner.write_checked(&::fast_io::char_literal_v<u8'\n', Char>, &::fast_io::char_literal_v<u8'\n', Char> + 1);
	}
	else if constexpr ((false || ... || ::fast_io::details::decay::print_semantic_input_argument_v<Char, Args &&>))
	{
		emit(::fast_io::details::decay::print_semantic_input_forward<Char>(::std::forward<Args>(args))...);
	}
	else
	{
		emit(::fast_io::io_print_forward<Char>(::fast_io::io_print_alias(args))...);
	}
	if constexpr (!::std::is_lvalue_reference_v<Output> && ::fast_io::temporary_output_finish_enabled<::std::remove_cvref_t<Output>>)
		owner.template finish_impl<true>();
}

template <typename T>
inline constexpr bool has_sync_transcode_print = [] {
	if constexpr (requires { ::fast_io::operations::output_stream_ref(::std::declval<T &>()); })
		return sync_transcode_output_checked<::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(::std::declval<T &>()))>>;
	else
		return false;
}();

}
#endif

} // namespace fast_io
