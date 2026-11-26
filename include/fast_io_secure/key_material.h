#pragma once
#include "../fast_io_crypto.h"
#include <array>
#include <span>

namespace fast_io::secure::details
{
using bytes_view = ::std::span<::std::byte const>;

template <::std::size_t N>
struct private_bytes
{
	::std::array<::std::byte,N> value{};
	private_bytes() = default;
	private_bytes(private_bytes const &) = delete;
	private_bytes &operator=(private_bytes const &) = delete;
	private_bytes(private_bytes &&other) noexcept : value(other.value) { other.clear(); }
	void clear() noexcept { ::fast_io::secure_clear(value.data(),N); }
	~private_bytes() { clear(); }
};

inline void hash_part(auto &hash, bytes_view part) noexcept
{
	if (!part.empty()) hash.update(part.data(),part.data()+part.size());
}

template <::std::size_t N>
inline bytes_view label_bytes(char8_t const (&label)[N]) noexcept
{
	return {reinterpret_cast<::std::byte const *>(label),N-1};
}

template <typename... Parts>
inline void sha256_hmac(bytes_view key, ::std::byte *result, Parts const &...parts) noexcept
{
	::fast_io::sha256_context hash;
	::fast_io::secure_clear_guard erase_hash{&hash,1};
	private_bytes<64> pad;
	if (key.size() > 64)
	{
		hash_part(hash,key);
		hash.do_final();
		hash.digest_to_byte_ptr(pad.value.data());
		hash.reset();
	}
	else if (!key.empty()) ::fast_io::freestanding::my_memcpy(pad.value.data(),key.data(),key.size());
	for (auto &b : pad.value) b ^= ::std::byte{0x36};
	hash_part(hash,pad.value);
	(hash_part(hash,parts),...);
	hash.do_final();
	hash.digest_to_byte_ptr(result);
	hash.reset();
	for (auto &b : pad.value) b ^= ::std::byte{0x36 ^ 0x5c};
	hash_part(hash,pad.value);
	hash_part(hash,{result,32});
	hash.do_final();
	hash.digest_to_byte_ptr(result);
}
} // namespace fast_io::secure::details
