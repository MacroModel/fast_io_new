#include <fast_io.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace compiler_constant_mutex_true_emit_test
{

struct state
{
	::std::array<char, 32u> bytes{};
	::std::size_t size{};
	::std::size_t sequence{};
	::std::size_t lock_order{};
	::std::size_t write_order{};
	::std::size_t unlock_order{};
	::std::size_t write_calls{};
	bool locked{};
	bool write_observed_unlocked{};
};

struct unlocked_sink
{
	using output_char_type = char;
	state *value{};
};

struct locked_sink
{
	using output_char_type = char;
	state *value{};
};

struct lock_proxy
{
	state *value{};

	inline void lock() noexcept
	{
		value->locked = true;
		value->lock_order = ++value->sequence;
	}

	inline void unlock() noexcept
	{
		value->unlock_order = ++value->sequence;
		value->locked = false;
	}
};

inline constexpr lock_proxy output_stream_mutex_ref_define(
	locked_sink output) noexcept
{
	return {output.value};
}

inline constexpr unlocked_sink output_stream_unlocked_ref_define(
	locked_sink output) noexcept
{
	return {output.value};
}

inline void write_all_overflow_define(
	unlocked_sink output, char const *first, char const *last) noexcept
{
	auto &result{*output.value};
	result.write_order = ++result.sequence;
	result.write_observed_unlocked = !result.locked;
	++result.write_calls;
	for (; first != last; ++first)
	{
		result.bytes[result.size++] = *first;
	}
}

} // namespace compiler_constant_mutex_true_emit_test

int main()
{
	using namespace compiler_constant_mutex_true_emit_test;
	state result{};
	locked_sink output{__builtin_addressof(result)};
	auto const &text{::fast_io::mnp::static_arg<"locked-constant">};

	// This is the continuation used after debug/print/panic prove the source is
	// compiler-constant. The write customization records whether emission occurs
	// strictly between lock acquisition and release.
	::fast_io::operations::decay::
		print_compiler_constant_pre_normalization_true_emit_after_lock<false>(
			output, text);

	if (result.locked || result.write_observed_unlocked ||
		result.write_calls != 1u || result.lock_order != 1u ||
		result.write_order != 2u || result.unlock_order != 3u ||
		::std::string_view{result.bytes.data(), result.size} !=
			"locked-constant")
	{
		return 1;
	}
}
