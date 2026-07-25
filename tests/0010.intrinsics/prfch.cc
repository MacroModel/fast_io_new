#include <array>
#include <cassert>
#include <cstddef>

#include <fast_io_core.h>

void default_visible_instruction_target() noexcept
{}

namespace
{

struct synthetic_x86_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct synthetic_aarch64_direct_instruction_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::aarch64};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::arm_server};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{true};
	inline static constexpr bool direct_instruction_available{true};
};

struct synthetic_x86_local_instruction_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
	inline static constexpr bool local_instruction_available{true};
};

struct inconsistent_direct_instruction_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
	[[maybe_unused]] inline static constexpr bool direct_instruction_available{true};
};

struct nonconstant_local_instruction_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
	[[maybe_unused]] inline static bool local_instruction_available{true};
};

struct nonconstant_platform
{
	[[maybe_unused]] inline static ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	[[maybe_unused]] inline static ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	[[maybe_unused]] inline static bool data_available{true};
	[[maybe_unused]] inline static bool instruction_available{false};
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
static_assert(::fast_io::direct_instruction_prfch_platform<synthetic_aarch64_direct_instruction_platform>);
static_assert(::fast_io::instruction_prfch_platform<synthetic_aarch64_direct_instruction_platform>);
static_assert(!::fast_io::direct_instruction_prfch_platform<inconsistent_direct_instruction_platform>);
static_assert(::fast_io::local_instruction_prfch_platform<synthetic_x86_local_instruction_platform>);
static_assert(!::fast_io::direct_instruction_prfch_platform<synthetic_x86_local_instruction_platform>);
static_assert(!::fast_io::local_instruction_prfch_platform<nonconstant_local_instruction_platform>);
static_assert(!::fast_io::prfch_platform<nonconstant_platform>);
static_assert(!::fast_io::data_prfch_platform<nonconstant_platform>);

static_assert(::fast_io::prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::data_prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::instruction_prfch_platform<synthetic_unknown_platform>);
static_assert(!::fast_io::tuned_prfch_platform<synthetic_unknown_platform>);

#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(__arm64))
static_assert(::fast_io::details::native_prfch_isa == ::fast_io::prfch_isa::aarch64);
static_assert(::fast_io::details::native_prfch_tune == ::fast_io::prfch_tune::arm_apple);
static_assert(::fast_io::conservative_read_prfch_platform<
			  ::fast_io::details::native_prfch_platform>);
static_assert(::fast_io::conservative_write_prfch_platform<
			  ::fast_io::details::native_prfch_platform>);
#endif

#if defined(__has_builtin)
#if defined(__x86_64__) && defined(__PREFETCHI__) && __has_builtin(__builtin_ia32_prefetchi)
#if defined(__clang__) && !defined(__INTEL_LLVM_COMPILER)
static_assert(::fast_io::details::native_instruction_prfch_available);
static_assert(::fast_io::details::native_direct_instruction_prfch_available);
static_assert(::fast_io::details::native_local_instruction_prfch_available);
#elif defined(__GNUC__) && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER)
static_assert(!::fast_io::details::native_instruction_prfch_available);
static_assert(!::fast_io::details::native_direct_instruction_prfch_available);
static_assert(::fast_io::details::native_local_instruction_prfch_available);
#endif
#endif
#endif

template <::fast_io::prfch_mode mode>
concept callable_prfch_mode = requires(void const *address) {
	::fast_io::prfch<mode>(address);
};

template <::fast_io::prfch_level level>
concept callable_prfch_level = requires(void const *address) {
	::fast_io::prfch<::fast_io::prfch_mode::read, level>(address);
};

template <::fast_io::prfch_retention retention>
concept callable_prfch_retention = requires(void const *address) {
	::fast_io::prfch<::fast_io::prfch_mode::read, ::fast_io::prfch_level::L1, retention>(address);
};

static_assert(callable_prfch_mode<::fast_io::prfch_mode::read>);
static_assert(callable_prfch_mode<::fast_io::prfch_mode::write>);
static_assert(callable_prfch_mode<::fast_io::prfch_mode::instruction>);
static_assert(!callable_prfch_mode<static_cast<::fast_io::prfch_mode>(3)>);
static_assert(callable_prfch_level<::fast_io::prfch_level::nta>);
static_assert(callable_prfch_level<::fast_io::prfch_level::L1>);
static_assert(!callable_prfch_level<static_cast<::fast_io::prfch_level>(4)>);
static_assert(callable_prfch_retention<::fast_io::prfch_retention::keep>);
static_assert(callable_prfch_retention<::fast_io::prfch_retention::strm>);
static_assert(!callable_prfch_retention<static_cast<::fast_io::prfch_retention>(2)>);

static_assert(::fast_io::details::aarch64_prfch_operation<
				  ::fast_io::prfch_mode::read, ::fast_io::prfch_level::L1,
				  ::fast_io::prfch_retention::keep> == 0u);
static_assert(::fast_io::details::aarch64_prfch_operation<
				  ::fast_io::prfch_mode::read, ::fast_io::prfch_level::nta,
				  ::fast_io::prfch_retention::keep> == 1u);
static_assert(::fast_io::details::aarch64_prfch_operation<
				  ::fast_io::prfch_mode::instruction, ::fast_io::prfch_level::L2,
				  ::fast_io::prfch_retention::strm> == 11u);
static_assert(::fast_io::details::aarch64_prfch_operation<
				  ::fast_io::prfch_mode::write, ::fast_io::prfch_level::L3,
				  ::fast_io::prfch_retention::keep> == 20u);

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

void direct_instruction_target() noexcept
{}

static_assert(requires {
	::fast_io::prfch_instruction_direct<&direct_instruction_target>();
});
static_assert(requires {
	::fast_io::prfch_instruction_direct<&default_visible_instruction_target>();
});

using local_instruction_target_binding =
	::fast_io::prfch_local_function_t<&direct_instruction_target>;

inline int nonfunction_instruction_target{};

template <auto address>
concept formable_local_instruction_binding = requires {
	typename ::fast_io::prfch_local_function_t<address>;
};

template <typename binding_type>
concept callable_local_instruction = requires {
	::fast_io::prfch_instruction_local<binding_type>();
};

static_assert(::fast_io::local_prfch_function_binding<local_instruction_target_binding>);
static_assert(!::fast_io::local_prfch_function_binding<decltype(&direct_instruction_target)>);
static_assert(formable_local_instruction_binding<&direct_instruction_target>);
static_assert(!formable_local_instruction_binding<nullptr>);
static_assert(!formable_local_instruction_binding<&nonfunction_instruction_target>);
static_assert(callable_local_instruction<local_instruction_target_binding>);
static_assert(!callable_local_instruction<decltype(&direct_instruction_target)>);

constexpr bool direct_instruction_constant_evaluation_is_a_noop()
{
	::fast_io::prfch_instruction_direct<&direct_instruction_target>();
	::fast_io::prfch_instruction_local<local_instruction_target_binding>();
	return true;
}

static_assert(direct_instruction_constant_evaluation_is_a_noop());

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
	alignas(64)::std::array<::std::byte, 64u> storage{};
	void const *const address{storage.data()};
	exercise_levels<::fast_io::prfch_mode::read, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::read, ::fast_io::prfch_retention::strm>(address);
	exercise_levels<::fast_io::prfch_mode::write, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::write, ::fast_io::prfch_retention::strm>(address);
	exercise_levels<::fast_io::prfch_mode::instruction, ::fast_io::prfch_retention::keep>(address);
	exercise_levels<::fast_io::prfch_mode::instruction, ::fast_io::prfch_retention::strm>(address);
	::fast_io::prfch_instruction_direct<&direct_instruction_target>();
	::fast_io::prfch_instruction_direct<&default_visible_instruction_target>();
	::fast_io::prfch_instruction_local<local_instruction_target_binding>();
	assert(storage.front() == ::std::byte{});
}
