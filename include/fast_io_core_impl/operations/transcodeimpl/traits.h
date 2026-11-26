#pragma once

/**
 * @file
 * @brief Defines allocation and publication policy for stream transcoders.
 *
 * Sizes are expressed in elements of the corresponding endpoint type, never
 * in bytes. Traits are deliberately static: adapters can specialize storage
 * without adding a runtime policy object to every stream instance.
 */

namespace fast_io
{

namespace details
{
template <typename Ref>
inline constexpr transcode_buffer_sizes transcode_preferred_sizes = [] {
	if constexpr (transcode_buffer_sizes_provider<Ref>)
	{
		return transcode_preferred_buffer_sizes_define(transcode_reserve<Ref>);
	}
	else
	{
		return transcode_buffer_sizes{};
	}
}();

/** @brief Rounds a byte hint up to whole units, within the configured allocation budget. */
template <typename Unit>
inline constexpr ::std::size_t transcode_buffer_units(::std::size_t bytes, ::std::size_t budget) noexcept
{
	if (!bytes)
	{
		return budget;
	}
	auto const units{1u + (bytes - 1u) / sizeof(Unit)};
	return units < budget ? units : budget;
}

template <typename Ref>
inline constexpr ::std::size_t input_transcode_bytes = [] {
	if constexpr (requires { typename ::fast_io::details::compile_time_size_constant<input_stream_transcode_bytes_define(io_type<Ref>)>; })
	{
		return input_stream_transcode_bytes_define(io_type<Ref>);
	}
	else
	{
		return ::std::size_t{};
	}
}();

template <typename Ref>
inline constexpr ::std::size_t output_transcode_bytes = [] {
	if constexpr (requires { typename ::fast_io::details::compile_time_size_constant<output_stream_transcode_bytes_define(io_type<Ref>)>; })
	{
		return output_stream_transcode_bytes_define(io_type<Ref>);
	}
	else
	{
		return ::std::size_t{};
	}
}();

/** @brief Converts the configured byte budget to a nonzero endpoint-unit count. */
template <::fast_io::transcode_unit unit_type>
inline constexpr ::std::size_t default_transcode_buffer_size() noexcept
{
	// Preserve the library-wide byte budget while guaranteeing at least one
	// complete endpoint unit on targets with unusually wide integer types.
	constexpr ::std::size_t bytes{
#ifdef FAST_IO_BUFFER_SIZE
		// Honor the library-wide caller override when one is configured.
		FAST_IO_BUFFER_SIZE
#elif SIZE_MAX <= UINT_LEAST16_MAX
		// Keep allocations small on 16-bit address spaces.
		128u
#elif SIZE_MAX <= UINT_LEAST32_MAX
		// Use the established medium budget on 32-bit targets.
		8192u
#else
		// Favor amortized throughput on wide hosted address spaces.
		131072u
#endif
	};
	return bytes < sizeof(unit_type) ? 1u : bytes / sizeof(unit_type);
}

} // namespace details

/** @brief Defines allocation, size, and clearing policy for output adapters. */
template <::std::integral public_char, ::fast_io::transcode_unit transformed_unit,
		  typename allocator = ::fast_io::native_global_allocator>
struct basic_otranscoder_traits
{
	using allocator_type = allocator;
	using public_char_type = public_char;
	using transformed_unit_type = transformed_unit;

	static inline constexpr ::std::size_t public_buffer_size{
		::fast_io::details::default_transcode_buffer_size<public_char_type>()};
	static inline constexpr ::std::size_t transform_buffer_size{
		::fast_io::details::default_transcode_buffer_size<transformed_unit_type>()};
	static inline constexpr bool secure_clear = false;
};

/** @brief Controls when decoded input may become visible to its caller. */
enum class transcode_input_publication_mode : ::std::uint_least8_t
{
	// Low-latency streaming; authentication may still fail at terminal finish.
	streaming_unverified,
	// Reserved for adapters that retain a complete authenticated message.
	hold_until_authenticated,
	// Every emitted unit belongs to a verified record. Final EOF validates the stream boundary.
	authenticated_records
};

/** @brief Defines allocation, size, clearing, and publication policy for input. */
template <::std::integral public_char,
		  ::fast_io::transcode_unit source_unit,
		  ::fast_io::transcode_unit transformed_unit,
		  typename allocator = ::fast_io::native_global_allocator>
struct basic_itranscoder_traits
{
	using allocator_type = allocator;
	using public_char_type = public_char;
	using source_unit_type = source_unit;
	using transformed_unit_type = transformed_unit;

	static inline constexpr ::std::size_t public_buffer_size{
		::fast_io::details::default_transcode_buffer_size<public_char_type>()};
	static inline constexpr ::std::size_t source_buffer_size{
		::fast_io::details::default_transcode_buffer_size<source_unit_type>()};
	static inline constexpr ::std::size_t transform_buffer_size{
		::fast_io::details::default_transcode_buffer_size<transformed_unit_type>()};
	static inline constexpr bool secure_clear = false;
	static inline constexpr transcode_input_publication_mode publication_mode{
		transcode_input_publication_mode::streaming_unverified};
};

/** @brief An engine declaring this capability must only emit plaintext from a verified record.
 * @details Both process and finish must quarantine tentative output privately. The
 * marker is a semantic obligation of the provider, with no whole-message guarantee.
 */
template <typename engine>
concept authenticated_record_transcoder = bounded_transcoder<engine> && requires {
	{ transcode_authenticated_records_define(io_type_t<engine>{}) } -> ::std::same_as<::std::true_type>;
};

/** @brief Identifies engines whose adapter-owned staging storage should be wiped on release. */
template <typename engine>
concept sensitive_transcoder = requires {
	{ transcode_sensitive_define(io_type_t<engine>{}) } -> ::std::same_as<::std::true_type>;
};

template <typename owner> struct basic_otranscoder_ref;
template <typename owner> struct basic_iotranscoder_ref;

namespace details
{
#if defined(__HERBCEPTIONS__)
template <typename T> inline constexpr bool sync_transcode_output_checked = false;
template <typename Owner> inline constexpr bool sync_transcode_output_checked<basic_otranscoder_ref<Owner>> = Owner::checked_output;
template <typename Owner> inline constexpr bool sync_transcode_output_checked<basic_iotranscoder_ref<Owner>> =
	Owner::output_adapter_type::checked_output;

template <typename Owner>
inline Owner &transcode_output_owner(basic_otranscoder_ref<Owner> ref) noexcept { return *ref.ptr; }
template <typename Owner>
inline auto &transcode_output_owner(basic_iotranscoder_ref<Owner> ref) noexcept { return ref.ptr->output; }
#endif

/** @brief Keeps every child buffer window inside the lifetime of its corresponding directional lock. */
template <bool Input,typename Ref,typename Operation>
inline decltype(auto) with_transcode_endpoint(Ref &ref,Operation &&operation)
{
	namespace decay = ::fast_io::operations::decay;
	if constexpr (Input && decay::defines::has_input_or_io_stream_mutex_ref_define<Ref>)
	{
		static_assert(decay::defines::has_complete_input_stream_mutex_protocol<Ref>);
		decay::stream_ref_decay_lock_guard lock{decay::input_stream_mutex_ref_decay(ref)};
		decltype(auto) child{decay::input_stream_unlocked_ref_decay(ref)};
		return with_transcode_endpoint<Input>(child,operation);
	}
	else if constexpr (!Input && decay::defines::has_output_or_io_stream_mutex_ref_define<Ref>)
	{
		static_assert(decay::defines::has_complete_output_stream_mutex_protocol<Ref>);
		decay::stream_ref_decay_lock_guard lock{decay::output_stream_mutex_ref_decay(ref)};
		decltype(auto) child{decay::output_stream_unlocked_ref_decay(ref)};
		return with_transcode_endpoint<Input>(child,operation);
	}
	else return operation(ref);
}

/** @brief Returns the type of the unlocked endpoint reached by the directional lock protocol. */
struct transcode_endpoint_identity
{
	template <typename Ref>
	constexpr ::std::type_identity<Ref> operator()(Ref &) const noexcept { return {}; }
};

template <bool Input,typename Ref>
using transcode_endpoint_ref_t = typename decltype(with_transcode_endpoint<Input>(::std::declval<Ref &>(),
	transcode_endpoint_identity{}))::type;

#if defined(__HERBCEPTIONS__)
/** @brief Keeps a checked output operation within the existing directional mutex boundary. */
template <typename Ref, typename Operation>
inline void with_transcode_output_checked(Ref &ref, Operation &operation) throws
{
	namespace decay = ::fast_io::operations::decay;
	if constexpr (decay::defines::has_output_or_io_stream_mutex_ref_define<Ref>)
	{
		static_assert(decay::defines::has_complete_output_stream_mutex_protocol<Ref>);
		decay::stream_ref_decay_lock_guard lock{decay::output_stream_mutex_ref_decay(ref)};
		decltype(auto) child{decay::output_stream_unlocked_ref_decay(ref)};
		with_transcode_output_checked(child, operation);
	}
	else
	{
		operation(ref);
	}
}
#endif

/** @brief Queries a directional observer without acquiring or bypassing its mutex at runtime. */
template <bool Input,typename Ref>
consteval bool transcode_stream_sensitive()
{
	if constexpr (Input && requires { Ref::input_secure_clear; }) return Ref::input_secure_clear;
	else if constexpr (!Input && requires { Ref::output_secure_clear; }) return Ref::output_secure_clear;
	else if constexpr (Input && requires (Ref r) { input_stream_unlocked_ref_define(r); })
		return transcode_stream_sensitive<Input,::std::remove_cvref_t<decltype(input_stream_unlocked_ref_define(::std::declval<Ref &>()))>>();
	else if constexpr (!Input && requires (Ref r) { output_stream_unlocked_ref_define(r); })
		return transcode_stream_sensitive<Input,::std::remove_cvref_t<decltype(output_stream_unlocked_ref_define(::std::declval<Ref &>()))>>();
	else return false;
}

/** @brief Carries erasure and verified-publication guarantees into stream adapter storage. */
template <typename base, bool verified>
struct sensitive_transcode_traits : base
{
	static inline constexpr bool secure_clear = true;
	static inline constexpr auto publication_mode = verified ? transcode_input_publication_mode::authenticated_records
		: transcode_input_publication_mode::streaming_unverified;
};

template <typename engine, typename base, bool child_sensitive>
using transcode_input_storage_traits = ::std::conditional_t<child_sensitive || sensitive_transcoder<engine> || authenticated_record_transcoder<engine>,
	sensitive_transcode_traits<base, authenticated_record_transcoder<engine>>, base>;

template <typename engine, typename base, bool child_sensitive>
using transcode_output_storage_traits = ::std::conditional_t<child_sensitive || sensitive_transcoder<engine>,
	sensitive_transcode_traits<base, false>, base>;

template <typename Base, ::std::size_t Bytes>
struct transcode_public_buffer_traits : Base
{
	static inline constexpr ::std::size_t public_buffer_size{
		transcode_buffer_units<typename Base::public_char_type>(Bytes, Base::public_buffer_size)};
};

template <typename Base, ::std::size_t Bytes>
using transcode_sized_traits = ::std::conditional_t<Bytes == 0, Base, transcode_public_buffer_traits<Base, Bytes>>;

template <typename Engine, typename Base, typename Storage>
using transcode_input_traits_for = transcode_sized_traits<transcode_input_storage_traits<Engine, Base,
	transcode_stream_sensitive<true,::std::remove_cvref_t<decltype(operations::input_stream_ref(::std::declval<Storage &>().get()))>>()>,
	transcode_preferred_sizes<::fast_io::operations::transcode_engine_ref_t<Engine>>.output_bytes>;

/** @brief Bare devices amortize writes in device-sized batches. A writable window can absorb a SIMD-sized batch. */
template <typename Engine, typename Base, typename Storage>
inline constexpr ::std::size_t transcode_output_batch_bytes = [] {
	using Ref = transcode_endpoint_ref_t<false,::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(::std::declval<Storage &>().get()))>>;
	constexpr auto preferred{transcode_preferred_sizes<::fast_io::operations::transcode_engine_ref_t<Engine>>.input_bytes};
	if constexpr (preferred && !operations::decay::defines::has_obuffer_basic_operations<Ref>)
	{
		constexpr auto device{output_transcode_bytes<Ref>};
		// A record protocol advertises its payload extent. A plain unbuffered sink uses the ordinary I/O budget.
		return device > preferred ? device : (device ? preferred : ::std::size_t{});
	}
	else
	{
		return preferred;
	}
}();

template <typename Engine, typename Base, typename Storage>
using transcode_output_traits_for = transcode_sized_traits<transcode_output_storage_traits<Engine, Base,
	transcode_stream_sensitive<false,::std::remove_cvref_t<decltype(operations::output_stream_ref(::std::declval<Storage &>().get()))>>()>,
	transcode_output_batch_bytes<Engine, Base, Storage>>;
}

} // namespace fast_io
