#pragma once

#if defined(__HERBCEPTIONS__)
namespace fast_io::details
{

/** @brief Recognizes control nodes whose children are formatted in the active print operation. */
template <typename T>
inline constexpr bool checked_print_control = decay::print_semantic_top_level_condition_v<T> ||
	::fast_io::details::print_pack<T> || ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;

/** @brief Visits only the selected condition arm or expands a pack in source order.
 *  @details Each active member is normalized once and borrowed throughout its synchronous callback.
 *  An empty node emits no field. The caller owns the newline and completion of the entire record.
 */
template <typename Char, typename Arg, typename Emit>
inline constexpr void visit_checked_print_control(Arg &arg, Emit emit) throws
{
	auto member = [&](auto &child) throws {
		decltype(auto) field{decay::print_semantic_input_forward<Char>(child)};
		emit(decay::print_runtime_scatter_plan_unwrap(field));
	};
	if constexpr (decay::print_semantic_top_level_condition_v<Arg>)
	{
		auto &node{decay::print_semantic_node_ref(arg)};
		if (node.pred) member(node.t1);
		else member(node.t2);
	}
	else if constexpr (::fast_io::details::print_pack<Arg>)
	{
		[&]<::std::size_t... I>(::std::index_sequence<I...>) throws {
			(member(::fast_io::containers::get<I>(arg.storage)), ...);
		}(::std::make_index_sequence<::std::remove_cvref_t<Arg>::size>{});
	}
}

/** @brief Owns the first failed write until a formatter with an ordinary ABI returns.
 *  @details A formatter can issue more writes and execute user code after the failed call.
 *  Later writes discard their input. Destruction also releases a retained C++ exception
 *  if the formatter itself unwinds before print can transfer ownership to the caller.
 */
struct transcode_callback_error
{
	::std::cxx_std_error failure{};

	transcode_callback_error() = default;
	transcode_callback_error(transcode_callback_error const &) = delete;
	transcode_callback_error &operator=(transcode_callback_error const &) = delete;

	~transcode_callback_error()
	{
		auto domain{static_cast<::std::error_domain_singleton const *>(failure.domain)};
		if (domain && domain->do_cleanup)
		{
			domain->do_cleanup(failure.code);
		}
	}

	/** @brief Transfers the retained error before another argument or a terminal flush can execute. */
	inline void deliver() throws
	{
		if (failure.domain) [[unlikely]]
		{
			auto error{failure};
			failure = {};
			throw throws error;
		}
	}
};

/** @brief Borrows the encoder for one direct formatter and selects its write effect at instantiation.
 *  @details The checked observer propagates an error at the write. The ordinary observer records
 *  that same error on the stack of the active print call. Neither observer owns the stream.
 */
template <typename Owner, bool Immediate>
struct transcode_callback_ref
{
	using output_char_type = typename Owner::output_char_type;
	Owner *owner;
	[[no_unique_address]] ::std::conditional_t<Immediate, empty, transcode_callback_error *> errors;
};

/** @brief Preserves the borrowed write channel through public pointer-range operations. */
template <typename Owner, bool Immediate>
inline constexpr auto output_stream_ref_define(transcode_callback_ref<Owner, Immediate> ref) noexcept
{
	return ref;
}

/** @brief Exposes the checked writer to a callback whose ABI can propagate its failure. */
template <typename Owner>
inline void write_all_checked_define(transcode_callback_ref<Owner, true> ref,
	typename Owner::output_char_type const *first, typename Owner::output_char_type const *last) throws
{
	ref.owner->write_checked(first, last);
}

/** @brief Writes object bytes through the encoder's byte binding, retaining any partial public unit. */
template <typename Owner>
inline void write_all_checked_define(transcode_callback_ref<Owner, true> ref,
	::std::byte const *first, ::std::byte const *last) throws
{
	ref.owner->template write_all_bytes_overflow_impl<true>(first, last);
}

/** @brief Executes an ordinary callback's writes until its first checked failure. */
template <typename Owner>
inline void write_all_overflow_define(transcode_callback_ref<Owner, false> ref,
	typename Owner::output_char_type const *first, typename Owner::output_char_type const *last)
{
	if (!ref.errors->failure.domain)
	{
		try { ref.owner->write_checked(first, last); }
		catch throws (::std::error error) { ref.errors->failure = error.release(); }
	}
}

/** @brief Retains the first byte-domain error at the same boundary as an ordinary typed callback. */
template <typename Owner>
inline void write_all_bytes_overflow_define(transcode_callback_ref<Owner, false> ref,
	::std::byte const *first, ::std::byte const *last)
{
	if (!ref.errors->failure.domain)
	{
		try { ref.owner->template write_all_bytes_overflow_impl<true>(first, last); }
		catch throws (::std::error error) { ref.errors->failure = error.release(); }
	}
}

} // namespace fast_io::details
#endif
