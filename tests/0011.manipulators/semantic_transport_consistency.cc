#include <concepts>
#include <type_traits>
#include <utility>

#define FAST_IO_DISABLE_FLOATING_POINT
#include <fast_io.h>

namespace
{

struct ordinary_small_value
{
	void *pointer{};
};

// Pack and condition storage are not separate ABIs. These identities are intentionally target-independent: a cross
// build must make all three decisions from the selected native ABI model, including its conservative unknown fallback.
static_assert(::fast_io::details::pack_value_transferable<ordinary_small_value> ==
	::fast_io::details::io_print_forward_transport_by_value<ordinary_small_value>);
static_assert(::fast_io::details::cond_value_transferable<ordinary_small_value> ==
	::fast_io::details::io_print_forward_transport_by_value<ordinary_small_value>);

struct owned_copy_only_child
{
	owned_copy_only_child() = default;
	inline explicit owned_copy_only_child(owned_copy_only_child const &) noexcept {}
	owned_copy_only_child(owned_copy_only_child &&) = delete;
};

struct owned_copy_only_source
{
	owned_copy_only_child const *child{};
};

inline constexpr owned_copy_only_child const &print_alias_define(
	::fast_io::io_alias_t, owned_copy_only_source &&source) noexcept
{
	return *source.child;
}

// The explicit copy can initialize a node member, but the resulting owned child cannot cross the next semantic
// by-value boundary. Width must reject it at the factory constraint, exactly as pack and condition already do.
static_assert(requires {
	static_cast<::fast_io::details::width_storage_type<owned_copy_only_source>>(
		::fast_io::io_print_alias(::std::declval<owned_copy_only_source>()));
});
static_assert(!::fast_io::details::width_storable<owned_copy_only_source>);
static_assert(!::fast_io::details::pack_alias_storable<owned_copy_only_source>);
static_assert(!::fast_io::details::cond_alias_storable<owned_copy_only_source>);

} // namespace

int main() {}
