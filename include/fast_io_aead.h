#pragma once
/**
 * @file
 * @brief Streaming authenticated encryption for files, messages and byte transports.
 * @details Implements RFC 8188 using fast_io's AES and SHA-256 and the operating system's
 * cryptographic random source. Each message has a fresh salt. The caller supplies high-entropy
 * shared key material and its expected public identifier.
 */
#include "fast_io_hosted.h"
#include "fast_io_secure/error.h"
#include "fast_io_secure/rfc8188_crypto.h"
#include <memory>
#include <optional>
#include <array>
#include <cstring>
#include <span>

namespace fast_io::secure
{

namespace details
{
/** @brief Holds one stream's secrets in uniquely owned storage, including provisional plaintext. */
template <::std::size_t capacity>
struct aes128gcm_records
{
	static_assert(18 <= capacity && capacity <= 1048576, "record capacity must lie between 18 octets and 1 MiB");
	::std::optional<aes128gcm_key> cipher;
	::std::array<unsigned char, capacity> record;
	::std::array<unsigned char, 64> ikm{};
	unsigned char nonce_base[12]{}, header[276]{}, expected_id[255]{};
	::std::size_t key_size{}, id_size{}, header_used{}, header_size{21}, record_size{}, used{};
	::std::uint_least64_t sequence{}, blocks{};
	bool initialized{}, final_record{}, finished{}, failed{};

	/** @brief Copies key material so the caller can immediately release its input range. */
	aes128gcm_records(::std::span<::std::byte const> master, ::std::span<::std::byte const> id)
		: key_size(master.size()), id_size(id.size())
	{
		check(16 <= key_size && key_size <= ikm.size() && id_size <= sizeof(expected_id), errc::configuration);
		::std::memcpy(ikm.data(), master.data(), key_size);
		if (id_size)
		{
			::std::memcpy(expected_id, id.data(), id_size);
		}
	}
	aes128gcm_records(aes128gcm_records const &) = delete;
	aes128gcm_records &operator=(aes128gcm_records const &) = delete;

	/** @brief Erases retained master keys, traffic keys and private record staging even on abandonment. */
	~aes128gcm_records() { clear(); }

	void clear() noexcept
	{
		cipher.reset();
		::fast_io::secure_clear(ikm.data(), ikm.size());
		::fast_io::secure_clear(nonce_base, sizeof(nonce_base));
		::fast_io::secure_clear(record.data(), record.size());
	}

	/** @brief Derives a purpose-specific value including the terminating zero in the RFC's HKDF info. */
	template <::std::size_t N>
	static void expand(::std::byte const (&prk)[32], char8_t const (&label)[N], ::std::byte (&value)[32]) noexcept
	{
		::std::byte info[N+1];
		::std::memcpy(info,label,N);
		info[N] = ::std::byte{1};
		record_hmac(prk,32,info,N+1,value);
	}

	/** @brief Extracts once and expands separate RFC encryption-key and base-nonce labels. */
	void initialize() noexcept
	{
		::std::byte prk[32], value[32];
		record_hmac(reinterpret_cast<::std::byte const *>(header),16,
			reinterpret_cast<::std::byte const *>(ikm.data()),key_size,prk);
		::fast_io::secure_clear(ikm.data(),ikm.size());
		expand(prk,u8"Content-Encoding: aes128gcm",value);
		cipher.emplace(value);
		expand(prk,u8"Content-Encoding: nonce",value);
		::std::memcpy(nonce_base,value,sizeof(nonce_base));
		::fast_io::secure_clear_array(prk);
		::fast_io::secure_clear_array(value);
		initialized = true;
	}

	/** @brief Validates public framing and the externally selected key identifier before key derivation. */
	void accept_header()
	{
		::std::uint_least32_t size{};
		for (unsigned i{16}; i != 20; ++i)
		{
			size = (size << 8) | header[i];
		}
		check(size >= 18 && size <= capacity && header[20] == id_size &&
				  (!id_size || ::std::memcmp(header + 21, expected_id, id_size) == 0),
			  errc::invalid_message);
		record_size = size;
		initialize();
	}

	/** @brief Reserves a unique sequential nonce and charges a conservative per-stream GCM block budget. */
	void next_record(unsigned char (&nonce)[12], ::std::size_t plaintext_size)
	{
		auto cost = 1u + (plaintext_size + 15u) / 16u;
		constexpr ::std::uint_least64_t limit{1ULL << 40};
		check(cost <= limit - blocks, errc::usage_limit);
		blocks += cost;
		::std::memcpy(nonce, nonce_base, sizeof(nonce));
		auto seq = sequence++;
		for (unsigned i{}; i != 8; ++i, seq >>= 8)
		{
			nonce[11u - i] ^= static_cast<unsigned char>(seq);
		}
	}

	/** @brief Encrypts one framed plaintext record and appends its full 128-bit tag. */
	::std::size_t seal(::std::byte *destination, bool terminal)
	{
		record[used++] = terminal ? 2u : 1u;
		if (!terminal && used < record_size - 16)
		{
			// An explicit flush pads the nonfinal record to exactly rs octets on the wire.
			::std::memset(record.data() + used, 0, record_size - 16 - used);
			used = record_size - 16;
		}
		unsigned char nonce[12];
		next_record(nonce, used);
		auto *out = reinterpret_cast<unsigned char *>(destination);
		cipher->seal(nonce,record.data(),used,out,out+used);
		auto length{used+16};
		::fast_io::secure_clear(record.data(),used);
		used = 0;
		return length;
	}

	/** @brief Authenticates in private storage before parsing the delimiter or exposing any plaintext.
	 * @details RFC 8188 uses empty additional data. Nonces bind record position.
	 * The complete tag is checked before decrypting into private storage.
	 */
	void open()
	{
		check(used >= 17, errc::invalid_message);
		auto const length = used - 16;
		unsigned char nonce[12];
		next_record(nonce, length);
		check(cipher->try_open(nonce,record.data(),length,record.data()+length,record.data()),errc::authentication);
		auto end = length;
		while (end && !record[end - 1])
		{
			--end;
		}
		check(end && (record[end - 1] == 1 || record[end - 1] == 2), errc::invalid_message);
		final_record = record[end - 1] == 2;
		// Only a terminal record may occupy less than the advertised wire record size.
		check(final_record || used == record_size, errc::invalid_message);
		used = end - 1;
	}

	/** @brief Copies only committed, authenticated plaintext, then wipes its private staging area. */
	::std::byte *publish(::std::byte *out)
	{
		::std::memcpy(out, record.data(), used);
		out += used;
		::fast_io::secure_clear(record.data(), used);
		used = 0;
		return out;
	}
};
} // namespace details

/**
 * @brief Move-only RFC 8188 engine with a compile-time cap on attacker-selected record size.
 * @details Encryption generates a fresh 128-bit salt and binds a monotonic
 * sequence to each nonce. Copying an active writer would fork its nonce
 * sequence and is prohibited. A reader may release verified prefix records,
 * and releases the final record only after checking physical end of input.
 */
template <bool encrypt, ::std::size_t capacity = 16384>
class basic_aes128gcm_transcoder
{
	/** @brief Permits the serial async driver to suspend between bounded steps while this engine retains its state. */
	template <bool Input>
	friend constexpr ::std::true_type async_transcode_step_define(::fast_io::transcode_reserve_t<basic_aes128gcm_transcoder>, ::std::bool_constant<Input>) noexcept
	{
		return {};
	}

	using state = details::aes128gcm_records<capacity>;
	::std::unique_ptr<state> storage;

	/** @brief Acquires state for one engine entry and poisons it if any subsequent check throws. */
	state &enter()
	{
		details::check(storage && !storage->failed && !storage->finished, errc::invalid_message);
		storage->failed = true;
		return *storage;
	}

	struct clear_on_failure
	{
		state &s;
		~clear_on_failure() { if (s.failed) { s.clear(); } }
	};

	/** @brief Emits the public header exactly once, including for an empty message. */
	static ::std::byte *emit_header(state &s, ::std::byte *out)
	{
		if (!s.header_used)
		{
			::std::memcpy(out, s.header, s.header_size);
			out += s.header_size;
			s.header_used = s.header_size;
		}
		return out;
	}

public:
	using from_value_type = ::std::byte;
	using to_value_type = ::std::byte;

	/** @brief Creates a writer using a private fresh salt and a caller-selected bounded record size. */
	explicit basic_aes128gcm_transcoder(::std::span<::std::byte const> master, ::std::size_t rs,
										::std::span<::std::byte const> key_id = {})
		requires encrypt
		: storage(::std::make_unique<state>(master, key_id))
	{
		auto &s = *storage;
		details::check(rs >= 18 && rs <= capacity, errc::configuration);
		s.record_size = rs;
		::fast_io::native_white_hole random;
		auto *salt{reinterpret_cast<::std::byte *>(s.header)};
		::fast_io::operations::read_all_bytes(random,salt,salt+16);
		for (unsigned i{}; i != 4; ++i)
		{
			s.header[19u - i] = static_cast<unsigned char>(static_cast<::std::uint_least32_t>(rs) >> (8u * i));
		}
		s.header[20] = static_cast<unsigned char>(s.id_size);
		if (s.id_size)
		{
			::std::memcpy(s.header + 21, s.expected_id, s.id_size);
		}
		s.header_size += s.id_size;
		s.initialize();
	}

	/** @brief Creates a reader for one externally selected key and its expected public identifier. */
	explicit basic_aes128gcm_transcoder(::std::span<::std::byte const> master,
										::std::span<::std::byte const> key_id = {})
		requires(!encrypt)
		: storage(::std::make_unique<state>(master, key_id))
	{}

	/** @brief Requests erasure of adapter-owned staging memory on release. */
	friend constexpr ::std::true_type transcode_sensitive_define(io_type_t<basic_aes128gcm_transcoder>) noexcept
	{
		return {};
	}

	/** @brief Declares the reader's verified-record publication obligation to generic input adapters. */
	friend constexpr ::std::true_type transcode_authenticated_records_define(io_type_t<basic_aes128gcm_transcoder>) noexcept
		requires(!encrypt)
	{
		return {};
	}

	/** @brief Reserves room for one record and, on encryption, the largest possible header. */
	friend constexpr ::std::size_t transcode_min_output_size_define(transcode_reserve_t<basic_aes128gcm_transcoder>,
																	transcode_phase) noexcept
	{
		return capacity + (encrypt ? 276u : 0u);
	}

	/** @brief Transforms at most one record per invocation and preserves unconsumed input for the next call. */
	friend basic_transcode_process_result<::std::byte, ::std::byte> transcode_process_define(
		basic_aes128gcm_transcoder &e, ::std::byte const *p, ::std::byte const *end, ::std::byte *out, ::std::byte *limit)
	{
		auto &s = e.enter();
		clear_on_failure guard{s};
		details::check(static_cast<::std::size_t>(limit - out) >= transcode_min_output_size_define(
																	  transcode_reserve<basic_aes128gcm_transcoder>, transcode_phase::process));
		if constexpr (encrypt)
		{
			out = emit_header(s, out);
			auto room = s.record_size - 17 - s.used;
			auto n = static_cast<::std::size_t>(end - p);
			if (n > room)
			{
				n = room;
			}
			if (n)
			{
				::std::memcpy(s.record.data() + s.used, p, n);
			}
			s.used += n;
			p += n;
			if (p != end)
			{
				out += s.seal(out, false);
			}
		}
		else
		{
			while (!s.initialized && p != end)
			{
				auto n = static_cast<::std::size_t>(end - p);
				auto room = s.header_size - s.header_used;
				if (n > room)
				{
					n = room;
				}
				::std::memcpy(s.header + s.header_used, p, n);
				p += n;
				s.header_used += n;
				if (s.header_used >= 21)
				{
					s.header_size = 21u + s.header[20];
				}
				if (s.header_used == s.header_size)
				{
					s.accept_header();
				}
			}
			if (p != end)
			{
				details::check(!s.final_record, errc::invalid_message);
				auto n = static_cast<::std::size_t>(end - p);
				auto room = s.record_size - s.used;
				if (n > room)
				{
					n = room;
				}
				::std::memcpy(s.record.data() + s.used, p, n);
				p += n;
				s.used += n;
				if (s.used == s.record_size)
				{
					s.open();
					if (!s.final_record)
					{
						out = s.publish(out);
					}
					else
					{
						details::check(p == end, errc::invalid_message);
					}
				}
			}
		}
		s.failed = false;
		return {p, out, p == end ? transcode_step_status::need_input : transcode_step_status::need_output};
	}

	/** @brief Flushes buffered plaintext as a padded nonfinal record for live streaming. */
	friend basic_transcode_drain_result<::std::byte> transcode_sync_flush_define(basic_aes128gcm_transcoder &e,
																				 ::std::byte *out, ::std::byte *limit)
		requires encrypt
	{
		auto &s = e.enter();
		clear_on_failure guard{s};
		details::check(static_cast<::std::size_t>(limit - out) >= capacity + 276u);
		out = emit_header(s, out);
		if (s.used)
		{
			out += s.seal(out, false);
		}
		s.failed = false;
		return {out, transcode_drain_status::complete};
	}

	/** @brief Emits or verifies the mandatory final record, including for zero-length plaintext. */
	friend basic_transcode_drain_result<::std::byte> transcode_finish_define(basic_aes128gcm_transcoder &e,
																			 ::std::byte *out, ::std::byte *limit)
	{
		auto &s = e.enter();
		clear_on_failure guard{s};
		details::check(static_cast<::std::size_t>(limit - out) >= transcode_min_output_size_define(
																	  transcode_reserve<basic_aes128gcm_transcoder>, transcode_phase::finish));
		if constexpr (encrypt)
		{
			out = emit_header(s, out);
			out += s.seal(out, true);
		}
		else
		{
			details::check(s.initialized, errc::invalid_message);
			if (!s.final_record)
			{
				s.open();
			}
			details::check(s.final_record, errc::invalid_message);
			out = s.publish(out);
		}
		s.clear();
		s.finished = true;
		s.failed = false;
		return {out, transcode_drain_status::complete};
	}
};

/** @brief Wraps a file, socket or nested output adapter in a fresh authenticated message.
 * @details Call finish() and check its result before treating the ciphertext as complete.
 */
template <::std::size_t capacity = 16384, typename handle>
inline auto make_aead_output(handle &&out, ::std::span<::std::byte const> key,
							 ::std::size_t rs = (capacity < 4096u ? capacity : 4096u), ::std::span<::std::byte const> key_id = {})
{
	return ::fast_io::make_otranscoder(::std::forward<handle>(out), basic_aes128gcm_transcoder<true, capacity>{key, rs, key_id});
}

/** @brief Wraps one bounded ciphertext message, publishing only authenticated records.
 * @details Read to EOF or call the input adapter's checked terminal operation to
 * validate the complete message. A socket transport must provide message EOF.
 */
template <::std::size_t capacity = 16384, typename handle>
inline auto make_aead_input(handle &&in, ::std::span<::std::byte const> key, ::std::span<::std::byte const> key_id = {})
{
	return ::fast_io::make_itranscoder(::std::forward<handle>(in), basic_aes128gcm_transcoder<false, capacity>{key, key_id});
}

/** @brief Encrypts an RFC 8188 aes128gcm output message with a fresh salt and authenticated final record.
 * @details The authenticated record provider owns key derivation, nonces and tags. Checked finish
 * emits the final record for an owned decorator. A borrowed message is finished by its owner.
 */
template <::std::size_t Capacity = 16384, typename Handle>
inline auto aes128gcm_encrypt(Handle &&out, ::std::span<::std::byte const> key,
	::std::size_t record_size = (Capacity < 4096u ? Capacity : 4096u), ::std::span<::std::byte const> key_id = {})
{
	return make_aead_output<Capacity>(::std::forward<Handle>(out), key, record_size, key_id);
}

/** @brief Decrypts an RFC 8188 aes128gcm input message and exposes only authenticated record prefixes.
 * @details Reading to logical EOF verifies the authenticated final record. A tag failure or
 * truncation is an error. Asynchronous scheduling preserves the provider's publication boundary.
 */
template <::std::size_t Capacity = 16384, typename Handle>
inline auto aes128gcm_decrypt(Handle &&in, ::std::span<::std::byte const> key, ::std::span<::std::byte const> key_id = {})
{
	return make_aead_input<Capacity>(::std::forward<Handle>(in), key, key_id);
}

} // namespace fast_io::secure
