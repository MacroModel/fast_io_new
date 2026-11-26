#pragma once

#if defined(__HERBCEPTIONS__)
namespace fast_io::details
{

/** @brief Classifies the formatter selected for a normalized field on an ordinary endpoint.
 *  @details Only the ADL call expressions are inspected. A traditional unwind specification
 *  and a deterministic error specification describe independent properties of those calls.
 */
template <typename Ref, typename Arg>
inline consteval bool print_field_has_hc()
{
	using Char = typename Ref::output_char_type;
	constexpr auto tag{io_reserve_type<Char, ::std::remove_cvref_t<Arg>>};
	if constexpr (checked_print_control<Arg>)
	{
		auto child = []<typename Child>() consteval {
			using Field = decltype(decay::print_semantic_input_forward<Char>(::std::declval<Child &>()));
			return decay::print_semantic_input_forward_herbceptions_throws<Char, Child &> ||
				print_field_has_hc<Ref, decltype(decay::print_runtime_scatter_plan_unwrap(::std::declval<Field &>()))>();
		};
		if constexpr (decay::print_semantic_top_level_condition_v<Arg>)
		{
			using Node = decltype(decay::print_semantic_node_ref(::std::declval<Arg &>()));
			return child.template operator()<decltype((::std::declval<Node &>().t1))>() ||
				child.template operator()<decltype((::std::declval<Node &>().t2))>();
		}
		else if constexpr (::fast_io::details::print_pack<Arg>)
		{
			return [&]<::std::size_t... I>(::std::index_sequence<I...>) {
				return (child.template operator()<decltype(::fast_io::containers::get<I>(::std::declval<Arg &>().storage))>() || ...);
			}(::std::make_index_sequence<::std::remove_cvref_t<Arg>::size>{});
		}
		else return false;
	}
	else if constexpr (scatter_printable_for<Char, Arg &>)
	{
		return throws((print_scatter_define(tag, ::std::declval<Arg &>())));
	}
	else if constexpr (reserve_printable<Char, Arg &> || dynamic_reserve_printable<Char, Arg &>)
	{
		constexpr bool writer{throws((print_reserve_define(tag, ::std::declval<Char *>(), ::std::declval<Arg &>())))};
		if constexpr (reserve_printable<Char, Arg &>) return writer;
		else return writer || throws((print_reserve_size(tag, ::std::declval<Arg &>())));
	}
	else if constexpr (direct_printable_to<Char, Ref, Arg &>)
	{
		return throws((print_define(tag, ::std::declval<Ref &>(), ::std::declval<Arg &>())));
	}
	else return false;
}

/** @brief Tests the whole-record customization before inspecting individual normalized fields. */
template <bool Line, typename Ref, typename... Fields>
inline consteval bool print_record_has_hc()
{
	using namespace ::fast_io::operations::decay;
	if constexpr (defines::has_status_print_define<Line, Ref, Fields...>)
	{
		return throws((status_print_define<Line>(::std::declval<Ref &>(), ::std::declval<Fields &>()...)));
	}
	else return (print_field_has_hc<Ref, decltype(decay::print_runtime_scatter_plan_unwrap(
		::std::declval<Fields &>()))>() || ...);
}

/** @brief Selects the checked facade only if the actual output and field declarations require it. */
template <bool Line, typename Output, typename... Args>
inline constexpr bool checked_print_record = [] {
	if constexpr (requires { ::fast_io::operations::output_stream_ref(::std::declval<Output &>()); })
	{
		using Ref = ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(::std::declval<Output &>()))>;
		using Leaf = transcode_endpoint_ref_t<false, Ref>;
		using Char = typename Leaf::output_char_type;
		if constexpr (requires { (decay::print_semantic_input_forward<Char>(::std::declval<Args &>()), ...); })
			return (decay::print_semantic_input_forward_herbceptions_throws<Char, Args &> || ...) ||
				print_record_has_hc<Line, Leaf, decltype(decay::print_semantic_input_forward<Char>(
					::std::declval<Args &>()))...>();
	}
	return false;
}();

/** @brief Executes one deterministic field's declared representation on the real normalized observer. */
template <typename Ref, typename Arg>
inline void print_checked_field(Ref &out, Arg &value) throws
{
	using Char = typename Ref::output_char_type;
	constexpr auto tag{io_reserve_type<Char, ::std::remove_cvref_t<Arg>>};
	if constexpr (checked_print_control<Arg>)
	{
		visit_checked_print_control<Char>(value, [&](auto &field) throws { print_checked_field(out, field); });
	}
	else if constexpr (!print_field_has_hc<Ref, Arg>())
	{
		::fast_io::operations::decay::print_freestanding_decay_borrowed_output_and_arguments<false>(out, value);
	}
	else if constexpr (scatter_printable_for<Char, Arg &>)
	{
		auto span{print_scatter_define(tag, value)};
		if (span.len) ::fast_io::operations::decay::write_all_decay_dispatch(out, span.base, span.base + span.len);
	}
	else if constexpr (reserve_printable<Char, Arg &> || dynamic_reserve_printable<Char, Arg &>)
	{
		auto emit = [&](Char *first) throws {
			auto last{print_reserve_define(tag, first, value)};
			::fast_io::operations::decay::write_all_decay_dispatch(out, first, last);
		};
		if constexpr (reserve_printable<Char, Arg &>)
		{
			constexpr auto size{print_reserve_size(tag)};
			if constexpr (decay::print_stack_buffer_size_within_limit<size, Char>)
			{
				Char buffer[size ? size : 1u];
				emit(buffer);
			}
			else
			{
				local_operator_new_array_ptr<Char> buffer(size);
				emit(buffer.ptr);
			}
		}
		else
		{
			auto size{print_reserve_size(tag, value)};
			local_operator_new_array_ptr<Char> buffer(size ? size : 1u);
			emit(buffer.ptr);
		}
	}
	else print_define(tag, out, value);
}

/** @brief Preserves the endpoint lock, whole-record customization and caller-owned finish boundary.
 *  @details A following field or newline is entered only after the preceding callback succeeds.
 */
template <bool Line, typename Output, typename... Args>
inline void print_checked_record(Output &&out, Args &&...args) throws
{
	decltype(auto) ref{::fast_io::operations::output_stream_ref(out)};
	using Char = typename ::std::remove_cvref_t<decltype(ref)>::output_char_type;
	auto body = [&](auto &leaf) throws {
		auto emit = [&](auto &&...fields) throws {
			using Ref = ::std::remove_cvref_t<decltype(leaf)>;
			if constexpr (::fast_io::operations::decay::defines::has_status_print_define<Line, Ref,
				::std::remove_reference_t<decltype(fields)>...>)
				status_print_define<Line>(leaf, fields...);
			else
			{
				(print_checked_field(leaf, decay::print_runtime_scatter_plan_unwrap(fields)), ...);
				if constexpr (Line)
					::fast_io::operations::decay::char_put_decay_dispatch(leaf, char_literal_v<u8'\n', Char>);
			}
		};
		emit(decay::print_semantic_input_forward<Char>(args)...);
	};
	with_transcode_output_checked(ref, body);
	if constexpr (!::std::is_lvalue_reference_v<Output> && temporary_output_finish_enabled<::std::remove_cvref_t<Output>>)
		output_stream_finish_define(ref);
}
} // namespace fast_io::details
#endif

namespace fast_io::details
{
/** @brief Selects the formatting ABI used by issue-time snapshots and opaque async endpoints.
 *  @details Only the device operation is marked. Unmarked fields are evaluated with the same
 *  declared effects as a synchronous print before borrowed input can go out of scope.
 */
template <bool Line, typename Output, typename... Args>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void print_snapshot_fields(Output &&out, Args &&...args)
#if defined(__HERBCEPTIONS__)
	throws(checked_print_record<Line, Output, Args...>)
#endif
{
#if defined(__HERBCEPTIONS__)
	if constexpr (checked_print_record<Line, Output, Args...>)
		print_checked_record<Line>(::std::forward<Output>(out), ::std::forward<Args>(args)...);
	else
#endif
		::fast_io::operations::print_freestanding<Line>(::std::forward<Output>(out), ::std::forward<Args>(args)...);
}
}
