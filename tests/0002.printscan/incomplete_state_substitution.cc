#include <fast_io_core.h>

namespace
{

struct incomplete_scan_state;
struct incomplete_scan_target
{};

inline constexpr ::fast_io::io_type_t<incomplete_scan_state> scan_context_type(
	::fast_io::io_reserve_type_t<char, incomplete_scan_target>) noexcept;

struct incomplete_print_state;
struct incomplete_print_target
{};

inline constexpr ::fast_io::io_type_t<incomplete_print_state> print_context_type(
	::fast_io::io_reserve_type_t<char, incomplete_print_target>) noexcept;

struct incomplete_staged_state;
struct incomplete_staged_target
{};

inline constexpr ::fast_io::io_type_t<incomplete_staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, incomplete_staged_target>) noexcept;

struct throwing_assignment_staged_state
{
	throwing_assignment_staged_state() = default;
	throwing_assignment_staged_state(throwing_assignment_staged_state const &) = default;
	throwing_assignment_staged_state(throwing_assignment_staged_state &&) = default;
	throwing_assignment_staged_state &operator=(throwing_assignment_staged_state const &) = default;
	throwing_assignment_staged_state &operator=(throwing_assignment_staged_state &&) noexcept(false)
	{
		return *this;
	}
};

struct throwing_assignment_staged_target
{};

struct throwing_default_staged_state
{
	throwing_default_staged_state() noexcept(false) {}
	constexpr explicit throwing_default_staged_state(int) noexcept {}
	throwing_default_staged_state(throwing_default_staged_state const &) = default;
	throwing_default_staged_state(throwing_default_staged_state &&) = default;
	throwing_default_staged_state &operator=(throwing_default_staged_state const &) = default;
	throwing_default_staged_state &operator=(throwing_default_staged_state &&) noexcept = default;
};

struct throwing_default_staged_target
{};

inline constexpr ::fast_io::io_type_t<throwing_assignment_staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, throwing_assignment_staged_target>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, throwing_assignment_staged_target>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, throwing_assignment_staged_target>,
	throwing_assignment_staged_target const &) noexcept
{
	return true;
}

inline constexpr throwing_assignment_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, throwing_assignment_staged_target>,
	throwing_assignment_staged_target const &) noexcept
{
	return {};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, throwing_assignment_staged_target>, char *destination,
	throwing_assignment_staged_target const &, throwing_assignment_staged_state const &) noexcept
{
	return destination;
}

inline constexpr ::fast_io::io_type_t<throwing_default_staged_state> print_staged_type(
	::fast_io::io_reserve_type_t<char, throwing_default_staged_target>) noexcept
{
	return {};
}

inline constexpr ::std::size_t print_staged_width(
	::fast_io::io_reserve_type_t<char, throwing_default_staged_target>) noexcept
{
	return 2u;
}

inline constexpr bool print_staged_eligible(
	::fast_io::io_reserve_type_t<char, throwing_default_staged_target>,
	throwing_default_staged_target const &) noexcept
{
	return true;
}

inline constexpr throwing_default_staged_state print_staged_prepare(
	::fast_io::io_reserve_type_t<char, throwing_default_staged_target>,
	throwing_default_staged_target const &) noexcept
{
	return throwing_default_staged_state{0};
}

inline constexpr char *print_staged_define(
	::fast_io::io_reserve_type_t<char, throwing_default_staged_target>, char *destination,
	throwing_default_staged_target const &, throwing_default_staged_state const &) noexcept
{
	return destination;
}

// An advertised forward declaration is not storage. All three protocol families must reject it during constraint
// substitution, before standard construction traits or a controller body can require a complete state object.
static_assert(!::fast_io::context_scannable<char, incomplete_scan_target>);
static_assert(!::fast_io::context_printable<char, incomplete_print_target>);
static_assert(!::fast_io::staged_printable<char, incomplete_staged_target>);
// The staged controller stores every prepared value inside an unconditionally-noexcept helper. A state whose move
// assignment may throw must retain the ordinary formatter instead of acquiring a terminate-only optimization path.
static_assert(!::fast_io::staged_printable<char, throwing_assignment_staged_target>);
// Staging also default-constructs its state array before preparation. A potentially throwing default constructor would
// add a new exception edge solely because the optional schedule was selected, even if prepare itself returns through a
// separate non-throwing constructor.
static_assert(!::fast_io::staged_printable<char, throwing_default_staged_target>);

} // namespace

int main() {}
