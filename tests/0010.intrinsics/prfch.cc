#include <array>
#include <cassert>
#include <cstddef>

#include <fast_io_core.h>

namespace
{

struct synthetic_x86_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct synthetic_unknown_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::unknown};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::generic};
	inline static constexpr bool data_available{false};
	inline static constexpr bool instruction_available{false};
};

static_assert(::fast_io::prfch_platform<synthetic_x86_platform>);
static_assert(::fast_io::data_prfch_platform<synthetic_x86_platform>);
static_assert(!::fast_io::instruction_prfch_platform<synthetic_x86_platform>);
static_assert(::fast_io::x86_prfch_platform<synthetic_x86_platform>);
static_assert(!::fast_io::arm_prfch_platform<synthetic_x86_platform>);
static_assert(::fast_io::tuned_prfch_platform<synthetic_x86_platform>);

static_assert(::fast_io::prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::data_prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::instruction_prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::tuned_prfch_platform<synthetic_unknown_platform>);

template <::fast_io::prfch_mode mode>
concept callable_prfch_mode = requires(void const *address) {
	::fast_io::prfch<mode>(address);
};

static_assert(callable_prfch_mode<::fast_io::prfch_mode::read>);
static_assert(callable_prfch_mode<::fast_io::prfch_mode::write>);
static_assert(callable_prfch_mode<::fast_io::prfch_mode::instruction>);
static_assert(!callable_prfch_mode<static_cast<::fast_io::prfch_mode>(3)>);

constexpr bool constant_evaluation_is_a_noop()
{
	int value{};
	::fast_io::prfch<::fast_io::prfch_mode::read, ::fast_io::prfch_level::L1>(&value);
	::fast_io::prfch<::fast_io::prfch_mode::write, ::fast_io::prfch_level::nta,
					  ::fast_io::prfch_retention::strm>(&value);
	::fast_io::prfch<::fast_io::prfch_mode::instruction>(&value);
	return value == 0;
}

static_assert(constant_evaluation_is_a_noop());

template <::fast_io::prfch_mode mode, ::fast_io::prfch_retention retention>
void exercise_levels(void const *address)
{
	::fast_io::prfch<mode, ::fast_io::prfch_level::nta, retention>(address);
	::fast_io::prfch<mode, ::fast_io::prfch_level::L3, retention>(address);
	::fast_io::prfch<mode, ::fast_io::prfch_level::L2, retention>(address);
	::fast_io::prfch<mode, ::fast_io::prfch_level::L1, retention>(address);
}

} // namespace

int main()
{
	alignas(64) ::std::array<::std::byte, 64u> storage{};
	void const *const address{storage.data()};
	exercise_levels<::fast_io::prfch_mode::read, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::read, ::fast_io::prfch_retention::strm>(address);
	exercise_levels<::fast_io::prfch_mode::write, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::write, ::fast_io::prfch_retention::strm>(address);
	exercise_levels<::fast_io::prfch_mode::instruction, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::instruction, ::fast_io::prfch_retention::strm>(address);
	assert(storage.front() == ::std::byte{});
}
