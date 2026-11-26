#pragma once
/** @file @brief Single-value binary output conversions and authenticated input targets. */
#include "fast_io_public_key.h"
#include "fast_io_secure/rfc8188_crypto.h"
#include <optional>

namespace fast_io::secure
{
/** @brief Tests GCM's 32-bit block counter and 64-bit associated-data bit length.
 * @details The formatted record adds a 16-byte tag and the I/O planner uses ptrdiff_t distances.
 * These bounds depend only on public lengths, so fixed-size callers can prove them at compile time.
 */
[[nodiscard]] inline constexpr bool gcm_record_size_fits(::std::size_t size, ::std::size_t aad) noexcept
{
	if constexpr (::std::numeric_limits<::std::size_t>::digits > 32)
	{
		if (size > (1ULL << 36u) - 32u || aad > UINT64_MAX / 8u) return false;
	}
	return size <= static_cast<::std::size_t>(PTRDIFF_MAX) - 16u;
}

/** @brief Borrows admissible message and AAD ranges. No payload or key is read by validation. */
template <::std::size_t Extent>
class basic_gcm_input
{
	::std::span<::std::byte const, Extent> message_;
	details::bytes_view aad_;

	/** @brief Constructs the proof after the length checks in try_bind. */
	constexpr basic_gcm_input(::std::span<::std::byte const, Extent> message, details::bytes_view aad, int) noexcept
		: message_(message), aad_(aad) {}
public:
	/** @brief Constructs a fixed-size proof from two statically bounded byte spans. */
	template <::std::size_t A>
		requires (Extent != ::std::dynamic_extent && A != ::std::dynamic_extent && gcm_record_size_fits(Extent, A))
	constexpr basic_gcm_input(::std::span<::std::byte const, Extent> message, ::std::span<::std::byte const, A> aad) noexcept
		: message_(message), aad_(aad) {}

	/** @brief Admits runtime extents before a nonthrowing output formatter receives them. */
	[[nodiscard]] static constexpr ::std::optional<basic_gcm_input> try_bind(
		::std::span<::std::byte const, Extent> message, details::bytes_view aad = {}) noexcept
	{
		if (!gcm_record_size_fits(message.size(), aad.size())) return {};
		return basic_gcm_input{message, aad, 0};
	}
	/** @brief Retrieves the bounded plaintext. */
	[[nodiscard]] constexpr auto message() const noexcept { return message_; }
	/** @brief Retrieves the associated data whose bit length can be encoded by GHASH. */
	[[nodiscard]] constexpr auto aad() const noexcept { return aad_; }
};

using gcm_input = basic_gcm_input<::std::dynamic_extent>;

namespace details
{
/** @brief Selects binary destinations on which one character represents one arbitrary octet. */
template <typename Char>
concept octet_character = CHAR_BIT == 8 &&
	(::std::same_as<Char, char> || ::std::same_as<Char, signed char> ||
	 ::std::same_as<Char, unsigned char> || ::std::same_as<Char, char8_t>);

/** @brief Preserves a bounded array or span's extent when constructing a conversion. */
template <typename Range>
concept fixed_bytes = requires(Range const &value) {
	{ ::std::span{value} } noexcept;
	requires ::std::same_as<typename decltype(::std::span{value})::value_type, ::std::byte>;
	requires (decltype(::std::span{value})::extent != ::std::dynamic_extent);
};

/** @brief Stores the public nonce by value so the caller may immediately reuse its nonce buffer. */
struct gcm_parameters
{
	::std::span<::std::byte const, 16> key;
	unsigned char nonce[12];

	/** @brief Borrows the key and copies the nonce without expanding a key schedule. */
	gcm_parameters(::std::span<::std::byte const, 16> k, ::std::span<::std::byte const, 12> iv) noexcept : key(k)
	{
		::std::memcpy(nonce, iv.data(), sizeof(nonce));
	}
};
}
}

namespace fast_io::manipulators
{
/** @brief Requests one GCM ciphertext followed by its full 128-bit tag.
 * @details Use a unique nonce for each distinct encryption under the key. Keep the borrowed plaintext,
 * key and associated data stable through synchronous formatting. Output storage must be disjoint from
 * those borrows. Only the emission CPO performs cryptography. Measuring and discarding a field are pure.
 */
template <::std::size_t N>
struct aes128_gcm_encoding
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::secure::details::gcm_parameters parameters;
	::fast_io::secure::basic_gcm_input<N> input;
};

/** @brief Wraps an admitted runtime or compile-time range for synchronous AES-128-GCM output. */
template <::std::size_t N>
[[nodiscard]] inline aes128_gcm_encoding<N> aes128_gcm_encode(::std::span<::std::byte const, 16> key,
	::std::span<::std::byte const, 12> nonce, ::fast_io::secure::basic_gcm_input<N> input) noexcept
{
	return {{key, nonce}, input};
}

/** @brief Proves fixed input extents during overload resolution and preserves them in the printable type. */
template <::fast_io::secure::details::fixed_bytes Message,
	::fast_io::secure::details::fixed_bytes Aad = ::std::span<::std::byte const, 0>>
	requires (::fast_io::secure::gcm_record_size_fits(decltype(::std::span{::std::declval<Message const &>()})::extent,
		decltype(::std::span{::std::declval<Aad const &>()})::extent))
[[nodiscard]] inline auto aes128_gcm_encode(::std::span<::std::byte const, 16> key,
	::std::span<::std::byte const, 12> nonce, Message const &message, Aad const &aad = {}) noexcept
{
	constexpr auto n{decltype(::std::span{message})::extent};
	constexpr auto a{decltype(::std::span{aad})::extent};
	return aes128_gcm_encode(key, nonce, ::fast_io::secure::basic_gcm_input<n>{
		::std::span<::std::byte const, n>{message}, ::std::span<::std::byte const, a>{aad}});
}

/** @brief Publishes a fixed reserve for records that fit the library's stack staging budget. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
	requires (N != ::std::dynamic_extent && N + 16u <= ::fast_io::details::dynamic_reserve_default_static_stack_size<Char>())
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<Char, aes128_gcm_encoding<N>>) noexcept
{
	return N + 16u;
}

/** @brief Measures an exact output extent using only the established GCM length proof. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<Char, aes128_gcm_encoding<N>>,
	aes128_gcm_encoding<N> const &value) noexcept
{
	return value.input.message().size() + 16u;
}

/** @brief An N-byte plaintext always contributes exactly N ciphertext octets and sixteen tag octets. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
	requires (N != ::std::dynamic_extent && ::fast_io::secure::gcm_record_size_fits(N, 0))
inline constexpr ::std::size_t print_reserve_static_precise_size(io_reserve_type_t<Char, aes128_gcm_encoding<N>>) noexcept
{
	return N + 16u;
}

/** @brief Seals directly into the selected output reserve and erases the short-lived expanded key. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
inline Char *print_reserve_define(io_reserve_type_t<Char, aes128_gcm_encoding<N>>, Char *out,
	aes128_gcm_encoding<N> const &value) noexcept
{
	::fast_io::secure::details::aes128gcm_key cipher{value.parameters.key.data()};
	auto dest{reinterpret_cast<unsigned char *>(out)};
	auto message{value.input.message()};
	cipher.seal(value.parameters.nonce, ::fast_io::secure::details::octets(message.data()), message.size(),
		dest, dest + message.size(), value.input.aad());
	return out + message.size() + 16u;
}

/** @brief Provides the exact runtime allocation size for single-pass emission by concat. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
inline constexpr ::std::size_t print_reserve_precise_size(io_reserve_type_t<Char, aes128_gcm_encoding<N>> tag,
	aes128_gcm_encoding<N> const &value) noexcept { return print_reserve_size(tag, value); }

/** @brief Emits exactly the extent measured by print_reserve_precise_size using the common seal kernel. */
template <::fast_io::secure::details::octet_character Char, ::std::size_t N>
inline Char *print_reserve_precise_define(io_reserve_type_t<Char, aes128_gcm_encoding<N>> tag, Char *out,
	::std::size_t, aes128_gcm_encoding<N> const &value) noexcept { return print_reserve_define(tag, out, value); }

/** @brief Describes the authenticated input field. Its destination length determines the ciphertext length.
 * @details The next sixteen octets are the tag. Extra octets belong to subsequent fields. The target, key,
 * and AAD must stay alive through scan completion. The target must be disjoint from unread subsequent fields.
 */
struct aes128_gcm_decoding
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::secure::details::gcm_parameters parameters;
	::std::span<::std::byte> destination;
	::fast_io::secure::details::bytes_view aad;
};

/** @brief Marks one target for authenticated scan. Construction reads the nonce and leaves the target untouched. */
[[nodiscard]] inline aes128_gcm_decoding aes128_gcm_decoded_get(::std::span<::std::byte const, 16> key,
	::std::span<::std::byte const, 12> nonce, ::std::span<::std::byte> destination,
	::fast_io::secure::details::bytes_view aad = {}) noexcept
{
	return {{key, nonce}, destination, aad};
}

/** @brief Holds only ciphertext and its tag across refills. A contiguous field requires no allocation. */
struct gcm_scan_state
{
	::fast_io::details::local_operator_new_array_ptr<unsigned char> fragments;
	::std::size_t used{};
};

/** @brief Requests an operation-local scanner state. Each submitted scan has independent progress. */
template <::fast_io::secure::details::octet_character Char>
inline constexpr auto scan_context_type(io_reserve_type_t<Char, aes128_gcm_decoding>) noexcept
{
	return ::fast_io::io_type<gcm_scan_state>;
}

/** @brief Certifies the cursor returned by every state transition.
 * @details Size rejection and an empty chunk return first. A short fragment returns last. A complete
 * field advances by its missing octets, which can only complete when that count fits in this chunk.
 */
template <::fast_io::secure::details::octet_character Char>
inline constexpr ::std::true_type scan_context_result_in_range(io_reserve_type_t<Char, aes128_gcm_decoding>) noexcept
{
	return {};
}

/** @brief Consumes exactly one authenticated field, retaining fragmented input privately until its tag arrives.
 * @details All failure exits preserve the original target. Contiguous disjoint input and exact in-place input
 * use the provider directly. Partial overlap is staged so CTR stores cannot clobber unread ciphertext.
 */
template <::fast_io::secure::details::octet_character Char>
inline ::fast_io::parse_result<Char const *> scan_context_define(io_reserve_type_t<Char, aes128_gcm_decoding>,
	gcm_scan_state &state, Char const *first, Char const *last, aes128_gcm_decoding const &value) noexcept
{
	auto size{value.destination.size()};
	if (!::fast_io::secure::gcm_record_size_fits(size, value.aad.size())) return {first, parse_code::overflow};
	if (first == last) return {first, parse_code::partial};
	auto extent{size + 16u};
	auto remaining{extent - state.used};
	auto available{static_cast<::std::size_t>(last - first)};
	auto source{reinterpret_cast<unsigned char const *>(first)};
	auto dest{::fast_io::secure::details::octets(value.destination.data())};
	::std::less<unsigned char const *> less;
	bool const overlap{size && available >= extent && dest != source &&
		less(dest, source + size) && less(source, dest + size)};
	if (state.used || available < extent || overlap)
	{
		if (!state.used) state.fragments.allocate_new(extent);
		auto amount{available < remaining ? available : remaining};
		::std::memcpy(state.fragments.get() + state.used, source, amount);
		state.used += amount;
		if (state.used < extent) return {last, parse_code::partial};
		source = state.fragments.get();
	}
	::fast_io::secure::details::aes128gcm_key cipher{value.parameters.key.data()};
	auto valid{cipher.try_open(value.parameters.nonce, source, size, source + size, dest, value.aad)};
	return {first + remaining, valid ? parse_code::ok : parse_code::invalid};
}

/** @brief Distinguishes clean exhaustion from a truncated record, including an incomplete empty-message tag. */
template <::fast_io::secure::details::octet_character Char>
inline constexpr parse_code scan_context_eof_define(io_reserve_type_t<Char, aes128_gcm_decoding>,
	gcm_scan_state &state, aes128_gcm_decoding const &value) noexcept
{
	if (!::fast_io::secure::gcm_record_size_fits(value.destination.size(), value.aad.size())) return parse_code::overflow;
	return state.used ? parse_code::invalid : parse_code::end_of_file;
}

/** @brief Borrows a 32-byte Ed25519 seed and the message. Every seed has a defined RFC 8032 expansion. */
struct ed25519_signing
{
	using manip_tag = ::fast_io::manip_tag_t;
	::std::span<::std::byte const, 32> seed;
	::fast_io::secure::details::bytes_view message;
};

/** @brief Wraps one message for deterministic Ed25519 signing. Keep the seed secret and both borrows stable. */
[[nodiscard]] inline constexpr ed25519_signing ed25519_sign(::std::span<::std::byte const, 32> seed,
	::fast_io::secure::details::bytes_view message) noexcept { return {seed, message}; }

/** @brief Supplies the fixed RFC 8032 signature length without executing a signature. */
template <::fast_io::secure::details::octet_character Char>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<Char, ed25519_signing>) noexcept { return 64; }

/** @brief Expands the seed and signs only when the I/O planner emits the selected field. */
template <::fast_io::secure::details::octet_character Char>
inline Char *print_reserve_define(io_reserve_type_t<Char, ed25519_signing>, Char *out, ed25519_signing value) noexcept
{
	auto signature{::fast_io::secure::ed25519_signing_key::sign_from_seed(value.seed, value.message)};
	::std::memcpy(out, signature.data(), signature.size());
	return out + signature.size();
}

/** @brief Borrows one scalar for output of its X25519 public coordinate. */
struct x25519_public_value
{
	using manip_tag = ::fast_io::manip_tag_t;
	::std::span<::std::byte const, 32> scalar;
};

/** @brief Wraps a private scalar for public-key derivation without performing key agreement. */
[[nodiscard]] inline constexpr x25519_public_value x25519_public(::std::span<::std::byte const, 32> scalar) noexcept
{ return {scalar}; }

/** @brief Reports the fixed RFC 7748 public-key extent. */
template <::fast_io::secure::details::octet_character Char>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<Char, x25519_public_value>) noexcept { return 32; }

/** @brief Emits a public key. The scalar and its clamped intermediate are never printed. */
template <::fast_io::secure::details::octet_character Char>
inline Char *print_reserve_define(io_reserve_type_t<Char, x25519_public_value>, Char *out, x25519_public_value value) noexcept
{
	auto public_key{::fast_io::secure::x25519_private_key::public_from_scalar(value.scalar)};
	::std::memcpy(out, public_key.data(), public_key.size());
	return out + public_key.size();
}
}
