#include <type_traits>

#include <fast_io_core.h>

namespace policy_test
{

struct unmarked_stream
{};

struct unmarked_scatter_source
{};

struct cacheable_read_source
{};

inline constexpr ::std::true_type
	prfch_cacheable_read_provenance_define(::fast_io::io_type_t<cacheable_read_source>) noexcept
{
	return {};
}

struct cacheable_write_destination
{};

inline constexpr ::std::true_type
	prfch_cacheable_write_provenance_define(::fast_io::io_type_t<cacheable_write_destination>) noexcept
{
	return {};
}

struct cacheable_owned_buffer
{};

inline constexpr ::std::true_type
	prfch_cacheable_read_provenance_define(::fast_io::io_type_t<cacheable_owned_buffer>) noexcept
{
	return {};
}

inline constexpr ::std::true_type
	prfch_cacheable_write_provenance_define(::fast_io::io_type_t<cacheable_owned_buffer>) noexcept
{
	return {};
}

struct bool_is_not_a_proof
{};

inline constexpr bool
	prfch_cacheable_read_provenance_define(::fast_io::io_type_t<bool_is_not_a_proof>) noexcept
{
	return true;
}

struct x86_core_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_hybrid_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_hybrid};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_zen_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_amd_zen};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_generic_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::generic};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct x86_atom_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_atom};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{false};
};

struct aarch64_server_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::aarch64};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::arm_server};
	inline static constexpr bool data_available{true};
	inline static constexpr bool instruction_available{true};
};

struct unavailable_platform
{
	inline static constexpr ::fast_io::prfch_isa isa{::fast_io::prfch_isa::x86};
	inline static constexpr ::fast_io::prfch_tune tune{::fast_io::prfch_tune::x86_intel_core};
	inline static constexpr bool data_available{false};
	inline static constexpr bool instruction_available{false};
};

} // namespace policy_test

static_assert(!::fast_io::prfch_cacheable_read_provenance<int *>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<int *>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<::fast_io::basic_io_scatter_t<char>>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<::fast_io::basic_io_scatter_t<char>>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<policy_test::unmarked_stream>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<policy_test::unmarked_stream>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<policy_test::unmarked_scatter_source>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<policy_test::bool_is_not_a_proof>);

static_assert(::fast_io::prfch_cacheable_read_provenance<policy_test::cacheable_read_source>);
static_assert(!::fast_io::prfch_cacheable_write_provenance<policy_test::cacheable_read_source>);
static_assert(::fast_io::prfch_cacheable_write_provenance<policy_test::cacheable_write_destination>);
static_assert(!::fast_io::prfch_cacheable_read_provenance<policy_test::cacheable_write_destination>);
static_assert(::fast_io::prfch_cacheable_read_write_provenance<policy_test::cacheable_owned_buffer>);
static_assert(::fast_io::prfch_cacheable_read_provenance<policy_test::cacheable_owned_buffer const &>);

static_assert(::fast_io::conservative_read_prfch_platform<policy_test::x86_core_platform>);
static_assert(::fast_io::conservative_write_prfch_platform<policy_test::x86_hybrid_platform>);
static_assert(::fast_io::conservative_read_prfch_platform<policy_test::x86_zen_platform>);
static_assert(::fast_io::conservative_read_prfch_platform<policy_test::aarch64_server_platform>);
static_assert(!::fast_io::conservative_read_prfch_platform<int>);
static_assert(!::fast_io::conservative_read_prfch_platform<policy_test::x86_generic_platform>);
static_assert(!::fast_io::conservative_write_prfch_platform<policy_test::x86_atom_platform>);
static_assert(!::fast_io::conservative_read_prfch_platform<policy_test::unavailable_platform>);

static_assert(::fast_io::conservative_read_prfch_strategy<policy_test::x86_core_platform,
														  policy_test::cacheable_read_source>);
static_assert(!::fast_io::conservative_write_prfch_strategy<policy_test::x86_core_platform,
															policy_test::cacheable_read_source>);
static_assert(::fast_io::conservative_write_prfch_strategy<policy_test::x86_core_platform,
														   policy_test::cacheable_owned_buffer>);
static_assert(!::fast_io::conservative_read_prfch_strategy<policy_test::x86_generic_platform,
														   policy_test::cacheable_read_source>);
static_assert(!::fast_io::conservative_read_prfch_strategy<policy_test::x86_core_platform,
														   policy_test::unmarked_scatter_source>);

int main()
{}
