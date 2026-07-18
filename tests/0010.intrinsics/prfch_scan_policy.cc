#include <type_traits>

#include <fast_io_core.h>
#include <fast_io_freestanding_impl/io_buffer/mode.h>

namespace scan_prfch_policy_test
{

struct x86_core_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct aarch64_application_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::aarch64};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::arm_application};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{true};
};

struct cacheable_input_owner
{};

inline constexpr ::std::true_type
	prfch_cacheable_read_provenance_define(::fast_io::io_type_t<cacheable_input_owner>) noexcept
{
	return {};
}

struct cacheable_refill_owner
{};

inline constexpr ::std::true_type
	prfch_cacheable_write_provenance_define(::fast_io::io_type_t<cacheable_refill_owner>) noexcept
{
	return {};
}

/// A semantic scan target describes where parsed state is stored, not where input bytes originate.
struct destination_only_scan_manipulator
{};

inline constexpr ::std::true_type
	prfch_cacheable_write_provenance_define(::fast_io::io_type_t<destination_only_scan_manipulator>) noexcept
{
	return {};
}

} // namespace scan_prfch_policy_test

static_assert(::fast_io::prfch_platform<scan_prfch_policy_test::x86_core_platform>);
static_assert(::fast_io::prfch_platform<scan_prfch_policy_test::aarch64_application_platform>);

// Both platforms pass the broad experimental envelope. The scan site decisions nevertheless remain false: platform
// instruction availability is not evidence that a sequential scanner or a repeated owned-buffer refill benefits.
static_assert(::fast_io::conservative_read_prfch_platform<
			  scan_prfch_policy_test::x86_core_platform>);
static_assert(::fast_io::conservative_write_prfch_platform<
			  scan_prfch_policy_test::x86_core_platform>);
static_assert(::fast_io::conservative_read_prfch_platform<
			  scan_prfch_policy_test::aarch64_application_platform>);
static_assert(::fast_io::conservative_write_prfch_platform<
			  scan_prfch_policy_test::aarch64_application_platform>);

static_assert(!::fast_io::scan_contiguous_consume_read_prfch_platform<
			  scan_prfch_policy_test::x86_core_platform>);
static_assert(!::fast_io::scan_contiguous_consume_read_prfch_platform<
			  scan_prfch_policy_test::aarch64_application_platform>);
static_assert(!::fast_io::scan_owned_refill_write_prfch_platform<
			  scan_prfch_policy_test::x86_core_platform>);
static_assert(!::fast_io::scan_owned_refill_write_prfch_platform<
			  scan_prfch_policy_test::aarch64_application_platform>);

static_assert(::fast_io::prfch_cacheable_read_provenance<
			  scan_prfch_policy_test::cacheable_input_owner>);
static_assert(::fast_io::prfch_cacheable_write_provenance<
			  scan_prfch_policy_test::cacheable_refill_owner>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<
			  scan_prfch_policy_test::destination_only_scan_manipulator>);

// Exact provenance cannot bypass a negative site decision, and cursor-shaped raw objects do not acquire provenance.
static_assert(!::fast_io::scan_contiguous_consume_read_prfch_strategy<
			  scan_prfch_policy_test::x86_core_platform,
			  scan_prfch_policy_test::cacheable_input_owner>);
static_assert(!::fast_io::scan_owned_refill_write_prfch_strategy<
			  scan_prfch_policy_test::x86_core_platform,
			  scan_prfch_policy_test::cacheable_refill_owner>);
static_assert(!::fast_io::scan_contiguous_consume_read_prfch_strategy<
			  scan_prfch_policy_test::x86_core_platform, char *>);
static_assert(!::fast_io::scan_owned_refill_write_prfch_strategy<
			  scan_prfch_policy_test::x86_core_platform,
			  ::fast_io::basic_io_buffer_pointers<char>>);

int main()
{}
