#pragma once
// Included inside fast_io::fmt with the ordinary grammar facades in scope.

namespace details
{

/** @brief Retains the original argument owners for a destination-specific formatting protocol. */
template <basic_fixed_string Literal, typename Grammar, typename O, typename... Values>
inline auto defer_format(O &&output, Values &&...values)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	namespace op = ::fast_io::async::details;
	// The format group is indivisible on this endpoint. Consume its original values before mutable aliases escape.
	auto &&out{op::operand(static_cast<O &&>(output))};
	lower_format_program_dispatch<Literal,Grammar>(print_lowered_components<decltype(out)>{static_cast<decltype(out)>(out)},values...);
	return op::printed_during_issue();
}

/**
 * @brief Lowers a structural format literal for a marked output while retaining the lifetime of every operand.
 * @details A completion-based stream stages eligible components at the call site, before format temporaries expire.
 * A destination-specific operation owns or borrows the original arguments and lowers them inside its task, so a
 * scanner, direct printer or status provider never observes a reference to a destroyed lowering temporary.
 */
template <basic_fixed_string Literal, bool AsyncDevice, typename Grammar, typename O, typename... Values>
FAST_IO_FMT_GCC_RULE_INLINE inline auto print_async_task(Grammar, O &&output, Values &&...values)
#if defined(__HERBCEPTIONS__)
	throws
#endif
{
	namespace op = ::fast_io::async::details;
	using ref = op::output_ref_type<O>;
	static_assert(::std::same_as<typename decltype(Literal)::value_type, typename ref::output_char_type>);
	if constexpr (!AsyncDevice || ::fast_io::async::awaitable_output<ref> || ::fast_io::async::immediate_output_range<ref> || ::fast_io::async::immediate_output_reserve<ref>)
	{
		using char_type = typename ref::output_char_type;
		using record_type = op::print_record<char_type>;
		using fallback_type = ::fast_io::async::task<void> (*)(O &&, Values &&...)
#if defined(__HERBCEPTIONS__)
			throws
#endif
			;
		record_type record;
		auto fallback{lower_format_program_dispatch<Literal, Grammar>([&]<typename... Components>(Components &&...components) -> fallback_type {
			if constexpr (op::staged_print<false, O, Components...>)
			{
				typename record_type::base buffer;
				::fast_io::operations::print_freestanding<false>(buffer, static_cast<Components &&>(components)...);
				record.adopt(buffer);
				return nullptr;
			}
			else
			{
				return &defer_format<Literal, Grammar, O, Values...>;
			}
		},
																	  values...)};
		if (fallback != nullptr)
		{
			return fallback(static_cast<O &&>(output), static_cast<Values &&>(values)...);
		}
		return op::emit_record<AsyncDevice>(
			op::capture<O>(static_cast<O &&>(output)), static_cast<record_type &&>(record));
	}
	else
	{
		return defer_format<Literal, Grammar>(static_cast<O &&>(output), static_cast<Values &&>(values)...);
	}
}

/**
 * @brief Retains the exact lowered operand types for resident formatting without running the grammar twice.
 * @details A snapshot copies every temporary before its lowering callback returns. An error before that
 * callback can still be stored in the same public result. Trivial default construction allows the failure
 * representation to leave harmless, unused operands behind without calling user code.
 */
template <typename O, typename... Components>
struct buffered_format_plan
{
	static constexpr bool value{::fast_io::async::details::staged_print<false, O, Components...>};
	static constexpr bool reserved{value && ::fast_io::async::details::reservable_record<false, O, Components...> &&
								   (::std::is_trivially_default_constructible_v<::std::remove_cvref_t<Components>> && ...)};

#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
	/** @brief Constructs a failed result when lowering stopped before its transient fields existed. */
	template <typename SaveError>
	static auto failure(O &&device, SaveError save_error)
		requires reserved
	{
		namespace op = ::fast_io::async::details;
		return [&device, &save_error, ... empty = op::record_argument<::std::remove_cvref_t<Components>>{}]() mutable {
			auto result{op::make_reserved_print<false, O, Components...>(
				static_cast<O &&>(device), static_cast<Components &&>(empty.value)...)};
			save_error(result);
			return result;
		}();
	}
#endif
};

/** @brief Classifies the grammar's lowered public arguments without formatting or reading any value. */
template <typename O>
struct buffered_format_probe
{
	template <typename... Components>
	constexpr auto operator()(Components &&...) const noexcept
	{
		return buffered_format_plan<O, Components...>{};
	}
};

/** @brief Binds formatting and early lowering errors to the lifetime of the public I/O result. */
template <basic_fixed_string Literal, typename Grammar, typename O, typename... Values>
FAST_IO_FMT_GCC_RULE_INLINE inline auto make_format_work(Grammar grammar, O &&output, Values &&...values)
{
	namespace op = ::fast_io::async::details;
	using output_char_type = typename op::output_ref_type<O>::output_char_type;
	static_assert(::std::same_as<typename decltype(Literal)::value_type, output_char_type>);
	if constexpr (!op::layered_device<O> && op::buffered_record_output<O>)
	{
		using eligible = decltype(lower_format_program_dispatch<Literal, Grammar>(buffered_format_probe<O>{}, values...));
		if constexpr (eligible::reserved)
		{
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
			try
#endif
			{
				// The callback owns every component before its transient grammar carrier is destroyed.
				return lower_format_program_dispatch<Literal, Grammar>([&]<typename... C>(C &&...components) {
					return op::make_reserved_print<false, O, C...>(
						static_cast<O &&>(output), static_cast<C &&>(components)...);
				},
																	   values...);
			}
#if defined(__HERBCEPTIONS__)
			catch throws(::std::error failure)
			{
				return eligible::failure(static_cast<O &&>(output), [&](auto &result) { result.creation_error(failure); });
			}
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			catch (...)
			{
				return eligible::failure(static_cast<O &&>(output), [](auto &result) { result.creation_exception(); });
			}
#endif
		}
		else if constexpr (eligible::value)
		{
			using ref = op::normalized_output<O>;
			using work = op::buffered_record<ref, false, op::native_buffer_path_t<false, O, op::output_ref_type<O>>>;
			op::resident_operation<void, work> result{::std::in_place,
																				 ::fast_io::operations::output_stream_ref(op::operand(static_cast<O &&>(output)))};
			auto &record{result.staging().record};
#if defined(__HERBCEPTIONS__) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
			try
#endif
			{
				typename op::print_record<output_char_type>::base buffer;
				lower_format_program_dispatch<Literal, Grammar>([&]<typename... Components>(Components &&...components) {
					::fast_io::operations::print_freestanding<false>(buffer, static_cast<Components &&>(components)...);
				},
																values...);
				record.adopt(buffer);
			}
#if defined(__HERBCEPTIONS__)
			catch throws(::std::error failure) { result.creation_error(failure); }
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
			catch (...) { result.creation_exception(); }
#endif
			return result;
		}
		else
		{
			return op::make_io_operation(print_async_task<Literal, true, Grammar, O, Values...>, grammar,
				static_cast<O &&>(output), static_cast<Values &&>(values)...);
		}
	}
	else
	{
		return op::make_io_operation(print_async_task<Literal, true, Grammar, O, Values...>, grammar,
			static_cast<O &&>(output), static_cast<Values &&>(values)...);
	}
}

/** @brief Checks the grammar and the file protocol selected by the binding in the actual character domain. */
template <basic_fixed_string Literal,typename Grammar,typename P,typename O,typename... Args>
consteval bool format_values_supported() noexcept
{
	using C=typename decltype(Literal)::value_type;
	using R=::fast_io::async::details::output_ref_type<O>;
	using binding=::fast_io::async::details::bound_points<P,::fast_io::async::details::format_graph<O,Args...>>;
	if constexpr (!binding::async_output && ::fast_io::async::awaitable_output<R> &&
		!::fast_io::operations::decay::defines::writable<R> && !::fast_io::operations::decay::defines::bytes_writable<R>)
		return false;
	else if constexpr (!::std::same_as<C,typename R::output_char_type>) return false;
	else if constexpr ((::fast_io::async::annotated_operand<Args> || ...))
		return async_literals<Literal,Grammar,sizeof...(Args)>.valid && ::fast_io::async::details::async_print_pack<false,O,Args...>();
	else return true;
}

/** @brief Applies the grammar to the file's bound observable points and preserves transient lowering lifetimes. */
template <basic_fixed_string Literal,typename Grammar,typename P,typename O,typename... Args>
inline auto print_async(Grammar grammar,P,O &&out,Args &&...args)
{
	namespace op = ::fast_io::async::details;
	using L = op::format_graph<O,Args...>;
	using B = op::bound_points<P,L>;
	auto factory = [&] {
		if constexpr ((::fast_io::async::annotated_operand<Args> || ...))
		{
			auto values=::fast_io::containers::forward_as_tuple(static_cast<Args &&>(args)...);
			return [&]<::std::size_t... N>(::std::index_sequence<N...>) {
				return op::issue_mapped_print_values<false, B, op::kernel_only_output<B>>(::std::index_sequence<(N / 2u)...>{},
																						  op::select_endpoint<B::async_output, typename B::output>(static_cast<O &&>(out)), format_field<N, async_literals<Literal, Grammar, sizeof...(Args)>, true>(values)...);
			}(::std::make_index_sequence<2u * sizeof...(Args) + 1u>{});
		}
		else if constexpr (!B::async_output)
			return op::make_io_operation(print_async_task<Literal,false,Grammar,O,Args...>,grammar,
				static_cast<O &&>(out),static_cast<Args &&>(args)...);
		else return make_format_work<Literal>(grammar,op::select_endpoint<true,typename B::output>(static_cast<O &&>(out)),static_cast<Args &&>(args)...);
	};
	return ::fast_io::async::async_operation<::fast_io::async::details::observed_graph<L,B>,decltype(factory())>{io_type<B>,factory};
}
/** @brief Resolves operand selections in the source grammar before lowering inserts literal fragments. */
template <basic_fixed_string Literal,typename Grammar,typename O,typename... Args>
concept annotated_format = ::fast_io::async::details::has_annotation<O,Args...> &&
	::fast_io::async::output_device<::fast_io::async::details::unannotated_t<O>> &&
	::fast_io::async::details::binds_points<::fast_io::async::details::output_request<O,Args...>,
		::fast_io::async::details::format_graph<::fast_io::async::details::unannotated_t<O>,Args...>> &&
	format_values_supported<Literal,Grammar,::fast_io::async::details::output_request<O,Args...>,
		::fast_io::async::details::unannotated_t<O>,Args...>();

/** @brief Keeps the original argument indices while transferring owned arguments to the formatter. */
template <basic_fixed_string Literal,typename Grammar,typename O,typename... Args>
inline auto print_annotated(Grammar grammar,O &&out,Args &&...args)
{
	using request = ::fast_io::async::details::output_request<O,Args...>;
	return print_async<Literal>(grammar,request{},::fast_io::async::details::value_operand(static_cast<O &&>(out)),
		static_cast<Args &&>(args)...);
}
} // namespace details

/** @brief Formats an annotated record using the brace grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,brace_fmt_t,O,Args...>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto print(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(brace_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the brace grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,brace_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,wchar_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto wprint(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(brace_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the brace grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,brace_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char8_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u8print(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(brace_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the brace grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,brace_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char16_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u16print(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(brace_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the brace grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,brace_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char32_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u32print(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(brace_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the percent grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,printf_fmt_t,O,Args...>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto printf(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(printf_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the percent grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,printf_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,wchar_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto wprintf(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(printf_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the percent grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,printf_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char8_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u8printf(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(printf_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the percent grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,printf_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char16_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u16printf(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(printf_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}

/** @brief Formats an annotated record using the percent grammar. */
template <basic_fixed_string Literal,typename O,typename... Args>
	requires details::annotated_format<Literal,printf_fmt_t,O,Args...> &&
		::std::same_as<typename decltype(Literal)::value_type,char32_t>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE inline auto u32printf(O &&out,Args &&...args)
{
	return details::print_annotated<Literal>(printf_fmt_t{},static_cast<O &&>(out),static_cast<Args &&>(args)...);
}
