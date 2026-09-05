#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include <fast_io_core.h>

namespace transcoder_protocol_boundary_test
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

template <typename function>
[[nodiscard]] inline bool rejects_protocol(function &&operation)
{
	try
	{
		::std::forward<function>(operation)();
	}
	catch (::fast_io::error error_value)
	{
		return error_value ==
			   ::fast_io::transcode_stream_errc::protocol_violation;
	}
	catch (...)
	{
		return false;
	}
	return false;
}

struct normalization_failure
{};

struct throwing_ref
{
	using from_value_type = char;
	using to_value_type = char;
};

struct throwing_engine
{};

inline throwing_ref transcode_ref_define(throwing_engine &)
{
	// A provider is permitted to fail while acquiring a stateful observer.
	throw normalization_failure{};
}

inline ::fast_io::basic_transcode_process_result<char, char>
transcode_process_define(throwing_ref &, char const *,
						 char const *from_last, char *to_first, char *) noexcept
{
	return {from_last, to_first,
			::fast_io::transcode_step_status::need_input};
}

inline ::fast_io::basic_transcode_drain_result<char>
transcode_finish_define(throwing_ref &, char *to_first, char *) noexcept
{
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<throwing_ref>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

struct cv_observer
{
	using from_value_type = char;
	using to_value_type = char;
};

struct cv_engine
{
	cv_observer observer{};
};

inline cv_observer const &transcode_ref_define(cv_engine &engine) noexcept
{
	return engine.observer;
}

inline ::fast_io::basic_transcode_process_result<char, char>
transcode_process_define(cv_observer &, char const *, char const *from_last,
						 char *to_first, char *) noexcept
{
	return {from_last, to_first,
			::fast_io::transcode_step_status::need_input};
}

inline ::fast_io::basic_transcode_process_result<char, char>
transcode_process_define(cv_observer const &, char const *,
						 char const *from_last, char *to_first,
						 char *) noexcept(false)
{
	// The body is nonthrowing, but the declared contract distinguishes the
	// selected const-observer expression from the non-const overload above.
	return {from_last, to_first,
			::fast_io::transcode_step_status::need_input};
}

inline ::fast_io::basic_transcode_drain_result<char>
transcode_finish_define(cv_observer const &, char *to_first, char *) noexcept
{
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<cv_observer>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

template <::fast_io::transcode_unit unit_type>
struct identity_engine
{
	using from_value_type = unit_type;
	using to_value_type = unit_type;
};

template <::fast_io::transcode_unit unit_type>
inline ::fast_io::basic_transcode_process_result<unit_type, unit_type>
transcode_process_define(identity_engine<unit_type> &, unit_type const *,
						 unit_type const *from_last, unit_type *to_first,
						 unit_type *) noexcept
{
	// This test engine consumes the complete source while producing no output.
	return {from_last, to_first,
			::fast_io::transcode_step_status::need_input};
}

template <::fast_io::transcode_unit unit_type>
inline ::fast_io::basic_transcode_drain_result<unit_type>
transcode_finish_define(identity_engine<unit_type> &, unit_type *to_first,
						unit_type *) noexcept
{
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

template <::fast_io::transcode_unit unit_type>
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<identity_engine<unit_type>>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

template <::fast_io::transcode_unit unit_type>
struct hostile_engine_cursor
{
	using from_value_type = unit_type;
	using to_value_type = unit_type;

	unit_type const *foreign_from{};
	unit_type *foreign_to{};
};

template <::fast_io::transcode_unit unit_type>
inline ::fast_io::basic_transcode_process_result<unit_type, unit_type>
transcode_process_define(hostile_engine_cursor<unit_type> &engine,
						 unit_type const *, unit_type const *from_last,
						 unit_type *to_first, unit_type *) noexcept
{
	// Non-null configured cursors deliberately violate exactly one endpoint.
	return {engine.foreign_from == nullptr ? from_last : engine.foreign_from,
			engine.foreign_to == nullptr ? to_first : engine.foreign_to,
			::fast_io::transcode_step_status::need_input};
}

template <::fast_io::transcode_unit unit_type>
inline ::fast_io::basic_transcode_drain_result<unit_type>
transcode_finish_define(hostile_engine_cursor<unit_type> &,
						unit_type *to_first, unit_type *) noexcept
{
	return {to_first, ::fast_io::transcode_drain_status::complete};
}

template <::fast_io::transcode_unit unit_type>
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<hostile_engine_cursor<unit_type>>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

struct hostile_input
{
	using input_char_type = char;
	char const *begin{};
	char const *current{};
	char const *end{};
};

struct hostile_input_ref
{
	using input_char_type = char;
	hostile_input *ptr{};
};

inline hostile_input_ref input_stream_ref_define(hostile_input &input) noexcept
{
	return {__builtin_addressof(input)};
}

inline char const *ibuffer_begin(hostile_input_ref ref) noexcept
{
	return ref.ptr->begin;
}

inline char const *ibuffer_curr(hostile_input_ref ref) noexcept
{
	return ref.ptr->current;
}

inline char const *ibuffer_end(hostile_input_ref ref) noexcept
{
	return ref.ptr->end;
}

inline void ibuffer_set_curr(hostile_input_ref ref,
							 char const *current) noexcept
{
	ref.ptr->current = current;
}

inline bool ibuffer_underflow(hostile_input_ref) noexcept
{
	return false;
}

struct hostile_output
{
	using output_char_type = char;
	char *begin{};
	char *current{};
	char *end{};
};

struct hostile_output_ref
{
	using output_char_type = char;
	hostile_output *ptr{};
};

inline hostile_output_ref output_stream_ref_define(hostile_output &output) noexcept
{
	return {__builtin_addressof(output)};
}

inline char *obuffer_begin(hostile_output_ref ref) noexcept
{
	return ref.ptr->begin;
}

inline char *obuffer_curr(hostile_output_ref ref) noexcept
{
	return ref.ptr->current;
}

inline char *obuffer_end(hostile_output_ref ref) noexcept
{
	return ref.ptr->end;
}

inline void obuffer_set_curr(hostile_output_ref ref, char *current) noexcept
{
	ref.ptr->current = current;
}

inline void write_all_overflow_define(hostile_output_ref,
									  char const *, char const *) noexcept
{
	// This fallback exists only to satisfy the ordinary writable-stream concept;
	// the malformed direct put area must be rejected before forwarding occurs.
}

constexpr bool constexpr_range_contract() noexcept
{
	::std::uint_least32_t storage[4]{};
	auto const validated{
		::fast_io::details::validate_transcode_closed_range_offsets(
			storage, storage + 4u, storage + 2u)};
	return validated.valid && validated.begin == storage &&
		   validated.current == storage + 2u &&
		   validated.end == storage + 4u &&
		   validated.current_offset == 2u && validated.end_offset == 4u;
}

static_assert(constexpr_range_contract());
static_assert(::fast_io::transcoder<throwing_engine>);
static_assert(::fast_io::transcoder<cv_engine>);
static_assert(::fast_io::transcoder<identity_engine<char>>);
static_assert(::fast_io::transcoder<hostile_engine_cursor<char>>);
static_assert(!noexcept(::fast_io::transcode_ref(
	::std::declval<throwing_engine &>())));
static_assert(noexcept(::fast_io::transcode_ref(
	::std::declval<identity_engine<char> &>())));
static_assert(!noexcept(::fast_io::operations::transcode_process(
	::std::declval<throwing_engine &>(), static_cast<char const *>(nullptr),
	static_cast<char const *>(nullptr), static_cast<char *>(nullptr),
	static_cast<char *>(nullptr))));
static_assert(noexcept(::fast_io::operations::transcode_process(
	::std::declval<identity_engine<char> &>(),
	static_cast<char const *>(nullptr), static_cast<char const *>(nullptr),
	static_cast<char *>(nullptr), static_cast<char *>(nullptr))));
static_assert(!noexcept(::fast_io::operations::transcode_process(
	::std::declval<cv_engine &>(), static_cast<char const *>(nullptr),
	static_cast<char const *>(nullptr), static_cast<char *>(nullptr),
	static_cast<char *>(nullptr))));

inline void test_runtime_validator()
{
	::std::array<::std::uint_least32_t, 8u> owned{};
	::std::array<::std::uint_least32_t, 8u> foreign{};
	auto const valid{::fast_io::details::validate_transcode_closed_range_offsets(
		owned.data(), owned.data() + owned.size(), owned.data() + 3u)};
	require(valid.valid && valid.current == owned.data() + 3u &&
			valid.current_offset == 3u && valid.end_offset == owned.size());
	auto const unrelated{
		::fast_io::details::validate_transcode_closed_range_offsets(
			owned.data(), owned.data() + owned.size(), foreign.data() + 4u)};
	require(!unrelated.valid);

	auto const misaligned_address{
		reinterpret_cast<::std::uintptr_t>(owned.data()) + 1u};
	auto *const misaligned{
		reinterpret_cast<::std::uint_least32_t *>(misaligned_address)};
	auto const wide_unit_misaligned{
		::fast_io::details::validate_transcode_closed_range_offsets(
			owned.data(), owned.data() + owned.size(), misaligned)};
	require(!wide_unit_misaligned.valid);
	auto *const misaligned_end{reinterpret_cast<::std::uint_least32_t *>(
		reinterpret_cast<::std::uintptr_t>(owned.data() + owned.size()) - 1u)};
	auto const wide_unit_misaligned_end{
		::fast_io::details::validate_transcode_closed_range_offsets(
			owned.data(), misaligned_end, owned.data())};
	require(!wide_unit_misaligned_end.valid);
}

inline void test_throwing_normalizer()
{
	throwing_engine engine{};
	char source{'x'};
	char destination{};
	bool public_caught{};
	try
	{
		(void)::fast_io::operations::transcode_process(
			engine, __builtin_addressof(source),
			__builtin_addressof(source) + 1u,
			__builtin_addressof(destination),
			__builtin_addressof(destination) + 1u);
	}
	catch (normalization_failure const &)
	{
		public_caught = true;
	}
	require(public_caught);

	::std::array<char, 8u> storage{};
	::fast_io::basic_obuffer_view<char> physical_output{storage};
	auto output{::fast_io::make_otranscoder(
		physical_output, throwing_engine{})};
	bool adapter_caught{};
	try
	{
		// Flush normalizes the engine inside the adapter's guarded transaction.
		output.flush();
	}
	catch (normalization_failure const &)
	{
		adapter_caught = true;
	}
	require(adapter_caught && output.state == ::fast_io::otranscoder_state::failed);

	::std::array<char, 1u> encoded{};
	::fast_io::basic_ibuffer_view<char> physical_input{encoded};
	auto input{::fast_io::make_itranscoder(
		physical_input, throwing_engine{})};
	bool input_caught{};
	try
	{
		(void)input.read_some(
			__builtin_addressof(destination),
			__builtin_addressof(destination) + 1u);
	}
	catch (normalization_failure const &)
	{
		input_caught = true;
	}
	require(input_caught && input.state == ::fast_io::itranscoder_state::failed);
}

inline void test_engine_cursor_boundaries()
{
	::std::array<char, 8u> physical_storage{};
	::std::array<char, 8u> foreign_source{};
	::fast_io::basic_obuffer_view<char> physical_output{physical_storage};
	auto output{::fast_io::make_otranscoder(
		physical_output,
		hostile_engine_cursor<char>{foreign_source.data() + 4u, nullptr})};
	using output_type = decltype(output);
	::std::array<char, output_type::traits_type::public_buffer_size> source{};
	// A full public block enters the engine in the same guarded overflow call,
	// avoiding any unrelated warning about a lazily buffered prefix.
	require(rejects_protocol([&] {
		output.write_all_overflow(source.data(), source.data() + source.size());
	}));
	require(output.state == ::fast_io::otranscoder_state::failed);

	using wide_unit = ::std::uint_least32_t;
	::std::array<wide_unit, 2u> physical_source{};
	::std::array<wide_unit, 2u> alignment_storage{};
	auto const misaligned_address{
		reinterpret_cast<::std::uintptr_t>(alignment_storage.data()) + 1u};
	auto *const misaligned{reinterpret_cast<wide_unit *>(misaligned_address)};
	::fast_io::basic_ibuffer_view<wide_unit> physical_input{physical_source};
	auto input{::fast_io::make_itranscoder(
		physical_input, hostile_engine_cursor<wide_unit>{nullptr, misaligned})};
	wide_unit decoded{};
	require(rejects_protocol([&] {
		(void)input.read_some(
			__builtin_addressof(decoded), __builtin_addressof(decoded) + 1u);
	}));
	require(input.state == ::fast_io::itranscoder_state::failed);
}

inline void test_stream_cursor_boundaries()
{
	::std::array<char, 8u> owned_input{};
	::std::array<char, 8u> foreign_input{};
	hostile_input physical_input{
		owned_input.data(), foreign_input.data() + 4u,
		owned_input.data() + owned_input.size()};
	auto input{::fast_io::make_itranscoder(
		physical_input, identity_engine<char>{})};
	char decoded{};
	require(rejects_protocol([&] {
		(void)input.read_some(
			__builtin_addressof(decoded), __builtin_addressof(decoded) + 1u);
	}));
	require(input.state == ::fast_io::itranscoder_state::failed);

	::std::array<char, 8u> owned_output{};
	::std::array<char, 8u> foreign_output{};
	hostile_output physical_output{
		owned_output.data(), foreign_output.data() + 4u,
		owned_output.data() + owned_output.size()};
	auto output{::fast_io::make_otranscoder(
		physical_output, identity_engine<char>{})};
	using output_type = decltype(output);
	::std::array<char, output_type::traits_type::public_buffer_size> source{};
	require(rejects_protocol([&] {
		output.write_all_overflow(source.data(), source.data() + source.size());
	}));
	require(output.state == ::fast_io::otranscoder_state::failed);
}

inline void test_adapter_ref_setters()
{
	::std::array<char, 8u> output_storage{};
	::std::array<char, 8u> foreign{};
	::fast_io::basic_obuffer_view<char> physical_output{output_storage};
	auto output{::fast_io::make_otranscoder(
		physical_output, identity_engine<char>{})};
	output.prepare_put_area();
	auto outref{::fast_io::output_stream_ref_define(output)};
	require(rejects_protocol([&] {
		::fast_io::obuffer_set_curr(outref, foreign.data() + 4u);
	}));

	::std::array<char, 1u> input_storage{};
	::fast_io::basic_ibuffer_view<char> physical_input{input_storage};
	auto input{::fast_io::make_itranscoder(
		physical_input, identity_engine<char>{})};
	input.prepare_get_area();
	auto inref{::fast_io::input_stream_ref_define(input)};
	require(rejects_protocol([&] {
		::fast_io::ibuffer_set_curr(inref, foreign.data() + 4u);
	}));
}

} // namespace transcoder_protocol_boundary_test

int main()
{
	using namespace ::transcoder_protocol_boundary_test;
	test_runtime_validator();
	test_throwing_normalizer();
	test_engine_cursor_boundaries();
	test_stream_cursor_boundaries();
	test_adapter_ref_setters();
}
