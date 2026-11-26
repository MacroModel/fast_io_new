#pragma once

namespace fast_io::transcoders
{
/**
 * @brief Applies AES in big-endian counter mode to a bounded byte stream.
 * @details The caller supplies an AES key and a unique initial 128-bit counter for the message.
 * Encryption and decryption use the same transformation. Each partial block retains its unused
 * keystream across calls. The caller must prevent counter reuse across messages. A request
 * exceeding the remaining counter space fails before publishing output. CTR requires separate integrity protection.
 * Counter generation may branch on the public counter and message length. The AES key and
 * the payload do not determine the addresses or the control flow of the cipher rounds.
 * Partial overlap is outside the input contract. Exact in-place transformation is supported.
 */
template <::std::size_t KeyBytes>
class aes_ctr
{
	/** @brief Permits the serial async driver to suspend between bounded steps while this engine retains its state. */
	template <bool Input>
	friend constexpr ::std::true_type async_transcode_step_define(::fast_io::transcode_reserve_t<aes_ctr>, ::std::bool_constant<Input>) noexcept
	{
		return {};
	}

	::fast_io::aes_ctx<KeyBytes> cipher;
	::std::byte counter[16]{}, tail[16]{};
	::std::size_t offset{16};
	bool exhausted{};

	constexpr void clear_stream() noexcept
	{
		::fast_io::secure_clear_array(counter);
		::fast_io::secure_clear_array(tail);
		offset = 16;
		exhausted = true;
	}

	/** @brief Includes buffered tail bytes and permits use of the final all-ones counter exactly once. */
	constexpr void check_capacity(::std::size_t size) const
	{
		auto left{16u - offset};
		if (size <= left) { return; }
		if (!exhausted)
		{
			if constexpr (sizeof(::std::size_t) < 16)
			{
				if (counter[0] != ::std::byte{255}) { return; }
			}
			// The highest counter used is current + ceil((size - left) / 16) - 1.
			auto add{(size - left - 1u) / 16u};
			for (unsigned i{16}; i && add; --i)
			{
				auto sum{::std::to_integer<unsigned>(counter[i - 1u]) + (add & 255u)};
				add = (add >> 8u) + (sum >> 8u);
			}
			if (!add) { return; }
		}
		::fast_io::throw_parse_code(::fast_io::parse_code::overflow);
	}

	/** @brief Advances the complete 128-bit counter, including carries across byte boundaries. */
	constexpr void advance() noexcept
	{
		for (::std::size_t i{16}; i != 0; --i)
		{
			auto &digit{counter[i - 1]};
			digit = static_cast<::std::byte>((::std::to_integer<unsigned>(digit) + 1u) & 255u);
			if (digit != ::std::byte{})
			{
				return;
			}
		}
		exhausted = true;
	}

	/** @brief Constructs a run of counters, paying for multi-byte carry only at the boundary of a low byte. */
	template <::std::size_t Blocks>
	constexpr void bulk(::std::byte const *src, ::std::byte *dest) noexcept
	{
		::std::byte stream[16 * Blocks];
		auto const low{::std::to_integer<unsigned>(counter[15])};
		if (low < 256u - Blocks)
		{
			for (::std::size_t n{}; n != Blocks; ++n)
			{
				for (::std::size_t i{}; i != 15; ++i) { stream[16 * n + i] = counter[i]; }
				stream[16 * n + 15] = static_cast<::std::byte>(low + n);
			}
			counter[15] = static_cast<::std::byte>(low + Blocks);
		}
		else
		{
			for (::std::size_t n{}; n != Blocks; ++n)
			{
				for (::std::size_t i{}; i != 16; ++i) { stream[16 * n + i] = counter[i]; }
				advance();
			}
		}
		cipher.encrypt(stream, Blocks, stream);
		for (::std::size_t i{}; i != 16 * Blocks; ++i) { dest[i] = src[i] ^ stream[i]; }
		::fast_io::secure_clear_array(stream);
	}

public:
	using from_value_type = ::std::byte;
	using to_value_type = ::std::byte;

	/** @brief Copies the initial counter and expands a key of exactly KeyBytes bytes. */
	constexpr aes_ctr(::std::byte const (&key)[KeyBytes], ::std::byte const (&initial)[16]) noexcept : cipher(key)
	{
		for (::std::size_t i{}; i != 16; ++i)
		{
			counter[i] = initial[i];
		}
	}

	aes_ctr(aes_ctr const &) = delete;
	aes_ctr &operator=(aes_ctr const &) = delete;

	constexpr aes_ctr(aes_ctr &&other) noexcept
		: cipher(::std::move(other.cipher)), offset(other.offset), exhausted(other.exhausted)
	{
		for (unsigned i{}; i != 16; ++i) { counter[i] = other.counter[i]; tail[i] = other.tail[i]; }
		other.clear_stream();
	}

	constexpr aes_ctr &operator=(aes_ctr &&other) noexcept
	{
		if (this != __builtin_addressof(other))
		{
			::std::destroy_at(this);
			::std::construct_at(this, ::std::move(other));
		}
		return *this;
	}

	constexpr ~aes_ctr() { clear_stream(); }

	friend constexpr ::std::true_type transcode_sensitive_define(io_type_t<aes_ctr>) noexcept { return {}; }

	friend constexpr transcode_buffer_sizes transcode_preferred_buffer_sizes_define(transcode_reserve_t<aes_ctr>) noexcept
	{
		return {256u, 256u};
	}

	/**
	 * @brief XORs source bytes with successive counter blocks, accepting identical input and output.
	 * @details Independent counters are encrypted together to expose independent AES round chains. A short
	 * final block and a subsequent call consume exactly the same keystream as a single contiguous call.
	 */
	constexpr void transform(::std::byte const *src, ::std::size_t size, ::std::byte *dest)
	{
		check_capacity(size);
		while (offset != 16 && size)
		{
			*dest++ = *src++ ^ tail[offset++];
			--size;
		}
		if (size >= 256)
		{
			auto copied{cipher.transform_ctr_bulk(counter, src, dest, size)};
			src += copied; dest += copied; size -= copied;
		}
		while (size >= 256)
		{
			bulk<16>(src, dest);
			src += 256; dest += 256; size -= 256;
		}
		while (size >= 64)
		{
			bulk<4>(src, dest);
			src += 64; dest += 64; size -= 64;
		}
		while (size)
		{
			// Each residual block starts at the current counter, including an exact 16-byte tail.
			cipher.encrypt(counter, 1, tail);
			advance();
			auto const n{size < 16 ? size : 16};
			for (::std::size_t i{}; i != n; ++i)
			{
				dest[i] = src[i] ^ tail[i];
			}
			offset = n;
			src += n;
			dest += n;
			size -= n;
		}
	}
};

/** @brief Advertises progress with a single writable byte and no terminal expansion. */
template <::std::size_t N>
inline constexpr ::std::size_t transcode_min_output_size_define(::fast_io::transcode_reserve_t<aes_ctr<N>>,
															   ::fast_io::transcode_phase) noexcept
{
	return 1;
}

/** @brief Processes the common prefix of two bounded byte ranges and preserves remaining counter state. */
template <::std::size_t N>
inline constexpr ::fast_io::basic_transcode_process_result<::std::byte, ::std::byte>
transcode_process_define(aes_ctr<N> &ctx, ::std::byte const *first, ::std::byte const *last,
						 ::std::byte *out, ::std::byte *end)
{
	auto const available{end - out}, requested{last - first};
	auto const n{available < requested ? available : requested};
	ctx.transform(first, static_cast<::std::size_t>(n), out);
	return {first + n, out + n, n == requested ? ::fast_io::transcode_step_status::need_input
											   : ::fast_io::transcode_step_status::need_output};
}

/** @brief Completes a CTR message immediately. Counter mode emits no padding or trailer. */
template <::std::size_t N>
inline constexpr ::fast_io::basic_transcode_drain_result<::std::byte>
transcode_finish_define(aes_ctr<N> &, ::std::byte *out, ::std::byte *) noexcept
{
	return {out, ::fast_io::transcode_drain_status::complete};
}

} // namespace fast_io::transcoders
