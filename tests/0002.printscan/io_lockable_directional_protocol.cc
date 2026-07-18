#include <fast_io_core.h>

#include <concepts>
#include <type_traits>

namespace
{

struct test_mutex
{
	constexpr void lock() noexcept {}
	constexpr void unlock() noexcept {}
	[[nodiscard]] constexpr bool try_lock() noexcept
	{
		return true;
	}
};

struct output_observer
{
	using output_char_type = char;
};

struct output_handle
{
	using output_char_type = char;
};

[[nodiscard]] inline constexpr output_observer output_stream_ref_define(output_handle &) noexcept
{
	return {};
}

struct input_observer
{
	using input_char_type = char;
};

struct input_handle
{
	using input_char_type = char;
};

[[nodiscard]] inline constexpr input_observer input_stream_ref_define(input_handle &) noexcept
{
	return {};
}

using output_lockable = ::fast_io::basic_general_io_lockable_nonmovable<output_handle, test_mutex>;
using input_lockable = ::fast_io::basic_general_io_lockable_nonmovable<input_handle, test_mutex>;
using output_lockable_ref = decltype(output_stream_ref_define(::std::declval<output_lockable &>()));
using input_lockable_ref = decltype(input_stream_ref_define(::std::declval<input_lockable &>()));

static_assert(::std::same_as<typename output_lockable_ref::output_char_type, char>);
static_assert(::std::same_as<typename output_lockable_ref::input_char_type, void>);
static_assert(::std::same_as<typename input_lockable_ref::input_char_type, char>);
static_assert(::std::same_as<typename input_lockable_ref::output_char_type, void>);

static_assert(::fast_io::operations::defines::has_output_stream_ref_define<output_lockable &>);
static_assert(!::fast_io::operations::defines::has_input_stream_ref_define<output_lockable &>);
static_assert(!::fast_io::operations::defines::has_io_stream_ref_define<output_lockable &>);
static_assert(::fast_io::operations::defines::has_input_stream_ref_define<input_lockable &>);
static_assert(!::fast_io::operations::defines::has_output_stream_ref_define<input_lockable &>);
static_assert(!::fast_io::operations::defines::has_io_stream_ref_define<input_lockable &>);

static_assert(requires(output_lockable &value) {
	{ ::fast_io::operations::output_stream_ref(value) } -> ::std::same_as<output_lockable_ref>;
});
static_assert(requires(input_lockable &value) {
	{ ::fast_io::operations::input_stream_ref(value) } -> ::std::same_as<input_lockable_ref>;
});

} // namespace

int main() {}
